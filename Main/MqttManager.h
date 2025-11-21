#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include "JsonDocumentPSRAM.h"  // BUG #31: MUST BE BEFORE ArduinoJson.h
#include <WiFi.h>

// CRITICAL FIX: Override PubSubClient's default MQTT_MAX_PACKET_SIZE (256 bytes)
// Must be defined BEFORE including PubSubClient.h
// This allows sending large payloads (2-16 KB) without truncation
#ifndef MQTT_MAX_PACKET_SIZE
#define MQTT_MAX_PACKET_SIZE 16384  // 16KB - matches MAX_BUFFER_SIZE
#endif

#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <map> // For std::map in deduplication logic
#include "ConfigManager.h"
#include "ServerConfig.h"
#include "QueueManager.h"
#include "NetworkManager.h"
#include "MQTTPersistentQueue.h" // Persistent queue for failed publishes

// FIXED BUG #21: Define named constants for magic numbers
// FIXED BUG #27: Increased BYTES_PER_REGISTER for accurate buffer sizing
// FIXED BUG #28 + #29: Increased MQTT_TASK_STACK_SIZE for ArduinoJson v7 dynamic allocations
namespace MqttConfig {
  constexpr uint32_t MQTT_TASK_STACK_SIZE = 24576;     // 24KB stack for ArduinoJson v7 dynamic allocations (50+ registers)
  constexpr uint16_t MIN_BUFFER_SIZE = 2048;           // 2KB minimum buffer
  constexpr uint16_t MAX_BUFFER_SIZE = 16384;          // 16KB maximum (PubSubClient limit)
  constexpr uint16_t DEFAULT_BUFFER_SIZE = 8192;       // 8KB conservative default
  constexpr uint16_t BYTES_PER_REGISTER = 120;         // Realistic bytes per register (includes metadata, descriptions, JSON overhead)
  constexpr uint16_t BUFFER_OVERHEAD = 500;            // JSON structure overhead (increased for safety)
  constexpr uint16_t MAX_REGISTERS_PER_PUBLISH = 100;  // Maximum registers to dequeue per MQTT publish cycle (increased from 50 to support larger configurations)
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
  void disconnect();  // Graceful disconnect from MQTT broker
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