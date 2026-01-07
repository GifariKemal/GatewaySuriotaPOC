/**
 * SDCardManager.h - SD Card Manager for SRT-MGATE-1210
 * =====================================================
 *
 * Features:
 * - Hot-plug detection with auto-reconnect
 * - FreeRTOS task integration (non-blocking)
 * - Mutex-protected thread-safe operations
 * - Switchable SPI/SDMMC modes
 * - Memory recovery integration
 * - Detailed status reporting
 *
 * Hardware Configuration:
 * -----------------------
 * ETHERNET (Default SPI - DO NOT MODIFY):
 *   - CS:   GPIO 48
 *   - MOSI: GPIO 14
 *   - MISO: GPIO 21
 *   - SCK:  GPIO 47
 *
 * SD CARD (Using HSPI/SPI3 - Separate Bus):
 *   - CS:   GPIO 11
 *   - MOSI: GPIO 10
 *   - MISO: GPIO 13
 *   - SCK:  GPIO 12
 *
 * Author: SURIOTA R&D Team
 * Date: January 2025
 */

#ifndef SD_CARD_MANAGER_H
#define SD_CARD_MANAGER_H

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>

// =============================================================================
// SPI HOST DEFINITION (ESP32 Arduino Core 3.x compatibility)
// =============================================================================
// ESP32-S3 has SPI2 (FSPI/default) and SPI3 (HSPI)
// HSPI is the second user-accessible SPI bus (separate from Ethernet's default)
// In Arduino Core 3.x, HSPI = 2 for ESP32-S3
#ifndef HSPI
#define HSPI 2
#endif

// =============================================================================
// MODE SELECTION
// =============================================================================
#define SD_MODE_SPI 1    // Use HSPI (separate from Ethernet's default SPI)
#define SD_MODE_SDMMC 2  // Use native SDMMC peripheral (faster, no SPI)

#ifndef SD_MODE
#define SD_MODE SD_MODE_SPI  // Default to SPI mode
#endif

#if SD_MODE == SD_MODE_SDMMC
#include <SD_MMC.h>
#endif

// =============================================================================
// PIN DEFINITIONS
// =============================================================================
#define SD_CS_PIN 11
#define SD_MOSI_PIN 10
#define SD_MISO_PIN 13
#define SD_SCK_PIN 12

// =============================================================================
// CONFIGURATION CONSTANTS
// =============================================================================
#define SD_SPI_FREQUENCY 4000000       // 4 MHz (conservative for stability)
#define SD_CHECK_INTERVAL_MS 2000      // Check card presence every 2 seconds
#define SD_RECONNECT_DELAY_MS 1000     // Delay before reconnect attempt
#define SD_MAX_RECONNECT_ATTEMPTS 3    // Max reconnect attempts before ERROR
#define SD_LOG_INTERVAL_MS 5000        // Auto-log interval
#define SD_MUTEX_TIMEOUT_MS 500        // Mutex acquisition timeout
#define SD_TASK_STACK_SIZE 8192        // 8KB stack for SD task
#define SD_TASK_PRIORITY 1             // Same priority as service tasks
#define SD_TASK_CORE 0                 // Core 0 (background processing)

// =============================================================================
// LOG MACROS (Mimics main firmware DebugConfig.h pattern)
// =============================================================================
#ifndef LOG_SD_ERROR
#define LOG_SD_ERROR(fmt, ...) Serial.printf("[SD] ERROR: " fmt "\n", ##__VA_ARGS__)
#endif

#ifndef LOG_SD_WARN
#define LOG_SD_WARN(fmt, ...) Serial.printf("[SD] WARN: " fmt "\n", ##__VA_ARGS__)
#endif

#ifndef LOG_SD_INFO
#define LOG_SD_INFO(fmt, ...) Serial.printf("[SD] " fmt "\n", ##__VA_ARGS__)
#endif

#ifndef LOG_SD_DEBUG
#define LOG_SD_DEBUG(fmt, ...) Serial.printf("[SD] DBG: " fmt "\n", ##__VA_ARGS__)
#endif

// =============================================================================
// LOG THROTTLE CLASS (From main firmware)
// =============================================================================
class LogThrottle {
 private:
  unsigned long lastLogTime;
  unsigned long intervalMs;

 public:
  LogThrottle(unsigned long interval = 30000) : lastLogTime(0), intervalMs(interval) {}

  bool canLog() {
    unsigned long now = millis();
    if (now - lastLogTime >= intervalMs) {
      lastLogTime = now;
      return true;
    }
    return false;
  }

  void reset() { lastLogTime = 0; }
};

// =============================================================================
// SD CARD STATE ENUM
// =============================================================================
enum SDCardState {
  SD_NOT_INITIALIZED = 0,  // Initial state
  SD_INITIALIZING,         // Currently initializing
  SD_READY,                // Card mounted and ready
  SD_REMOVED,              // Card physically removed
  SD_RECONNECTING,         // Attempting to reconnect
  SD_ERROR,                // Error state (max retries reached)
  SD_DISABLED              // Manually disabled
};

// =============================================================================
// SD CARD INFO STRUCT
// =============================================================================
struct SDCardInfo {
  SDCardState state;
  uint8_t cardType;       // CARD_NONE, CARD_MMC, CARD_SD, CARD_SDHC
  uint64_t cardSizeMB;    // Card size in MB
  uint64_t totalSpaceMB;  // Total usable space in MB
  uint64_t usedSpaceMB;   // Used space in MB
  uint64_t freeSpaceMB;   // Free space in MB

  // Statistics
  uint32_t mountCount;       // Number of successful mounts
  uint32_t unmountCount;     // Number of unmounts (removals)
  uint32_t errorCount;       // Number of errors encountered
  uint32_t successfulWrites; // Number of successful write operations
  uint32_t failedWrites;     // Number of failed write operations
  uint32_t logEntries;       // Number of log entries written
  uint32_t reconnectAttempts;// Current reconnect attempt counter
};

// =============================================================================
// SD CARD MANAGER CLASS (Singleton Pattern)
// =============================================================================
class SDCardManager {
 private:
  // Singleton instance
  static SDCardManager* instance;

  // State
  SDCardState currentState;
  SDCardInfo info;
  bool taskRunning;
  bool autoLogEnabled;
  String currentLogFile;

  // FreeRTOS handles
  SemaphoreHandle_t sdMutex;
  TaskHandle_t sdTaskHandle;

  // SPI instance (for SPI mode)
#if SD_MODE == SD_MODE_SPI
  SPIClass* sdSPI;
#endif

  // Throttled logging
  LogThrottle stateChangeThrottle;
  LogThrottle statusThrottle;

  // Timing
  unsigned long lastCardCheck;
  unsigned long lastLogTime;
  unsigned long lastStateChange;

  // Private constructor (singleton)
  SDCardManager();

  // Internal methods
  bool mountCard();
  void unmountCard();
  bool checkCardPresence();
  void updateCardInfo();
  void handleStateTransition(SDCardState newState);

 public:
  // Singleton accessor
  static SDCardManager* getInstance();

  // Prevent copying
  SDCardManager(const SDCardManager&) = delete;
  SDCardManager& operator=(const SDCardManager&) = delete;

  // Destructor
  ~SDCardManager();

  // ==========================================================================
  // INITIALIZATION
  // ==========================================================================

  /**
   * Initialize the SD Card Manager
   * @return true if initialization successful (card may not be present)
   */
  bool init();

  /**
   * Start the background monitoring task
   * @return true if task started successfully
   */
  bool start();

  /**
   * Stop the background monitoring task
   */
  void stop();

  // ==========================================================================
  // STATE MANAGEMENT
  // ==========================================================================

  /**
   * Get current state
   * @return Current SDCardState
   */
  SDCardState getState() const { return currentState; }

  /**
   * Get state as string
   * @return Human-readable state name
   */
  const char* getStateString() const;

  /**
   * Check if card is ready for operations
   * @return true if state is SD_READY
   */
  bool isReady() const { return currentState == SD_READY; }

  /**
   * Get complete card information
   * @return SDCardInfo struct with all statistics
   */
  SDCardInfo getInfo();

  // ==========================================================================
  // FILE OPERATIONS (Thread-safe)
  // ==========================================================================

  /**
   * Write a log entry to the current log file
   * @param message Log message to write
   * @return true if write successful
   */
  bool writeLog(const char* message);

  /**
   * Write data to a specific file
   * @param path File path
   * @param data Data to write
   * @param append true to append, false to overwrite
   * @return true if write successful
   */
  bool writeFile(const char* path, const char* data, bool append = true);

  /**
   * Read data from a file
   * @param path File path
   * @param buffer Output buffer
   * @param maxLen Maximum bytes to read
   * @return Number of bytes read, -1 on error
   */
  int readFile(const char* path, char* buffer, size_t maxLen);

  /**
   * Check if file exists
   * @param path File path
   * @return true if file exists
   */
  bool fileExists(const char* path);

  /**
   * Delete a file
   * @param path File path
   * @return true if deletion successful
   */
  bool deleteFile(const char* path);

  /**
   * Create a directory
   * @param path Directory path
   * @return true if creation successful
   */
  bool createDir(const char* path);

  /**
   * List directory contents to Serial
   * @param path Directory path
   * @param levels Depth to recurse
   */
  void listDir(const char* path, uint8_t levels = 1);

  // ==========================================================================
  // STATUS REPORTING
  // ==========================================================================

  /**
   * Print detailed status to Serial
   */
  void printStatus();

  /**
   * Get status as JSON string
   * @param buffer Output buffer
   * @param bufferSize Buffer size
   * @return true if JSON generated successfully
   */
  bool getStatusJson(char* buffer, size_t bufferSize);

  /**
   * Print memory diagnostics
   */
  void printMemoryDiagnostics();

  // ==========================================================================
  // CONFIGURATION
  // ==========================================================================

  /**
   * Enable/disable automatic logging
   * @param enabled true to enable auto-logging
   */
  void setAutoLog(bool enabled) { autoLogEnabled = enabled; }

  /**
   * Check if auto-logging is enabled
   * @return true if auto-logging enabled
   */
  bool isAutoLogEnabled() const { return autoLogEnabled; }

  /**
   * Force reconnection attempt
   * @return true if reconnection started
   */
  bool forceReconnect();

  /**
   * Disable SD card operations
   */
  void disable();

  /**
   * Enable SD card operations
   */
  void enable();

  // ==========================================================================
  // TASK FUNCTION (Called by FreeRTOS)
  // ==========================================================================
  void taskLoop();
};

#endif  // SD_CARD_MANAGER_H
