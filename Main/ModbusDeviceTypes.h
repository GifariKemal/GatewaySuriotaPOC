#ifndef MODBUS_DEVICE_TYPES_H
#define MODBUS_DEVICE_TYPES_H

/**
 * @file ModbusDeviceTypes.h
 * @brief Shared data structures for ModbusRtuService and ModbusTcpService
 *
 * Refactoring v2.5.41 - Eliminates code duplication (~1074 lines)
 * All structs use PSRAMString based on Espressif research:
 * - PSRAM cache makes small strings (<16KB) perform identically to DRAM
 * - DRAM is limited (512KB) and shared with WiFi/BLE stack
 * - PSRAMString with DRAM fallback is the optimal strategy
 *
 * @see Documentation/Technical_Guides/REFACTORING_MODBUS_SERVICES.md
 */

#include "PSRAMString.h"  // BUG #31: PSRAM-based string for all device tracking

// ============================================================================
// DEVICE POLLING TYPES
// ============================================================================

/**
 * @brief Device-level polling timer
 *
 * Tracks when each device was last polled to respect refresh_rate_ms
 * Used by both RTU and TCP services for device-level timing
 */
struct ModbusDeviceTimer {
  PSRAMString deviceId;    // Device identifier (PSRAM-allocated)
  unsigned long lastRead;  // Timestamp of last read attempt
  uint32_t refreshRateMs;  // Device-specific polling interval

  ModbusDeviceTimer() : lastRead(0), refreshRateMs(1000) {}

  ModbusDeviceTimer(const char* id, uint32_t refreshRate = 1000)
      : deviceId(id), lastRead(0), refreshRateMs(refreshRate) {}
};

/**
 * @brief Server data transmission interval
 *
 * Controls how often data is sent to MQTT/HTTP backends
 * Separate from device polling rate
 */
struct ModbusDataTransmissionInterval {
  unsigned long lastTransmitted;  // Last time data was sent to MQTT/HTTP
  uint32_t
      dataIntervalMs;  // Interval for data transmission (from server_config)

  ModbusDataTransmissionInterval() : lastTransmitted(0), dataIntervalMs(5000) {}
};

// ============================================================================
// DEVICE FAILURE TRACKING
// ============================================================================

/**
 * @brief Reason why a device was disabled
 *
 * Used for diagnostic purposes and auto-recovery logic
 */
enum class ModbusDisableReason : uint8_t {
  NONE = 0,         // Device is enabled
  MANUAL = 1,       // Manually disabled via BLE command
  AUTO_RETRY = 2,   // Auto-disabled: max retries exceeded
  AUTO_TIMEOUT = 3  // Auto-disabled: max consecutive timeouts
};

/**
 * @brief Device failure state tracking
 *
 * Tracks consecutive failures, retry attempts, and exponential backoff
 * Common to both RTU and TCP services
 */
struct ModbusDeviceFailureState {
  PSRAMString deviceId;                  // Device identifier
  uint8_t consecutiveFailures = 0;       // Track consecutive read failures
  uint8_t retryCount = 0;                // Current retry attempt count
  unsigned long nextRetryTime = 0;       // When to retry (exponential backoff)
  unsigned long lastReadAttempt = 0;     // Timestamp of last read attempt
  unsigned long lastSuccessfulRead = 0;  // Track last success time for metrics
  bool isEnabled = true;                 // Enable/disable for polling
  uint32_t maxRetries = 5;  // Max retry attempts before auto-disable

  // RTU-specific: baud rate (ignored by TCP, safe to include)
  uint16_t baudRate = 9600;

  // Flexible disable reason tracking
  ModbusDisableReason disableReason = ModbusDisableReason::NONE;
  PSRAMString
      disableReasonDetail;  // User-provided reason (e.g., "maintenance")
  unsigned long disabledTimestamp =
      0;  // When device was disabled (for auto-recovery)

  ModbusDeviceFailureState() = default;

  explicit ModbusDeviceFailureState(const char* id) : deviceId(id) {}
};

// ============================================================================
// DEVICE TIMEOUT CONFIGURATION
// ============================================================================

/**
 * @brief Per-device timeout configuration
 *
 * Allows different timeout settings per device
 * Auto-disables device after consecutive timeouts
 */
struct ModbusDeviceReadTimeout {
  PSRAMString deviceId;             // Device identifier
  uint16_t timeoutMs = 5000;        // Per-device timeout (5 seconds default)
  uint8_t consecutiveTimeouts = 0;  // Track consecutive timeouts
  unsigned long lastSuccessfulRead = 0;  // Last successful read time
  uint8_t maxConsecutiveTimeouts = 3;    // Disable device after N timeouts

  ModbusDeviceReadTimeout() = default;

  explicit ModbusDeviceReadTimeout(const char* id, uint16_t timeout = 5000)
      : deviceId(id), timeoutMs(timeout) {}
};

// ============================================================================
// DEVICE HEALTH METRICS
// ============================================================================

/**
 * @brief Device health metrics tracking
 *
 * Collects statistics for monitoring device reliability
 * Includes success rate, response times, and read counts
 */
struct ModbusDeviceHealthMetrics {
  PSRAMString deviceId;

  // Counters
  uint32_t totalReads = 0;
  uint32_t successfulReads = 0;
  uint32_t failedReads = 0;

  // Response time tracking (in milliseconds)
  uint32_t totalResponseTimeMs = 0;    // Sum for averaging
  uint16_t minResponseTimeMs = 65535;  // Min response time (init to max)
  uint16_t maxResponseTimeMs = 0;      // Max response time
  uint16_t lastResponseTimeMs = 0;     // Most recent response time

  ModbusDeviceHealthMetrics() = default;

  explicit ModbusDeviceHealthMetrics(const char* id) : deviceId(id) {}

  /**
   * @brief Calculate success rate percentage
   * @return Success rate (0-100%)
   */
  float getSuccessRate() const {
    if (totalReads == 0) return 100.0f;
    return (successfulReads * 100.0f) / totalReads;
  }

  /**
   * @brief Calculate average response time
   * @return Average response time in milliseconds
   */
  uint16_t getAvgResponseTimeMs() const {
    if (successfulReads == 0) return 0;
    return static_cast<uint16_t>(totalResponseTimeMs / successfulReads);
  }

  /**
   * @brief Record a read attempt (success or failure)
   * @param success Whether the read was successful
   * @param responseTimeMs Response time for successful reads
   */
  void recordRead(bool success, uint16_t responseTimeMs = 0) {
    totalReads++;
    if (success) {
      successfulReads++;
      totalResponseTimeMs += responseTimeMs;
      lastResponseTimeMs = responseTimeMs;
      if (responseTimeMs < minResponseTimeMs)
        minResponseTimeMs = responseTimeMs;
      if (responseTimeMs > maxResponseTimeMs)
        maxResponseTimeMs = responseTimeMs;
    } else {
      failedReads++;
    }
  }

  /**
   * @brief Reset all metrics to initial state
   */
  void reset() {
    totalReads = 0;
    successfulReads = 0;
    failedReads = 0;
    totalResponseTimeMs = 0;
    minResponseTimeMs = 65535;
    maxResponseTimeMs = 0;
    lastResponseTimeMs = 0;
  }
};

// ============================================================================
// CONFIGURATION CONSTANTS
// ============================================================================

namespace ModbusDeviceConfig {
// Backoff timing
constexpr unsigned long RTU_BASE_BACKOFF_MS = 100;  // RTU: Fast serial recovery
constexpr unsigned long TCP_BASE_BACKOFF_MS =
    2000;  // TCP: Slower network recovery
constexpr unsigned long MAX_BACKOFF_MULTIPLIER = 32;  // Max 32x base delay

// Auto-recovery
constexpr unsigned long AUTO_RECOVERY_CHECK_INTERVAL_MS =
    60000;  // Check every 1 minute
constexpr unsigned long AUTO_RECOVERY_DELAY_MS =
    300000;  // 5 minutes before retry

// Default timeouts
constexpr uint16_t DEFAULT_DEVICE_TIMEOUT_MS = 5000;
constexpr uint8_t DEFAULT_MAX_CONSECUTIVE_TIMEOUTS = 3;
constexpr uint32_t DEFAULT_MAX_RETRIES = 5;
}  // namespace ModbusDeviceConfig

#endif  // MODBUS_DEVICE_TYPES_H
