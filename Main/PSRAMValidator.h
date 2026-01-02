#ifndef PSRAM_VALIDATOR_H
#define PSRAM_VALIDATOR_H

#include <Arduino.h>
#include <esp_heap_caps.h>

#include <vector>

/*
 * @brief PSRAM Validation and Memory Management System
 *
 * Provides pre-allocation validation, fragmentation prevention, and memory
 * safety checks for ESP32-S3 OPI PSRAM usage.
 *
 * Features:
 * - Pre-allocation validation before attempting allocations
 * - Memory fragmentation tracking
 * - Allocation size limits and safety checks
 * - Memory statistics and reporting
 * - Thresholds for low-memory warnings
 */

// Memory allocation tracking structure
struct MemoryAllocation {
  void* address;            // Allocated memory address
  size_t size;              // Allocation size
  unsigned long timestamp;  // When allocated (millis())
  const char* allocator;    // Which component allocated
  bool isLarge;             // True if size > 1KB
};

// Memory statistics structure
struct MemoryStats {
  size_t totalPSRAM;         // Total PSRAM available
  size_t freePSRAM;          // Currently free PSRAM
  size_t usedPSRAM;          // Currently used PSRAM
  size_t freeDRAM;           // Free internal SRAM
  size_t usedDRAM;           // Used internal SRAM
  float psramUtilization;    // Percentage (0-100)
  float dramUtilization;     // Percentage (0-100)
  uint32_t allocationCount;  // Number of active allocations
  size_t largestFreeBlock;   // Largest contiguous free block
  size_t avgAllocationSize;  // Average allocation size
  uint16_t fragmentation;    // Fragmentation index (0-100)
};

// Memory health status
enum MemoryHealthStatus {
  MEMORY_HEALTHY = 0,      // >40% free
  MEMORY_WARNING = 1,      // 20-40% free
  MEMORY_CRITICAL = 2,     // 10-20% free
  MEMORY_INSUFFICIENT = 3  // <10% free
};

class PSRAMValidator {
 private:
  static PSRAMValidator* instance;

  // Configuration thresholds
  size_t minSafeThreshold = 50 * 1024;       // 50 KB minimum safe threshold
  size_t criticalThreshold = 20 * 1024;      // 20 KB critical threshold
  size_t warningThreshold = 100 * 1024;      // 100 KB warning threshold
  size_t maxAllocationSize = 500 * 1024;     // 500 KB max single allocation
  size_t minFragmentationCheck = 10 * 1024;  // Check fragmentation >10KB

  // Memory profiling (tracking removed for performance)
  size_t peakMemoryUsage = 0;
  size_t peakAllocationCount = 0;
  unsigned long lastCheckTime = 0;

  // Private constructor for singleton
  PSRAMValidator();

 public:
  // Singleton access
  static PSRAMValidator* getInstance();

  // Pre-allocation validation
  bool canAllocate(size_t requestedSize, const char* component = "UNKNOWN");
  bool canAllocateMultiple(const std::vector<size_t>& sizes,
                           const char* component = "UNKNOWN");

  // Safe allocation wrappers
  void* allocatePSRAM(size_t size, const char* component = "UNKNOWN");
  void freePSRAM(void* ptr, const char* component = "UNKNOWN");

  // Memory statistics
  MemoryStats getMemoryStats();
  MemoryHealthStatus getHealthStatus();

  // Thresholds and configuration
  void setMinSafeThreshold(size_t bytes);
  void setCriticalThreshold(size_t bytes);
  void setWarningThreshold(size_t bytes);
  void setMaxAllocationSize(size_t bytes);

  // Fragmentation analysis
  uint16_t calculateFragmentationIndex();
  size_t getLargestFreeBlock();
  bool isFragmentationHigh();

  // Memory checks
  bool isMemoryHealthy();
  bool isMemoryWarning();
  bool isMemoryCritical();
  bool hasEnoughMemory();

  // Reporting and diagnostics
  void printMemoryStatus();
  void printDetailedStats();
  void printMemoryWarnings();

  // Peak tracking
  size_t getPeakMemoryUsage() const;
  size_t getPeakAllocationCount() const;
  void resetPeakTracking();

  // Utility methods
  const char* getHealthStatusString(MemoryHealthStatus status);
  void logMemoryEvent(const char* event, const char* component, size_t size);

  // Destructor
  ~PSRAMValidator();
};

#endif  // PSRAM_VALIDATOR_H
