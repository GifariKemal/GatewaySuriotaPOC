#ifndef DEBUG_CONFIG_H
#define DEBUG_CONFIG_H

#include <Arduino.h>

/*
 * ============================================
 * DEBUG CONFIGURATION - ENHANCED LOG LEVEL SYSTEM
 * ============================================
 *
 * Two-tier logging system:
 * 1. PRODUCTION_MODE (compile-time): Binary on/off for entire system
 * 2. LOG_LEVEL (runtime): Granular control within debug mode
 *
 * PRODUCTION_MODE = 1: All debug output disabled (production/customer deployment)
 * PRODUCTION_MODE = 0: Debug enabled with LOG_LEVEL control (development/testing)
 *
 * Usage:
 * - Set PRODUCTION_MODE in Main.ino BEFORE including this header
 * - Set runtime log level via setLogLevel(LOG_INFO) in setup()
 * - Change log level at runtime via BLE CRUD or serial command
 */

// ============================================
// LOG LEVEL ENUMERATION
// ============================================
enum LogLevel : uint8_t {
  LOG_NONE = 0,    // No logging (silent mode)
  LOG_ERROR = 1,   // Critical errors only (production minimal)
  LOG_WARN = 2,    // Warnings + errors (production recommended)
  LOG_INFO = 3,    // Info + warnings + errors (production default)
  LOG_DEBUG = 4,   // Debug + info + warnings + errors (development)
  LOG_VERBOSE = 5  // All logs including verbose (heavy development)
};

// Global log level (runtime configurable)
extern LogLevel currentLogLevel;

// ============================================
// RTC TIMESTAMP FORMATTER (Phase 3)
// ============================================
/**
 * Get formatted timestamp for logging
 * Returns: "[YYYY-MM-DD HH:MM:SS]" if RTC available, "[uptime_sec]" otherwise
 */
const char* getLogTimestamp();

// Enable/disable timestamps in logs (default: enabled)
extern bool logTimestampsEnabled;
void setLogTimestamps(bool enabled);

// ============================================
// COMPILE-TIME LOG LEVEL (build optimization)
// ============================================
#ifndef COMPILE_LOG_LEVEL
  #if PRODUCTION_MODE == 1
    #define COMPILE_LOG_LEVEL LOG_INFO  // Production: INFO only
  #else
    #define COMPILE_LOG_LEVEL LOG_VERBOSE  // Development: All logs
  #endif
#endif

// ============================================
// LOG LEVEL CONTROL FUNCTIONS
// ============================================
void setLogLevel(LogLevel level);
LogLevel getLogLevel();
const char* getLogLevelName(LogLevel level);
void printLogLevelStatus();

// ============================================
// ENHANCED LOGGING MACROS (with log levels)
// ============================================

#if PRODUCTION_MODE == 1
// ==========================================
// PRODUCTION MODE - Minimal logging
// ==========================================

// Only ERROR and WARN levels in production
#define LOG_CHECK(level) (currentLogLevel >= level && level <= LOG_WARN)

// Generic log macros (production - ERROR/WARN only) with timestamps
#define LOG_ERROR_F(prefix, fmt, ...)   \
  do { if (LOG_CHECK(LOG_ERROR) && logTimestampsEnabled) Serial.printf("%s[ERROR][%s] " fmt, getLogTimestamp(), prefix, ##__VA_ARGS__); \
       else if (LOG_CHECK(LOG_ERROR)) Serial.printf("[ERROR][%s] " fmt, prefix, ##__VA_ARGS__); } while(0)

#define LOG_WARN_F(prefix, fmt, ...)    \
  do { if (LOG_CHECK(LOG_WARN) && logTimestampsEnabled) Serial.printf("%s[WARN][%s] " fmt, getLogTimestamp(), prefix, ##__VA_ARGS__); \
       else if (LOG_CHECK(LOG_WARN)) Serial.printf("[WARN][%s] " fmt, prefix, ##__VA_ARGS__); } while(0)

// INFO, DEBUG, VERBOSE disabled in production (compile out)
#define LOG_INFO_F(prefix, fmt, ...)    do {} while(0)
#define LOG_DEBUG_F(prefix, fmt, ...)   do {} while(0)
#define LOG_VERBOSE_F(prefix, fmt, ...) do {} while(0)

#else
// ==========================================
// DEVELOPMENT MODE - Full logging
// ==========================================

// Runtime + compile-time filtering
#define LOG_CHECK(level) (currentLogLevel >= level && COMPILE_LOG_LEVEL >= level)

// Generic log macros (development - all levels) with timestamps
#define LOG_ERROR_F(prefix, fmt, ...)   \
  do { if (LOG_CHECK(LOG_ERROR) && logTimestampsEnabled) Serial.printf("%s[ERROR][%s] " fmt, getLogTimestamp(), prefix, ##__VA_ARGS__); \
       else if (LOG_CHECK(LOG_ERROR)) Serial.printf("[ERROR][%s] " fmt, prefix, ##__VA_ARGS__); } while(0)

#define LOG_WARN_F(prefix, fmt, ...)    \
  do { if (LOG_CHECK(LOG_WARN) && logTimestampsEnabled) Serial.printf("%s[WARN][%s] " fmt, getLogTimestamp(), prefix, ##__VA_ARGS__); \
       else if (LOG_CHECK(LOG_WARN)) Serial.printf("[WARN][%s] " fmt, prefix, ##__VA_ARGS__); } while(0)

#define LOG_INFO_F(prefix, fmt, ...)    \
  do { if (LOG_CHECK(LOG_INFO) && logTimestampsEnabled) Serial.printf("%s[INFO][%s] " fmt, getLogTimestamp(), prefix, ##__VA_ARGS__); \
       else if (LOG_CHECK(LOG_INFO)) Serial.printf("[INFO][%s] " fmt, prefix, ##__VA_ARGS__); } while(0)

#define LOG_DEBUG_F(prefix, fmt, ...)   \
  do { if (LOG_CHECK(LOG_DEBUG) && logTimestampsEnabled) Serial.printf("%s[DEBUG][%s] " fmt, getLogTimestamp(), prefix, ##__VA_ARGS__); \
       else if (LOG_CHECK(LOG_DEBUG)) Serial.printf("[DEBUG][%s] " fmt, prefix, ##__VA_ARGS__); } while(0)

#define LOG_VERBOSE_F(prefix, fmt, ...) \
  do { if (LOG_CHECK(LOG_VERBOSE) && logTimestampsEnabled) Serial.printf("%s[VERBOSE][%s] " fmt, getLogTimestamp(), prefix, ##__VA_ARGS__); \
       else if (LOG_CHECK(LOG_VERBOSE)) Serial.printf("[VERBOSE][%s] " fmt, prefix, ##__VA_ARGS__); } while(0)

#endif

// ============================================
// MODULE-SPECIFIC LOG MACROS
// ============================================

// --- MODBUS RTU ---
#define LOG_RTU_ERROR(fmt, ...)     LOG_ERROR_F("RTU", fmt, ##__VA_ARGS__)
#define LOG_RTU_WARN(fmt, ...)      LOG_WARN_F("RTU", fmt, ##__VA_ARGS__)
#define LOG_RTU_INFO(fmt, ...)      LOG_INFO_F("RTU", fmt, ##__VA_ARGS__)
#define LOG_RTU_DEBUG(fmt, ...)     LOG_DEBUG_F("RTU", fmt, ##__VA_ARGS__)
#define LOG_RTU_VERBOSE(fmt, ...)   LOG_VERBOSE_F("RTU", fmt, ##__VA_ARGS__)

// --- MODBUS TCP ---
#define LOG_TCP_ERROR(fmt, ...)     LOG_ERROR_F("TCP", fmt, ##__VA_ARGS__)
#define LOG_TCP_WARN(fmt, ...)      LOG_WARN_F("TCP", fmt, ##__VA_ARGS__)
#define LOG_TCP_INFO(fmt, ...)      LOG_INFO_F("TCP", fmt, ##__VA_ARGS__)
#define LOG_TCP_DEBUG(fmt, ...)     LOG_DEBUG_F("TCP", fmt, ##__VA_ARGS__)
#define LOG_TCP_VERBOSE(fmt, ...)   LOG_VERBOSE_F("TCP", fmt, ##__VA_ARGS__)

// --- MQTT ---
#define LOG_MQTT_ERROR(fmt, ...)    LOG_ERROR_F("MQTT", fmt, ##__VA_ARGS__)
#define LOG_MQTT_WARN(fmt, ...)     LOG_WARN_F("MQTT", fmt, ##__VA_ARGS__)
#define LOG_MQTT_INFO(fmt, ...)     LOG_INFO_F("MQTT", fmt, ##__VA_ARGS__)
#define LOG_MQTT_DEBUG(fmt, ...)    LOG_DEBUG_F("MQTT", fmt, ##__VA_ARGS__)
#define LOG_MQTT_VERBOSE(fmt, ...)  LOG_VERBOSE_F("MQTT", fmt, ##__VA_ARGS__)

// --- HTTP ---
#define LOG_HTTP_ERROR(fmt, ...)    LOG_ERROR_F("HTTP", fmt, ##__VA_ARGS__)
#define LOG_HTTP_WARN(fmt, ...)     LOG_WARN_F("HTTP", fmt, ##__VA_ARGS__)
#define LOG_HTTP_INFO(fmt, ...)     LOG_INFO_F("HTTP", fmt, ##__VA_ARGS__)
#define LOG_HTTP_DEBUG(fmt, ...)    LOG_DEBUG_F("HTTP", fmt, ##__VA_ARGS__)

// --- BLE ---
#define LOG_BLE_ERROR(fmt, ...)     LOG_ERROR_F("BLE", fmt, ##__VA_ARGS__)
#define LOG_BLE_WARN(fmt, ...)      LOG_WARN_F("BLE", fmt, ##__VA_ARGS__)
#define LOG_BLE_INFO(fmt, ...)      LOG_INFO_F("BLE", fmt, ##__VA_ARGS__)
#define LOG_BLE_DEBUG(fmt, ...)     LOG_DEBUG_F("BLE", fmt, ##__VA_ARGS__)

// --- BATCH MANAGER ---
#define LOG_BATCH_ERROR(fmt, ...)   LOG_ERROR_F("BATCH", fmt, ##__VA_ARGS__)
#define LOG_BATCH_WARN(fmt, ...)    LOG_WARN_F("BATCH", fmt, ##__VA_ARGS__)
#define LOG_BATCH_INFO(fmt, ...)    LOG_INFO_F("BATCH", fmt, ##__VA_ARGS__)
#define LOG_BATCH_DEBUG(fmt, ...)   LOG_DEBUG_F("BATCH", fmt, ##__VA_ARGS__)

// --- CONFIG MANAGER ---
#define LOG_CONFIG_ERROR(fmt, ...)  LOG_ERROR_F("CONFIG", fmt, ##__VA_ARGS__)
#define LOG_CONFIG_WARN(fmt, ...)   LOG_WARN_F("CONFIG", fmt, ##__VA_ARGS__)
#define LOG_CONFIG_INFO(fmt, ...)   LOG_INFO_F("CONFIG", fmt, ##__VA_ARGS__)
#define LOG_CONFIG_DEBUG(fmt, ...)  LOG_DEBUG_F("CONFIG", fmt, ##__VA_ARGS__)

// --- NETWORK MANAGER ---
#define LOG_NET_ERROR(fmt, ...)     LOG_ERROR_F("NET", fmt, ##__VA_ARGS__)
#define LOG_NET_WARN(fmt, ...)      LOG_WARN_F("NET", fmt, ##__VA_ARGS__)
#define LOG_NET_INFO(fmt, ...)      LOG_INFO_F("NET", fmt, ##__VA_ARGS__)
#define LOG_NET_DEBUG(fmt, ...)     LOG_DEBUG_F("NET", fmt, ##__VA_ARGS__)

// --- MEMORY MANAGER ---
#define LOG_MEM_ERROR(fmt, ...)     LOG_ERROR_F("MEM", fmt, ##__VA_ARGS__)
#define LOG_MEM_WARN(fmt, ...)      LOG_WARN_F("MEM", fmt, ##__VA_ARGS__)
#define LOG_MEM_INFO(fmt, ...)      LOG_INFO_F("MEM", fmt, ##__VA_ARGS__)
#define LOG_MEM_DEBUG(fmt, ...)     LOG_DEBUG_F("MEM", fmt, ##__VA_ARGS__)

// --- QUEUE MANAGER ---
#define LOG_QUEUE_ERROR(fmt, ...)   LOG_ERROR_F("QUEUE", fmt, ##__VA_ARGS__)
#define LOG_QUEUE_WARN(fmt, ...)    LOG_WARN_F("QUEUE", fmt, ##__VA_ARGS__)
#define LOG_QUEUE_INFO(fmt, ...)    LOG_INFO_F("QUEUE", fmt, ##__VA_ARGS__)
#define LOG_QUEUE_DEBUG(fmt, ...)   LOG_DEBUG_F("QUEUE", fmt, ##__VA_ARGS__)

// --- DATA / TELEMETRY ---
#define LOG_DATA_ERROR(fmt, ...)    LOG_ERROR_F("DATA", fmt, ##__VA_ARGS__)
#define LOG_DATA_WARN(fmt, ...)     LOG_WARN_F("DATA", fmt, ##__VA_ARGS__)
#define LOG_DATA_INFO(fmt, ...)     LOG_INFO_F("DATA", fmt, ##__VA_ARGS__)
#define LOG_DATA_DEBUG(fmt, ...)    LOG_DEBUG_F("DATA", fmt, ##__VA_ARGS__)

// --- LED MANAGER ---
#define LOG_LED_ERROR(fmt, ...)     LOG_ERROR_F("LED", fmt, ##__VA_ARGS__)
#define LOG_LED_WARN(fmt, ...)      LOG_WARN_F("LED", fmt, ##__VA_ARGS__)
#define LOG_LED_INFO(fmt, ...)      LOG_INFO_F("LED", fmt, ##__VA_ARGS__)
#define LOG_LED_DEBUG(fmt, ...)     LOG_DEBUG_F("LED", fmt, ##__VA_ARGS__)

// --- CRUD HANDLER ---
#define LOG_CRUD_ERROR(fmt, ...)    LOG_ERROR_F("CRUD", fmt, ##__VA_ARGS__)
#define LOG_CRUD_WARN(fmt, ...)     LOG_WARN_F("CRUD", fmt, ##__VA_ARGS__)
#define LOG_CRUD_INFO(fmt, ...)     LOG_INFO_F("CRUD", fmt, ##__VA_ARGS__)
#define LOG_CRUD_DEBUG(fmt, ...)    LOG_DEBUG_F("CRUD", fmt, ##__VA_ARGS__)

// --- RTC MANAGER ---
#define LOG_RTC_ERROR(fmt, ...)     LOG_ERROR_F("RTC", fmt, ##__VA_ARGS__)
#define LOG_RTC_WARN(fmt, ...)      LOG_WARN_F("RTC", fmt, ##__VA_ARGS__)
#define LOG_RTC_INFO(fmt, ...)      LOG_INFO_F("RTC", fmt, ##__VA_ARGS__)
#define LOG_RTC_DEBUG(fmt, ...)     LOG_DEBUG_F("RTC", fmt, ##__VA_ARGS__)

// ============================================
// THROTTLED LOGGING (prevent log spam)
// ============================================
class LogThrottle {
private:
  unsigned long lastLogTime = 0;
  uint32_t intervalMs;
  uint32_t suppressedCount = 0;

public:
  LogThrottle(uint32_t intervalMs = 10000) : intervalMs(intervalMs) {}

  bool shouldLog() {
    unsigned long now = millis();
    if (now - lastLogTime >= intervalMs) {
      if (suppressedCount > 0) {
        Serial.printf("  (suppressed %lu similar messages)\n", suppressedCount);
        suppressedCount = 0;
      }
      lastLogTime = now;
      return true;
    }
    suppressedCount++;
    return false;
  }

  void setInterval(uint32_t newIntervalMs) {
    intervalMs = newIntervalMs;
  }

  void reset() {
    lastLogTime = 0;
    suppressedCount = 0;
  }

  uint32_t getSuppressedCount() const {
    return suppressedCount;
  }
};

// ============================================
// BACKWARD COMPATIBILITY (legacy debug macros)
// ============================================
#if PRODUCTION_MODE == 1
// Production Mode - All debug output disabled

#define DEBUG_PRINT(fmt, ...) do {} while(0)
#define DEBUG_PRINTLN(msg) do {} while(0)
#define DEBUG_DEVICE_PRINT(fmt, ...) do {} while(0)
#define DEBUG_MODBUS_PRINT(fmt, ...) do {} while(0)
#define DEBUG_BLE_PRINT(fmt, ...) do {} while(0)
#define DEBUG_MQTT_PRINT(fmt, ...) do {} while(0)
#define DEBUG_NETWORK_PRINT(fmt, ...) do {} while(0)
#define DEBUG_HTTP_PRINT(fmt, ...) do {} while(0)
#define DEBUG_CONFIG_PRINT(fmt, ...) do {} while(0)
#define DEBUG_MEMORY_PRINT(fmt, ...) do {} while(0)
#define DEBUG_ERROR_PRINT(fmt, ...) do {} while(0)
#define DEBUG_METRICS_PRINT(fmt, ...) do {} while(0)

#else
// Development Mode - All debug output enabled

#define DEBUG_PRINT(fmt, ...) Serial.printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#define DEBUG_PRINTLN(msg) Serial.println("[DEBUG] " msg)
#define DEBUG_DEVICE_PRINT(fmt, ...) Serial.printf("[DEVICE] " fmt "\n", ##__VA_ARGS__)
#define DEBUG_MODBUS_PRINT(fmt, ...) Serial.printf("[MODBUS] " fmt "\n", ##__VA_ARGS__)
#define DEBUG_BLE_PRINT(fmt, ...) Serial.printf("[BLE] " fmt "\n", ##__VA_ARGS__)
#define DEBUG_MQTT_PRINT(fmt, ...) Serial.printf("[MQTT] " fmt "\n", ##__VA_ARGS__)
#define DEBUG_NETWORK_PRINT(fmt, ...) Serial.printf("[NETWORK] " fmt "\n", ##__VA_ARGS__)
#define DEBUG_HTTP_PRINT(fmt, ...) Serial.printf("[HTTP] " fmt "\n", ##__VA_ARGS__)
#define DEBUG_CONFIG_PRINT(fmt, ...) Serial.printf("[CONFIG] " fmt "\n", ##__VA_ARGS__)
#define DEBUG_MEMORY_PRINT(fmt, ...) Serial.printf("[MEMORY] " fmt "\n", ##__VA_ARGS__)
#define DEBUG_ERROR_PRINT(fmt, ...) Serial.printf("[ERROR_LOG] " fmt "\n", ##__VA_ARGS__)
#define DEBUG_METRICS_PRINT(fmt, ...) Serial.printf("[METRICS] " fmt "\n", ##__VA_ARGS__)

#endif

// ============================================
// MODE CHECKING MACROS
// ============================================
#if PRODUCTION_MODE == 0
#define IS_DEBUG_MODE() true
#define IS_PRODUCTION_MODE() false
#else
#define IS_DEBUG_MODE() false
#define IS_PRODUCTION_MODE() true
#endif

// Conditional code execution based on mode
#if PRODUCTION_MODE == 0
#define DEBUG_CODE(code) code
#define PRODUCTION_SKIP(code) do {} while(0)
#else
#define DEBUG_CODE(code) do {} while(0)
#define PRODUCTION_SKIP(code) code
#endif

// ============================================
// MODE INDICATOR FUNCTION
// ============================================
#if PRODUCTION_MODE == 0
inline void debugPrintMode()
{
  Serial.println("\n=================================================");
  Serial.println("   DEVELOPMENT MODE - DEBUG ACTIVE");
  Serial.println("   All monitoring & debug output enabled");
  Serial.printf("   Default log level: %s\n", getLogLevelName(currentLogLevel));
  Serial.println("=================================================");
}
#else
inline void debugPrintMode() {}
#endif

// ============================================
// PRODUCTION MODE OPTIMIZATIONS
// ============================================
#if PRODUCTION_MODE == 1
// OPTIMIZED: Disable verbose MTU negotiation logging in production
#define VERBOSE_MTU_LOG 0
// OPTIMIZED: Disable verbose PSRAM allocation logging in production
#define VERBOSE_PSRAM_LOG 0
// OPTIMIZED: Reduce MQTT queue diagnostics in production
#define VERBOSE_MQTT_QUEUE_LOG 0
#else
// Development mode - enable all verbose logging
#define VERBOSE_MTU_LOG 1
#define VERBOSE_PSRAM_LOG 1
#define VERBOSE_MQTT_QUEUE_LOG 1
#endif

#endif // DEBUG_CONFIG_H
