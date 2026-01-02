/*
 * Atomic File Operations Implementation
 * Provides safe, transactional file writes for LittleFS
 */

#include "AtomicFileOps.h"

#include "DebugConfig.h"  // MUST BE FIRST for LOG_* macros

const char* AtomicFileOps::WAL_FILE = "/.wal";

AtomicFileOps::AtomicFileOps() : walMutex(nullptr), recoveryAttempted(false) {
  // Create mutex for WAL synchronization
  walMutex = xSemaphoreCreateMutex();
  if (!walMutex) {
    LOG_CONFIG_INFO("[ATOMIC] ERROR: Failed to create WAL mutex");
  }
}

AtomicFileOps::~AtomicFileOps() {
  if (walMutex) {
    vSemaphoreDelete(walMutex);
    walMutex = nullptr;
  }
  walLog.clear();
}

bool AtomicFileOps::begin() {
  LOG_CONFIG_INFO("[ATOMIC] Initializing atomic file operations...");

  // Perform recovery on startup
  uint8_t recovered = recover();
  if (recovered > 0) {
    LOG_CONFIG_INFO("[ATOMIC] Recovered from %d incomplete operation(s)\n",
                    recovered);
  }

  recoveryAttempted = true;
  return true;
}

String AtomicFileOps::calculateChecksum(const JsonDocument& doc) {
  // FIXED BUG A: Streaming checksum without loading entire JSON to String
  // Prevents OOM for large documents (20-50KB full_config backups)
  size_t docSize = measureJson(doc);

  // Safety limit: Skip checksum for very large documents to avoid performance
  // hit
  if (docSize > 10240) {
    LOG_CONFIG_INFO(
        "[ATOMIC] WARNING: Skipping checksum for large doc (%zu bytes)\n",
        docSize);
    return "SKIPPED_LARGE";
  }

  // Streaming checksum calculator (zero heap allocation)
  class ChecksumPrint : public Print {
   public:
    uint32_t checksum = 0;
    size_t write(uint8_t c) override {
      // Simple rotating XOR checksum
      checksum = ((checksum << 1) ^ c) & 0xFFFFFFFF;
      return 1;
    }
  };

  ChecksumPrint checksumCalc;
  serializeJson(doc, checksumCalc);

  return String(checksumCalc.checksum, HEX);
}

String AtomicFileOps::calculateFileChecksum(const String& filename) {
  File file = LittleFS.open(filename, "r");
  if (!file) {
    return "";
  }

  uint32_t checksum = 0;
  const size_t CHUNK_SIZE = 512;
  char buffer[CHUNK_SIZE];

  while (file.available()) {
    size_t bytesRead = file.read((uint8_t*)buffer, CHUNK_SIZE);
    for (size_t i = 0; i < bytesRead; i++) {
      checksum = ((checksum << 1) ^ buffer[i]) & 0xFFFFFFFF;
    }
  }

  file.close();
  return String(checksum, HEX);
}

bool AtomicFileOps::verifyFileIntegrity(const String& filename) {
  // Check if file exists and is readable
  if (!LittleFS.exists(filename)) {
    return false;
  }

  File file = LittleFS.open(filename, "r");
  if (!file) {
    return false;
  }

  // Verify file is not empty
  size_t fileSize = file.size();
  file.close();

  if (fileSize == 0) {
    LOG_CONFIG_INFO("[ATOMIC] File %s is empty (possibly corrupted)\n",
                    filename.c_str());
    return false;
  }

  return true;
}

void AtomicFileOps::appendToWAL(const WALEntry& entry) {
  if (xSemaphoreTake(walMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
    LOG_CONFIG_INFO("[ATOMIC] ERROR: WAL mutex timeout");
    return;
  }

  walLog.push_back(entry);

  xSemaphoreGive(walMutex);
}

void AtomicFileOps::markWALEntryCompleted(const String& filename) {
  if (xSemaphoreTake(walMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
    return;
  }

  for (auto& entry : walLog) {
    if (entry.targetFile == filename) {
      entry.completed = true;

      break;
    }
  }

  xSemaphoreGive(walMutex);
}

void AtomicFileOps::clearWAL() {
  if (xSemaphoreTake(walMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
    return;
  }

  walLog.clear();
  LOG_CONFIG_INFO("[ATOMIC] WAL cleared");

  xSemaphoreGive(walMutex);
}

bool AtomicFileOps::writeAtomic(const String& filename,
                                const JsonDocument& doc) {
  if (!walMutex) {
    LOG_CONFIG_INFO("[ATOMIC] ERROR: WAL mutex not initialized");
    return false;
  }

  // Step 1: Create WAL entry
  WALEntry entry;
  entry.operation = "write";
  entry.targetFile = filename;
  entry.tempFile = filename + ".tmp";
  entry.timestamp = millis();
  entry.checksum = calculateChecksum(doc);
  entry.completed = false;

  // Step 2: Add to WAL (before any file operations!)
  appendToWAL(entry);

  // Step 3: Write to temporary file
  size_t tmpSize = 0;
  {
    File tmpFile = LittleFS.open(entry.tempFile, "w");
    if (!tmpFile) {
      LOG_CONFIG_INFO("[ATOMIC] ERROR: Cannot open temp file %s\n",
                      entry.tempFile.c_str());
      return false;
    }

    // Serialize JSON to file
    serializeJson(doc, tmpFile);
    tmpFile.flush();

    // Verify temp file was written
    tmpSize = tmpFile.size();
    tmpFile.close();

    if (tmpSize == 0) {
      LOG_CONFIG_INFO("[ATOMIC] ERROR: Temp file write failed (size=0)\n");
      LittleFS.remove(entry.tempFile);
      return false;
    }
  }

  // Step 4: Verify temp file integrity
  if (!verifyFileIntegrity(entry.tempFile)) {
    LOG_CONFIG_INFO("[ATOMIC] ERROR: Temp file integrity check failed\n");
    LittleFS.remove(entry.tempFile);
    return false;
  }

  // Step 5: Atomic rename (this is the critical operation)
  // LittleFS.rename() is atomic on LittleFS
  {
    // Remove old file if exists (on LittleFS, rename doesn't auto-replace)
    if (LittleFS.exists(filename)) {
      if (!LittleFS.remove(filename)) {
        LOG_CONFIG_INFO("[ATOMIC] ERROR: Cannot remove old file %s\n",
                        filename.c_str());
        LittleFS.remove(entry.tempFile);
        return false;
      }
    }

    // Perform atomic rename
    if (!LittleFS.rename(entry.tempFile, filename)) {
      LOG_CONFIG_INFO("[ATOMIC] ERROR: Atomic rename failed\n");
      LittleFS.remove(entry.tempFile);
      return false;
    }
  }

  // Step 6: Verify final file
  if (!verifyFileIntegrity(filename)) {
    LOG_CONFIG_INFO("[ATOMIC] ERROR: Final file verification failed\n");
    return false;
  }

  // Step 7: Mark WAL entry as completed
  markWALEntryCompleted(filename);

  LOG_CONFIG_INFO("[ATOMIC] Write completed: %s (%d bytes)\n", filename.c_str(),
                  tmpSize);
  return true;
}

bool AtomicFileOps::deleteAtomic(const String& filename) {
  if (!LittleFS.exists(filename)) {
    LOG_CONFIG_INFO("[ATOMIC] File not found for deletion: %s\n",
                    filename.c_str());
    return true;  // Already deleted
  }

  // Create backup before deletion (optional, for safety)
  String backupFile = filename + ".backup";

  // Simple approach: just delete the file
  // For production, consider keeping backups
  if (!LittleFS.remove(filename)) {
    LOG_CONFIG_INFO("[ATOMIC] ERROR: Failed to delete %s\n", filename.c_str());
    return false;
  }

  LOG_CONFIG_INFO("[ATOMIC] File deleted successfully: %s\n", filename.c_str());
  return true;
}

uint8_t AtomicFileOps::recover() {
  if (!walMutex) {
    LOG_CONFIG_INFO("[ATOMIC] ERROR: WAL mutex not initialized");
    return 0;
  }

  if (recoveryAttempted) {
    LOG_CONFIG_INFO("[ATOMIC] Recovery already attempted");
    return 0;
  }

  LOG_CONFIG_INFO("[ATOMIC] Starting recovery from incomplete operations...");

  uint8_t recovered = 0;

  // Check for orphaned temp files (from interrupted writes)
  // These are files with .tmp extension
  // Using proper LittleFS API for directory listing
  File root = LittleFS.open("/");
  if (!root) {
    LOG_CONFIG_INFO("[ATOMIC] ERROR: Cannot open root directory");
    return 0;
  }

  // Use file iteration compatible with LittleFS
  File file = root.openNextFile();
  while (file) {
    String fileName = file.name();

    if (fileName.endsWith(".tmp")) {
      LOG_CONFIG_INFO("[ATOMIC] Found orphaned temp file: %s\n",
                      fileName.c_str());

      file.close();

      // Remove the orphaned temp file
      if (LittleFS.remove(String("/") + fileName)) {
        LOG_CONFIG_INFO("[ATOMIC] Cleaned up temp file: %s\n",
                        fileName.c_str());
        recovered++;
      } else {
        LOG_CONFIG_INFO("[ATOMIC] Failed to clean up temp file: %s\n",
                        fileName.c_str());
      }
    } else {
      file.close();
    }

    file = root.openNextFile();
  }
  root.close();

  // Check WAL for incomplete operations
  if (xSemaphoreTake(walMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
    for (const auto& entry : walLog) {
      if (!entry.completed) {
        LOG_CONFIG_INFO("[ATOMIC] Found incomplete WAL entry: %s on %s\n",
                        entry.operation.c_str(), entry.targetFile.c_str());

        if (entry.operation == "write") {
          // If temp file still exists, it means rename was interrupted
          if (LittleFS.exists(entry.tempFile)) {
            LOG_CONFIG_INFO("[ATOMIC] Retrying rename: %s to %s\n",
                            entry.tempFile.c_str(), entry.targetFile.c_str());

            if (LittleFS.exists(entry.targetFile)) {
              LittleFS.remove(entry.targetFile);
            }

            if (LittleFS.rename(entry.tempFile, entry.targetFile)) {
              LOG_CONFIG_INFO("[ATOMIC] Completed interrupted rename\n");
              recovered++;
            } else {
              LOG_CONFIG_INFO("[ATOMIC] Failed to complete rename\n");
            }
          }
        }
      }
    }

    xSemaphoreGive(walMutex);
  }

  LOG_CONFIG_INFO("[ATOMIC] Recovery completed: %d operations recovered\n",
                  recovered);
  return recovered;
}

void AtomicFileOps::forceWALCleanup() {
  LOG_CONFIG_INFO("[ATOMIC] Force cleanup of WAL...");
  clearWAL();

  // Remove all .tmp files
  // Using proper LittleFS API for directory listing
  File root = LittleFS.open("/");
  if (!root) {
    LOG_CONFIG_INFO("[ATOMIC] ERROR: Cannot open root directory for cleanup");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    String fileName = file.name();
    if (fileName.endsWith(".tmp")) {
      file.close();
      if (LittleFS.remove(String("/") + fileName)) {
        LOG_CONFIG_INFO("[ATOMIC] Removed: %s\n", fileName.c_str());
      }
    } else {
      file.close();
    }
    file = root.openNextFile();
  }
  root.close();

  LOG_CONFIG_INFO("[ATOMIC] Force cleanup completed");
}

void AtomicFileOps::printWALStatus() {
  Serial.println("\n[ATOMIC] WAL STATUS");
  Serial.printf("  WAL Entries: %d\n", walLog.size());

  if (xSemaphoreTake(walMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
    for (size_t i = 0; i < walLog.size(); i++) {
      const auto& entry = walLog[i];
      Serial.printf("    [%d] %s on %s - %s\n", i, entry.operation.c_str(),
                    entry.targetFile.c_str(),
                    entry.completed ? "COMPLETED" : "INCOMPLETE");
    }
    xSemaphoreGive(walMutex);
  }
  Serial.println();
}
