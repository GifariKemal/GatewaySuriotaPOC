#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <map> // For std::map in deduplication logic
#include "ConfigManager.h"
#include "ServerConfig.h"
#include "QueueManager.h"
#include "NetworkManager.h"
#include "MQTTPersistentQueue.h" // Persistent queue for failed publishes

class MqttManager
{
private:
  static MqttManager *instance;
  ConfigManager *configManager;
  QueueManager *queueManager;
  ServerConfig *serverConfig;
  NetworkMgr *networkManager;
  PubSubClient mqttClient;

  bool running;
  TaskHandle_t taskHandle;

  String brokerAddress;
  int brokerPort;
  String clientId;
  String username;
  String password;
  String topicPublish;
  unsigned long lastReconnectAttempt;

  // MQTT Publish Mode ("default" or "customize")
  String publishMode;

  // Default mode fields
  bool defaultModeEnabled;
  String defaultTopicPublish;
  String defaultTopicSubscribe;
  uint32_t defaultInterval;
  String defaultIntervalUnit;  // "ms", "s", or "m"
  unsigned long lastDefaultPublish;

  // Customize mode fields
  bool customizeModeEnabled;
  struct CustomTopic {
    String topic;
    std::vector<String> registers;  // Changed from int to String (register_id)
    uint32_t interval;
    String intervalUnit;  // "ms", "s", or "m"
    unsigned long lastPublish;
  };
  std::vector<CustomTopic> customTopics;

  // Persistent queue for failed publishes
  MQTTPersistentQueue *persistentQueue; // Queue for reliable message delivery
  bool persistentQueueEnabled = true;   // Enable/disable persistent queue

  MqttManager(ConfigManager *config, ServerConfig *serverCfg, NetworkMgr *netMgr);

  static void mqttTask(void *parameter);
  void mqttLoop();
  bool connectToMqtt();
  void loadMqttConfig();
  void publishQueueData();
  void publishDefaultMode(std::map<String, JsonDocument> &uniqueRegisters, unsigned long now);
  void publishCustomizeMode(std::map<String, JsonDocument> &uniqueRegisters, unsigned long now);
  void debugNetworkConnectivity();
  bool isNetworkAvailable();

public:
  static MqttManager *getInstance(ConfigManager *config = nullptr, ServerConfig *serverCfg = nullptr, NetworkMgr *netMgr = nullptr);

  bool init();
  void start();
  void stop();
  void getStatus(JsonObject &status);

  // Persistent queue methods
  void setPersistentQueueEnabled(bool enable);
  bool isPersistentQueueEnabled() const;
  MQTTPersistentQueue *getPersistentQueue() const;
  void printQueueStatus() const;
  uint32_t getQueuedMessageCount() const;

  ~MqttManager();
};

#endif