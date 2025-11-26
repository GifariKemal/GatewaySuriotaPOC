#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include "JsonDocumentPSRAM.h" // BUG #31: MUST BE BEFORE ArduinoJson.h
#include <ArduinoJson.h>
#include "WiFiManager.h"
#include "EthernetManager.h"
#include "ServerConfig.h" // Added for ServerConfig declaration
#include <vector>
#include <freertos/semphr.h> // For mutex (Bug #6 fix)

class NetworkMgr
{
private:
  static NetworkMgr *instance;
  WiFiManager *wifiManager;
  EthernetManager *ethernetManager;

  String primaryMode;    // Configured primary mode (e.g., "ETH", "WIFI")
  String activeMode;     // Currently active mode
  bool networkAvailable; // Overall network availability

  SemaphoreHandle_t modeMutex; // FIXED Bug #6: Protect activeMode from race conditions

  TaskHandle_t failoverTaskHandle;
  static void failoverTask(void *parameter);
  void failoverLoop();

  WiFiClient _wifiClient;         // Internal WiFiClient for MQTT/HTTP
  EthernetClient _ethernetClient; // Internal EthernetClient for MQTT/HTTP

  // Configurable failover timeouts (milliseconds)
  uint32_t failoverCheckInterval = 5000;       // How often to check failover (5 seconds)
  uint32_t failoverSwitchDelay = 1000;         // Delay before switching modes (1 second)
  uint32_t signalStrengthCheckInterval = 2000; // Check WiFi signal every 2 seconds

  // Signal strength monitoring
  struct SignalStrengthMetrics
  {
    int32_t rssi = 0;                  // WiFi RSSI (dBm)
    uint8_t signalQuality = 0;         // 0-100% quality
    unsigned long lastChecked = 0;     // Timestamp of last check
    uint32_t rssiCheckInterval = 2000; // Check interval (2 seconds)
  } wifiSignalMetrics;

  // Connection pooling for clients
  struct PooledClientConnection
  {
    String clientId;        // Unique client identifier
    Client *client;         // Pointer to client (WiFi or Ethernet)
    bool isConnected;       // Current connection status
    unsigned long lastUsed; // Timestamp of last use
    uint32_t requestCount;  // Total requests via this client
  };
  std::vector<PooledClientConnection> clientPool; // Pool of reusable clients
  uint8_t maxPoolSize = 5;                        // Maximum clients in pool

  NetworkMgr();
  bool initWiFi(const JsonObject &wifiConfig);
  bool initEthernet(bool useDhcp, IPAddress staticIp, IPAddress gateway, IPAddress subnet);
  void startFailoverTask();
  void switchMode(const String &newMode);

  // Signal strength monitoring methods
  void updateWiFiSignalStrength();
  int32_t getWiFiRSSI() const;
  uint8_t calculateSignalQuality(int32_t rssi) const;

  // Connection pooling methods
  PooledClientConnection *getPooledClient(const String &clientId);
  void releasePooledClient(const String &clientId);
  void cleanupPooledClients();

public:
  static NetworkMgr *getInstance();

  bool init(ServerConfig *serverConfig); // Changed parameter type
  bool isAvailable();
  IPAddress getLocalIP();
  String getCurrentMode();
  Client *getActiveClient(); // New method to get active client
  void cleanup();
  void getStatus(JsonObject &status);

  // Configurable timeout methods
  void setFailoverCheckInterval(uint32_t intervalMs);
  void setFailoverSwitchDelay(uint32_t delayMs);
  void setSignalStrengthCheckInterval(uint32_t intervalMs);
  uint32_t getFailoverCheckInterval() const;
  uint32_t getFailoverSwitchDelay() const;
  uint32_t getSignalStrengthCheckInterval() const;

  // Signal strength query methods
  int32_t getWiFiSignalStrength() const;
  uint8_t getWiFiSignalQuality() const;

  // Connection pooling query methods
  int getPooledClientCount() const;
  void getPoolStats(JsonObject &stats) const;

  // Network status methods
  void printNetworkStatus() const;

  ~NetworkMgr();
};

#endif