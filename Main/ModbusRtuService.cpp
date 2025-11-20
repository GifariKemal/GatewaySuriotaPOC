#include "ModbusRtuService.h"
#include "QueueManager.h"
#include "CRUDHandler.h"
#include "RTCManager.h"
#include "DeviceBatchManager.h"
#include "DebugConfig.h"
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
  Serial.println("Initializing Modbus RTU service with ModbusMaster library...");

  if (!configManager)
  {
    Serial.println("ConfigManager is null");
    return false;
  }

  // Initialize Serial1 for Bus 1 (default 9600)
  serial1 = new HardwareSerial(1);
  // FIXED Bug #12: Check allocation success
  if (!serial1)
  {
    Serial.println("[RTU] ERROR: Failed to allocate Serial1");
    return false;
  }
  serial1->begin(9600, SERIAL_8N1, RTU_RX1, RTU_TX1);
  currentBaudRate1 = 9600;

  // Initialize Serial2 for Bus 2 (default 9600)
  serial2 = new HardwareSerial(2);
  // FIXED Bug #12: Check allocation success
  if (!serial2)
  {
    Serial.println("[RTU] ERROR: Failed to allocate Serial2");
    return false;
  }
  serial2->begin(9600, SERIAL_8N1, RTU_RX2, RTU_TX2);
  currentBaudRate2 = 9600;

  // Initialize ModbusMaster instances
  modbus1 = new ModbusMaster();
  // FIXED Bug #12: Check allocation success
  if (!modbus1)
  {
    Serial.println("[RTU] ERROR: Failed to allocate ModbusMaster1");
    return false;
  }
  modbus1->begin(1, *serial1);

  modbus2 = new ModbusMaster();
  // FIXED Bug #12: Check allocation success
  if (!modbus2)
  {
    Serial.println("[RTU] ERROR: Failed to allocate ModbusMaster2");
    return false;
  }
  modbus2->begin(1, *serial2);

  Serial.println("Modbus RTU service initialized successfully");
  return true;
}

void ModbusRtuService::start()
{
  Serial.println("Starting Modbus RTU service...");

  if (running)
  {
    return;
  }

  running = true;
  BaseType_t result = xTaskCreatePinnedToCore(
      readRtuDevicesTask,
      "MODBUS_RTU_TASK",
      8192,
      this,
      2,
      &rtuTaskHandle, // Store the task handle
      1);

  if (result == pdPASS)
  {
    Serial.println("Modbus RTU service started successfully");
  }
  else
  {
    Serial.println("Failed to create Modbus RTU task");
    running = false;
    rtuTaskHandle = nullptr;
  }
}

void ModbusRtuService::stop()
{
  running = false;
  if (rtuTaskHandle)
  {
    vTaskDelay(pdMS_TO_TICKS(100));
    vTaskDelete(rtuTaskHandle);
    rtuTaskHandle = nullptr;
  }
  Serial.println("Modbus RTU service stopped");
}

void ModbusRtuService::notifyConfigChange()
{
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
  Serial.println("[RTU Task] Refreshing device list and schedule...");
  rtuDevices.clear(); // FIXED Bug #2: Now safe - unique_ptr auto-deletes old documents

  // Clear the priority queue
  std::priority_queue<PollingTask, std::vector<PollingTask>, std::greater<PollingTask>> emptyQueue;
  pollingQueue.swap(emptyQueue);

  JsonDocument devicesIdList;
  JsonArray deviceIds = devicesIdList.to<JsonArray>();
  configManager->listDevices(deviceIds);

  unsigned long now = millis();

  for (JsonVariant deviceIdVar : deviceIds)
  {
    const char* deviceId = deviceIdVar.as<const char*>();  // BUG #31: const char* (zero allocation!)
    if (!deviceId || strcmp(deviceId, "") == 0 || strcmp(deviceId, "{}") == 0 || strlen(deviceId) < 3)
    {
      continue;
    }

    JsonDocument tempDeviceDoc;
    JsonObject deviceObj = tempDeviceDoc.to<JsonObject>();
    if (configManager->readDevice(deviceId, deviceObj))
    {
      const char* protocol = deviceObj["protocol"] | "";  // BUG #31: const char* (zero allocation!)
      if (strcmp(protocol, "RTU") == 0)
      {
        RtuDeviceConfig newDeviceEntry;
        newDeviceEntry.deviceId = deviceId;
        newDeviceEntry.doc = std::make_unique<JsonDocument>(); // FIXED Bug #2: Use smart pointer
        newDeviceEntry.doc->set(deviceObj);
        rtuDevices.push_back(std::move(newDeviceEntry));

        // Add device to the polling schedule for an immediate first poll
        pollingQueue.push({deviceId, now});
      }
    }
  }
  Serial.printf("[RTU Task] Found %d RTU devices. Schedule rebuilt.\n", rtuDevices.size());
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

    // Check for configuration change notifications (non-blocking)
    if (ulTaskNotifyTake(pdTRUE, 0) > 0)
    {
      refreshDeviceList();
      Serial.println("[RTU] Configuration changed, device list refreshed");
    }

    JsonDocument devicesDoc;
    JsonArray devices = devicesDoc.to<JsonArray>();
    configManager->listDevices(devices);

    unsigned long currentTime = millis();

    for (JsonVariant deviceVar : devices)
    {
      if (!running)
        break;

      const char* deviceId = deviceVar.as<const char*>();  // BUG #31: const char* (zero allocation!)

      if (!deviceId || strcmp(deviceId, "") == 0 || strcmp(deviceId, "{}") == 0 || strlen(deviceId) < 3)
      {
        continue;
      }

      JsonDocument deviceDoc;
      JsonObject deviceObj = deviceDoc.to<JsonObject>();

      if (configManager->readDevice(deviceId, deviceObj))
      {
        const char* protocol = deviceObj["protocol"] | "";  // BUG #31: const char* (zero allocation!)

        if (strcmp(protocol, "RTU") == 0)
        {
          // Level 2: Get device refresh rate (minimum retry interval)
          uint32_t deviceRefreshRate = deviceObj["refresh_rate_ms"] | 5000;

          // Level 2: Check if device minimum retry interval has elapsed
          if (shouldPollDevice(deviceId, deviceRefreshRate))
          {
            readRtuDeviceData(deviceObj);
            // Device-level timing is updated inside readRtuDeviceData via updateDeviceLastRead()
          }
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

// ... rest of the functions (readRtuDeviceData, processRegisterValue, etc.) remain the same ...

void ModbusRtuService::readRtuDeviceData(const JsonObject &deviceConfig)
{
  const char* deviceId = deviceConfig["device_id"] | "UNKNOWN";  // BUG #31: const char* (zero allocation!)
  int serialPort = deviceConfig["serial_port"] | 1;
  uint8_t slaveId = deviceConfig["slave_id"] | 1;
  uint32_t deviceRefreshRate = deviceConfig["refresh_rate_ms"] | 5000; // Device-level refresh rate
  uint32_t baudRate = deviceConfig["baud_rate"] | 9600;                // Get device baudrate from config
  JsonArray registers = deviceConfig["registers"];

  // OPTIMIZED: Get device_name once per device cycle (not per register)
  const char* deviceName = deviceConfig["device_name"] | "";  // BUG #31: const char* (zero allocation!)

  if (registers.size() == 0)
  {
    return;
  }

  // Check if device is enabled (failure state check)
  if (!isDeviceEnabled(deviceId))
  {
    LOG_RTU_INFO("Device %s is disabled, skipping read\n", deviceId);
    return;
  }

  // Check if device should retry based on backoff timing
  DeviceFailureState *failureState = getDeviceFailureState(deviceId);
  if (failureState && failureState->retryCount > 0)
  {
    if (!shouldRetryDevice(deviceId))
    {
      static LogThrottle retryThrottle(30000); // Log every 30s to reduce spam
      if (retryThrottle.shouldLog()) {
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

  // CRITICAL FIX: Start batch tracking BEFORE reading registers
  DeviceBatchManager *batchMgr = DeviceBatchManager::getInstance();
  if (batchMgr)
  {
    batchMgr->startBatch(deviceId, registers.size());
  }

  // Track if any registers succeeded (for failure state reset)
  bool anyRegisterSucceeded = false;
  uint8_t failedRegisterCount = 0;

  // COMPACT LOGGING: Buffer all output for atomic printing (prevent interruption by other tasks)
  PSRAMString outputBuffer = "";  // BUG #31: PSRAMString to avoid DRAM fragmentation
  PSRAMString compactLine = "";   // BUG #31: PSRAMString to avoid DRAM fragmentation
  int successCount = 0;
  int lineNumber = 1;

  for (JsonVariant regVar : registers)
  {
    if (!running)
      break;

    JsonObject reg = regVar.as<JsonObject>();
    uint8_t functionCode = reg["function_code"] | 3;
    uint16_t address = reg["address"] | 0;
    const char* registerName = reg["register_name"] | "Unknown";  // BUG #31: const char* (zero allocation!)

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
        storeRegisterValue(deviceId, reg, value, deviceName);

        // CRITICAL FIX: Track successful register read
        if (batchMgr)
        {
          batchMgr->incrementEnqueued(deviceId);
        }

        // COMPACT LOGGING: Collect reading to buffer (BUG #31: Optimized to avoid temp String objects)
        const char* unit = reg["unit"] | "";
        char unitBuf[32];
        strncpy(unitBuf, unit, sizeof(unitBuf) - 1);
        unitBuf[sizeof(unitBuf) - 1] = '\0';
        // Replace degree symbol with "deg"
        char* degPos = strstr(unitBuf, "°");
        if (degPos) {
          size_t remaining = strlen(degPos + 2);
          memmove(degPos + 3, degPos + 2, remaining + 1);
          memcpy(degPos, "deg", 3);
        }

        // Add header on first success (sequential appends to avoid temp objects)
        if (successCount == 0) {
          outputBuffer += "[DATA] ";
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
        if (successCount % 6 == 0) {
          char lineBuf[16];
          snprintf(lineBuf, sizeof(lineBuf), "  L%d: ", lineNumber++);
          outputBuffer += lineBuf;
          outputBuffer += compactLine;
          outputBuffer += "\n";
          compactLine.clear();
        } else {
          compactLine += " | ";
        }

        registerSuccess = true;
        anyRegisterSucceeded = true;
      }
      else
      {
        Serial.printf("%s: %s = ERROR\n", deviceId, registerName);
        failedRegisterCount++;

        // CRITICAL FIX: Track failed register read
        if (batchMgr)
        {
          batchMgr->incrementFailed(deviceId);
        }
      }
    }
    else if (functionCode == 2)
    {
      result = modbus->readDiscreteInputs(address, 1);
      if (result == modbus->ku8MBSuccess)
      {
        double value = (modbus->getResponseBuffer(0) & 0x01) ? 1.0 : 0.0;
        storeRegisterValue(deviceId, reg, value, deviceName);

        // CRITICAL FIX: Track successful register read
        if (batchMgr)
        {
          batchMgr->incrementEnqueued(deviceId);
        }

        // COMPACT LOGGING: Collect reading to buffer (BUG #31: Optimized to avoid temp String objects)
        const char* unit = reg["unit"] | "";
        char unitBuf[32];
        strncpy(unitBuf, unit, sizeof(unitBuf) - 1);
        unitBuf[sizeof(unitBuf) - 1] = '\0';
        // Replace degree symbol with "deg"
        char* degPos = strstr(unitBuf, "°");
        if (degPos) {
          size_t remaining = strlen(degPos + 2);
          memmove(degPos + 3, degPos + 2, remaining + 1);
          memcpy(degPos, "deg", 3);
        }

        // Add header on first success (sequential appends to avoid temp objects)
        if (successCount == 0) {
          outputBuffer += "[DATA] ";
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
        if (successCount % 6 == 0) {
          char lineBuf[16];
          snprintf(lineBuf, sizeof(lineBuf), "  L%d: ", lineNumber++);
          outputBuffer += lineBuf;
          outputBuffer += compactLine;
          outputBuffer += "\n";
          compactLine.clear();
        } else {
          compactLine += " | ";
        }

        registerSuccess = true;
        anyRegisterSucceeded = true;
      }
      else
      {
        Serial.printf("%s: %s = ERROR\n", deviceId, registerName);
        failedRegisterCount++;

        // CRITICAL FIX: Track failed register read
        if (batchMgr)
        {
          batchMgr->incrementFailed(deviceId);
        }
      }
    }
    else if (functionCode == 3 || functionCode == 4)
    {
      // BUG #31: Use PSRAMString for data type parsing (was String)
      PSRAMString dataType = reg["data_type"] | "INT16";

      // Manual uppercase conversion (PSRAMString doesn't have toUpperCase() yet)
      char dataTypeBuf[64];
      strncpy(dataTypeBuf, dataType.c_str(), sizeof(dataTypeBuf) - 1);
      dataTypeBuf[sizeof(dataTypeBuf) - 1] = '\0';
      for (int i = 0; dataTypeBuf[i]; i++) {
        dataTypeBuf[i] = toupper(dataTypeBuf[i]);
      }

      int registerCount = 1;
      const char* baseType = dataTypeBuf;
      const char* endianness_variant = "";

      char* underscorePos = strchr(dataTypeBuf, '_');
      if (underscorePos != NULL)
      {
        *underscorePos = '\0';  // Split string at underscore
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
        Serial.printf("[RTU] ERROR: Address range overflow for %s (addr=%d, count=%d, max=%d)\n",
                      registerName, address, registerCount, address + registerCount - 1);  // BUG #31: removed .c_str()
        failedRegisterCount++;
        continue; // Skip this register
      }

      uint16_t values[4];
      if (readMultipleRegisters(modbus, functionCode, address, registerCount, values))
      {
        double value = (registerCount == 1) ? processRegisterValue(reg, values[0]) : processMultiRegisterValue(reg, values, registerCount, baseType, endianness_variant);
        storeRegisterValue(deviceId, reg, value, deviceName);

        // CRITICAL FIX: Track successful register read
        if (batchMgr)
        {
          batchMgr->incrementEnqueued(deviceId);
        }

        // COMPACT LOGGING: Collect reading to buffer (BUG #31: Optimized to avoid temp String objects)
        const char* unit = reg["unit"] | "";
        char unitBuf[32];
        strncpy(unitBuf, unit, sizeof(unitBuf) - 1);
        unitBuf[sizeof(unitBuf) - 1] = '\0';
        // Replace degree symbol with "deg"
        char* degPos = strstr(unitBuf, "°");
        if (degPos) {
          size_t remaining = strlen(degPos + 2);
          memmove(degPos + 3, degPos + 2, remaining + 1);
          memcpy(degPos, "deg", 3);
        }

        // Add header on first success (sequential appends to avoid temp objects)
        if (successCount == 0) {
          outputBuffer += "[DATA] ";
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
        if (successCount % 6 == 0) {
          char lineBuf[16];
          snprintf(lineBuf, sizeof(lineBuf), "  L%d: ", lineNumber++);
          outputBuffer += lineBuf;
          outputBuffer += compactLine;
          outputBuffer += "\n";
          compactLine.clear();
        } else {
          compactLine += " | ";
        }

        registerSuccess = true;
        anyRegisterSucceeded = true;
      }
      else
      {
        Serial.printf("%s: %s = ERROR\n", deviceId, registerName);
        failedRegisterCount++;

        // CRITICAL FIX: Track failed register read
        if (batchMgr)
        {
          batchMgr->incrementFailed(deviceId);
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }

  // COMPACT LOGGING: Add remaining items and print buffer atomically
  if (successCount > 0) {
    if (!compactLine.isEmpty()) {
      // Remove trailing " | "
      if (compactLine.endsWith(" | ")) {
        compactLine = compactLine.substring(0, compactLine.length() - 3);
      }
      outputBuffer += "  L" + String(lineNumber) + ": " + compactLine + "\n";
    }
    // Print all at once (atomic - prevents interruption by other tasks)
    Serial.print(outputBuffer);
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
}

double ModbusRtuService::processRegisterValue(const JsonObject &reg, uint16_t rawValue)
{
  // BUG #31: Use const char* and manual uppercase (was String)
  const char* dataType = reg["data_type"] | "UINT16";
  char dataTypeBuf[32];
  strncpy(dataTypeBuf, dataType, sizeof(dataTypeBuf) - 1);
  dataTypeBuf[sizeof(dataTypeBuf) - 1] = '\0';

  // Manual uppercase conversion
  for (int i = 0; dataTypeBuf[i]; i++) {
    dataTypeBuf[i] = toupper(dataTypeBuf[i]);
  }

  if (strcmp(dataTypeBuf, "INT16") == 0)
  {
    return (int16_t)rawValue;
  }
  else if (strcmp(dataTypeBuf, "UINT16") == 0)
  {
    return rawValue;
  }
  else if (strcmp(dataTypeBuf, "BOOL") == 0)
  {
    return rawValue != 0 ? 1.0 : 0.0;
  }
  else if (strcmp(dataTypeBuf, "BINARY") == 0)
  {
    return rawValue;
  }

  // Multi-register types - need to read 2 registers
  // For now return single register value, implement multi-register later
  return rawValue;
}

void ModbusRtuService::storeRegisterValue(const char* deviceId, const JsonObject &reg, double value, const char* deviceName)
{
  QueueManager *queueMgr = QueueManager::getInstance();

  // FIXED Bug #12: Defensive null check for QueueManager
  if (!queueMgr)
  {
    Serial.println("[RTU] ERROR: QueueManager is null, cannot store register value");
    return;
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
  if (!deviceName.isEmpty())
  {
    dataPoint["device_name"] = deviceName;
  }

  dataPoint["address"] = reg["address"];  // Register address (e.g., 4112) for BLE streaming
  dataPoint["value"] = calibratedValue;  // Use calibrated value
  dataPoint["description"] = reg["description"] | "";  // Optional field from BLE config
  dataPoint["unit"] = reg["unit"] | "";  // Unit field for measurement
  dataPoint["register_id"] = reg["register_id"].as<String>();  // Internal use for deduplication
  dataPoint["register_index"] = reg["register_index"] | 0;  // For customize mode topic mapping

  // Add to message queue
  queueMgr->enqueue(dataPoint);

  // Check if this device is being streamed (BUG #31: Optimized String usage)
  if (crudHandler && queueMgr)
  {
    String streamIdStr = crudHandler->getStreamDeviceId();  // CRUDHandler returns String
    if (!streamIdStr.isEmpty() && strcmp(streamIdStr.c_str(), deviceId) == 0)
    {
      Serial.printf("[RTU] Streaming data for device %s to BLE\n", deviceId);
      queueMgr->enqueueStream(dataPoint);
    }
  }
}

bool ModbusRtuService::readMultipleRegisters(ModbusMaster *modbus, uint8_t functionCode, uint16_t address, int count, uint16_t *values)
{
  uint8_t result;
  if (functionCode == 3)
  {
    result = modbus->readHoldingRegisters(address, count);
  }
  else  // Read Input Registers (0x04)
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

double ModbusRtuService::processMultiRegisterValue(const JsonObject &reg, uint16_t *values, int count, const char* baseType, const char* endianness_variant)
{

  if (count == 2)
  {
    uint32_t combined;
    if (endianness_variant == "BE")
    { // Big Endian (ABCD)
      combined = ((uint32_t)values[0] << 16) | values[1];
    }
    else if (endianness_variant == "LE")
    { // True Little Endian (DCBA)
      combined = (((uint32_t)values[1] & 0xFF) << 24) | (((uint32_t)values[1] & 0xFF00) << 8) | (((uint32_t)values[0] & 0xFF) << 8) | ((uint32_t)values[0] >> 8);
    }
    else if (endianness_variant == "BE_BS")
    { // Big Endian + Byte Swap (BADC)
      combined = (((uint32_t)values[0] & 0xFF) << 24) | (((uint32_t)values[0] & 0xFF00) << 8) | (((uint32_t)values[1] & 0xFF) << 8) | ((uint32_t)values[1] >> 8);
    }
    else if (endianness_variant == "LE_BS")
    { // Little Endian + Word Swap (CDAB)
      combined = ((uint32_t)values[1] << 16) | values[0];
    }
    else
    { // Default to Big Endian
      combined = ((uint32_t)values[0] << 16) | values[1];
    }

    if (baseType == "INT32")
    {
      return (int32_t)combined;
    }
    else if (baseType == "UINT32")
    {
      return combined;
    }
    else if (baseType == "FLOAT32")
    {
      // FIXED Bug #5: Use union for safe type conversion (no strict aliasing violation)
      union {
        uint32_t bits;
        float value;
      } converter;
      converter.bits = combined;
      return converter.value;
    }
  }
  else if (count == 4)
  {
    uint64_t combined;

    if (endianness_variant == "BE")
    { // Big Endian (W0, W1, W2, W3)
      combined = ((uint64_t)values[0] << 48) | ((uint64_t)values[1] << 32) | ((uint64_t)values[2] << 16) | values[3];
    }
    else if (endianness_variant == "LE")
    { // True Little Endian (B8..B1)
      uint64_t b1 = values[0] >> 8;
      uint64_t b2 = values[0] & 0xFF;
      uint64_t b3 = values[1] >> 8;
      uint64_t b4 = values[1] & 0xFF;
      uint64_t b5 = values[2] >> 8;
      uint64_t b6 = values[2] & 0xFF;
      uint64_t b7 = values[3] >> 8;
      uint64_t b8 = values[3] & 0xFF;
      combined = (b8 << 56) | (b7 << 48) | (b6 << 40) | (b5 << 32) | (b4 << 24) | (b3 << 16) | (b2 << 8) | b1;
    }
    else if (endianness_variant == "BE_BS")
    { // Big Endian with Byte Swap (B2B1, B4B3, B6B5, B8B7)
      // Registers have bytes swapped within each word. Extract bytes and reassemble as BADCFEHG pattern
      uint64_t b1 = (values[0] >> 8) & 0xFF; // High byte of R1
      uint64_t b2 = values[0] & 0xFF;        // Low byte of R1
      uint64_t b3 = (values[1] >> 8) & 0xFF; // High byte of R2
      uint64_t b4 = values[1] & 0xFF;        // Low byte of R2
      uint64_t b5 = (values[2] >> 8) & 0xFF; // High byte of R3
      uint64_t b6 = values[2] & 0xFF;        // Low byte of R3
      uint64_t b7 = (values[3] >> 8) & 0xFF; // High byte of R4
      uint64_t b8 = values[3] & 0xFF;        // Low byte of R4
      combined = (b2 << 56) | (b1 << 48) | (b4 << 40) | (b3 << 32) | (b6 << 24) | (b5 << 16) | (b8 << 8) | b7;
    }
    else if (endianness_variant == "LE_BS")
    { // Little Endian with Word Swap (W3, W2, W1, W0)
      combined = ((uint64_t)values[3] << 48) | ((uint64_t)values[2] << 32) | ((uint64_t)values[1] << 16) | (uint64_t)values[0];
    }
    else
    { // Default to Big Endian
      combined = ((uint64_t)values[0] << 48) | ((uint64_t)values[1] << 32) | ((uint64_t)values[2] << 16) | values[3];
    }

    if (baseType == "INT64")
    {
      return (double)(int64_t)combined;
    }
    else if (baseType == "UINT64")
    {
      return (double)combined;
    }
    else if (baseType == "DOUBLE64")
    {
      // Safe type-punning using union for IEEE 754 reinterpretation
      union
      {
        uint64_t bits;
        double value;
      } converter;
      converter.bits = combined;
      return converter.value;
    }
  }

  return values[0]; // Fallback
}

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

void ModbusRtuService::getStatus(JsonObject &status)
{
  status["running"] = running;
  status["service_type"] = "modbus_rtu";

  status["rtu_device_count"] = rtuDevices.size(); // Use cached list size
}

// NEW: Modbus Improvement Phase - Helper Method Implementations

void ModbusRtuService::initializeDeviceFailureTracking()
{
  Serial.println("[RTU] Initializing device failure tracking...");
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

    Serial.printf("[RTU] Initialized tracking for device %s (baudrate: %d)\n", device.deviceId, state.baudRate);
  }
}

void ModbusRtuService::initializeDeviceTimeouts()
{
  Serial.println("[RTU] Initializing device timeout tracking...");
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

    Serial.printf("[RTU] Initialized timeout tracking for device: %s (timeout: %dms)\n",
                  device.deviceId, timeout.timeoutMs);
  }
}

ModbusRtuService::DeviceFailureState *ModbusRtuService::getDeviceFailureState(const char* deviceId)
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

ModbusRtuService::DeviceReadTimeout *ModbusRtuService::getDeviceTimeout(const char* deviceId)
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

bool ModbusRtuService::configureDeviceBaudRate(const char* deviceId, uint16_t baudRate)
{
  DeviceFailureState *state = getDeviceFailureState(deviceId);
  if (!state)
  {
    Serial.printf("[RTU] ERROR: Device %s not found for baud rate configuration\n", deviceId);
    return false;
  }

  state->baudRate = baudRate;
  Serial.printf("[RTU] Configured device %s baud rate to: %d\n", deviceId, baudRate);
  return true;
}

uint16_t ModbusRtuService::getDeviceBaudRate(const char* deviceId)
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
    Serial.printf("[RTU] ERROR: Invalid baudrate %d (only 1200-115200 allowed)\n", baudRate);
    return false;
  }
}

// Configure baudrate for a specific serial port (with caching)
void ModbusRtuService::configureBaudRate(int serialPort, uint32_t baudRate)
{
  // Validate baudrate first
  if (!validateBaudRate(baudRate))
  {
    Serial.printf("[RTU] Using default baudrate 9600 instead\n");
    baudRate = 9600;
  }

  if (serialPort == 1)
  {
    // Check if baudrate actually changed (avoid unnecessary reconfiguration)
    if (currentBaudRate1 != baudRate)
    {
      Serial.printf("[RTU] Reconfiguring Serial1 from %d to %d baud\n", currentBaudRate1, baudRate);
      serial1->end();
      serial1->begin(baudRate, SERIAL_8N1, RTU_RX1, RTU_TX1);
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
      Serial.printf("[RTU] Reconfiguring Serial2 from %d to %d baud\n", currentBaudRate2, baudRate);
      serial2->end();
      serial2->begin(baudRate, SERIAL_8N1, RTU_RX2, RTU_TX2);
      currentBaudRate2 = baudRate;

      // Delay to stabilize serial communication
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }
  else
  {
    Serial.printf("[RTU] ERROR: Invalid serial port %d\n", serialPort);
  }
}

void ModbusRtuService::handleReadFailure(const char* deviceId)
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

    Serial.printf("[RTU] Device %s read failed. Retry %d/%d in %lums\n",
                  deviceId, state->retryCount, state->maxRetries, backoffTime);
  }
  else
  {
    // Max retries exceeded
    Serial.printf("[RTU] Device %s exceeded max retries (%d), disabling...\n",
                  deviceId, state->maxRetries);
    disableDevice(deviceId);
  }
}

bool ModbusRtuService::shouldRetryDevice(const char* deviceId)
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

  Serial.printf("[RTU] Exponential backoff: retry %d to %lums base + %lums jitter = %lums total\n",
                retryCount, baseBackoff, jitter, totalBackoff);

  return totalBackoff;
}

void ModbusRtuService::resetDeviceFailureState(const char* deviceId)
{
  DeviceFailureState *state = getDeviceFailureState(deviceId);
  if (!state)
    return;

  state->consecutiveFailures = 0;
  state->retryCount = 0;
  state->nextRetryTime = 0;
  state->lastReadAttempt = millis();

  Serial.printf("[RTU] Reset failure state for device %s\n", deviceId);
}

void ModbusRtuService::handleReadTimeout(const char* deviceId)
{
  DeviceReadTimeout *timeout = getDeviceTimeout(deviceId);
  if (!timeout)
    return;

  timeout->consecutiveTimeouts++;

  if (timeout->consecutiveTimeouts >= timeout->maxConsecutiveTimeouts)
  {
    Serial.printf("[RTU] Device %s exceeded timeout limit (%d), disabling...\n",
                  deviceId, timeout->maxConsecutiveTimeouts);
    disableDevice(deviceId);
  }
  else
  {
    Serial.printf("[RTU] Device %s timeout %d/%d\n",
                  deviceId, timeout->consecutiveTimeouts, timeout->maxConsecutiveTimeouts);
  }
}

bool ModbusRtuService::isDeviceEnabled(const char* deviceId)
{
  DeviceFailureState *state = getDeviceFailureState(deviceId);
  if (!state)
    return true; // Default to enabled if not found
  return state->isEnabled;
}

void ModbusRtuService::enableDevice(const char* deviceId)
{
  DeviceFailureState *state = getDeviceFailureState(deviceId);
  if (!state)
    return;

  state->isEnabled = true;
  resetDeviceFailureState(deviceId);

  DeviceReadTimeout *timeout = getDeviceTimeout(deviceId);
  if (timeout)
  {
    timeout->consecutiveTimeouts = 0;
    timeout->lastSuccessfulRead = millis();
  }

  Serial.printf("[RTU] Device %s enabled\n", deviceId);
}

void ModbusRtuService::disableDevice(const char* deviceId)
{
  DeviceFailureState *state = getDeviceFailureState(deviceId);
  if (!state)
    return;

  state->isEnabled = false;

  Serial.printf("[RTU] Device %s disabled due to failures/timeouts\n", deviceId);
}

// NEW: Polling Hierarchy Implementation
// ============================================
// CLEANUP: Removed Level 1 per-register polling methods (getRegisterTimer, shouldPollRegister, updateRegisterLastRead)
// All registers now use device-level polling interval

// Level 1: Device-level timing methods
ModbusRtuService::DeviceTimer *ModbusRtuService::getDeviceTimer(const char* deviceId)
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

bool ModbusRtuService::shouldPollDevice(const char* deviceId, uint32_t refreshRateMs)
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

void ModbusRtuService::updateDeviceLastRead(const char* deviceId)
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
  Serial.printf("[RTU] Data transmission interval set to: %lums\n", intervalMs);
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
}
