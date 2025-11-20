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

// FIXED BUG #21: Define named constants for magic numbers
namespace MqttConfig {
  constexpr uint32_t MQTT_TASK_STACK_SIZE = 8192;      // Stack size for MQTT task
  constexpr uint16_t MIN_BUFFER_SIZE = 2048;           // 2KB minimum buffer
  constexpr uint16_t MAX_BUFFER_SIZE = 16384;          // 16KB maximum (PubSubClient limit)
  constexpr uint16_t DEFAULT_BUFFER_SIZE = 8192;       // 8KB conservative default
  constexpr uint16_t BYTES_PER_REGISTER = 70;          // Estimated bytes per register in JSON
  constexpr uint16_t BUFFER_OVERHEAD = 300;            // JSON structure overhead
}

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

  // FIXED BUG #15: Dynamic MQTT buffer sizing
  uint16_t calculateOptimalBufferSize();

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