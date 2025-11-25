#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include <Arduino.h>
#include <deque>
#include <functional>
#include "UnifiedErrorCodes.h"
#include "PSRAMAllocator.h" // FIXED BUG B: For PSRAM allocator

/*
 * @brief Unified Error Handler - Centralized error management
 *
 * Manages error reporting, logging, tracking, and recovery across all subsystems.
 *
 * Features:
 * - Centralized error reporting from all managers
 * - Error history tracking (circular buffer)
 * - Error callback system for error events
 * - Error aggregation and pattern detection
 * - Recovery recommendations
 * - Error statistics tracking
 * - Severity-based filtering
 */

class ErrorHandler
{
private:
  static ErrorHandler *instance;

  // Configuration
  bool enabled = true;
  bool logToSerial = true;
  uint8_t maxHistorySize = 100;                      // Maximum errors to keep in history
  ErrorSeverity minSeverityToLog = SEVERITY_WARNING; // Minimum severity to log

  // Error history (circular buffer)
  // FIXED BUG B: Use PSRAM allocator to save precious DRAM (5-10KB moved to PSRAM)
  std::deque<ErrorContext, STLPSRAMAllocator<ErrorContext>> errorHistory;

  // Statistics
  ErrorStatistics statistics;

  // Callbacks
  typedef std::function<void(const ErrorContext &)> ErrorCallback;
  // FIXED BUG B: Use PSRAM allocator to save DRAM
  std::deque<ErrorCallback, STLPSRAMAllocator<ErrorCallback>> errorCallbacks;

  // Tracking
  unsigned long lastResetTime = 0;
  uint32_t errorCounterThisHour = 0;
  uint32_t errorCounterThisDay = 0;
  unsigned long hourStartTime = 0;
  unsigned long dayStartTime = 0;

  // Private constructor for singleton
  ErrorHandler();

  // Internal methods
  void updateStatistics(const ErrorContext &context);
  void maintainHistory();
  void updateTimers();
  ErrorContext enrichErrorContext(const ErrorContext &context);

public:
  // Singleton access
  static ErrorHandler *getInstance();

  // Core error reporting
  void reportError(UnifiedErrorCode code, const String &description = "");
  void reportError(const ErrorContext &context);
  void reportError(UnifiedErrorCode code, uint32_t detailValue1, uint32_t detailValue2 = 0);
  void reportError(UnifiedErrorCode code, const String &deviceId, uint32_t detailValue1);

  // Configuration
  void setEnabled(bool enable);
  void setLogToSerial(bool enable);
  void setMinSeverityToLog(ErrorSeverity severity);
  void setMaxHistorySize(uint8_t size);

  // Callback management
  void registerErrorCallback(ErrorCallback callback);
  void clearErrorCallbacks();

  // Error queries
  uint32_t getErrorCount(ErrorSeverity severity) const;
  uint32_t getErrorCountByDomain(ErrorDomain domain) const;
  uint32_t getTotalErrorCount() const;
  ErrorContext getLastError() const;
  ErrorContext getLastErrorByDomain(ErrorDomain domain) const;
  ErrorContext getLastErrorByCode(UnifiedErrorCode code) const;

  // History access
  uint32_t getHistorySize() const;
  ErrorContext getHistoryEntry(uint32_t index) const; // 0 = newest
  // FIXED BUG B: Return type must match PSRAM allocator
  std::deque<ErrorContext, STLPSRAMAllocator<ErrorContext>> getErrorHistory() const;
  std::deque<ErrorContext, STLPSRAMAllocator<ErrorContext>> getErrorHistoryByDomain(ErrorDomain domain) const;
  std::deque<ErrorContext, STLPSRAMAllocator<ErrorContext>> getErrorHistoryBySeverity(ErrorSeverity severity) const;

  // Statistics
  ErrorStatistics getStatistics() const;
  void resetStatistics();
  float getErrorRatePerHour() const;
  float getErrorRatePerDay() const;
  uint32_t getErrorsThisHour();
  uint32_t getErrorsThisDay();

  // Error patterns
  bool isErrorRepeating(UnifiedErrorCode code, uint32_t withinMs = 60000) const;
  uint32_t getRepeatingErrorCount(UnifiedErrorCode code, uint32_t withinMs = 60000) const;
  ErrorDomain getMostFrequentErrorDomain(uint32_t withinMs = 3600000) const;

  // Recovery suggestions
  const char *getRecoverySuggestion(UnifiedErrorCode code) const;
  void printErrorWithRecovery(const ErrorContext &context);

  // Diagnostics and reporting
  void printLastError();
  void printErrorStatistics();
  void printErrorHistory(uint32_t count = 10); // Print last N errors
  void printErrorsByDomain();
  void printErrorsBySeverity();
  void printDetailedErrorReport();
  void printErrorTrends();

  // Utility methods
  String formatErrorMessage(const ErrorContext &context);
  String formatErrorCode(UnifiedErrorCode code);
  bool isRecoverableError(UnifiedErrorCode code) const;
  uint32_t getSuggestedRetryDelay(UnifiedErrorCode code) const;

  // State management
  void clearErrorHistory();
  void enableDiagnosticMode(bool enable);

  // Destructor
  ~ErrorHandler();
};

#endif // ERROR_HANDLER_H
