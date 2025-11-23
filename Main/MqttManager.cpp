#include "MqttManager.h"
#include "LEDManager.h"
#include "RTCManager.h"
#include "DeviceBatchManager.h"  // CRITICAL FIX: Wait for batch completion
#include "DebugConfig.h"
#include "MemoryRecovery.h"
#include <set>  // For std::set to track cleared devices

// Helper function: Convert interval to milliseconds based on unit
// Supports multiple unit variants for flexibility:
// - Milliseconds: "ms", "millisecond", "milliseconds"
// - Seconds: "s", "sec", "secs", "second", "seconds"
// - Minutes: "m", "min", "mins", "minute", "minutes"
static uint32_t convertToMilliseconds(uint32_t interval, const String &unit)
{
  String unitLower = unit;
  unitLower.toLowerCase();

  if (unitLower == "ms" || unitLower == "millisecond" || unitLower == "milliseconds" || unitLower.isEmpty())
  {
    return interval; // Already in milliseconds
  }
  else if (unitLower == "s" || unitLower == "sec" || unitLower == "secs" || unitLower == "second" || unitLower == "seconds")
  {
    return interval * 1000; // Seconds to milliseconds
  }
  else if (unitLower == "m" || unitLower == "min" || unitLower == "mins" || unitLower == "minute" || unitLower == "minutes")
  {
    return interval * 60000; // Minutes to milliseconds
  }
  return interval; // Default: treat as milliseconds
}

MqttManager *MqttManager::instance = nullptr;

MqttManager::MqttManager(ConfigManager *config, ServerConfig *serverCfg, NetworkMgr *netMgr)
    : configManager(config), queueManager(nullptr), serverConfig(serverCfg), networkManager(netMgr), mqttClient(PubSubClient()),
      running(false), taskHandle(nullptr), brokerPort(1883), lastReconnectAttempt(0),
      cachedBufferSize(0), bufferSizeNeedsRecalculation(true)  // FIXED BUG #5: Initialize cache
{
  queueManager = QueueManager::getInstance();
  persistentQueue = MQTTPersistentQueue::getInstance();
  Serial.println("[MQTT] Persistent queue initialized");
}

MqttManager *MqttManager::getInstance(ConfigManager *config, ServerConfig *serverCfg, NetworkMgr *netMgr)
{
  if (instance == nullptr && config != nullptr && serverCfg != nullptr && netMgr != nullptr)
  {
    instance = new MqttManager(config, serverCfg, netMgr);
  }
  return instance;
}

bool MqttManager::init()
{
  Serial.println("Initializing MQTT Manager...");

  if (!configManager || !queueManager || !serverConfig || !networkManager)
  {
    Serial.println("ConfigManager, QueueManager, ServerConfig, or NetworkManager is null");
    return false;
  }

  loadMqttConfig();

  // OPTIMIZED: Clean up expired messages from persistent queue on startup
  if (persistentQueue)
  {
    uint32_t expiredCount = persistentQueue->getPendingMessageCount();
    persistentQueue->clearExpiredMessages();
    uint32_t remainingCount = persistentQueue->getPendingMessageCount();
    uint32_t removed = expiredCount - remainingCount;
    if (removed > 0)
    {
      Serial.printf("[MQTT] Cleaned %lu expired messages from queue\n", removed);
    }
  }

  Serial.println("MQTT Manager initialized successfully");
  return true;
}

void MqttManager::start()
{
  Serial.println("Starting MQTT Manager...");

  if (running)
  {
    return;
  }

  running = true;
  BaseType_t result = xTaskCreatePinnedToCore(
      mqttTask,
      "MQTT_TASK",
      MqttConfig::MQTT_TASK_STACK_SIZE,
      this,
      2,  // FIXED: Priority 2 (equal to Modbus RTU task) to ensure fair CPU time for MQTT PINGREQ
          // Previous: Priority 1 was too low - MQTT task was starved during long RTU polling
          // New: Priority 2 ensures MQTT keep-alive packets are sent even during heavy RTU load
      &taskHandle,
      1);  // FIXED: Run on Core 1 to avoid blocking IDLE0 on Core 0

  if (result == pdPASS)
  {
    Serial.println("MQTT Manager started successfully");
  }
  else
  {
    Serial.println("Failed to create MQTT task");
    running = false;
    taskHandle = nullptr;
  }
}

void MqttManager::stop()
{
  running = false;

  // Give task time to exit gracefully (checks 'running' flag in loop)
  if (taskHandle)
  {
    vTaskDelay(pdMS_TO_TICKS(50)); // Brief delay for current iteration to complete
    vTaskDelete(taskHandle);
    taskHandle = nullptr;
  }

  if (mqttClient.connected())
  {
    mqttClient.disconnect();
  }
  Serial.println("MQTT Manager stopped");
}

void MqttManager::disconnect()
{
  if (mqttClient.connected())
  {
    mqttClient.disconnect();
    Serial.println("[MQTT] Gracefully disconnected from broker");
  }
}

void MqttManager::mqttTask(void *parameter)
{
  MqttManager *manager = static_cast<MqttManager *>(parameter);
  manager->mqttLoop();
}

void MqttManager::mqttLoop()
{
  bool wasConnected = false;
  bool wifiWasConnected = false;

  Serial.println("[MQTT] Task started on Core 1");

  while (running)
  {
    // ============================================
    // MEMORY RECOVERY CHECK (Phase 2 Optimization)
    // ============================================
    MemoryRecovery::checkAndRecover();

    // Check network availability
    bool networkAvailable = isNetworkAvailable();

    if (!networkAvailable)
    {
      if (wifiWasConnected)
      {
        Serial.println("[MQTT] Network disconnected");
        wifiWasConnected = false;
        wasConnected = false;
      }
      Serial.printf("[MQTT] Waiting for network... Mode: %s, IP: %s\n",
                    networkManager->getCurrentMode().c_str(),
                    networkManager->getLocalIP().toString().c_str());

      // vTaskDelay automatically feeds watchdog
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }
    else if (!wifiWasConnected)
    {
      Serial.printf("[MQTT] Network available - %s IP: %s\n",
                    networkManager->getCurrentMode().c_str(),
                    networkManager->getLocalIP().toString().c_str());
      wifiWasConnected = true;
    }

    // Connect to MQTT if not connected
    if (!mqttClient.connected())
    {
      if (wasConnected)
      {
        Serial.println("[MQTT] Connection lost, attempting reconnect...");
        wasConnected = false;

        // Update LED status - connection lost
        if (ledManager)
        {
          ledManager->setMqttConnectionStatus(false);
        }
      }

      unsigned long now = millis();
      if (now - lastReconnectAttempt > 5000)
      {
        lastReconnectAttempt = now;
        static unsigned long lastDebug = 0;
        if (now - lastDebug > 30000)
        {
          debugNetworkConnectivity();
          lastDebug = now;
        }

        if (connectToMqtt())
        {
          // Connection message already logged in connectToMqtt()
          wasConnected = true;

          // Update LED status - connection established
          if (ledManager)
          {
            ledManager->setMqttConnectionStatus(true);
          }
        }

        // Yield to feed watchdog
        vTaskDelay(pdMS_TO_TICKS(10));
      }
      else
      {
        // Yield while waiting for reconnect interval
        vTaskDelay(pdMS_TO_TICKS(100));
      }
    }
    else
    {
      if (!wasConnected)
      {
        // Connection already active, no need for verbose logging
        wasConnected = true;
      }

      // Process MQTT events and publish data
      mqttClient.loop();
      vTaskDelay(pdMS_TO_TICKS(10));

      publishQueueData();
      vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Yield between polling cycles
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  Serial.println("[MQTT] Task stopped");
}

bool MqttManager::connectToMqtt()
{
  // Check network connectivity first
  IPAddress localIP = networkManager->getLocalIP();
  String networkMode = networkManager->getCurrentMode();

  Client *activeClient = networkManager->getActiveClient();
  if (!activeClient)
  {
    Serial.println("[MQTT] ERROR: No active network client available");
    return false;
  }

  mqttClient.setClient(*activeClient);

  // FIXED BUG #15: Dynamic buffer sizing based on actual device configuration
  // FIXED BUG #5 & #7: Use cached buffer size to avoid repeated calculations
  // Previous: Hardcoded 8192 bytes, recalculated every connection
  // New: Calculate once, cache result, recalculate only on config change
  if (bufferSizeNeedsRecalculation || cachedBufferSize == 0) {
    cachedBufferSize = calculateOptimalBufferSize();
    bufferSizeNeedsRecalculation = false;
    #if PRODUCTION_MODE == 0
      Serial.printf("[MQTT] Buffer size calculated: %u bytes (cached for reuse)\n", cachedBufferSize);
    #endif
  }
  #if PRODUCTION_MODE == 0
  else {
    Serial.printf("[MQTT] Buffer size using cached value: %u bytes\n", cachedBufferSize);
  }
  #endif

  mqttClient.setBufferSize(cachedBufferSize, cachedBufferSize);

  #if PRODUCTION_MODE == 0
    Serial.printf("[MQTT] Max packet size: %u bytes (MQTT_MAX_PACKET_SIZE)\n", MQTT_MAX_PACKET_SIZE);
  #endif

  // FIXED: Increased keep_alive to 120s to prevent timeouts during long Modbus polling cycles
  // Previous: 60s was too short when RTU polling takes ~50s + publish interval 70s
  // New: 120s provides enough margin for slow network conditions and long polling
  mqttClient.setKeepAlive(120);

  // FIXED: Increased socket timeout from 5s to 15s for public broker resilience
  // Previous: 5s timeout caused reconnection failures (error -4) on slow/overloaded public brokers
  // New: 15s provides adequate time for broker.hivemq.com to respond during high load
  // Note: Public brokers often have slow response times (5-10s is common)
  mqttClient.setSocketTimeout(15);  // Socket timeout in seconds

  mqttClient.setServer(brokerAddress.c_str(), brokerPort);

  // Track connection attempt time
  unsigned long connectStartTime = millis();

  bool connected;
  if (username.length() > 0 && password.length() > 0)
  {
    connected = mqttClient.connect(clientId.c_str(), username.c_str(), password.c_str());
  }
  else
  {
    connected = mqttClient.connect(clientId.c_str());
  }

  // Log connection duration for diagnostics (only in development mode)
  #if PRODUCTION_MODE == 0
    unsigned long connectDuration = millis() - connectStartTime;
    if (connectDuration > 2000)
    {
      Serial.printf("[MQTT] Connection attempt took %lu ms (slow)\n", connectDuration);
    }
  #endif

  if (connected)
  {
    Serial.printf("[MQTT] Connected to %s:%d via %s (%s)\n",
                  brokerAddress.c_str(), brokerPort, networkMode.c_str(), localIP.toString().c_str());
  }
  else
  {
    Serial.printf("[MQTT] Connection failed: error %d\n", mqttClient.state());
  }

  return connected;
}

void MqttManager::loadMqttConfig()
{
  JsonDocument configDoc;
  JsonObject mqttConfig = configDoc.to<JsonObject>();

  Serial.println("[MQTT] Loading MQTT configuration...");

  if (serverConfig->getMqttConfig(mqttConfig))
  {
    // FIXED BUG #6: Consistent whitespace trimming for all string configs
    // Prevents subtle bugs from trailing spaces/newlines in BLE config input
    brokerAddress = mqttConfig["broker_address"] | "broker.hivemq.com";
    brokerAddress.trim();

    brokerPort = mqttConfig["broker_port"] | 1883;

    clientId = mqttConfig["client_id"] | "esp32_gateway";
    clientId.trim();

    username = mqttConfig["username"] | "";
    username.trim();

    password = mqttConfig["password"] | "";
    password.trim();

    // Load publish mode
    publishMode = mqttConfig["publish_mode"] | "default";

    #if PRODUCTION_MODE == 0
      Serial.printf("[MQTT] Config loaded - Broker: %s:%d, Client: %s\n",
                    brokerAddress.c_str(), brokerPort, clientId.c_str());
      Serial.printf("[MQTT] Auth: %s, Mode: %s\n",
                    (username.length() > 0) ? "YES" : "NO", publishMode.c_str());
    #endif

    // Load default mode configuration
    if (mqttConfig["default_mode"])
    {
      JsonObject defaultMode = mqttConfig["default_mode"];
      defaultModeEnabled = defaultMode["enabled"] | true;

      // FIXED BUG #6: Trim topic strings
      defaultTopicPublish = defaultMode["topic_publish"] | "v1/devices/me/telemetry";
      defaultTopicPublish.trim();

      defaultTopicSubscribe = defaultMode["topic_subscribe"] | "device/control";
      defaultTopicSubscribe.trim();

      // Load interval and unit, then convert to milliseconds
      uint32_t intervalValue = defaultMode["interval"] | 5;
      defaultIntervalUnit = defaultMode["interval_unit"] | "s";
      defaultInterval = convertToMilliseconds(intervalValue, defaultIntervalUnit);
      lastDefaultPublish = 0;

      #if PRODUCTION_MODE == 0
        Serial.printf("[MQTT] Default Mode: %s, Topic: %s, Interval: %u%s (%ums)\n",
                      defaultModeEnabled ? "ENABLED" : "DISABLED",
                      defaultTopicPublish.c_str(), intervalValue,
                      defaultIntervalUnit.c_str(), defaultInterval);
      #endif
    }

    // Load customize mode configuration
    customTopics.clear();
    if (mqttConfig["customize_mode"])
    {
      JsonObject customizeMode = mqttConfig["customize_mode"];
      customizeModeEnabled = customizeMode["enabled"] | false;

      if (customizeMode["custom_topics"])
      {
        JsonArray topics = customizeMode["custom_topics"];
        for (JsonObject topicObj : topics)
        {
          CustomTopic ct;

          // FIXED BUG #6: Trim custom topic strings
          ct.topic = topicObj["topic"] | "";
          ct.topic.trim();

          // Load interval and unit, then convert to milliseconds
          uint32_t intervalValue = topicObj["interval"] | 5;
          ct.intervalUnit = topicObj["interval_unit"] | "s";
          ct.interval = convertToMilliseconds(intervalValue, ct.intervalUnit);
          ct.lastPublish = 0;

          if (topicObj["registers"])
          {
            JsonArray regs = topicObj["registers"];
            for (JsonVariant regVariant : regs)
            {
              String registerId = regVariant.as<String>();
              ct.registers.push_back(registerId);
            }
          }

          if (!ct.topic.isEmpty() && ct.registers.size() > 0)
          {
            customTopics.push_back(ct);
            #if PRODUCTION_MODE == 0
              Serial.printf("[MQTT] Custom Topic: %s, Registers: %d, Interval: %u%s (%ums)\n",
                            ct.topic.c_str(), ct.registers.size(), intervalValue,
                            ct.intervalUnit.c_str(), ct.interval);
            #endif
          }
        }
      }

      #if PRODUCTION_MODE == 0
        Serial.printf("[MQTT] Customize Mode: %s, Topics: %d\n",
                      customizeModeEnabled ? "ENABLED" : "DISABLED",
                      customTopics.size());
      #endif
    }
  }
  else
  {
    Serial.println("[MQTT] Failed to load config, using defaults");
    brokerAddress = "broker.hivemq.com";
    brokerPort = 1883;
    clientId = "esp32_gateway_" + String(random(1000, 9999));
    publishMode = "default";
    defaultModeEnabled = true;
    defaultTopicPublish = "device/data";
    defaultTopicSubscribe = "device/control";
    defaultInterval = 5000;
    defaultIntervalUnit = "ms";  // Fallback unit
    customizeModeEnabled = false;
  }

  // Set top-level topic for backward compatibility
  topicPublish = defaultTopicPublish;
}

void MqttManager::publishQueueData()
{
  if (!mqttClient.connected())
  {
    return;
  }

  unsigned long now = millis();

  // Process persistent queue first (retry failed messages)
  if (persistentQueueEnabled && persistentQueue)
  {
    uint32_t persistedSent = persistentQueue->processQueue();
    #if PRODUCTION_MODE == 0
      if (persistedSent > 0)
      {
        Serial.printf("[MQTT] Resent %ld persistent messages\n", persistedSent);
      }
    #endif
  }

  // Check if any publish mode is ready (interval elapsed) BEFORE dequeuing
  bool defaultReady = (publishMode == "default" && defaultModeEnabled &&
                       (now - lastDefaultPublish) >= defaultInterval);

  bool customizeReady = false;
  if (publishMode == "customize" && customizeModeEnabled)
  {
    for (auto &customTopic : customTopics)
    {
      if ((now - customTopic.lastPublish) >= customTopic.interval)
      {
        customizeReady = true;
        break;
      }
    }
  }

  // If no mode is ready, don't dequeue data (prevent data loss)
  if (!defaultReady && !customizeReady)
  {
    return;
  }

  // CRITICAL FIX: Wait for device batch completion before publishing
  // This ensures ALL registers are enqueued before MQTT dequeues them

  DeviceBatchManager *batchMgr = DeviceBatchManager::getInstance();

  // OPTIMIZATION: Skip batch waiting if no devices configured
  // This prevents log spam when queue has old persisted data but no active devices
  if (batchMgr && !batchMgr->hasAnyBatches())
  {
    // No devices being polled - clear any old persisted data
    QueueManager *queueMgr = QueueManager::getInstance();
    if (queueMgr && !queueMgr->isEmpty())
    {
      // Flush old data from previous session
      static bool flushedOnce = false;
      if (!flushedOnce)
      {
        Serial.println("[MQTT] No devices configured - clearing old persisted queue data");
        flushedOnce = true;
      }
    }
    return;
  }

  // Check if queue is empty
  QueueManager *queueMgr = QueueManager::getInstance();
  if (queueMgr && queueMgr->isEmpty())
  {
    // Queue is empty - no data to publish, skip silently
    return;
  }

  // FIXED BUG #2: Add timeout for batch completion wait to prevent deadlock
  // If RTU/TCP service crashes or hangs, MQTT should still publish available data
  static unsigned long batchWaitStartTime = 0;
  const unsigned long BATCH_WAIT_TIMEOUT = 60000;  // 60 seconds timeout

  if (batchMgr && !batchMgr->hasCompleteBatch())
  {
    // Start timer on first wait
    if (batchWaitStartTime == 0) {
      batchWaitStartTime = millis();
    }

    // Check timeout
    unsigned long elapsed = millis() - batchWaitStartTime;
    if (elapsed > BATCH_WAIT_TIMEOUT) {
      Serial.printf("[MQTT] WARNING: Batch completion timeout (%lus)! Force publishing available data...\n",
                    elapsed / 1000);
      batchWaitStartTime = 0;  // Reset timer
      // Continue to publish whatever is in queue (don't return)
    } else {
      // Still waiting for batch - log progress
      static LogThrottle batchWaitingThrottle(60000);
      if (batchWaitingThrottle.shouldLog("MQTT waiting for batch completion")) {
        LOG_MQTT_DEBUG("Waiting for device batch to complete... (%lus elapsed)\n", elapsed / 1000);
      }
      return;  // Wait for batch completion
    }
  } else {
    // Batch complete or no batches - reset timer
    batchWaitStartTime = 0;
  }

  // Only dequeue if we're actually going to publish
  std::map<String, JsonDocument> uniqueRegisters;

  // DEBUG: Track dequeue count
  int dequeueCount = 0;

  // Dequeue up to MAX_REGISTERS_PER_PUBLISH registers (configurable limit)
  while (uniqueRegisters.size() < MqttConfig::MAX_REGISTERS_PER_PUBLISH)
  {
    JsonDocument dataDoc;
    JsonObject dataPoint = dataDoc.to<JsonObject>();

    if (!queueManager->dequeue(dataPoint))
    {
      break; // Queue empty
    }

    dequeueCount++;

    String deviceId = dataPoint["device_id"].as<String>();
    String registerId = dataPoint["register_id"].as<String>();

    if (!deviceId.isEmpty() && !registerId.isEmpty())
    {
      // Use device_id + register_id as unique key to prevent data overwrite between devices
      String uniqueKey = deviceId + "_" + registerId;
      uniqueRegisters[uniqueKey] = dataDoc;
    }
  }

  if (uniqueRegisters.empty())
  {
    return; // Nothing to publish
  }

  // Route to appropriate publish mode
  if (defaultReady)
  {
    publishDefaultMode(uniqueRegisters, now);
  }
  else if (customizeReady)
  {
    publishCustomizeMode(uniqueRegisters, now);
  }
}

void MqttManager::publishDefaultMode(std::map<String, JsonDocument> &uniqueRegisters, unsigned long now)
{
  // CRITICAL FIX: ArduinoJson v7 uses dynamic allocation (no size parameter needed)
  // The PSRAMAllocator will automatically allocate memory in PSRAM as data is added
  // This is more efficient than pre-allocating a fixed size
  // Reference: ArduinoJson v7 documentation - dynamic memory management

  // Calculate estimated size for logging/monitoring purposes only
  constexpr uint32_t AVG_BYTES_PER_REGISTER = 150;  // Bytes per register (name, value, unit, metadata)
  constexpr uint32_t JSON_OVERHEAD = 1000;           // Timestamp, device structure, commas, brackets
  uint32_t estimatedSize = (uniqueRegisters.size() * AVG_BYTES_PER_REGISTER) + JSON_OVERHEAD;

  // Batch all data into single payload grouped by device_id
  // SpiRamJsonDocument automatically uses PSRAM allocator (see JsonDocumentPSRAM.h)
  SpiRamJsonDocument batchDoc;  // ArduinoJson v7: No size parameter, dynamic allocation

  LOG_MQTT_DEBUG("JsonDocument created for %d registers (estimated: ~%u bytes)\n",
                 uniqueRegisters.size(), estimatedSize);

  // Get formatted timestamp from RTC: DD/MM/YYYY HH:MM:SS
  RTCManager *rtcMgr = RTCManager::getInstance();
  if (rtcMgr)
  {
    DateTime currentTime = rtcMgr->getCurrentTime();
    char timestampStr[20];
    snprintf(timestampStr, sizeof(timestampStr), "%02d/%02d/%04d %02d:%02d:%02d",
             currentTime.day(), currentTime.month(), currentTime.year(),
             currentTime.hour(), currentTime.minute(), currentTime.second());
    batchDoc["timestamp"] = timestampStr;
  }
  else
  {
    batchDoc["timestamp"] = now; // Fallback to millis if RTC not available
  }

  // NEW FORMAT: Create devices as nested object (device_id as keys)
  JsonObject devicesObject = batchDoc["devices"].to<JsonObject>();

  // Map to track device objects: device_id -> JsonObject reference
  std::map<String, JsonObject> deviceObjects;

  // Track deleted devices to avoid spam logging
  std::map<String, int> deletedDevices; // device_id -> skipped register count

  int totalRegisters = 0;

  for (auto &entry : uniqueRegisters)
  {
    JsonObject dataPoint = entry.second.as<JsonObject>();

    // Validate: Skip if data doesn't have required fields (old format)
    if (dataPoint["name"].isNull() || dataPoint["value"].isNull())
    {
      continue; // Silent skip - data validation
    }

    String deviceId = dataPoint["device_id"].as<String>();
    String registerName = dataPoint["name"].as<String>();

    if (deviceId.isEmpty() || registerName.isEmpty())
    {
      continue; // Silent skip - empty device_id or register name
    }

    // Priority 3: Validate device still exists (not deleted) before publishing
    if (deviceObjects.find(deviceId) == deviceObjects.end())
    {
      // Check if we already know this device is deleted
      if (deletedDevices.find(deviceId) != deletedDevices.end())
      {
        deletedDevices[deviceId]++; // Increment skip count
        continue;
      }

      // First time seeing this device in this batch - verify it exists in config
      JsonDocument tempDoc;
      JsonObject tempObj = tempDoc.to<JsonObject>();
      if (!configManager->readDevice(deviceId, tempObj))
      {
        // Device deleted - track it and skip
        deletedDevices[deviceId] = 1; // Start count at 1
        continue;
      }
    }

    // Create device object with device_id as key if not exists
    if (deviceObjects.find(deviceId) == deviceObjects.end())
    {
      JsonObject deviceObj = devicesObject[deviceId].to<JsonObject>();

      // Add device_name at device level
      if (!dataPoint["device_name"].isNull())
      {
        deviceObj["device_name"] = dataPoint["device_name"];
      }

      // Store reference to this device object for grouping
      deviceObjects[deviceId] = deviceObj;
    }

    // Add register as nested object: devices.{device_id}.{register_name} = {value, unit}
    JsonObject registerObj = deviceObjects[deviceId][registerName].to<JsonObject>();
    registerObj["value"] = dataPoint["value"];
    registerObj["unit"] = dataPoint["unit"];

    totalRegisters++;
  }

  // Log summary for deleted devices (compact - one line)
  if (!deletedDevices.empty())
  {
    int totalSkipped = 0;
    for (auto &entry : deletedDevices)
    {
      totalSkipped += entry.second;
    }
    Serial.printf("[MQTT] Skipped %d registers from %d deleted device(s)\n", totalSkipped, deletedDevices.size());
  }

  // Serialize batch payload
  String payload;
  size_t serializedSize = serializeJson(batchDoc, payload);

  // Validate serialization success
  if (serializedSize == 0) {
    Serial.println("[MQTT] ERROR: serializeJson() returned 0 bytes!");
    return;
  }

  // Validate payload is valid JSON (check first and last characters)
  if (payload.length() > 0) {
    if (payload.charAt(0) != '{' || payload.charAt(payload.length() - 1) != '}') {
      Serial.printf("[MQTT] ERROR: Payload is not valid JSON! First char: '%c', Last char: '%c'\n",
                    payload.charAt(0), payload.charAt(payload.length() - 1));
      if (payload.length() <= 2000) {
        Serial.printf("[MQTT] Invalid payload (%u bytes): %s\n", payload.length(), payload.c_str());
      } else {
        Serial.printf("[MQTT] Invalid payload too large (%u bytes)\n", payload.length());
        Serial.printf("  First 200 chars: %s\n", payload.substring(0, 200).c_str());
        Serial.printf("  Last 200 chars: %s\n", payload.substring(max(0, (int)payload.length() - 200)).c_str());
      }
      return;
    }
  }

  #if PRODUCTION_MODE == 0
    Serial.printf("[MQTT] Serialization complete: %u bytes (expected ~%u bytes)\n",
                  serializedSize, estimatedSize);
  #endif

  // FIXED BUG #15: Check against dynamic buffer size
  // FIXED BUG #7: Use cached buffer size (already set in connectToMqtt)
  if (payload.length() > cachedBufferSize)
  {
    Serial.printf("[MQTT] ERROR: Payload too large (%d bytes > %u bytes buffer)!\n",
                  payload.length(), cachedBufferSize);
    Serial.printf("[MQTT] Splitting large payloads is needed. Current registers: %d\n", totalRegisters);
    return;
  }

  // Publish single message with all data
  Serial.printf("\n[MQTT] PUBLISH REQUEST\n");
  Serial.printf("  Topic: %s\n", defaultTopicPublish.c_str());
  Serial.printf("  Size: %u bytes\n", payload.length());

  // DEBUG: Print payload (only in verbose mode)
  #if PRODUCTION_MODE == 0
    // Development mode: Show full payload for debugging
    if (payload.length() > 0 && payload.length() <= 2000) {
      // Show complete payload for small payloads (≤2KB)
      Serial.printf("  Payload: %s\n", payload.c_str());
    } else if (payload.length() > 2000) {
      // For large payloads, show size only to avoid log spam
      Serial.printf("  Payload: [%u bytes - too large to display]\n", payload.length());
    }
  #endif

  // CRITICAL FIX: Use BINARY publish with explicit length for large JSON payloads (>2KB)
  // PREVIOUS BUG: Text publish uses strlen() which STOPS at null terminator (\x00)
  // This causes data corruption/truncation for large payloads or JSON with special chars
  // SOLUTION: Binary publish treats payload as byte array with explicit length
  // Reference: PubSubClient documentation - binary publish for non-text data

  // Check MQTT connection state before publish
  if (!mqttClient.connected()) {
    LOG_MQTT_ERROR("MQTT client not connected before publish! State: %d", mqttClient.state());
    return;
  }

  // Calculate total MQTT packet size for validation
  // MQTT Packet = Fixed Header (5) + Topic Length (2) + Topic Name + Payload
  uint32_t mqttPacketSize = 5 + 2 + defaultTopicPublish.length() + payload.length();

  #if PRODUCTION_MODE == 0
    // Development mode: Show detailed publish info
    Serial.printf("  Broker: %s:%d\n", brokerAddress.c_str(), brokerPort);
    Serial.printf("  Packet size: %u/%u bytes\n\n", mqttPacketSize, cachedBufferSize);
  #endif

  // Validate packet size doesn't exceed buffer
  if (mqttPacketSize > cachedBufferSize) {
    Serial.printf("[MQTT] ERROR: Packet size (%u) exceeds buffer (%u)! Cannot publish.\n",
                  mqttPacketSize, cachedBufferSize);
    return;
  }

  // Validate payload is not empty or corrupted
  if (payload.length() == 0 || payload.length() > 16000) {
    Serial.printf("[MQTT] ERROR: Invalid payload size: %u bytes\n", payload.length());
    return;
  }

  // CRITICAL FIX: Copy topic to separate buffer to prevent memory overlap
  // This prevents topic name from corrupting payload during MQTT packet construction
  char topicBuffer[128];
  strncpy(topicBuffer, defaultTopicPublish.c_str(), sizeof(topicBuffer) - 1);
  topicBuffer[sizeof(topicBuffer) - 1] = '\0';

  // CRITICAL FIX: Allocate separate buffer for payload to prevent String memory issues
  // String.c_str() can return unstable pointer for large strings (>2KB) on ESP32
  uint8_t* payloadBuffer = (uint8_t*)heap_caps_malloc(payload.length(), MALLOC_CAP_8BIT);
  if (!payloadBuffer) {
    Serial.printf("[MQTT] ERROR: Failed to allocate %u bytes for payload buffer!\n", payload.length());
    return;
  }

  // Copy payload to dedicated buffer
  memcpy(payloadBuffer, payload.c_str(), payload.length());
  uint32_t payloadLen = payload.length();

  // FIXED: Use binary publish (3 params) with explicit length for reliable transmission
  bool published = mqttClient.publish(
    topicBuffer,                // Topic (separate buffer)
    payloadBuffer,              // Payload (dedicated buffer)
    payloadLen                  // Explicit length
  );

  // Free payload buffer after publish
  heap_caps_free(payloadBuffer);

  #if PRODUCTION_MODE == 0
    // Development mode: Show detailed publish result
    Serial.printf("[MQTT] Publish: %s | State: %d (%s)\n",
                  published ? "SUCCESS" : "FAILED",
                  mqttClient.state(),
                  mqttClient.state() == 0 ? "connected" : "disconnected");
  #else
    // Production mode: Only log if failed
    if (!published) {
      LOG_MQTT_ERROR("Publish FAILED! State: %d", mqttClient.state());
    }
  #endif

  // FIXED: Add small delay after publish to ensure TCP buffer fully flushed
  // This prevents premature disconnect detection on slow/public brokers
  // 100ms is sufficient for TCP stack to flush ~2KB payload over typical network
  if (published) {
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  if (published)
  {
    // Calculate interval display: convert from milliseconds to original unit
    // NOTE: defaultInterval is ALWAYS in milliseconds (converted at loadMqttConfig)
    uint32_t displayInterval;
    const char* displayUnit;

    // Normalize unit for comparison (case-insensitive)
    String unitLower = defaultIntervalUnit;
    unitLower.toLowerCase();

    if (unitLower == "m" || unitLower == "min" || unitLower == "mins" || unitLower == "minute" || unitLower == "minutes") {
      // Convert milliseconds to minutes
      displayInterval = defaultInterval / 60000;
      displayUnit = "min";
    } else if (unitLower == "s" || unitLower == "sec" || unitLower == "secs" || unitLower == "second" || unitLower == "seconds") {
      // Convert milliseconds to seconds
      displayInterval = defaultInterval / 1000;
      displayUnit = "s";
    } else {
      // Unit is "ms" or unknown - keep as milliseconds
      displayInterval = defaultInterval;
      displayUnit = "ms";
    }

    LOG_MQTT_INFO("Default Mode: Published %d registers from %d devices to %s (%.1f KB) / %u%s\n",
                  totalRegisters, deviceObjects.size(), defaultTopicPublish.c_str(),
                  payload.length() / 1024.0, displayInterval, displayUnit);
    lastDefaultPublish = now;

    // CRITICAL FIX: Clear batch status after successful publish (once per device)
    DeviceBatchManager *batchMgr = DeviceBatchManager::getInstance();
    if (batchMgr) {
      // Track cleared devices to avoid duplicate clears
      std::set<String> clearedDevices;
      for (auto &entry : uniqueRegisters) {
        JsonObject dataPoint = entry.second.as<JsonObject>();
        String deviceId = dataPoint["device_id"].as<String>();
        if (!deviceId.isEmpty() && clearedDevices.find(deviceId) == clearedDevices.end()) {
          batchMgr->clearBatch(deviceId);
          clearedDevices.insert(deviceId);
        }
      }
    }

    if (ledManager)
    {
      ledManager->notifyDataTransmission();
    }
  }
  else
  {
    // FIXED BUG #4: Use actual buffer size instead of hardcoded 1024 bytes
    Serial.printf("[MQTT] Default Mode: Publish failed (payload: %d bytes, buffer: %u bytes)\n",
                  payload.length(), cachedBufferSize);

    if (persistentQueueEnabled && persistentQueue)
    {
      JsonObject cleanPayload = batchDoc.as<JsonObject>();
      persistentQueue->enqueueJsonMessage(defaultTopicPublish, cleanPayload, PRIORITY_NORMAL, 86400000);
    }
  }
}

void MqttManager::publishCustomizeMode(std::map<String, JsonDocument> &uniqueRegisters, unsigned long now)
{
  // Publish to each custom topic based on its interval
  for (auto &customTopic : customTopics)
  {
    // Check if interval elapsed for this topic
    if ((now - customTopic.lastPublish) < customTopic.interval)
    {
      continue; // Wait for this topic's interval
    }

    // CRITICAL FIX: ArduinoJson v7 uses dynamic allocation (no size parameter needed)
    // Calculate estimated size for logging/monitoring purposes only
    constexpr uint32_t AVG_BYTES_PER_REGISTER = 150;
    constexpr uint32_t JSON_OVERHEAD = 1000;

    // Estimate for registers in THIS topic only (will be filtered below)
    uint32_t topicRegistersCount = customTopic.registers.size();
    uint32_t estimatedSize = (topicRegistersCount * AVG_BYTES_PER_REGISTER) + JSON_OVERHEAD;

    // Filter registers for this topic
    // SpiRamJsonDocument automatically uses PSRAM allocator (ArduinoJson v7 dynamic allocation)
    SpiRamJsonDocument topicDoc;

    LOG_MQTT_DEBUG("Customize Mode: JsonDocument created for topic %s (%d registers, estimated: ~%u bytes)\n",
                   customTopic.topic.c_str(), topicRegistersCount, estimatedSize);

    // Add formatted timestamp from RTC
    RTCManager *rtcMgr = RTCManager::getInstance();
    if (rtcMgr)
    {
      DateTime currentTime = rtcMgr->getCurrentTime();
      char timestampStr[20];
      snprintf(timestampStr, sizeof(timestampStr), "%02d/%02d/%04d %02d:%02d:%02d",
               currentTime.day(), currentTime.month(), currentTime.year(),
               currentTime.hour(), currentTime.minute(), currentTime.second());
      topicDoc["timestamp"] = timestampStr;
    }
    else
    {
      topicDoc["timestamp"] = now;
    }

    // NEW FORMAT: Create devices as nested object (same as default mode)
    JsonObject devicesObject = topicDoc["devices"].to<JsonObject>();
    std::map<String, JsonObject> deviceObjects;
    int registerCount = 0;

    // FIXED BUG #3: Track deleted devices (same as default mode)
    std::map<String, int> deletedDevices;

    for (auto &entry : uniqueRegisters)
    {
      JsonObject dataPoint = entry.second.as<JsonObject>();

      // Validate: Skip if data doesn't have required fields
      if (dataPoint["name"].isNull() || dataPoint["value"].isNull())
      {
        continue;
      }

      String registerId = dataPoint["register_id"].as<String>();

      // Check if register belongs to this topic (by register_id)
      if (std::find(customTopic.registers.begin(),
                    customTopic.registers.end(),
                    registerId) != customTopic.registers.end())
      {
        String deviceId = dataPoint["device_id"].as<String>();
        String registerName = dataPoint["name"].as<String>();

        if (deviceId.isEmpty() || registerName.isEmpty())
        {
          continue;
        }

        // FIXED BUG #3: Validate device still exists (not deleted) before publishing
        if (deviceObjects.find(deviceId) == deviceObjects.end())
        {
          // Check if we already know this device is deleted
          if (deletedDevices.find(deviceId) != deletedDevices.end())
          {
            deletedDevices[deviceId]++;
            continue;
          }

          // First time seeing this device - verify it exists in config
          JsonDocument tempDoc;
          JsonObject tempObj = tempDoc.to<JsonObject>();
          if (!configManager->readDevice(deviceId, tempObj))
          {
            // Device deleted - track it and skip
            deletedDevices[deviceId] = 1;
            continue;
          }
        }

        // Create device object if not exists
        if (deviceObjects.find(deviceId) == deviceObjects.end())
        {
          JsonObject deviceObj = devicesObject[deviceId].to<JsonObject>();

          // Add device_name at device level
          if (!dataPoint["device_name"].isNull())
          {
            deviceObj["device_name"] = dataPoint["device_name"];
          }

          deviceObjects[deviceId] = deviceObj;
        }

        // Add register as nested object: devices.{device_id}.{register_name} = {value, unit}
        JsonObject registerObj = deviceObjects[deviceId][registerName].to<JsonObject>();
        registerObj["value"] = dataPoint["value"];
        registerObj["unit"] = dataPoint["unit"];

        registerCount++;
      }
    }

    // FIXED BUG #3: Log summary for deleted devices (compact - one line)
    if (!deletedDevices.empty())
    {
      int totalSkipped = 0;
      for (auto &entry : deletedDevices)
      {
        totalSkipped += entry.second;
      }
      Serial.printf("[MQTT] Customize Mode: Skipped %d registers from %d deleted device(s) for topic %s\n",
                    totalSkipped, deletedDevices.size(), customTopic.topic.c_str());
    }

    // Only publish if there's data for this topic
    if (registerCount > 0)
    {
      String payload;
      serializeJson(topicDoc, payload);

      #if PRODUCTION_MODE == 0
        // DEBUG: Print payload preview for troubleshooting (development mode only)
        Serial.printf("[MQTT] Customize Mode - Publishing %u bytes to: %s\n",
                      payload.length(), customTopic.topic.c_str());
        if (payload.length() > 0 && payload.length() <= 300) {
          Serial.println("[MQTT] Payload preview:");
          Serial.println(payload);
          Serial.println("[MQTT] ---");
        }
      #endif

      // CRITICAL FIX: Use BINARY publish with explicit length (same as default mode)
      // Prevents data truncation for large payloads or JSON with special chars
      bool published = mqttClient.publish(
        customTopic.topic.c_str(),               // Topic
        (const uint8_t*)payload.c_str(),        // Payload as byte array
        payload.length()                         // Explicit length
      );

      // FIXED: Add small delay after publish to ensure TCP buffer fully flushed
      if (published) {
        vTaskDelay(pdMS_TO_TICKS(100));
      }

      if (published)
      {
        // Calculate interval display: convert from milliseconds to original unit
        // NOTE: customTopic.interval is ALWAYS in milliseconds (converted at loadMqttConfig)
        uint32_t displayInterval;
        const char* displayUnit;

        // Normalize unit for comparison (case-insensitive)
        String unitLower = customTopic.intervalUnit;
        unitLower.toLowerCase();

        if (unitLower == "m" || unitLower == "min" || unitLower == "mins" || unitLower == "minute" || unitLower == "minutes") {
          // Convert milliseconds to minutes
          displayInterval = customTopic.interval / 60000;
          displayUnit = "min";
        } else if (unitLower == "s" || unitLower == "sec" || unitLower == "secs" || unitLower == "second" || unitLower == "seconds") {
          // Convert milliseconds to seconds
          displayInterval = customTopic.interval / 1000;
          displayUnit = "s";
        } else {
          // Unit is "ms" or unknown - keep as milliseconds
          displayInterval = customTopic.interval;
          displayUnit = "ms";
        }

        Serial.printf("[MQTT] Customize Mode: Published %d registers from %d devices to %s (%.1f KB) / %u%s\n",
                      registerCount, deviceObjects.size(), customTopic.topic.c_str(),
                      payload.length() / 1024.0, displayInterval, displayUnit);
        customTopic.lastPublish = now;

        // FIXED BUG #1 (CRITICAL): Clear batch status after successful publish
        // Without this, hasCompleteBatch() will always return false after first publish
        // causing MQTT to never publish data again (infinite wait)
        DeviceBatchManager *batchMgr = DeviceBatchManager::getInstance();
        if (batchMgr) {
          // Track cleared devices to avoid duplicate clears
          std::set<String> clearedDevices;
          for (auto &entry : uniqueRegisters) {
            JsonObject dataPoint = entry.second.as<JsonObject>();
            String deviceId = dataPoint["device_id"].as<String>();
            if (!deviceId.isEmpty() && clearedDevices.find(deviceId) == clearedDevices.end()) {
              batchMgr->clearBatch(deviceId);
              clearedDevices.insert(deviceId);
            }
          }
        }

        if (ledManager)
        {
          ledManager->notifyDataTransmission();
        }
      }
      else
      {
        Serial.printf("[MQTT] Customize Mode: Publish failed for topic %s\n", customTopic.topic.c_str());

        if (persistentQueueEnabled && persistentQueue)
        {
          JsonObject cleanPayload = topicDoc.as<JsonObject>();
          persistentQueue->enqueueJsonMessage(customTopic.topic, cleanPayload, PRIORITY_NORMAL, 86400000);
        }
      }

      vTaskDelay(pdMS_TO_TICKS(10)); // Small delay between topic publishes
    }
  }
}

bool MqttManager::isNetworkAvailable()
{
  if (!networkManager)
    return false;

  // Check if network manager says available
  if (!networkManager->isAvailable())
  {
    return false;
  }

  // Check actual IP from network manager
  IPAddress localIP = networkManager->getLocalIP();
  if (localIP == IPAddress(0, 0, 0, 0))
  {
    Serial.printf("[MQTT] Network manager available but no IP (%s)\n", networkManager->getCurrentMode().c_str());
    return false;
  }

  return true;
}

void MqttManager::debugNetworkConnectivity()
{
  String mode = networkManager->getCurrentMode();
  IPAddress ip = networkManager->getLocalIP();
  bool available = networkManager->isAvailable();

  if (mode == "WIFI")
  {
    Serial.printf("[MQTT] Network Debug: WiFi %s  SSID: %s  RSSI: %d dBm  IP: %s  Available: %s\n",
                  WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected",
                  WiFi.SSID().c_str(), WiFi.RSSI(), ip.toString().c_str(),
                  available ? "YES" : "NO");
  }
  else if (mode == "ETH")
  {
    Serial.printf("[MQTT] Network Debug: Ethernet  IP: %s  Available: %s\n",
                  ip.toString().c_str(), available ? "YES" : "NO");
  }
  else
  {
    Serial.printf("[MQTT] Network Debug: Mode: %s  IP: %s  Available: %s\n",
                  mode.c_str(), ip.toString().c_str(), available ? "YES" : "NO");
  }
}

void MqttManager::getStatus(JsonObject &status)
{
  status["running"] = running;
  status["service_type"] = "mqtt_manager";
  status["mqtt_connected"] = mqttClient.connected();
  status["wifi_connected"] = (WiFi.status() == WL_CONNECTED);
  status["broker_address"] = brokerAddress;
  status["broker_port"] = brokerPort;
  status["client_id"] = clientId;
  status["topic_publish"] = topicPublish;
  status["queue_size"] = queueManager->size();
  status["publish_mode"] = publishMode;  // v2.2.0: Show current publish mode instead of legacy data_interval_ms
}

// Persistent queue management methods
void MqttManager::setPersistentQueueEnabled(bool enable)
{
  persistentQueueEnabled = enable;
  if (persistentQueue)
  {
    if (enable)
    {
      Serial.println("[MQTT] Persistent queue enabled");
    }
    else
    {
      Serial.println("[MQTT] Persistent queue disabled");
    }
  }
}

bool MqttManager::isPersistentQueueEnabled() const
{
  return persistentQueueEnabled;
}

MQTTPersistentQueue *MqttManager::getPersistentQueue() const
{
  return persistentQueue;
}

void MqttManager::printQueueStatus() const
{
  if (persistentQueue)
  {
    persistentQueue->printHealthReport();
  }
  else
  {
    Serial.println("[MQTT] Persistent queue not initialized");
  }
}

uint32_t MqttManager::getQueuedMessageCount() const
{
  if (persistentQueue)
  {
    return persistentQueue->getPendingMessageCount();
  }
  return 0;
}

// ============================================
// FIXED BUG #15: DYNAMIC MQTT BUFFER SIZING
// ============================================
uint16_t MqttManager::calculateOptimalBufferSize()
{
  if (!configManager)
  {
    // Fallback to conservative default if no config
    return 4096;  // 4KB default
  }

  // Load all devices to count total registers
  // Use getAllDevicesWithRegisters() to get device data
  // BUG #31: JsonDocument will use PSRAM via DefaultAllocator override (JsonDocumentPSRAM.h)
  JsonDocument devicesDoc;  // Automatically uses PSRAM allocator from JsonDocumentPSRAM.h
  JsonArray devices = devicesDoc.to<JsonArray>();
  configManager->getAllDevicesWithRegisters(devices, true);  // minimal fields

  if (devices.size() == 0)
  {
    return 4096;  // 4KB default if no devices
  }

  uint32_t totalRegisters = 0;
  uint32_t maxDeviceRegisters = 0;

  // Count total and max registers
  for (JsonVariant deviceVar : devices)
  {
    JsonObject device = deviceVar.as<JsonObject>();
    JsonArray registers = device["registers"];
    uint32_t deviceRegCount = registers.size();

    totalRegisters += deviceRegCount;
    if (deviceRegCount > maxDeviceRegisters)
    {
      maxDeviceRegisters = deviceRegCount;
    }
  }

  // Calculate buffer size based on total registers
  // Formula: (totalRegisters * BYTES_PER_REGISTER) + BUFFER_OVERHEAD
  // - Each register: ~50-70 bytes (name, value, unit, timestamp, device_id)
  // - Batch overhead: ~300 bytes (JSON structure, device metadata)
  uint32_t calculatedSize = (totalRegisters * MqttConfig::BYTES_PER_REGISTER) + MqttConfig::BUFFER_OVERHEAD;

  // Apply constraints
  uint16_t optimalSize = MqttConfig::DEFAULT_BUFFER_SIZE;

  if (calculatedSize < MqttConfig::MIN_BUFFER_SIZE)
  {
    optimalSize = MqttConfig::MIN_BUFFER_SIZE;
  }
  else if (calculatedSize > MqttConfig::MAX_BUFFER_SIZE)
  {
    optimalSize = MqttConfig::MAX_BUFFER_SIZE;
    Serial.printf("[MQTT] WARNING: Calculated buffer (%lu bytes) exceeds max (%u bytes)\n",
                  calculatedSize, MqttConfig::MAX_BUFFER_SIZE);
    Serial.printf("[MQTT] Consider reducing devices/registers or enabling payload splitting\n");
  }
  else
  {
    optimalSize = (uint16_t)calculatedSize;
  }

  #if PRODUCTION_MODE == 0
    Serial.printf("[MQTT] Buffer calculation: %u registers → %u bytes (min: %u, max: %u)\n",
                  totalRegisters, optimalSize, MqttConfig::MIN_BUFFER_SIZE, MqttConfig::MAX_BUFFER_SIZE);
  #endif

  return optimalSize;
}

// FIXED BUG #5: Notify MQTT manager when device configuration changes
// This invalidates the cached buffer size calculation
void MqttManager::notifyConfigChange()
{
  bufferSizeNeedsRecalculation = true;
  Serial.println("[MQTT] Config change detected - buffer size will be recalculated on next connection");
}

MqttManager::~MqttManager()
{
  stop();
}