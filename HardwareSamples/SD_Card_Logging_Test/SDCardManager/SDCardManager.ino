/**
 * SDCardManager.ino - SD Card Test Entry Point for SRT-MGATE-1210
 * ================================================================
 *
 * Consolidated SD Card test with:
 * - Hot-plug detection and auto-reconnect
 * - FreeRTOS background task (non-blocking)
 * - Thread-safe operations with mutex
 * - Switchable SPI/SDMMC modes
 * - Serial command interface
 *
 * MODE SELECTION:
 * ---------------
 * Change SD_MODE in SDCardManager.h:
 *   #define SD_MODE SD_MODE_SPI    // HSPI (separate from Ethernet) - DEFAULT
 *   #define SD_MODE SD_MODE_SDMMC  // Native SDMMC (faster, no SPI)
 *
 * HARDWARE CONFIGURATION:
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
 * SERIAL COMMANDS:
 * ----------------
 *   status  - Print detailed SD card status
 *   json    - Print status as JSON
 *   write   - Write test log entry
 *   read    - Read current log file
 *   list    - List directory contents
 *   perf    - Run performance test
 *   mem     - Print memory diagnostics
 *   enable  - Enable SD card operations
 *   disable - Disable SD card operations
 *   help    - Show available commands
 *
 * Author: SURIOTA R&D Team
 * Date: January 2025
 * Board: ESP32-S3-WROOM-1-N16R8 (16MB Flash, 8MB PSRAM)
 */

#include "SDCardManager.h"

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================
SDCardManager* sdCardManager = nullptr;

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(2000);  // Wait for serial to stabilize

  printBanner();

  // Initialize SD Card Manager (singleton)
  sdCardManager = SDCardManager::getInstance();

  if (sdCardManager && sdCardManager->init()) {
    // Start background monitoring task
    if (sdCardManager->start()) {
      LOG_SD_INFO("SD Card Manager started successfully");

      // Print initial status if card is ready
      if (sdCardManager->isReady()) {
        sdCardManager->printStatus();
        sdCardManager->listDir("/", 1);
      }
    } else {
      LOG_SD_ERROR("Failed to start SD Card Manager task");
    }
  } else {
    LOG_SD_ERROR("Failed to initialize SD Card Manager");
  }

  // Print available commands
  printHelp();

  // Print initial memory status
  sdCardManager->printMemoryDiagnostics();
}

// =============================================================================
// LOOP
// =============================================================================
void loop() {
  // Process serial commands
  processSerialCommands();

  // Main loop delay (actual SD work is done in FreeRTOS task)
  delay(100);
}

// =============================================================================
// PRINT BANNER
// =============================================================================
void printBanner() {
  Serial.println();
  Serial.println("========================================================");
  Serial.println("     SD Card Manager Test - SRT-MGATE-1210              ");
  Serial.println("========================================================");
  Serial.println(" Platform:  ESP32-S3-WROOM-1-N16R8");
  Serial.println(" Flash:     16MB (QIO 80MHz)");
  Serial.println(" PSRAM:     8MB OPI");

#if SD_MODE == SD_MODE_SPI
  Serial.println(" SD Mode:   SPI (HSPI/SPI3 - Separate from Ethernet)");
  Serial.printf(" SD Pins:   CS=%d, MOSI=%d, MISO=%d, SCK=%d\n",
                SD_CS_PIN, SD_MOSI_PIN, SD_MISO_PIN, SD_SCK_PIN);
#elif SD_MODE == SD_MODE_SDMMC
  Serial.println(" SD Mode:   SDMMC (Native peripheral, no SPI)");
  Serial.printf(" SD Pins:   CLK=%d, CMD=%d, D0=%d\n",
                SD_SCK_PIN, SD_CS_PIN, SD_MISO_PIN);
#endif

  Serial.println("========================================================");
  Serial.println();
}

// =============================================================================
// PRINT HELP
// =============================================================================
void printHelp() {
  Serial.println();
  Serial.println("Available Commands:");
  Serial.println("-------------------");
  Serial.println("  status  - Print detailed SD card status");
  Serial.println("  json    - Print status as JSON");
  Serial.println("  write   - Write test log entry");
  Serial.println("  read    - Read current log file");
  Serial.println("  list    - List root directory");
  Serial.println("  perf    - Run performance test (100KB)");
  Serial.println("  mem     - Print memory diagnostics");
  Serial.println("  enable  - Enable SD card operations");
  Serial.println("  disable - Disable SD card operations");
  Serial.println("  reconnect - Force reconnection attempt");
  Serial.println("  help    - Show this help");
  Serial.println();
}

// =============================================================================
// PROCESS SERIAL COMMANDS
// =============================================================================
void processSerialCommands() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();

    if (cmd.length() == 0) return;

    Serial.printf("\n> Command: %s\n", cmd.c_str());

    if (cmd == "status") {
      sdCardManager->printStatus();

    } else if (cmd == "json") {
      char jsonBuffer[512];
      if (sdCardManager->getStatusJson(jsonBuffer, sizeof(jsonBuffer))) {
        Serial.println(jsonBuffer);
      } else {
        Serial.println("{\"error\":\"Failed to generate JSON\"}");
      }

    } else if (cmd == "write") {
      char testMsg[128];
      snprintf(testMsg, sizeof(testMsg),
               "[%lu] Manual test entry - Heap: %lu bytes",
               millis(), ESP.getFreeHeap());

      if (sdCardManager->writeLog(testMsg)) {
        Serial.printf("Written: %s\n", testMsg);
      } else {
        Serial.println("Write failed (card not ready?)");
      }

    } else if (cmd == "read") {
      // Read current log file
      char readBuffer[2048];
      int bytesRead = sdCardManager->readFile("/logs/log_000001.txt",
                                               readBuffer, sizeof(readBuffer));
      if (bytesRead > 0) {
        Serial.println("--- Log Contents ---");
        Serial.println(readBuffer);
        Serial.println("--- End of Log ---");
      } else {
        Serial.println("Failed to read log file");
      }

    } else if (cmd == "list") {
      sdCardManager->listDir("/", 2);

    } else if (cmd == "perf") {
      runPerformanceTest();

    } else if (cmd == "mem") {
      sdCardManager->printMemoryDiagnostics();

    } else if (cmd == "enable") {
      sdCardManager->enable();
      Serial.println("SD card operations enabled");

    } else if (cmd == "disable") {
      sdCardManager->disable();
      Serial.println("SD card operations disabled");

    } else if (cmd == "reconnect") {
      if (sdCardManager->forceReconnect()) {
        Serial.println("Reconnection initiated");
      } else {
        Serial.println("Cannot reconnect");
      }

    } else if (cmd == "help") {
      printHelp();

    } else {
      Serial.printf("Unknown command: %s\n", cmd.c_str());
      Serial.println("Type 'help' for available commands");
    }
  }
}

// =============================================================================
// PERFORMANCE TEST
// =============================================================================
void runPerformanceTest() {
  Serial.println();
  Serial.println("=== Performance Test (100KB) ===");

  if (!sdCardManager->isReady()) {
    Serial.println("ERROR: SD card not ready");
    return;
  }

  const char* testFile = "/perf_test.bin";
  const size_t bufferSize = 512;
  const size_t totalBytes = 100 * 1024;  // 100KB
  uint8_t buffer[bufferSize];

  // Fill buffer with pattern
  for (size_t i = 0; i < bufferSize; i++) {
    buffer[i] = i & 0xFF;
  }

  // Write test
  Serial.printf("Writing %d bytes in %d-byte chunks...\n", totalBytes, bufferSize);

#if SD_MODE == SD_MODE_SPI
  File file = SD.open(testFile, FILE_WRITE);
#elif SD_MODE == SD_MODE_SDMMC
  File file = SD_MMC.open(testFile, FILE_WRITE);
#endif

  if (!file) {
    Serial.println("Failed to create test file");
    return;
  }

  unsigned long startWrite = millis();
  size_t bytesWritten = 0;

  while (bytesWritten < totalBytes) {
    size_t written = file.write(buffer, bufferSize);
    if (written != bufferSize) {
      Serial.println("Write error!");
      break;
    }
    bytesWritten += written;
  }
  file.close();

  unsigned long writeTime = millis() - startWrite;
  float writeSpeed = (float)bytesWritten / writeTime;

  Serial.printf("Write: %lu bytes in %lu ms (%.1f KB/s)\n",
                bytesWritten, writeTime, writeSpeed);

  // Read test
  Serial.println("Reading back...");

#if SD_MODE == SD_MODE_SPI
  file = SD.open(testFile, FILE_READ);
#elif SD_MODE == SD_MODE_SDMMC
  file = SD_MMC.open(testFile, FILE_READ);
#endif

  if (!file) {
    Serial.println("Failed to open test file for reading");
    return;
  }

  unsigned long startRead = millis();
  size_t bytesRead = 0;

  while (file.available()) {
    bytesRead += file.read(buffer, bufferSize);
  }
  file.close();

  unsigned long readTime = millis() - startRead;
  float readSpeed = (float)bytesRead / readTime;

  Serial.printf("Read:  %lu bytes in %lu ms (%.1f KB/s)\n",
                bytesRead, readTime, readSpeed);

  // Cleanup
#if SD_MODE == SD_MODE_SPI
  SD.remove(testFile);
#elif SD_MODE == SD_MODE_SDMMC
  SD_MMC.remove(testFile);
#endif

  Serial.println("=== Performance Test Complete ===");
}
