#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <ArduinoJson.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "JsonDocumentPSRAM.h"  // BUG #31: MUST BE BEFORE ArduinoJson.h

class WiFiManager {
 private:
  static WiFiManager* instance;
  bool initialized;
  bool configStored;  // v2.5.33: Track if credentials are stored for reconnect
  int referenceCount;
  SemaphoreHandle_t refCountMutex;  // FIXED: Thread safety for referenceCount
  String ssid;
  String password;

  // v2.5.33: Reconnect tracking
  unsigned long lastReconnectAttempt;
  uint32_t reconnectCount;

  WiFiManager();

 public:
  static WiFiManager* getInstance();

  bool init(const String& ssid, const String& password);
  void addReference();
  void removeReference();
  void cleanup();

  bool isAvailable();
  IPAddress getLocalIP();
  String getSSID();
  void getStatus(JsonObject& status);

  // v2.5.33: Reconnect support
  bool tryReconnect();  // Attempt to reconnect using stored credentials
  bool hasStoredConfig()
      const;  // Check if credentials are available for reconnect
  bool isInitialized() const { return initialized; }
  uint32_t getReconnectCount() const { return reconnectCount; }

  ~WiFiManager();
};

#endif