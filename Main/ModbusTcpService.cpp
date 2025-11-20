#include "ModbusTcpService.h"
#include "QueueManager.h"
#include "DeviceBatchManager.h"  // CRITICAL FIX: Track batch completion
#include "CRUDHandler.h"
#include "RTCManager.h"
#include "NetworkManager.h"
#include "TCPClient.h"
#include "DebugConfig.h"
#include "MemoryRecovery.h"
#include <byteswap.h>
#include <WiFi.h>

extern CRUDHandler *crudHandler;

// Atomic transaction counter initialization (thread-safe)
std::atomic<uint16_t> ModbusTcpService::atomicTransactionCounter(1);

ModbusTcpService::ModbusTcpService(ConfigManager *config, EthernetManager *ethernet)
    : configManager(config), ethernetManager(ethernet), running(false), tcpTaskHandle(nullptr)
{
  // Initialize data transmission schedule
  dataTransmissionSchedule.lastTransmitted = 0;
  dataTransmissionSchedule.dataIntervalMs = 5000; // Default 5 seconds
}

bool ModbusTcpService::init()
{
  Serial.println("Initializing custom Modbus TCP service...");

  if (!configManager)
  {
    Serial.println("ConfigManager is null");
    return false;
  }

  if (!ethernetManager)
  {
    Serial.println("EthernetManager is null");
    return false;
  }

  Serial.printf("Ethernet available: %s\n", ethernetManager->isAvailable() ? "YES" : "NO");
  Serial.println("Custom Modbus TCP service initialized successfully");
  return true;
}

void ModbusTcpService::start()
{
  Serial.println("Starting custom Modbus TCP service...");

  if (running)
  {
    Serial.println("Service already running");
    return;
  }

  running = true;
  BaseType_t result = xTaskCreatePinnedToCore(
      readTcpDevicesTask,
      "MODBUS_TCP_TASK",
      8192,
      this,
      2,
      &tcpTaskHandle, // Store the task handle
      1);

  if (result == pdPASS)
  {
    Serial.println("Custom Modbus TCP service started successfully");
  }
  else
  {
    Serial.println("Failed to create Modbus TCP task");
    running = false;
    tcpTaskHandle = nullptr;
  }
}

void ModbusTcpService::stop()
{
  running = false;
  if (tcpTaskHandle)
  {
    vTaskDelay(pdMS_TO_TICKS(100)); // Allow task to exit gracefully
    vTaskDelete(tcpTaskHandle);
    tcpTaskHandle = nullptr;
  }
  Serial.println("Custom Modbus TCP service stopped");
}

void ModbusTcpService::notifyConfigChange()
{
  if (tcpTaskHandle != nullptr)
  {
    xTaskNotifyGive(tcpTaskHandle);
  }
}

void ModbusTcpService::refreshDeviceList()
{
  Serial.println("[TCP Task] Refreshing device list and schedule...");
  tcpDevices.clear(); // FIXED Bug #2: Now safe - unique_ptr auto-deletes old documents

  // Clear the priority queue
  std::priority_queue<PollingTask, std::vector<PollingTask>, std::greater<PollingTask>> emptyQueue;
  pollingQueue.swap(emptyQueue);

  JsonDocument devicesIdList;
  JsonArray deviceIds = devicesIdList.to<JsonArray>();
  configManager->listDevices(deviceIds);

  unsigned long now = millis();

  for (JsonVariant deviceIdVar : deviceIds)
  {
    String deviceId = deviceIdVar.as<String>();
    if (deviceId.isEmpty() || deviceId == "{}" || deviceId.length() < 3)
    {
      continue;
    }

    JsonDocument tempDeviceDoc;
    JsonObject deviceObj = tempDeviceDoc.to<JsonObject>();
    if (configManager->readDevice(deviceId, deviceObj))
    {
      String protocol = deviceObj["protocol"] | "";
      if (protocol == "TCP")
      {
        TcpDeviceConfig newDeviceEntry;
        newDeviceEntry.deviceId = deviceId;
        newDeviceEntry.doc = std::make_unique<JsonDocument>(); // FIXED Bug #2: Use smart pointer
        newDeviceEntry.doc->set(deviceObj);
        tcpDevices.push_back(std::move(newDeviceEntry));

        // Add device to the polling schedule for an immediate first poll
        pollingQueue.push({deviceId, now});
      }
    }
  }
  Serial.printf("[TCP Task] Found %d TCP devices. Schedule rebuilt.\n", tcpDevices.size());
}

void ModbusTcpService::readTcpDevicesTask(void *parameter)
{
  ModbusTcpService *service = static_cast<ModbusTcpService *>(parameter);
  service->readTcpDevicesLoop();
}

void ModbusTcpService::readTcpDevicesLoop()
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
      Serial.println("[TCP] Configuration changed, device list refreshed");
    }

    // Smart network detection - check both Ethernet AND WiFi
    // Modbus TCP works with ANY TCP/IP interface (Ethernet OR WiFi)

    bool hasEthernet = (ethernetManager && ethernetManager->isAvailable());

    // Check WiFi status via NetworkManager singleton
    bool hasWiFi = false;
    NetworkMgr *netMgr = NetworkMgr::getInstance();
    if (netMgr)
    {
      // Check if WiFi mode is active or if NetworkManager has any active network
      String activeMode = netMgr->getCurrentMode();
      hasWiFi = (activeMode == "WIFI" && netMgr->isAvailable());
    }

    bool hasNetwork = hasEthernet || hasWiFi;

    if (!hasNetwork)
    {
      // No network available at all - wait and retry
      Serial.println("[TCP] No network available (Ethernet and WiFi both disabled/disconnected)");
      vTaskDelay(pdMS_TO_TICKS(5000)); // Wait 5 seconds before retrying
      continue;
    }

    // Network is available - load device list
    JsonDocument devicesDoc;
    JsonArray devices = devicesDoc.to<JsonArray>();
    configManager->listDevices(devices);

    // Count TCP devices before polling
    int tcpDeviceCount = 0;
    for (JsonVariant deviceVar : devices)
    {
      String deviceId = deviceVar.as<String>();
      JsonDocument tempDoc;
      JsonObject tempObj = tempDoc.to<JsonObject>();
      if (configManager->readDevice(deviceId, tempObj))
      {
        String protocol = tempObj["protocol"] | "";
        if (protocol == "TCP")
        {
          tcpDeviceCount++;
        }
      }
    }

    // Skip polling if no TCP devices
    if (tcpDeviceCount == 0)
    {
      vTaskDelay(pdMS_TO_TICKS(2000));
      continue;
    }

    // Network is available and we have TCP devices - proceed with polling
    // Only log if mode or device count changes (avoid repetitive messages)
    static bool lastHasEthernet = false;
    static int lastDeviceCount = -1;
    static bool firstRun = true;

    bool shouldLog = firstRun ||
                     (hasEthernet != lastHasEthernet) ||
                     (tcpDeviceCount != lastDeviceCount);

    if (shouldLog)
    {
      if (hasEthernet)
      {
        Serial.printf("[TCP] Using Ethernet for Modbus TCP polling (%d devices)\n", tcpDeviceCount);
      }
      else if (hasWiFi)
      {
        Serial.printf("[TCP] Using WiFi for Modbus TCP polling (%d devices)\n", tcpDeviceCount);
      }
      lastHasEthernet = hasEthernet;
      lastDeviceCount = tcpDeviceCount;
      firstRun = false;
    }

    unsigned long currentTime = millis();

    for (JsonVariant deviceVar : devices)
    {
      if (!running)
        break; // Exit if stopped

      String deviceId = deviceVar.as<String>();

      JsonDocument deviceDoc;
      JsonObject deviceObj = deviceDoc.to<JsonObject>();

      if (configManager->readDevice(deviceId, deviceObj))
      {
        String protocol = deviceObj["protocol"] | "";

        if (protocol == "TCP")
        {
          // Get device refresh rate (minimum retry interval)
          uint32_t deviceRefreshRate = deviceObj["refresh_rate_ms"] | 5000;

          // Check if device minimum retry interval has elapsed
          if (shouldPollDevice(deviceId, deviceRefreshRate))
          {
            readTcpDeviceData(deviceObj);
            // Device-level timing is updated inside readTcpDeviceData via updateDeviceLastRead()
          }
        }
      }

      // Feed watchdog between device reads
      vTaskDelay(pdMS_TO_TICKS(10));
    }

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void ModbusTcpService::readTcpDeviceData(const JsonObject &deviceConfig)
{
  String deviceId = deviceConfig["device_id"] | "UNKNOWN";
  String ip = deviceConfig["ip"] | "";
  int port = deviceConfig["port"] | 502;
  uint8_t slaveId = deviceConfig["slave_id"] | 1;
  JsonArray registers = deviceConfig["registers"];

  // FIXED BUG #8: Validate IP address format before use
  // Previous code only checked isEmpty() → invalid IPs like "999.999.999.999" passed!
  ip.trim();
  if (ip.isEmpty() || registers.size() == 0)
  {
    return;
  }

  // Validate IP address format using IPAddress::fromString()
  IPAddress validatedIP;
  if (!validatedIP.fromString(ip))
  {
    static unsigned long lastWarning = 0;
    if (millis() - lastWarning > 30000)  // Log max once per 30s per device
    {
      Serial.printf("[TCP] ERROR: Invalid IP address format for device %s: '%s'\n", deviceId.c_str(), ip.c_str());
      Serial.println("[TCP] HINT: IP must be in format A.B.C.D where 0 <= A,B,C,D <= 255");
      lastWarning = millis();
    }
    return;  // Skip device with invalid IP
  }

  // Determine network type for log
  String networkType = "TCP/WiFi";  // Default assume WiFi
  NetworkMgr *networkMgr = NetworkMgr::getInstance();
  if (networkMgr)
  {
    String currentMode = networkMgr->getCurrentMode();
    if (currentMode == "ETH")
    {
      networkType = "TCP/ETH";
    }
  }

  Serial.printf("[%s] Polling device %s at %s:%d\n", networkType.c_str(), deviceId.c_str(), ip.c_str(), port);

  // OPTIMIZED: Get device_name once per device cycle (not per register)
  String deviceName = deviceConfig["device_name"] | "";

  // CRITICAL FIX: Start batch tracking BEFORE reading registers
  DeviceBatchManager *batchMgr = DeviceBatchManager::getInstance();
  if (batchMgr)
  {
    batchMgr->startBatch(deviceId, registers.size());
  }

  // COMPACT LOGGING: Buffer all output for atomic printing (prevent interruption by other tasks)
  String outputBuffer = "";
  String compactLine = "";
  int successCount = 0;
  int lineNumber = 1;

  for (JsonVariant regVar : registers)
  {
    if (!running)
      break;

    JsonObject reg = regVar.as<JsonObject>();
    uint8_t functionCode = reg["function_code"] | 3;
    uint16_t address = reg["address"] | 0;
    String registerName = reg["register_name"] | "Unknown";

    if (functionCode == 1 || functionCode == 2)
    {
      // Read coils/discrete inputs
      bool result = false;
      if (readModbusCoil(ip, port, slaveId, address, &result))
      {
        float value = result ? 1.0 : 0.0;
        storeRegisterValue(deviceId, reg, value, deviceName);

        // CRITICAL FIX: Track successful register read
        if (batchMgr)
        {
          batchMgr->incrementEnqueued(deviceId);
        }

        // COMPACT LOGGING: Collect reading to buffer
        String unit = reg["unit"] | "";
        unit.replace("°", "deg");  // Replace UTF-8 degree symbol with ASCII "deg"

        // Add header on first success
        if (successCount == 0) {
          outputBuffer += "[DATA] " + deviceId + ":\n";
        }

        compactLine += registerName + ":" + String(value, 1) + unit;
        successCount++;
        if (successCount % 6 == 0) {
          outputBuffer += "  L" + String(lineNumber++) + ": " + compactLine + "\n";
          compactLine = "";
        } else {
          compactLine += " | ";
        }
      }
      else
      {
        Serial.printf("%s: %s = ERROR\n", deviceId.c_str(), registerName.c_str());

        // CRITICAL FIX: Track failed register read
        if (batchMgr)
        {
          batchMgr->incrementFailed(deviceId);
        }
      }
    }
    else
    {
      // Read registers
      String dataType = reg["data_type"] | "uint16";
      dataType.toUpperCase();
      int registerCount = 1;
      String baseType = dataType;
      String endianness_variant = "";

      // Extract baseType and endianness variant
      int underscoreIndex = dataType.indexOf('_');
      if (underscoreIndex != -1)
      {
        baseType = dataType.substring(0, underscoreIndex);
        endianness_variant = dataType.substring(underscoreIndex + 1);
      }

      // Determine register count based on base type
      if (baseType == "INT32" || baseType == "UINT32" || baseType == "FLOAT32")
      {
        registerCount = 2;
      }
      else if (baseType == "INT64" || baseType == "UINT64" || baseType == "DOUBLE64")
      {
        registerCount = 4;
      }

      // FIXED Bug #8: Validate address range doesn't overflow uint16_t address space
      if (address > (65535 - registerCount + 1))
      {
        Serial.printf("[TCP] ERROR: Address range overflow for %s (addr=%d, count=%d, max=%d)\n",
                      registerName.c_str(), address, registerCount, address + registerCount - 1);

        // CRITICAL FIX: Track failed register (validation error)
        if (batchMgr)
        {
          batchMgr->incrementFailed(deviceId);
        }

        continue; // Skip this register
      }

      uint16_t results[4];
      if (readModbusRegisters(ip, port, slaveId, functionCode, address, registerCount, results))
      {
        double value = (registerCount == 1) ? processRegisterValue(reg, results[0]) : processMultiRegisterValue(reg, results, registerCount, baseType, endianness_variant);
        storeRegisterValue(deviceId, reg, value, deviceName);

        // CRITICAL FIX: Track successful register read
        if (batchMgr)
        {
          batchMgr->incrementEnqueued(deviceId);
        }

        // COMPACT LOGGING: Collect reading to buffer
        String unit = reg["unit"] | "";
        unit.replace("°", "deg");  // Replace UTF-8 degree symbol with ASCII "deg"

        // Add header on first success
        if (successCount == 0) {
          outputBuffer += "[DATA] " + deviceId + ":\n";
        }

        compactLine += registerName + ":" + String(value, 1) + unit;
        successCount++;
        if (successCount % 6 == 0) {
          outputBuffer += "  L" + String(lineNumber++) + ": " + compactLine + "\n";
          compactLine = "";
        } else {
          compactLine += " | ";
        }
      }
      else
      {
        Serial.printf("%s: %s = ERROR\n", deviceId.c_str(), registerName.c_str());

        // CRITICAL FIX: Track failed register read
        if (batchMgr)
        {
          batchMgr->incrementFailed(deviceId);
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(50)); // Small delay between registers
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
}

double ModbusTcpService::processRegisterValue(const JsonObject &reg, uint16_t rawValue)
{
  String dataType = reg["data_type"];
  dataType.toUpperCase();

  if (dataType == "INT16")
  {
    return (int16_t)rawValue;
  }
  else if (dataType == "UINT16")
  {
    return rawValue;
  }
  else if (dataType == "BOOL")
  {
    return rawValue != 0 ? 1.0 : 0.0;
  }
  else if (dataType == "BINARY")
  {
    return rawValue;
  }

  // Multi-register types - need to read 2 registers
  // For now return single register value, implement multi-register later
  return rawValue;
}

double ModbusTcpService::processMultiRegisterValue(const JsonObject &reg, uint16_t *values, int count, const String &baseType, const String &endianness_variant)
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

      combined = (((uint32_t)values[1] & 0xFF) << 24) | (((uint32_t)values[1] & 0xFF00) << 8) |

                 (((uint32_t)values[0] & 0xFF) << 8) | ((uint32_t)values[0] >> 8);
    }
    else if (endianness_variant == "BE_BS")
    { // Big Endian + Byte Swap (BADC)

      combined = (((uint32_t)values[0] & 0xFF) << 24) | (((uint32_t)values[0] & 0xFF00) << 8) |

                 (((uint32_t)values[1] & 0xFF) << 8) | ((uint32_t)values[1] >> 8);
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

bool ModbusTcpService::readModbusRegister(const String &ip, int port, uint8_t slaveId, uint8_t functionCode, uint16_t address, uint16_t *result)
{
  // Support both WiFi and Ethernet using TCPClient wrapper
  TCPClient client;
  client.setTimeout(ModbusTcpConfig::TIMEOUT_MS); // FIXED Bug #10

  if (!client.connect(ip.c_str(), port))
  {
    Serial.printf("[TCP] Register connection failed to %s:%d\n", ip.c_str(), port);
    return false;
  }

  // Build Modbus TCP request
  uint8_t request[ModbusTcpConfig::MODBUS_TCP_HEADER_SIZE]; // FIXED Bug #10
  uint16_t transId = getNextTransactionId();
  buildModbusRequest(request, transId, slaveId, functionCode, address, 1);

  // Send request
  client.write(request, ModbusTcpConfig::MODBUS_TCP_HEADER_SIZE); // FIXED Bug #10

  // FIXED Bug #11: Safe time comparison to handle millis() wraparound
  unsigned long startTime = millis();
  while (client.available() < ModbusTcpConfig::MIN_RESPONSE_SIZE && (millis() - startTime) < ModbusTcpConfig::TIMEOUT_MS)
  {
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  if (client.available() < ModbusTcpConfig::MIN_RESPONSE_SIZE) // FIXED Bug #10
  {
    client.stop();
    return false;
  }

  // Read response - FIXED Bug #3: Use dynamic buffer with bounds check
  int availableBytes = client.available();

  // Sanity check: Modbus TCP response shouldn't exceed reasonable size
  if (availableBytes > ModbusTcpConfig::MAX_RESPONSE_SIZE) // FIXED Bug #10
  {
    Serial.printf("[TCP] Response too large (%d bytes), rejecting\n", availableBytes);
    client.stop();
    return false;
  }

  std::vector<uint8_t> response(availableBytes);
  int bytesRead = client.readBytes(response.data(), availableBytes);
  client.stop();

  bool dummy = false;
  return parseModbusResponse(response.data(), bytesRead, functionCode, result, &dummy);
}

bool ModbusTcpService::readModbusRegisters(const String &ip, int port, uint8_t slaveId, uint8_t functionCode, uint16_t address, int count, uint16_t *results)
{
  // ROLLBACK Bug #4: Use TCPClient wrapper to support both WiFi and Ethernet
  TCPClient client;
  client.setTimeout(ModbusTcpConfig::TIMEOUT_MS); // FIXED Bug #10: Use named constant

  if (!client.connect(ip.c_str(), port))
  {
    Serial.printf("[TCP] Failed to connect to %s:%d\n", ip.c_str(), port);
    return false;
  }

  // Build Modbus TCP request
  uint8_t request[ModbusTcpConfig::MODBUS_TCP_HEADER_SIZE]; // FIXED Bug #10
  uint16_t transId = getNextTransactionId();
  buildModbusRequest(request, transId, slaveId, functionCode, address, count);

  // Send request
  client.write(request, ModbusTcpConfig::MODBUS_TCP_HEADER_SIZE); // FIXED Bug #10

  // FIXED Bug #11: Safe time comparison to handle millis() wraparound
  unsigned long startTime = millis();
  int expectedBytes = ModbusTcpConfig::MIN_RESPONSE_SIZE + (count * 2); // FIXED Bug #10
  while (client.available() < expectedBytes && (millis() - startTime) < ModbusTcpConfig::TIMEOUT_MS)
  {
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  if (client.available() < expectedBytes)
  {
    Serial.printf("[TCP] Response timeout or incomplete for %s:%d\n", ip.c_str(), port);
    client.stop();
    return false;
  }

  // Read response - FIXED Bug #3: Use dynamic buffer with bounds check
  int availableBytes = client.available();

  // Sanity check: Modbus TCP response shouldn't exceed reasonable size
  if (availableBytes > ModbusTcpConfig::MAX_RESPONSE_SIZE) // FIXED Bug #10
  {
    Serial.printf("[TCP] Response too large (%d bytes), rejecting\n", availableBytes);
    client.stop();
    return false;
  }

  std::vector<uint8_t> response(availableBytes);
  int bytesRead = client.readBytes(response.data(), availableBytes);
  client.stop();

  return parseMultiModbusResponse(response.data(), bytesRead, functionCode, count, results);
}

bool ModbusTcpService::readModbusCoil(const String &ip, int port, uint8_t slaveId, uint16_t address, bool *result)
{
  // Support both WiFi and Ethernet using TCPClient wrapper
  TCPClient client;
  client.setTimeout(ModbusTcpConfig::TIMEOUT_MS); // FIXED Bug #10

  if (!client.connect(ip.c_str(), port))
  {
    Serial.printf("[TCP] Coil connection failed to %s:%d\n", ip.c_str(), port);
    return false;
  }

  // Build Modbus TCP request for coil
  uint8_t request[ModbusTcpConfig::MODBUS_TCP_HEADER_SIZE]; // FIXED Bug #10
  uint16_t transId = getNextTransactionId();
  buildModbusRequest(request, transId, slaveId, 1, address, 1); // Function code 1 for coils

  // Send request
  client.write(request, ModbusTcpConfig::MODBUS_TCP_HEADER_SIZE); // FIXED Bug #10

  // FIXED Bug #11: Safe time comparison to handle millis() wraparound
  unsigned long startTime = millis();
  while (client.available() < ModbusTcpConfig::MIN_RESPONSE_SIZE && (millis() - startTime) < ModbusTcpConfig::TIMEOUT_MS)
  {
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  if (client.available() < ModbusTcpConfig::MIN_RESPONSE_SIZE) // FIXED Bug #10
  {
    client.stop();
    return false;
  }

  // Read response - FIXED Bug #3: Use dynamic buffer with bounds check
  int availableBytes = client.available();

  // Sanity check: Modbus TCP response shouldn't exceed reasonable size
  if (availableBytes > ModbusTcpConfig::MAX_RESPONSE_SIZE) // FIXED Bug #10
  {
    Serial.printf("[TCP] Response too large (%d bytes), rejecting\n", availableBytes);
    client.stop();
    return false;
  }

  std::vector<uint8_t> response(availableBytes);
  int bytesRead = client.readBytes(response.data(), availableBytes);
  client.stop();

  uint16_t dummy;
  return parseModbusResponse(response.data(), bytesRead, 1, &dummy, result);
}

void ModbusTcpService::buildModbusRequest(uint8_t *buffer, uint16_t transId, uint8_t unitId, uint8_t funcCode, uint16_t addr, uint16_t qty)
{
  // Modbus TCP header
  buffer[0] = (transId >> 8) & 0xFF; // Transaction ID high
  buffer[1] = transId & 0xFF;        // Transaction ID low
  buffer[2] = 0x00;                  // Protocol ID high
  buffer[3] = 0x00;                  // Protocol ID low
  buffer[4] = 0x00;                  // Length high
  buffer[5] = 0x06;                  // Length low (6 bytes following)
  buffer[6] = unitId;                // Unit ID

  // Modbus PDU
  buffer[7] = funcCode;           // Function code
  buffer[8] = (addr >> 8) & 0xFF; // Start address high
  buffer[9] = addr & 0xFF;        // Start address low
  buffer[10] = (qty >> 8) & 0xFF; // Quantity high
  buffer[11] = qty & 0xFF;        // Quantity low
}

bool ModbusTcpService::parseModbusResponse(uint8_t *buffer, int length, uint8_t expectedFunc, uint16_t *result, bool *boolResult)
{
  if (length < 9)
  {
    return false;
  }

  // Check function code
  uint8_t funcCode = buffer[7];
  if (funcCode != expectedFunc)
  {
    // Check for error response
    if (funcCode == (expectedFunc | 0x80))
    {
      // Modbus error
    }
    return false;
  }

  // Parse data based on function code
  if (funcCode == 1 || funcCode == 2)
  {
    // Coil/discrete input response
    if (length >= 10 && boolResult)
    {
      uint8_t byteCount = buffer[8];
      if (byteCount > 0)
      {
        *boolResult = (buffer[9] & 0x01) != 0;
        return true;
      }
    }
  }
  else if (funcCode == 3 || funcCode == 4)
  {
    // Register response
    if (length >= 11 && result)
    {
      uint8_t byteCount = buffer[8];
      if (byteCount >= 2)
      {
        *result = (buffer[9] << 8) | buffer[10];
        return true;
      }
    }
  }

  return false;
}

bool ModbusTcpService::parseMultiModbusResponse(uint8_t *buffer, int length, uint8_t expectedFunc, int count, uint16_t *results)
{
  if (length < 9)
  {
    return false;
  }

  // Check function code
  uint8_t funcCode = buffer[7];
  if (funcCode != expectedFunc)
  {
    // Check for error response
    if (funcCode == (expectedFunc | 0x80))
    {
      // Modbus error
    }
    return false;
  }

  // Parse data based on function code
  if (funcCode == 3 || funcCode == 4)
  {
    // Register response (holding or input registers)
    if (length >= 9 && results)
    {
      uint8_t byteCount = buffer[8];
      if (byteCount == count * 2)
      {
        // Extract register values
        for (int i = 0; i < count; i++)
        {
          results[i] = (buffer[9 + (i * 2)] << 8) | buffer[9 + (i * 2) + 1];
        }
        return true;
      }
    }
  }

  return false;
}

void ModbusTcpService::storeRegisterValue(const String &deviceId, const JsonObject &reg, double value, const String &deviceName)
{
  QueueManager *queueMgr = QueueManager::getInstance();

  // FIXED Bug #12: Defensive null check for QueueManager
  if (!queueMgr)
  {
    Serial.println("[TCP] ERROR: QueueManager is null, cannot store register value");
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

  // OPTIMIZED: Use pre-fetched device_name (passed from readTcpDeviceData)
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

  // Check if this device is being streamed
  String streamId = "";
  bool crudHandlerAvailable = (crudHandler != nullptr);

  if (crudHandler)
  {
    streamId = crudHandler->getStreamDeviceId();
  }

  if (!streamId.isEmpty() && streamId == deviceId && queueMgr)
  {
    Serial.printf("[TCP] Streaming data for device %s to BLE\n", deviceId.c_str());
    queueMgr->enqueueStream(dataPoint);
  }
}

void ModbusTcpService::getStatus(JsonObject &status)
{
  status["running"] = running;
  status["service_type"] = "modbus_tcp";
  status["tcp_device_count"] = tcpDevices.size();
}

// Modbus TCP Improvement Phase - Helper Method Implementations

uint16_t ModbusTcpService::getNextTransactionId()
{
  // Thread-safe atomic increment
  uint16_t transId = ++atomicTransactionCounter;

  // Handle wrap-around: if reached max, reset to 1
  if (transId == 0)
  {
    atomicTransactionCounter.store(1);
    transId = 1;
  }

  return transId;
}

// Polling Hierarchy Implementation
// ============================================
// CLEANUP: Removed Level 1 per-register polling methods (getRegisterTimer, shouldPollRegister, updateRegisterLastRead)
// All registers now use device-level polling interval

// Level 1: Device-level timing methods
ModbusTcpService::DeviceTimer *ModbusTcpService::getDeviceTimer(const String &deviceId)
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

bool ModbusTcpService::shouldPollDevice(const String &deviceId, uint32_t refreshRateMs)
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

void ModbusTcpService::updateDeviceLastRead(const String &deviceId)
{
  DeviceTimer *timer = getDeviceTimer(deviceId);
  if (timer)
  {
    timer->lastRead = millis();
  }
}

// Level 3: Server data transmission interval methods
bool ModbusTcpService::shouldTransmitData(uint32_t dataIntervalMs)
{
  unsigned long now = millis();
  return (now - dataTransmissionSchedule.lastTransmitted) >= dataIntervalMs;
}

void ModbusTcpService::updateDataTransmissionTime()
{
  dataTransmissionSchedule.lastTransmitted = millis();
}

void ModbusTcpService::setDataTransmissionInterval(uint32_t intervalMs)
{
  dataTransmissionSchedule.dataIntervalMs = intervalMs;
  Serial.printf("[TCP] Data transmission interval set to: %lums\n", intervalMs);
}

ModbusTcpService::~ModbusTcpService()
{
  stop();
}
