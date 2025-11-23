#include "CRUDHandler.h"
#include "BLEManager.h"
#include "QueueManager.h"
#include "ModbusRtuService.h"
#include "ModbusTcpService.h"
#include "MqttManager.h"   //For calling updateDataTransmissionInterval()
#include "HttpManager.h"   //For calling updateDataTransmissionInterval()
#include "MemoryManager.h" // For make_psram_unique
#include "MemoryRecovery.h" // For triggerCleanup() - memory optimization
#include "RTCManager.h"    // For RTC timestamp in factory reset
#include "LEDManager.h"    // For stopping LED task during factory reset

// Make service pointers available to the handler
extern ModbusRtuService *modbusRtuService;
extern ModbusTcpService *modbusTcpService;

CRUDHandler::CRUDHandler(ConfigManager *config, ServerConfig *serverCfg, LoggingConfig *loggingCfg)
    : configManager(config), serverConfig(serverCfg), loggingConfig(loggingCfg),
      mqttManager(nullptr), httpManager(nullptr),
      modbusRtuService(nullptr), modbusTcpService(nullptr),
      streamDeviceId(""), commandIdCounter(0)
{
  setupCommandHandlers();

  // Initialize priority queue mutex
  queueMutex = xSemaphoreCreateMutex();
  if (!queueMutex)
  {
    Serial.println("[CRUD] ERROR: Failed to create queue mutex");
  }

  // Create command processor task
  // FIXED BUG #30: Increased stack size for large device operations
  xTaskCreatePinnedToCore(
      commandProcessorTask,
      "CRUD_PROCESSOR_TASK",
      CRUDConfig::CRUD_TASK_STACK_SIZE,  // 24KB stack (was 8KB)
      this,
      2, // Medium priority
      &commandProcessorTaskHandle,
      1); // Pin to Core 1

  Serial.println("[CRUD] Batch operations and priority queue initialized");
}

// Destructor
CRUDHandler::~CRUDHandler()
{
  if (commandProcessorTaskHandle)
  {
    vTaskDelete(commandProcessorTaskHandle);
    commandProcessorTaskHandle = nullptr;
  }

  if (queueMutex)
  {
    vSemaphoreDelete(queueMutex);
  }

  Serial.println("[CRUD] CRUDHandler destroyed");
}

void CRUDHandler::handle(BLEManager *manager, const JsonDocument &command)
{
  String op = command["op"] | "";
  String type = command["type"] | "";

  Serial.printf("[CRUD] Command - op: '%s', type: '%s'\n", op.c_str(), type.c_str());

  // Check if this is a batch operation
  if (op == "batch")
  {
    handleBatchOperation(manager, command);
    return;
  }

  // Get command priority (default: PRIORITY_NORMAL)
  const char *priorityStr = command["priority"] | "normal";
  CommandPriority priority = CommandPriority::PRIORITY_NORMAL;
  if (String(priorityStr) == "high")
  {
    priority = CommandPriority::PRIORITY_HIGH;
  }
  else if (String(priorityStr) == "low")
  {
    priority = CommandPriority::PRIORITY_LOW;
  }

  // Enqueue command for priority processing
  enqueueCommand(manager, command, priority);
}

void CRUDHandler::setupCommandHandlers()
{
  // === READ HANDLERS ===
  readHandlers["devices"] = [this](BLEManager *manager, const JsonDocument &command)
  {
    auto response = make_psram_unique<JsonDocument>();
    (*response)["status"] = "ok";
    JsonArray devices = (*response)["devices"].to<JsonArray>();
    configManager->listDevices(devices);
    manager->sendResponse(*response);
  };

  readHandlers["devices_summary"] = [this](BLEManager *manager, const JsonDocument &command)
  {
    auto response = make_psram_unique<JsonDocument>();
    (*response)["status"] = "ok";
    JsonArray summary = (*response)["devices_summary"].to<JsonArray>();
    configManager->getDevicesSummary(summary);
    manager->sendResponse(*response);
  };

  readHandlers["devices_with_registers"] = [this](BLEManager *manager, const JsonDocument &command)
  {
    bool minimalFields = command["minimal"] | false;  // Support optional "minimal" parameter

    // Start processing timer for performance monitoring
    unsigned long startTime = millis();

    auto response = make_psram_unique<JsonDocument>();
    (*response)["status"] = "ok";
    JsonArray devices = (*response)["devices"].to<JsonArray>();
    configManager->getAllDevicesWithRegisters(devices, minimalFields);

    // Calculate processing time
    unsigned long processingTime = millis() - startTime;

    // Calculate total register count
    int totalRegisters = 0;
    for (JsonObject device : devices)
    {
      totalRegisters += device["registers"].size();
    }

    Serial.printf("[CRUD] devices_with_registers returned %d devices, %d total registers (minimal=%s) in %lu ms\n",
                  devices.size(), totalRegisters, minimalFields ? "true" : "false", processingTime);

    // Warn if no data returned
    if (devices.size() == 0)
    {
      Serial.println("[CRUD] WARNING: No devices returned! Check if devices are configured in devices.json");
    }

    // Warn if processing takes too long (>10 seconds)
    if (processingTime > 10000)
    {
      Serial.printf("[CRUD] WARNING: Processing took %lu ms (>10s). Consider using minimal=true for large datasets.\n", processingTime);
    }

    manager->sendResponse(*response);
  };

  readHandlers["device"] = [this](BLEManager *manager, const JsonDocument &command)
  {
    String deviceId = command["device_id"] | "";
    bool minimal = command["minimal"] | false;  // Support optional "minimal" parameter

    // OPTIMIZATION: Pagination support for device registers
    int regOffset = command["reg_offset"] | -1;  // Register offset (default: -1 = no pagination)
    int regLimit = command["reg_limit"] | -1;    // Register limit (default: -1 = all)
    bool usePagination = (regOffset >= 0 && regLimit > 0);

    // OPTIMIZATION: Check register count for proactive memory cleanup
    // Read device in minimal mode to get register_count field
    JsonDocument deviceCheckDoc;
    JsonObject deviceCheck = deviceCheckDoc.to<JsonObject>();
    if (configManager->readDevice(deviceId, deviceCheck, true))  // Read minimal first
    {
      // Get register count from minimal response (ConfigManager provides this field)
      int registerCount = deviceCheck["register_count"] | 0;

      // Proactive cleanup for devices with > 20 registers
      if (registerCount > 20 && !minimal && !usePagination)
      {
        // Check INTERNAL DRAM only (not PSRAM) - MALLOC_CAP_INTERNAL filters out external PSRAM
        size_t dramBefore = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        Serial.printf("[CRUD] Large device detected (%d regs). Free DRAM: %d bytes\n",
                      registerCount, dramBefore);

        // Trigger memory cleanup if DRAM is below 50KB
        if (dramBefore < 50000)
        {
          Serial.println("[CRUD] Triggering proactive memory cleanup...");
          uint32_t freed = MemoryRecovery::triggerCleanup();
          delay(50);  // Give time for cleanup to complete

          size_t dramAfter = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
          Serial.printf("[CRUD] Memory cleanup complete. Free DRAM: %d bytes (freed %d bytes)\n",
                        dramAfter, freed);
        }
        else
        {
          Serial.printf("[CRUD] DRAM healthy (%d bytes), no cleanup needed\n", dramBefore);
        }
      }
    }

    auto response = make_psram_unique<JsonDocument>();
    (*response)["status"] = "ok";
    JsonObject data = (*response)["data"].to<JsonObject>();

    // Read device data
    if (configManager->readDevice(deviceId, data, minimal))
    {
      // Apply pagination to registers if requested
      if (usePagination && data["registers"].is<JsonArray>())
      {
        JsonArray allRegisters = data["registers"].as<JsonArray>();
        int totalRegisters = allRegisters.size();

        // Create new paginated registers array
        JsonArray paginatedRegs;
        data.remove("registers");  // Remove original array
        paginatedRegs = data["registers"].to<JsonArray>();

        int endIndex = min(regOffset + regLimit, totalRegisters);

        // Copy only requested range
        for (int i = regOffset; i < endIndex; i++)
        {
          paginatedRegs.add(allRegisters[i]);
        }

        // Add pagination metadata
        data["total_registers"] = totalRegisters;
        data["reg_offset"] = regOffset;
        data["reg_limit"] = regLimit;
        data["returned_registers"] = paginatedRegs.size();
        data["has_more_registers"] = (endIndex < totalRegisters);

        Serial.printf("[CRUD] Paginated device read: %d/%d registers (offset=%d, limit=%d)\n",
                      paginatedRegs.size(), totalRegisters, regOffset, regLimit);
      }

      // Log payload size for debugging
      String payload;
      serializeJson(*response, payload);
      Serial.printf("[CRUD] Device read response size: %d bytes (minimal=%s, paginated=%s)\n",
                    payload.length(), minimal ? "true" : "false", usePagination ? "true" : "false");

      manager->sendResponse(*response);
    }
    else
    {
      manager->sendError("Device not found");
    }
  };

  readHandlers["registers"] = [this](BLEManager *manager, const JsonDocument &command)
  {
    String deviceId = command["device_id"] | "";

    // OPTIMIZATION: Pagination support for large register lists
    int offset = command["offset"] | 0;        // Start index (default: 0)
    int limit = command["limit"] | -1;         // Max registers to return (default: all)

    auto response = make_psram_unique<JsonDocument>();
    (*response)["status"] = "ok";
    JsonArray registers = (*response)["registers"].to<JsonArray>();

    // Get all registers first
    JsonDocument tempDoc;
    JsonArray allRegisters = tempDoc.to<JsonArray>();

    if (configManager->listRegisters(deviceId, allRegisters))
    {
      int totalRegisters = allRegisters.size();

      // Apply pagination if limit is specified
      if (limit > 0)
      {
        int endIndex = min(offset + limit, totalRegisters);

        // Copy only the requested range
        for (int i = offset; i < endIndex; i++)
        {
          registers.add(allRegisters[i]);
        }

        // Add pagination metadata
        (*response)["total_registers"] = totalRegisters;
        (*response)["offset"] = offset;
        (*response)["limit"] = limit;
        (*response)["returned_count"] = registers.size();
        (*response)["has_more"] = (endIndex < totalRegisters);

        Serial.printf("[CRUD] Paginated registers read: offset=%d, limit=%d, returned=%d/%d\n",
                      offset, limit, registers.size(), totalRegisters);
      }
      else
      {
        // No pagination - return all registers
        for (JsonVariant reg : allRegisters)
        {
          registers.add(reg);
        }

        Serial.printf("[CRUD] All registers read: %d registers\n", registers.size());
      }

      manager->sendResponse(*response);
    }
    else
    {
      manager->sendError("No registers found");
    }
  };

  readHandlers["registers_summary"] = [this](BLEManager *manager, const JsonDocument &command)
  {
    String deviceId = command["device_id"] | "";
    auto response = make_psram_unique<JsonDocument>();
    (*response)["status"] = "ok";
    JsonArray summary = (*response)["registers_summary"].to<JsonArray>();
    if (configManager->getRegistersSummary(deviceId, summary))
    {
      manager->sendResponse(*response);
    }
    else
    {
      manager->sendError("No registers found");
    }
  };

  readHandlers["server_config"] = [this](BLEManager *manager, const JsonDocument &command)
  {
    auto response = make_psram_unique<JsonDocument>();
    (*response)["status"] = "ok";
    JsonObject serverConfigObj = (*response)["server_config"].to<JsonObject>();
    if (serverConfig->getConfig(serverConfigObj))
    {
      manager->sendResponse(*response);
    }
    else
    {
      manager->sendError("Failed to get server config");
    }
  };

  readHandlers["logging_config"] = [this](BLEManager *manager, const JsonDocument &command)
  {
    auto response = make_psram_unique<JsonDocument>();
    (*response)["status"] = "ok";
    JsonObject loggingConfigObj = (*response)["logging_config"].to<JsonObject>();
    if (loggingConfig->getConfig(loggingConfigObj))
    {
      manager->sendResponse(*response);
    }
    else
    {
      manager->sendError("Failed to get logging config");
    }
  };

  readHandlers["full_config"] = [this](BLEManager *manager, const JsonDocument &command)
  {
    Serial.println("[CRUD] Full config backup requested");
    unsigned long startTime = millis();

    // Allocate large PSRAM document for complete config
    auto response = make_psram_unique<JsonDocument>();
    (*response)["status"] = "ok";

    // Backup metadata
    JsonObject backupInfo = (*response)["backup_info"].to<JsonObject>();
    backupInfo["timestamp"] = millis();
    backupInfo["firmware_version"] = "2.3.4";
    backupInfo["device_name"] = "SURIOTA_GW";

    // Get all configurations
    JsonObject config = (*response)["config"].to<JsonObject>();

    // 1. Devices + registers (complete structure)
    JsonArray devices = config["devices"].to<JsonArray>();
    configManager->getAllDevicesWithRegisters(devices, false);  // Full mode, not minimal

    // 2. Server config (MQTT, HTTP, WiFi, Ethernet)
    JsonObject serverCfg = config["server_config"].to<JsonObject>();
    if (!serverConfig->getConfig(serverCfg))
    {
      Serial.println("[CRUD] WARNING: Failed to get server config");
    }

    // 3. Logging config
    JsonObject loggingCfg = config["logging_config"].to<JsonObject>();
    if (!loggingConfig->getConfig(loggingCfg))
    {
      Serial.println("[CRUD] WARNING: Failed to get logging config");
    }

    // Calculate statistics
    int totalDevices = devices.size();
    int totalRegisters = 0;
    for (JsonObject dev : devices)
    {
      if (dev["registers"].is<JsonArray>())
      {
        totalRegisters += dev["registers"].size();
      }
    }

    backupInfo["total_devices"] = totalDevices;
    backupInfo["total_registers"] = totalRegisters;

    unsigned long processingTime = millis() - startTime;
    backupInfo["processing_time_ms"] = processingTime;

    // Calculate approximate JSON size
    String testOutput;
    serializeJson(*response, testOutput);
    backupInfo["backup_size_bytes"] = testOutput.length();

    Serial.printf("[CRUD] Full config backup complete: %d devices, %d registers, %d bytes, %lu ms\n",
                  totalDevices, totalRegisters, testOutput.length(), processingTime);

    manager->sendResponse(*response);
  };

  readHandlers["data"] = [this](BLEManager *manager, const JsonDocument &command)
  {
    String device = command["device_id"] | "";
    if (device == "stop")
    {
      Serial.println("[CRUD] Stop streaming command received");

      // Set streaming flag to false FIRST to prevent new dequeue operations
      manager->setStreamingActive(false);

      // Clear stream device ID and queue
      streamDeviceId = "";
      QueueManager::getInstance()->clearStream();

      // Brief delay to allow streaming task to see the flag change
      // Streaming task checks isStreamingActive() every 100ms
      Serial.println("[CRUD] Waiting 150ms for streaming task to sync...");
      vTaskDelay(pdMS_TO_TICKS(150));

      // No need to wait for transmissions - transmissionMutex will handle synchronization
      // When we call sendResponse(), it will automatically wait for any in-flight
      // transmission to complete before sending the stop response
      Serial.println("[CRUD] Sending stop response");

      // Simple streaming completion summary
      Serial.println("[STREAM] Stopped");

      auto response = make_psram_unique<JsonDocument>();
      (*response)["status"] = "ok";
      (*response)["message"] = "Data streaming stopped";
      manager->sendResponse(*response);
      Serial.println("[CRUD] Stop response sent");
    }
    else if (!device.isEmpty())
    {
      streamDeviceId = device;

      // Get register count for this device using public API
      int registerCount = 0;
      JsonDocument tempDoc;
      JsonObject deviceObj = tempDoc.to<JsonObject>();
      if (configManager->readDevice(device, deviceObj)) {
        if (deviceObj["registers"].is<JsonArray>()) {
          registerCount = deviceObj["registers"].size();
        }
      }

      // Set streaming flag to true when starting
      manager->setStreamingActive(true);

      // Simple summary log
      Serial.printf("[STREAM] Started: %s (%d registers)\n", device.c_str(), registerCount);

      auto response = make_psram_unique<JsonDocument>();
      (*response)["status"] = "ok";
      (*response)["message"] = "Data streaming started for device: " + device;
      manager->sendResponse(*response);
    }
    else
    {
      manager->sendError("Empty device ID");
    }
  };

  // === CREATE HANDLERS ===
  createHandlers["device"] = [this](BLEManager *manager, const JsonDocument &command)
  {
    JsonObjectConst config = command["config"];
    String deviceId = configManager->createDevice(config);
    if (!deviceId.isEmpty())
    {
      if (modbusRtuService)
        modbusRtuService->notifyConfigChange();
      if (modbusTcpService)
        modbusTcpService->notifyConfigChange();

      // Return created device data
      auto response = make_psram_unique<JsonDocument>();
      (*response)["status"] = "ok";
      (*response)["device_id"] = deviceId;

      JsonObject deviceData = (*response)["data"].to<JsonObject>();
      configManager->readDevice(deviceId, deviceData);

      manager->sendResponse(*response);
    }
    else
    {
      manager->sendError("Device creation failed");
    }
  };

  createHandlers["register"] = [this](BLEManager *manager, const JsonDocument &command)
  {
    String deviceId = command["device_id"] | "";
    JsonObjectConst config = command["config"];
    String registerId = configManager->createRegister(deviceId, config);
    if (!registerId.isEmpty())
    {
      if (modbusRtuService)
        modbusRtuService->notifyConfigChange();
      if (modbusTcpService)
        modbusTcpService->notifyConfigChange();

      // Return created register data
      auto response = make_psram_unique<JsonDocument>();
      (*response)["status"] = "ok";
      (*response)["device_id"] = deviceId;
      (*response)["register_id"] = registerId;

      // Load device and find the created register
      // BUG #31: Global PSRAM allocator handles all JsonDocument instances automatically
      JsonDocument deviceDoc;
      JsonObject device = deviceDoc.to<JsonObject>();
      if (configManager->readDevice(deviceId, device) && device["registers"].is<JsonArray>())
      {
        JsonArray registers = device["registers"];
        for (JsonObject reg : registers)
        {
          if (reg["register_id"] == registerId)
          {
            (*response)["data"] = reg;
            break;
          }
        }
      }

      manager->sendResponse(*response);
    }
    else
    {
      manager->sendError("Register creation failed");
    }
  };

  // === UPDATE HANDLERS ===
  updateHandlers["device"] = [this](BLEManager *manager, const JsonDocument &command)
  {
    String deviceId = command["device_id"] | "";
    JsonObjectConst config = command["config"];
    if (configManager->updateDevice(deviceId, config))
    {
      if (modbusRtuService)
        modbusRtuService->notifyConfigChange();
      if (modbusTcpService)
        modbusTcpService->notifyConfigChange();

      // Return updated device data
      auto response = make_psram_unique<JsonDocument>();
      (*response)["status"] = "ok";
      (*response)["device_id"] = deviceId;
      (*response)["message"] = "Device updated";

      JsonObject deviceData = (*response)["data"].to<JsonObject>();
      configManager->readDevice(deviceId, deviceData);

      manager->sendResponse(*response);
    }
    else
    {
      manager->sendError("Device update failed");
    }
  };

  updateHandlers["register"] = [this](BLEManager *manager, const JsonDocument &command)
  {
    String deviceId = command["device_id"] | "";
    String registerId = command["register_id"] | "";
    JsonObjectConst config = command["config"];
    if (configManager->updateRegister(deviceId, registerId, config))
    {
      if (modbusRtuService)
        modbusRtuService->notifyConfigChange();
      if (modbusTcpService)
        modbusTcpService->notifyConfigChange();

      // Return updated register data
      auto response = make_psram_unique<JsonDocument>();
      (*response)["status"] = "ok";
      (*response)["device_id"] = deviceId;
      (*response)["register_id"] = registerId;
      (*response)["message"] = "Register updated";

      // Load device and find the updated register
      // BUG #31: Global PSRAM allocator handles all JsonDocument instances automatically
      JsonDocument deviceDoc;
      JsonObject device = deviceDoc.to<JsonObject>();
      if (configManager->readDevice(deviceId, device) && device["registers"].is<JsonArray>())
      {
        JsonArray registers = device["registers"];
        for (JsonObject reg : registers)
        {
          if (reg["register_id"] == registerId)
          {
            (*response)["data"] = reg;
            break;
          }
        }
      }

      manager->sendResponse(*response);
    }
    else
    {
      manager->sendError("Register update failed");
    }
  };

  updateHandlers["server_config"] = [this](BLEManager *manager, const JsonDocument &command)
  {
    JsonObjectConst config = command["config"];
    if (serverConfig->updateConfig(config))
    {
      // v2.2.0: Only HTTP Manager needs interval update (MQTT uses mode-specific intervals)
      // Device will restart in 5s after server_config update, MQTT will reload config on restart
      if (httpManager)
      {
        httpManager->updateDataTransmissionInterval();
        Serial.println("[CRUD] HTTP Manager data interval updated");
      }

      auto response = make_psram_unique<JsonDocument>();
      (*response)["status"] = "ok";
      (*response)["message"] = "Server configuration updated. Device will restart in 5s.";
      manager->sendResponse(*response);
    }
    else
    {
      manager->sendError("Server configuration update failed");
    }
  };

  updateHandlers["logging_config"] = [this](BLEManager *manager, const JsonDocument &command)
  {
    JsonObjectConst config = command["config"];
    if (loggingConfig->updateConfig(config))
    {
      auto response = make_psram_unique<JsonDocument>();
      (*response)["status"] = "ok";
      (*response)["message"] = "Logging configuration updated";
      manager->sendResponse(*response);
    }
    else
    {
      manager->sendError("Logging configuration update failed");
    }
  };

  // === DELETE HANDLERS ===
  deleteHandlers["device"] = [this](BLEManager *manager, const JsonDocument &command)
  {
    String deviceId = command["device_id"] | "";

    // Get device data before deletion for response
    auto response = make_psram_unique<JsonDocument>();
    JsonObject deletedData = (*response)["deleted_data"].to<JsonObject>();
    configManager->readDevice(deviceId, deletedData);

    if (configManager->deleteDevice(deviceId))
    {
      if (modbusRtuService)
        modbusRtuService->notifyConfigChange();
      if (modbusTcpService)
        modbusTcpService->notifyConfigChange();

      // Return deleted device data
      (*response)["status"] = "ok";
      (*response)["device_id"] = deviceId;
      (*response)["message"] = "Device deleted";

      manager->sendResponse(*response);
    }
    else
    {
      manager->sendError("Device deletion failed");
    }
  };

  deleteHandlers["register"] = [this](BLEManager *manager, const JsonDocument &command)
  {
    String deviceId = command["device_id"] | "";
    String registerId = command["register_id"] | "";

    // Get register data before deletion for response
    auto response = make_psram_unique<JsonDocument>();

    // Load device and find the register before deletion
    // BUG #31: Global PSRAM allocator handles all JsonDocument instances automatically
    JsonDocument deviceDoc;
    JsonObject device = deviceDoc.to<JsonObject>();
    if (configManager->readDevice(deviceId, device) && device["registers"].is<JsonArray>())
    {
      JsonArray registers = device["registers"];
      for (JsonObject reg : registers)
      {
        if (reg["register_id"] == registerId)
        {
          (*response)["deleted_data"] = reg;
          break;
        }
      }
    }

    if (configManager->deleteRegister(deviceId, registerId))
    {
      if (modbusRtuService)
        modbusRtuService->notifyConfigChange();
      if (modbusTcpService)
        modbusTcpService->notifyConfigChange();

      // Return deleted register data
      (*response)["status"] = "ok";
      (*response)["device_id"] = deviceId;
      (*response)["register_id"] = registerId;
      (*response)["message"] = "Register deleted";

      manager->sendResponse(*response);
    }
    else
    {
      manager->sendError("Register deletion failed");
    }
  };

  // === CONTROL HANDLERS (Device Enable/Disable/Status) ===

  controlHandlers["enable_device"] = [this](BLEManager *manager, const JsonDocument &command)
  {
    String deviceId = command["device_id"] | "";
    bool clearMetrics = command["clear_metrics"] | false;

    auto response = make_psram_unique<JsonDocument>();

    if (deviceId.isEmpty())
    {
      manager->sendError("device_id is required");
      return;
    }

    // Check device protocol to route to correct service
    JsonDocument deviceDoc;
    JsonObject device = deviceDoc.to<JsonObject>();
    if (!configManager->readDevice(deviceId, device))
    {
      manager->sendError("Device not found");
      return;
    }

    String protocol = device["protocol"] | "";
    bool success = false;

    if (protocol == "RTU" && modbusRtuService)
    {
      success = modbusRtuService->enableDeviceByCommand(deviceId.c_str(), clearMetrics);
    }
    else if (protocol == "TCP" && modbusTcpService)
    {
      success = modbusTcpService->enableDeviceByCommand(deviceId, clearMetrics);
    }
    else
    {
      manager->sendError("Invalid protocol or service not available");
      return;
    }

    if (success)
    {
      (*response)["status"] = "ok";
      (*response)["device_id"] = deviceId;
      (*response)["message"] = "Device enabled";
      (*response)["metrics_cleared"] = clearMetrics;
      manager->sendResponse(*response);
    }
    else
    {
      manager->sendError("Failed to enable device");
    }
  };

  controlHandlers["disable_device"] = [this](BLEManager *manager, const JsonDocument &command)
  {
    String deviceId = command["device_id"] | "";
    String reason = command["reason"] | "Manual disable via BLE";

    auto response = make_psram_unique<JsonDocument>();

    if (deviceId.isEmpty())
    {
      manager->sendError("device_id is required");
      return;
    }

    // Check device protocol to route to correct service
    JsonDocument deviceDoc;
    JsonObject device = deviceDoc.to<JsonObject>();
    if (!configManager->readDevice(deviceId, device))
    {
      manager->sendError("Device not found");
      return;
    }

    String protocol = device["protocol"] | "";
    bool success = false;

    if (protocol == "RTU" && modbusRtuService)
    {
      success = modbusRtuService->disableDeviceByCommand(deviceId.c_str(), reason.c_str());
    }
    else if (protocol == "TCP" && modbusTcpService)
    {
      success = modbusTcpService->disableDeviceByCommand(deviceId, reason);
    }
    else
    {
      manager->sendError("Invalid protocol or service not available");
      return;
    }

    if (success)
    {
      (*response)["status"] = "ok";
      (*response)["device_id"] = deviceId;
      (*response)["message"] = "Device disabled";
      (*response)["reason"] = reason;
      manager->sendResponse(*response);
    }
    else
    {
      manager->sendError("Failed to disable device");
    }
  };

  controlHandlers["get_device_status"] = [this](BLEManager *manager, const JsonDocument &command)
  {
    String deviceId = command["device_id"] | "";

    auto response = make_psram_unique<JsonDocument>();

    if (deviceId.isEmpty())
    {
      manager->sendError("device_id is required");
      return;
    }

    // Check device protocol to route to correct service
    JsonDocument deviceDoc;
    JsonObject device = deviceDoc.to<JsonObject>();
    if (!configManager->readDevice(deviceId, device))
    {
      manager->sendError("Device not found");
      return;
    }

    String protocol = device["protocol"] | "";
    bool success = false;

    (*response)["status"] = "ok";
    JsonObject statusInfo = (*response)["device_status"].to<JsonObject>();

    if (protocol == "RTU" && modbusRtuService)
    {
      success = modbusRtuService->getDeviceStatusInfo(deviceId.c_str(), statusInfo);
    }
    else if (protocol == "TCP" && modbusTcpService)
    {
      success = modbusTcpService->getDeviceStatusInfo(deviceId, statusInfo);
    }
    else
    {
      manager->sendError("Invalid protocol or service not available");
      return;
    }

    if (success)
    {
      manager->sendResponse(*response);
    }
    else
    {
      manager->sendError("Failed to get device status");
    }
  };

  controlHandlers["get_all_device_status"] = [this](BLEManager *manager, const JsonDocument &command)
  {
    auto response = make_psram_unique<JsonDocument>();
    (*response)["status"] = "ok";

    // Get RTU devices status
    if (modbusRtuService)
    {
      JsonObject rtuStatus = (*response)["rtu_devices"].to<JsonObject>();
      modbusRtuService->getAllDevicesStatus(rtuStatus);
    }

    // Get TCP devices status
    if (modbusTcpService)
    {
      JsonObject tcpStatus = (*response)["tcp_devices"].to<JsonObject>();
      modbusTcpService->getAllDevicesStatus(tcpStatus);
    }

    manager->sendResponse(*response);
  };

  // === SYSTEM HANDLERS ===

  // Factory Reset - Simple single-command reset
  systemHandlers["factory_reset"] = [this](BLEManager *manager, const JsonDocument &command)
  {
    String reason = command["reason"] | "No reason provided";

    // Get RTC timestamp for audit trail
    RTCManager *rtc = RTCManager::getInstance();
    String timestamp = "Unknown";
    if (rtc)
    {
      DateTime now = rtc->getCurrentTime();
      // Check if RTC time is valid (year >= 2024)
      if (now.year() >= 2024)
      {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
                 now.year(), now.month(), now.day(),
                 now.hour(), now.minute(), now.second());
        timestamp = String(buffer);
      }
      else
      {
        // RTC time invalid, use uptime
        timestamp = String(millis() / 1000) + "s uptime";
      }
    }
    else
    {
      timestamp = String(millis() / 1000) + "s uptime";
    }

    // Serial log audit trail (without emoji)
    Serial.println("");
    Serial.println("========================================");
    Serial.println("[FACTORY RESET] WARNING - INITIATED by BLE client");
    Serial.printf("[FACTORY RESET] Timestamp: %s\n", timestamp.c_str());
    Serial.printf("[FACTORY RESET] Reason: %s\n", reason.c_str());
    Serial.println("[FACTORY RESET] This will ERASE all device, server, and network configurations!");
    Serial.println("========================================");
    Serial.println("");

    // Write to persistent audit log file
    File auditLog = LittleFS.open("/factory_reset_audit.log", "a");
    if (auditLog)
    {
      size_t bytesWritten = auditLog.printf("%s|%s|BLE Client|SUCCESS\n",
                                             timestamp.c_str(),
                                             reason.c_str());
      auditLog.close();

      if (bytesWritten > 0)
      {
        Serial.println("[FACTORY RESET] Audit log written to /factory_reset_audit.log");
      }
      else
      {
        Serial.println("[FACTORY RESET] WARNING: Failed to write to audit log");
      }
    }
    else
    {
      Serial.println("[FACTORY RESET] ERROR: Failed to open audit log file");
    }

    // Send confirmation response BEFORE reset
    auto response = make_psram_unique<JsonDocument>();
    (*response)["status"] = "ok";
    (*response)["message"] = "Factory reset initiated. Device will restart in 3 seconds.";
    JsonArray configsCleared = (*response)["configs_cleared"].to<JsonArray>();
    configsCleared.add("devices.json");
    configsCleared.add("server_config.json");
    configsCleared.add("logging_config.json");
    (*response)["restart_in_ms"] = 3000;

    manager->sendResponse(*response);

    // Wait 500ms for BLE response to be sent
    delay(500);

    // Perform factory reset
    performFactoryReset();
  };

  // Config Restore - Import full configuration from backup
  systemHandlers["restore_config"] = [this](BLEManager *manager, const JsonDocument &command)
  {
    Serial.println("");
    Serial.println("========================================");
    Serial.println("[CONFIG RESTORE] INITIATED by BLE client");
    Serial.println("========================================");

    // BUG #32 DEBUG: Show what's actually in the payload
    #if PRODUCTION_MODE == 0
      Serial.println("[CONFIG RESTORE] DEBUG: Payload analysis:");
      Serial.printf("  - Document is null: %s\n", command.isNull() ? "YES" : "NO");
      Serial.printf("  - Document serialized size: %u bytes\n", measureJson(command));
      Serial.printf("  - Document is object: %s\n", command.is<JsonObject>() ? "YES" : "NO");

      if (command.is<JsonObject>()) {
        JsonObjectConst obj = command.as<JsonObjectConst>();
        Serial.printf("  - Object has %u keys:\n", obj.size());
        for (JsonPairConst kv : obj) {
          Serial.printf("    - Key: '%s', Type: ", kv.key().c_str());
          if (kv.value().is<JsonObject>()) Serial.println("JsonObject");
          else if (kv.value().is<JsonArray>()) Serial.println("JsonArray");
          else if (kv.value().is<const char*>()) Serial.printf("String (\"%s\")\n", kv.value().as<const char*>());
          else if (kv.value().is<int>()) Serial.printf("Int (%d)\n", kv.value().as<int>());
          else Serial.println("Other");
        }
      }

      JsonVariantConst configCheck = command["config"];
      Serial.printf("  - Has 'config' key: %s\n", !configCheck.isNull() ? "YES" : "NO");
      if (!configCheck.isNull()) {
        Serial.printf("  - 'config' is JsonObject: %s\n", command["config"].is<JsonObject>() ? "YES" : "NO");
        Serial.printf("  - 'config' is null: %s\n", command["config"].isNull() ? "YES" : "NO");
      }

      // Show first 500 chars of serialized JSON for comparison
      String serialized;
      serializeJson(command, serialized);
      Serial.printf("  - Serialized JSON (%u bytes):\n", serialized.length());
      if (serialized.length() > 500) {
        Serial.printf("    %s...\n", serialized.substring(0, 500).c_str());
      } else {
        Serial.printf("    %s\n", serialized.c_str());
      }
    #endif

    // BUG #32 FIX: Try accessing config directly instead of type checking
    // Type check may fail even when data is valid (ArduinoJson v7 quirk with PSRAM)
    JsonVariantConst configVariant = command["config"];

    if (configVariant.isNull())
    {
      Serial.println("[CONFIG RESTORE] ERROR: 'config' key is null or missing");
      manager->sendError("Missing 'config' object in restore payload");
      return;
    }

    // Try to access as object even if type check fails
    JsonObjectConst restoreConfig = configVariant.as<JsonObjectConst>();

    if (restoreConfig.isNull())
    {
      Serial.println("[CONFIG RESTORE] ERROR: Cannot cast 'config' to JsonObject");
      manager->sendError("Invalid 'config' object in restore payload");
      return;
    }

    Serial.printf("[CONFIG RESTORE] OK: Config object validated (%u keys)\n", restoreConfig.size());

    int successCount = 0;
    int failCount = 0;
    JsonArray restoredConfigs;

    auto response = make_psram_unique<JsonDocument>();
    (*response)["status"] = "ok";
    restoredConfigs = (*response)["restored_configs"].to<JsonArray>();

    // Step 1: Restore devices configuration
    Serial.println("[CONFIG RESTORE] [1/3] Restoring devices configuration...");

    // BUG #32 FIX: Bypass type check, access directly
    JsonVariantConst devicesVariant = restoreConfig["devices"];
    if (!devicesVariant.isNull())
    {
      JsonArrayConst devices = devicesVariant.as<JsonArrayConst>();
      if (!devices.isNull() && devices.size() > 0)
      {
        // Clear existing devices first
        configManager->clearAllConfigurations();
        Serial.println("[CONFIG RESTORE] Existing devices cleared");

        // Restore each device
        int deviceCount = 0;
        int deviceIndex = 0;
        for (JsonObjectConst device : devices)
        {
          #if PRODUCTION_MODE == 0
            // BUG #32 DEBUG: Show device data BEFORE createDevice
            Serial.printf("[CONFIG RESTORE] DEBUG: Processing device %d:\n", deviceIndex);
            String deviceJson;
            serializeJson(device, deviceJson);
            Serial.printf("  JSON (%u bytes): %s\n", deviceJson.length(), deviceJson.c_str());

            JsonVariantConst idVariant = device["device_id"];
            Serial.printf("  device_id present: %s\n", !idVariant.isNull() ? "YES" : "NO");
            if (!idVariant.isNull()) {
              Serial.printf("  device_id value: %s\n", idVariant.as<const char*>());
            }

            JsonVariantConst regsVariant = device["registers"];
            Serial.printf("  registers present: %s\n", !regsVariant.isNull() ? "YES" : "NO");
            if (!regsVariant.isNull()) {
              JsonArrayConst regs = regsVariant.as<JsonArrayConst>();
              Serial.printf("  registers count: %u\n", regs.size());
            }
          #endif

          String deviceId = configManager->createDevice(device);
          if (!deviceId.isEmpty())
          {
            deviceCount++;
            #if PRODUCTION_MODE == 0
              Serial.printf("[CONFIG RESTORE] OK: Created device: %s\n", deviceId.c_str());
            #endif
          }
          else
          {
            Serial.printf("[CONFIG RESTORE] WARNING: Failed to restore device %d\n", deviceIndex);
          }

          deviceIndex++;
        }

        Serial.printf("[CONFIG RESTORE] Restored %d devices\n", deviceCount);
        restoredConfigs.add("devices.json");
        successCount++;
      }
      else
      {
        Serial.printf("[CONFIG RESTORE] WARNING: devices array is null or empty (size: %u)\n", devices.size());
      }
    }
    else
    {
      Serial.println("[CONFIG RESTORE] WARNING: No devices in backup");
    }

    // Step 2: Restore server configuration
    Serial.println("[CONFIG RESTORE] [2/3] Restoring server configuration...");

    // BUG #32 FIX: Bypass type check, access directly
    JsonVariantConst serverVariant = restoreConfig["server_config"];
    if (!serverVariant.isNull())
    {
      JsonObjectConst serverCfg = serverVariant.as<JsonObjectConst>();
      if (!serverCfg.isNull())
      {
        if (serverConfig->updateConfig(serverCfg))
        {
          Serial.println("[CONFIG RESTORE] Server config restored successfully");
          restoredConfigs.add("server_config.json");
          successCount++;
        }
        else
        {
          Serial.println("[CONFIG RESTORE] ERROR: Failed to restore server config");
          failCount++;
        }
      }
      else
      {
        Serial.println("[CONFIG RESTORE] WARNING: server_config cast to object failed");
      }
    }
    else
    {
      Serial.println("[CONFIG RESTORE] WARNING: No server_config in backup");
    }

    // Step 3: Restore logging configuration
    Serial.println("[CONFIG RESTORE] [3/3] Restoring logging configuration...");

    // BUG #32 FIX: Bypass type check, access directly
    JsonVariantConst loggingVariant = restoreConfig["logging_config"];
    if (!loggingVariant.isNull())
    {
      JsonObjectConst loggingCfg = loggingVariant.as<JsonObjectConst>();
      if (!loggingCfg.isNull())
      {
        if (loggingConfig->updateConfig(loggingCfg))
        {
          Serial.println("[CONFIG RESTORE] Logging config restored successfully");
          restoredConfigs.add("logging_config.json");
          successCount++;
        }
        else
        {
          Serial.println("[CONFIG RESTORE] ERROR: Failed to restore logging config");
          failCount++;
        }
      }
      else
      {
        Serial.println("[CONFIG RESTORE] WARNING: logging_config cast to object failed");
      }
    }
    else
    {
      Serial.println("[CONFIG RESTORE] WARNING: No logging_config in backup");
    }

    // Send response with restore summary
    (*response)["success_count"] = successCount;
    (*response)["fail_count"] = failCount;
    (*response)["message"] = "Configuration restore completed. Device restart recommended.";
    (*response)["requires_restart"] = true;

    Serial.println("[CONFIG RESTORE] ========================================");
    Serial.printf("[CONFIG RESTORE] Restore complete: %d succeeded, %d failed\n", successCount, failCount);
    Serial.println("[CONFIG RESTORE] Device restart recommended to apply all changes");
    Serial.println("[CONFIG RESTORE] ========================================");
    Serial.println("");

    manager->sendResponse(*response);

    // Notify services of config changes
    if (modbusRtuService)
    {
      modbusRtuService->notifyConfigChange();
    }
    if (modbusTcpService)
    {
      modbusTcpService->notifyConfigChange();
    }
  };
}

// ============================================================================
// BATCH OPERATIONS AND PRIORITY QUEUE IMPLEMENTATION
// ============================================================================

void CRUDHandler::enqueueCommand(BLEManager *manager, const JsonDocument &command, CommandPriority priority)
{
  if (xSemaphoreTake(queueMutex, pdMS_TO_TICKS(100)) != pdTRUE)
  {
    manager->sendError("Failed to enqueue command (mutex timeout)");
    return;
  }

  // Create command with ID and timestamp
  Command cmd;
  cmd.id = ++commandIdCounter;
  uint64_t cmdId = cmd.id; // Save ID before move
  cmd.priority = priority;
  cmd.enqueueTime = millis();
  cmd.manager = manager; // Store BLE manager for response sending

  // BUG #32 FIX: Serialize to String instead of using .set() which corrupts type info
  size_t commandSize = measureJson(command);

  #if PRODUCTION_MODE == 0
    Serial.printf("[CRUD QUEUE] Serializing command payload (%u bytes JSON)...\n", commandSize);
  #endif

  // Serialize JsonDocument to String (avoids .set() corruption issue)
  serializeJson(command, cmd.payloadJson);

  if (cmd.payloadJson.isEmpty())
  {
    Serial.println("[CRUD QUEUE] ERROR: Failed to serialize command payload!");
    xSemaphoreGive(queueMutex);
    manager->sendError("Failed to serialize command payload");
    return;
  }

  #if PRODUCTION_MODE == 0
    Serial.printf("[CRUD QUEUE] Command serialized successfully (%u bytes String)\n", cmd.payloadJson.length());
  #endif

  // Track statistics
  if (priority == CommandPriority::PRIORITY_HIGH)
  {
    batchStats.highPriorityCount++;
  }
  else if (priority == CommandPriority::PRIORITY_NORMAL)
  {
    batchStats.normalPriorityCount++;
  }
  else
  {
    batchStats.lowPriorityCount++;
  }

  // Add to priority queue
  commandQueue.push(std::move(cmd));
  updateQueueDepth();

  Serial.printf("[CRUD QUEUE] Command %lu enqueued (Priority: %d, Queue Depth: %lu)\n",
                cmdId, (uint8_t)priority, (unsigned long)commandQueue.size());

  xSemaphoreGive(queueMutex);
}

void CRUDHandler::processPriorityQueue()
{
  if (xSemaphoreTake(queueMutex, pdMS_TO_TICKS(100)) != pdTRUE)
  {
    return;
  }

  if (commandQueue.empty())
  {
    xSemaphoreGive(queueMutex);
    return;
  }

  // Get highest priority command (copy from queue)
  Command cmd;

  if (!commandQueue.empty())
  {
    cmd = commandQueue.top();  // Copy command (String is copyable)
    commandQueue.pop();
  }
  else
  {
    xSemaphoreGive(queueMutex);
    return;
  }

  updateQueueDepth();
  xSemaphoreGive(queueMutex);

  // Deserialize JSON String to JsonDocument (outside mutex to prevent blocking)
  SpiRamJsonDocument payload;

  #if PRODUCTION_MODE == 0
    Serial.printf("[CRUD EXEC] Deserializing payload from queue (%u bytes String)...\n", cmd.payloadJson.length());
  #endif

  DeserializationError error = deserializeJson(payload, cmd.payloadJson);

  if (error)
  {
    Serial.printf("[CRUD EXEC] ERROR: Failed to deserialize payload - %s\n", error.c_str());
    Serial.printf("[CRUD EXEC] JSON String length: %u bytes\n", cmd.payloadJson.length());
    Serial.printf("[CRUD EXEC] Free PSRAM: %u bytes\n",
                  heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    if (cmd.manager)
    {
      cmd.manager->sendError("Failed to deserialize command: " + String(error.c_str()));
    }
    return;
  }

  #if PRODUCTION_MODE == 0
    Serial.printf("[CRUD EXEC] Payload deserialized successfully (%u bytes)\n", measureJson(payload));
  #endif

  // BUG #32 FIX: Free DRAM by clearing String after deserialization
  cmd.payloadJson.clear();
  cmd.payloadJson = String();  // Force deallocation

  #if PRODUCTION_MODE == 0
    Serial.printf("[CRUD EXEC] Freed %u bytes DRAM (cleared payload string)\n",
                  heap_caps_get_free_size(MALLOC_CAP_8BIT));
  #endif

  // Only execute if we have a valid manager
  if (!cmd.manager)
  {
    Serial.println("[CRUD] ERROR: Command has no payload or manager, skipping execution");
    return;
  }

  // Execute command (outside mutex to prevent blocking)
  Serial.printf("[CRUD EXEC] Processing command %lu\n", cmd.id);

  String op = payload["op"] | "";
  String type = payload["type"] | "";

  // Route to appropriate handler (with valid manager pointer)
  bool handlerFound = false;

  if (op == "read" && readHandlers.count(type))
  {
    readHandlers[type](cmd.manager, payload);
    handlerFound = true;
  }
  else if (op == "create" && createHandlers.count(type))
  {
    createHandlers[type](cmd.manager, payload);
    handlerFound = true;
  }
  else if (op == "update" && updateHandlers.count(type))
  {
    updateHandlers[type](cmd.manager, payload);
    handlerFound = true;
  }
  else if (op == "delete" && deleteHandlers.count(type))
  {
    deleteHandlers[type](cmd.manager, payload);
    handlerFound = true;
  }
  else if (op == "control" && controlHandlers.count(type))
  {
    controlHandlers[type](cmd.manager, payload);
    handlerFound = true;
  }
  else if (op == "system" && systemHandlers.count(type))
  {
    systemHandlers[type](cmd.manager, payload);
    handlerFound = true;
  }

  if (!handlerFound)
  {
    Serial.printf("[CRUD EXEC] ERROR: No handler found for op='%s', type='%s'\n", op.c_str(), type.c_str());
    cmd.manager->sendError("Unknown operation or type: op=" + op + ", type=" + type);
  }

  batchStats.totalCommandsProcessed++;
}

void CRUDHandler::commandProcessorTask(void *parameter)
{
  CRUDHandler *handler = static_cast<CRUDHandler *>(parameter);

  Serial.println("[CRUD] Command processor task started");

  while (true)
  {
    handler->processPriorityQueue();
    vTaskDelay(pdMS_TO_TICKS(50)); // Check queue every 50ms
  }
}

void CRUDHandler::handleBatchOperation(BLEManager *manager, const JsonDocument &command)
{
  String batchMode = command["mode"] | "sequential";
  JsonArrayConst commands = command["commands"];

  if (commands.isNull() || commands.size() == 0)
  {
    manager->sendError("Batch operation requires 'commands' array");
    return;
  }

  // Convert mode string to enum
  BatchMode mode = BatchMode::SEQUENTIAL;
  if (batchMode == "atomic")
  {
    mode = BatchMode::ATOMIC;
  }
  else if (batchMode == "parallel")
  {
    mode = BatchMode::PARALLEL;
  }

  // Create batch ID
  String batchId = createBatch(command, mode);

  Serial.printf("[CRUD BATCH] Starting batch %s with %d commands (Mode: %s)\n",
                batchId.c_str(), commands.size(), batchMode.c_str());

  // Execute batch based on mode
  if (mode == BatchMode::ATOMIC)
  {
    executeBatchAtomic(manager, batchId, commands);
  }
  else
  {
    executeBatchSequential(manager, batchId, commands);
  }

  // Log results
  logBatchStats();
}

String CRUDHandler::createBatch(const JsonDocument &batchConfig, BatchMode mode)
{
  String batchId = "batch_" + String(millis() / 1000) + "_" + String(commandIdCounter++);

  BatchOperation batch;
  batch.batchId = batchId;
  batch.mode = mode;
  batch.startTime = millis();
  batch.isActive = true;
  batch.totalCommands = batchConfig["commands"].size();

  if (xSemaphoreTake(queueMutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    activeBatches[batchId] = batch;
    xSemaphoreGive(queueMutex);
  }

  return batchId;
}

void CRUDHandler::executeBatchSequential(BLEManager *manager, const String &batchId, JsonArrayConst commands)
{
  Serial.printf("[CRUD BATCH] Executing SEQUENTIAL batch %s\n", batchId.c_str());

  uint32_t completed = 0;
  uint32_t failed = 0;

  for (JsonVariantConst cmdVar : commands)
  {
    JsonObjectConst cmdObj = cmdVar.as<JsonObjectConst>();

    String op = cmdObj["op"] | "";
    String type = cmdObj["type"] | "";

    bool success = false;

    // Execute command - need to create a JsonDocument from the variant
    // since handlers expect JsonDocument& (non-const)
    auto cmdDoc = make_psram_unique<JsonDocument>();
    cmdDoc->set(cmdVar);

    if (op == "read" && readHandlers.count(type))
    {
      readHandlers[type](manager, *cmdDoc);
      success = true;
    }
    else if (op == "create" && createHandlers.count(type))
    {
      createHandlers[type](manager, *cmdDoc);
      success = true;
    }
    else if (op == "update" && updateHandlers.count(type))
    {
      updateHandlers[type](manager, *cmdDoc);
      success = true;
    }
    else if (op == "delete" && deleteHandlers.count(type))
    {
      deleteHandlers[type](manager, *cmdDoc);
      success = true;
    }
    else if (op == "control" && controlHandlers.count(type))
    {
      controlHandlers[type](manager, *cmdDoc);
      success = true;
    }
    else if (op == "system" && systemHandlers.count(type))
    {
      systemHandlers[type](manager, *cmdDoc);
      success = true;
    }

    if (success)
    {
      completed++;
    }
    else
    {
      failed++;
    }

    // Small delay between sequential commands
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  // Update batch stats
  if (xSemaphoreTake(queueMutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    if (activeBatches.count(batchId))
    {
      activeBatches[batchId].completedCommands = completed;
      activeBatches[batchId].failedCommands = failed;
      activeBatches[batchId].endTime = millis();
      activeBatches[batchId].isActive = false;

      batchStats.totalBatchesProcessed++;
      batchStats.totalCommandsFailed += failed;
    }
    xSemaphoreGive(queueMutex);
  }

  Serial.printf("[CRUD BATCH] Batch %s completed: %lu succeeded, %lu failed\n",
                batchId.c_str(), completed, failed);
}

void CRUDHandler::executeBatchAtomic(BLEManager *manager, const String &batchId, JsonArrayConst commands)
{
  Serial.printf("[CRUD BATCH] Executing ATOMIC batch %s\n", batchId.c_str());

  // In ATOMIC mode, all commands must succeed or all must fail
  uint32_t completed = 0;
  uint32_t failed = 0;
  bool shouldRollback = false;

  // First pass: validate all commands
  for (JsonVariantConst cmdVar : commands)
  {
    JsonObjectConst cmdObj = cmdVar.as<JsonObjectConst>();
    String op = cmdObj["op"] | "";
    String type = cmdObj["type"] | "";

    bool handlerExists = false;
    if (op == "read")
    {
      handlerExists = readHandlers.count(type) > 0;
    }
    else if (op == "create")
    {
      handlerExists = createHandlers.count(type) > 0;
    }
    else if (op == "update")
    {
      handlerExists = updateHandlers.count(type) > 0;
    }
    else if (op == "delete")
    {
      handlerExists = deleteHandlers.count(type) > 0;
    }
    else if (op == "control")
    {
      handlerExists = controlHandlers.count(type) > 0;
    }
    else if (op == "system")
    {
      handlerExists = systemHandlers.count(type) > 0;
    }

    if (!handlerExists)
    {
      shouldRollback = true;
      failed++;
      Serial.printf("[CRUD BATCH] Validation failed for op '%s' type '%s'\n", op.c_str(), type.c_str());
    }
  }

  // If validation passes, execute all commands
  if (!shouldRollback)
  {
    for (JsonVariantConst cmdVar : commands)
    {
      JsonObjectConst cmdObj = cmdVar.as<JsonObjectConst>();
      String op = cmdObj["op"] | "";
      String type = cmdObj["type"] | "";

      // Create a JsonDocument from the variant for the handler
      auto cmdDoc = make_psram_unique<JsonDocument>();
      cmdDoc->set(cmdVar);

      if (op == "read" && readHandlers.count(type))
      {
        readHandlers[type](manager, *cmdDoc);
        completed++;
      }
      else if (op == "create" && createHandlers.count(type))
      {
        createHandlers[type](manager, *cmdDoc);
        completed++;
      }
      else if (op == "update" && updateHandlers.count(type))
      {
        updateHandlers[type](manager, *cmdDoc);
        completed++;
      }
      else if (op == "delete" && deleteHandlers.count(type))
      {
        deleteHandlers[type](manager, *cmdDoc);
        completed++;
      }
      else if (op == "control" && controlHandlers.count(type))
      {
        controlHandlers[type](manager, *cmdDoc);
        completed++;
      }
      else if (op == "system" && systemHandlers.count(type))
      {
        systemHandlers[type](manager, *cmdDoc);
        completed++;
      }

      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
  else
  {
    manager->sendError("ATOMIC batch failed validation");
  }

  // Update batch stats
  if (xSemaphoreTake(queueMutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    if (activeBatches.count(batchId))
    {
      activeBatches[batchId].completedCommands = completed;
      activeBatches[batchId].failedCommands = failed;
      activeBatches[batchId].endTime = millis();
      activeBatches[batchId].isActive = false;

      batchStats.totalBatchesProcessed++;
      batchStats.totalCommandsFailed += failed;
    }
    xSemaphoreGive(queueMutex);
  }

  Serial.printf("[CRUD BATCH] Batch %s (ATOMIC) completed: %lu succeeded, %lu failed\n",
                batchId.c_str(), completed, failed);
}

void CRUDHandler::updateQueueDepth()
{
  batchStats.currentQueueDepth = commandQueue.size();
  if (batchStats.currentQueueDepth > batchStats.queuePeakDepth)
  {
    batchStats.queuePeakDepth = batchStats.currentQueueDepth;
  }
}

void CRUDHandler::logBatchStats()
{
  if (xSemaphoreTake(queueMutex, pdMS_TO_TICKS(100)) != pdTRUE)
  {
    return;
  }

  Serial.println("[CRUD STATS] ==================== BATCH STATISTICS ====================");
  Serial.printf("[CRUD STATS] Total Batches Processed: %lu\n", batchStats.totalBatchesProcessed);
  Serial.printf("[CRUD STATS] Total Commands Processed: %lu\n", batchStats.totalCommandsProcessed);
  Serial.printf("[CRUD STATS] Total Commands Failed: %lu\n", batchStats.totalCommandsFailed);
  Serial.printf("[CRUD STATS] High Priority Commands: %lu\n", batchStats.highPriorityCount);
  Serial.printf("[CRUD STATS] Normal Priority Commands: %lu\n", batchStats.normalPriorityCount);
  Serial.printf("[CRUD STATS] Low Priority Commands: %lu\n", batchStats.lowPriorityCount);
  Serial.printf("[CRUD STATS] Current Queue Depth: %lu\n", batchStats.currentQueueDepth);
  Serial.printf("[CRUD STATS] Queue Peak Depth: %lu\n", batchStats.queuePeakDepth);
  Serial.println("[CRUD STATS] =============================================================");

  xSemaphoreGive(queueMutex);
}

BatchStats CRUDHandler::getBatchStats() const
{
  return batchStats;
}

void CRUDHandler::reportStats(JsonObject &statsObj)
{
  if (xSemaphoreTake(queueMutex, pdMS_TO_TICKS(100)) != pdTRUE)
  {
    return;
  }

  statsObj["total_batches"] = batchStats.totalBatchesProcessed;
  statsObj["total_commands"] = batchStats.totalCommandsProcessed;
  statsObj["failed_commands"] = batchStats.totalCommandsFailed;
  statsObj["high_priority"] = batchStats.highPriorityCount;
  statsObj["normal_priority"] = batchStats.normalPriorityCount;
  statsObj["low_priority"] = batchStats.lowPriorityCount;
  statsObj["current_queue_depth"] = batchStats.currentQueueDepth;
  statsObj["peak_queue_depth"] = batchStats.queuePeakDepth;

  double successRate = 0.0;
  if (batchStats.totalCommandsProcessed > 0)
  {
    successRate = ((double)(batchStats.totalCommandsProcessed - batchStats.totalCommandsFailed) /
                   batchStats.totalCommandsProcessed) *
                  100.0;
  }
  statsObj["success_rate_percent"] = successRate;

  xSemaphoreGive(queueMutex);
}

// ============================================================================
// FACTORY RESET IMPLEMENTATION
// ============================================================================

void CRUDHandler::performFactoryReset()
{
  Serial.println("[FACTORY RESET] ========================================");
  Serial.println("[FACTORY RESET] Starting graceful shutdown sequence...");
  Serial.println("[FACTORY RESET] ========================================");

  // Step 1: Stop all services (Modbus + LED + Button)
  Serial.println("[FACTORY RESET] [1/6] Stopping all services...");

  // Stop LED manager
  LEDManager *ledManager = LEDManager::getInstance();
  if (ledManager)
  {
    ledManager->stop();
    Serial.println("[FACTORY RESET] LED manager stopped");
  }

  // Stop Modbus services
  if (modbusRtuService)
  {
    modbusRtuService->stop();
    Serial.println("[FACTORY RESET] RTU service stopped");
  }
  if (modbusTcpService)
  {
    modbusTcpService->stop();
    Serial.println("[FACTORY RESET] TCP service stopped");
  }

  // Step 2: Disconnect network services
  Serial.println("[FACTORY RESET] [2/6] Disconnecting network services...");
  if (mqttManager)
  {
    mqttManager->disconnect();
    Serial.println("[FACTORY RESET] MQTT disconnected");
  }
  if (httpManager)
  {
    httpManager->stop();
    Serial.println("[FACTORY RESET] HTTP stopped");
  }

  // Step 3: Clear device configurations
  Serial.println("[FACTORY RESET] [3/6] Clearing device configurations...");
  configManager->clearAllConfigurations();
  Serial.println("[FACTORY RESET] devices.json cleared");

  // Step 4: Reset server config to defaults
  Serial.println("[FACTORY RESET] [4/6] Resetting server config to defaults...");
  if (serverConfig)
  {
    // Suppress auto-restart since factory reset has its own restart
    serverConfig->setSuppressRestart(true);

    JsonDocument defaultServerConfig;
    JsonObject root = defaultServerConfig.to<JsonObject>();

    // Communication config (mobile app structure)
    JsonObject comm = root["communication"].to<JsonObject>();
    comm["mode"] = "ETH";  // Mobile app expects this field

    // WiFi at root level (mobile app structure)
    JsonObject wifi = root["wifi"].to<JsonObject>();
    wifi["enabled"] = true;
    wifi["ssid"] = "";
    wifi["password"] = "";

    // Ethernet at root level (mobile app structure)
    JsonObject ethernet = root["ethernet"].to<JsonObject>();
    ethernet["enabled"] = true;
    ethernet["use_dhcp"] = true;
    ethernet["static_ip"] = "";
    ethernet["gateway"] = "";
    ethernet["subnet"] = "";

    // Protocol
    root["protocol"] = "mqtt";

    // MQTT config with publish modes
    JsonObject mqtt = root["mqtt_config"].to<JsonObject>();
    mqtt["enabled"] = true;
    mqtt["broker_address"] = "broker.hivemq.com";
    mqtt["broker_port"] = 1883;
    mqtt["client_id"] = "";
    mqtt["username"] = "";
    mqtt["password"] = "";
    mqtt["topic_publish"] = "v1/devices/me/telemetry";  // Top level for mobile app compatibility
    mqtt["topic_subscribe"] = "";  // Top level for mobile app compatibility
    mqtt["keep_alive"] = 60;
    mqtt["clean_session"] = true;
    mqtt["use_tls"] = false;
    mqtt["publish_mode"] = "default";  // "default" or "customize"

    // Default mode configuration (for MQTT modes feature)
    JsonObject defaultMode = mqtt["default_mode"].to<JsonObject>();
    defaultMode["enabled"] = true;
    defaultMode["topic_publish"] = "v1/devices/me/telemetry";
    defaultMode["topic_subscribe"] = "";
    defaultMode["interval"] = 5;
    defaultMode["interval_unit"] = "s";  // "ms" (milliseconds), "s" (seconds), "m" (minutes)

    // Customize mode configuration (for MQTT modes feature)
    JsonObject customizeMode = mqtt["customize_mode"].to<JsonObject>();
    customizeMode["enabled"] = false;
    customizeMode["custom_topics"].to<JsonArray>();

    // HTTP config
    JsonObject http = root["http_config"].to<JsonObject>();
    http["enabled"] = false;
    http["endpoint_url"] = "https://api.example.com/data";
    http["method"] = "POST";
    http["body_format"] = "json";
    http["timeout"] = 5000;
    http["retry"] = 3;
    http["interval"] = 5;           // HTTP transmission interval
    http["interval_unit"] = "s";    // "ms", "s", or "m"

    JsonObject headers = http["headers"].to<JsonObject>();
    headers["Authorization"] = "Bearer token";
    headers["Content-Type"] = "application/json";

    serverConfig->updateConfig(root);

    // Re-enable auto-restart for future config updates
    serverConfig->setSuppressRestart(false);

    Serial.println("[FACTORY RESET] server_config.json reset to defaults (COMPLETE with MQTT modes + HTTP)");
  }

  // Step 5: Reset logging config to defaults
  Serial.println("[FACTORY RESET] [5/6] Resetting logging config to defaults...");
  if (loggingConfig)
  {
    JsonDocument defaultLoggingConfig;
    JsonObject root = defaultLoggingConfig.to<JsonObject>();

    // Create default logging config
    root["logging_ret"] = "1w";
    root["logging_interval"] = "5m";

    loggingConfig->updateConfig(root);
    Serial.println("[FACTORY RESET] logging_config.json reset to defaults");
  }

  // Step 6: Restart device
  Serial.println("[FACTORY RESET] [6/6] All configurations cleared successfully");
  Serial.println("[FACTORY RESET] ========================================");
  Serial.println("[FACTORY RESET] Device will restart in 3 seconds...");
  Serial.println("[FACTORY RESET] ========================================");

  delay(3000);

  Serial.println("[FACTORY RESET] RESTARTING NOW...");
  ESP.restart();
}
