#include "MemoryRecovery.h"
#include "QueueManager.h"
#include "MqttManager.h"
#include <esp_system.h>

// ============================================
// STATIC VARIABLE INITIALIZATION
// ============================================
unsigned long MemoryRecovery::lastMemoryCheck = 0;
uint32_t MemoryRecovery::memoryCheckInterval = 5000; // Check every 5 seconds
uint32_t MemoryRecovery::lowMemoryEventCount = 0;
uint32_t MemoryRecovery::criticalEventCount = 0;
bool MemoryRecovery::recoveryInProgress = false;
bool MemoryRecovery::autoRecoveryEnabled = true;

// FIXED BUG #7: Add recursion guard to prevent infinite recursion
// Previous code could recurse if logging functions called checkAndRecover()
static bool inRecoveryCall = false;

// ============================================
// CORE FUNCTIONS IMPLEMENTATION
// ============================================

RecoveryAction MemoryRecovery::checkAndRecover() {
  // FIXED BUG #7: Recursion guard - prevent infinite recursion
  // If already in recovery call (e.g., from logging), return immediately
  if (inRecoveryCall) {
    return RECOVERY_NONE;
  }

  // Set guard flag
  inRecoveryCall = true;

  // Auto-recovery disabled - skip check
  if (!autoRecoveryEnabled) {
    inRecoveryCall = false;  // Release guard
    return RECOVERY_NONE;
  }

  unsigned long now = millis();

  // Throttle memory checks to avoid overhead
  if (now - lastMemoryCheck < memoryCheckInterval) {
    inRecoveryCall = false;  // Release guard before return
    return RECOVERY_NONE;
  }
  lastMemoryCheck = now;

  // Get current memory stats
  uint32_t freeDram = ESP.getFreeHeap();
  uint32_t freePsram = ESP.getFreePsram();

  // ============================================
  // TIER 1: EMERGENCY (< 10KB DRAM)
  // ============================================
  if (freeDram < MemoryThresholds::DRAM_EMERGENCY) {
    criticalEventCount++;
    LOG_MEM_ERROR("EMERGENCY: DRAM only %lu bytes! Critical event #%lu\n",
                  freeDram, criticalEventCount);

    if (criticalEventCount >= 3) {
      // 3 consecutive emergency events - restart required
      LOG_MEM_ERROR("FATAL: 3+ consecutive emergency events - initiating restart...\n");
      executeEmergencyRestart();
      // Never returns
    }

    // Try all recovery actions in sequence
    bool recovered = false;
    recovered |= forceRecovery(RECOVERY_FLUSH_OLD_QUEUE);
    recovered |= forceRecovery(RECOVERY_CLEAR_MQTT_PERSISTENT);
    recovered |= forceRecovery(RECOVERY_FORCE_GARBAGE_COLLECT);

    inRecoveryCall = false;  // Release guard
    return recovered ? RECOVERY_FORCE_GARBAGE_COLLECT : RECOVERY_NONE;
  }

  // ============================================
  // TIER 2: CRITICAL (< 15KB DRAM)
  // ============================================
  if (freeDram < MemoryThresholds::DRAM_CRITICAL) {
    lowMemoryEventCount++;
    LOG_MEM_ERROR("CRITICAL: DRAM only %lu bytes! Forcing recovery... (event #%lu)\n",
                  freeDram, lowMemoryEventCount);

    if (lowMemoryEventCount >= 5) {
      // 5 consecutive critical events - escalate to emergency
      LOG_MEM_ERROR("CRITICAL: 5+ consecutive events - escalating to emergency restart\n");
      criticalEventCount = 3; // Trigger restart on next check
      inRecoveryCall = false;  // Release guard
      return RECOVERY_NONE;
    }

    // Try aggressive recovery
    if (forceRecovery(RECOVERY_FLUSH_OLD_QUEUE)) {
      inRecoveryCall = false;  // Release guard
      return RECOVERY_FLUSH_OLD_QUEUE;
    }

    if (forceRecovery(RECOVERY_CLEAR_MQTT_PERSISTENT)) {
      inRecoveryCall = false;  // Release guard
      return RECOVERY_CLEAR_MQTT_PERSISTENT;
    }

    inRecoveryCall = false;  // Release guard
    return RECOVERY_NONE;
  }

  // ============================================
  // TIER 3: WARNING (< 30KB DRAM)
  // ============================================
  if (freeDram < MemoryThresholds::DRAM_WARNING) {
    lowMemoryEventCount++;

    // Throttle DRAM warnings - log only every 60 seconds to reduce noise
    // System is stable at ~29KB when BLE connected with no devices configured
    static LogThrottle dramWarnThrottle(120000); // Log every 120 seconds
    char dramContext[64];
    snprintf(dramContext, sizeof(dramContext), "DRAM at %lu KB (event #%lu)", freeDram / 1024, lowMemoryEventCount);
    if (dramWarnThrottle.shouldLog(dramContext)) {
      LOG_MEM_WARN("LOW DRAM: %lu bytes (threshold: %lu). Triggering proactive cleanup... (event #%lu)\n",
                   freeDram, MemoryThresholds::DRAM_WARNING, lowMemoryEventCount);
    }

    // Proactive cleanup - clear expired messages
    if (forceRecovery(RECOVERY_CLEAR_MQTT_PERSISTENT)) {
      inRecoveryCall = false;  // Release guard
      return RECOVERY_CLEAR_MQTT_PERSISTENT;
    }

    inRecoveryCall = false;  // Release guard
    return RECOVERY_NONE;
  }

  // ============================================
  // TIER 4: HEALTHY (> 50KB DRAM)
  // ============================================
  if (freeDram > MemoryThresholds::DRAM_HEALTHY) {
    // Healthy state - reset counters
    if (lowMemoryEventCount > 0 || criticalEventCount > 0) {
      LOG_MEM_INFO("Memory recovered: %lu bytes DRAM. Resetting event counters.\n", freeDram);
      resetRecoveryState();
    }
  }

  // ============================================
  // PSRAM MONITORING (Informational)
  // ============================================
  if (freePsram < MemoryThresholds::PSRAM_CRITICAL) {
    LOG_MEM_ERROR("CRITICAL PSRAM: only %lu bytes free!\n", freePsram);
  } else if (freePsram < MemoryThresholds::PSRAM_WARNING) {
    static LogThrottle psramWarnThrottle(300000); // Log every 5 minutes
    char psramContext[64];
    snprintf(psramContext, sizeof(psramContext), "PSRAM low (%lu KB free)", freePsram / 1024);
    if (psramWarnThrottle.shouldLog(psramContext)) {
      LOG_MEM_WARN("LOW PSRAM: %lu bytes free\n", freePsram);
    }
  }

  // Release recursion guard before exit
  inRecoveryCall = false;
  return RECOVERY_NONE;
}

// ============================================
// MEMORY STATISTICS
// ============================================

void MemoryRecovery::getMemoryStats(uint32_t &freeDram, uint32_t &freePsram) {
  freeDram = ESP.getFreeHeap();
  freePsram = ESP.getFreePsram();
}

void MemoryRecovery::logMemoryStatus(const char* context) {
  uint32_t freeDram, freePsram;
  getMemoryStats(freeDram, freePsram);

  float dramUsage = getDramUsagePercent();
  float psramUsage = getPsramUsagePercent();

  LOG_MEM_INFO("[%s] DRAM: %lu bytes (%.1f%% used), PSRAM: %lu bytes (%.1f%% used)\n",
               context,
               freeDram, dramUsage,
               freePsram, psramUsage);
}

float MemoryRecovery::getDramUsagePercent(uint32_t totalDram) {
  uint32_t freeDram = ESP.getFreeHeap();
  return ((float)(totalDram - freeDram) / totalDram) * 100.0f;
}

float MemoryRecovery::getPsramUsagePercent(uint32_t totalPsram) {
  uint32_t freePsram = ESP.getFreePsram();
  return ((float)(totalPsram - freePsram) / totalPsram) * 100.0f;
}

// ============================================
// MANUAL CLEANUP TRIGGER
// ============================================

uint32_t MemoryRecovery::triggerCleanup() {
  uint32_t dramBefore = ESP.getFreeHeap();

  LOG_MEM_INFO("Manual memory cleanup triggered. Free DRAM before: %lu bytes\n", dramBefore);

  // Execute progressive cleanup actions
  int bytesFlushed = 0;

  // 1. Queue flush (remove oldest 10 entries)
  bytesFlushed += executeQueueFlush(10);

  // 2. MQTT queue cleanup (remove expired messages)
  bytesFlushed += executeMqttQueueCleanup();

  // 3. Garbage collection
  executeGarbageCollection();

  uint32_t dramAfter = ESP.getFreeHeap();
  uint32_t dramFreed = (dramAfter > dramBefore) ? (dramAfter - dramBefore) : 0;

  LOG_MEM_INFO("Manual cleanup complete. Free DRAM after: %lu bytes (freed %lu bytes)\n",
               dramAfter, dramFreed);

  return dramFreed;
}

// ============================================
// RECOVERY ACTIONS
// ============================================

bool MemoryRecovery::forceRecovery(RecoveryAction action) {
  if (recoveryInProgress) {
    LOG_MEM_WARN("Recovery already in progress, skipping %d\n", action);
    return false;
  }

  recoveryInProgress = true;
  bool success = false;
  uint32_t beforeDram = ESP.getFreeHeap();

  switch (action) {
    case RECOVERY_FLUSH_OLD_QUEUE: {
      int flushed = executeQueueFlush(20);
      success = (flushed > 0);
      if (success) {
        uint32_t afterDram = ESP.getFreeHeap();
        LOG_MEM_INFO("Flushed %d old queue entries. DRAM: %lu -> %lu bytes (+%lu)\n",
                     flushed, beforeDram, afterDram, afterDram - beforeDram);
      }
      break;
    }

    case RECOVERY_CLEAR_MQTT_PERSISTENT: {
      int cleared = executeMqttQueueCleanup();
      success = (cleared > 0);
      if (success) {
        uint32_t afterDram = ESP.getFreeHeap();
        LOG_MEM_INFO("Cleared %d MQTT messages. DRAM: %lu -> %lu bytes (+%lu)\n",
                     cleared, beforeDram, afterDram, afterDram - beforeDram);
      } else {
        LOG_MEM_DEBUG("No MQTT messages to clear\n");
      }
      break;
    }

    case RECOVERY_FORCE_GARBAGE_COLLECT: {
      success = executeGarbageCollection();
      if (success) {
        uint32_t afterDram = ESP.getFreeHeap();
        LOG_MEM_INFO("Forced GC. DRAM: %lu -> %lu bytes (+%lu)\n",
                     beforeDram, afterDram, afterDram - beforeDram);
      }
      break;
    }

    case RECOVERY_EMERGENCY_RESTART: {
      executeEmergencyRestart();
      // Never returns
      break;
    }

    default:
      LOG_MEM_WARN("Unknown recovery action: %d\n", action);
      break;
  }

  recoveryInProgress = false;
  return success;
}

void MemoryRecovery::resetRecoveryState() {
  lowMemoryEventCount = 0;
  criticalEventCount = 0;
  recoveryInProgress = false;
}

void MemoryRecovery::setAutoRecovery(bool enabled) {
  autoRecoveryEnabled = enabled;
  LOG_MEM_INFO("Auto-recovery %s\n", enabled ? "ENABLED" : "DISABLED");
}

bool MemoryRecovery::isAutoRecoveryEnabled() {
  return autoRecoveryEnabled;
}

void MemoryRecovery::setCheckInterval(uint32_t intervalMs) {
  memoryCheckInterval = intervalMs;
  LOG_MEM_INFO("Memory check interval set to %lu ms\n", intervalMs);
}

uint32_t MemoryRecovery::getLowMemoryEventCount() {
  return lowMemoryEventCount;
}

// ============================================
// PRIVATE RECOVERY EXECUTORS
// ============================================

int MemoryRecovery::executeQueueFlush(int entriesToFlush) {
  QueueManager *queueMgr = QueueManager::getInstance();
  if (!queueMgr) {
    LOG_MEM_ERROR("QueueManager instance not available\n");
    return 0;
  }

  int flushed = 0;
  JsonDocument tempDoc;

  for (int i = 0; i < entriesToFlush; i++) {
    JsonObject tempObj = tempDoc.to<JsonObject>();
    if (queueMgr->dequeue(tempObj)) {
      flushed++;
    } else {
      break; // Queue empty
    }
    tempDoc.clear(); // Clear for next iteration
  }

  return flushed;
}

int MemoryRecovery::executeMqttQueueCleanup() {
  MqttManager *mqttMgr = MqttManager::getInstance();
  if (!mqttMgr || !mqttMgr->getPersistentQueue()) {
    LOG_MEM_ERROR("MQTT Manager or persistent queue not available\n");
    return 0;
  }

  uint32_t before = mqttMgr->getQueuedMessageCount();
  mqttMgr->getPersistentQueue()->clearExpiredMessages();
  uint32_t after = mqttMgr->getQueuedMessageCount();

  return (before - after);
}

bool MemoryRecovery::executeGarbageCollection() {
  // Force heap defragmentation by allocating/freeing large PSRAM block
  void* temp = ps_malloc(100000); // 100KB temporary block
  if (temp) {
    free(temp);
    return true;
  }
  return false;
}

void MemoryRecovery::executeEmergencyRestart() {
  LOG_MEM_ERROR("\n[MEMORY] EMERGENCY RESTART INITIATED\n");

  uint32_t freeDram, freePsram;
  getMemoryStats(freeDram, freePsram);

  LOG_MEM_ERROR("  Final Memory State:\n");
  LOG_MEM_ERROR("    DRAM: %lu bytes\n", freeDram);
  LOG_MEM_ERROR("    PSRAM: %lu bytes\n", freePsram);
  LOG_MEM_ERROR("    Low Memory Events: %lu\n", lowMemoryEventCount);
  LOG_MEM_ERROR("    Critical Events: %lu\n\n", criticalEventCount);

  delay(1000); // Allow logs to flush
  ESP.restart();
}
