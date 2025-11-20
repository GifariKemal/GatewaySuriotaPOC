/*
 * Atomic File Operations for LittleFS
 *
 * Provides safe, atomic write operations with Write-Ahead Logging (WAL)
 * to prevent file corruption in case of power loss during writes.
 *
 * Features:
 * - Atomic file writes using temp file + rename pattern
 * - Write-Ahead Log (WAL) for recovery
 * - Checksum verification
 * - Automatic recovery on startup
 * - Zero breaking changes to existing code
 */

#ifndef ATOMIC_FILE_OPS_H
#define ATOMIC_FILE_OPS_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/**
 * @brief Write-Ahead Log Entry for tracking incomplete operations
 */
struct WALEntry
{
  String operation;        // "write", "delete", etc
  String targetFile;       // File being modified
  String tempFile;         // Temporary file (if applicable)
  unsigned long timestamp; // When operation started
  String checksum;         // For verification
  bool completed = false;  // Mark as completed after successful write
  uint8_t retryCount = 0;  // Retry tracking

  // Calculate hash for this entry
  String getHash() const
  {
    String data = operation + "|" + targetFile + "|" + String(timestamp);
    uint32_t hash = 0;
    for (char c : data)
    {
      hash = ((hash << 1) ^ c) & 0xFFFFFFFF;
    }
    return String(hash, HEX);
  }
};

/**
 * @brief Atomic File Operations Manager
 *
 * Provides safe file operations with automatic recovery.
 * Uses Write-Ahead Logging to ensure data integrity even with power loss.
 */
class AtomicFileOps
{
private:
  static const char *WAL_FILE;
  std::vector<WALEntry> walLog;
  SemaphoreHandle_t walMutex;
  bool recoveryAttempted = false;

  // Private helper methods
  String calculateChecksum(const JsonDocument &doc);
  String calculateFileChecksum(const String &filename);
  bool verifyFileIntegrity(const String &filename);
  void appendToWAL(const WALEntry &entry);
  void markWALEntryCompleted(const String &filename);
  void clearWAL();

public:
  AtomicFileOps();
  ~AtomicFileOps();

  /**
   * @brief Initialize atomic file operations
   * @return true if initialization successful
   */
  bool begin();

  /**
   * @brief Perform atomic write of JSON document to file
   *
   * Uses two-phase commit:
   * 1. Write to temporary file (.tmp)
   * 2. Atomic rename to target filename
   *
   * If power is lost during step 1, recovery will clean up .tmp
   * If power is lost during step 2, recovery will complete the rename
   *
   * @param filename Target filename (must be absolute path like "/devices.json")
   * @param doc JsonDocument to write
   * @return true if write successful
   */
  bool writeAtomic(const String &filename, const JsonDocument &doc);

  /**
   * @brief Perform atomic delete of file
   * @param filename File to delete
   * @return true if delete successful
   */
  bool deleteAtomic(const String &filename);

  /**
   * @brief Recover from incomplete operations
   *
   * Called on startup to clean up after power loss.
   * - Removes orphaned temp files
   * - Completes interrupted renames if possible
   * - Verifies file integrity
   *
   * @return Number of files recovered
   */
  uint8_t recover();

  /**
   * @brief Get WAL status for debugging
   * @return Number of incomplete operations in log
   */
  uint8_t getWALSize() const { return walLog.size(); }

  /**
   * @brief Force WAL cleanup (use with caution)
   */
  void forceWALCleanup();

  /**
   * @brief Print WAL status to Serial (debugging)
   */
  void printWALStatus();
};

#endif // ATOMIC_FILE_OPS_H
