#include "DebugConfig.h"  // MUST BE FIRST for DEV_SERIAL_* macros
#include "ModbusRtuService.h"
#include "QueueManager.h"
#include "CRUDHandler.h"
#include "RTCManager.h"
#include "MemoryRecovery.h"
#include <byteswap.h>

extern CRUDHandler *crudHandler;

ModbusRtuService::ModbusRtuService(ConfigManager *config)
    : configManager(config), running(false), rtuTaskHandle(nullptr),
      serial1(nullptr), serial2(nullptr), modbus1(nullptr), modbus2(nullptr)
{
  // Initialize data transmission schedule
  dataTransmissionSchedule.lastTransmitted = 0;
  dataTransmissionSchedule.dataIntervalMs = 5000; // Default 5 seconds
}

bool ModbusRtuService::init()
{
  LOG_RTU_INFO("[RTU] Initializing service with ModbusMaster library...");

  if (!configManager)
  {
    LOG_RTU_INFO("[RTU] ERROR: ConfigManager is null");
    return false;
  }

  // Initialize Serial1 for Bus 1 (default 9600)
  serial1 = new HardwareSerial(1);
  // FIXED Bug #12: Check allocation success
  if (!serial1)
  {
    LOG_RTU_INFO("[RTU] ERROR: Failed to allocate Serial1");
    return false;
  }
  serial1->begin(9600, SERIAL_8N1, RTU_RX1, RTU_TX1);
  serial1->setTimeout(200); // FIXED: Reduce default 1000ms timeout to 200ms
  currentBaudRate1 = 9600;

  // Initialize Serial2 for Bus 2 (default 9600)
  serial2 = new HardwareSerial(2);
  // FIXED Bug #12: Check allocation success
  if (!serial2)
  {
    LOG_RTU_INFO("[RTU] ERROR: Failed to allocate Serial2");
    return false;
  }
  serial2->begin(9600, SERIAL_8N1, RTU_RX2, RTU_TX2);
  serial2->setTimeout(200); // FIXED: Reduce default 1000ms timeout to 200ms
  currentBaudRate2 = 9600;

  // Initialize ModbusMaster instances
  modbus1 = new ModbusMaster();
  // FIXED Bug #12: Check allocation success
  if (!modbus1)
  {
    LOG_RTU_INFO("[RTU] ERROR: Failed to allocate ModbusMaster1");
    return false;
  }
  modbus1->begin(1, *serial1);

  modbus2 = new ModbusMaster();
  // FIXED Bug #12: Check allocation success
  if (!modbus2)
  {
    LOG_RTU_INFO("[RTU] ERROR: Failed to allocate ModbusMaster2");
    return false;
  }
  modbus2->begin(1, *serial2);

  // FIXED ISSUE #1: Initialize vector mutex for thread safety
  // Prevents race conditions when BLE + polling + auto-recovery access vectors simultaneously
  vectorMutex = xSemaphoreCreateRecursiveMutex();
  if (!vectorMutex)
  {
    LOG_RTU_INFO("[RTU] CRITICAL: Failed to create vector mutex!");
    return false;
  }

  LOG_RTU_INFO("[RTU] Service initialized");
  return true;
}

void ModbusRtuService::start()
{
  LOG_RTU_INFO("[RTU] Starting service...");

  if (running)
  {
    return;
  }

  running = true;
  BaseType_t result = xTaskCreatePinnedToCore(
      readRtuDevicesTask,
      "MODBUS_RTU_TASK",
      10240, // STACK OVERFLOW FIX (v2.3.8): Increased from 8KB to 10KB for very safe operation
      this,
      2,
      &rtuTaskHandle, // Store the task handle
      1);

  if (result == pdPASS)
  {
    LOG_RTU_INFO("[RTU] Service started successfully");

    // Start auto-recovery task
    // MUST stay on Core 1: Modifies device status accessed by MODBUS_RTU_TASK (Core 1)
    BaseType_t recoveryResult = xTaskCreatePinnedToCore(
        autoRecoveryTask,
        "RTU_AUTO_RECOVERY",
        4096,
        this,
        1,
        &autoRecoveryTaskHandle,
        1); // Core 1 (must stay with RTU polling task to avoid race conditions)

    if (recoveryResult == pdPASS)
    {
      LOG_RTU_INFO("[RTU] Auto-recovery task started");
    }
    else
    {
      LOG_RTU_INFO("[RTU] WARNING: Failed to create auto-recovery task");
    }
  }
  else
  {
    LOG_RTU_INFO("[RTU] ERROR: Failed to create Modbus RTU task");
    running = false;
    rtuTaskHandle = nullptr;
  }
}

void ModbusRtuService::stop()
{
  LOG_RTU_INFO("[RTU] Stopping service...");
  running = false;

  // Stop auto-recovery task
  if (autoRecoveryTaskHandle)
  {
    vTaskDelay(pdMS_TO_TICKS(200));
    vTaskDelete(autoRecoveryTaskHandle);
    autoRecoveryTaskHandle = nullptr;
    LOG_RTU_INFO("[RTU] Auto-recovery task stopped");
  }

  // CRITICAL FIX: Don't force delete main task - let it exit gracefully
  // Task will self-delete after while(running) loop exits
  if (rtuTaskHandle)
  {
    LOG_RTU_INFO("[RTU] Waiting for main task to exit gracefully...");
    // Wait up to 2 seconds for task to exit loop and self-delete
    for (int i = 0; i < 20; i++)
    {
      vTaskDelay(pdMS_TO_TICKS(100));
      if (rtuTaskHandle == nullptr)
      {
        LOG_RTU_INFO("[RTU] Main task exited gracefully");
        break;
      }
    }
    
    // If still running after 2 seconds, force delete (shouldn't happen)
    if (rtuTaskHandle != nullptr)
    {
      LOG_RTU_INFO("[RTU] WARNING: Force deleting stuck task");
      vTaskDelete(rtuTaskHandle);
      rtuTaskHandle = nullptr;
    }
  }

  LOG_RTU_INFO("[RTU] Service stopped");
}

void ModbusRtuService::notifyConfigChange()
{
  // v2.5.39: Set atomic flag for reliable config change detection
  // Consistent with ModbusTcpService implementation
  configChangePending.store(true);
  LOG_RTU_INFO("[RTU] Config change notified - flagged for refresh\n");

  if (rtuTaskHandle != nullptr)
  {
    xTaskNotifyGive(rtuTaskHandle);
  }
}

void ModbusRtuService::readRtuDevicesTask(void *parameter)
{
  ModbusRtuService *service = static_cast<ModbusRtuService *>(parameter);
  service->readRtuDevicesLoop();
}

void ModbusRtuService::refreshDeviceList()
{
  // FIXED ISSUE #1: Protect vector operations from race conditions
  xSemaphoreTakeRecursive(vectorMutex, portMAX_DELAY);

  LOG_RTU_INFO("[RTU Task] Refreshing device list...");
  rtuDevices.clear(); // FIXED Bug #2: Now safe - unique_ptr auto-deletes old documents

  JsonDocument devicesIdList;
  JsonArray deviceIds = devicesIdList.to<JsonArray>();
  configManager->listDevices(deviceIds);

  unsigned long now = millis();

  for (JsonVariant deviceIdVar : deviceIds)
  {
    const char *deviceId = deviceIdVar.as<const char *>(); // BUG #31: const char* (zero allocation!)
    if (!deviceId || strcmp(deviceId, "") == 0 || strcmp(deviceId, "{}") == 0 || strlen(deviceId) < 3)
    {
      continue;
    }

    JsonDocument tempDeviceDoc;
    JsonObject deviceObj = tempDeviceDoc.to<JsonObject>();
    if (configManager->readDevice(deviceId, deviceObj))
    {
      const char *protocol = deviceObj["protocol"] | ""; // BUG #31: const char* (zero allocation!)
      if (strcmp(protocol, "RTU") == 0)
      {
        RtuDeviceConfig newDeviceEntry;
        newDeviceEntry.deviceId = deviceId;
        newDeviceEntry.doc = std::make_unique<JsonDocument>(); // FIXED Bug #2: Use smart pointer
        newDeviceEntry.doc->set(deviceObj);
        rtuDevices.push_back(std::move(newDeviceEntry));
      }
    }
  }
  LOG_RTU_INFO("[RTU Task] Found %d RTU devices. Schedule rebuilt.\n", rtuDevices.size());

  // Initialize device tracking after device list is loaded
  initializeDeviceFailureTracking();
  initializeDeviceTimeouts();
  initializeDeviceMetrics();

  xSemaphoreGiveRecursive(vectorMutex);
}

void ModbusRtuService::readRtuDevicesLoop()
{
  // Load device list at startup
  refreshDeviceList();

  while (running)
  {
    // ============================================
    // MEMORY RECOVERY CHECK (Phase 2 Optimization)
    // ============================================
    MemoryRecovery::checkAndRecover();

    // v2.5.39: Check BOTH atomic flag AND task notification for reliable config change detection
    // Consistent with ModbusTcpService implementation
    bool shouldRefresh = configChangePending.exchange(false);
    if (shouldRefresh || ulTaskNotifyTake(pdTRUE, 0) > 0)
    {
      LOG_RTU_INFO("[RTU] Config change detected - refreshing device list...\n");
      refreshDeviceList();
    }

    // FIXED ISSUE #1: Use cached rtuDevices vector instead of calling ConfigManager repeatedly
    // This eliminates redundant listDevices() and readDevice() calls every 2 seconds
    // Performance improvement: No file system access in polling loop
    unsigned long currentTime = millis();

    // FIXED ISSUE #2 (REVISED): Non-blocking millis-based timing per device
    // Loop runs continuously with SHORT delay, shouldPollDevice() handles per-device timing
    // This allows independent refresh rates without blocking other devices

    // FIXED ISSUE #1: Protect vector iteration with mutex
    xSemaphoreTakeRecursive(vectorMutex, portMAX_DELAY);

    for (auto &deviceEntry : rtuDevices)
    {
      if (!running)
        break;

      // v2.5.39: Check for config changes during iteration using BOTH atomic flag AND task notification
      // Consistent with ModbusTcpService implementation
      bool midLoopRefresh = configChangePending.exchange(false);
      if (midLoopRefresh || ulTaskNotifyTake(pdTRUE, 0) > 0)
      {
        LOG_RTU_INFO("[RTU] Config change during polling - refreshing immediately...\n");
        refreshDeviceList();
        break; // Exit current iteration, next iteration will use updated device list
      }

      // Use cached device data from rtuDevices vector
      const char *deviceId = deviceEntry.deviceId.c_str();
      JsonObject deviceObj = deviceEntry.doc->as<JsonObject>();

      // Get device refresh rate from BLE config
      uint32_t deviceRefreshRate = deviceObj["refresh_rate_ms"] | 5000;

      // Check if device's refresh interval has elapsed (millis-based, non-blocking)
      // shouldPollDevice() uses per-device lastRead timestamp for accurate timing
      if (shouldPollDevice(deviceId, deviceRefreshRate))
      {
        readRtuDeviceData(deviceObj);
        // Device-level timing is updated inside readRtuDeviceData via updateDeviceLastRead()
      }
    }

    xSemaphoreGiveRecursive(vectorMutex);

    // FIXED ISSUE #2 (REVISED): Constant short delay (non-blocking approach)
    // Loop runs every 100ms to check if any device is ready
    // Per-device timing handled by shouldPollDevice() with millis()
    // This allows accurate, independent timing for each device without blocking
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  
  // CRITICAL FIX: Task must self-delete when loop exits to prevent FreeRTOS abort
  LOG_RTU_INFO("[RTU] Task loop exited, self-deleting...");
  rtuTaskHandle = nullptr; // Clear handle before deletion
  vTaskDelete(NULL); // Delete self (NULL = current task)
}

// ... rest of the functions (readRtuDeviceData, processRegisterValue, etc.) remain the same ...

void ModbusRtuService::readRtuDeviceData(const JsonObject &deviceConfig)
{
  const char *deviceId = deviceConfig["device_id"] | "UNKNOWN"; // BUG #31: const char* (zero allocation!)
  int serialPort = deviceConfig["serial_port"] | 1;
  uint8_t slaveId = deviceConfig["slave_id"] | 1;
  uint32_t deviceRefreshRate = deviceConfig["refresh_rate_ms"] | 5000; // Device-level refresh rate
  uint32_t baudRate = deviceConfig["baud_rate"] | 9600;                // Get device baudrate from config
  JsonArray registers = deviceConfig["registers"];

  // OPTIMIZED: Get device_name once per device cycle (not per register)
  const char *deviceName = deviceConfig["device_name"] | ""; // BUG #31: const char* (zero allocation!)

  if (registers.size() == 0)
  {
    return;
  }

  // Check if device is enabled (failure state check)
  if (!isDeviceEnabled(deviceId))
  {
    // v2.5.9: Use throttled logging to prevent spam (log once per 30s per device)
    static LogThrottle disabledThrottle(30000);
    char contextMsg[64];
    snprintf(contextMsg, sizeof(contextMsg), "RTU Device %s disabled", deviceId);
    if (disabledThrottle.shouldLog(contextMsg))
    {
      LOG_RTU_INFO("Device %s is disabled, skipping read\n", deviceId);
    }
    return;
  }

  // Check if device should retry based on backoff timing
  DeviceFailureState *failureState = getDeviceFailureState(deviceId);
  if (failureState && failureState->retryCount > 0)
  {
    if (!shouldRetryDevice(deviceId))
    {
      static LogThrottle retryThrottle(30000); // Log every 30s to reduce spam
      char contextMsg[128];
      snprintf(contextMsg, sizeof(contextMsg), "RTU Device %s retry backoff", deviceId);
      if (retryThrottle.shouldLog(contextMsg))
      {
        LOG_RTU_DEBUG("Device %s retry backoff not elapsed, skipping\n", deviceId);
      }
      return;
    }
  }

  ModbusMaster *modbus = getModbusForBus(serialPort);
  if (!modbus)
  {
    return;
  }

  // Configure baudrate for this device (with caching to avoid unnecessary reconfig)
  configureBaudRate(serialPort, baudRate);

  // Set slave ID for this device
  LOG_RTU_VERBOSE("Polling device %s (Slave:%d Port:%d Baud:%d)\n", deviceId, slaveId, serialPort, baudRate);
  if (serialPort == 1)
  {
    modbus1->begin(slaveId, *serial1);
  }
  else if (serialPort == 2)
  {
    modbus2->begin(slaveId, *serial2);
  }

  // Track device attempt time (for retry interval gating)
  updateDeviceLastRead(deviceId);

  // Track register read results (for End-of-Batch Marker)
  bool anyRegisterSucceeded = false;
  uint8_t successRegisterCount = 0;
  uint8_t failedRegisterCount = 0;

  // COMPACT LOGGING: Buffer all output for atomic printing (prevent interruption by other tasks)
  PSRAMString outputBuffer = ""; // BUG #31: PSRAMString to avoid DRAM fragmentation
  PSRAMString compactLine = "";  // BUG #31: PSRAMString to avoid DRAM fragmentation
  int successCount = 0;
  int lineNumber = 1;

  // FIXED ISSUE #4: Move buffers outside register loop to reduce stack allocation
  // Prevents stack overflow with large register counts (50+ registers)
  // These buffers are reused across loop iterations instead of allocated per iteration
  char dataTypeBuf[64]; // For FC3/4 data type parsing (max 64 chars)
  uint16_t values[4];   // For FC3/4 multi-register reads (max 4 registers = 8 bytes)

  // Development mode: Collect polled data in JSON format for debugging (always compiled, runtime-checked)
  SpiRamJsonDocument polledDataDoc;
  JsonObject polledData;
  JsonArray polledRegisters;

  if (IS_DEV_MODE())
  {
    polledData = polledDataDoc.to<JsonObject>();
    polledData["device_id"] = deviceId;
    polledData["device_name"] = deviceName;
    polledData["protocol"] = "RTU";
    polledData["slave_id"] = slaveId;
    polledData["timestamp"] = millis();
    polledRegisters = polledData["registers"].to<JsonArray>();
  }

  for (JsonVariant regVar : registers)
  {
    if (!running)
      break;

    // v2.5.41: Check for config changes DURING register iteration
    // CRITICAL FIX: Without this, many registers × timeout = long delay before config refresh
    // With this check, config changes are detected within 1 register poll cycle
    if (configChangePending.load())
    {
      LOG_RTU_INFO("[RTU] Config change during register polling - aborting device read...\n");
      break; // Exit register loop immediately, let device loop handle refresh
    }

    JsonObject reg = regVar.as<JsonObject>();
    uint8_t functionCode = reg["function_code"] | 3;
    uint16_t address = reg["address"] | 0;
    const char *registerName = reg["register_name"] | "Unknown"; // BUG #31: const char* (zero allocation!)

    // CLEANUP: Removed per-register polling logic
    // All registers now use device-level polling interval (deviceRefreshRate)
    // Device-level timing is already checked in shouldPollDevice() before calling this function

    uint8_t result;
    bool registerSuccess = false;

    if (functionCode == 1)
    {
      result = modbus->readCoils(address, 1);
      if (result == modbus->ku8MBSuccess)
      {
        double value = (modbus->getResponseBuffer(0) & 0x01) ? 1.0 : 0.0;

        // CRITICAL FIX: Check storeRegisterValue() return to detect enqueue failures
        bool storeSuccess = storeRegisterValue(deviceId, reg, value, deviceName);

        // Track result for End-of-Batch Marker
        if (storeSuccess)
        {
          successRegisterCount++;
        }
        else
        {
          failedRegisterCount++; // Failure: queue full or memory exhausted
        }

        // FIXED ISSUE #3: Use helper function to eliminate duplication
        const char *unit = reg["unit"] | "";
        appendRegisterToLog(registerName, value, unit, deviceId, outputBuffer, compactLine, successCount, lineNumber);

        // Add to JSON debug output (runtime check)
        if (IS_DEV_MODE())
        {
          JsonObject regObj = polledRegisters.add<JsonObject>();
          regObj["name"] = registerName;
          regObj["address"] = address;
          regObj["function_code"] = functionCode;
          regObj["value"] = value;
          if (strlen(unit) > 0) regObj["unit"] = unit;
        }

        registerSuccess = true;
        anyRegisterSucceeded = true;
      }
      else
      {
        // v2.5.35: Use DEV_MODE check to prevent log leak in production
        DEV_SERIAL_PRINTF("%s: %s = ERROR\n", deviceId, registerName);
        failedRegisterCount++;
      }
    }
    else if (functionCode == 2)
    {
      result = modbus->readDiscreteInputs(address, 1);
      if (result == modbus->ku8MBSuccess)
      {
        double value = (modbus->getResponseBuffer(0) & 0x01) ? 1.0 : 0.0;

        // CRITICAL FIX: Check storeRegisterValue() return to detect enqueue failures
        bool storeSuccess = storeRegisterValue(deviceId, reg, value, deviceName);

        // Track result for End-of-Batch Marker
        if (storeSuccess)
        {
          successRegisterCount++;
        }
        else
        {
          failedRegisterCount++; // Failure: queue full or memory exhausted
        }

        // FIXED ISSUE #3: Use helper function to eliminate duplication
        const char *unit = reg["unit"] | "";
        appendRegisterToLog(registerName, value, unit, deviceId, outputBuffer, compactLine, successCount, lineNumber);

        // Add to JSON debug output (runtime check)
        if (IS_DEV_MODE())
        {
          JsonObject regObj = polledRegisters.add<JsonObject>();
          regObj["name"] = registerName;
          regObj["address"] = address;
          regObj["function_code"] = functionCode;
          regObj["value"] = value;
          if (strlen(unit) > 0) regObj["unit"] = unit;
        }

        registerSuccess = true;
        anyRegisterSucceeded = true;
      }
      else
      {
        // v2.5.35: Use DEV_MODE check to prevent log leak in production
        DEV_SERIAL_PRINTF("%s: %s = ERROR\n", deviceId, registerName);
        failedRegisterCount++;
      }
    }
    else if (functionCode == 3 || functionCode == 4)
    {
      // BUG #31: Use PSRAMString for data type parsing (was String)
      PSRAMString dataType = reg["data_type"] | "INT16";

      // FIXED ISSUE #4: Use buffer declared outside loop (line 370)
      // Manual uppercase conversion (PSRAMString doesn't have toUpperCase() yet)
      strncpy(dataTypeBuf, dataType.c_str(), sizeof(dataTypeBuf) - 1);
      dataTypeBuf[sizeof(dataTypeBuf) - 1] = '\0';
      for (int i = 0; dataTypeBuf[i]; i++)
      {
        dataTypeBuf[i] = toupper(dataTypeBuf[i]);
      }

      int registerCount = 1;
      const char *baseType = dataTypeBuf;
      const char *endianness_variant = "";

      char *underscorePos = strchr(dataTypeBuf, '_');
      if (underscorePos != NULL)
      {
        *underscorePos = '\0'; // Split string at underscore
        baseType = dataTypeBuf;
        endianness_variant = underscorePos + 1;
      }

      // Determine register count based on base type
      if (strcmp(baseType, "INT32") == 0 || strcmp(baseType, "UINT32") == 0 || strcmp(baseType, "FLOAT32") == 0)
      {
        registerCount = 2;
      }
      else if (strcmp(baseType, "INT64") == 0 || strcmp(baseType, "UINT64") == 0 || strcmp(baseType, "DOUBLE64") == 0)
      {
        registerCount = 4;
      }

      // FIXED Bug #8: Validate address range doesn't overflow uint16_t address space
      if (address > (65535 - registerCount + 1))
      {
        LOG_RTU_INFO("[RTU] ERROR: Address range overflow for %s (addr=%d, count=%d, max=%d)\n",
                      registerName, address, registerCount, address + registerCount - 1); // BUG #31: removed .c_str()
        failedRegisterCount++;
        continue; // Skip this register
      }

      // FIXED ISSUE #4: Use buffer declared outside loop (line 371) to reduce stack usage
      if (readMultipleRegisters(modbus, functionCode, address, registerCount, values))
      {
        double value = (registerCount == 1) ? ModbusUtils::processRegisterValue(reg, values[0]) : ModbusUtils::processMultiRegisterValue(reg, values, registerCount, baseType, endianness_variant);

        // CRITICAL FIX: Check storeRegisterValue() return to detect enqueue failures
        bool storeSuccess = storeRegisterValue(deviceId, reg, value, deviceName);

        // Track result for End-of-Batch Marker
        if (storeSuccess)
        {
          successRegisterCount++;
        }
        else
        {
          failedRegisterCount++; // Failure: queue full or memory exhausted
        }

        // FIXED ISSUE #3: Use helper function to eliminate duplication
        const char *unit = reg["unit"] | "";
        appendRegisterToLog(registerName, value, unit, deviceId, outputBuffer, compactLine, successCount, lineNumber);

        // Add to JSON debug output (runtime check)
        if (IS_DEV_MODE())
        {
          JsonObject regObj = polledRegisters.add<JsonObject>();
          regObj["name"] = registerName;
          regObj["address"] = address;
          regObj["function_code"] = functionCode;
          regObj["value"] = value;
          if (strlen(unit) > 0) regObj["unit"] = unit;
        }

        registerSuccess = true;
        anyRegisterSucceeded = true;
      }
      else
      {
        // v2.5.35: Use DEV_MODE check to prevent log leak in production
        DEV_SERIAL_PRINTF("%s: %s = ERROR\n", deviceId, registerName);
        failedRegisterCount++;
      }
    }

    // OPTIMIZED: Reduced delay from 100ms to 10ms to speed up batch processing
    // 50 registers * 10ms = 0.5s overhead (vs 5s overhead previously)
    // This prevents MQTT keep-alive timeout during long polling cycles
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  // COMPACT LOGGING: Add remaining items and print buffer atomically
  if (successCount > 0)
  {
    if (!compactLine.isEmpty())
    {
      // Remove trailing " | " (BUG #31: manual check instead of endsWith())
      size_t len = compactLine.length();
      const char *str = compactLine.c_str();

      char lineBuf[16];
      snprintf(lineBuf, sizeof(lineBuf), "  L%d: ", lineNumber);
      outputBuffer += lineBuf;

      // Check if ends with " | " and remove it
      if (len >= 3 && str[len - 3] == ' ' && str[len - 2] == '|' && str[len - 1] == ' ')
      {
        PSRAMString trimmed = compactLine.substring(0, len - 3);
        outputBuffer += trimmed;
      }
      else
      {
        outputBuffer += compactLine;
      }
      outputBuffer += "\n";
    }
    // Print all at once (atomic - prevents interruption by other tasks)
    // Use LOG_DATA_DEBUG so it's compiled out in production mode
    LOG_DATA_DEBUG("%s", outputBuffer.c_str());
  }

  // Handle device failure state based on read results
  if (anyRegisterSucceeded)
  {
    // At least one register succeeded - reset failure state
    resetDeviceFailureState(deviceId);
    LOG_RTU_INFO("Device %s: Read successful, failure state reset\n", deviceId);

    // Update timeout tracking
    DeviceReadTimeout *timeout = getDeviceTimeout(deviceId);
    if (timeout)
    {
      timeout->lastSuccessfulRead = millis();
      timeout->consecutiveTimeouts = 0;
    }
  }
  else if (registers.size() > 0)
  {
    // All registers failed - handle read failure
    LOG_RTU_ERROR("Device %s: All %d register reads failed\n", deviceId, failedRegisterCount);
    handleReadFailure(deviceId);
  }

  // END-OF-BATCH MARKER: Signal to MQTT that device batch is complete
  // This marker allows MQTT to know when to publish per-device data
  QueueManager *queueMgr = QueueManager::getInstance();
  if (queueMgr && registers.size() > 0)
  {
    SpiRamJsonDocument markerDoc;
    JsonObject marker = markerDoc.to<JsonObject>();
    marker["event"] = "BATCH_END";
    marker["device_id"] = deviceId;
    marker["success_count"] = successRegisterCount;
    marker["failed_count"] = failedRegisterCount;
    marker["timestamp"] = millis();

    if (queueMgr->enqueue(marker))
    {
      LOG_RTU_DEBUG("Batch marker enqueued for device %s (success=%d, failed=%d)\n",
                    deviceId, successRegisterCount, failedRegisterCount);
    }
    else
    {
      LOG_RTU_WARN("Failed to enqueue batch marker for device %s\n", deviceId);
    }
  }

  // Development mode: Print polled data as JSON (one-line) - runtime check
  if (IS_DEV_MODE() && successRegisterCount > 0)
  {
    polledData["success_count"] = successRegisterCount;
    polledData["failed_count"] = failedRegisterCount;

    Serial.print("\n[RTU] POLLED DATA: ");
    serializeJson(polledDataDoc, Serial);
    Serial.println("\n");
  }
}

// NOTE: processRegisterValue() moved to ModbusUtils class (shared with ModbusTcpService)
// See ModbusUtils.cpp for implementation

bool ModbusRtuService::storeRegisterValue(const char *deviceId, const JsonObject &reg, double value, const char *deviceName)
{
  QueueManager *queueMgr = QueueManager::getInstance();

  // FIXED Bug #12: Defensive null check for QueueManager
  if (!queueMgr)
  {
    LOG_RTU_INFO("[RTU] ERROR: QueueManager is null, cannot store register value");
    return false; // FIXED: Return false on failure
  }

  // Create data point in simplified format for MQTT
  JsonDocument dataDoc;
  JsonObject dataPoint = dataDoc.to<JsonObject>();

  // Apply calibration formula: final_value = (raw_value × scale) + offset
  float scale = reg["scale"] | 1.0;
  float offset = reg["offset"] | 0.0;
  double calibratedValue = (value * scale) + offset;

  RTCManager *rtc = RTCManager::getInstance();
  if (rtc)
  {
    DateTime now = rtc->getCurrentTime();
    dataPoint["time"] = now.unixtime();
  }
  dataPoint["name"] = reg["register_name"].as<String>();
  dataPoint["device_id"] = deviceId;

  // OPTIMIZED: Use pre-fetched device_name (passed from readRtuDeviceData)
  // This avoids expensive getAllDevicesWithRegisters() call per register
  // BUG #31: deviceName is const char*, not String, so check for null/empty differently
  if (deviceName && deviceName[0] != '\0')
  {
    dataPoint["device_name"] = deviceName;
  }

  dataPoint["address"] = reg["address"];                      // Register address (e.g., 4112) for BLE streaming
  dataPoint["value"] = calibratedValue;                       // Use calibrated value
  dataPoint["description"] = reg["description"] | "";         // Optional field from BLE config

  // v2.5.40: Convert "deg" to degree symbol (°) for proper unit display
  // User request: show "°C" instead of "degC" in output
  // This also handles legacy data that might have been stored with "deg" prefix
  String rawUnit = reg["unit"] | "";
  rawUnit.replace("deg", "\xC2\xB0");    // Convert "deg" to UTF-8 degree symbol (°)
  dataPoint["unit"] = rawUnit;           // Unit with proper degree symbol

  dataPoint["register_id"] = reg["register_id"].as<String>(); // Internal use for deduplication
  dataPoint["register_index"] = reg["register_index"] | 0;    // For customize mode topic mapping

  // CRITICAL FIX: Check enqueue() return value to detect data loss
  // If enqueue fails (queue full, memory exhausted, mutex timeout), return false
  bool enqueueSuccess = queueMgr->enqueue(dataPoint);

  if (!enqueueSuccess)
  {
    // CRITICAL: Log detailed error for debugging
    LOG_RTU_ERROR("Failed to enqueue register '%s' for device %s (queue full or memory exhausted)\n",
                  reg["register_name"].as<String>().c_str(), deviceId);

    // Trigger memory diagnostics to identify root cause
    MemoryRecovery::logMemoryStatus("ENQUEUE_FAILED_RTU");

    return false; // Return failure to caller
  }

  // Check if this device is being streamed (BUG #31: Optimized String usage)
  if (crudHandler && queueMgr)
  {
    String streamIdStr = crudHandler->getStreamDeviceId(); // CRUDHandler returns String
    if (!streamIdStr.isEmpty() && strcmp(streamIdStr.c_str(), deviceId) == 0)
    {
      // Verbose log suppressed - summary shown in [STREAM] logs
      queueMgr->enqueueStream(dataPoint);
    }
  }

  return true; // Success
}

bool ModbusRtuService::readMultipleRegisters(ModbusMaster *modbus, uint8_t functionCode, uint16_t address, int count, uint16_t *values)
{
  uint8_t result;
  if (functionCode == 3)
  {
    result = modbus->readHoldingRegisters(address, count);
  }
  else // Read Input Registers (0x04)
  {
    result = modbus->readInputRegisters(address, count);
  }

  if (result == modbus->ku8MBSuccess)
  {
    for (int i = 0; i < count; i++)
    {
      values[i] = modbus->getResponseBuffer(i);
    }
    return true;
  }
  return false;
}

// NOTE: processMultiRegisterValue() moved to ModbusUtils class (shared with ModbusTcpService)
// See ModbusUtils.cpp for implementation

ModbusMaster *ModbusRtuService::getModbusForBus(int serialPort)
{
  if (serialPort == 1)
  {
    return modbus1;
  }
  else if (serialPort == 2)
  {
    return modbus2;
  }
  return nullptr;
}

// FIXED ISSUE #3: Consolidated register logging helper (eliminates 80+ lines of duplication)
// Previously duplicated across FC1, FC2, and FC3/4 handlers
void ModbusRtuService::appendRegisterToLog(const char *registerName, double value, const char *unit,
                                           const char *deviceId, PSRAMString &outputBuffer,
                                           PSRAMString &compactLine, int &successCount, int &lineNumber)
{
  // Process unit buffer (handle degree symbol)
  char unitBuf[32];
  strncpy(unitBuf, unit, sizeof(unitBuf) - 1);
  unitBuf[sizeof(unitBuf) - 1] = '\0';

  // Replace degree symbol with "deg"
  char *degPos = strstr(unitBuf, "°");
  if (degPos)
  {
    size_t remaining = strlen(degPos + 2);
    memmove(degPos + 3, degPos + 2, remaining + 1);
    memcpy(degPos, "deg", 3);
  }

  // Add header on first success (sequential appends to avoid temp objects)
  if (successCount == 0)
  {
    outputBuffer += "[DATA] ";

    // Add timestamp from RTC
    RTCManager *rtc = RTCManager::getInstance();
    if (rtc)
    {
      DateTime now = rtc->getCurrentTime();
      char timeBuf[12];
      snprintf(timeBuf, sizeof(timeBuf), "[%02d:%02d:%02d] ", now.hour(), now.minute(), now.second());
      outputBuffer += timeBuf;
    }

    outputBuffer += deviceId;
    outputBuffer += ":\n";
  }

  // Build compact line with individual appends to avoid temp String objects
  char valueBuf[16];
  snprintf(valueBuf, sizeof(valueBuf), "%.1f", value);
  compactLine += registerName;
  compactLine += ":";
  compactLine += valueBuf;
  compactLine += unitBuf;

  successCount++;
  if (successCount % 6 == 0)
  {
    char lineBuf[16];
    snprintf(lineBuf, sizeof(lineBuf), "  L%d: ", lineNumber++);
    outputBuffer += lineBuf;
    outputBuffer += compactLine;
    outputBuffer += "\n";
    compactLine.clear();
  }
  else
  {
    compactLine += " | ";
  }
}

void ModbusRtuService::getStatus(JsonObject &status)
{
  status["running"] = running;
  status["service_type"] = "modbus_rtu";

  status["rtu_device_count"] = rtuDevices.size(); // Use cached list size
}

// NEW: Modbus Improvement Phase - Helper Method Implementations

void ModbusRtuService::initializeDeviceFailureTracking()
{
  deviceFailureStates.clear();

  for (const auto &device : rtuDevices)
  {
    DeviceFailureState state;
    state.deviceId = device.deviceId;
    state.consecutiveFailures = 0;
    state.retryCount = 0;
    state.nextRetryTime = 0;
    state.lastReadAttempt = 0;
    state.isEnabled = true;

    // Read baudrate from device config
    if (device.doc)
    {
      JsonObject deviceObj = device.doc->as<JsonObject>();
      state.baudRate = deviceObj["baud_rate"] | 9600;
    }
    else
    {
      state.baudRate = 9600; // Fallback default
    }

    state.maxRetries = 5;
    deviceFailureStates.push_back(state);
  }
  // Concise summary log
  LOG_RTU_INFO("[RTU] Failure tracking init: %d devices\n", deviceFailureStates.size());
}

void ModbusRtuService::initializeDeviceTimeouts()
{
  deviceTimeouts.clear();

  for (const auto &device : rtuDevices)
  {
    DeviceReadTimeout timeout;
    timeout.deviceId = device.deviceId;
    timeout.timeoutMs = 5000; // Default 5 second timeout
    timeout.consecutiveTimeouts = 0;
    timeout.lastSuccessfulRead = millis();
    timeout.maxConsecutiveTimeouts = 3;
    deviceTimeouts.push_back(timeout);
  }
  // Concise summary log
  LOG_RTU_INFO("[RTU] Timeout tracking init: %d devices\n", deviceTimeouts.size());
}

ModbusRtuService::DeviceFailureState *ModbusRtuService::getDeviceFailureState(const char *deviceId)
{
  for (auto &state : deviceFailureStates)
  {
    if (state.deviceId == deviceId)
    {
      return &state;
    }
  }
  return nullptr;
}

ModbusRtuService::DeviceReadTimeout *ModbusRtuService::getDeviceTimeout(const char *deviceId)
{
  for (auto &timeout : deviceTimeouts)
  {
    if (timeout.deviceId == deviceId)
    {
      return &timeout;
    }
  }
  return nullptr;
}

// NEW: Enhancement - Device Health Metrics Tracking
void ModbusRtuService::initializeDeviceMetrics()
{
  deviceMetrics.clear();

  for (const auto &device : rtuDevices)
  {
    DeviceHealthMetrics metrics;
    metrics.deviceId = device.deviceId;
    metrics.totalReads = 0;
    metrics.successfulReads = 0;
    metrics.failedReads = 0;
    metrics.totalResponseTimeMs = 0;
    metrics.minResponseTimeMs = 65535;
    metrics.maxResponseTimeMs = 0;
    metrics.lastResponseTimeMs = 0;
    deviceMetrics.push_back(metrics);
  }
  // Concise summary log
  LOG_RTU_INFO("[RTU] Metrics tracking init: %d devices\n", deviceMetrics.size());
}

ModbusRtuService::DeviceHealthMetrics *ModbusRtuService::getDeviceMetrics(const char *deviceId)
{
  for (auto &metrics : deviceMetrics)
  {
    if (metrics.deviceId == deviceId)
    {
      return &metrics;
    }
  }
  return nullptr;
}

bool ModbusRtuService::configureDeviceBaudRate(const char *deviceId, uint16_t baudRate)
{
  DeviceFailureState *state = getDeviceFailureState(deviceId);
  if (!state)
  {
    LOG_RTU_INFO("[RTU] ERROR: Device %s not found for baud rate configuration\n", deviceId);
    return false;
  }

  state->baudRate = baudRate;
  LOG_RTU_INFO("[RTU] Configured device %s baud rate to: %d\n", deviceId, baudRate);
  return true;
}

uint16_t ModbusRtuService::getDeviceBaudRate(const char *deviceId)
{
  DeviceFailureState *state = getDeviceFailureState(deviceId);
  if (!state)
  {
    return 9600; // Default baud rate
  }
  return state->baudRate;
}

// Validate baudrate (only allow standard Modbus RTU baudrates)
bool ModbusRtuService::validateBaudRate(uint32_t baudRate)
{
  // Standard Modbus RTU baudrates
  switch (baudRate)
  {
  case 1200:
  case 2400:
  case 4800:
  case 9600:
  case 19200:
  case 38400:
  case 57600:
  case 115200: // Not recommended for long cables, but supported
    return true;
  default:
    LOG_RTU_INFO("[RTU] ERROR: Invalid baudrate %d (only 1200-115200 allowed)\n", baudRate);
    return false;
  }
}

// Configure baudrate for a specific serial port (with caching)
void ModbusRtuService::configureBaudRate(int serialPort, uint32_t baudRate)
{
  // Validate baudrate first
  if (!validateBaudRate(baudRate))
  {
    LOG_RTU_INFO("[RTU] Using default baudrate 9600 instead\n");
    baudRate = 9600;
  }

  if (serialPort == 1)
  {
    // Check if baudrate actually changed (avoid unnecessary reconfiguration)
    if (currentBaudRate1 != baudRate)
    {
      LOG_RTU_INFO("[RTU] Reconfiguring Serial1 from %d to %d baud\n", currentBaudRate1, baudRate);
      serial1->end();
      serial1->begin(baudRate, SERIAL_8N1, RTU_RX1, RTU_TX1);
      serial1->setTimeout(200); // FIXED: Reset timeout after baudrate change
      currentBaudRate1 = baudRate;

      // Delay to stabilize serial communication
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }
  else if (serialPort == 2)
  {
    // Check if baudrate actually changed
    if (currentBaudRate2 != baudRate)
    {
      LOG_RTU_INFO("[RTU] Reconfiguring Serial2 from %d to %d baud\n", currentBaudRate2, baudRate);
      serial2->end();
      serial2->begin(baudRate, SERIAL_8N1, RTU_RX2, RTU_TX2);
      serial2->setTimeout(200); // FIXED: Reset timeout after baudrate change
      currentBaudRate2 = baudRate;

      // Delay to stabilize serial communication
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }
  else
  {
    LOG_RTU_INFO("[RTU] ERROR: Invalid serial port %d\n", serialPort);
  }
}

void ModbusRtuService::handleReadFailure(const char *deviceId)
{
  DeviceFailureState *state = getDeviceFailureState(deviceId);
  if (!state)
    return;

  state->consecutiveFailures++;
  state->lastReadAttempt = millis();

  if (state->retryCount < state->maxRetries)
  {
    state->retryCount++;
    unsigned long backoffTime = calculateBackoffTime(state->retryCount);
    state->nextRetryTime = millis() + backoffTime;

    LOG_RTU_INFO("[RTU] Device %s read failed. Retry %d/%d in %lums\n",
                  deviceId, state->retryCount, state->maxRetries, backoffTime);
  }
  else
  {
    // Max retries exceeded
    LOG_RTU_INFO("[RTU] Device %s exceeded max retries (%d), disabling...\n",
                  deviceId, state->maxRetries);
    disableDevice(deviceId, DeviceFailureState::AUTO_RETRY, "Max retries exceeded");
  }
}

bool ModbusRtuService::shouldRetryDevice(const char *deviceId)
{
  DeviceFailureState *state = getDeviceFailureState(deviceId);
  if (!state || !state->isEnabled)
    return false;
  if (state->retryCount == 0)
    return false;

  return millis() >= state->nextRetryTime;
}

unsigned long ModbusRtuService::calculateBackoffTime(uint8_t retryCount)
{
  // ENHANCED: Exponential backoff with jitter
  //  Base calculation: 100ms * 2^(retryCount-1)
  //  Retry 1: 100ms, Retry 2: 200ms, Retry 3: 400ms, Retry 4: 800ms, Retry 5+: 1600ms

  if (retryCount == 0)
    return 0;

  // Calculate base exponential backoff: 100ms * 2^(retryCount-1)
  unsigned long baseBackoff = 100 * (1 << (retryCount - 1));

  // Cap at maximum (1600ms for retry 5+)
  if (baseBackoff > 1600)
  {
    baseBackoff = 1600;
  }

  // Add random jitter (0-50% of base backoff) to prevent synchronized retries
  uint32_t jitter = (esp_random() % (baseBackoff / 2));
  unsigned long totalBackoff = baseBackoff + jitter;

  LOG_RTU_INFO("[RTU] Exponential backoff: retry %d to %lums base + %lums jitter = %lums total\n",
                retryCount, baseBackoff, jitter, totalBackoff);

  return totalBackoff;
}

void ModbusRtuService::resetDeviceFailureState(const char *deviceId)
{
  DeviceFailureState *state = getDeviceFailureState(deviceId);
  if (!state)
    return;

  state->consecutiveFailures = 0;
  state->retryCount = 0;
  state->nextRetryTime = 0;
  state->lastReadAttempt = millis();

  LOG_RTU_INFO("[RTU] Reset failure state for device %s\n", deviceId);
}

void ModbusRtuService::handleReadTimeout(const char *deviceId)
{
  DeviceReadTimeout *timeout = getDeviceTimeout(deviceId);
  if (!timeout)
    return;

  timeout->consecutiveTimeouts++;

  if (timeout->consecutiveTimeouts >= timeout->maxConsecutiveTimeouts)
  {
    LOG_RTU_INFO("[RTU] Device %s exceeded timeout limit (%d), disabling...\n",
                  deviceId, timeout->maxConsecutiveTimeouts);
    disableDevice(deviceId, DeviceFailureState::AUTO_TIMEOUT, "Max consecutive timeouts exceeded");
  }
  else
  {
    LOG_RTU_INFO("[RTU] Device %s timeout %d/%d\n",
                  deviceId, timeout->consecutiveTimeouts, timeout->maxConsecutiveTimeouts);
  }
}

bool ModbusRtuService::isDeviceEnabled(const char *deviceId)
{
  DeviceFailureState *state = getDeviceFailureState(deviceId);
  if (!state)
    return true; // Default to enabled if not found
  return state->isEnabled;
}

void ModbusRtuService::enableDevice(const char *deviceId, bool clearMetrics)
{
  DeviceFailureState *state = getDeviceFailureState(deviceId);
  if (!state)
    return;

  state->isEnabled = true;
  state->disableReason = DeviceFailureState::NONE; // Clear disable reason
  state->disableReasonDetail = "";
  state->disabledTimestamp = 0;
  resetDeviceFailureState(deviceId);

  DeviceReadTimeout *timeout = getDeviceTimeout(deviceId);
  if (timeout)
  {
    timeout->consecutiveTimeouts = 0;
    timeout->lastSuccessfulRead = millis();
  }

  // Optionally clear health metrics
  if (clearMetrics)
  {
    DeviceHealthMetrics *metrics = getDeviceMetrics(deviceId);
    if (metrics)
    {
      metrics->totalReads = 0;
      metrics->successfulReads = 0;
      metrics->failedReads = 0;
      metrics->totalResponseTimeMs = 0;
      metrics->minResponseTimeMs = 65535;
      metrics->maxResponseTimeMs = 0;
      metrics->lastResponseTimeMs = 0;
      LOG_RTU_INFO("[RTU] Device %s metrics cleared\n", deviceId);
    }
  }

  LOG_RTU_INFO("[RTU] Device %s enabled (reason cleared)\n", deviceId);
}

void ModbusRtuService::disableDevice(const char *deviceId, DeviceFailureState::DisableReason reason, const char *reasonDetail)
{
  DeviceFailureState *state = getDeviceFailureState(deviceId);
  if (!state)
    return;

  state->isEnabled = false;
  state->disableReason = reason;
  state->disableReasonDetail = reasonDetail;
  state->disabledTimestamp = millis();

  const char *reasonText = "";
  switch (reason)
  {
  case DeviceFailureState::MANUAL:
    reasonText = "MANUAL";
    break;
  case DeviceFailureState::AUTO_RETRY:
    reasonText = "AUTO_RETRY";
    break;
  case DeviceFailureState::AUTO_TIMEOUT:
    reasonText = "AUTO_TIMEOUT";
    break;
  default:
    reasonText = "UNKNOWN";
    break;
  }

  LOG_RTU_INFO("[RTU] Device %s disabled (reason: %s, detail: %s)\n",
                deviceId, reasonText, reasonDetail);
}

// NEW: Polling Hierarchy Implementation
// ============================================
// CLEANUP: Removed Level 1 per-register polling methods (getRegisterTimer, shouldPollRegister, updateRegisterLastRead)
// All registers now use device-level polling interval

// Level 1: Device-level timing methods
ModbusRtuService::DeviceTimer *ModbusRtuService::getDeviceTimer(const char *deviceId)
{
  for (auto &timer : deviceTimers)
  {
    if (timer.deviceId == deviceId)
    {
      return &timer;
    }
  }
  return nullptr;
}

bool ModbusRtuService::shouldPollDevice(const char *deviceId, uint32_t refreshRateMs)
{
  DeviceTimer *timer = getDeviceTimer(deviceId);
  if (!timer)
  {
    // First time polling this device, create entry
    DeviceTimer newTimer;
    newTimer.deviceId = deviceId;
    newTimer.lastRead = 0;
    newTimer.refreshRateMs = refreshRateMs;
    deviceTimers.push_back(newTimer);
    return true; // Always poll on first encounter
  }

  unsigned long now = millis();
  return (now - timer->lastRead) >= timer->refreshRateMs;
}

void ModbusRtuService::updateDeviceLastRead(const char *deviceId)
{
  DeviceTimer *timer = getDeviceTimer(deviceId);
  if (timer)
  {
    timer->lastRead = millis();
  }
}

// Level 3: Server data transmission interval methods
bool ModbusRtuService::shouldTransmitData(uint32_t dataIntervalMs)
{
  unsigned long now = millis();
  return (now - dataTransmissionSchedule.lastTransmitted) >= dataIntervalMs;
}

void ModbusRtuService::updateDataTransmissionTime()
{
  dataTransmissionSchedule.lastTransmitted = millis();
}

void ModbusRtuService::setDataTransmissionInterval(uint32_t intervalMs)
{
  dataTransmissionSchedule.dataIntervalMs = intervalMs;
  LOG_RTU_INFO("[RTU] Data transmission interval set to: %lums\n", intervalMs);
}

// ============================================
// NEW: Enhancement - Public API for BLE Device Control Commands
// ============================================

bool ModbusRtuService::enableDeviceByCommand(const char *deviceId, bool clearMetrics)
{
  LOG_RTU_INFO("[RTU] BLE Command: Enable device %s (clearMetrics: %s)\n",
                deviceId, clearMetrics ? "true" : "false");

  DeviceFailureState *state = getDeviceFailureState(deviceId);
  if (!state)
  {
    LOG_RTU_INFO("[RTU] ERROR: Device %s not found\n", deviceId);
    return false;
  }

  enableDevice(deviceId, clearMetrics);
  return true;
}

bool ModbusRtuService::disableDeviceByCommand(const char *deviceId, const char *reasonDetail)
{
  LOG_RTU_INFO("[RTU] BLE Command: Disable device %s (reason: %s)\n",
                deviceId, reasonDetail);

  DeviceFailureState *state = getDeviceFailureState(deviceId);
  if (!state)
  {
    LOG_RTU_INFO("[RTU] ERROR: Device %s not found\n", deviceId);
    return false;
  }

  disableDevice(deviceId, DeviceFailureState::MANUAL, reasonDetail);
  return true;
}

bool ModbusRtuService::getDeviceStatusInfo(const char *deviceId, JsonObject &statusInfo)
{
  DeviceFailureState *state = getDeviceFailureState(deviceId);
  if (!state)
  {
    LOG_RTU_INFO("[RTU] ERROR: Device %s not found\n", deviceId);
    return false;
  }

  DeviceReadTimeout *timeout = getDeviceTimeout(deviceId);
  DeviceHealthMetrics *metrics = getDeviceMetrics(deviceId);

  // Basic status
  statusInfo["device_id"] = deviceId;
  statusInfo["enabled"] = state->isEnabled;
  statusInfo["consecutive_failures"] = state->consecutiveFailures;
  statusInfo["retry_count"] = state->retryCount;

  // Disable reason
  const char *reasonText = "";
  switch (state->disableReason)
  {
  case DeviceFailureState::NONE:
    reasonText = "NONE";
    break;
  case DeviceFailureState::MANUAL:
    reasonText = "MANUAL";
    break;
  case DeviceFailureState::AUTO_RETRY:
    reasonText = "AUTO_RETRY";
    break;
  case DeviceFailureState::AUTO_TIMEOUT:
    reasonText = "AUTO_TIMEOUT";
    break;
  }
  statusInfo["disable_reason"] = reasonText;
  statusInfo["disable_reason_detail"] = state->disableReasonDetail.c_str();

  if (state->disabledTimestamp > 0)
  {
    unsigned long disabledDuration = millis() - state->disabledTimestamp;
    statusInfo["disabled_duration_ms"] = disabledDuration;
  }

  // Timeout info
  if (timeout)
  {
    statusInfo["timeout_ms"] = timeout->timeoutMs;
    statusInfo["consecutive_timeouts"] = timeout->consecutiveTimeouts;
    statusInfo["max_consecutive_timeouts"] = timeout->maxConsecutiveTimeouts;
  }

  // Health metrics
  if (metrics)
  {
    JsonObject metricsObj = statusInfo["metrics"].to<JsonObject>();
    metricsObj["total_reads"] = metrics->totalReads;
    metricsObj["successful_reads"] = metrics->successfulReads;
    metricsObj["failed_reads"] = metrics->failedReads;
    metricsObj["success_rate"] = metrics->getSuccessRate();
    metricsObj["avg_response_time_ms"] = metrics->getAvgResponseTimeMs();
    metricsObj["min_response_time_ms"] = metrics->minResponseTimeMs;
    metricsObj["max_response_time_ms"] = metrics->maxResponseTimeMs;
    metricsObj["last_response_time_ms"] = metrics->lastResponseTimeMs;
  }

  return true;
}

bool ModbusRtuService::getAllDevicesStatus(JsonObject &allStatus)
{
  JsonArray devicesArray = allStatus["devices"].to<JsonArray>();

  for (const auto &device : rtuDevices)
  {
    JsonObject deviceStatus = devicesArray.add<JsonObject>();
    getDeviceStatusInfo(device.deviceId.c_str(), deviceStatus);
  }

  allStatus["total_devices"] = rtuDevices.size();
  return true;
}

// ============================================
// NEW: Enhancement - Auto-Recovery Task
// ============================================

void ModbusRtuService::autoRecoveryTask(void *parameter)
{
  ModbusRtuService *service = static_cast<ModbusRtuService *>(parameter);
  service->autoRecoveryLoop();
}

void ModbusRtuService::autoRecoveryLoop()
{
  LOG_RTU_INFO("[RTU AutoRecovery] Task started");
  // v1.0.5: Reduced from 5 minutes to 30 seconds for faster recovery
  // - RTU retry cycle takes ~3.1s, so 30s provides good balance
  // - Total worst-case recovery: 3.1s + 30s = ~33 seconds
  // - Safe for RTOS: lightweight task, no memory allocation
  const unsigned long RECOVERY_INTERVAL_MS = 30000; // 30 seconds

  while (running)
  {
    vTaskDelay(pdMS_TO_TICKS(RECOVERY_INTERVAL_MS));

    if (!running)
      break;

    LOG_RTU_INFO("[RTU AutoRecovery] Checking for auto-disabled devices...");
    unsigned long now = millis();

    // FIXED ISSUE #1: Protect vector access with mutex
    xSemaphoreTakeRecursive(vectorMutex, portMAX_DELAY);

    for (auto &state : deviceFailureStates)
    {
      if (!state.isEnabled &&
          (state.disableReason == DeviceFailureState::AUTO_RETRY ||
           state.disableReason == DeviceFailureState::AUTO_TIMEOUT))
      {
        unsigned long disabledDuration = now - state.disabledTimestamp;
        LOG_RTU_INFO("[RTU AutoRecovery] Device %s auto-disabled for %lu ms, attempting recovery...\n",
                      state.deviceId.c_str(), disabledDuration);

        enableDevice(state.deviceId.c_str(), false); // Don't clear metrics
        LOG_RTU_INFO("[RTU AutoRecovery] Device %s re-enabled\n", state.deviceId.c_str());
      }
    }

    xSemaphoreGiveRecursive(vectorMutex);
  }

  // CRITICAL FIX: Task must self-delete when loop exits to prevent FreeRTOS abort
  LOG_RTU_INFO("[RTU AutoRecovery] Task loop exited, self-deleting...");
  autoRecoveryTaskHandle = nullptr; // Clear handle before deletion
  vTaskDelete(NULL); // Delete self (NULL = current task)
}

// v2.5.35: Get aggregated Modbus stats for ProductionLogger
void ModbusRtuService::getAggregatedStats(uint32_t &totalSuccess, uint32_t &totalFailed)
{
  totalSuccess = 0;
  totalFailed = 0;

  if (xSemaphoreTake(vectorMutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    for (const auto &metrics : deviceMetrics)
    {
      totalSuccess += metrics.successfulReads;
      totalFailed += metrics.failedReads;
    }
    xSemaphoreGive(vectorMutex);
  }
}

ModbusRtuService::~ModbusRtuService()
{
  stop();

  // FIXED Bug #7: Delete in REVERSE order to prevent use-after-free
  // ModbusMaster objects hold references to HardwareSerial objects
  // Must delete modbus objects BEFORE deleting serial objects

  if (modbus1)
  {
    delete modbus1;
    modbus1 = nullptr;
  }
  if (modbus2)
  {
    delete modbus2;
    modbus2 = nullptr;
  }
  if (serial1)
  {
    delete serial1;
    serial1 = nullptr;
  }
  if (serial2)
  {
    delete serial2;
    serial2 = nullptr;
  }

  // FIXED: Delete vectorMutex to prevent memory leak
  if (vectorMutex)
  {
    vSemaphoreDelete(vectorMutex);
    vectorMutex = nullptr;
  }

  LOG_RTU_INFO("[RTU] Service destroyed, resources cleaned up");
}
