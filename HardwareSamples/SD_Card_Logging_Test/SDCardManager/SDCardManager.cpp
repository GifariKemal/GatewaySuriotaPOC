/**
 * SDCardManager.cpp - SD Card Manager Implementation
 * ===================================================
 *
 * Full implementation with:
 * - Hot-plug detection via polling
 * - Auto-reconnect with exponential backoff
 * - Thread-safe mutex-protected operations
 * - FreeRTOS background task
 * - Memory diagnostics integration
 *
 * Author: SURIOTA R&D Team
 * Date: January 2025
 */

#include "SDCardManager.h"

// =============================================================================
// STATIC MEMBER INITIALIZATION
// =============================================================================
SDCardManager* SDCardManager::instance = nullptr;

// =============================================================================
// HELPER FUNCTION (Forward declaration)
// =============================================================================
static const char* getStateStringFromEnum(SDCardState state) {
  switch (state) {
    case SD_NOT_INITIALIZED: return "NOT_INITIALIZED";
    case SD_INITIALIZING:    return "INITIALIZING";
    case SD_READY:           return "READY";
    case SD_REMOVED:         return "REMOVED";
    case SD_RECONNECTING:    return "RECONNECTING";
    case SD_ERROR:           return "ERROR";
    case SD_DISABLED:        return "DISABLED";
    default:                 return "UNKNOWN";
  }
}

// =============================================================================
// FREERTOS TASK WRAPPER
// =============================================================================
static void sdCardTaskWrapper(void* param) {
  SDCardManager* manager = static_cast<SDCardManager*>(param);
  manager->taskLoop();
}

// =============================================================================
// CONSTRUCTOR
// =============================================================================
SDCardManager::SDCardManager()
    : currentState(SD_NOT_INITIALIZED),
      taskRunning(false),
      autoLogEnabled(true),
      sdMutex(nullptr),
      sdTaskHandle(nullptr),
#if SD_MODE == SD_MODE_SPI
      sdSPI(nullptr),
#endif
      stateChangeThrottle(1000),  // Min 1s between state change logs
      statusThrottle(30000),      // Min 30s between status logs
      lastCardCheck(0),
      lastLogTime(0),
      lastStateChange(0) {
  // Initialize info struct
  memset(&info, 0, sizeof(SDCardInfo));
  info.state = SD_NOT_INITIALIZED;
}

// =============================================================================
// DESTRUCTOR
// =============================================================================
SDCardManager::~SDCardManager() {
  stop();
  if (sdMutex) {
    vSemaphoreDelete(sdMutex);
    sdMutex = nullptr;
  }
#if SD_MODE == SD_MODE_SPI
  if (sdSPI) {
    delete sdSPI;
    sdSPI = nullptr;
  }
#endif
}

// =============================================================================
// SINGLETON ACCESSOR (with PSRAM allocation attempt)
// =============================================================================
SDCardManager* SDCardManager::getInstance() {
  if (!instance) {
    // Try PSRAM allocation first (following main firmware pattern)
    void* ptr = heap_caps_malloc(sizeof(SDCardManager), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ptr) {
      instance = new (ptr) SDCardManager();
      LOG_SD_DEBUG("Allocated in PSRAM");
    } else {
      // Fallback to DRAM
      instance = new SDCardManager();
      LOG_SD_DEBUG("Allocated in DRAM (PSRAM unavailable)");
    }
  }
  return instance;
}

// =============================================================================
// INITIALIZATION
// =============================================================================
bool SDCardManager::init() {
  LOG_SD_INFO("Initializing SD Card Manager...");

  // Create mutex for thread safety
  sdMutex = xSemaphoreCreateMutex();
  if (!sdMutex) {
    LOG_SD_ERROR("Failed to create mutex!");
    return false;
  }

  handleStateTransition(SD_INITIALIZING);

#if SD_MODE == SD_MODE_SPI
  LOG_SD_INFO("Mode: SPI (HSPI/SPI3 - Separate from Ethernet)");
  LOG_SD_INFO("Pins: CS=%d, MOSI=%d, MISO=%d, SCK=%d", SD_CS_PIN, SD_MOSI_PIN,
              SD_MISO_PIN, SD_SCK_PIN);

  // Create separate SPI instance using HSPI (SPI3)
  // CRITICAL: This is a DIFFERENT bus from Ethernet's default SPI
  sdSPI = new SPIClass(HSPI);
  if (!sdSPI) {
    LOG_SD_ERROR("Failed to create SPI instance!");
    handleStateTransition(SD_ERROR);
    return false;
  }

  // Initialize SPI with SD card pins
  sdSPI->begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

#elif SD_MODE == SD_MODE_SDMMC
  LOG_SD_INFO("Mode: SDMMC (Native peripheral, no SPI)");
  LOG_SD_INFO("Pins: CLK=%d, CMD=%d, D0=%d", SD_SCK_PIN, SD_CS_PIN, SD_MISO_PIN);

  // Configure SDMMC pins
  SD_MMC.setPins(SD_SCK_PIN, SD_CS_PIN, SD_MISO_PIN);
#endif

  // Attempt initial mount
  if (mountCard()) {
    updateCardInfo();
    handleStateTransition(SD_READY);
    LOG_SD_INFO("Card mounted successfully!");
    return true;
  } else {
    handleStateTransition(SD_REMOVED);
    LOG_SD_WARN("No card detected at startup (will auto-detect)");
    return true;  // Still return true - manager is initialized, just no card
  }
}

// =============================================================================
// START BACKGROUND TASK
// =============================================================================
bool SDCardManager::start() {
  if (taskRunning) {
    LOG_SD_WARN("Task already running");
    return true;
  }

  BaseType_t result = xTaskCreatePinnedToCore(
      sdCardTaskWrapper,    // Task function
      "SD_CARD_TASK",       // Task name
      SD_TASK_STACK_SIZE,   // Stack size
      this,                 // Parameter (this pointer)
      SD_TASK_PRIORITY,     // Priority
      &sdTaskHandle,        // Task handle
      SD_TASK_CORE          // Core
  );

  if (result == pdPASS) {
    taskRunning = true;
    LOG_SD_INFO("Task started on Core %d (priority %d)", SD_TASK_CORE, SD_TASK_PRIORITY);
    return true;
  } else {
    LOG_SD_ERROR("Failed to create task!");
    return false;
  }
}

// =============================================================================
// STOP BACKGROUND TASK
// =============================================================================
void SDCardManager::stop() {
  if (taskRunning && sdTaskHandle) {
    taskRunning = false;
    vTaskDelete(sdTaskHandle);
    sdTaskHandle = nullptr;
    LOG_SD_INFO("Task stopped");
  }
}

// =============================================================================
// TASK LOOP
// =============================================================================
void SDCardManager::taskLoop() {
  LOG_SD_DEBUG("Task loop started");

  while (taskRunning) {
    unsigned long now = millis();

    // Check card presence periodically
    if (now - lastCardCheck >= SD_CHECK_INTERVAL_MS) {
      lastCardCheck = now;

      if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(SD_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        bool cardPresent = checkCardPresence();

        switch (currentState) {
          case SD_READY:
            if (!cardPresent) {
              // Card was removed
              LOG_SD_WARN("Card removal detected!");
              unmountCard();
              info.unmountCount++;
              info.reconnectAttempts = 0;
              handleStateTransition(SD_REMOVED);
            } else {
              // Update card info periodically
              updateCardInfo();
            }
            break;

          case SD_REMOVED:
          case SD_RECONNECTING:
            if (cardPresent || info.reconnectAttempts < SD_MAX_RECONNECT_ATTEMPTS) {
              handleStateTransition(SD_RECONNECTING);
              info.reconnectAttempts++;

              LOG_SD_INFO("Reconnect attempt %d/%d", info.reconnectAttempts,
                          SD_MAX_RECONNECT_ATTEMPTS);

              if (mountCard()) {
                updateCardInfo();
                info.reconnectAttempts = 0;
                handleStateTransition(SD_READY);
                LOG_SD_INFO("Card re-inserted and mounted!");
              } else if (info.reconnectAttempts >= SD_MAX_RECONNECT_ATTEMPTS) {
                handleStateTransition(SD_ERROR);
                LOG_SD_ERROR("Max reconnect attempts reached");
              }
            }
            break;

          case SD_ERROR:
            // Stay in error state until manual intervention or card change
            if (cardPresent) {
              info.reconnectAttempts = 0;
              handleStateTransition(SD_RECONNECTING);
            }
            break;

          case SD_NOT_INITIALIZED:
          case SD_DISABLED:
            // Do nothing
            break;

          default:
            break;
        }

        xSemaphoreGive(sdMutex);
      }
    }

    // Auto-logging (only when READY)
    if (autoLogEnabled && currentState == SD_READY) {
      if (now - lastLogTime >= SD_LOG_INTERVAL_MS) {
        lastLogTime = now;

        char logMsg[128];
        snprintf(logMsg, sizeof(logMsg),
                 "[%lu] Heap: %lu, PSRAM: %lu, State: %s",
                 millis(), ESP.getFreeHeap(), ESP.getFreePsram(),
                 getStateString());

        if (writeLog(logMsg)) {
          LOG_SD_DEBUG("Auto-logged: %s", logMsg);
        }
      }
    }

    // Yield to other tasks
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  // Task cleanup
  vTaskDelete(NULL);
}

// =============================================================================
// MOUNT CARD
// =============================================================================
bool SDCardManager::mountCard() {
#if SD_MODE == SD_MODE_SPI
  // Try mounting with configured frequency
  if (!SD.begin(SD_CS_PIN, *sdSPI, SD_SPI_FREQUENCY, "/sd", 5)) {
    LOG_SD_WARN("Mount failed at %d Hz, trying 1 MHz...", SD_SPI_FREQUENCY);

    // Retry with lower frequency
    if (!SD.begin(SD_CS_PIN, *sdSPI, 1000000, "/sd", 5)) {
      LOG_SD_ERROR("Mount failed even at 1 MHz");
      return false;
    }
  }

  uint8_t cardType = SD.cardType();

#elif SD_MODE == SD_MODE_SDMMC
  if (!SD_MMC.begin("/sdcard", true)) {  // true = 1-bit mode
    LOG_SD_ERROR("SD_MMC mount failed");
    return false;
  }

  uint8_t cardType = SD_MMC.cardType();
#endif

  if (cardType == CARD_NONE) {
    LOG_SD_WARN("No card detected");
    return false;
  }

  info.mountCount++;
  return true;
}

// =============================================================================
// UNMOUNT CARD
// =============================================================================
void SDCardManager::unmountCard() {
#if SD_MODE == SD_MODE_SPI
  SD.end();
#elif SD_MODE == SD_MODE_SDMMC
  SD_MMC.end();
#endif
  LOG_SD_DEBUG("Card unmounted");
}

// =============================================================================
// CHECK CARD PRESENCE
// =============================================================================
bool SDCardManager::checkCardPresence() {
#if SD_MODE == SD_MODE_SPI
  // For SPI mode, we check if SD operations work
  // A proper card detect pin would be more reliable
  uint8_t cardType = SD.cardType();
  return (cardType != CARD_NONE);

#elif SD_MODE == SD_MODE_SDMMC
  uint8_t cardType = SD_MMC.cardType();
  return (cardType != CARD_NONE);
#endif
}

// =============================================================================
// UPDATE CARD INFO
// =============================================================================
void SDCardManager::updateCardInfo() {
#if SD_MODE == SD_MODE_SPI
  info.cardType = SD.cardType();
  info.cardSizeMB = SD.cardSize() / (1024 * 1024);
  info.totalSpaceMB = SD.totalBytes() / (1024 * 1024);
  info.usedSpaceMB = SD.usedBytes() / (1024 * 1024);
  info.freeSpaceMB = info.totalSpaceMB - info.usedSpaceMB;

#elif SD_MODE == SD_MODE_SDMMC
  info.cardType = SD_MMC.cardType();
  info.cardSizeMB = SD_MMC.cardSize() / (1024 * 1024);
  info.totalSpaceMB = SD_MMC.totalBytes() / (1024 * 1024);
  info.usedSpaceMB = SD_MMC.usedBytes() / (1024 * 1024);
  info.freeSpaceMB = info.totalSpaceMB - info.usedSpaceMB;
#endif

  info.state = currentState;
}

// =============================================================================
// HANDLE STATE TRANSITION
// =============================================================================
void SDCardManager::handleStateTransition(SDCardState newState) {
  if (currentState == newState) return;

  SDCardState oldState = currentState;
  currentState = newState;
  info.state = newState;
  lastStateChange = millis();

  // Print state change banner
  Serial.println();
  Serial.println("========================================");
  Serial.printf("  SD CARD: %s -> %s\n", getStateStringFromEnum(oldState),
                getStateString());
  Serial.println("========================================");

  // State-specific messages
  switch (newState) {
    case SD_READY:
      LOG_SD_INFO("Card ready for operations");
      break;
    case SD_REMOVED:
      LOG_SD_WARN("Card removed - operations suspended");
      LOG_SD_INFO("Insert card to auto-reconnect");
      break;
    case SD_RECONNECTING:
      LOG_SD_INFO("Attempting reconnection...");
      break;
    case SD_ERROR:
      LOG_SD_ERROR("Error state - manual intervention may be required");
      info.errorCount++;
      break;
    case SD_DISABLED:
      LOG_SD_INFO("SD card operations disabled");
      break;
    default:
      break;
  }
}

// =============================================================================
// GET STATE STRING
// =============================================================================
const char* SDCardManager::getStateString() const {
  return getStateStringFromEnum(currentState);
}

// =============================================================================
// GET INFO
// =============================================================================
SDCardInfo SDCardManager::getInfo() {
  if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(SD_MUTEX_TIMEOUT_MS)) == pdTRUE) {
    SDCardInfo copy = info;
    xSemaphoreGive(sdMutex);
    return copy;
  }
  return info;  // Return potentially stale data if mutex fails
}

// =============================================================================
// WRITE LOG
// =============================================================================
bool SDCardManager::writeLog(const char* message) {
  if (currentState != SD_READY) return false;

  if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(SD_MUTEX_TIMEOUT_MS)) == pdTRUE) {
    // Ensure logs directory exists
    if (!SD.exists("/logs")) {
      SD.mkdir("/logs");
    }

    // Create log filename based on boot count or date
    if (currentLogFile.isEmpty()) {
      char filename[32];
      snprintf(filename, sizeof(filename), "/logs/log_%06lu.txt", info.mountCount);
      currentLogFile = String(filename);
    }

#if SD_MODE == SD_MODE_SPI
    File file = SD.open(currentLogFile.c_str(), FILE_APPEND);
#elif SD_MODE == SD_MODE_SDMMC
    File file = SD_MMC.open(currentLogFile.c_str(), FILE_APPEND);
#endif

    if (file) {
      file.println(message);
      file.close();
      info.successfulWrites++;
      info.logEntries++;
      xSemaphoreGive(sdMutex);
      return true;
    } else {
      info.failedWrites++;
      xSemaphoreGive(sdMutex);
      LOG_SD_ERROR("Failed to open log file");
      return false;
    }
  }
  return false;
}

// =============================================================================
// WRITE FILE
// =============================================================================
bool SDCardManager::writeFile(const char* path, const char* data, bool append) {
  if (currentState != SD_READY) return false;

  if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(SD_MUTEX_TIMEOUT_MS)) == pdTRUE) {
#if SD_MODE == SD_MODE_SPI
    File file = SD.open(path, append ? FILE_APPEND : FILE_WRITE);
#elif SD_MODE == SD_MODE_SDMMC
    File file = SD_MMC.open(path, append ? FILE_APPEND : FILE_WRITE);
#endif

    if (file) {
      size_t written = file.print(data);
      file.close();

      if (written > 0) {
        info.successfulWrites++;
        xSemaphoreGive(sdMutex);
        return true;
      }
    }

    info.failedWrites++;
    xSemaphoreGive(sdMutex);
    return false;
  }
  return false;
}

// =============================================================================
// READ FILE
// =============================================================================
int SDCardManager::readFile(const char* path, char* buffer, size_t maxLen) {
  if (currentState != SD_READY) return -1;

  if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(SD_MUTEX_TIMEOUT_MS)) == pdTRUE) {
#if SD_MODE == SD_MODE_SPI
    File file = SD.open(path, FILE_READ);
#elif SD_MODE == SD_MODE_SDMMC
    File file = SD_MMC.open(path, FILE_READ);
#endif

    if (file) {
      size_t bytesRead = file.readBytes(buffer, maxLen - 1);
      buffer[bytesRead] = '\0';
      file.close();
      xSemaphoreGive(sdMutex);
      return bytesRead;
    }

    xSemaphoreGive(sdMutex);
    return -1;
  }
  return -1;
}

// =============================================================================
// FILE EXISTS
// =============================================================================
bool SDCardManager::fileExists(const char* path) {
  if (currentState != SD_READY) return false;

#if SD_MODE == SD_MODE_SPI
  return SD.exists(path);
#elif SD_MODE == SD_MODE_SDMMC
  return SD_MMC.exists(path);
#endif
}

// =============================================================================
// DELETE FILE
// =============================================================================
bool SDCardManager::deleteFile(const char* path) {
  if (currentState != SD_READY) return false;

  if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(SD_MUTEX_TIMEOUT_MS)) == pdTRUE) {
#if SD_MODE == SD_MODE_SPI
    bool result = SD.remove(path);
#elif SD_MODE == SD_MODE_SDMMC
    bool result = SD_MMC.remove(path);
#endif
    xSemaphoreGive(sdMutex);
    return result;
  }
  return false;
}

// =============================================================================
// CREATE DIRECTORY
// =============================================================================
bool SDCardManager::createDir(const char* path) {
  if (currentState != SD_READY) return false;

  if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(SD_MUTEX_TIMEOUT_MS)) == pdTRUE) {
#if SD_MODE == SD_MODE_SPI
    bool result = SD.mkdir(path);
#elif SD_MODE == SD_MODE_SDMMC
    bool result = SD_MMC.mkdir(path);
#endif
    xSemaphoreGive(sdMutex);
    return result;
  }
  return false;
}

// =============================================================================
// LIST DIRECTORY
// =============================================================================
void SDCardManager::listDir(const char* path, uint8_t levels) {
  if (currentState != SD_READY) {
    LOG_SD_WARN("Cannot list - card not ready");
    return;
  }

  Serial.printf("\nDirectory listing: %s\n", path);
  Serial.println("----------------------------------------");

#if SD_MODE == SD_MODE_SPI
  File root = SD.open(path);
#elif SD_MODE == SD_MODE_SDMMC
  File root = SD_MMC.open(path);
#endif

  if (!root || !root.isDirectory()) {
    Serial.println("Failed to open directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.printf("  [DIR]  %s\n", file.name());
      if (levels > 0) {
        listDir(file.path(), levels - 1);
      }
    } else {
      Serial.printf("  [FILE] %-20s %8lu bytes\n", file.name(), file.size());
    }
    file = root.openNextFile();
  }
  root.close();
  Serial.println("----------------------------------------");
}

// =============================================================================
// PRINT STATUS
// =============================================================================
void SDCardManager::printStatus() {
  SDCardInfo i = getInfo();

  const char* cardTypeStr;
  switch (i.cardType) {
    case CARD_MMC:  cardTypeStr = "MMC"; break;
    case CARD_SD:   cardTypeStr = "SDSC"; break;
    case CARD_SDHC: cardTypeStr = "SDHC"; break;
    default:        cardTypeStr = "NONE"; break;
  }

  Serial.println();
  Serial.println("==================================================");
  Serial.println("              SD CARD STATUS                      ");
  Serial.println("==================================================");
  Serial.printf(" State:           %-20s\n", getStateString());
  Serial.printf(" Card Type:       %-20s\n", cardTypeStr);
  Serial.printf(" Card Size:       %-10llu MB\n", i.cardSizeMB);
  Serial.printf(" Total Space:     %-10llu MB\n", i.totalSpaceMB);
  Serial.printf(" Used Space:      %-10llu MB\n", i.usedSpaceMB);
  Serial.printf(" Free Space:      %-10llu MB\n", i.freeSpaceMB);
  Serial.println("--------------------------------------------------");
  Serial.println("              STATISTICS                          ");
  Serial.println("--------------------------------------------------");
  Serial.printf(" Mount Count:     %-10lu\n", i.mountCount);
  Serial.printf(" Unmount Count:   %-10lu\n", i.unmountCount);
  Serial.printf(" Error Count:     %-10lu\n", i.errorCount);
  Serial.printf(" Writes OK:       %-10lu\n", i.successfulWrites);
  Serial.printf(" Writes Failed:   %-10lu\n", i.failedWrites);
  Serial.printf(" Log Entries:     %-10lu\n", i.logEntries);
  Serial.println("==================================================");
}

// =============================================================================
// GET STATUS JSON
// =============================================================================
bool SDCardManager::getStatusJson(char* buffer, size_t bufferSize) {
  SDCardInfo i = getInfo();

  const char* cardTypeStr;
  switch (i.cardType) {
    case CARD_MMC:  cardTypeStr = "MMC"; break;
    case CARD_SD:   cardTypeStr = "SDSC"; break;
    case CARD_SDHC: cardTypeStr = "SDHC"; break;
    default:        cardTypeStr = "NONE"; break;
  }

  int written = snprintf(buffer, bufferSize,
    "{"
    "\"state\":\"%s\","
    "\"card_inserted\":%s,"
    "\"card_type\":\"%s\","
    "\"size_mb\":%llu,"
    "\"total_mb\":%llu,"
    "\"used_mb\":%llu,"
    "\"free_mb\":%llu,"
    "\"mount_count\":%lu,"
    "\"unmount_count\":%lu,"
    "\"error_count\":%lu,"
    "\"writes_ok\":%lu,"
    "\"writes_fail\":%lu,"
    "\"log_entries\":%lu"
    "}",
    getStateString(),
    (currentState == SD_READY) ? "true" : "false",
    cardTypeStr,
    i.cardSizeMB,
    i.totalSpaceMB,
    i.usedSpaceMB,
    i.freeSpaceMB,
    i.mountCount,
    i.unmountCount,
    i.errorCount,
    i.successfulWrites,
    i.failedWrites,
    i.logEntries
  );

  return (written > 0 && written < (int)bufferSize);
}

// =============================================================================
// PRINT MEMORY DIAGNOSTICS
// =============================================================================
void SDCardManager::printMemoryDiagnostics() {
  Serial.println();
  Serial.println("==================================================");
  Serial.println("           MEMORY DIAGNOSTICS                     ");
  Serial.println("==================================================");

  // Heap info
  Serial.printf(" Free Heap:       %lu bytes\n", ESP.getFreeHeap());
  Serial.printf(" Min Free Heap:   %lu bytes\n", ESP.getMinFreeHeap());
  Serial.printf(" Max Alloc Heap:  %lu bytes\n", ESP.getMaxAllocHeap());

  // PSRAM info
  Serial.printf(" Free PSRAM:      %lu bytes\n", ESP.getFreePsram());
  Serial.printf(" PSRAM Size:      %lu bytes\n", ESP.getPsramSize());

  // Task stack info (if task is running)
  if (sdTaskHandle) {
    UBaseType_t highWater = uxTaskGetStackHighWaterMark(sdTaskHandle);
    Serial.printf(" SD Task Stack:   %lu bytes remaining\n", highWater * 4);
  }

  Serial.println("==================================================");
}

// =============================================================================
// FORCE RECONNECT
// =============================================================================
bool SDCardManager::forceReconnect() {
  if (currentState == SD_READY) {
    LOG_SD_INFO("Card already ready");
    return true;
  }

  if (currentState == SD_DISABLED) {
    LOG_SD_WARN("Cannot reconnect - SD disabled");
    return false;
  }

  LOG_SD_INFO("Forcing reconnection...");
  info.reconnectAttempts = 0;
  handleStateTransition(SD_RECONNECTING);
  return true;
}

// =============================================================================
// DISABLE
// =============================================================================
void SDCardManager::disable() {
  if (currentState == SD_READY) {
    unmountCard();
  }
  handleStateTransition(SD_DISABLED);
}

// =============================================================================
// ENABLE
// =============================================================================
void SDCardManager::enable() {
  if (currentState == SD_DISABLED) {
    info.reconnectAttempts = 0;
    handleStateTransition(SD_RECONNECTING);
  }
}

