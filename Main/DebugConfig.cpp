#include "DebugConfig.h"
#include "RTCManager.h"

// ============================================
// GLOBAL LOG LEVEL VARIABLE
// ============================================
// Default log level: INFO (production recommended)
// Shows: ERROR, WARN, INFO
// Hides: DEBUG, VERBOSE
LogLevel currentLogLevel = LOG_INFO;

// ============================================
// TIMESTAMP CONTROL (Phase 3)
// ============================================
bool logTimestampsEnabled = true; // Enabled by default

// ============================================
// LOG LEVEL CONTROL FUNCTIONS
// ============================================

void setLogLevel(LogLevel level) {
  if (level > LOG_VERBOSE) {
    Serial.println("[LOG] ERROR: Invalid log level, using LOG_INFO");
    currentLogLevel = LOG_INFO;
    return;
  }

  currentLogLevel = level;
  Serial.printf("[LOG] Log level set to: %s (%d)\n", getLogLevelName(level), level);

  // Print what will be visible
  Serial.print("[LOG] Visible levels: ");
  if (level >= LOG_ERROR) Serial.print("ERROR ");
  if (level >= LOG_WARN) Serial.print("WARN ");
  if (level >= LOG_INFO) Serial.print("INFO ");
  if (level >= LOG_DEBUG) Serial.print("DEBUG ");
  if (level >= LOG_VERBOSE) Serial.print("VERBOSE");
  Serial.println();
}

LogLevel getLogLevel() {
  return currentLogLevel;
}

const char* getLogLevelName(LogLevel level) {
  switch (level) {
    case LOG_NONE:    return "NONE";
    case LOG_ERROR:   return "ERROR";
    case LOG_WARN:    return "WARN";
    case LOG_INFO:    return "INFO";
    case LOG_DEBUG:   return "DEBUG";
    case LOG_VERBOSE: return "VERBOSE";
    default:          return "UNKNOWN";
  }
}

void printLogLevelStatus() {
  Serial.println("\n[SYSTEM] LOG LEVEL STATUS");
  Serial.printf("  Current Level: %s (%d)\n", getLogLevelName(currentLogLevel), currentLogLevel);
  Serial.printf("  Production Mode: %s\n", IS_PRODUCTION_MODE() ? "YES" : "NO");
  Serial.printf("  Compile Level: ");

  #if PRODUCTION_MODE == 1
    Serial.println("INFO (production)");
  #else
    Serial.println("VERBOSE (development)");
  #endif

  Serial.println("\n  Available Levels:");
  Serial.println("    0 = NONE    (silent)");
  Serial.println("    1 = ERROR   (critical errors only)");
  Serial.println("    2 = WARN    (warnings + errors)");
  Serial.println("    3 = INFO    (info + warn + errors) [DEFAULT]");
  Serial.println("    4 = DEBUG   (debug + info + warn + errors)");
  Serial.println("    5 = VERBOSE (all logs)");

  Serial.println("\n  Change level: setLogLevel(LOG_INFO)");
  Serial.printf("  Timestamps: %s\n\n", logTimestampsEnabled ? "ENABLED" : "DISABLED");
}

// ============================================
// RTC TIMESTAMP FUNCTIONS (Phase 3)
// ============================================

const char* getLogTimestamp() {
  static char timestamp[22]; // "[YYYY-MM-DD HH:MM:SS] " = 21 chars + null

  RTCManager *rtc = RTCManager::getInstance();

  if (rtc) {
    // Try to get RTC time
    DateTime now = rtc->getCurrentTime();

    // Check if time is valid (year > 2020 indicates RTC is synced)
    if (now.year() >= 2020) {
      // RTC available and synced - use real time
      snprintf(timestamp, sizeof(timestamp), "[%04d-%02d-%02d %02d:%02d:%02d]",
               now.year(), now.month(), now.day(),
               now.hour(), now.minute(), now.second());
      return timestamp;
    }
  }

  // RTC not available or not synced - use uptime (seconds since boot)
  unsigned long uptimeSec = millis() / 1000;
  snprintf(timestamp, sizeof(timestamp), "[%010lu]", uptimeSec);

  return timestamp;
}

void setLogTimestamps(bool enabled) {
  logTimestampsEnabled = enabled;
  Serial.printf("[LOG] Timestamps %s\n", enabled ? "ENABLED" : "DISABLED");
}
