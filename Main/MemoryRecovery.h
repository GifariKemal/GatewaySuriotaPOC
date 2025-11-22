#ifndef MEMORY_RECOVERY_H
#define MEMORY_RECOVERY_H

#include <Arduino.h>
#include "DebugConfig.h"

/*
 * ============================================
 * MEMORY RECOVERY MANAGER
 * ============================================
 *
 * Automatic memory monitoring and recovery system for ESP32-S3
 * - Monitors DRAM/PSRAM every 5 seconds
 * - Tiered recovery actions based on severity
 * - Emergency restart on sustained critical conditions
 * - Zero manual intervention required
 *
 * Integration: Call MemoryRecovery::checkAndRecover() in main task loops
 */

// ============================================
// MEMORY THRESHOLDS (bytes)
// ============================================
namespace MemoryThresholds {
  // DRAM thresholds (ESP32-S3 has ~400KB total DRAM)
  // OPTIMIZED for large BLE responses (up to 10KB) and device reads with 45+ registers
  constexpr uint32_t DRAM_HEALTHY     = 80000;  // 80KB - Normal operation (increased from 50KB)
  constexpr uint32_t DRAM_WARNING     = 40000;  // 40KB - Proactive cleanup (increased from 25KB for large responses)
  constexpr uint32_t DRAM_CRITICAL    = 20000;  // 20KB - Emergency recovery (increased from 15KB)
  constexpr uint32_t DRAM_EMERGENCY   = 10000;  // 10KB - Imminent crash (unchanged)

  // PSRAM thresholds (ESP32-S3 has 8MB OPI PSRAM)
  constexpr uint32_t PSRAM_WARNING    = 1000000; // 1MB - Warn if PSRAM low
  constexpr uint32_t PSRAM_CRITICAL   = 500000;  // 500KB - Critical PSRAM
}

// ============================================
// RECOVERY ACTION TYPES
// ============================================
enum RecoveryAction {
  RECOVERY_NONE = 0,
  RECOVERY_FLUSH_OLD_QUEUE = 1,      // Remove oldest 20 queue entries
  RECOVERY_CLEAR_MQTT_PERSISTENT = 2,// Clear expired MQTT persistent queue
  RECOVERY_FORCE_GARBAGE_COLLECT = 3,// Trigger aggressive heap cleanup
  RECOVERY_EMERGENCY_RESTART = 4     // Last resort - ESP restart
};

// ============================================
// MEMORY RECOVERY CLASS
// ============================================
class MemoryRecovery {
private:
  static unsigned long lastMemoryCheck;
  static uint32_t memoryCheckInterval;  // Default 5000ms
  static uint32_t lowMemoryEventCount;
  static uint32_t criticalEventCount;
  static bool recoveryInProgress;
  static bool autoRecoveryEnabled;

  // Private constructor (singleton pattern)
  MemoryRecovery() {}

public:
  // Get singleton instance
  static MemoryRecovery* getInstance() {
    static MemoryRecovery instance;
    return &instance;
  }

  // ============================================
  // CORE FUNCTIONS
  // ============================================

  /**
   * Check memory status and trigger recovery if needed
   * Call this from main loops (RTU, TCP, MQTT tasks)
   * Returns: Recovery action taken (if any)
   */
  static RecoveryAction checkAndRecover();

  /**
   * Get current memory statistics
   * @param freeDram Output: Free DRAM in bytes
   * @param freePsram Output: Free PSRAM in bytes
   */
  static void getMemoryStats(uint32_t &freeDram, uint32_t &freePsram);

  /**
   * Log memory status with context
   * @param context Caller identifier (e.g., "RTU", "MQTT")
   */
  static void logMemoryStatus(const char* context);

  /**
   * Force specific recovery action manually
   * @param action Recovery action to execute
   * @return true if recovery succeeded, false otherwise
   */
  static bool forceRecovery(RecoveryAction action);

  /**
   * Reset recovery state (e.g., after successful recovery)
   */
  static void resetRecoveryState();

  /**
   * Enable/disable automatic recovery
   * @param enabled true to enable auto-recovery (default)
   */
  static void setAutoRecovery(bool enabled);

  /**
   * Get auto-recovery status
   * @return true if auto-recovery enabled
   */
  static bool isAutoRecoveryEnabled();

  /**
   * Set memory check interval
   * @param intervalMs Interval in milliseconds (default 5000ms)
   */
  static void setCheckInterval(uint32_t intervalMs);

  /**
   * Get current low memory event count
   * @return Number of consecutive low memory events
   */
  static uint32_t getLowMemoryEventCount();

  /**
   * Get memory usage percentage
   * @param totalDram Total DRAM capacity (default 400000 bytes)
   * @return DRAM usage percentage (0-100)
   */
  static float getDramUsagePercent(uint32_t totalDram = 400000);

  /**
   * Get PSRAM usage percentage
   * @param totalPsram Total PSRAM capacity (default 8388608 bytes = 8MB)
   * @return PSRAM usage percentage (0-100)
   */
  static float getPsramUsagePercent(uint32_t totalPsram = 8388608);

  /**
   * Manually trigger proactive memory cleanup
   * Useful before large operations (e.g., reading device with 45+ registers)
   * @return Number of bytes freed
   */
  static uint32_t triggerCleanup();

private:
  /**
   * Execute queue flush recovery
   * @param entriesToFlush Number of queue entries to remove
   * @return Number of entries actually flushed
   */
  static int executeQueueFlush(int entriesToFlush = 20);

  /**
   * Execute MQTT persistent queue cleanup
   * @return Number of messages cleared
   */
  static int executeMqttQueueCleanup();

  /**
   * Execute garbage collection
   * @return true if GC succeeded
   */
  static bool executeGarbageCollection();

  /**
   * Execute emergency restart
   * Logs critical info before restart
   */
  static void executeEmergencyRestart();
};

#endif // MEMORY_RECOVERY_H
