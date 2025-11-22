#ifndef MODBUS_RTU_SERVICE_H
#define MODBUS_RTU_SERVICE_H

#include "JsonDocumentPSRAM.h"  // BUG #31: MUST BE BEFORE ArduinoJson.h
#include <ArduinoJson.h>
#include "PSRAMString.h"         // BUG #31: Replace Arduino String with PSRAM-based String
#include <HardwareSerial.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ModbusMaster.h>
#include "ConfigManager.h"
#include <vector>
#include <queue>  // For std::priority_queue
#include <memory> // For std::unique_ptr (Bug #2 fix)

class ModbusRtuService
{
private:
  ConfigManager *configManager;
  bool running;
  TaskHandle_t rtuTaskHandle;

  // 2-Level Polling Hierarchy (CLEANUP: Removed Level 1 per-register polling)

  // Level 1: Device-level timing (device refresh_rate)
  struct DeviceTimer
  {
    PSRAMString deviceId;  // BUG #31: Use PSRAM instead of DRAM
    unsigned long lastRead; // Last time device's registers were collectively polled
    uint32_t refreshRateMs; // Device-level interval
  };
  std::vector<DeviceTimer> deviceTimers;

  // Level 2: Server data transmission interval (data_interval untuk MQTT/HTTP)
  struct DataTransmissionInterval
  {
    unsigned long lastTransmitted; // Last time data was sent to MQTT/HTTP
    uint32_t dataIntervalMs;       // Interval for data transmission (from server_config)
  } dataTransmissionSchedule;

  struct RtuDeviceConfig
  {
    PSRAMString deviceId;  // BUG #31: Use PSRAM instead of DRAM
    std::unique_ptr<JsonDocument> doc; // FIXED Bug #2: Use smart pointer for auto-cleanup
  };
  std::vector<RtuDeviceConfig> rtuDevices;

  // Device Failure State Tracking (Modbus Improvement Phase 1)
  struct DeviceFailureState
  {
    PSRAMString deviceId;  // BUG #31: Use PSRAM instead of DRAM
    uint8_t consecutiveFailures = 0;   // Track consecutive read failures
    uint8_t retryCount = 0;            // Current retry attempt count
    unsigned long nextRetryTime = 0;   // When to retry (exponential backoff)
    unsigned long lastReadAttempt = 0; // Timestamp of last read attempt
    unsigned long lastSuccessfulRead = 0; // NEW: Track last success time for metrics
    bool isEnabled = true;             // Enable/disable for polling
    uint16_t baudRate = 9600;          // Per-device baud rate
    uint32_t maxRetries = 5;           // Max retry attempts

    // NEW: Enhancement - Flexible Disable Reason Tracking
    enum DisableReason {
      NONE = 0,           // Device is enabled
      MANUAL = 1,         // Manually disabled via BLE command
      AUTO_RETRY = 2,     // Auto-disabled: max retries exceeded
      AUTO_TIMEOUT = 3    // Auto-disabled: max consecutive timeouts
    };
    DisableReason disableReason = NONE;
    PSRAMString disableReasonDetail;      // User-provided reason (e.g., "maintenance")
    unsigned long disabledTimestamp = 0;  // When device was disabled (for auto-recovery)
  };
  std::vector<DeviceFailureState> deviceFailureStates; // Dynamic device tracking

  // Device Read Timeout Configuration (Modbus Improvement Phase 2)
  struct DeviceReadTimeout
  {
    PSRAMString deviceId;  // BUG #31: Use PSRAM instead of DRAM
    uint16_t timeoutMs = 5000;            // Per-device timeout (5 seconds default)
    uint8_t consecutiveTimeouts = 0;      // Track consecutive timeouts
    unsigned long lastSuccessfulRead = 0; // Last successful read time
    uint8_t maxConsecutiveTimeouts = 3;   // Disable device after 3 timeouts
  };
  std::vector<DeviceReadTimeout> deviceTimeouts; // Dynamic timeout tracking

  // NEW: Enhancement - Device Health Metrics Tracking (Phase 2)
  struct DeviceHealthMetrics
  {
    PSRAMString deviceId;

    // Counters
    uint32_t totalReads = 0;
    uint32_t successfulReads = 0;
    uint32_t failedReads = 0;

    // Response time tracking (in milliseconds)
    uint32_t totalResponseTimeMs = 0;  // Sum for averaging
    uint16_t minResponseTimeMs = 65535;
    uint16_t maxResponseTimeMs = 0;
    uint16_t lastResponseTimeMs = 0;

    // Calculate metrics
    float getSuccessRate() const {
      if (totalReads == 0) return 100.0f;
      return (successfulReads * 100.0f) / totalReads;
    }

    uint16_t getAvgResponseTimeMs() const {
      if (successfulReads == 0) return 0;
      return totalResponseTimeMs / successfulReads;
    }

    // Record a read attempt
    void recordRead(bool success, uint16_t responseTimeMs = 0) {
      totalReads++;
      if (success) {
        successfulReads++;
        totalResponseTimeMs += responseTimeMs;
        lastResponseTimeMs = responseTimeMs;
        if (responseTimeMs < minResponseTimeMs) minResponseTimeMs = responseTimeMs;
        if (responseTimeMs > maxResponseTimeMs) maxResponseTimeMs = responseTimeMs;
      } else {
        failedReads++;
      }
    }
  };
  std::vector<DeviceHealthMetrics> deviceMetrics; // Dynamic metrics tracking

  // --- New Scheduler Structures ---
  struct PollingTask
  {
    PSRAMString deviceId;  // BUG #31: Use PSRAM instead of DRAM
    unsigned long nextPollTime;

    // Overload > operator for the priority queue (min-heap)
    bool operator>(const PollingTask &other) const
    {
      return nextPollTime > other.nextPollTime;
    }
  };

  std::priority_queue<PollingTask, std::vector<PollingTask>, std::greater<PollingTask>> pollingQueue;
  // --------------------------------

  static const int RTU_RX1 = 15;
  static const int RTU_TX1 = 16;
  static const int RTU_RX2 = 17;
  static const int RTU_TX2 = 18;

  HardwareSerial *serial1;
  HardwareSerial *serial2;
  ModbusMaster *modbus1;
  ModbusMaster *modbus2;

  // Baudrate caching to avoid unnecessary serial reconfig
  uint32_t currentBaudRate1;
  uint32_t currentBaudRate2;

  // Helper method for dynamic baudrate switching
  void configureBaudRate(int serialPort, uint32_t baudRate);
  bool validateBaudRate(uint32_t baudRate);

  static void readRtuDevicesTask(void *parameter);
  void readRtuDevicesLoop();
  void readRtuDeviceData(const JsonObject &deviceConfig);
  double processRegisterValue(const JsonObject &reg, uint16_t rawValue);
  double processMultiRegisterValue(const JsonObject &reg, uint16_t *values, int count, const char* baseType, const char* endianness_variant);  // BUG #31: const char* instead of String
  bool readMultipleRegisters(ModbusMaster *modbus, uint8_t functionCode, uint16_t address, int count, uint16_t *values);
  bool storeRegisterValue(const char* deviceId, const JsonObject &reg, double value, const char* deviceName = "");  // BUG #31: const char* instead of String, FIXED: Returns bool for error handling
  ModbusMaster *getModbusForBus(int serialPort);

  void refreshDeviceList();

  // Polling Hierarchy Helper Methods (CLEANUP: Removed Level 1 per-register methods)
  // Level 1 (Device-level): Device-level timing
  DeviceTimer *getDeviceTimer(const char* deviceId);  // BUG #31: const char* instead of String
  bool shouldPollDevice(const char* deviceId, uint32_t refreshRateMs);  // BUG #31: const char* instead of String
  void updateDeviceLastRead(const char* deviceId);  // BUG #31: const char* instead of String

  // Level 2 (Server-level): Data transmission interval
  bool shouldTransmitData(uint32_t dataIntervalMs);
  void updateDataTransmissionTime();
  void setDataTransmissionInterval(uint32_t intervalMs);

  // Modbus Improvement Phase - Helper Methods
  void initializeDeviceFailureTracking();
  void initializeDeviceTimeouts();
  void initializeDeviceMetrics();  // NEW: Enhancement - Initialize metrics tracking
  struct DeviceFailureState *getDeviceFailureState(const char* deviceId);  // BUG #31: const char* instead of String
  struct DeviceReadTimeout *getDeviceTimeout(const char* deviceId);  // BUG #31: const char* instead of String
  struct DeviceHealthMetrics *getDeviceMetrics(const char* deviceId);  // NEW: Enhancement - Get metrics

  // Baud rate configuration
  bool configureDeviceBaudRate(const char* deviceId, uint16_t baudRate);  // BUG #31: const char* instead of String
  uint16_t getDeviceBaudRate(const char* deviceId);  // BUG #31: const char* instead of String

  // Exponential backoff retry logic
  void handleReadFailure(const char* deviceId);  // BUG #31: const char* instead of String
  bool shouldRetryDevice(const char* deviceId);  // BUG #31: const char* instead of String
  unsigned long calculateBackoffTime(uint8_t retryCount);
  void resetDeviceFailureState(const char* deviceId);  // BUG #31: const char* instead of String

  // Timeout and device management
  void handleReadTimeout(const char* deviceId);  // BUG #31: const char* instead of String
  bool isDeviceEnabled(const char* deviceId);  // BUG #31: const char* instead of String

  // NEW: Enhancement - Flexible enable/disable with reason tracking
  void enableDevice(const char* deviceId, bool clearMetrics = false);  // Updated signature
  void disableDevice(const char* deviceId, DeviceFailureState::DisableReason reason, const char* reasonDetail = "");  // Updated signature

  // NEW: Enhancement - Auto-recovery for auto-disabled devices
  static void autoRecoveryTask(void *parameter);
  void autoRecoveryLoop();
  TaskHandle_t autoRecoveryTaskHandle = nullptr;

public:
  ModbusRtuService(ConfigManager *config);

  bool init();
  void start();
  void stop();
  void getStatus(JsonObject &status);

  void notifyConfigChange();

  // NEW: Enhancement - Public API for BLE device control commands
  bool enableDeviceByCommand(const char* deviceId, bool clearMetrics = false);
  bool disableDeviceByCommand(const char* deviceId, const char* reasonDetail = "");
  bool getDeviceStatusInfo(const char* deviceId, JsonObject &statusInfo);
  bool getAllDevicesStatus(JsonObject &allStatus);

  ~ModbusRtuService();
};

#endif