#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <WiFi.h>
#include <freertos/event_groups.h>  // v2.5.1 FIX: For safe task termination

#include "JsonDocumentPSRAM.h"  // BUG #31: MUST BE BEFORE ArduinoJson.h

// CRITICAL FIX: Override PubSubClient's default MQTT_MAX_PACKET_SIZE (256
// bytes) Must be defined BEFORE including PubSubClient.h This allows sending
// large payloads (2-16 KB) without truncation
#ifndef MQTT_MAX_PACKET_SIZE
#define MQTT_MAX_PACKET_SIZE 16384  // 16KB - matches MAX_BUFFER_SIZE
#endif

#include <ArduinoJson.h>
#include <PubSubClient.h>

#include <map>  // For std::map in deduplication logic

#include "ConfigManager.h"
#include "MQTTPersistentQueue.h"  // Persistent queue for failed publishes
#include "NetworkManager.h"
#include "QueueManager.h"
#include "ServerConfig.h"

// FIXED BUG #21: Define named constants for magic numbers
// FIXED BUG #27: Increased BYTES_PER_REGISTER for accurate buffer sizing
// FIXED BUG #28 + #29: Increased MQTT_TASK_STACK_SIZE for ArduinoJson v7
// dynamic allocations
namespace MqttConfig {
constexpr uint32_t MQTT_TASK_STACK_SIZE =
    24576;  // 24KB stack for ArduinoJson v7 dynamic allocations (50+ registers)
constexpr uint16_t MIN_BUFFER_SIZE =
    8192;  // 8KB minimum buffer (supports 60+ registers safely)
constexpr uint16_t MAX_BUFFER_SIZE =
    16384;  // 16KB maximum (PubSubClient limit)
constexpr uint16_t DEFAULT_BUFFER_SIZE = 8192;  // 8KB conservative default
constexpr uint16_t BYTES_PER_REGISTER =
    120;  // Realistic bytes per register (includes metadata, descriptions, JSON
          // overhead)
constexpr uint16_t BUFFER_OVERHEAD =
    500;  // JSON structure overhead (increased for safety)
constexpr uint16_t MAX_REGISTERS_PER_PUBLISH =
    100;  // Maximum registers to dequeue per MQTT publish cycle (increased from
          // 50 to support larger configurations)
}  // namespace MqttConfig

class MqttManager {
 private:
  static MqttManager* instance;
  ConfigManager* configManager;
  QueueManager* queueManager;
  ServerConfig* serverConfig;
  NetworkMgr* networkManager;
  PubSubClient mqttClient;

  bool running;
  TaskHandle_t taskHandle;

  // v2.5.1 FIX: Event group for safe task termination (race condition fix)
  EventGroupHandle_t taskExitEvent;
  static constexpr uint32_t TASK_EXITED_BIT = BIT0;

  String brokerAddress;
  int brokerPort;
  String clientId;
  String username;
  String password;
  String topicPublish;
  unsigned long lastReconnectAttempt;

  // FIXED BUG #5 & #7: Cache buffer size calculation
  uint16_t cachedBufferSize;
  bool bufferSizeNeedsRecalculation;

  // v2.3.8 PHASE 1: Thread safety - Mutex for buffer cache
  SemaphoreHandle_t bufferCacheMutex;

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
  MQTTPersistentQueue* persistentQueue;  // Queue for reliable message delivery
  bool persistentQueueEnabled = true;    // Enable/disable persistent queue

  // v2.3.8 PHASE 1: Thread safety - Instance members replacing static variables
  // These were previously static variables in publishQueueData() - moved to
  // instance for thread safety
  struct PublishState {
    unsigned long targetTime;          // Replaces static publishTargetTime
    bool timeLocked;                   // Replaces static targetTimeLocked
    unsigned long lastLoggedInterval;  // Replaces static lastLoggedInterval
    unsigned long batchWaitStart;      // Replaces static batchWaitStartTime
    uint32_t batchTimeout;             // Replaces static cachedBatchTimeout
    bool flushedOnce;                  // Replaces static flushedOnce
  };
  PublishState publishState;
  SemaphoreHandle_t publishStateMutex;  // Mutex for publishState

  // Connection state - replaces static in mqttLoop()
  unsigned long lastDebugTime;

  MqttManager(ConfigManager* config, ServerConfig* serverCfg,
              NetworkMgr* netMgr);

  static void mqttTask(void* parameter);
  void mqttLoop();
  bool connectToMqtt();
  void loadMqttConfig();
  void publishQueueData();
  void publishDefaultMode(std::map<String, JsonDocument>& uniqueRegisters,
                          unsigned long now);
  void publishCustomizeMode(std::map<String, JsonDocument>& uniqueRegisters,
                            unsigned long now);
  void debugNetworkConnectivity();
  bool isNetworkAvailable();

  // FIXED BUG #15: Dynamic MQTT buffer sizing
  uint16_t calculateOptimalBufferSize();

  // v2.3.7: Adaptive batch timeout based on device configuration
  uint32_t calculateAdaptiveBatchTimeout();

  // v2.3.8 PHASE 1: Helper methods to eliminate code duplication (DRY
  // principle)
  void buildTimestamp(JsonDocument& doc, unsigned long now);
  int validateAndGroupRegisters(
      std::map<String, JsonDocument>& uniqueRegisters,
      JsonObject& devicesObject, std::map<String, JsonObject>& deviceObjects,
      const std::vector<String>* filterRegisters = nullptr);
  bool serializeAndValidatePayload(JsonDocument& doc, String& payload,
                                   uint32_t estimatedSize);
  bool publishPayload(const String& topic, const String& payload,
                      const char* modeLabel);
  void calculateDisplayInterval(uint32_t intervalMs, const String& unit,
                                uint32_t& displayInterval,
                                const char*& displayUnit);

  // v2.3.8 PHASE 2: Helper methods to reduce method complexity
  void loadBrokerConfig(JsonObject& mqttConfig);
  void loadDefaultModeConfig(JsonObject& mqttConfig);
  void loadCustomizeModeConfig(JsonObject& mqttConfig);
  void analyzeDeviceConfigurations(JsonArray& devices, uint32_t& maxRefreshRate,
                                   uint32_t& totalRegisters, bool& hasSlowRTU);
  uint32_t determineTimeoutStrategy(uint32_t totalRegisters,
                                    uint32_t maxRefreshRate, bool hasSlowRTU);

 public:
  static MqttManager* getInstance(ConfigManager* config = nullptr,
                                  ServerConfig* serverCfg = nullptr,
                                  NetworkMgr* netMgr = nullptr);

  bool init();
  void start();
  void stop();
  void disconnect();  // Graceful disconnect from MQTT broker
  void getStatus(JsonObject& status);

  // FIXED BUG #5: Config change notification to invalidate buffer size cache
  void notifyConfigChange();

  // Persistent queue methods
  void setPersistentQueueEnabled(bool enable);
  bool isPersistentQueueEnabled() const;
  MQTTPersistentQueue* getPersistentQueue() const;
  void printQueueStatus() const;
  uint32_t getQueuedMessageCount() const;

  ~MqttManager();
};

#endif