#ifndef ETHERNET_MANAGER_H
#define ETHERNET_MANAGER_H

#include <ArduinoJson.h>
#include <Ethernet.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "JsonDocumentPSRAM.h"  // BUG #31: MUST BE BEFORE ArduinoJson.h

class EthernetManager {
 private:
  static EthernetManager* instance;
  bool initialized;
  bool configStored;  // v2.5.33: Track if config is stored for reconnect
  int referenceCount;
  byte mac[6];
  SemaphoreHandle_t
      refCountMutex;  // Protect reference counting from race condition

  // W5500 pins
  static const int CS_PIN = 48;
  static const int INT_PIN = 9;
  static const int MOSI_PIN = 14;
  static const int MISO_PIN = 21;
  static const int SCK_PIN = 47;

  // v2.5.33: Stored config for reconnect
  bool storedUseDhcp;
  IPAddress storedStaticIp;
  IPAddress storedGateway;
  IPAddress storedSubnet;

  // v2.5.33: Reconnect tracking
  unsigned long lastReconnectAttempt;
  uint32_t reconnectCount;

  EthernetManager();
  void generateMacAddress();

 public:
  static EthernetManager* getInstance();

  bool init(bool useDhcp = true, IPAddress staticIp = IPAddress(0, 0, 0, 0),
            IPAddress gateway = IPAddress(0, 0, 0, 0),
            IPAddress subnet = IPAddress(0, 0, 0, 0));
  void addReference();
  void removeReference();
  void cleanup();

  bool isAvailable();
  IPAddress getLocalIP();
  void getStatus(JsonObject& status);

  // v2.5.33: Reconnect support
  bool tryReconnect();           // Attempt to reconnect using stored config
  bool hasStoredConfig() const;  // Check if config is available for reconnect
  bool isInitialized() const { return initialized; }
  uint32_t getReconnectCount() const { return reconnectCount; }

  ~EthernetManager();
};

#endif