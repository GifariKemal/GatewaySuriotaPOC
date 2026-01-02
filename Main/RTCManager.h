#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <ArduinoJson.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <NTPClient.h>
#include <RTClib.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "JsonDocumentPSRAM.h"  // BUG #31: MUST BE BEFORE ArduinoJson.h

class RTCManager {
 private:
  static RTCManager* instance;
  RTC_DS3231 rtc;
  bool initialized;
  bool syncRunning;
  TaskHandle_t syncTaskHandle;
  unsigned long lastNtpSync;

  // NTP settings
  const char* ntpServer = "pool.ntp.org";
  const long gmtOffset_sec = 7 * 3600;  // GMT+7 (WIB - Waktu Indonesia Barat)
  const unsigned long ntpUpdateInterval = 21600000;  // 6 hours
  const unsigned long ntpTimeout = 5000;  // 5 seconds timeout for NTP sync

  // Separate UDP clients for WiFi and Ethernet
  WiFiUDP wifiUdp;
  EthernetUDP ethernetUdp;
  NTPClient* ntpClient;

  RTCManager();

  static void timeSyncTask(void* parameter);
  void timeSyncLoop();
  bool syncWithNTP();
  bool checkInternetConnectivity();
  void updateSystemTime(DateTime rtcTime);

 public:
  static RTCManager* getInstance();

  bool init();
  void startSync();
  void stopSync();

  DateTime getCurrentTime();
  bool setTime(DateTime newTime);
  bool forceNtpSync();
  void getStatus(JsonObject& status);

  ~RTCManager();
};

#endif