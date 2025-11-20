#include "CRUDHandler.h"
#include "BLEManager.h"
#include "QueueManager.h"
#include "ModbusRtuService.h"
#include "ModbusTcpService.h"
#include "MqttManager.h"   //For calling updateDataTransmissionInterval()
#include "HttpManager.h"   //For calling updateDataTransmissionInterval()
#include "MemoryManager.h" // For make_psram_unique

// Make service pointers available to the handler
extern ModbusRtuService *modbusRtuService;
extern ModbusTcpService *modbusTcpService;

CRUDHandler::CRUDHandler(ConfigManager *config, ServerConfig *serverCfg, LoggingConfig *loggingCfg)
    : configManager(config), serverConfig(serverCfg), loggingConfig(loggingCfg), mqttManager(nullptr), httpManager(nullptr), streamDeviceId(""),
      commandIdCounter(0)
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
      Serial.println("[CRUD] ⚠️  WARNING: No devices returned! Check if devices are configured in devices.json");
    }

    // Warn if processing takes too long (>10 seconds)
    if (processingTime > 10000)
    {
      Serial.printf("[CRUD] ⚠️  WARNING: Processing took %lu ms (>10s). Consider using minimal=true for large datasets.\n", processingTime);
    }

    manager->sendResponse(*response);
  };

  readHandlers["device"] = [this](BLEManager *manager, const JsonDocument &command)
  {
    String deviceId = command["device_id"] | "";
    bool minimal = command["minimal"] | false;  // Support optional "minimal" parameter

    auto response = make_psram_unique<JsonDocument>();
    (*response)["status"] = "ok";
    JsonObject data = (*response)["data"].to<JsonObject>();
    if (configManager->readDevice(deviceId, data, minimal))
    {
      // Log payload size for debugging
      String payload;
      serializeJson(*response, payload);
      Serial.printf("[CRUD] Device read response size: %d bytes (minimal=%s)\n",
                    payload.length(), minimal ? "true" : "false");

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
    auto response = make_psram_unique<JsonDocument>();
    (*response)["status"] = "ok";
    JsonArray registers = (*response)["registers"].to<JsonArray>();
    if (configManager->listRegisters(deviceId, registers))
    {
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

      auto response = make_psram_unique<JsonDocument>();
      (*response)["status"] = "ok";
      (*response)["message"] = "Data streaming stopped";
      manager->sendResponse(*response);
      Serial.println("[CRUD] Stop response sent");
    }
    else if (!device.isEmpty())
    {
      streamDeviceId = device;

      // Set streaming flag to true when starting
      manager->setStreamingActive(true);

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
  cmd.payload = std::make_unique<JsonDocument>();
  cmd.payload->set(command);

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

  // Get highest priority command using move semantics
  // We need to create a temporary to work around priority_queue limitations
  Command cmd;
  bool hasCommand = false;

  if (!commandQueue.empty())
  {
    // Copy the top element's data manually to avoid const reference issues
    const Command &topCmd = commandQueue.top();

    // Check if payload exists before accessing
    if (topCmd.payload)
    {
      cmd.id = topCmd.id;
      cmd.priority = topCmd.priority;
      cmd.enqueueTime = topCmd.enqueueTime;
      cmd.manager = topCmd.manager; // Copy manager pointer
      cmd.payload = std::make_unique<JsonDocument>();
      cmd.payload->set(*topCmd.payload);
      hasCommand = true;
    }

    commandQueue.pop();
  }
  updateQueueDepth();

  xSemaphoreGive(queueMutex);

  // Only execute if we have a valid command with payload and manager
  if (!hasCommand || !cmd.payload || !cmd.manager)
  {
    Serial.println("[CRUD] ERROR: Command has no payload or manager, skipping execution");
    return;
  }

  // Execute command (outside mutex to prevent blocking)
  Serial.printf("[CRUD EXEC] Processing command %lu\n", cmd.id);

  String op = (*cmd.payload)["op"] | "";
  String type = (*cmd.payload)["type"] | "";

  // Route to appropriate handler (with valid manager pointer)
  if (op == "read" && readHandlers.count(type))
  {
    readHandlers[type](cmd.manager, *cmd.payload);
  }
  else if (op == "create" && createHandlers.count(type))
  {
    createHandlers[type](cmd.manager, *cmd.payload);
  }
  else if (op == "update" && updateHandlers.count(type))
  {
    updateHandlers[type](cmd.manager, *cmd.payload);
  }
  else if (op == "delete" && deleteHandlers.count(type))
  {
    deleteHandlers[type](cmd.manager, *cmd.payload);
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
