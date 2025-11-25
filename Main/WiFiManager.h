#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "JsonDocumentPSRAM.h" // BUG #31: MUST BE BEFORE ArduinoJson.h
#include <WiFi.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class WiFiManager
{
private:
  static WiFiManager *instance;
  bool initialized;
  int referenceCount;
  SemaphoreHandle_t refCountMutex; // FIXED: Thread safety for referenceCount
  String ssid;
  String password;

  WiFiManager();

public:
  static WiFiManager *getInstance();

  bool init(const String &ssid, const String &password);
  void addReference();
  void removeReference();
  void cleanup();

  bool isAvailable();
  IPAddress getLocalIP();
  String getSSID();
  void getStatus(JsonObject &status);

  ~WiFiManager();
};

#endif