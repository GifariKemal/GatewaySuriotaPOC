#include "MqttManager.h"
#include "LEDManager.h"
#include "RTCManager.h"
#include "DeviceBatchManager.h"  // CRITICAL FIX: Wait for batch completion
#include "DebugConfig.h"
#include "MemoryRecovery.h"
#include <set>  // For std::set to track cleared devices

// Helper function: Convert interval to milliseconds based on unit
static uint32_t convertToMilliseconds(uint32_t interval, const String &unit)
{
  if (unit == "ms" || unit.isEmpty())
  {
    return interval; // Already in milliseconds
  }
  else if (unit == "s")
  {
    return interval * 1000; // Seconds to milliseconds
  }
  else if (unit == "m")
  {
    return interval * 60000; // Minutes to milliseconds
  }
  return interval; // Default: treat as milliseconds
}

MqttManager *MqttManager::instance = nullptr;

MqttManager::MqttManager(ConfigManager *config, ServerConfig *serverCfg, NetworkMgr *netMgr)
    : configManager(config), queueManager(nullptr), serverConfig(serverCfg), networkManager(netMgr), mqttClient(PubSubClient()),
      running(false), taskHandle(nullptr), brokerPort(1883), lastReconnectAttempt(0)
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
      8192,
      this,
      1,
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
  if (taskHandle)
  {
    vTaskDelay(pdMS_TO_TICKS(100));
    vTaskDelete(taskHandle);
    taskHandle = nullptr;
  }
  if (mqttClient.connected())
  {
    mqttClient.disconnect();
  }
  Serial.println("MQTT Manager stopped");
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
  // Previous: Hardcoded 8192 bytes
  // New: Calculate based on total registers across all devices
  uint16_t optimalBufferSize = calculateOptimalBufferSize();
  mqttClient.setBufferSize(optimalBufferSize, optimalBufferSize);
  Serial.printf("[MQTT] Buffer size set to %u bytes (dynamically calculated)\n", optimalBufferSize);
  mqttClient.setKeepAlive(60);
  mqttClient.setSocketTimeout(5);  // Socket timeout in seconds

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

  // Log connection duration for diagnostics
  unsigned long connectDuration = millis() - connectStartTime;
  if (connectDuration > 2000)
  {
    Serial.printf("[MQTT] Connection attempt took %lu ms (slow)\n", connectDuration);
  }

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
    brokerAddress = mqttConfig["broker_address"] | "broker.hivemq.com";
    brokerAddress.trim(); // FIXED Bug #9: Trim whitespace to avoid isEmpty() false assumption
    brokerPort = mqttConfig["broker_port"] | 1883;
    clientId = mqttConfig["client_id"] | "esp32_gateway";
    username = mqttConfig["username"] | "";
    password = mqttConfig["password"] | "";

    // Load publish mode
    publishMode = mqttConfig["publish_mode"] | "default";

    Serial.printf("[MQTT] Config loaded - Broker: %s:%d, Client: %s\n",
                  brokerAddress.c_str(), brokerPort, clientId.c_str());
    Serial.printf("[MQTT] Auth: %s, Mode: %s\n",
                  (username.length() > 0) ? "YES" : "NO", publishMode.c_str());

    // Load default mode configuration
    if (mqttConfig["default_mode"])
    {
      JsonObject defaultMode = mqttConfig["default_mode"];
      defaultModeEnabled = defaultMode["enabled"] | true;
      defaultTopicPublish = defaultMode["topic_publish"] | "v1/devices/me/telemetry";
      defaultTopicSubscribe = defaultMode["topic_subscribe"] | "device/control";

      // Load interval and unit, then convert to milliseconds
      uint32_t intervalValue = defaultMode["interval"] | 5;
      defaultIntervalUnit = defaultMode["interval_unit"] | "s";
      defaultInterval = convertToMilliseconds(intervalValue, defaultIntervalUnit);
      lastDefaultPublish = 0;

      Serial.printf("[MQTT] Default Mode: %s, Topic: %s, Interval: %u%s (%ums)\n",
                    defaultModeEnabled ? "ENABLED" : "DISABLED",
                    defaultTopicPublish.c_str(), intervalValue,
                    defaultIntervalUnit.c_str(), defaultInterval);
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
          ct.topic = topicObj["topic"] | "";

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
            Serial.printf("[MQTT] Custom Topic: %s, Registers: %d, Interval: %u%s (%ums)\n",
                          ct.topic.c_str(), ct.registers.size(), intervalValue,
                          ct.intervalUnit.c_str(), ct.interval);
          }
        }
      }

      Serial.printf("[MQTT] Customize Mode: %s, Topics: %d\n",
                    customizeModeEnabled ? "ENABLED" : "DISABLED",
                    customTopics.size());
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
    if (persistedSent > 0)
    {
      Serial.printf("[MQTT] Resent %ld persistent messages\n", persistedSent);
    }
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

  if (batchMgr && !batchMgr->hasCompleteBatch())
  {
    // No complete batches yet - wait for RTU/TCP to finish reading all registers
    static LogThrottle batchWaitingThrottle(60000); // Increased to 60s to reduce log spam
    if (batchWaitingThrottle.shouldLog()) {
      LOG_MQTT_DEBUG("Waiting for device batch to complete...\n");
    }
    return;
  }

  // Only dequeue if we're actually going to publish
  std::map<String, JsonDocument> uniqueRegisters;

  // DEBUG: Track dequeue count
  int dequeueCount = 0;

  while (uniqueRegisters.size() < 50)
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
  // Batch all data into single payload grouped by device_id
  JsonDocument batchDoc;

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

  // OPTIMIZED: Create devices array instead of object for smaller payload size
  JsonArray devicesArray = batchDoc["devices"].to<JsonArray>();

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

    if (deviceId.isEmpty())
    {
      continue; // Silent skip - empty device_id
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

    // Create device object in array if not exists
    if (deviceObjects.find(deviceId) == deviceObjects.end())
    {
      JsonObject deviceObj = devicesArray.add<JsonObject>();

      // OPTIMIZED: Minimal device info (device_id, device_name, data only)
      deviceObj["device_id"] = deviceId;

      if (!dataPoint["device_name"].isNull())
      {
        deviceObj["device_name"] = dataPoint["device_name"];
      }

      // Create data array for this device
      deviceObj["data"].to<JsonArray>();

      // Store reference to this device object for grouping
      deviceObjects[deviceId] = deviceObj;
    }

    // Get data array for this device
    JsonArray dataArray = deviceObjects[deviceId]["data"].as<JsonArray>();

    // OPTIMIZED: Add minimal register data (name, value, unit only - removed description)
    JsonObject item = dataArray.add<JsonObject>();
    item["name"] = dataPoint["name"];
    item["value"] = dataPoint["value"];
    item["unit"] = dataPoint["unit"];

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
  serializeJson(batchDoc, payload);

  // FIXED BUG #15: Check against dynamic buffer size
  uint16_t currentBufferSize = calculateOptimalBufferSize();
  if (payload.length() > currentBufferSize)
  {
    Serial.printf("[MQTT] ERROR: Payload too large (%d bytes > %u bytes buffer)!\n",
                  payload.length(), currentBufferSize);
    Serial.printf("[MQTT] Splitting large payloads is needed. Current registers: %d\n", totalRegisters);
  }

  // Publish single message with all data
  if (mqttClient.publish(defaultTopicPublish.c_str(), payload.c_str()))
  {
    LOG_MQTT_INFO("Default Mode: Published %d registers from %d devices to %s (%.1f KB)\n",
                  totalRegisters, devicesArray.size(), defaultTopicPublish.c_str(), payload.length() / 1024.0);
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
    Serial.printf("[MQTT] Default Mode: Publish failed (payload: %d bytes, buffer: 1024 bytes)\n", payload.length());

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

    // Filter registers for this topic
    JsonDocument topicDoc;
    topicDoc["timestamp"] = now;

    JsonArray dataArray = topicDoc["data"].to<JsonArray>();

    for (auto &entry : uniqueRegisters)
    {
      JsonObject dataPoint = entry.second.as<JsonObject>();

      // Validate: Skip if data doesn't have required fields (old format)
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
        JsonObject item = dataArray.add<JsonObject>();
        item["device_id"] = dataPoint["device_id"];  // Include device_id per data point
        item["name"] = dataPoint["name"];
        item["value"] = dataPoint["value"];
        item["description"] = dataPoint["description"];
        item["unit"] = dataPoint["unit"];
      }
    }

    // Only publish if there's data for this topic
    if (dataArray.size() > 0)
    {
      String payload;
      serializeJson(topicDoc, payload);

      if (mqttClient.publish(customTopic.topic.c_str(), payload.c_str()))
      {
        Serial.printf("[MQTT] Customize Mode: Published %d registers to %s (Interval: %ums)\n",
                      dataArray.size(), customTopic.topic.c_str(), customTopic.interval);
        customTopic.lastPublish = now;

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
  SpiRamJsonDocument devicesDoc(16384);  // 16KB for devices list
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
  // Formula: (totalRegisters * 70 bytes per register) + 300 bytes overhead
  // - Each register: ~50-70 bytes (name, value, unit, timestamp, device_id)
  // - Batch overhead: ~300 bytes (JSON structure, device metadata)
  uint32_t calculatedSize = (totalRegisters * 70) + 300;

  // Apply constraints
  const uint16_t MIN_BUFFER_SIZE = 2048;   // 2KB minimum
  const uint16_t MAX_BUFFER_SIZE = 16384;  // 16KB maximum (PubSubClient limit)
  const uint16_t DEFAULT_BUFFER_SIZE = 8192;  // 8KB conservative default

  uint16_t optimalSize = DEFAULT_BUFFER_SIZE;

  if (calculatedSize < MIN_BUFFER_SIZE)
  {
    optimalSize = MIN_BUFFER_SIZE;
  }
  else if (calculatedSize > MAX_BUFFER_SIZE)
  {
    optimalSize = MAX_BUFFER_SIZE;
    Serial.printf("[MQTT] WARNING: Calculated buffer (%lu bytes) exceeds max (%u bytes)\n",
                  calculatedSize, MAX_BUFFER_SIZE);
    Serial.printf("[MQTT] Consider reducing devices/registers or enabling payload splitting\n");
  }
  else
  {
    optimalSize = (uint16_t)calculatedSize;
  }

  Serial.printf("[MQTT] Buffer calculation: %u registers → %u bytes (min: %u, max: %u)\n",
                totalRegisters, optimalSize, MIN_BUFFER_SIZE, MAX_BUFFER_SIZE);

  return optimalSize;
}

MqttManager::~MqttManager()
{
  stop();
}