#ifndef MODBUS_RTU_SERVICE_H
#define MODBUS_RTU_SERVICE_H

#include "JsonDocumentPSRAM.h"  // BUG #31: MUST BE BEFORE ArduinoJson.h
#include <ArduinoJson.h>
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

  struct RtuDeviceConfig
  {
    String deviceId;
    std::unique_ptr<JsonDocument> doc; // FIXED Bug #2: Use smart pointer for auto-cleanup
  };
  std::vector<RtuDeviceConfig> rtuDevices;

  // Device Failure State Tracking (Modbus Improvement Phase 1)
  struct DeviceFailureState
  {
    String deviceId;
    uint8_t consecutiveFailures = 0;   // Track consecutive read failures
    uint8_t retryCount = 0;            // Current retry attempt count
    unsigned long nextRetryTime = 0;   // When to retry (exponential backoff)
    unsigned long lastReadAttempt = 0; // Timestamp of last read attempt
    bool isEnabled = true;             // Enable/disable for polling
    uint16_t baudRate = 9600;          // Per-device baud rate
    uint32_t maxRetries = 5;           // Max retry attempts
  };
  std::vector<DeviceFailureState> deviceFailureStates; // Dynamic device tracking

  // Device Read Timeout Configuration (Modbus Improvement Phase 2)
  struct DeviceReadTimeout
  {
    String deviceId;
    uint16_t timeoutMs = 5000;            // Per-device timeout (5 seconds default)
    uint8_t consecutiveTimeouts = 0;      // Track consecutive timeouts
    unsigned long lastSuccessfulRead = 0; // Last successful read time
    uint8_t maxConsecutiveTimeouts = 3;   // Disable device after 3 timeouts
  };
  std::vector<DeviceReadTimeout> deviceTimeouts; // Dynamic timeout tracking

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
  double processMultiRegisterValue(const JsonObject &reg, uint16_t *values, int count, const String &baseType, const String &endianness_variant);
  bool readMultipleRegisters(ModbusMaster *modbus, uint8_t functionCode, uint16_t address, int count, uint16_t *values);
  void storeRegisterValue(const String &deviceId, const JsonObject &reg, double value, const String &deviceName = "");
  ModbusMaster *getModbusForBus(int serialPort);

  void refreshDeviceList();

  // Polling Hierarchy Helper Methods (CLEANUP: Removed Level 1 per-register methods)
  // Level 1 (Device-level): Device-level timing
  DeviceTimer *getDeviceTimer(const String &deviceId);
  bool shouldPollDevice(const String &deviceId, uint32_t refreshRateMs);
  void updateDeviceLastRead(const String &deviceId);

  // Level 2 (Server-level): Data transmission interval
  bool shouldTransmitData(uint32_t dataIntervalMs);
  void updateDataTransmissionTime();
  void setDataTransmissionInterval(uint32_t intervalMs);

  // Modbus Improvement Phase - Helper Methods
  void initializeDeviceFailureTracking();
  void initializeDeviceTimeouts();
  struct DeviceFailureState *getDeviceFailureState(const String &deviceId);
  struct DeviceReadTimeout *getDeviceTimeout(const String &deviceId);

  // Baud rate configuration
  bool configureDeviceBaudRate(const String &deviceId, uint16_t baudRate);
  uint16_t getDeviceBaudRate(const String &deviceId);

  // Exponential backoff retry logic
  void handleReadFailure(const String &deviceId);
  bool shouldRetryDevice(const String &deviceId);
  unsigned long calculateBackoffTime(uint8_t retryCount);
  void resetDeviceFailureState(const String &deviceId);

  // Timeout and device management
  void handleReadTimeout(const String &deviceId);
  bool isDeviceEnabled(const String &deviceId);
  void enableDevice(const String &deviceId);
  void disableDevice(const String &deviceId);

public:
  ModbusRtuService(ConfigManager *config);

  bool init();
  void start();
  void stop();
  void getStatus(JsonObject &status);

  void notifyConfigChange();

  ~ModbusRtuService();
};

#endif