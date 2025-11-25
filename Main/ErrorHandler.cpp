#include "ErrorHandler.h"

// Singleton instance
ErrorHandler *ErrorHandler::instance = nullptr;

// Constructor
ErrorHandler::ErrorHandler()
{
  lastResetTime = millis();
  hourStartTime = millis();
  dayStartTime = millis();
  Serial.println("[ERROR_HANDLER] Initialized - Unified error code system ready");
}

// Singleton access
ErrorHandler *ErrorHandler::getInstance()
{
  // Thread-safe Meyers Singleton (C++11 guarantees thread-safe static init)
  static ErrorHandler instance;
  static ErrorHandler *ptr = &instance;
  return ptr;
}

// Core error reporting
void ErrorHandler::reportError(UnifiedErrorCode code, const String &description)
{
  ErrorContext context;
  context.code = code;
  context.description = description;
  context.domain = getErrorDomain(code);
  context.severity = getDefaultSeverity(code);
  reportError(context);
}

void ErrorHandler::reportError(const ErrorContext &context)
{
  if (!enabled)
    return;

  ErrorContext enrichedContext = enrichErrorContext(context);

  // Update statistics
  updateStatistics(enrichedContext);

  // Add to history
  if (errorHistory.size() >= maxHistorySize)
  {
    errorHistory.pop_back();
  }
  errorHistory.push_front(enrichedContext);

  // Log to Serial if enabled
  if (logToSerial && enrichedContext.severity >= minSeverityToLog)
  {
    printErrorWithRecovery(enrichedContext);
  }

  // Call registered callbacks
  for (const auto &callback : errorCallbacks)
  {
    callback(enrichedContext);
  }

  // Update timers
  updateTimers();

  // Maintain history
  maintainHistory();
}

void ErrorHandler::reportError(UnifiedErrorCode code, uint32_t detailValue1, uint32_t detailValue2)
{
  ErrorContext context;
  context.code = code;
  context.domain = getErrorDomain(code);
  context.severity = getDefaultSeverity(code);
  context.detailValue1 = detailValue1;
  context.detailValue2 = detailValue2;
  reportError(context);
}

void ErrorHandler::reportError(UnifiedErrorCode code, const String &deviceId, uint32_t detailValue1)
{
  ErrorContext context;
  context.code = code;
  context.domain = getErrorDomain(code);
  context.severity = getDefaultSeverity(code);
  context.deviceId = deviceId;
  context.detailValue1 = detailValue1;
  reportError(context);
}

// Configuration
void ErrorHandler::setEnabled(bool enable)
{
  enabled = enable;
  Serial.printf("[ERROR_HANDLER] %s\n", enable ? "ENABLED" : "DISABLED");
}

void ErrorHandler::setLogToSerial(bool enable)
{
  logToSerial = enable;
}

void ErrorHandler::setMinSeverityToLog(ErrorSeverity severity)
{
  minSeverityToLog = severity;
}

void ErrorHandler::setMaxHistorySize(uint8_t size)
{
  maxHistorySize = size;
  while (errorHistory.size() > maxHistorySize)
  {
    errorHistory.pop_back();
  }
}

// Callback management
void ErrorHandler::registerErrorCallback(ErrorCallback callback)
{
  errorCallbacks.push_back(callback);
  Serial.printf("[ERROR_HANDLER] Registered error callback (%d total)\n", errorCallbacks.size());
}

void ErrorHandler::clearErrorCallbacks()
{
  errorCallbacks.clear();
  Serial.println("[ERROR_HANDLER] Cleared all error callbacks");
}

// Error queries
uint32_t ErrorHandler::getErrorCount(ErrorSeverity severity) const
{
  uint32_t count = 0;
  for (const auto &error : errorHistory)
  {
    if (error.severity == severity)
      count++;
  }
  return count;
}

uint32_t ErrorHandler::getErrorCountByDomain(ErrorDomain domain) const
{
  uint32_t count = 0;
  for (const auto &error : errorHistory)
  {
    if (error.domain == domain)
      count++;
  }
  return count;
}

uint32_t ErrorHandler::getTotalErrorCount() const
{
  return errorHistory.size();
}

ErrorContext ErrorHandler::getLastError() const
{
  if (errorHistory.empty())
  {
    return ErrorContext();
  }
  return errorHistory.front();
}

ErrorContext ErrorHandler::getLastErrorByDomain(ErrorDomain domain) const
{
  for (const auto &error : errorHistory)
  {
    if (error.domain == domain)
    {
      return error;
    }
  }
  return ErrorContext();
}

ErrorContext ErrorHandler::getLastErrorByCode(UnifiedErrorCode code) const
{
  for (const auto &error : errorHistory)
  {
    if (error.code == code)
    {
      return error;
    }
  }
  return ErrorContext();
}

// History access
uint32_t ErrorHandler::getHistorySize() const
{
  return errorHistory.size();
}

ErrorContext ErrorHandler::getHistoryEntry(uint32_t index) const
{
  if (index >= errorHistory.size())
  {
    return ErrorContext();
  }
  return errorHistory[index];
}

std::deque<ErrorContext, STLPSRAMAllocator<ErrorContext>> ErrorHandler::getErrorHistory() const
{
  return errorHistory;
}

std::deque<ErrorContext, STLPSRAMAllocator<ErrorContext>> ErrorHandler::getErrorHistoryByDomain(ErrorDomain domain) const
{
  std::deque<ErrorContext, STLPSRAMAllocator<ErrorContext>> filtered;
  for (const auto &error : errorHistory)
  {
    if (error.domain == domain)
    {
      filtered.push_back(error);
    }
  }
  return filtered;
}

std::deque<ErrorContext, STLPSRAMAllocator<ErrorContext>> ErrorHandler::getErrorHistoryBySeverity(ErrorSeverity severity) const
{
  std::deque<ErrorContext, STLPSRAMAllocator<ErrorContext>> filtered;
  for (const auto &error : errorHistory)
  {
    if (error.severity == severity)
    {
      filtered.push_back(error);
    }
  }
  return filtered;
}

// Statistics
ErrorStatistics ErrorHandler::getStatistics() const
{
  return statistics;
}

void ErrorHandler::resetStatistics()
{
  statistics = ErrorStatistics();
  lastResetTime = millis();
  Serial.println("[ERROR_HANDLER] Statistics reset");
}

float ErrorHandler::getErrorRatePerHour() const
{
  if (errorCounterThisHour == 0)
    return 0.0f;
  return (errorCounterThisHour * 3600000.0f) / (millis() - hourStartTime);
}

float ErrorHandler::getErrorRatePerDay() const
{
  if (errorCounterThisDay == 0)
    return 0.0f;
  return (errorCounterThisDay * 86400000.0f) / (millis() - dayStartTime);
}

uint32_t ErrorHandler::getErrorsThisHour()
{
  updateTimers();
  return errorCounterThisHour;
}

uint32_t ErrorHandler::getErrorsThisDay()
{
  updateTimers();
  return errorCounterThisDay;
}

// Error patterns
bool ErrorHandler::isErrorRepeating(UnifiedErrorCode code, uint32_t withinMs) const
{
  uint32_t count = 0;
  unsigned long now = millis();

  for (const auto &error : errorHistory)
  {
    if (error.code == code && (now - error.timestamp) <= withinMs)
    {
      count++;
      if (count >= 2)
        return true;
    }
  }
  return false;
}

uint32_t ErrorHandler::getRepeatingErrorCount(UnifiedErrorCode code, uint32_t withinMs) const
{
  uint32_t count = 0;
  unsigned long now = millis();

  for (const auto &error : errorHistory)
  {
    if (error.code == code && (now - error.timestamp) <= withinMs)
    {
      count++;
    }
  }
  return count;
}

ErrorDomain ErrorHandler::getMostFrequentErrorDomain(uint32_t withinMs) const
{
  unsigned long now = millis();
  uint32_t domainCounts[7] = {0};

  for (const auto &error : errorHistory)
  {
    if ((now - error.timestamp) <= withinMs)
    {
      domainCounts[error.domain]++;
    }
  }

  uint32_t maxCount = 0;
  ErrorDomain maxDomain = DOMAIN_SYSTEM;

  for (int i = 0; i < 7; i++)
  {
    if (domainCounts[i] > maxCount)
    {
      maxCount = domainCounts[i];
      maxDomain = (ErrorDomain)i;
    }
  }

  return maxDomain;
}

// Recovery suggestions
const char *ErrorHandler::getRecoverySuggestion(UnifiedErrorCode code) const
{
  return ::getRecoverySuggestion(code);
}

void ErrorHandler::printErrorWithRecovery(const ErrorContext &context)
{
  unsigned long nowMs = millis();
  uint32_t seconds = nowMs / 1000;
  uint32_t minutes = seconds / 60;
  uint32_t hours = minutes / 60;

  Serial.printf("[%02ld:%02ld:%02ld] [%s] [%s] ",
                hours % 24, minutes % 60, seconds % 60,
                getErrorSeverityString(context.severity),
                getErrorDomainString(context.domain));

  Serial.printf("%s", getErrorCodeDescription(context.code));

  if (!context.deviceId.isEmpty())
  {
    Serial.printf(" (Device: %s)", context.deviceId.c_str());
  }

  if (context.detailValue1 > 0)
  {
    Serial.printf(" [Detail: %ld]", context.detailValue1);
  }

  Serial.printf("\n");

  if (!context.description.isEmpty())
  {
    Serial.printf("  Description: %s\n", context.description.c_str());
  }

  if (context.severity >= SEVERITY_ERROR)
  {
    Serial.printf("  Recovery: %s\n", getRecoverySuggestion(context.code));
  }
}

// Diagnostics and reporting
void ErrorHandler::printLastError()
{
  if (errorHistory.empty())
  {
    Serial.println("[ERROR_HANDLER] No errors recorded");
    return;
  }

  Serial.println("\n[ERROR HANDLER] LAST ERROR");
  printErrorWithRecovery(errorHistory.front());
}

void ErrorHandler::printErrorStatistics()
{
  Serial.println("\n[ERROR HANDLER] ERROR STATISTICS");
  Serial.printf("  Total Errors: %ld\n", statistics.totalErrorsReported);
  Serial.printf("  Critical: %ld | Error: %ld | Warning: %ld | Info: %ld\n",
                statistics.criticalErrorCount,
                statistics.errorErrorCount,
                statistics.warningCount,
                statistics.infoCount);
  Serial.printf("  Errors This Hour: %ld | This Day: %ld\n",
                statistics.errorsThisHour,
                statistics.errorsThisDay);
  Serial.printf("  Most Frequent Domain: %s\n",
                getErrorDomainString(statistics.mostFrequentDomain));
  Serial.printf("  Error Rate: %.2f/hour, %.2f/day\n\n",
                getErrorRatePerHour(),
                getErrorRatePerDay());
}

void ErrorHandler::printErrorHistory(uint32_t count)
{
  Serial.printf("\n[ERROR HANDLER] ERROR HISTORY (Last %ld)\n", count);

  if (errorHistory.empty())
  {
    Serial.println("No errors recorded");
    return;
  }

  uint32_t printed = 0;
  for (const auto &error : errorHistory)
  {
    if (printed >= count)
      break;
    printErrorWithRecovery(error);
    printed++;
  }
}

void ErrorHandler::printErrorsByDomain()
{
  Serial.println("\n[ERROR HANDLER] ERRORS BY DOMAIN");

  for (int i = 0; i < 7; i++)
  {
    ErrorDomain domain = (ErrorDomain)i;
    uint32_t count = getErrorCountByDomain(domain);
    Serial.printf("  %s: %ld\n", getErrorDomainString(domain), count);
  }
  Serial.println();
}

void ErrorHandler::printErrorsBySeverity()
{
  Serial.println("\n[ERROR HANDLER] ERRORS BY SEVERITY");

  Serial.printf("  CRITICAL: %ld\n", getErrorCount(SEVERITY_CRITICAL));
  Serial.printf("  ERROR:    %ld\n", getErrorCount(SEVERITY_ERROR));
  Serial.printf("  WARNING:  %ld\n", getErrorCount(SEVERITY_WARNING));
  Serial.printf("  INFO:     %ld\n\n", getErrorCount(SEVERITY_INFO));
}

void ErrorHandler::printDetailedErrorReport()
{
  Serial.println("\n[ERROR HANDLER] DETAILED REPORT\n");

  printErrorStatistics();
  printErrorsByDomain();
  printErrorsBySeverity();
  printErrorHistory(10);
}

void ErrorHandler::printErrorTrends()
{
  Serial.println("\n[ERROR HANDLER] ERROR TRENDS");
  Serial.printf("  Errors this hour: %ld (rate: %.2f/hour)\n",
                errorCounterThisHour,
                getErrorRatePerHour());
  Serial.printf("  Errors this day: %ld (rate: %.2f/day)\n",
                errorCounterThisDay,
                getErrorRatePerDay());

  // Most frequent domain
  ErrorDomain frequent = getMostFrequentErrorDomain(3600000);
  Serial.printf("  Most frequent domain (1h): %s\n", getErrorDomainString(frequent));

  // Check for repeating errors
  Serial.println("\n  Repeating errors (last hour):");
  for (const auto &error : errorHistory)
  {
    if (isErrorRepeating(error.code, 3600000))
    {
      uint32_t count = getRepeatingErrorCount(error.code, 3600000);
      Serial.printf("    - %s: %ld times\n", getErrorCodeDescription(error.code), count);
    }
  }
  Serial.println();
}

// Utility methods
String ErrorHandler::formatErrorMessage(const ErrorContext &context)
{
  String msg = "[";
  msg += getErrorSeverityString(context.severity);
  msg += "] ";
  msg += getErrorCodeDescription(context.code);

  if (!context.deviceId.isEmpty())
  {
    msg += " (";
    msg += context.deviceId;
    msg += ")";
  }

  return msg;
}

String ErrorHandler::formatErrorCode(UnifiedErrorCode code)
{
  String formatted = "0x";
  if (code < 100)
    formatted += "0";
  formatted += String(code);
  return formatted;
}

bool ErrorHandler::isRecoverableError(UnifiedErrorCode code) const
{
  // Most errors are recoverable except critical hardware failures
  switch (code)
  {
  case ERR_SYS_WATCHDOG_TRIGGERED:
  case ERR_SYS_THERMAL_CRITICAL:
  case ERR_SYS_BROWNOUT_DETECTED:
  case ERR_MEM_STACK_OVERFLOW:
  case ERR_MEM_HEAP_CORRUPTION:
    return false;
  default:
    return true;
  }
}

uint32_t ErrorHandler::getSuggestedRetryDelay(UnifiedErrorCode code) const
{
  switch (code)
  {
  case ERR_NET_WIFI_CONNECTION_TIMEOUT:
  case ERR_NET_WIFI_DISCONNECTED:
    return 5000; // 5 seconds

  case ERR_MQTT_BROKER_UNREACHABLE:
  case ERR_MQTT_CONNECTION_FAILED:
    return 10000; // 10 seconds

  case ERR_BLE_CONNECTION_TIMEOUT:
  case ERR_BLE_DISCONNECTED:
    return 3000; // 3 seconds

  case ERR_MODBUS_DEVICE_TIMEOUT:
    return 5000; // 5 seconds

  case ERR_CFG_FILESYSTEM_ERROR:
    return 2000; // 2 seconds

  default:
    return 0;
  }
}

void ErrorHandler::clearErrorHistory()
{
  errorHistory.clear();
  Serial.println("[ERROR_HANDLER] Error history cleared");
}

void ErrorHandler::enableDiagnosticMode(bool enable)
{
  if (enable)
  {
    setLogToSerial(true);
    setMinSeverityToLog(SEVERITY_INFO);
    Serial.println("[ERROR_HANDLER] Diagnostic mode ENABLED - logging all errors");
  }
  else
  {
    setMinSeverityToLog(SEVERITY_WARNING);
    Serial.println("[ERROR_HANDLER] Diagnostic mode DISABLED");
  }
}

// Private helper methods
void ErrorHandler::updateStatistics(const ErrorContext &context)
{
  statistics.totalErrorsReported++;
  statistics.lastErrorTime = millis();
  statistics.lastErrorCode = context.code;
  statistics.mostFrequentDomain = context.domain;

  switch (context.severity)
  {
  case SEVERITY_CRITICAL:
    statistics.criticalErrorCount++;
    break;
  case SEVERITY_ERROR:
    statistics.errorErrorCount++;
    break;
  case SEVERITY_WARNING:
    statistics.warningCount++;
    break;
  case SEVERITY_INFO:
    statistics.infoCount++;
    break;
  }

  updateTimers();
}

void ErrorHandler::maintainHistory()
{
  // Keep history size within limits
  while (errorHistory.size() > maxHistorySize)
  {
    errorHistory.pop_back();
  }
}

void ErrorHandler::updateTimers()
{
  unsigned long now = millis();

  // Update hour counter
  if ((now - hourStartTime) >= 3600000)
  { // 1 hour = 3600000ms
    errorCounterThisHour = 0;
    hourStartTime = now;
  }
  else
  {
    errorCounterThisHour++;
  }

  // Update day counter
  if ((now - dayStartTime) >= 86400000)
  { // 1 day = 86400000ms
    errorCounterThisDay = 0;
    dayStartTime = now;
  }
  else
  {
    errorCounterThisDay++;
  }

  statistics.errorsThisHour = errorCounterThisHour;
  statistics.errorsThisDay = errorCounterThisDay;
}

ErrorContext ErrorHandler::enrichErrorContext(const ErrorContext &context)
{
  ErrorContext enriched = context;

  if (enriched.timestamp == 0)
  {
    enriched.timestamp = millis();
  }

  if (enriched.domain == DOMAIN_SYSTEM && context.domain != DOMAIN_SYSTEM)
  {
    enriched.domain = getErrorDomain(context.code);
  }

  if (enriched.severity == SEVERITY_INFO && context.severity == SEVERITY_INFO)
  {
    enriched.severity = getDefaultSeverity(context.code);
  }

  enriched.occurrenceCount++;
  enriched.lastOccurrenceTime = millis();
  enriched.isRecoverable = isRecoverableError(context.code);
  enriched.suggestedRetryDelayMs = getSuggestedRetryDelay(context.code);

  return enriched;
}

// Destructor
ErrorHandler::~ErrorHandler()
{
  Serial.println("[ERROR_HANDLER] Destroyed");
}
