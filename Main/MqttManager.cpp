#include "MqttManager.h"

#include <set>  // For std::set to track cleared devices

#include "DebugConfig.h"  // MUST BE FIRST for DEV_SERIAL_* macros
#include "LEDManager.h"
#include "MemoryRecovery.h"
#include "RTCManager.h"

// Helper function: Convert interval to milliseconds based on unit
// Supports multiple unit variants for flexibility:
// - Milliseconds: "ms", "millisecond", "milliseconds"
// - Seconds: "s", "sec", "secs", "second", "seconds"
// - Minutes: "m", "min", "mins", "minute", "minutes"
static uint32_t convertToMilliseconds(uint32_t interval, const String& unit) {
  String unitLower = unit;
  unitLower.toLowerCase();

  if (unitLower == "ms" || unitLower == "millisecond" ||
      unitLower == "milliseconds" || unitLower.isEmpty()) {
    return interval;  // Already in milliseconds
  } else if (unitLower == "s" || unitLower == "sec" || unitLower == "secs" ||
             unitLower == "second" || unitLower == "seconds") {
    return interval * 1000;  // Seconds to milliseconds
  } else if (unitLower == "m" || unitLower == "min" || unitLower == "mins" ||
             unitLower == "minute" || unitLower == "minutes") {
    return interval * 60000;  // Minutes to milliseconds
  }
  return interval;  // Default: treat as milliseconds
}

MqttManager* MqttManager::instance = nullptr;

MqttManager::MqttManager(ConfigManager* config, ServerConfig* serverCfg,
                         NetworkMgr* netMgr)
    : configManager(config),
      queueManager(nullptr),
      serverConfig(serverCfg),
      networkManager(netMgr),
      mqttClient(PubSubClient()),
      running(false),
      taskHandle(nullptr),
      taskExitEvent(nullptr),  // v2.5.1 FIX: Initialize event group
      brokerPort(1883),
      lastReconnectAttempt(0),
      cachedBufferSize(0),
      bufferSizeNeedsRecalculation(true),  // FIXED BUG #5: Initialize cache
      lastDebugTime(0)  // v2.3.8 PHASE 1: Initialize connection state
{
  queueManager = QueueManager::getInstance();
  persistentQueue = MQTTPersistentQueue::getInstance();

  // v2.3.8 PHASE 1: Initialize publish state (replaces static variables)
  publishState.targetTime = 0;
  publishState.timeLocked = false;
  publishState.lastLoggedInterval = 0;
  publishState.batchWaitStart = 0;
  publishState.batchTimeout = 0;
  publishState.flushedOnce = false;

  // v2.3.8 PHASE 1: Create mutexes for thread safety
  bufferCacheMutex = xSemaphoreCreateMutex();
  publishStateMutex = xSemaphoreCreateMutex();

  if (bufferCacheMutex == NULL || publishStateMutex == NULL) {
    LOG_MQTT_INFO("[MQTT] CRITICAL: Failed to create mutexes!");
  } else {
    LOG_MQTT_INFO("[MQTT] Thread safety mutexes created successfully");
  }

  // v2.5.1 FIX: Create event group for safe task termination (prevents race
  // condition)
  taskExitEvent = xEventGroupCreate();
  if (!taskExitEvent) {
    LOG_MQTT_INFO("[MQTT] WARNING: Failed to create task exit event group");
  }

  LOG_MQTT_INFO("[MQTT] Persistent queue initialized");
}

MqttManager* MqttManager::getInstance(ConfigManager* config,
                                      ServerConfig* serverCfg,
                                      NetworkMgr* netMgr) {
  if (instance == nullptr && config != nullptr && serverCfg != nullptr &&
      netMgr != nullptr) {
    instance = new MqttManager(config, serverCfg, netMgr);
  }
  return instance;
}

bool MqttManager::init() {
  LOG_MQTT_INFO("[MQTT] Initializing manager...");

  if (!configManager || !queueManager || !serverConfig || !networkManager) {
    LOG_MQTT_INFO(
        "[MQTT] ERROR: ConfigManager, QueueManager, ServerConfig, or "
        "NetworkManager is null");
    return false;
  }

  loadMqttConfig();

  // OPTIMIZED: Clean up expired messages from persistent queue on startup
  if (persistentQueue) {
    uint32_t expiredCount = persistentQueue->getPendingMessageCount();
    persistentQueue->clearExpiredMessages();
    uint32_t remainingCount = persistentQueue->getPendingMessageCount();
    uint32_t removed = expiredCount - remainingCount;
    if (removed > 0) {
      LOG_MQTT_INFO("[MQTT] Cleaned %lu expired messages from queue\n",
                    removed);
    }
  }

  LOG_MQTT_INFO("[MQTT] Manager initialized");
  return true;
}

void MqttManager::start() {
  LOG_MQTT_INFO("[MQTT] Starting manager...");

  if (running) {
    return;
  }

  running = true;
  BaseType_t result = xTaskCreatePinnedToCore(
      mqttTask, "MQTT_TASK", MqttConfig::MQTT_TASK_STACK_SIZE, this,
      2,  // FIXED: Priority 2 (equal to Modbus RTU task) to ensure fair CPU
          // time for MQTT PINGREQ Previous: Priority 1 was too low - MQTT task
          // was starved during long RTU polling New: Priority 2 ensures MQTT
          // keep-alive packets are sent even during heavy RTU load
      &taskHandle,
      1);  // FIXED: Run on Core 1 to avoid blocking IDLE0 on Core 0

  if (result == pdPASS) {
    LOG_MQTT_INFO("[MQTT] Manager started successfully");
  } else {
    LOG_MQTT_INFO("[MQTT] ERROR: Failed to create MQTT task");
    running = false;
    taskHandle = nullptr;
  }
}

void MqttManager::stop() {
  running = false;

  // v2.5.1 FIX: Wait for task to signal exit (prevents race condition)
  // Previous: 50ms delay was insufficient - task might still access member
  // variables New: Wait up to 2 seconds for task to confirm exit via event
  // group
  if (taskHandle) {
    if (taskExitEvent) {
      // Clear the bit first in case it was set from previous run
      xEventGroupClearBits(taskExitEvent, TASK_EXITED_BIT);

      // Wait for task to signal it has exited (max 2 seconds)
      EventBits_t bits = xEventGroupWaitBits(
          taskExitEvent,
          TASK_EXITED_BIT,     // Wait for this bit
          pdTRUE,              // Clear bit after return
          pdTRUE,              // Wait for all bits (only 1 in this case)
          pdMS_TO_TICKS(2000)  // 2 second timeout
      );

      if (!(bits & TASK_EXITED_BIT)) {
        LOG_MQTT_INFO(
            "[MQTT] WARNING: Task did not exit gracefully within 2s, forcing "
            "deletion");
      }
    } else {
      // Fallback if event group creation failed
      vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Now safe to delete the task (it has confirmed exit or timed out)
    vTaskDelete(taskHandle);
    taskHandle = nullptr;
  }

  if (mqttClient.connected()) {
    mqttClient.disconnect();
  }
  LOG_MQTT_INFO("[MQTT] Manager stopped");
}

void MqttManager::disconnect() {
  if (mqttClient.connected()) {
    mqttClient.disconnect();
    LOG_MQTT_INFO("[MQTT] Gracefully disconnected from broker");
  }
}

void MqttManager::mqttTask(void* parameter) {
  MqttManager* manager = static_cast<MqttManager*>(parameter);
  manager->mqttLoop();
}

void MqttManager::mqttLoop() {
  bool wasConnected = false;
  bool wifiWasConnected = false;

  LOG_MQTT_INFO("[MQTT] Task started on Core 1");

  while (running) {
    // ============================================
    // MEMORY RECOVERY CHECK (Phase 2 Optimization)
    // ============================================
    MemoryRecovery::checkAndRecover();

    // Check network availability
    bool networkAvailable = isNetworkAvailable();

    if (!networkAvailable) {
      if (wifiWasConnected) {
        LOG_MQTT_INFO("[MQTT] Network disconnected");
        wifiWasConnected = false;
        wasConnected = false;
      }
      LOG_MQTT_INFO("[MQTT] Waiting for network | Mode: %s | IP: %s\n",
                    networkManager->getCurrentMode().c_str(),
                    networkManager->getLocalIP().toString().c_str());

      // vTaskDelay automatically feeds watchdog
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    } else if (!wifiWasConnected) {
      LOG_MQTT_INFO("[MQTT] Network available | Mode: %s | IP: %s\n",
                    networkManager->getCurrentMode().c_str(),
                    networkManager->getLocalIP().toString().c_str());
      wifiWasConnected = true;
    }

    // Connect to MQTT if not connected
    if (!mqttClient.connected()) {
      if (wasConnected) {
        LOG_MQTT_INFO("[MQTT] Connection lost, attempting reconnect...");
        wasConnected = false;

        // Update LED status - connection lost
        if (ledManager) {
          ledManager->setMqttConnectionStatus(false);
        }
      }

      unsigned long now = millis();
      if (now - lastReconnectAttempt > 5000) {
        lastReconnectAttempt = now;
        // v2.3.8 PHASE 1: Use instance member instead of static variable
        if (now - lastDebugTime > 30000) {
          debugNetworkConnectivity();
          lastDebugTime = now;
        }

        if (connectToMqtt()) {
          // Connection message already logged in connectToMqtt()
          wasConnected = true;

          // Update LED status - connection established
          if (ledManager) {
            ledManager->setMqttConnectionStatus(true);
          }
        }

        // Yield to feed watchdog
        vTaskDelay(pdMS_TO_TICKS(10));
      } else {
        // Yield while waiting for reconnect interval
        vTaskDelay(pdMS_TO_TICKS(100));
      }
    } else {
      if (!wasConnected) {
        // Connection already active, no need for verbose logging
        wasConnected = true;
      }

      // Process MQTT events and publish data
      // v1.0.6 OPTIMIZED: Consolidated delays for better responsiveness
      // Previous: loop() + 10ms + publish() + 10ms + 50ms = 70ms per iteration
      // New: loop() + publish() + 30ms = 30ms per iteration (2.3x faster)
      mqttClient.loop();
      publishQueueData();
      vTaskDelay(pdMS_TO_TICKS(30));  // Single consolidated delay
    }

    // v1.0.6: Base delay moved inside conditions for better control
    // Connected: 30ms (above), Disconnected: 50-100ms (reconnect logic)
  }

  // v2.5.1 FIX: Signal that task has exited (for safe stop() synchronization)
  // This prevents race condition where stop() deletes object while task still
  // runs
  if (taskExitEvent) {
    xEventGroupSetBits(taskExitEvent, TASK_EXITED_BIT);
  }

  LOG_MQTT_INFO("[MQTT] Task stopped");
}

bool MqttManager::connectToMqtt() {
  // Check network connectivity first
  IPAddress localIP = networkManager->getLocalIP();
  String networkMode = networkManager->getCurrentMode();

  Client* activeClient = networkManager->getActiveClient();
  if (!activeClient) {
    LOG_MQTT_INFO("[MQTT] ERROR: No active network client available");
    return false;
  }

  mqttClient.setClient(*activeClient);

  // FIXED BUG #15: Dynamic buffer sizing based on actual device configuration
  // FIXED BUG #5 & #7: Use cached buffer size to avoid repeated calculations
  // Previous: Hardcoded 8192 bytes, recalculated every connection
  // New: Calculate once, cache result, recalculate only on config change
  // v2.3.8 PHASE 1: Added mutex protection for thread safety
  xSemaphoreTake(bufferCacheMutex, portMAX_DELAY);
  if (bufferSizeNeedsRecalculation || cachedBufferSize == 0) {
    cachedBufferSize = calculateOptimalBufferSize();
    bufferSizeNeedsRecalculation = false;
#if PRODUCTION_MODE == 0
    LOG_MQTT_INFO(
        "[MQTT] Buffer size calculated: %u bytes (cached for reuse)\n",
        cachedBufferSize);
#endif
  }
#if PRODUCTION_MODE == 0
  else {
    LOG_MQTT_INFO("[MQTT] Buffer size using cached value: %u bytes\n",
                  cachedBufferSize);
  }
#endif
  uint16_t bufferSize = cachedBufferSize;  // Copy under mutex protection
  xSemaphoreGive(bufferCacheMutex);

  mqttClient.setBufferSize(bufferSize,
                           bufferSize);  // Use local copy (thread-safe)

#if PRODUCTION_MODE == 0
  LOG_MQTT_INFO("[MQTT] Max packet size: %u bytes (MQTT_MAX_PACKET_SIZE)\n",
                MQTT_MAX_PACKET_SIZE);
#endif

  // FIXED: Increased keep_alive to 120s to prevent timeouts during long Modbus
  // polling cycles Previous: 60s was too short when RTU polling takes ~50s +
  // publish interval 70s New: 120s provides enough margin for slow network
  // conditions and long polling
  mqttClient.setKeepAlive(120);

  // FIXED: Increased socket timeout from 5s to 15s for public broker resilience
  // Previous: 5s timeout caused reconnection failures (error -4) on
  // slow/overloaded public brokers New: 15s provides adequate time for
  // broker.hivemq.com to respond during high load Note: Public brokers often
  // have slow response times (5-10s is common)
  mqttClient.setSocketTimeout(15);  // Socket timeout in seconds

  mqttClient.setServer(brokerAddress.c_str(), brokerPort);

  // Track connection attempt time
  unsigned long connectStartTime = millis();

  bool connected;
  if (username.length() > 0 && password.length() > 0) {
    connected = mqttClient.connect(clientId.c_str(), username.c_str(),
                                   password.c_str());
  } else {
    connected = mqttClient.connect(clientId.c_str());
  }

// Log connection duration for diagnostics (only in development mode)
#if PRODUCTION_MODE == 0
  unsigned long connectDuration = millis() - connectStartTime;
  if (connectDuration > 2000) {
    LOG_MQTT_INFO("[MQTT] Connection attempt took %lu ms (slow)\n",
                  connectDuration);
  }
#endif

  if (connected) {
    LOG_MQTT_INFO("[MQTT] Connected | Broker: %s:%d | Network: %s (%s)\n",
                  brokerAddress.c_str(), brokerPort, networkMode.c_str(),
                  localIP.toString().c_str());
  } else {
    LOG_MQTT_INFO("[MQTT] ERROR: Connection failed | Error code: %d\n",
                  mqttClient.state());
  }

  return connected;
}

// v2.3.8 PHASE 2: REFACTORED - Reduced from 131 lines to ~25 lines using helper
// methods (81% reduction)
void MqttManager::loadMqttConfig() {
  JsonDocument configDoc;
  JsonObject mqttConfig = configDoc.to<JsonObject>();

  LOG_MQTT_INFO("[MQTT] Loading MQTT configuration...");

  if (serverConfig->getMqttConfig(mqttConfig)) {
    // Helper 7: Load broker configuration
    loadBrokerConfig(mqttConfig);

    // Helper 8: Load default mode configuration
    loadDefaultModeConfig(mqttConfig);

    // Helper 9: Load customize mode configuration
    loadCustomizeModeConfig(mqttConfig);
  } else {
    // Fallback to defaults if config loading fails
    LOG_MQTT_INFO("[MQTT] Failed to load config, using defaults");
    brokerAddress = "broker.hivemq.com";
    brokerPort = 1883;
    // v2.5.36 FIX: Use MAC-based unique ID instead of random() which may not be
    // seeded
    uint64_t mac = ESP.getEfuseMac();
    clientId = String("MGate1210_") + String((uint32_t)(mac & 0xFFFFFF), HEX);
    publishMode = "default";
    defaultModeEnabled = true;
    defaultTopicPublish = "device/data";
    defaultTopicSubscribe = "device/control";
    defaultInterval = 5000;
    defaultIntervalUnit = "ms";
    customizeModeEnabled = false;
  }

  // Set top-level topic for backward compatibility
  topicPublish = defaultTopicPublish;
}

// ============================================================================
// v2.3.8 PHASE 1: HELPER METHODS - Eliminate Code Duplication (DRY Principle)
// ============================================================================
// These methods extract common patterns from publishDefaultMode() and
// publishCustomizeMode() Reduces code duplication from 60% to 0%, improving
// maintainability and testability

/**
 * Helper 1: Build RTC timestamp for JSON payload
 * @param doc JsonDocument to add timestamp to
 * @param now Fallback timestamp (millis) if RTC unavailable
 */
void MqttManager::buildTimestamp(JsonDocument& doc, unsigned long now) {
  RTCManager* rtcMgr = RTCManager::getInstance();
  if (rtcMgr) {
    DateTime currentTime = rtcMgr->getCurrentTime();
    char timestampStr[20];
    snprintf(timestampStr, sizeof(timestampStr),
             "%02d/%02d/%04d %02d:%02d:%02d", currentTime.day(),
             currentTime.month(), currentTime.year(), currentTime.hour(),
             currentTime.minute(), currentTime.second());
    doc["timestamp"] = timestampStr;
  } else {
    doc["timestamp"] = now;  // Fallback to millis if RTC not available
  }
}

/**
 * Helper 2: Validate devices and group registers by device_id
 * @param uniqueRegisters Input map of register data
 * @param devicesObject Output JSON object for devices
 * @param deviceObjects Output map tracking device objects
 * @param filterRegisters Optional filter (nullptr = include all, else filter by
 * register_id)
 * @return Number of valid registers added
 */
int MqttManager::validateAndGroupRegisters(
    std::map<String, JsonDocument>& uniqueRegisters, JsonObject& devicesObject,
    std::map<String, JsonObject>& deviceObjects,
    const std::vector<String>* filterRegisters) {
  // Track deleted devices to avoid spam logging
  std::map<String, int> deletedDevices;  // device_id -> skipped register count
  int validRegisterCount = 0;

  for (auto& entry : uniqueRegisters) {
    JsonObject dataPoint = entry.second.as<JsonObject>();

    // Validate: Skip if data doesn't have required fields
    if (dataPoint["name"].isNull() || dataPoint["value"].isNull()) {
      continue;  // Silent skip - data validation
    }

    String deviceId = dataPoint["device_id"].as<String>();
    String registerName = dataPoint["name"].as<String>();
    String registerId = dataPoint["register_id"].as<String>();

    if (deviceId.isEmpty() || registerName.isEmpty()) {
      continue;  // Silent skip - empty device_id or register name
    }

    // Optional: Filter by register_id list (for customize mode)
    if (filterRegisters != nullptr) {
      if (std::find(filterRegisters->begin(), filterRegisters->end(),
                    registerId) == filterRegisters->end()) {
        continue;  // Register not in filter list
      }
    }

    // Validate device still exists (not deleted) before publishing
    if (deviceObjects.find(deviceId) == deviceObjects.end()) {
      // Check if we already know this device is deleted
      if (deletedDevices.find(deviceId) != deletedDevices.end()) {
        deletedDevices[deviceId]++;  // Increment skip count
        continue;
      }

      // First time seeing this device in this batch - verify it exists in
      // config
      JsonDocument tempDoc;
      JsonObject tempObj = tempDoc.to<JsonObject>();
      if (!configManager->readDevice(deviceId, tempObj)) {
        // Device deleted - track it and skip
        deletedDevices[deviceId] = 1;  // Start count at 1
        continue;
      }
    }

    // Create device object with device_id as key if not exists
    if (deviceObjects.find(deviceId) == deviceObjects.end()) {
      JsonObject deviceObj = devicesObject[deviceId].to<JsonObject>();

      // Add device_name at device level
      if (!dataPoint["device_name"].isNull()) {
        deviceObj["device_name"] = dataPoint["device_name"];
      }

      // Store reference to this device object for grouping
      deviceObjects[deviceId] = deviceObj;
    }

    // Add register as nested object: devices.{device_id}.{register_name} =
    // {value, unit}
    JsonObject registerObj =
        deviceObjects[deviceId][registerName].to<JsonObject>();
    registerObj["value"] = dataPoint["value"];
    registerObj["unit"] = dataPoint["unit"];

    validRegisterCount++;
  }

  // Log summary for deleted devices (compact - one line)
  if (!deletedDevices.empty()) {
    int totalSkipped = 0;
    for (auto& entry : deletedDevices) {
      totalSkipped += entry.second;
    }
    LOG_MQTT_INFO("[MQTT] Skipped %d registers from %d deleted device(s)\n",
                  totalSkipped, deletedDevices.size());
  }

  return validRegisterCount;
}

/**
 * Helper 3: Serialize and validate JSON payload
 * @param doc JsonDocument to serialize
 * @param payload Output string for serialized JSON
 * @param estimatedSize Estimated size for logging (optional)
 * @return true if serialization successful and valid, false otherwise
 */
bool MqttManager::serializeAndValidatePayload(JsonDocument& doc,
                                              String& payload,
                                              uint32_t estimatedSize) {
  // Serialize batch payload
  size_t serializedSize = serializeJson(doc, payload);

  // Validate serialization success
  if (serializedSize == 0) {
    LOG_MQTT_INFO("[MQTT] ERROR: serializeJson() returned 0 bytes!");
    return false;
  }

  // Validate payload is valid JSON (check first and last characters)
  if (payload.length() > 0) {
    if (payload.charAt(0) != '{' ||
        payload.charAt(payload.length() - 1) != '}') {
      LOG_MQTT_INFO(
          "[MQTT] ERROR: Payload is not valid JSON! First char: '%c', Last "
          "char: '%c'\n",
          payload.charAt(0), payload.charAt(payload.length() - 1));
      if (payload.length() <= 2000) {
        LOG_MQTT_INFO("[MQTT] Invalid payload (%u bytes): %s\n",
                      payload.length(), payload.c_str());
      } else {
        LOG_MQTT_INFO("[MQTT] Invalid payload too large (%u bytes)\n",
                      payload.length());
        // v2.5.35: Use DEV_MODE check to prevent log leak in production
        DEV_SERIAL_PRINTF("  First 200 chars: %s\n",
                          payload.substring(0, 200).c_str());
        DEV_SERIAL_PRINTF(
            "  Last 200 chars: %s\n",
            payload.substring(max(0, (int)payload.length() - 200)).c_str());
      }
      return false;
    }
  }

#if PRODUCTION_MODE == 0
  LOG_MQTT_INFO(
      "[MQTT] Serialization complete: %u bytes (expected ~%u bytes)\n",
      serializedSize, estimatedSize);
#endif

  return true;
}

/**
 * Helper 4: Publish payload to MQTT broker with buffer validation
 * @param topic MQTT topic to publish to
 * @param payload JSON payload string
 * @param modeLabel Label for logging ("Default Mode" or "Customize Mode")
 * @return true if publish successful, false otherwise
 */
bool MqttManager::publishPayload(const String& topic, const String& payload,
                                 const char* modeLabel) {
  // Check buffer size
  if (payload.length() > cachedBufferSize) {
    LOG_MQTT_INFO(
        "[MQTT] ERROR: Payload too large (%d bytes > %u bytes buffer)!\n",
        payload.length(), cachedBufferSize);
    LOG_MQTT_INFO("[MQTT] %s: Splitting needed. Payload size: %d\n", modeLabel,
                  payload.length());
    return false;
  }

  // DEBUG: Print publish request info (only in development mode) - runtime
  // check
  if (IS_DEV_MODE()) {
    Serial.printf("\n[MQTT] PUBLISH REQUEST - %s\n", modeLabel);
    Serial.printf("  Topic: %s\n", topic.c_str());
    Serial.printf("  Size: %u bytes\n", payload.length());

    // Print payload (verbose mode) - show full JSON as one-line
    if (payload.length() > 0) {
      Serial.printf("  Payload: %s\n", payload.c_str());
    } else {
      Serial.println("  Payload: [EMPTY]");
    }
  }

  // Check MQTT connection state before publish
  if (!mqttClient.connected()) {
    LOG_MQTT_ERROR("MQTT client not connected before publish! State: %d",
                   mqttClient.state());
    return false;
  }

  // Calculate total MQTT packet size for validation
  uint32_t mqttPacketSize = 5 + 2 + topic.length() + payload.length();

  // Show broker info in development mode - runtime check
  if (IS_DEV_MODE()) {
    Serial.printf("  Broker: %s:%d\n", brokerAddress.c_str(), brokerPort);
    Serial.printf("  Packet size: %u/%u bytes\n\n", mqttPacketSize,
                  cachedBufferSize);
  }

  // Validate packet size doesn't exceed buffer
  if (mqttPacketSize > cachedBufferSize) {
    LOG_MQTT_INFO(
        "[MQTT] ERROR: Packet size (%u) exceeds buffer (%u)! Cannot publish.\n",
        mqttPacketSize, cachedBufferSize);
    return false;
  }

  // Validate payload is not empty or corrupted
  if (payload.length() == 0 || payload.length() > 16000) {
    LOG_MQTT_INFO("[MQTT] ERROR: Invalid payload size: %u bytes\n",
                  payload.length());
    return false;
  }

  // CRITICAL FIX: Dynamic topic buffer allocation (prevent buffer overflow for
  // long topics) MQTT spec allows topics up to 65535 bytes, but practically
  // limit to 512 bytes
  constexpr uint16_t MAX_TOPIC_LENGTH = 512;

  if (topic.length() > MAX_TOPIC_LENGTH) {
    LOG_MQTT_INFO("[MQTT] ERROR: Topic too long (%u bytes > %u bytes max)\n",
                  topic.length(), MAX_TOPIC_LENGTH);
    LOG_MQTT_INFO("[MQTT] Topic: %s\n", topic.substring(0, 100).c_str());
    return false;
  }

  // Allocate topic buffer dynamically based on actual topic length
  char* topicBuffer =
      (char*)heap_caps_malloc(topic.length() + 1, MALLOC_CAP_8BIT);
  if (!topicBuffer) {
    LOG_MQTT_INFO(
        "[MQTT] ERROR: Failed to allocate %u bytes for topic buffer!\n",
        topic.length() + 1);
    return false;
  }
  // v2.3.15 FIX: Use memcpy for bounds-safe copy (defensive programming)
  memcpy(topicBuffer, topic.c_str(), topic.length() + 1);

  // Allocate separate buffer for payload to prevent String memory issues
  uint8_t* payloadBuffer =
      (uint8_t*)heap_caps_malloc(payload.length(), MALLOC_CAP_8BIT);
  if (!payloadBuffer) {
    LOG_MQTT_INFO(
        "[MQTT] ERROR: Failed to allocate %u bytes for payload buffer!\n",
        payload.length());
    heap_caps_free(topicBuffer);  // Free topic buffer before returning
    return false;
  }

  // Copy payload to dedicated buffer
  memcpy(payloadBuffer, payload.c_str(), payload.length());
  uint32_t payloadLen = payload.length();

  // v2.3.18 FIX: Adaptive retain flag based on payload size
  // Some public brokers (e.g., broker.emqx.io) have undocumented ~2KB limit for
  // retained messages Payloads exceeding this limit are silently dropped by the
  // broker when retain=true Solution: Only use retain for payloads under the
  // safe threshold For larger payloads, publish without retain (real-time
  // delivery still works) v2.5.19 UPDATE: Increased to 16KB (maximum buffer
  // size) for full retained message support Modern brokers (HiveMQ, Mosquitto,
  // EMQX, AWS IoT) support 16KB+ retained messages Note: broker.emqx.io free
  // tier may still drop >2KB, but most production brokers support this
  const uint32_t RETAIN_PAYLOAD_THRESHOLD =
      16384;  // 16KB threshold (maximum buffer size)
  bool useRetain = (payloadLen <= RETAIN_PAYLOAD_THRESHOLD);

#if PRODUCTION_MODE == 0
  if (!useRetain) {
    LOG_MQTT_INFO(
        "[MQTT] Payload %u bytes exceeds retain threshold (%u), publishing "
        "without retain flag\n",
        payloadLen, RETAIN_PAYLOAD_THRESHOLD);
  }
#endif

  bool published = mqttClient.publish(
      topicBuffer,    // Topic (dynamically allocated buffer)
      payloadBuffer,  // Payload (dedicated buffer)
      payloadLen,     // Explicit length
      useRetain       // ADAPTIVE: retain only for small payloads (v2.3.18)
  );

  // CRITICAL: Free buffers after publish to prevent memory leak
  heap_caps_free(topicBuffer);    // Free topic buffer (dynamic allocation)
  heap_caps_free(payloadBuffer);  // Free payload buffer

#if PRODUCTION_MODE == 0
  LOG_MQTT_INFO("[MQTT] Publish: %s | State: %d (%s)\n",
                published ? "SUCCESS" : "FAILED", mqttClient.state(),
                mqttClient.state() == 0 ? "connected" : "disconnected");
#else
  if (!published) {
    LOG_MQTT_ERROR("Publish FAILED! State: %d", mqttClient.state());
  }
#endif

  // Delay for TCP flush (20ms sufficient for 16KB payloads)
  if (published) {
    vTaskDelay(pdMS_TO_TICKS(20));
  }

  return published;
}

/**
 * Helper 5: Calculate display interval with unit conversion
 * @param intervalMs Interval in milliseconds
 * @param unit Original unit string ("ms", "s", "m", etc.)
 * @param displayInterval Output: converted interval value
 * @param displayUnit Output: display unit string
 */
void MqttManager::calculateDisplayInterval(uint32_t intervalMs,
                                           const String& unit,
                                           uint32_t& displayInterval,
                                           const char*& displayUnit) {
  // Normalize unit for comparison (case-insensitive)
  String unitLower = unit;
  unitLower.toLowerCase();

  if (unitLower == "m" || unitLower == "min" || unitLower == "mins" ||
      unitLower == "minute" || unitLower == "minutes") {
    // Convert milliseconds to minutes
    displayInterval = intervalMs / 60000;
    displayUnit = "min";
  } else if (unitLower == "s" || unitLower == "sec" || unitLower == "secs" ||
             unitLower == "second" || unitLower == "seconds") {
    // Convert milliseconds to seconds
    displayInterval = intervalMs / 1000;
    displayUnit = "s";
  } else {
    // Unit is "ms" or unknown - keep as milliseconds
    displayInterval = intervalMs;
    displayUnit = "ms";
  }
}

// clearBatchesAfterPublish() removed - no longer needed with End-of-Batch
// Marker pattern

// ============================================================================
// END OF PHASE 1 HELPER METHODS
// ============================================================================

// ============================================================================
// v2.3.8 PHASE 2: HELPER METHODS - Reduce Method Complexity
// ============================================================================
// These methods split long methods into focused, single-responsibility
// functions Improves readability, maintainability, and testability

/**
 * Helper 7: Load broker configuration (address, port, credentials)
 * @param mqttConfig MQTT configuration JSON object
 */
void MqttManager::loadBrokerConfig(JsonObject& mqttConfig) {
  // FIXED BUG #6: Consistent whitespace trimming for all string configs
  brokerAddress = mqttConfig["broker_address"] | "broker.hivemq.com";
  brokerAddress.trim();

  brokerPort = mqttConfig["broker_port"] | 1883;

  // v2.5.36 FIX: Always generate unique client_id to prevent broker collision
  // Previous bug: Default "esp32_gateway" caused all devices to fight for same
  // client_id Fix: Default to empty string, then auto-generate unique ID based
  // on MAC address
  clientId = mqttConfig["client_id"] | "";
  clientId.trim();

  // Auto-generate unique client_id if empty OR if using old static default
  if (clientId.isEmpty() || clientId == "esp32_gateway") {
    // Generate unique ID based on ESP32 MAC address (last 6 hex digits)
    uint64_t mac = ESP.getEfuseMac();
    clientId = String("MGate1210_") + String((uint32_t)(mac & 0xFFFFFF), HEX);
    LOG_MQTT_INFO("[MQTT] Auto-generated unique client_id: %s\n",
                  clientId.c_str());
  }

  username = mqttConfig["username"] | "";
  username.trim();

  password = mqttConfig["password"] | "";
  password.trim();

  publishMode = mqttConfig["publish_mode"] | "default";

#if PRODUCTION_MODE == 0
  LOG_MQTT_INFO(
      "[MQTT] Config loaded | Broker: %s:%d | Client: %s | Auth: %s | Mode: "
      "%s\n",
      brokerAddress.c_str(), brokerPort, clientId.c_str(),
      (username.length() > 0) ? "YES" : "NO", publishMode.c_str());
#endif
}

/**
 * Helper 8: Load default mode configuration (topic, interval)
 * @param mqttConfig MQTT configuration JSON object
 */
void MqttManager::loadDefaultModeConfig(JsonObject& mqttConfig) {
  if (mqttConfig["default_mode"]) {
    JsonObject defaultMode = mqttConfig["default_mode"];
    defaultModeEnabled = defaultMode["enabled"] | true;

    // FIXED BUG #6: Trim topic strings
    defaultTopicPublish =
        defaultMode["topic_publish"] | "v1/devices/me/telemetry";
    defaultTopicPublish.trim();

    defaultTopicSubscribe = defaultMode["topic_subscribe"] | "device/control";
    defaultTopicSubscribe.trim();

    // Load interval and unit, then convert to milliseconds
    uint32_t intervalValue = defaultMode["interval"] | 5;
    defaultIntervalUnit = defaultMode["interval_unit"] | "s";
    defaultInterval = convertToMilliseconds(intervalValue, defaultIntervalUnit);
    lastDefaultPublish = 0;

#if PRODUCTION_MODE == 0
    LOG_MQTT_INFO(
        "[MQTT] Default Mode: %s | Topic: %s | Interval: %u%s (%ums)\n",
        defaultModeEnabled ? "ENABLED" : "DISABLED",
        defaultTopicPublish.c_str(), intervalValue, defaultIntervalUnit.c_str(),
        defaultInterval);
#endif
  }
}

/**
 * Helper 9: Load customize mode configuration (custom topics with registers)
 * @param mqttConfig MQTT configuration JSON object
 */
void MqttManager::loadCustomizeModeConfig(JsonObject& mqttConfig) {
  customTopics.clear();

  if (mqttConfig["customize_mode"]) {
    JsonObject customizeMode = mqttConfig["customize_mode"];
    customizeModeEnabled = customizeMode["enabled"] | false;

    if (customizeMode["custom_topics"]) {
      JsonArray topics = customizeMode["custom_topics"];
      for (JsonObject topicObj : topics) {
        CustomTopic ct;

        // FIXED BUG #6: Trim custom topic strings
        ct.topic = topicObj["topic"] | "";
        ct.topic.trim();

        // Load interval and unit, then convert to milliseconds
        uint32_t intervalValue = topicObj["interval"] | 5;
        ct.intervalUnit = topicObj["interval_unit"] | "s";
        ct.interval = convertToMilliseconds(intervalValue, ct.intervalUnit);
        ct.lastPublish = 0;

        if (topicObj["registers"]) {
          JsonArray regs = topicObj["registers"];
          for (JsonVariant regVariant : regs) {
            String registerId = regVariant.as<String>();
            ct.registers.push_back(registerId);
          }
        }

        if (!ct.topic.isEmpty() && ct.registers.size() > 0) {
          customTopics.push_back(ct);
#if PRODUCTION_MODE == 0
          LOG_MQTT_INFO(
              "[MQTT] Custom Topic: %s | Registers: %d | Interval: %u%s "
              "(%ums)\n",
              ct.topic.c_str(), ct.registers.size(), intervalValue,
              ct.intervalUnit.c_str(), ct.interval);
#endif
        }
      }
    }

#if PRODUCTION_MODE == 0
    LOG_MQTT_INFO("[MQTT] Customize Mode: %s | Topics: %d\n",
                  customizeModeEnabled ? "ENABLED" : "DISABLED",
                  customTopics.size());
#endif
  }
}

/**
 * Helper 10: Analyze device configurations for timeout calculation
 * @param devices Device array from config
 * @param maxRefreshRate Output: Maximum refresh rate found
 * @param totalRegisters Output: Total register count
 * @param hasSlowRTU Output: Whether slow RTU detected
 */
void MqttManager::analyzeDeviceConfigurations(JsonArray& devices,
                                              uint32_t& maxRefreshRate,
                                              uint32_t& totalRegisters,
                                              bool& hasSlowRTU) {
  maxRefreshRate = 0;
  totalRegisters = 0;
  hasSlowRTU = false;

  int deviceIndex = 0;
  for (JsonVariant deviceVar : devices) {
    JsonObject device = deviceVar.as<JsonObject>();
    String protocol = device["protocol"] | "RTU";
    uint32_t refreshRate = device["refresh_rate_ms"] | 5000;
    uint32_t baudRate = device["baud_rate"] | 9600;
    JsonArray registers = device["registers"];
    uint32_t registerCount = registers.size();

#if PRODUCTION_MODE == 0
    String deviceId = device["device_id"] | "UNKNOWN";
    LOG_MQTT_INFO(
        "[MQTT][DEBUG] Device %d (%s): protocol=%s, refresh=%lums, baud=%lu, "
        "registers=%lu\n",
        deviceIndex, deviceId.c_str(), protocol.c_str(), refreshRate, baudRate,
        registerCount);
#endif

    totalRegisters += registerCount;

    // Track slowest refresh rate
    if (refreshRate > maxRefreshRate) {
      maxRefreshRate = refreshRate;
#if PRODUCTION_MODE == 0
      LOG_MQTT_INFO("[MQTT][DEBUG]   → New maxRefreshRate: %lums\n",
                    maxRefreshRate);
#endif
    }

    // Detect slow RTU configurations (9600 baud with many registers)
    if (protocol == "RTU" && baudRate <= 9600 && registerCount > 20) {
      hasSlowRTU = true;
#if PRODUCTION_MODE == 0
      LOG_MQTT_INFO(
          "[MQTT][DEBUG]   → Slow RTU detected! (baud=%lu, registers=%lu)\n",
          baudRate, registerCount);
#endif
    }

    deviceIndex++;
  }

#if PRODUCTION_MODE == 0
  LOG_MQTT_INFO(
      "[MQTT][DEBUG] Analysis summary: totalRegisters=%lu, "
      "maxRefreshRate=%lums, hasSlowRTU=%s\n",
      totalRegisters, maxRefreshRate, hasSlowRTU ? "YES" : "NO");
#endif
}

/**
 * Helper 11: Determine timeout strategy based on device analysis
 * @param totalRegisters Total register count
 * @param maxRefreshRate Maximum refresh rate
 * @param hasSlowRTU Whether slow RTU was detected
 * @return Calculated timeout in milliseconds
 */
uint32_t MqttManager::determineTimeoutStrategy(uint32_t totalRegisters,
                                               uint32_t maxRefreshRate,
                                               bool hasSlowRTU) {
  uint32_t timeout;
  String calculationReason = "";

  if (totalRegisters == 0) {
    timeout = 1000;  // 1s for empty config
    calculationReason = "no registers";
  } else if (hasSlowRTU) {
    // Slow RTU detected - use longer timeout
    timeout = maxRefreshRate + 2000;  // Refresh rate + 2s margin
    calculationReason = "slow RTU (refresh + 2000ms)";
  } else if (totalRegisters <= 10) {
    // Fast scenario: Few registers
    timeout = 1000;  // 1s timeout
    calculationReason = "few registers (≤10)";
  } else if (totalRegisters <= 50) {
    // Medium scenario: Moderate registers
    timeout = maxRefreshRate + 1000;  // Refresh rate + 1s margin
    calculationReason = "moderate registers (≤50, refresh + 1000ms)";
  } else {
    // Heavy scenario: Many registers
    timeout = maxRefreshRate + 2000;  // Refresh rate + 2s margin
    calculationReason = "many registers (>50, refresh + 2000ms)";
  }

#if PRODUCTION_MODE == 0
  LOG_MQTT_INFO("[MQTT][DEBUG] Timeout BEFORE clamp: %lums (reason: %s)\n",
                timeout, calculationReason.c_str());
#endif

  // Clamp between 1s and 10s
  uint32_t timeoutBeforeClamp = timeout;
  timeout = constrain(timeout, 1000, 10000);

#if PRODUCTION_MODE == 0
  if (timeout != timeoutBeforeClamp) {
    LOG_MQTT_INFO(
        "[MQTT][DEBUG] Timeout AFTER clamp: %lums (clamped from %lums)\n",
        timeout, timeoutBeforeClamp);
  }
#endif

  return timeout;
}

// ============================================================================
// END OF PHASE 2 HELPER METHODS
// ============================================================================

void MqttManager::publishQueueData() {
  if (!mqttClient.connected()) {
    return;
  }

  unsigned long now = millis();

  // ============================================
  // SOLUTION #1: SEPARATE INTERVAL TIMER FROM BATCH WAITING
  // v2.3.7: Fix MQTT publish interval inconsistency
  // ============================================

  // Step 1: Check if interval has elapsed (TIME-BASED TRIGGER)
  bool defaultIntervalElapsed =
      (publishMode == "default" && defaultModeEnabled &&
       (now - lastDefaultPublish) >= defaultInterval);

  bool customizeIntervalElapsed = false;
  if (publishMode == "customize" && customizeModeEnabled) {
    for (auto& customTopic : customTopics) {
      if ((now - customTopic.lastPublish) >= customTopic.interval) {
        customizeIntervalElapsed = true;
        break;
      }
    }
  }

  // If interval not elapsed, don't publish yet (precise timing)
  if (!defaultIntervalElapsed && !customizeIntervalElapsed) {
    return;
  }

  // Step 2: Interval ELAPSED - Capture target timestamp ONCE
  // v2.3.7 CRITICAL FIX: Use instance member to preserve first interval elapsed
  // time v2.3.8 PHASE 1: Moved from static to instance member for thread safety
  // This prevents timestamp from drifting during batch wait period
  xSemaphoreTake(publishStateMutex, portMAX_DELAY);

  // Capture target time ONCE when interval first elapses
  if ((defaultIntervalElapsed || customizeIntervalElapsed) &&
      !publishState.timeLocked) {
    publishState.targetTime = now;  // Lock this time for this publish cycle
    publishState.timeLocked = true;

#if PRODUCTION_MODE == 0
    LOG_MQTT_INFO(
        "[MQTT] ✓ Target time captured at %lu ms (interval elapsed)\n",
        publishState.targetTime);
#endif
  }

  // v2.5.1 CRITICAL FIX: Update lastPublish timestamps IMMEDIATELY when
  // interval elapses This MUST happen BEFORE queue empty check to prevent tight
  // loop spam when no data Previous bug: When queue empty, lastDefaultPublish
  // was never updated, causing defaultIntervalElapsed to stay true and spam
  // "Target time captured" every ~70ms
  if (defaultIntervalElapsed) {
    lastDefaultPublish = publishState.targetTime;
  }
  if (customizeIntervalElapsed) {
    for (auto& customTopic : customTopics) {
      if ((now - customTopic.lastPublish) >= customTopic.interval) {
        customTopic.lastPublish = publishState.targetTime;
      }
    }
  }

// v2.3.7 FIX: Only log ONCE per interval (not every loop!)
// v2.3.8 PHASE 1: Moved from static to instance member for thread safety
#if PRODUCTION_MODE == 0
  if (defaultIntervalElapsed &&
      (publishState.targetTime - publishState.lastLoggedInterval) > 1000) {
    LOG_MQTT_INFO(
        "[MQTT] Default mode interval elapsed - checking batch status at %lu "
        "ms\n",
        publishState.targetTime);
    publishState.lastLoggedInterval = publishState.targetTime;
  }
  if (customizeIntervalElapsed &&
      (publishState.targetTime - publishState.lastLoggedInterval) > 1000) {
    LOG_MQTT_INFO(
        "[MQTT] Customize mode interval elapsed - checking batch status at %lu "
        "ms\n",
        publishState.targetTime);
    publishState.lastLoggedInterval = publishState.targetTime;
  }
#endif

  // Step 3: Check if queue is empty
  // NOTE: Batch waiting logic removed - End-of-Batch Marker pattern handles
  // device completion signaling directly through queue markers
  QueueManager* queueMgr = QueueManager::getInstance();
  if (queueMgr && queueMgr->isEmpty()) {
    // Queue is empty - no data to publish, skip silently
    // v2.5.1: lastDefaultPublish already updated above, so next interval check
    // will be correct
    publishState.timeLocked = false;  // Reset for next interval
    xSemaphoreGive(publishStateMutex);
    return;
  }

#if PRODUCTION_MODE == 0
  if (defaultIntervalElapsed) {
    LOG_MQTT_INFO("[MQTT] Default mode ready to publish at %lu ms\n",
                  publishState.targetTime);
  }
  if (customizeIntervalElapsed) {
    LOG_MQTT_INFO("[MQTT] Customize mode ready to publish at %lu ms\n",
                  publishState.targetTime);
  }
#endif

  // v2.3.7 OPTIMIZED: Process persistent queue AFTER batch wait (before
  // dequeue) This prevents queue processing delays from affecting interval
  // precision
  if (persistentQueueEnabled && persistentQueue) {
    uint32_t persistedSent = persistentQueue->processQueue();
#if PRODUCTION_MODE == 0
    if (persistedSent > 0) {
      LOG_MQTT_INFO("[MQTT] Resent %ld persistent messages\n", persistedSent);
    }
#endif
  }

  // Only dequeue if we're actually going to publish
  std::map<String, JsonDocument> uniqueRegisters;

  // DEBUG: Track dequeue count
  int dequeueCount = 0;

  // Dequeue up to MAX_REGISTERS_PER_PUBLISH registers (configurable limit)
  while (uniqueRegisters.size() < MqttConfig::MAX_REGISTERS_PER_PUBLISH) {
    JsonDocument dataDoc;
    JsonObject dataPoint = dataDoc.to<JsonObject>();

    if (!queueManager->dequeue(dataPoint)) {
      break;  // Queue empty
    }

    dequeueCount++;

    String deviceId = dataPoint["device_id"].as<String>();
    String registerId = dataPoint["register_id"].as<String>();

    if (!deviceId.isEmpty() && !registerId.isEmpty()) {
      // Use device_id + register_id as unique key to prevent data overwrite
      // between devices
      String uniqueKey = deviceId + "_" + registerId;
      uniqueRegisters[uniqueKey] = dataDoc;
    }
  }

  if (uniqueRegisters.empty()) {
    publishState.timeLocked = false;  // Reset for next interval
    xSemaphoreGive(publishStateMutex);
    return;  // Nothing to publish
  }

  // Route to appropriate publish mode
  if (defaultIntervalElapsed) {
    publishDefaultMode(uniqueRegisters, now);
  } else if (customizeIntervalElapsed) {
    publishCustomizeMode(uniqueRegisters, now);
  }

  // v2.3.7 CRITICAL FIX: Reset target time lock for next interval
  // This allows the next interval to capture a fresh timestamp
  publishState.timeLocked = false;

#if PRODUCTION_MODE == 0
  LOG_MQTT_INFO("[MQTT] ✓ Publish cycle complete - ready for next interval\n");
#endif

  // v2.3.8 PHASE 1: Release mutex at end of publish cycle
  xSemaphoreGive(publishStateMutex);
}

// v2.3.8 PHASE 1: REFACTORED - Reduced from 323 lines to ~60 lines using helper
// methods (81% reduction)
void MqttManager::publishDefaultMode(
    std::map<String, JsonDocument>& uniqueRegisters, unsigned long now) {
  // Calculate estimated size for logging/monitoring
  constexpr uint32_t AVG_BYTES_PER_REGISTER = 150;
  constexpr uint32_t JSON_OVERHEAD = 1000;
  uint32_t estimatedSize =
      (uniqueRegisters.size() * AVG_BYTES_PER_REGISTER) + JSON_OVERHEAD;

  // Create JSON document with PSRAM allocation
  SpiRamJsonDocument batchDoc;
  LOG_MQTT_DEBUG(
      "JsonDocument created for %d registers (estimated: ~%u bytes)\n",
      uniqueRegisters.size(), estimatedSize);

  // Helper 1: Build RTC timestamp
  buildTimestamp(batchDoc, now);

  // Create devices object for grouping
  JsonObject devicesObject = batchDoc["devices"].to<JsonObject>();
  std::map<String, JsonObject> deviceObjects;

  // Helper 2: Validate devices and group registers (nullptr = include all
  // registers)
  int totalRegisters = validateAndGroupRegisters(uniqueRegisters, devicesObject,
                                                 deviceObjects, nullptr);

  // Helper 3: Serialize and validate JSON payload
  String payload;
  if (!serializeAndValidatePayload(batchDoc, payload, estimatedSize)) {
    return;  // Serialization or validation failed
  }

  // Helper 4: Publish payload to MQTT broker
  bool published = publishPayload(defaultTopicPublish, payload, "Default Mode");

  if (published) {
    // Helper 5: Calculate display interval with unit conversion
    uint32_t displayInterval;
    const char* displayUnit;
    calculateDisplayInterval(defaultInterval, defaultIntervalUnit,
                             displayInterval, displayUnit);

    LOG_MQTT_INFO(
        "Default Mode: Published %d registers from %d devices to %s (%.1f KB) "
        "/ %u%s\n",
        totalRegisters, deviceObjects.size(), defaultTopicPublish.c_str(),
        payload.length() / 1024.0, displayInterval, displayUnit);

    // Batch clearing no longer needed - End-of-Batch Marker pattern handles
    // this automatically

    if (ledManager) {
      ledManager->notifyDataTransmission();
    }
  } else {
    LOG_MQTT_INFO(
        "[MQTT] Default Mode: Publish failed (payload: %d bytes, buffer: %u "
        "bytes)\n",
        payload.length(), cachedBufferSize);

    // CRITICAL FIX: Prevent "Poison Message" - Don't enqueue payloads that are
    // too large for buffer If payload exceeds buffer size, it will fail again
    // when retried, creating infinite loop
    if (payload.length() > cachedBufferSize) {
      LOG_MQTT_INFO(
          "[MQTT] ERROR: Payload dropped (too large: %d bytes > %u bytes "
          "buffer)\n",
          payload.length(), cachedBufferSize);
      LOG_MQTT_INFO(
          "[MQTT] SOLUTION: Increase MQTT_MAX_PACKET_SIZE or reduce "
          "device/register count");
      // Don't enqueue - message is permanently dropped to prevent queue
      // poisoning
    } else if (persistentQueueEnabled && persistentQueue) {
      // Only enqueue if size is reasonable (failed for other reasons like
      // network)
      JsonObject cleanPayload = batchDoc.as<JsonObject>();
      persistentQueue->enqueueJsonMessage(defaultTopicPublish, cleanPayload,
                                          PRIORITY_NORMAL, 86400000);
    }
  }
}

// v2.3.8 PHASE 1: REFACTORED - Reduced from 342 lines using helper methods
// (similar reduction as default mode)
void MqttManager::publishCustomizeMode(
    std::map<String, JsonDocument>& uniqueRegisters, unsigned long now) {
  // Publish to each custom topic based on its interval
  for (auto& customTopic : customTopics) {
    // Check if interval elapsed for this topic
    if ((now - customTopic.lastPublish) < customTopic.interval) {
      continue;  // Wait for this topic's interval
    }

    // Calculate estimated size for logging
    constexpr uint32_t AVG_BYTES_PER_REGISTER = 150;
    constexpr uint32_t JSON_OVERHEAD = 1000;
    uint32_t estimatedSize =
        (customTopic.registers.size() * AVG_BYTES_PER_REGISTER) + JSON_OVERHEAD;

    // Create JSON document with PSRAM allocation
    SpiRamJsonDocument topicDoc;
    LOG_MQTT_DEBUG(
        "Customize Mode: JsonDocument created for topic %s (%d registers, "
        "estimated: ~%u bytes)\n",
        customTopic.topic.c_str(), customTopic.registers.size(), estimatedSize);

    // Helper 1: Build RTC timestamp
    buildTimestamp(topicDoc, now);

    // Create devices object for grouping
    JsonObject devicesObject = topicDoc["devices"].to<JsonObject>();
    std::map<String, JsonObject> deviceObjects;

    // Helper 2: Validate and group registers (with filter for this topic's
    // registers)
    int registerCount = validateAndGroupRegisters(
        uniqueRegisters, devicesObject, deviceObjects, &customTopic.registers);

    // Only publish if there's data for this topic
    if (registerCount > 0) {
      // Helper 3: Serialize and validate JSON payload
      String payload;
      if (!serializeAndValidatePayload(topicDoc, payload, estimatedSize)) {
        continue;  // Skip to next topic if serialization failed
      }

      // Helper 4: Publish payload to MQTT broker
      bool published =
          publishPayload(customTopic.topic, payload, "Customize Mode");

      if (published) {
        // Helper 5: Calculate display interval with unit conversion
        uint32_t displayInterval;
        const char* displayUnit;
        calculateDisplayInterval(customTopic.interval, customTopic.intervalUnit,
                                 displayInterval, displayUnit);

        LOG_MQTT_INFO(
            "[MQTT] Customize Mode: Published %d registers from %d devices to "
            "%s (%.1f KB) / %u%s\n",
            registerCount, deviceObjects.size(), customTopic.topic.c_str(),
            payload.length() / 1024.0, displayInterval, displayUnit);

        // Batch clearing no longer needed - End-of-Batch Marker pattern handles
        // this automatically

        if (ledManager) {
          ledManager->notifyDataTransmission();
        }
      } else {
        LOG_MQTT_INFO("[MQTT] Customize Mode: Publish failed for topic %s\n",
                      customTopic.topic.c_str());

        // CRITICAL FIX: Prevent "Poison Message" - Don't enqueue payloads that
        // are too large for buffer
        if (payload.length() > cachedBufferSize) {
          LOG_MQTT_INFO(
              "[MQTT] ERROR: Payload dropped for topic %s (too large: %d bytes "
              "> %u bytes buffer)\n",
              customTopic.topic.c_str(), payload.length(), cachedBufferSize);
          LOG_MQTT_INFO(
              "[MQTT] SOLUTION: Increase MQTT_MAX_PACKET_SIZE or reduce "
              "registers in this topic");
          // Don't enqueue - message is permanently dropped to prevent queue
          // poisoning
        } else if (persistentQueueEnabled && persistentQueue) {
          // Only enqueue if size is reasonable (failed for other reasons like
          // network)
          JsonObject cleanPayload = topicDoc.as<JsonObject>();
          persistentQueue->enqueueJsonMessage(customTopic.topic, cleanPayload,
                                              PRIORITY_NORMAL, 86400000);
        }
      }

      vTaskDelay(pdMS_TO_TICKS(10));  // Small delay between topic publishes
    }
  }
}

bool MqttManager::isNetworkAvailable() {
  if (!networkManager) return false;

  // Check if network manager says available
  if (!networkManager->isAvailable()) {
    return false;
  }

  // Check actual IP from network manager
  IPAddress localIP = networkManager->getLocalIP();
  if (localIP == IPAddress(0, 0, 0, 0)) {
    LOG_MQTT_INFO("[MQTT] Network manager available but no IP (%s)\n",
                  networkManager->getCurrentMode().c_str());
    return false;
  }

  return true;
}

void MqttManager::debugNetworkConnectivity() {
  String mode = networkManager->getCurrentMode();
  IPAddress ip = networkManager->getLocalIP();
  bool available = networkManager->isAvailable();

  if (mode == "WIFI") {
    LOG_MQTT_INFO(
        "[MQTT] Network Debug: WiFi %s  SSID: %s  RSSI: %d dBm  IP: %s  "
        "Available: %s\n",
        WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected",
        WiFi.SSID().c_str(), WiFi.RSSI(), ip.toString().c_str(),
        available ? "YES" : "NO");
  } else if (mode == "ETH") {
    LOG_MQTT_INFO("[MQTT] Network Debug: Ethernet  IP: %s  Available: %s\n",
                  ip.toString().c_str(), available ? "YES" : "NO");
  } else {
    LOG_MQTT_INFO("[MQTT] Network Debug: Mode: %s  IP: %s  Available: %s\n",
                  mode.c_str(), ip.toString().c_str(),
                  available ? "YES" : "NO");
  }
}

void MqttManager::getStatus(JsonObject& status) {
  status["running"] = running;
  status["service_type"] = "mqtt_manager";
  status["mqtt_connected"] = mqttClient.connected();
  status["wifi_connected"] = (WiFi.status() == WL_CONNECTED);
  status["broker_address"] = brokerAddress;
  status["broker_port"] = brokerPort;
  status["client_id"] = clientId;
  status["topic_publish"] = topicPublish;
  status["queue_size"] = queueManager->size();
  status["publish_mode"] = publishMode;  // v2.2.0: Show current publish mode
                                         // instead of legacy data_interval_ms
}

// Persistent queue management methods
void MqttManager::setPersistentQueueEnabled(bool enable) {
  persistentQueueEnabled = enable;
  if (persistentQueue) {
    if (enable) {
      LOG_MQTT_INFO("[MQTT] Persistent queue enabled");
    } else {
      LOG_MQTT_INFO("[MQTT] Persistent queue disabled");
    }
  }
}

bool MqttManager::isPersistentQueueEnabled() const {
  return persistentQueueEnabled;
}

MQTTPersistentQueue* MqttManager::getPersistentQueue() const {
  return persistentQueue;
}

void MqttManager::printQueueStatus() const {
  if (persistentQueue) {
    persistentQueue->printHealthReport();
  } else {
    LOG_MQTT_INFO("[MQTT] Persistent queue not initialized");
  }
}

uint32_t MqttManager::getQueuedMessageCount() const {
  if (persistentQueue) {
    return persistentQueue->getPendingMessageCount();
  }
  return 0;
}

// ============================================
// FIXED BUG #15: DYNAMIC MQTT BUFFER SIZING
// ============================================
uint16_t MqttManager::calculateOptimalBufferSize() {
  if (!configManager) {
    // Fallback to conservative default if no config
    return 4096;  // 4KB default
  }

  // Load all devices to count total registers
  // Use getAllDevicesWithRegisters() to get device data
  // BUG #31: JsonDocument will use PSRAM via DefaultAllocator override
  // (JsonDocumentPSRAM.h)
  JsonDocument devicesDoc;  // Automatically uses PSRAM allocator from
                            // JsonDocumentPSRAM.h
  JsonArray devices = devicesDoc.to<JsonArray>();
  configManager->getAllDevicesWithRegisters(devices, true);  // minimal fields

  if (devices.size() == 0) {
    return 4096;  // 4KB default if no devices
  }

  uint32_t totalRegisters = 0;
  uint32_t maxDeviceRegisters = 0;

  // Count total and max registers
  for (JsonVariant deviceVar : devices) {
    JsonObject device = deviceVar.as<JsonObject>();
    JsonArray registers = device["registers"];
    uint32_t deviceRegCount = registers.size();

    totalRegisters += deviceRegCount;
    if (deviceRegCount > maxDeviceRegisters) {
      maxDeviceRegisters = deviceRegCount;
    }
  }

  // Calculate buffer size based on total registers
  // Formula: (totalRegisters * BYTES_PER_REGISTER) + BUFFER_OVERHEAD
  // - Each register: ~50-70 bytes (name, value, unit, timestamp, device_id)
  // - Batch overhead: ~300 bytes (JSON structure, device metadata)
  uint32_t calculatedSize = (totalRegisters * MqttConfig::BYTES_PER_REGISTER) +
                            MqttConfig::BUFFER_OVERHEAD;

  // Apply constraints
  uint16_t optimalSize = MqttConfig::DEFAULT_BUFFER_SIZE;

  if (calculatedSize < MqttConfig::MIN_BUFFER_SIZE) {
    optimalSize = MqttConfig::MIN_BUFFER_SIZE;
  } else if (calculatedSize > MqttConfig::MAX_BUFFER_SIZE) {
    optimalSize = MqttConfig::MAX_BUFFER_SIZE;
    LOG_MQTT_INFO(
        "[MQTT] WARNING: Calculated buffer (%lu bytes) exceeds max (%u "
        "bytes)\n",
        calculatedSize, MqttConfig::MAX_BUFFER_SIZE);
    LOG_MQTT_INFO(
        "[MQTT] Consider reducing devices/registers or enabling payload "
        "splitting\n");
  } else {
    optimalSize = (uint16_t)calculatedSize;
  }

#if PRODUCTION_MODE == 0
  LOG_MQTT_INFO(
      "[MQTT] Buffer calculation: %u registers = %u bytes (min: %u, max: %u)\n",
      totalRegisters, optimalSize, MqttConfig::MIN_BUFFER_SIZE,
      MqttConfig::MAX_BUFFER_SIZE);
#endif

  return optimalSize;
}

// ============================================
// v2.3.7: ADAPTIVE BATCH TIMEOUT CALCULATION
// v2.3.8 PHASE 2: REFACTORED - Reduced from 128 lines to ~30 lines using helper
// methods (77% reduction)
// ============================================
uint32_t MqttManager::calculateAdaptiveBatchTimeout() {
  if (!configManager) {
#if PRODUCTION_MODE == 0
    LOG_MQTT_INFO("[MQTT][DEBUG] No ConfigManager - returning default 2000ms");
#endif
    return 2000;  // Default 2s if no config
  }

  // Load all devices to analyze configuration
  JsonDocument devicesDoc;
  JsonArray devices = devicesDoc.to<JsonArray>();
  configManager->getAllDevicesWithRegisters(devices, true);  // minimal fields

#if PRODUCTION_MODE == 0
  LOG_MQTT_INFO("[MQTT][DEBUG] Loaded %d devices for timeout calculation\n",
                devices.size());
#endif

  if (devices.size() == 0) {
#if PRODUCTION_MODE == 0
    LOG_MQTT_INFO("[MQTT][DEBUG] No devices - returning default 1000ms");
#endif
    return 1000;  // 1s default if no devices
  }

  // Helper 10: Analyze device configurations (refresh rates, register counts,
  // RTU detection)
  uint32_t maxRefreshRate = 0;
  uint32_t totalRegisters = 0;
  bool hasSlowRTU = false;
  analyzeDeviceConfigurations(devices, maxRefreshRate, totalRegisters,
                              hasSlowRTU);

  // Helper 11: Determine timeout strategy based on analysis results
  uint32_t timeout =
      determineTimeoutStrategy(totalRegisters, maxRefreshRate, hasSlowRTU);

#if PRODUCTION_MODE == 0
  LOG_MQTT_INFO(
      "[MQTT] ✓ Adaptive batch timeout: %lums (devices: %d, registers: %lu, "
      "max_refresh: %lums)\n",
      timeout, devices.size(), totalRegisters, maxRefreshRate);
#endif

  return timeout;
}

// FIXED BUG #5: Notify MQTT manager when device configuration changes
// This invalidates the cached buffer size calculation
// v2.3.8 PHASE 1: Added mutex protection for thread safety
void MqttManager::notifyConfigChange() {
  xSemaphoreTake(bufferCacheMutex, portMAX_DELAY);
  bufferSizeNeedsRecalculation = true;
  xSemaphoreGive(bufferCacheMutex);

  // CRITICAL FIX: Reset batchTimeout cache to force recalculation with new
  // device configs
  xSemaphoreTake(publishStateMutex, portMAX_DELAY);
  publishState.batchTimeout = 0;
  xSemaphoreGive(publishStateMutex);

  LOG_MQTT_INFO(
      "[MQTT] Config change detected - buffer size and batch timeout will be "
      "recalculated");
}

MqttManager::~MqttManager() {
  stop();

  // v2.3.8 PHASE 1: Delete mutexes for cleanup
  if (bufferCacheMutex != NULL) {
    vSemaphoreDelete(bufferCacheMutex);
    bufferCacheMutex = NULL;
  }
  if (publishStateMutex != NULL) {
    vSemaphoreDelete(publishStateMutex);
    publishStateMutex = NULL;
  }

  // v2.5.1 FIX: Delete event group for cleanup
  if (taskExitEvent != NULL) {
    vEventGroupDelete(taskExitEvent);
    taskExitEvent = NULL;
  }
}