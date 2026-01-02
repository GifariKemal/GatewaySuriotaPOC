#include "PSRAMValidator.h"

#include <esp_heap_caps.h>

#include "DebugConfig.h"

// Singleton instance
PSRAMValidator* PSRAMValidator::instance = nullptr;

PSRAMValidator::PSRAMValidator() {
  // v2.5.2: Use runtime check instead of compile-time
  if (!IS_PRODUCTION_MODE()) {
    Serial.println("[PSRAM] Validator initialized");
  }
  lastCheckTime = millis();
}

PSRAMValidator* PSRAMValidator::getInstance() {
  // Thread-safe Meyers Singleton (C++11 guarantees thread-safe static init)
  static PSRAMValidator instance;
  static PSRAMValidator* ptr = &instance;
  return ptr;
}

// Pre-allocation validation
bool PSRAMValidator::canAllocate(size_t requestedSize, const char* component) {
  // Check if requested size is too large
  if (requestedSize > maxAllocationSize) {
    // v2.5.2: Error logs only in development mode
    if (!IS_PRODUCTION_MODE()) {
      Serial.printf(
          "[PSRAM] ERROR: Allocation %zu bytes exceeds max %zu bytes\n",
          requestedSize, maxAllocationSize);
    }
    return false;
  }

  // Get current free PSRAM
  size_t freePSRAM = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

  // Must have requested size + safety margin
  size_t requiredFree = requestedSize + minSafeThreshold;

  if (freePSRAM < requiredFree) {
    // v2.5.2: Warning logs only in development mode
    if (!IS_PRODUCTION_MODE()) {
      Serial.printf("[PSRAM] WARNING: Insufficient memory for %s\n", component);
      Serial.printf("  Requested: %zu bytes\n", requestedSize);
      Serial.printf("  Required (+ margin): %zu bytes\n", requiredFree);
      Serial.printf("  Available: %zu bytes\n", freePSRAM);
    }
    return false;
  }

  return true;
}

bool PSRAMValidator::canAllocateMultiple(const std::vector<size_t>& sizes,
                                         const char* component) {
  size_t totalSize = 0;

  // Calculate total size
  for (size_t size : sizes) {
    if (size > maxAllocationSize) {
      // v2.5.2: Error log only in development mode
      if (!IS_PRODUCTION_MODE()) {
        Serial.printf("[PSRAM] ERROR: Single allocation %zu exceeds max %zu\n",
                      size, maxAllocationSize);
      }
      return false;
    }
    totalSize += size;
  }

  // Get current free PSRAM
  size_t freePSRAM = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  size_t requiredFree = totalSize + minSafeThreshold;

  if (freePSRAM < requiredFree) {
    // v2.5.2: Warning log only in development mode
    if (!IS_PRODUCTION_MODE()) {
      Serial.printf(
          "[PSRAM] WARNING: Cannot allocate %d items for %s (total %zu "
          "bytes)\n",
          sizes.size(), component, totalSize);
      Serial.printf("  Available: %zu bytes, Required: %zu bytes\n", freePSRAM,
                    requiredFree);
    }
    return false;
  }

  return true;
}

// Safe allocation wrappers
void* PSRAMValidator::allocatePSRAM(size_t size, const char* component) {
  // Pre-check before allocation
  if (!canAllocate(size, component)) {
    return nullptr;
  }

  // Attempt allocation
  void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  if (!ptr) {
    // v2.5.2: Error log only in development mode
    if (!IS_PRODUCTION_MODE()) {
      Serial.printf("[PSRAM] ERROR: Failed to allocate %zu bytes for %s\n",
                    size, component);
    }
    return nullptr;
  }

  // v2.5.2: Allocation log only in development mode
  if (!IS_PRODUCTION_MODE()) {
    Serial.printf("[PSRAM] Allocated %zu bytes for %s\n", size, component);
  }

  return ptr;
}

void PSRAMValidator::freePSRAM(void* ptr, const char* component) {
  if (!ptr) {
    // v2.5.2: Warning log only in development mode
    if (!IS_PRODUCTION_MODE()) {
      Serial.printf("[PSRAM] WARNING: Attempt to free null pointer from %s\n",
                    component);
    }
    return;
  }

  // Tracking removed for performance
  heap_caps_free(ptr);
}

// Memory statistics
MemoryStats PSRAMValidator::getMemoryStats() {
  MemoryStats stats;

  // PSRAM stats
  stats.totalPSRAM = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
  stats.freePSRAM = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  stats.usedPSRAM = stats.totalPSRAM - stats.freePSRAM;

  // DRAM stats
  size_t totalDRAM = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
  stats.freeDRAM = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  stats.usedDRAM = totalDRAM - stats.freeDRAM;

  // Utilization percentages
  stats.psramUtilization = (stats.totalPSRAM > 0)
                               ? (100.0f * stats.usedPSRAM / stats.totalPSRAM)
                               : 0.0f;
  stats.dramUtilization =
      (totalDRAM > 0) ? (100.0f * stats.usedDRAM / totalDRAM) : 0.0f;

  // Allocation tracking removed - always return 0
  stats.allocationCount = 0;

  // Fragmentation and block info
  stats.largestFreeBlock = getLargestFreeBlock();
  stats.fragmentation = calculateFragmentationIndex();

  // Average allocation size
  if (stats.allocationCount > 0) {
    stats.avgAllocationSize = stats.usedPSRAM / stats.allocationCount;
  } else {
    stats.avgAllocationSize = 0;
  }

  // Update peak usage
  if (stats.usedPSRAM > peakMemoryUsage) {
    peakMemoryUsage = stats.usedPSRAM;
  }

  lastCheckTime = millis();
  return stats;
}

MemoryHealthStatus PSRAMValidator::getHealthStatus() {
  size_t freePSRAM = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

  if (freePSRAM < criticalThreshold) {
    return MEMORY_INSUFFICIENT;
  } else if (freePSRAM < (criticalThreshold + warningThreshold) / 2) {
    return MEMORY_CRITICAL;
  } else if (freePSRAM < warningThreshold) {
    return MEMORY_WARNING;
  } else {
    return MEMORY_HEALTHY;
  }
}

// Configuration setters
void PSRAMValidator::setMinSafeThreshold(size_t bytes) {
  minSafeThreshold = bytes;
  // v2.5.2: Config log only in development mode
  if (!IS_PRODUCTION_MODE()) {
    Serial.printf("[PSRAM] Min safe threshold set to %zu bytes\n", bytes);
  }
}

void PSRAMValidator::setCriticalThreshold(size_t bytes) {
  criticalThreshold = bytes;
  // v2.5.2: Config log only in development mode
  if (!IS_PRODUCTION_MODE()) {
    Serial.printf("[PSRAM] Critical threshold set to %zu bytes\n", bytes);
  }
}

void PSRAMValidator::setWarningThreshold(size_t bytes) {
  warningThreshold = bytes;
  // v2.5.2: Config log only in development mode
  if (!IS_PRODUCTION_MODE()) {
    Serial.printf("[PSRAM] Warning threshold set to %zu bytes\n", bytes);
  }
}

void PSRAMValidator::setMaxAllocationSize(size_t bytes) {
  maxAllocationSize = bytes;
  // v2.5.2: Config log only in development mode
  if (!IS_PRODUCTION_MODE()) {
    Serial.printf("[PSRAM] Max allocation size set to %zu bytes\n", bytes);
  }
}

// Fragmentation analysis
uint16_t PSRAMValidator::calculateFragmentationIndex() {
  // Simple fragmentation calculation
  // Based on ratio of largest free block to total free space

  size_t freePSRAM = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  size_t largestBlock = getLargestFreeBlock();

  if (freePSRAM == 0) return 0;

  // Fragmentation index: 0 = no fragmentation, 100 = highly fragmented
  // If largest block is same as total free = 0 fragmentation
  // If largest block is much smaller = high fragmentation
  uint16_t index = 100 - ((largestBlock * 100) / freePSRAM);

  return (index > 100) ? 100 : index;
}

size_t PSRAMValidator::getLargestFreeBlock() {
  // This is an ESP32-specific call
  // Returns the size of the largest contiguous block of free memory
  return heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
}

bool PSRAMValidator::isFragmentationHigh() {
  return calculateFragmentationIndex() > 60;  // >60% is high
}

// Memory checks
bool PSRAMValidator::isMemoryHealthy() {
  return getHealthStatus() == MEMORY_HEALTHY;
}

bool PSRAMValidator::isMemoryWarning() {
  MemoryHealthStatus status = getHealthStatus();
  return status == MEMORY_WARNING || status == MEMORY_CRITICAL ||
         status == MEMORY_INSUFFICIENT;
}

bool PSRAMValidator::isMemoryCritical() {
  MemoryHealthStatus status = getHealthStatus();
  return status == MEMORY_CRITICAL || status == MEMORY_INSUFFICIENT;
}

bool PSRAMValidator::hasEnoughMemory() {
  return getHealthStatus() != MEMORY_INSUFFICIENT;
}

// Reporting and diagnostics
void PSRAMValidator::printMemoryStatus() {
  // v2.5.2: Skip memory status printing in production mode
  if (IS_PRODUCTION_MODE()) {
    return;
  }

  MemoryStats stats = getMemoryStats();
  MemoryHealthStatus health = getHealthStatus();

  Serial.println("\n[PSRAM] MEMORY STATUS");
  Serial.printf("  Status: %s\n", getHealthStatusString(health));
  Serial.printf("  PSRAM: %zu / %zu bytes (%.1f%%)\n", stats.usedPSRAM,
                stats.totalPSRAM, stats.psramUtilization);
  Serial.printf("  DRAM:  %zu bytes (%.1f%% of %zu)\n", stats.freeDRAM,
                stats.dramUtilization, stats.usedDRAM + stats.freeDRAM);
  Serial.printf("  Free: %zu KB, Largest block: %zu KB\n",
                stats.freePSRAM / 1024, stats.largestFreeBlock / 1024);
  Serial.printf("  Fragmentation: %d%%\n\n", stats.fragmentation);
}

void PSRAMValidator::printDetailedStats() {
  // v2.5.2: Skip detailed stats printing in production mode
  if (IS_PRODUCTION_MODE()) {
    return;
  }

  MemoryStats stats = getMemoryStats();

  Serial.println("\n[PSRAM] DETAILED MEMORY STATISTICS");
  Serial.printf("  Total PSRAM:        %zu KB (%zu bytes)\n",
                stats.totalPSRAM / 1024, stats.totalPSRAM);
  Serial.printf("  Used PSRAM:         %zu KB (%.1f%%)\n",
                stats.usedPSRAM / 1024, stats.psramUtilization);
  Serial.printf("  Free PSRAM:         %zu KB\n", stats.freePSRAM / 1024);
  Serial.printf("  Largest free block: %zu KB\n",
                stats.largestFreeBlock / 1024);
  Serial.printf("  Fragmentation:      %d%%\n", stats.fragmentation);
  Serial.printf("  Allocations:        %lu active\n", stats.allocationCount);
  Serial.printf("  Avg alloc size:     %zu bytes\n", stats.avgAllocationSize);
  Serial.printf("  Peak memory usage:  %zu KB\n", peakMemoryUsage / 1024);
  Serial.printf("  Peak allocations:   %lu\n\n", peakAllocationCount);
}

void PSRAMValidator::printMemoryWarnings() {
  // v2.5.2: Skip memory warnings printing in production mode
  if (IS_PRODUCTION_MODE()) {
    return;
  }

  MemoryStats stats = getMemoryStats();
  MemoryHealthStatus health = getHealthStatus();

  if (health != MEMORY_HEALTHY) {
    Serial.printf("[PSRAM]  WARNING: Memory status is %s!\n",
                  getHealthStatusString(health));

    if (health == MEMORY_CRITICAL || health == MEMORY_INSUFFICIENT) {
      Serial.printf("[PSRAM] CRITICAL: Only %zu KB free\n",
                    stats.freePSRAM / 1024);
      Serial.println("[PSRAM] Consider freeing memory or reducing allocations");
    }
  }

  if (isFragmentationHigh()) {
    Serial.printf("[PSRAM]  HIGH FRAGMENTATION: %d%% \n", stats.fragmentation);
    Serial.printf("[PSRAM]    Largest block: %zu KB, Total free: %zu KB\n",
                  stats.largestFreeBlock / 1024, stats.freePSRAM / 1024);
  }
}

// Peak tracking
size_t PSRAMValidator::getPeakMemoryUsage() const { return peakMemoryUsage; }

size_t PSRAMValidator::getPeakAllocationCount() const {
  return peakAllocationCount;
}

void PSRAMValidator::resetPeakTracking() {
  peakMemoryUsage = 0;
  peakAllocationCount = 0;
  // v2.5.2: Reset log only in development mode
  if (!IS_PRODUCTION_MODE()) {
    Serial.println("[PSRAM] Peak tracking reset");
  }
}

// Utility methods
const char* PSRAMValidator::getHealthStatusString(MemoryHealthStatus status) {
  switch (status) {
    case MEMORY_HEALTHY:
      return "HEALTHY (>40% free)";
    case MEMORY_WARNING:
      return " WARNING (20-40% free)";
    case MEMORY_CRITICAL:
      return " CRITICAL (10-20% free)";
    case MEMORY_INSUFFICIENT:
      return "INSUFFICIENT (<10% free)";
    default:
      return " UNKNOWN";
  }
}

void PSRAMValidator::logMemoryEvent(const char* event, const char* component,
                                    size_t size) {
  // v2.5.2: Event log only in development mode
  if (IS_PRODUCTION_MODE()) {
    return;
  }

  MemoryStats stats = getMemoryStats();
  Serial.printf("[PSRAM] EVENT: %s | %s | Size: %zu KB | Free: %zu KB\n", event,
                component, size / 1024, stats.freePSRAM / 1024);
}

PSRAMValidator::~PSRAMValidator() {
  // v2.5.2: Destructor log only in development mode
  if (!IS_PRODUCTION_MODE()) {
    Serial.println("[PSRAM] Validator destroyed");
  }
}
