#ifndef MODBUS_TCP_SERVICE_H
#define MODBUS_TCP_SERVICE_H

#include "JsonDocumentPSRAM.h"  // BUG #31: MUST BE BEFORE ArduinoJson.h
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "ConfigManager.h"
#include "EthernetManager.h"
#include "TCPClient.h"  // FIXED BUG #14: Required for connection pooling
#include <vector>
#include <queue>
#include <atomic>
#include <memory> // For std::unique_ptr (Bug #2 fix)

// FIXED Bug #10: Named constants instead of magic numbers
namespace ModbusTcpConfig {
  constexpr uint32_t TIMEOUT_MS = 5000;                 // Default timeout for Modbus TCP operations
  constexpr uint16_t MAX_RESPONSE_SIZE = 512;           // Maximum acceptable Modbus TCP response size
  constexpr uint8_t MODBUS_TCP_HEADER_SIZE = 12;        // Modbus TCP request header size
  constexpr uint8_t MIN_RESPONSE_SIZE = 9;              // Minimum Modbus TCP response size
}

class ModbusTcpService
{
private:
  ConfigManager *configManager;
  EthernetManager *ethernetManager;
  bool running;
  TaskHandle_t tcpTaskHandle;

  // 2-Level Polling Hierarchy (CLEANUP: Removed Level 1 per-register polling)

  // Level 1: Device-level timing (device refresh_rate)
  struct DeviceTimer
  {
    String deviceId;
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

  struct TcpDeviceConfig
  {
    String deviceId;
    std::unique_ptr<JsonDocument> doc; // FIXED Bug #2: Use smart pointer for auto-cleanup
  };
  std::vector<TcpDeviceConfig> tcpDevices;

  // NEW: Enhancement - Device Failure State Tracking (matching RTU service)
  struct DeviceFailureState
  {
    String deviceId;  // TODO: Convert to PSRAMString in future (BUG #31 continuation)
    uint8_t consecutiveFailures = 0;
    uint8_t retryCount = 0;
    unsigned long nextRetryTime = 0;
    unsigned long lastReadAttempt = 0;
    unsigned long lastSuccessfulRead = 0;
    bool isEnabled = true;
    uint32_t maxRetries = 5;

    // Flexible disable reason tracking
    enum DisableReason {
      NONE = 0,
      MANUAL = 1,
      AUTO_RETRY = 2,
      AUTO_TIMEOUT = 3
    };
    DisableReason disableReason = NONE;
    String disableReasonDetail;
    unsigned long disabledTimestamp = 0;
  };
  std::vector<DeviceFailureState> deviceFailureStates;

  // NEW: Enhancement - Device Read Timeout Configuration
  struct DeviceReadTimeout
  {
    String deviceId;
    uint16_t timeoutMs = 5000;
    uint8_t consecutiveTimeouts = 0;
    unsigned long lastSuccessfulRead = 0;
    uint8_t maxConsecutiveTimeouts = 3;
  };
  std::vector<DeviceReadTimeout> deviceTimeouts;

  // NEW: Enhancement - Device Health Metrics Tracking
  struct DeviceHealthMetrics
  {
    String deviceId;

    uint32_t totalReads = 0;
    uint32_t successfulReads = 0;
    uint32_t failedReads = 0;

    uint32_t totalResponseTimeMs = 0;
    uint16_t minResponseTimeMs = 65535;
    uint16_t maxResponseTimeMs = 0;
    uint16_t lastResponseTimeMs = 0;

    float getSuccessRate() const {
      if (totalReads == 0) return 100.0f;
      return (successfulReads * 100.0f) / totalReads;
    }

    uint16_t getAvgResponseTimeMs() const {
      if (successfulReads == 0) return 0;
      return totalResponseTimeMs / successfulReads;
    }

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
  std::vector<DeviceHealthMetrics> deviceMetrics;

  // --- New Scheduler Structures ---
  struct PollingTask
  {
    String deviceId;
    unsigned long nextPollTime;

    // Overload > operator for the priority queue (min-heap)
    bool operator>(const PollingTask &other) const
    {
      return nextPollTime > other.nextPollTime;
    }
  };

  std::priority_queue<PollingTask, std::vector<PollingTask>, std::greater<PollingTask>> pollingQueue;
  // --------------------------------

  // Atomic Transaction Counter (Modbus TCP Improvement Phase 2)
  static std::atomic<uint16_t> atomicTransactionCounter;

  // FIXED BUG #14: TCP Connection Pooling
  // Keep persistent connections for frequently polled devices
  struct ConnectionPoolEntry {
    String deviceKey;              // "IP:PORT" identifier
    TCPClient* client;             // Persistent connection
    unsigned long lastUsed;        // Last activity timestamp
    unsigned long createdAt;       // Connection creation time
    uint32_t useCount;             // Number of times reused
    bool isHealthy;                // Connection health status
  };
  std::vector<ConnectionPoolEntry> connectionPool;
  SemaphoreHandle_t poolMutex;     // Protect connection pool access
  static constexpr uint32_t CONNECTION_IDLE_TIMEOUT_MS = 60000;  // Close after 60s idle
  static constexpr uint32_t CONNECTION_MAX_AGE_MS = 300000;       // Recreate after 5min
  static constexpr uint8_t MAX_POOL_SIZE = 10;                    // Max concurrent connections

  // Connection pool methods
  TCPClient* getPooledConnection(const String &ip, int port);
  void returnPooledConnection(const String &ip, int port, TCPClient* client, bool healthy);
  void closeIdleConnections();
  void closeAllConnections();
  String getDeviceKey(const String &ip, int port);

  static void readTcpDevicesTask(void *parameter);
  void readTcpDevicesLoop();
  void readTcpDeviceData(const JsonObject &deviceConfig);
  double processRegisterValue(const JsonObject &reg, uint16_t rawValue);
  double processMultiRegisterValue(const JsonObject &reg, uint16_t *values, int count, const String &baseType = "", const String &endianness_variant = "");
  bool storeRegisterValue(const String &deviceId, const JsonObject &reg, double value, const String &deviceName = "");  // FIXED: Returns bool for error handling
  bool readModbusRegister(const String &ip, int port, uint8_t slaveId, uint8_t functionCode, uint16_t address, uint16_t *result, TCPClient* existingClient = nullptr);
  bool readModbusRegisters(const String &ip, int port, uint8_t slaveId, uint8_t functionCode, uint16_t address, int count, uint16_t *results, TCPClient* existingClient = nullptr);
  bool readModbusCoil(const String &ip, int port, uint8_t slaveId, uint16_t address, bool *result, TCPClient* existingClient = nullptr);
  void buildModbusRequest(uint8_t *buffer, uint16_t transId, uint8_t unitId, uint8_t funcCode, uint16_t addr, uint16_t qty);
  bool parseModbusResponse(uint8_t *buffer, int length, uint8_t expectedFunc, uint16_t *result, bool *boolResult);
  bool parseMultiModbusResponse(uint8_t *buffer, int length, uint8_t expectedFunc, int count, uint16_t *results);

  void refreshDeviceList();

  // Polling Hierarchy Helper Methods (CLEANUP: Removed Level 1 per-register methods)
  // Level 1 (Device-level): Device-level timing
  DeviceTimer *getDeviceTimer(const String &deviceId);
  bool shouldPollDevice(const String &deviceId, uint32_t refreshRateMs);
  void updateDeviceLastRead(const String &deviceId);

  // Level 3 (Server-level): Data transmission interval
  bool shouldTransmitData(uint32_t dataIntervalMs);
  void updateDataTransmissionTime();
  void setDataTransmissionInterval(uint32_t intervalMs);

  // Modbus TCP Improvement Phase - Helper Methods

  // Atomic transaction counter
  uint16_t getNextTransactionId();

  // NEW: Enhancement - Device failure and metrics management
  void initializeDeviceFailureTracking();
  void initializeDeviceTimeouts();
  void initializeDeviceMetrics();
  struct DeviceFailureState *getDeviceFailureState(const String &deviceId);
  struct DeviceReadTimeout *getDeviceTimeout(const String &deviceId);
  struct DeviceHealthMetrics *getDeviceMetrics(const String &deviceId);

  // NEW: Enhancement - Flexible enable/disable with reason tracking
  void enableDevice(const String &deviceId, bool clearMetrics = false);
  void disableDevice(const String &deviceId, DeviceFailureState::DisableReason reason, const String &reasonDetail = "");

  // Exponential backoff retry logic
  void handleReadFailure(const String &deviceId);
  bool shouldRetryDevice(const String &deviceId);
  unsigned long calculateBackoffTime(uint8_t retryCount);
  void resetDeviceFailureState(const String &deviceId);

  // Timeout and device management
  void handleReadTimeout(const String &deviceId);
  bool isDeviceEnabled(const String &deviceId);

  // NEW: Enhancement - Auto-recovery for auto-disabled devices
  static void autoRecoveryTask(void *parameter);
  void autoRecoveryLoop();
  TaskHandle_t autoRecoveryTaskHandle = nullptr;

public:
  ModbusTcpService(ConfigManager *config, EthernetManager *ethernet);

  bool init();
  void start();
  void stop();
  void getStatus(JsonObject &status);

  void notifyConfigChange();

  // NEW: Enhancement - Public API for BLE device control commands
  bool enableDeviceByCommand(const String &deviceId, bool clearMetrics = false);
  bool disableDeviceByCommand(const String &deviceId, const String &reasonDetail = "");
  bool getDeviceStatusInfo(const String &deviceId, JsonObject &statusInfo);
  bool getAllDevicesStatus(JsonObject &allStatus);

  ~ModbusTcpService();
};

#endif