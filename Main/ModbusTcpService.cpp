#include "DebugConfig.h"  // MUST BE FIRST for DEV_SERIAL_* macros
#include "ModbusTcpService.h"
#include "QueueManager.h"
#include "CRUDHandler.h"
#include "RTCManager.h"
#include "NetworkManager.h"
#include "TCPClient.h"
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

  // FIXED BUG #14: Initialize connection pool mutex
  poolMutex = xSemaphoreCreateMutex();
  if (!poolMutex)
  {
    LOG_TCP_INFO("[TCP] ERROR: Failed to create pool mutex!");
  }
  else
  {
    LOG_TCP_INFO("[TCP] Connection pool initialized");
  }

  // FIXED ISSUE #1: Initialize vector mutex (recursive for nested calls)
  vectorMutex = xSemaphoreCreateRecursiveMutex();
  if (!vectorMutex)
  {
    LOG_TCP_INFO("[TCP] CRITICAL: Failed to create vector mutex!");
  }
  else
  {
    LOG_TCP_INFO("[TCP] Vector mutex initialized (race condition protection active)");
  }
}

bool ModbusTcpService::init()
{
  LOG_TCP_INFO("[TCP] Initializing service...");

  if (!configManager)
  {
    LOG_TCP_INFO("[TCP] ERROR: ConfigManager is null");
    return false;
  }

  if (!ethernetManager)
  {
    LOG_TCP_INFO("[TCP] ERROR: EthernetManager is null");
    return false;
  }

  LOG_TCP_INFO("[TCP] Ethernet available: %s\n", ethernetManager->isAvailable() ? "YES" : "NO");
  LOG_TCP_INFO("[TCP] Service initialized");
  return true;
}

void ModbusTcpService::start()
{
  LOG_TCP_INFO("[TCP] Starting service...");

  if (running)
  {
    LOG_TCP_INFO("[TCP] Service already running");
    return;
  }

  running = true;
  BaseType_t result = xTaskCreatePinnedToCore(
      readTcpDevicesTask,
      "MODBUS_TCP_TASK",
      10240, // STACK OVERFLOW FIX (v2.3.8): Increased from 8KB to 10KB for very safe operation
      this,
      2,
      &tcpTaskHandle, // Store the task handle
      1);

  if (result == pdPASS)
  {
    LOG_TCP_INFO("[TCP] Service started successfully");

    // Start auto-recovery task
    // MUST stay on Core 1: Modifies device status accessed by MODBUS_TCP_TASK (Core 1)
    BaseType_t recoveryResult = xTaskCreatePinnedToCore(
        autoRecoveryTask,
        "TCP_AUTO_RECOVERY",
        4096,
        this,
        1,
        &autoRecoveryTaskHandle,
        1); // Core 1 (must stay with TCP polling task to avoid race conditions)

    if (recoveryResult == pdPASS)
    {
      LOG_TCP_INFO("[TCP] Auto-recovery task started");
    }
    else
    {
      LOG_TCP_INFO("[TCP] WARNING: Failed to create auto-recovery task");
    }
  }
  else
  {
    LOG_TCP_INFO("[TCP] ERROR: Failed to create Modbus TCP task");
    running = false;
    tcpTaskHandle = nullptr;
  }
}

void ModbusTcpService::stop()
{
  running = false;

  // Stop auto-recovery task
  if (autoRecoveryTaskHandle)
  {
    vTaskDelay(pdMS_TO_TICKS(100));
    vTaskDelete(autoRecoveryTaskHandle);
    autoRecoveryTaskHandle = nullptr;
    LOG_TCP_INFO("[TCP] Auto-recovery task stopped");
  }

  // Stop main TCP task
  if (tcpTaskHandle)
  {
    vTaskDelay(pdMS_TO_TICKS(100)); // Allow task to exit gracefully
    vTaskDelete(tcpTaskHandle);
    tcpTaskHandle = nullptr;
  }

  LOG_TCP_INFO("[TCP] Service stopped");
}

void ModbusTcpService::notifyConfigChange()
{
  // v2.5.39: Set atomic flag for reliable config change detection
  // This ensures config changes are detected even if task is blocked in TCP operations
  configChangePending.store(true);
  LOG_TCP_INFO("[TCP] Config change notified - flagged for refresh\n");

  if (tcpTaskHandle != nullptr)
  {
    xTaskNotifyGive(tcpTaskHandle);
  }
}

void ModbusTcpService::refreshDeviceList()
{
  // FIXED ISSUE #1: Protect vector operations from race conditions
  xSemaphoreTakeRecursive(vectorMutex, portMAX_DELAY);

  // v2.5.40 FIX: Close all pooled connections when config changes
  // This ensures clean reconnection when IP/port changes
  // Without this, old connections might linger and cause confusion
  closeAllConnections();

  LOG_TCP_INFO("[TCP Task] Refreshing device list (connections cleared)...");
  tcpDevices.clear(); // FIXED Bug #2: Now safe - unique_ptr auto-deletes old documents

  JsonDocument devicesIdList;
  JsonArray deviceIds = devicesIdList.to<JsonArray>();
  configManager->listDevices(deviceIds);

  unsigned long now = millis();

  for (JsonVariant deviceIdVar : deviceIds)
  {
    // v2.5.41: Use const char* to avoid String allocation (matches RTU service pattern)
    const char *deviceId = deviceIdVar.as<const char *>();
    if (!deviceId || strcmp(deviceId, "") == 0 || strcmp(deviceId, "{}") == 0 || strlen(deviceId) < 3)
    {
      continue;
    }

    JsonDocument tempDeviceDoc;
    JsonObject deviceObj = tempDeviceDoc.to<JsonObject>();
    if (configManager->readDevice(deviceId, deviceObj))
    {
      const char *protocol = deviceObj["protocol"] | "";
      if (strcmp(protocol, "TCP") == 0)
      {
        TcpDeviceConfig newDeviceEntry;
        newDeviceEntry.deviceId = deviceId;  // PSRAMString accepts const char*
        newDeviceEntry.doc = std::make_unique<JsonDocument>(); // FIXED Bug #2: Use smart pointer
        newDeviceEntry.doc->set(deviceObj);
        tcpDevices.push_back(std::move(newDeviceEntry));
      }
    }
  }
  LOG_TCP_INFO("[TCP Task] Found %d TCP devices. Schedule rebuilt.\n", tcpDevices.size());

  // Initialize device tracking after device list is loaded
  initializeDeviceFailureTracking();
  initializeDeviceTimeouts();
  initializeDeviceMetrics();

  // FIXED ISSUE #1: Release vector mutex
  xSemaphoreGiveRecursive(vectorMutex);
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

    // v2.5.39: Check BOTH atomic flag AND task notification for reliable config change detection
    // Atomic flag catches changes when task is blocked in TCP operations
    bool shouldRefresh = configChangePending.exchange(false);
    if (shouldRefresh || ulTaskNotifyTake(pdTRUE, 0) > 0)
    {
      LOG_TCP_INFO("[TCP] Config change detected - refreshing device list...\n");
      refreshDeviceList();
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
      LOG_TCP_INFO("[TCP] No network available (Ethernet and WiFi both disabled/disconnected)");
      vTaskDelay(pdMS_TO_TICKS(5000)); // Wait 5 seconds before retrying
      continue;
    }

    // FIXED ISSUE #3: Use cached tcpDevices vector instead of parsing JSON from ConfigManager
    // This eliminates redundant file system access and JSON parsing every iteration
    // Performance improvement: No file reads in polling loop

    // FIXED ISSUE #1: Protect vector access with mutex
    xSemaphoreTakeRecursive(vectorMutex, portMAX_DELAY);
    int tcpDeviceCount = tcpDevices.size();
    xSemaphoreGiveRecursive(vectorMutex);

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
        LOG_TCP_INFO("[TCP] Using Ethernet for Modbus TCP polling (%d devices)\n", tcpDeviceCount);
      }
      else if (hasWiFi)
      {
        LOG_TCP_INFO("[TCP] Using WiFi for Modbus TCP polling (%d devices)\n", tcpDeviceCount);
      }
      lastHasEthernet = hasEthernet;
      lastDeviceCount = tcpDeviceCount;
      firstRun = false;
    }

    unsigned long currentTime = millis();

    // FIXED ISSUE #5 (REVISED): Non-blocking millis-based timing per device
    // Loop runs continuously with SHORT delay, shouldPollDevice() handles per-device timing
    // This allows independent refresh rates without blocking other devices

    // FIXED ISSUE #3: Use cached tcpDevices vector (eliminates file system access)
    // FIXED ISSUE #1: Protect vector iteration with mutex
    xSemaphoreTakeRecursive(vectorMutex, portMAX_DELAY);

    for (auto &deviceEntry : tcpDevices)
    {
      if (!running)
      {
        xSemaphoreGiveRecursive(vectorMutex);
        break; // Exit if stopped
      }

      // v2.5.39: Check for config changes during iteration using BOTH atomic flag AND task notification
      // This catches config changes that occur while task is blocked in TCP connect/read operations
      bool midLoopRefresh = configChangePending.exchange(false);
      if (midLoopRefresh || ulTaskNotifyTake(pdTRUE, 0) > 0)
      {
        LOG_TCP_INFO("[TCP] Config change during polling - refreshing immediately...\n");
        xSemaphoreGiveRecursive(vectorMutex);
        refreshDeviceList();
        break; // Exit current iteration, next iteration will use updated device list
      }

      // Use cached device data from tcpDevices vector
      // v2.5.41: Use const char* instead of String (matches RTU service pattern)
      const char *deviceId = deviceEntry.deviceId.c_str();
      JsonObject deviceObj = deviceEntry.doc->as<JsonObject>();

      // Get device refresh rate from BLE config
      uint32_t deviceRefreshRate = deviceObj["refresh_rate_ms"] | 5000;

      // Check if device's refresh interval has elapsed (millis-based, non-blocking)
      // shouldPollDevice() uses per-device lastRead timestamp for accurate timing
      if (shouldPollDevice(deviceId, deviceRefreshRate))
      {
        // CRITICAL FIX: Check if device is enabled before polling
        if (!isDeviceEnabled(deviceId))
        {
          // Device is disabled, skip polling
          static LogThrottle disabledThrottle(30000); // Log every 30s to reduce spam
          char contextMsg[64];
          snprintf(contextMsg, sizeof(contextMsg), "TCP Device %s disabled", deviceId);
          if (disabledThrottle.shouldLog(contextMsg))
          {
            LOG_TCP_INFO("Device %s is disabled, skipping read\n", deviceId);
          }
          continue; // Skip to next device
        }

        readTcpDeviceData(deviceObj);
        // Device-level timing is updated inside readTcpDeviceData via updateDeviceLastRead()
      }

      // Feed watchdog between device reads
      vTaskDelay(pdMS_TO_TICKS(10));
    }

    // FIXED ISSUE #1: Release vector mutex
    xSemaphoreGiveRecursive(vectorMutex);

    // DRAM FIX (v2.3.9): Aggressive cleanup interval (15s instead of 30s)
    static unsigned long lastCleanup = 0;
    if (millis() - lastCleanup > 15000)
    { // Cleanup every 15 seconds (includes DRAM emergency check)
      closeIdleConnections();
      lastCleanup = millis();
    }

    // FIXED ISSUE #5 (REVISED): Constant short delay (non-blocking approach)
    // Loop runs every 100ms to check if any device is ready
    // Per-device timing handled by shouldPollDevice() with millis()
    // This allows accurate, independent timing for each device without blocking
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  
  // CRITICAL FIX: Task must self-delete when loop exits to prevent FreeRTOS abort
  LOG_TCP_INFO("[TCP] Task loop exited, self-deleting...");
  tcpTaskHandle = nullptr; // Clear handle before deletion
  vTaskDelete(NULL); // Delete self (NULL = current task)
}

void ModbusTcpService::readTcpDeviceData(const JsonObject &deviceConfig)
{
  // v2.5.41: Use const char* instead of String (matches RTU service pattern)
  const char *deviceId = deviceConfig["device_id"] | "UNKNOWN";
  const char *ip = deviceConfig["ip"] | "";
  int port = deviceConfig["port"] | 502;
  uint8_t slaveId = deviceConfig["slave_id"] | 1;
  JsonArray registers = deviceConfig["registers"];

  // FIXED BUG #8: Validate IP address format before use
  // Previous code only checked isEmpty() → invalid IPs like "999.999.999.999" passed!
  if (!ip || strlen(ip) == 0 || registers.size() == 0)
  {
    return;
  }

  // Validate IP address format using IPAddress::fromString()
  IPAddress validatedIP;
  if (!validatedIP.fromString(ip))
  {
    static unsigned long lastWarning = 0;
    if (millis() - lastWarning > 30000) // Log max once per 30s per device
    {
      LOG_TCP_INFO("[TCP] ERROR: Invalid IP address format for device %s: '%s'\n", deviceId, ip);
      LOG_TCP_INFO("[TCP] HINT: IP must be in format A.B.C.D where 0 <= A,B,C,D <= 255");
      lastWarning = millis();
    }
    return; // Skip device with invalid IP
  }

  // Determine network type for log
  const char *networkType = "TCP/WiFi"; // Default assume WiFi
  NetworkMgr *networkMgr = NetworkMgr::getInstance();
  if (networkMgr)
  {
    String currentMode = networkMgr->getCurrentMode();
    if (currentMode == "ETH")
    {
      networkType = "TCP/ETH";
    }
  }

  LOG_TCP_VERBOSE("[%s] Polling device %s at %s:%d\n", networkType, deviceId, ip, port);

  // OPTIMIZED: Get device_name once per device cycle (not per register)
  // v2.5.41: Use const char* instead of String
  const char *deviceName = deviceConfig["device_name"] | "";

  // Track register read results (for End-of-Batch Marker)
  uint8_t successRegisterCount = 0;
  uint8_t failedRegisterCount = 0;

  // COMPACT LOGGING: Buffer all output for atomic printing (prevent interruption by other tasks)
  // v2.5.41: Use PSRAMString instead of String (matches RTU service pattern)
  PSRAMString outputBuffer = "";
  PSRAMString compactLine = "";
  int successCount = 0;
  int lineNumber = 1;

  // Development mode: Collect polled data in JSON format for debugging (always compiled, runtime-checked)
  SpiRamJsonDocument polledDataDoc;
  JsonObject polledData;
  JsonArray polledRegisters;

  if (IS_DEV_MODE())
  {
    polledData = polledDataDoc.to<JsonObject>();
    polledData["device_id"] = deviceId;
    polledData["device_name"] = deviceName;
    polledData["protocol"] = "TCP";
    polledData["slave_id"] = slaveId;
    polledData["ip"] = ip;
    polledData["port"] = port;
    polledData["timestamp"] = millis();
    polledRegisters = polledData["registers"].to<JsonArray>();
  }

  // FIXED ISSUE #2: Get pooled connection ONCE for all registers (eliminates repeated handshakes)
  // Instead of Connect->Read->Disconnect per register, we now Connect->Read all->Disconnect
  TCPClient *pooledClient = getPooledConnection(ip, port);
  bool connectionHealthy = (pooledClient != nullptr && pooledClient->connected());

  if (!connectionHealthy)
  {
    LOG_TCP_INFO("[TCP] ERROR: Failed to get pooled connection for %s:%d\n", ip, port);
    // Continue without pooling (fallback to per-register connections)
    pooledClient = nullptr;
  }

  for (JsonVariant regVar : registers)
  {
    if (!running)
      break;

    JsonObject reg = regVar.as<JsonObject>();
    uint8_t functionCode = reg["function_code"] | 3;
    uint16_t address = reg["address"] | 0;
    // STRING OPTIMIZATION (v2.3.8): Use const char* instead of String to reduce DRAM fragmentation
    const char *registerName = reg["register_name"] | "Unknown";

    if (functionCode == 1 || functionCode == 2)
    {
      // Read coils/discrete inputs
      bool result = false;
      // FIXED ISSUE #2: Pass pooled connection to eliminate per-register TCP handshakes
      if (readModbusCoil(ip, port, slaveId, address, &result, pooledClient))
      {
        float value = result ? 1.0 : 0.0;

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

        // FIXED ISSUE #4: Use helper function to eliminate code duplication (FC 1/2)
        // STRING OPTIMIZATION (v2.3.8): Use const char* instead of String
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
      }
      else
      {
        // v2.5.35: Use DEV_MODE check to prevent log leak in production
        DEV_SERIAL_PRINTF("%s: %s = ERROR\n", deviceId, registerName);
        failedRegisterCount++;

        // FIXED ISSUE #2: Mark connection as unhealthy on read failure
        connectionHealthy = false;
      }
    }
    else
    {
      // Read registers
      // STRING OPTIMIZATION (v2.3.8): Use const char* + manual uppercase (like RTU service does)
      const char *dataType = reg["data_type"] | "UINT16";
      char dataTypeBuf[32];
      strncpy(dataTypeBuf, dataType, sizeof(dataTypeBuf) - 1);
      dataTypeBuf[sizeof(dataTypeBuf) - 1] = '\0';

      // Manual uppercase conversion
      for (int i = 0; dataTypeBuf[i]; i++)
      {
        dataTypeBuf[i] = toupper(dataTypeBuf[i]);
      }

      int registerCount = 1;
      const char *baseType = dataTypeBuf;
      const char *endianness_variant = "";

      // Extract baseType and endianness variant
      char *underscorePos = strchr(dataTypeBuf, '_');
      if (underscorePos != NULL)
      {
        *underscorePos = '\0'; // Split string at underscore
        baseType = dataTypeBuf;
        endianness_variant = underscorePos + 1;
      }

      // Determine register count based on base type
      // v2.5.41: Use strcmp() instead of == for const char* comparison
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
        // STRING OPTIMIZATION (v2.3.8): registerName already const char*, no .c_str() needed
        LOG_TCP_INFO("[TCP] ERROR: Address range overflow for %s (addr=%d, count=%d, max=%d)\n",
                      registerName, address, registerCount, address + registerCount - 1);
        failedRegisterCount++;

        continue; // Skip this register
      }

      uint16_t results[4];
      // FIXED ISSUE #2: Pass pooled connection to eliminate per-register TCP handshakes
      if (readModbusRegisters(ip, port, slaveId, functionCode, address, registerCount, results, pooledClient))
      {
        // STRING OPTIMIZATION (v2.3.8): baseType and endianness_variant already const char*
        double value = (registerCount == 1) ? ModbusUtils::processRegisterValue(reg, results[0]) : ModbusUtils::processMultiRegisterValue(reg, results, registerCount, baseType, endianness_variant);

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

        // FIXED ISSUE #4: Use helper function to eliminate code duplication (FC 3/4)
        // STRING OPTIMIZATION (v2.3.8): Use const char* instead of String
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
      }
      else
      {
        // v2.5.35: Use DEV_MODE check to prevent log leak in production
        DEV_SERIAL_PRINTF("%s: %s = ERROR\n", deviceId, registerName);
        failedRegisterCount++;

        // FIXED ISSUE #2: Mark connection as unhealthy on read failure
        connectionHealthy = false;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(50)); // Small delay between registers
  }

  // FIXED ISSUE #2: Return connection to pool (mark as healthy/unhealthy for reuse decision)
  // If connection was unhealthy, pool will close it. If healthy, pool keeps it for next device.
  if (pooledClient != nullptr)
  {
    returnPooledConnection(ip, port, pooledClient, connectionHealthy);
    LOG_TCP_INFO("[TCP] Returned pooled connection for %s:%d (healthy: %s)\n",
                  ip, port, connectionHealthy ? "YES" : "NO");
  }

  // COMPACT LOGGING: Add remaining items and print buffer atomically
  // v2.5.16: Optimized with snprintf to eliminate heap fragmentation
  if (successCount > 0)
  {
    if (!compactLine.isEmpty())
    {
      // Remove trailing " | "
      if (compactLine.endsWith(" | "))
      {
        compactLine = compactLine.substring(0, compactLine.length() - 3);
      }
      // Use char buffer with snprintf instead of String concatenation
      char lineBuf[256];
      snprintf(lineBuf, sizeof(lineBuf), "  L%d: %s\n", lineNumber, compactLine.c_str());
      outputBuffer += lineBuf;
    }
    // Print all at once (atomic - prevents interruption by other tasks)
    // Use LOG_DATA_DEBUG so it's compiled out in production mode
    LOG_DATA_DEBUG("%s", outputBuffer.c_str());
  }

  // CRITICAL FIX: Update device last read timestamp to respect refresh_rate_ms
  // Without this, shouldPollDevice() always returns true and device is polled every 100ms!
  updateDeviceLastRead(deviceId);

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
      LOG_TCP_DEBUG("Batch marker enqueued for device %s (success=%d, failed=%d)\n",
                    deviceId, successRegisterCount, failedRegisterCount);
    }
    else
    {
      LOG_TCP_WARN("Failed to enqueue batch marker for device %s\n", deviceId);
    }
  }

  // Development mode: Print polled data as JSON (one-line) - runtime check
  if (IS_DEV_MODE() && successRegisterCount > 0)
  {
    polledData["success_count"] = successRegisterCount;
    polledData["failed_count"] = failedRegisterCount;

    Serial.print("\n[TCP] POLLED DATA: ");
    serializeJson(polledDataDoc, Serial);
    Serial.println("\n");
  }
}

// FIXED ISSUE #4: Helper function to eliminate code duplication in register logging
// Consolidates 80+ lines of duplicated code across FC1/2/3/4 blocks
// Pattern matches RTU service for consistency
// v2.5.16: Optimized to use char buffer with snprintf (eliminates heap fragmentation)
// v2.5.41: Changed from String to PSRAMString and const char* for consistency
void ModbusTcpService::appendRegisterToLog(const char *registerName, double value, const char *unit,
                                           const char *deviceId, PSRAMString &outputBuffer,
                                           PSRAMString &compactLine, int &successCount, int &lineNumber)
{
  // v2.5.16: Use fixed-size char buffer instead of String concatenation
  // This eliminates heap fragmentation from repeated String reallocations
  char tempBuf[128];  // Sufficient for register:value.unit format

  // Process unit - replace degree symbol with "deg" to avoid UTF-8 issues
  // v2.5.41: Use PSRAMString for processedUnit
  PSRAMString processedUnit = unit;
  processedUnit.replace("°", "deg");

  // Add header on first success
  if (successCount == 0)
  {
    // Add timestamp from RTC using char buffer
    RTCManager *rtc = RTCManager::getInstance();
    if (rtc)
    {
      DateTime now = rtc->getCurrentTime();
      snprintf(tempBuf, sizeof(tempBuf), "[DATA] [%02d:%02d:%02d] %s:\n",
               now.hour(), now.minute(), now.second(), deviceId);
    }
    else
    {
      snprintf(tempBuf, sizeof(tempBuf), "[DATA] %s:\n", deviceId);
    }
    outputBuffer += tempBuf;
  }

  // Build compact line using snprintf: "RegisterName:value.unit"
  snprintf(tempBuf, sizeof(tempBuf), "%s:%.1f%s",
           registerName, value, processedUnit.c_str());
  compactLine += tempBuf;
  successCount++;

  // Print line every 6 registers
  if (successCount % 6 == 0)
  {
    snprintf(tempBuf, sizeof(tempBuf), "  L%d: %s\n", lineNumber++, compactLine.c_str());
    outputBuffer += tempBuf;
    compactLine = "";
  }
  else
  {
    compactLine += " | ";
  }
}

// NOTE: processRegisterValue() moved to ModbusUtils class (shared with ModbusRtuService)
// See ModbusUtils.cpp for implementation

// NOTE: processMultiRegisterValue() moved to ModbusUtils class (shared with ModbusRtuService)
// See ModbusUtils.cpp for implementation

// v2.5.41: Changed from const String& to const char* for consistency with RTU service
bool ModbusTcpService::readModbusRegister(const char *ip, int port, uint8_t slaveId, uint8_t functionCode, uint16_t address, uint16_t *result, TCPClient *existingClient)
{
  // FIXED ISSUE #2: Activate connection pooling (eliminates repeated TCP handshakes)
  // If existingClient provided, use it. Otherwise fall back to new connection (backward compatible)

  TCPClient *client = nullptr;
  bool shouldCloseConnection = false;

  if (existingClient && existingClient->connected())
  {
    // POOLING ACTIVE: Use existing connection
    client = existingClient;
  }
  else
  {
    // FALLBACK: Create new connection (backward compatibility or pooling disabled)
    client = new TCPClient();
    client->setTimeout(ModbusTcpConfig::TIMEOUT_MS);

    if (!client->connect(ip, port))
    {
      LOG_TCP_INFO("[TCP] Register connection failed to %s:%d\n", ip, port);
      delete client;
      return false;
    }
    shouldCloseConnection = true;
  }

  // Build Modbus TCP request
  uint8_t request[ModbusTcpConfig::MODBUS_TCP_HEADER_SIZE]; // FIXED Bug #10
  uint16_t transId = getNextTransactionId();
  buildModbusRequest(request, transId, slaveId, functionCode, address, 1);

  // Send request
  client->write(request, ModbusTcpConfig::MODBUS_TCP_HEADER_SIZE);

  // FIXED Bug #11: Safe time comparison to handle millis() wraparound
  unsigned long startTime = millis();
  while (client->available() < ModbusTcpConfig::MIN_RESPONSE_SIZE && (millis() - startTime) < ModbusTcpConfig::TIMEOUT_MS)
  {
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  if (client->available() < ModbusTcpConfig::MIN_RESPONSE_SIZE)
  {
    if (shouldCloseConnection)
    {
      client->stop();
      delete client;
    }
    return false;
  }

  // Read response - FIXED Bug #3: Use dynamic buffer with bounds check
  int availableBytes = client->available();

  // Sanity check: Modbus TCP response shouldn't exceed reasonable size
  if (availableBytes > ModbusTcpConfig::MAX_RESPONSE_SIZE)
  {
    LOG_TCP_INFO("[TCP] Response too large (%d bytes), rejecting\n", availableBytes);
    if (shouldCloseConnection)
    {
      client->stop();
      delete client;
    }
    return false;
  }

  std::vector<uint8_t> response(availableBytes);
  int bytesRead = client->readBytes(response.data(), availableBytes);

  // FIXED ISSUE #2: Only close if NOT using pooled connection
  if (shouldCloseConnection)
  {
    client->stop();
    delete client;
  }

  bool dummy = false;
  return parseModbusResponse(response.data(), bytesRead, functionCode, result, &dummy);
}

// v2.5.41: Changed from const String& to const char* for consistency with RTU service
bool ModbusTcpService::readModbusRegisters(const char *ip, int port, uint8_t slaveId, uint8_t functionCode, uint16_t address, int count, uint16_t *results, TCPClient *existingClient)
{
  // FIXED ISSUE #2: Activate connection pooling
  TCPClient *client = nullptr;
  bool shouldCloseConnection = false;

  if (existingClient && existingClient->connected())
  {
    client = existingClient;
  }
  else
  {
    client = new TCPClient();
    client->setTimeout(ModbusTcpConfig::TIMEOUT_MS);

    if (!client->connect(ip, port))
    {
      LOG_TCP_INFO("[TCP] Failed to connect to %s:%d\n", ip, port);
      delete client;
      return false;
    }
    shouldCloseConnection = true;
  }

  // Build Modbus TCP request
  uint8_t request[ModbusTcpConfig::MODBUS_TCP_HEADER_SIZE]; // FIXED Bug #10
  uint16_t transId = getNextTransactionId();
  buildModbusRequest(request, transId, slaveId, functionCode, address, count);

  // Send request
  client->write(request, ModbusTcpConfig::MODBUS_TCP_HEADER_SIZE);

  // FIXED Bug #11: Safe time comparison to handle millis() wraparound
  unsigned long startTime = millis();
  int expectedBytes = ModbusTcpConfig::MIN_RESPONSE_SIZE + (count * 2);
  while (client->available() < expectedBytes && (millis() - startTime) < ModbusTcpConfig::TIMEOUT_MS)
  {
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  if (client->available() < expectedBytes)
  {
    LOG_TCP_INFO("[TCP] Response timeout or incomplete for %s:%d\n", ip, port);
    if (shouldCloseConnection)
    {
      client->stop();
      delete client;
    }
    return false;
  }

  // Read response - FIXED Bug #3: Use dynamic buffer with bounds check
  int availableBytes = client->available();

  // Sanity check: Modbus TCP response shouldn't exceed reasonable size
  if (availableBytes > ModbusTcpConfig::MAX_RESPONSE_SIZE)
  {
    LOG_TCP_INFO("[TCP] Response too large (%d bytes), rejecting\n", availableBytes);
    if (shouldCloseConnection)
    {
      client->stop();
      delete client;
    }
    return false;
  }

  std::vector<uint8_t> response(availableBytes);
  int bytesRead = client->readBytes(response.data(), availableBytes);

  // FIXED ISSUE #2: Only close if NOT using pooled connection
  if (shouldCloseConnection)
  {
    client->stop();
    delete client;
  }

  return parseMultiModbusResponse(response.data(), bytesRead, functionCode, count, results);
}

// v2.5.41: Changed from const String& to const char* for consistency with RTU service
bool ModbusTcpService::readModbusCoil(const char *ip, int port, uint8_t slaveId, uint16_t address, bool *result, TCPClient *existingClient)
{
  // FIXED ISSUE #2: Activate connection pooling (eliminates repeated TCP handshakes)
  TCPClient *client = nullptr;
  bool shouldCloseConnection = false;

  if (existingClient && existingClient->connected())
  {
    // POOLING ACTIVE: Use existing connection
    client = existingClient;
  }
  else
  {
    // FALLBACK: Create new connection (backward compatibility)
    client = new TCPClient();
    client->setTimeout(ModbusTcpConfig::TIMEOUT_MS);

    if (!client->connect(ip, port))
    {
      LOG_TCP_INFO("[TCP] Coil connection failed to %s:%d\n", ip, port);
      delete client;
      return false;
    }
    shouldCloseConnection = true;
  }

  // Build Modbus TCP request for coil
  uint8_t request[ModbusTcpConfig::MODBUS_TCP_HEADER_SIZE]; // FIXED Bug #10
  uint16_t transId = getNextTransactionId();
  buildModbusRequest(request, transId, slaveId, 1, address, 1); // Function code 1 for coils

  // Send request
  client->write(request, ModbusTcpConfig::MODBUS_TCP_HEADER_SIZE); // FIXED Bug #10

  // FIXED Bug #11: Safe time comparison to handle millis() wraparound
  unsigned long startTime = millis();
  while (client->available() < ModbusTcpConfig::MIN_RESPONSE_SIZE && (millis() - startTime) < ModbusTcpConfig::TIMEOUT_MS)
  {
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  if (client->available() < ModbusTcpConfig::MIN_RESPONSE_SIZE) // FIXED Bug #10
  {
    // FIXED ISSUE #2: Only close if NOT using pooled connection
    if (shouldCloseConnection)
    {
      client->stop();
      delete client;
    }
    return false;
  }

  // Read response - FIXED Bug #3: Use dynamic buffer with bounds check
  int availableBytes = client->available();

  // Sanity check: Modbus TCP response shouldn't exceed reasonable size
  if (availableBytes > ModbusTcpConfig::MAX_RESPONSE_SIZE) // FIXED Bug #10
  {
    LOG_TCP_INFO("[TCP] Response too large (%d bytes), rejecting\n", availableBytes);
    // FIXED ISSUE #2: Only close if NOT using pooled connection
    if (shouldCloseConnection)
    {
      client->stop();
      delete client;
    }
    return false;
  }

  std::vector<uint8_t> response(availableBytes);
  int bytesRead = client->readBytes(response.data(), availableBytes);

  // FIXED ISSUE #2: Only close if NOT using pooled connection
  if (shouldCloseConnection)
  {
    client->stop();
    delete client;
  }

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

// v2.5.41: Changed from const String& to const char* for consistency with RTU service
bool ModbusTcpService::storeRegisterValue(const char *deviceId, const JsonObject &reg, double value, const char *deviceName)
{
  QueueManager *queueMgr = QueueManager::getInstance();

  // FIXED Bug #12: Defensive null check for QueueManager
  if (!queueMgr)
  {
    LOG_TCP_INFO("[TCP] ERROR: QueueManager is null, cannot store register value");
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

  // OPTIMIZED: Use pre-fetched device_name (passed from readTcpDeviceData)
  // This avoids expensive getAllDevicesWithRegisters() call per register
  if (deviceName && strlen(deviceName) > 0)
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
    LOG_TCP_ERROR("Failed to enqueue register '%s' for device %s (queue full or memory exhausted)\n",
                  reg["register_name"].as<String>().c_str(), deviceId);

    // Trigger memory diagnostics to identify root cause
    MemoryRecovery::logMemoryStatus("ENQUEUE_FAILED_TCP");

    return false; // Return failure to caller
  }

  // Check if this device is being streamed
  String streamId = "";
  bool crudHandlerAvailable = (crudHandler != nullptr);

  if (crudHandler)
  {
    streamId = crudHandler->getStreamDeviceId();
  }

  // v2.5.41: Compare String with const char* using strcmp
  if (!streamId.isEmpty() && strcmp(streamId.c_str(), deviceId) == 0 && queueMgr)
  {
    // Verbose log suppressed - summary shown in [STREAM] logs
    queueMgr->enqueueStream(dataPoint);
  }

  return true; // Success
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
// v2.5.41: Changed from const String& to const char* for consistency with RTU service
ModbusTcpService::DeviceTimer *ModbusTcpService::getDeviceTimer(const char *deviceId)
{
  for (auto &timer : deviceTimers)
  {
    // v2.5.41: PSRAMString comparison with const char*
    if (strcmp(timer.deviceId.c_str(), deviceId) == 0)
    {
      return &timer;
    }
  }
  return nullptr;
}

// v2.5.41: Changed from const String& to const char* for consistency with RTU service
bool ModbusTcpService::shouldPollDevice(const char *deviceId, uint32_t refreshRateMs)
{
  DeviceTimer *timer = getDeviceTimer(deviceId);
  if (!timer)
  {
    // First time polling this device, create entry
    DeviceTimer newTimer;
    newTimer.deviceId = deviceId;  // PSRAMString accepts const char*
    newTimer.lastRead = 0;
    newTimer.refreshRateMs = refreshRateMs;
    deviceTimers.push_back(newTimer);
    return true; // Always poll on first encounter
  }

  unsigned long now = millis();
  return (now - timer->lastRead) >= timer->refreshRateMs;
}

// v2.5.41: Changed from const String& to const char* for consistency with RTU service
void ModbusTcpService::updateDeviceLastRead(const char *deviceId)
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
  LOG_TCP_INFO("[TCP] Data transmission interval set to: %lums\n", intervalMs);
}

// ============================================
// FIXED BUG #14: TCP CONNECTION POOLING (Phase 1)
// ============================================
// Infrastructure for persistent TCP connections to reduce overhead
//
// Current Implementation:
// - Connection pool with up to 10 concurrent connections
// - Automatic idle connection cleanup (60s timeout)
// - Connection age management (5min max age)
// - Thread-safe pool access with mutex protection
//
// Benefits:
// - Reduces TCP handshake overhead
// - Lower latency for frequently polled devices
// - Automatic resource management
//
// Future Enhancement (Phase 2):
// - Integrate pool usage into readModbusRegister/Registers/Coil functions
// - Requires extensive testing to ensure backward compatibility
// - Current infrastructure is ready for this integration
//
// Performance Impact:
// - Reduces connection setup time from ~100ms to <1ms for pooled connections
// - For 10 devices polled every 5s: saves ~1000ms per cycle (20% improvement)
// ============================================

// v2.5.41: Changed from const String& to const char* and return PSRAMString
PSRAMString ModbusTcpService::getDeviceKey(const char *ip, int port)
{
  char keyBuf[64];
  snprintf(keyBuf, sizeof(keyBuf), "%s:%d", ip, port);
  return PSRAMString(keyBuf);
}

// v2.5.41: Changed from const String& to const char* for consistency with RTU service
TCPClient *ModbusTcpService::getPooledConnection(const char *ip, int port)
{
  if (!poolMutex)
  {
    return nullptr; // Pool not initialized
  }

  PSRAMString deviceKey = getDeviceKey(ip, port);
  TCPClient *client = nullptr;
  unsigned long now = millis();

  // Lock pool for thread-safe access
  if (xSemaphoreTake(poolMutex, pdMS_TO_TICKS(100)) != pdTRUE)
  {
    LOG_TCP_WARN("Pool mutex timeout in getPooledConnection\n");
    return nullptr;
  }

  // Search for existing connection
  for (auto &entry : connectionPool)
  {
    // v2.5.41: PSRAMString comparison
    if (strcmp(entry.deviceKey.c_str(), deviceKey.c_str()) == 0)
    {
      // OPTIMIZED (v2.3.10): Smarter health check - don't rely on connected() alone
      // connected() is unreliable for pooled connections (false positives for idle sockets)
      // Trust isHealthy flag (set by actual read/write success/failure)
      if (entry.client && entry.isHealthy)
      {
        // Check connection age
        if ((now - entry.createdAt) < CONNECTION_MAX_AGE_MS)
        {
          // Connection is good - reuse it
          entry.lastUsed = now;
          entry.useCount++;
          client = entry.client;

          LOG_TCP_DEBUG("Reusing pooled connection to %s (uses: %u, age: %lums)\n",
                        deviceKey.c_str(), entry.useCount, (now - entry.createdAt));
        }
        else
        {
          // Connection too old - close and recreate
          LOG_TCP_INFO("Connection to %s expired (age: %lums), recreating\n",
                       deviceKey.c_str(), (now - entry.createdAt));
          entry.client->stop();
          delete entry.client;
          entry.client = nullptr;
          entry.isHealthy = false;
        }
      }
      else
      {
        // Connection marked unhealthy (failed during actual use) - recreate
        LOG_TCP_INFO("Connection to %s marked unhealthy, recreating\n", deviceKey.c_str());
        if (entry.client)
        {
          entry.client->stop();
          delete entry.client;
          entry.client = nullptr;
        }
        entry.isHealthy = false;
        // CRITICAL FIX: Don't break yet! We need to reuse THIS entry, not create duplicate!
        // Fall through to recreate connection using the SAME entry slot below
      }
      break;
    }
  }

  // FIXED ISSUE #2 (CRITICAL BUG): If no existing connection in pool, CREATE NEW ONE
  // Previous bug: returned nullptr when pool empty, forcing fallback to per-register connections
  if (client == nullptr)
  {
    // No existing connection - create new one
    client = new TCPClient();
    client->setTimeout(ModbusTcpConfig::TIMEOUT_MS);

    if (!client->connect(ip, port))
    {
      LOG_TCP_INFO("[TCP] Failed to connect to %s:%d for pooling\n", ip, port);
      delete client;
      xSemaphoreGive(poolMutex);
      return nullptr;
    }

    // CRITICAL FIX: Check if we have an existing entry with nullptr client (from unhealthy cleanup above)
    // If yes, REUSE that entry instead of creating duplicate!
    bool foundExistingEntry = false;
    for (auto &entry : connectionPool)
    {
      // v2.5.41: PSRAMString comparison
      if (strcmp(entry.deviceKey.c_str(), deviceKey.c_str()) == 0 && entry.client == nullptr)
      {
        // Reuse existing entry slot
        entry.client = client;
        entry.lastUsed = now;
        entry.createdAt = now;
        entry.useCount = 1;
        entry.isHealthy = true;
        foundExistingEntry = true;
        LOG_TCP_INFO("[TCP] Recreated connection for %s (reused pool entry)\n", deviceKey.c_str());
        break;
      }
    }

    // If no existing entry found, add new one
    if (!foundExistingEntry)
    {
      // DRAM FIX (v2.3.9): NEVER create temporary connections - always force cleanup oldest!
      if (connectionPool.size() >= MAX_POOL_SIZE)
      {
        // Pool full - FORCE cleanup oldest connection BEFORE adding new one
        LOG_TCP_INFO("[TCP] Pool full (%d), force cleanup oldest connection before adding %s\n",
                      MAX_POOL_SIZE, deviceKey.c_str());

        unsigned long oldestTime = now;
        size_t oldestIdx = 0;
        for (size_t i = 0; i < connectionPool.size(); i++)
        {
          if (connectionPool[i].lastUsed < oldestTime)
          {
            oldestTime = connectionPool[i].lastUsed;
            oldestIdx = i;
          }
        }

        // Close and delete oldest connection
        if (connectionPool[oldestIdx].client)
        {
          connectionPool[oldestIdx].client->stop();
          delete connectionPool[oldestIdx].client;
          connectionPool[oldestIdx].client = nullptr;
        }
        connectionPool.erase(connectionPool.begin() + oldestIdx);
        LOG_TCP_INFO("[TCP] Cleaned up oldest connection (freed DRAM)\n");
      }

      // Now add new connection (pool has space)
      ConnectionPoolEntry newEntry;
      newEntry.deviceKey = deviceKey;
      newEntry.client = client;
      newEntry.lastUsed = now;
      newEntry.createdAt = now;
      newEntry.useCount = 1;
      newEntry.isHealthy = true;
      connectionPool.push_back(newEntry);

      LOG_TCP_INFO("[TCP] Created new pooled connection to %s (pool size: %d/%d)\n",
                    deviceKey.c_str(), connectionPool.size(), MAX_POOL_SIZE);
    }
  }

  xSemaphoreGive(poolMutex);
  return client;
}

// v2.5.41: Changed from const String& to const char* for consistency with RTU service
void ModbusTcpService::returnPooledConnection(const char *ip, int port, TCPClient *client, bool healthy)
{
  if (!poolMutex || !client)
  {
    return;
  }

  PSRAMString deviceKey = getDeviceKey(ip, port);
  unsigned long now = millis();

  if (xSemaphoreTake(poolMutex, pdMS_TO_TICKS(100)) != pdTRUE)
  {
    LOG_TCP_WARN("Pool mutex timeout in returnPooledConnection\n");
    return;
  }

  // Find existing entry or create new one
  bool found = false;
  for (auto &entry : connectionPool)
  {
    // v2.5.41: PSRAMString comparison
    if (strcmp(entry.deviceKey.c_str(), deviceKey.c_str()) == 0)
    {
      entry.lastUsed = now;
      entry.isHealthy = healthy;
      found = true;

      if (!healthy)
      {
        LOG_TCP_WARN("Connection to %s marked as unhealthy\n", deviceKey.c_str());
        if (entry.client)
        {
          entry.client->stop();
          delete entry.client;
          entry.client = nullptr;
        }
      }
      break;
    }
  }

  if (!found && healthy)
  {
    // New connection - add to pool if not full
    if (connectionPool.size() < MAX_POOL_SIZE)
    {
      ConnectionPoolEntry newEntry;
      newEntry.deviceKey = deviceKey;
      newEntry.client = client;
      newEntry.lastUsed = now;
      newEntry.createdAt = now;
      newEntry.useCount = 1;
      newEntry.isHealthy = true;
      connectionPool.push_back(newEntry);

      LOG_TCP_INFO("Added new connection to pool: %s (pool size: %d)\n",
                   deviceKey.c_str(), connectionPool.size());
    }
    else
    {
      // Pool full - close oldest connection
      LOG_TCP_WARN("Connection pool full (%d), closing oldest connection\n", MAX_POOL_SIZE);

      unsigned long oldestTime = now;
      size_t oldestIdx = 0;
      for (size_t i = 0; i < connectionPool.size(); i++)
      {
        if (connectionPool[i].lastUsed < oldestTime)
        {
          oldestTime = connectionPool[i].lastUsed;
          oldestIdx = i;
        }
      }

      if (connectionPool[oldestIdx].client)
      {
        connectionPool[oldestIdx].client->stop();
        delete connectionPool[oldestIdx].client;
      }
      connectionPool.erase(connectionPool.begin() + oldestIdx);

      // Add new connection
      ConnectionPoolEntry newEntry;
      newEntry.deviceKey = deviceKey;
      newEntry.client = client;
      newEntry.lastUsed = now;
      newEntry.createdAt = now;
      newEntry.useCount = 1;
      newEntry.isHealthy = true;
      connectionPool.push_back(newEntry);

      LOG_TCP_INFO("Replaced oldest connection with %s\n", deviceKey.c_str());
    }
  }

  xSemaphoreGive(poolMutex);
}

void ModbusTcpService::closeIdleConnections()
{
  if (!poolMutex)
  {
    return;
  }

  unsigned long now = millis();

  if (xSemaphoreTake(poolMutex, pdMS_TO_TICKS(100)) != pdTRUE)
  {
    return;
  }

  // DRAM FIX (v2.3.9): Aggressive cleanup - check DRAM first!
  size_t dramFree = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  bool emergencyMode = (dramFree < 50000); // Emergency if DRAM < 50KB

  if (emergencyMode)
  {
    LOG_TCP_INFO("[TCP] EMERGENCY DRAM CLEANUP! Free DRAM: %d bytes, closing ALL connections\n", dramFree);
    // Emergency: close ALL connections to free DRAM immediately
    for (auto &entry : connectionPool)
    {
      if (entry.client)
      {
        entry.client->stop();
        delete entry.client;
        entry.client = nullptr;
      }
    }
    connectionPool.clear();
    LOG_TCP_INFO("[TCP] Emergency cleanup: ALL connections closed (pool cleared)\n");
    xSemaphoreGive(poolMutex);
    return;
  }

  // Normal cleanup: Remove idle connections
  for (auto it = connectionPool.begin(); it != connectionPool.end();)
  {
    unsigned long idleTime = now - it->lastUsed;
    if (idleTime > CONNECTION_IDLE_TIMEOUT_MS || !it->isHealthy)
    {
      LOG_TCP_INFO("Closing idle/unhealthy connection to %s (idle: %lums)\n",
                   it->deviceKey.c_str(), idleTime);

      if (it->client)
      {
        it->client->stop();
        delete it->client;
      }
      it = connectionPool.erase(it);
    }
    else
    {
      ++it;
    }
  }

  xSemaphoreGive(poolMutex);
}

void ModbusTcpService::closeAllConnections()
{
  if (!poolMutex)
  {
    return;
  }

  if (xSemaphoreTake(poolMutex, pdMS_TO_TICKS(1000)) != pdTRUE)
  {
    return;
  }

  LOG_TCP_INFO("Closing all pooled connections (%d)\n", connectionPool.size());

  for (auto &entry : connectionPool)
  {
    if (entry.client)
    {
      entry.client->stop();
      delete entry.client;
      entry.client = nullptr;
    }
  }
  connectionPool.clear();

  xSemaphoreGive(poolMutex);
}

// ============================================
// NEW: Enhancement - Device Failure and Metrics Management
// ============================================

void ModbusTcpService::initializeDeviceFailureTracking()
{
  deviceFailureStates.clear();

  for (const auto &device : tcpDevices)
  {
    DeviceFailureState state;
    state.deviceId = device.deviceId;
    state.consecutiveFailures = 0;
    state.retryCount = 0;
    state.nextRetryTime = 0;
    state.lastReadAttempt = 0;
    state.lastSuccessfulRead = 0;
    state.isEnabled = true;
    state.maxRetries = 5;
    state.disableReason = DeviceFailureState::NONE;
    state.disableReasonDetail = "";
    state.disabledTimestamp = 0;
    deviceFailureStates.push_back(state);
  }
  // Concise summary log
  LOG_TCP_INFO("[TCP] Failure tracking init: %d devices\n", deviceFailureStates.size());
}

void ModbusTcpService::initializeDeviceTimeouts()
{
  deviceTimeouts.clear();

  for (const auto &device : tcpDevices)
  {
    DeviceReadTimeout timeout;
    timeout.deviceId = device.deviceId;
    timeout.timeoutMs = 5000;
    timeout.consecutiveTimeouts = 0;
    timeout.lastSuccessfulRead = millis();
    timeout.maxConsecutiveTimeouts = 3;
    deviceTimeouts.push_back(timeout);
  }
  // Concise summary log
  LOG_TCP_INFO("[TCP] Timeout tracking init: %d devices\n", deviceTimeouts.size());
}

void ModbusTcpService::initializeDeviceMetrics()
{
  deviceMetrics.clear();

  for (const auto &device : tcpDevices)
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
  LOG_TCP_INFO("[TCP] Metrics tracking init: %d devices\n", deviceMetrics.size());
}

// v2.5.41: Changed from const String& to const char* for consistency with RTU service
ModbusTcpService::DeviceFailureState *ModbusTcpService::getDeviceFailureState(const char *deviceId)
{
  for (auto &state : deviceFailureStates)
  {
    // v2.5.41: PSRAMString comparison with const char*
    if (strcmp(state.deviceId.c_str(), deviceId) == 0)
    {
      return &state;
    }
  }
  return nullptr;
}

// v2.5.41: Changed from const String& to const char* for consistency with RTU service
ModbusTcpService::DeviceReadTimeout *ModbusTcpService::getDeviceTimeout(const char *deviceId)
{
  for (auto &timeout : deviceTimeouts)
  {
    // v2.5.41: PSRAMString comparison with const char*
    if (strcmp(timeout.deviceId.c_str(), deviceId) == 0)
    {
      return &timeout;
    }
  }
  return nullptr;
}

// v2.5.41: Changed from const String& to const char* for consistency with RTU service
ModbusTcpService::DeviceHealthMetrics *ModbusTcpService::getDeviceMetrics(const char *deviceId)
{
  for (auto &metrics : deviceMetrics)
  {
    // v2.5.41: PSRAMString comparison with const char*
    if (strcmp(metrics.deviceId.c_str(), deviceId) == 0)
    {
      return &metrics;
    }
  }
  return nullptr;
}

// v2.5.41: Changed from const String& to const char* for consistency with RTU service
void ModbusTcpService::enableDevice(const char *deviceId, bool clearMetrics)
{
  DeviceFailureState *state = getDeviceFailureState(deviceId);
  if (!state)
    return;

  state->isEnabled = true;
  state->disableReason = DeviceFailureState::NONE;
  state->disableReasonDetail = "";
  state->disabledTimestamp = 0;
  resetDeviceFailureState(deviceId);

  DeviceReadTimeout *timeout = getDeviceTimeout(deviceId);
  if (timeout)
  {
    timeout->consecutiveTimeouts = 0;
    timeout->lastSuccessfulRead = millis();
  }

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
      LOG_TCP_INFO("[TCP] Device %s metrics cleared\n", deviceId);
    }
  }

  LOG_TCP_INFO("[TCP] Device %s enabled (reason cleared)\n", deviceId);
}

// v2.5.41: Changed from const String& to const char* for consistency with RTU service
void ModbusTcpService::disableDevice(const char *deviceId, DeviceFailureState::DisableReason reason, const char *reasonDetail)
{
  DeviceFailureState *state = getDeviceFailureState(deviceId);
  if (!state)
    return;

  state->isEnabled = false;
  state->disableReason = reason;
  state->disableReasonDetail = reasonDetail ? reasonDetail : "";
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

  LOG_TCP_INFO("[TCP] Device %s disabled (reason: %s, detail: %s)\n",
                deviceId, reasonText, reasonDetail ? reasonDetail : "");
}

// v2.5.41: Changed from const String& to const char* for consistency with RTU service
void ModbusTcpService::handleReadFailure(const char *deviceId)
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

    LOG_TCP_INFO("[TCP] Device %s read failed. Retry %d/%d in %lums\n",
                  deviceId, state->retryCount, state->maxRetries, backoffTime);
  }
  else
  {
    LOG_TCP_INFO("[TCP] Device %s exceeded max retries (%d), disabling...\n",
                  deviceId, state->maxRetries);
    disableDevice(deviceId, DeviceFailureState::AUTO_RETRY, "Max retries exceeded");
  }
}

// v2.5.41: Changed from const String& to const char* for consistency with RTU service
bool ModbusTcpService::shouldRetryDevice(const char *deviceId)
{
  DeviceFailureState *state = getDeviceFailureState(deviceId);
  if (!state || !state->isEnabled)
    return false;
  if (state->retryCount == 0)
    return false;

  return millis() >= state->nextRetryTime;
}

unsigned long ModbusTcpService::calculateBackoffTime(uint8_t retryCount)
{
  unsigned long baseDelay = 2000;
  unsigned long backoff = baseDelay * (1 << (retryCount - 1));
  unsigned long maxBackoff = 32000;
  backoff = min(backoff, maxBackoff);

  unsigned long jitter = random(0, backoff / 4);
  return backoff + jitter;
}

// v2.5.41: Changed from const String& to const char* for consistency with RTU service
void ModbusTcpService::resetDeviceFailureState(const char *deviceId)
{
  DeviceFailureState *state = getDeviceFailureState(deviceId);
  if (!state)
    return;

  state->consecutiveFailures = 0;
  state->retryCount = 0;
  state->nextRetryTime = 0;
}

// v2.5.41: Changed from const String& to const char* for consistency with RTU service
void ModbusTcpService::handleReadTimeout(const char *deviceId)
{
  DeviceReadTimeout *timeout = getDeviceTimeout(deviceId);
  if (!timeout)
    return;

  timeout->consecutiveTimeouts++;

  if (timeout->consecutiveTimeouts >= timeout->maxConsecutiveTimeouts)
  {
    LOG_TCP_INFO("[TCP] Device %s exceeded timeout limit (%d), disabling...\n",
                  deviceId, timeout->maxConsecutiveTimeouts);
    disableDevice(deviceId, DeviceFailureState::AUTO_TIMEOUT, "Max consecutive timeouts exceeded");
  }
  else
  {
    LOG_TCP_INFO("[TCP] Device %s timeout %d/%d\n",
                  deviceId, timeout->consecutiveTimeouts, timeout->maxConsecutiveTimeouts);
  }
}

// v2.5.41: Changed from const String& to const char* for consistency with RTU service
bool ModbusTcpService::isDeviceEnabled(const char *deviceId)
{
  DeviceFailureState *state = getDeviceFailureState(deviceId);
  if (!state)
    return true;
  return state->isEnabled;
}

// ============================================
// NEW: Enhancement - Public API for BLE Device Control Commands
// ============================================

// v2.5.41: Changed from const String& to const char* for consistency with RTU service
bool ModbusTcpService::enableDeviceByCommand(const char *deviceId, bool clearMetrics)
{
  LOG_TCP_INFO("[TCP] BLE Command: Enable device %s (clearMetrics: %s)\n",
                deviceId, clearMetrics ? "true" : "false");

  DeviceFailureState *state = getDeviceFailureState(deviceId);
  if (!state)
  {
    LOG_TCP_INFO("[TCP] ERROR: Device %s not found\n", deviceId);
    return false;
  }

  enableDevice(deviceId, clearMetrics);
  return true;
}

// v2.5.41: Changed from const String& to const char* for consistency with RTU service
bool ModbusTcpService::disableDeviceByCommand(const char *deviceId, const char *reasonDetail)
{
  LOG_TCP_INFO("[TCP] BLE Command: Disable device %s (reason: %s)\n",
                deviceId, reasonDetail ? reasonDetail : "");

  DeviceFailureState *state = getDeviceFailureState(deviceId);
  if (!state)
  {
    LOG_TCP_INFO("[TCP] ERROR: Device %s not found\n", deviceId);
    return false;
  }

  disableDevice(deviceId, DeviceFailureState::MANUAL, reasonDetail);
  return true;
}

// v2.5.41: Changed from const String& to const char* for consistency with RTU service
bool ModbusTcpService::getDeviceStatusInfo(const char *deviceId, JsonObject &statusInfo)
{
  DeviceFailureState *state = getDeviceFailureState(deviceId);
  if (!state)
  {
    LOG_TCP_INFO("[TCP] ERROR: Device %s not found\n", deviceId);
    return false;
  }

  DeviceReadTimeout *timeout = getDeviceTimeout(deviceId);
  DeviceHealthMetrics *metrics = getDeviceMetrics(deviceId);

  statusInfo["device_id"] = deviceId;
  statusInfo["enabled"] = state->isEnabled;
  statusInfo["consecutive_failures"] = state->consecutiveFailures;
  statusInfo["retry_count"] = state->retryCount;

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
  statusInfo["disable_reason_detail"] = state->disableReasonDetail;

  if (state->disabledTimestamp > 0)
  {
    unsigned long disabledDuration = millis() - state->disabledTimestamp;
    statusInfo["disabled_duration_ms"] = disabledDuration;
  }

  if (timeout)
  {
    statusInfo["timeout_ms"] = timeout->timeoutMs;
    statusInfo["consecutive_timeouts"] = timeout->consecutiveTimeouts;
    statusInfo["max_consecutive_timeouts"] = timeout->maxConsecutiveTimeouts;
  }

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

bool ModbusTcpService::getAllDevicesStatus(JsonObject &allStatus)
{
  JsonArray devicesArray = allStatus["devices"].to<JsonArray>();

  for (const auto &device : tcpDevices)
  {
    JsonObject deviceStatus = devicesArray.add<JsonObject>();
    // v2.5.41: Pass PSRAMString as const char*
    getDeviceStatusInfo(device.deviceId.c_str(), deviceStatus);
  }

  allStatus["total_devices"] = tcpDevices.size();
  return true;
}

// ============================================
// NEW: Enhancement - Auto-Recovery Task
// ============================================

void ModbusTcpService::autoRecoveryTask(void *parameter)
{
  ModbusTcpService *service = static_cast<ModbusTcpService *>(parameter);
  service->autoRecoveryLoop();
}

void ModbusTcpService::autoRecoveryLoop()
{
  LOG_TCP_INFO("[TCP AutoRecovery] Task started");
  const unsigned long RECOVERY_INTERVAL_MS = 300000; // 5 minutes

  while (running)
  {
    vTaskDelay(pdMS_TO_TICKS(RECOVERY_INTERVAL_MS));

    if (!running)
      break;

    LOG_TCP_INFO("[TCP AutoRecovery] Checking for auto-disabled devices...");
    unsigned long now = millis();

    for (auto &state : deviceFailureStates)
    {
      if (!state.isEnabled &&
          (state.disableReason == DeviceFailureState::AUTO_RETRY ||
           state.disableReason == DeviceFailureState::AUTO_TIMEOUT))
      {
        unsigned long disabledDuration = now - state.disabledTimestamp;
        LOG_TCP_INFO("[TCP AutoRecovery] Device %s auto-disabled for %lu ms, attempting recovery...\n",
                      state.deviceId.c_str(), disabledDuration);

        // v2.5.41: Pass PSRAMString as const char*
        enableDevice(state.deviceId.c_str(), false);
        LOG_TCP_INFO("[TCP AutoRecovery] Device %s re-enabled\n", state.deviceId.c_str());
      }
    }
  }

  // CRITICAL FIX: Task must self-delete when loop exits to prevent FreeRTOS abort
  LOG_TCP_INFO("[TCP AutoRecovery] Task loop exited, self-deleting...");
  autoRecoveryTaskHandle = nullptr; // Clear handle before deletion
  vTaskDelete(NULL); // Delete self (NULL = current task)
}

// v2.5.35: Get aggregated Modbus stats for ProductionLogger
void ModbusTcpService::getAggregatedStats(uint32_t &totalSuccess, uint32_t &totalFailed)
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

ModbusTcpService::~ModbusTcpService()
{
  stop();

  // FIXED BUG #14: Clean up connection pool
  closeAllConnections();

  if (poolMutex)
  {
    vSemaphoreDelete(poolMutex);
    poolMutex = nullptr;
  }

  // FIXED ISSUE #1: Clean up vector mutex
  if (vectorMutex)
  {
    vSemaphoreDelete(vectorMutex);
    vectorMutex = nullptr;
  }
}
