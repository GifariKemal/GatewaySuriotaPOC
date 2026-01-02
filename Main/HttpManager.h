#ifndef HTTP_MANAGER_H
#define HTTP_MANAGER_H

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "ConfigManager.h"
#include "JsonDocumentPSRAM.h"  // BUG #31: MUST BE BEFORE ArduinoJson.h
#include "NetworkManager.h"
#include "QueueManager.h"
#include "ServerConfig.h"

class HttpManager {
 private:
  static HttpManager* instance;
  ConfigManager* configManager;
  QueueManager* queueManager;
  ServerConfig* serverConfig;
  NetworkMgr* networkManager;
  bool running;
  TaskHandle_t taskHandle;

  String endpointUrl;
  String method;
  JsonObject headers;
  String bodyFormat;
  int timeout;
  int retryCount;
  unsigned long lastSendAttempt;

  // Level 3: Server data transmission interval control
  unsigned long lastDataTransmission;  // Last time data was transmitted
  uint32_t dataIntervalMs;  // Data transmission interval in milliseconds

  HttpManager(ConfigManager* config, ServerConfig* serverCfg,
              NetworkMgr* netMgr);

  static void httpTask(void* parameter);
  void httpLoop();
  bool sendHttpRequest(const JsonObject& data);
  void loadHttpConfig();
  void publishQueueData();
  void debugNetworkConnectivity();
  bool isNetworkAvailable();

 public:
  static HttpManager* getInstance(ConfigManager* config = nullptr,
                                  ServerConfig* serverCfg = nullptr,
                                  NetworkMgr* netMgr = nullptr);

  bool init();
  void start();
  void stop();
  void getStatus(JsonObject& status);

  // Level 3: Allow runtime update of data transmission interval
  void updateDataTransmissionInterval();

  ~HttpManager();
};

#endif