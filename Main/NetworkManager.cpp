#include "DebugConfig.h"  // MUST BE FIRST for LOG_* macros
#include "NetworkManager.h"
#include "ServerConfig.h"
#include <WiFi.h>

NetworkMgr *NetworkMgr::instance = nullptr;

NetworkMgr *NetworkMgr::getInstance()
{
  if (!instance)
  {
    instance = new NetworkMgr();
  }
  return instance;
}

NetworkMgr::NetworkMgr()
    : wifiManager(nullptr), ethernetManager(nullptr), primaryMode(""), activeMode(""), networkAvailable(false), failoverTaskHandle(nullptr), modeMutex(nullptr)
{
  // FIXED Bug #6: Initialize mutex for thread-safe activeMode access
  modeMutex = xSemaphoreCreateMutex();
  if (!modeMutex)
  {
    LOG_NET_INFO("[NetworkMgr] ERROR: Failed to create mode mutex");
  }
}

bool NetworkMgr::init(ServerConfig *serverConfig)
{
  if (!serverConfig)
  {
    Serial.println("ServerConfig is null");
    return false;
  }

  // 1. Ambil seluruh konfigurasi server ke dalam JsonObject sementara
  JsonDocument serverConfigDoc; // Ukuran disesuaikan jika perlu
  JsonObject serverRoot = serverConfigDoc.to<JsonObject>();
  if (!serverConfig->getConfig(serverRoot))
  {
    LOG_NET_INFO("[NetworkMgr] Failed to get full server config");
    // Pertimbangkan penanganan error, misalnya kembali ke default atau return false
  }

  // 2. Tentukan primaryMode (Logika ini bisa tetap, atau disesuaikan jika 'mode' juga pindah)
  // Periksa apakah field 'communication' masih ada dan berisi 'primary_network_mode' atau 'mode'
  String oldMode = "";
  String primaryNetworkMode = "ETH"; // Default jika tidak ditemukan

  // v2.3.14 FIX: Add defensive JsonObject validation before cast
  if (serverRoot["communication"] && serverRoot["communication"].is<JsonObject>())
  {
    JsonObject comm = serverRoot["communication"].as<JsonObject>();
    oldMode = comm["mode"] | "";                               // Baca field lama jika ada
    primaryNetworkMode = comm["primary_network_mode"] | "ETH"; // Baca field baru
  }

  if (oldMode == "ETH" || oldMode == "WIFI")
  {
    primaryMode = oldMode;
  }
  else
  {
    primaryMode = primaryNetworkMode;
  }
  // Verbose logging removed for cleaner output

  // 3. Ambil config WiFi dari ROOT server config
  JsonObject wifiConfig;
  bool wifiConfigPresent = false;
  // v2.3.14 FIX: Validate JsonObject type before cast
  if (serverRoot["wifi"] && serverRoot["wifi"].is<JsonObject>())
  {
    wifiConfig = serverRoot["wifi"].as<JsonObject>();
    wifiConfigPresent = true;
  }
  // Ambil status 'enabled' dari objek wifi yang ditemukan
  bool wifiEnabled = wifiConfigPresent && (wifiConfig["enabled"] | false);

  // 4. Ambil config Ethernet dari ROOT server config
  JsonObject ethernetConfig;
  bool ethernetConfigPresent = false;
  // v2.3.14 FIX: Validate JsonObject type before cast
  if (serverRoot["ethernet"] && serverRoot["ethernet"].is<JsonObject>())
  {
    ethernetConfig = serverRoot["ethernet"].as<JsonObject>();
    ethernetConfigPresent = true;
  }
  // Ambil status 'enabled' dari objek ethernet yang ditemukan
  bool ethernetEnabled = ethernetConfigPresent && (ethernetConfig["enabled"] | false);

  // Always initialize WiFi library (needed for WiFiClient even when using Ethernet)
  WiFi.mode(WIFI_STA);

  // Initialize WiFi if enabled (Gunakan wifiConfig dari root)
  if (wifiEnabled)
  {
    if (!initWiFi(wifiConfig))
    {
      LOG_NET_INFO("[NetworkMgr] ERROR: Failed to initialize WiFi");
    }
  }

  // Initialize Ethernet if enabled (Gunakan ethernetConfig dari root)
  if (ethernetEnabled)
  {
    bool useDhcp = ethernetConfig["use_dhcp"] | true;
    IPAddress staticIp, gateway, subnet;
    if (!useDhcp)
    {
      staticIp.fromString(ethernetConfig["static_ip"] | "0.0.0.0");
      gateway.fromString(ethernetConfig["gateway"] | "0.0.0.0");
      subnet.fromString(ethernetConfig["subnet"] | "0.0.0.0");
      if (staticIp.toString() == "0.0.0.0")
      {
        LOG_NET_INFO("[NetworkMgr] WARNING: Static IP is 0.0.0.0 or invalid");
      }
    }
    if (!initEthernet(useDhcp, staticIp, gateway, subnet))
    {
      LOG_NET_INFO("[NetworkMgr] ERROR: Failed to initialize Ethernet");
    }
  }

  // --- Logika penentuan activeMode tetap sama ---
  // Tentukan mode aktif awal berdasarkan primaryMode dan ketersediaan
  if (primaryMode == "ETH" && ethernetManager && ethernetManager->isAvailable())
  {
    activeMode = "ETH";
  }
  else if (primaryMode == "WIFI" && wifiManager && wifiManager->isAvailable())
  {
    activeMode = "WIFI";
  }
  else if (ethernetManager && ethernetManager->isAvailable())
  { // Fallback ke Ethernet jika ada
    activeMode = "ETH";
  }
  else if (wifiManager && wifiManager->isAvailable())
  { // Fallback ke WiFi jika ada
    activeMode = "WIFI";
  }
  else
  {
    activeMode = "NONE";
  }

  if (activeMode != "NONE")
  {
    networkAvailable = true;
    LOG_NET_INFO("[NetworkMgr] Initial active network: %s. IP: %s\n", activeMode.c_str(), getLocalIP().toString().c_str());
  }
  else
  {
    networkAvailable = false;
    LOG_NET_INFO("[NetworkMgr] No network available initially.");
  }

  startFailoverTask(); // Mulai task failover
  return true;         // Kembalikan true jika inisialisasi (walaupun tanpa koneksi) berhasil
}

bool NetworkMgr::initWiFi(const JsonObject &wifiConfig)
{
  String ssid = wifiConfig["ssid"] | "";
  String password = wifiConfig["password"] | "";

  if (ssid.isEmpty())
  {
    Serial.println("WiFi SSID not provided");
    return false;
  }

  wifiManager = WiFiManager::getInstance();
  if (wifiManager->init(ssid, password))
  {
    LOG_NET_INFO("[NETWORK] Initialized | Mode: WiFi | SSID: %s\n", ssid.c_str());
    return true;
  }

  return false;
}

bool NetworkMgr::initEthernet(bool useDhcp, IPAddress staticIp, IPAddress gateway, IPAddress subnet)
{
  ethernetManager = EthernetManager::getInstance();
  if (ethernetManager->init(useDhcp, staticIp, gateway, subnet))
  {
    LOG_NET_INFO("[NETWORK] Initialized | Mode: Ethernet");
    return true;
  }

  return false;
}

void NetworkMgr::startFailoverTask()
{
  if (failoverTaskHandle == nullptr)
  {
    // OPTIMIZATION: Moved to Core 0 to balance load (network monitoring background task)
    xTaskCreatePinnedToCore(
        failoverTask,
        "NET_FAILOVER_TASK",
        4096, // Stack size
        this,
        1, // Priority
        &failoverTaskHandle,
        0 // Core 0 (moved from Core 1 for load balancing)
    );
    LOG_NET_INFO("[NETWORK] Failover task started");
  }
}

void NetworkMgr::failoverTask(void *parameter)
{
  NetworkMgr *manager = static_cast<NetworkMgr *>(parameter);
  manager->failoverLoop();
}

void NetworkMgr::failoverLoop()
{
  unsigned long lastCheck = 0;
  unsigned long lastSignalCheck = 0;

  while (true)
  {
    unsigned long now = millis();

    // Use configurable check interval instead of hardcoded 5000
    if (now - lastCheck >= failoverCheckInterval)
    {
      lastCheck = now;

      bool primaryAvailable = false;
      bool secondaryAvailable = false;
      int8_t primaryRSSI = 0;
      int8_t secondaryRSSI = 0;

      // Check primary mode availability
      if (primaryMode == "ETH" && ethernetManager)
      {
        primaryAvailable = ethernetManager->isAvailable();
      }
      else if (primaryMode == "WIFI" && wifiManager)
      {
        primaryAvailable = wifiManager->isAvailable();
        if (primaryAvailable)
        {
          primaryRSSI = (int8_t)getWiFiRSSI(); // Get RSSI for WiFi
        }
      }

      // Check secondary mode availability
      if (primaryMode == "ETH" && wifiManager)
      {
        secondaryAvailable = wifiManager->isAvailable();
        if (secondaryAvailable)
        {
          secondaryRSSI = (int8_t)getWiFiRSSI();
        }
      }
      else if (primaryMode == "WIFI" && ethernetManager)
      {
        secondaryAvailable = ethernetManager->isAvailable();
      }

      // Network failover switching logic
      if (activeMode == "NONE")
      {
        if (primaryAvailable)
        {
          switchMode(primaryMode);
        }
        else if (secondaryAvailable)
        {
          String secondaryMode = (primaryMode == "ETH") ? "WIFI" : "ETH";
          switchMode(secondaryMode);
        }
      }
      else if (activeMode == primaryMode)
      {
        if (!primaryAvailable)
        {
          Serial.printf("Primary network (%s) lost. Attempting to switch to secondary.\n", primaryMode.c_str());
          String secondaryMode = (primaryMode == "ETH") ? "WIFI" : "ETH";

          if (secondaryAvailable)
          {
            switchMode(secondaryMode);
          }
          else
          {
            switchMode("NONE"); // Both down
          }
        }
      }
      else
      { // activeMode is secondary
        if (!secondaryAvailable)
        {
          Serial.printf("Secondary network (%s) lost. Attempting to switch to primary.\n", activeMode.c_str());
          if (primaryAvailable)
          {
            switchMode(primaryMode);
          }
        }
        else if (primaryAvailable)
        {
          // Primary is back, switch back to primary
          Serial.printf("Primary network (%s) restored. Switching back.\n", primaryMode.c_str());
          switchMode(primaryMode);
        }
      }

      // Update WiFi signal strength monitoring
      if (now - lastSignalCheck >= signalStrengthCheckInterval)
      {
        lastSignalCheck = now;
        updateWiFiSignalStrength();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100)); // Small delay to yield
  }
}

void NetworkMgr::switchMode(const String &newMode)
{
  // FIXED BUG #10 (was Bug #6): Protect activeMode access with mutex
  // VERIFIED: Thread-safe mode switching with 1s timeout
  if (!modeMutex)
  {
    LOG_NET_INFO("[NetworkMgr] ERROR: Mode mutex not initialized");
    return;
  }

  if (xSemaphoreTake(modeMutex, pdMS_TO_TICKS(1000)) != pdTRUE)
  {
    LOG_NET_INFO("[NetworkMgr] ERROR: Failed to acquire mode mutex");
    return;
  }

  // Early exit if no change needed (inside mutex to ensure consistency)
  if (newMode == activeMode)
  {
    xSemaphoreGive(modeMutex);
    return; // No change needed
  }

  Serial.printf("Switching network mode from %s to %s\n", activeMode.c_str(), newMode.c_str());

  // Cleanup old active mode if any
  if (activeMode == "WIFI" && wifiManager)
  {
    wifiManager->removeReference(); // Decrement reference count
  }
  else if (activeMode == "ETH" && ethernetManager)
  {
    ethernetManager->removeReference(); // Decrement reference count
  }

  // Set new active mode
  activeMode = newMode;
  if (activeMode == "WIFI" && wifiManager)
  {
    wifiManager->addReference(); // Increment reference count
    networkAvailable = true;
  }
  else if (activeMode == "ETH" && ethernetManager)
  {
    ethernetManager->addReference(); // Increment reference count
    networkAvailable = true;
  }
  else
  {
    networkAvailable = false;
  }

  if (networkAvailable)
  {
    Serial.printf("Successfully switched to %s. IP: %s\n", activeMode.c_str(), getLocalIP().toString().c_str());
  }
  else
  {
    Serial.println("No network active.");
  }

  xSemaphoreGive(modeMutex); // Release mutex
}

bool NetworkMgr::isAvailable()
{
  // FIXED Bug #6: Protect activeMode read with mutex
  if (!modeMutex)
  {
    return false; // Safety fallback
  }

  bool available = false;
  if (xSemaphoreTake(modeMutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    if (activeMode == "WIFI" && wifiManager)
    {
      available = wifiManager->isAvailable();
    }
    else if (activeMode == "ETH" && ethernetManager)
    {
      available = ethernetManager->isAvailable();
    }
    xSemaphoreGive(modeMutex);
  }
  else
  {
    LOG_NET_INFO("[NetworkMgr] WARNING: isAvailable mutex timeout");
  }

  return available;
}

IPAddress NetworkMgr::getLocalIP()
{
  // FIXED Bug #6: Protect activeMode read with mutex
  if (!modeMutex)
  {
    return IPAddress(0, 0, 0, 0); // Safety fallback
  }

  IPAddress ip(0, 0, 0, 0);
  if (xSemaphoreTake(modeMutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    if (activeMode == "WIFI" && wifiManager && wifiManager->isAvailable())
    {
      ip = wifiManager->getLocalIP();
    }
    else if (activeMode == "ETH" && ethernetManager && ethernetManager->isAvailable())
    {
      // Tambahkan ini untuk mendapatkan IP Ethernet
      ip = ethernetManager->getLocalIP();
    }
    xSemaphoreGive(modeMutex);
  }
  else
  {
    LOG_NET_INFO("[NetworkMgr] WARNING: getLocalIP mutex timeout");
  }

  return ip;
}

String NetworkMgr::getCurrentMode()
{
  // FIXED BUG #10 (was Bug #6): Protect activeMode read with mutex
  // VERIFIED: Thread-safe mode reading with 100ms timeout
  if (!modeMutex)
  {
    return ""; // Safety fallback
  }

  String mode;
  if (xSemaphoreTake(modeMutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    mode = activeMode;
    xSemaphoreGive(modeMutex);
  }
  else
  {
    LOG_NET_INFO("[NetworkMgr] WARNING: getCurrentMode mutex timeout");
    mode = ""; // Return empty on timeout
  }

  return mode;
}

Client *NetworkMgr::getActiveClient()
{
  // FIXED Bug #6: Protect activeMode read with mutex
  if (!modeMutex)
  {
    return nullptr; // Safety fallback
  }

  Client *client = nullptr;
  if (xSemaphoreTake(modeMutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    if (activeMode == "WIFI" && wifiManager && wifiManager->isAvailable())
    {
      client = &_wifiClient;
    }
    else if (activeMode == "ETH" && ethernetManager && ethernetManager->isAvailable())
    {
      // Kembalikan EthernetClient jika Ethernet aktif
      client = &_ethernetClient;
    }
    xSemaphoreGive(modeMutex);
  }
  else
  {
    LOG_NET_INFO("[NetworkMgr] WARNING: getActiveClient mutex timeout");
  }

  return client;
}

void NetworkMgr::cleanup()
{
  if (failoverTaskHandle)
  {
    vTaskDelete(failoverTaskHandle);
    failoverTaskHandle = nullptr;
  }
  if (wifiManager)
  {
    wifiManager->cleanup();
  }
  if (ethernetManager)
  {
    ethernetManager->cleanup();
  }
  networkAvailable = false;
  activeMode = "NONE";
}

// Configurable failover timeout setters
void NetworkMgr::setFailoverCheckInterval(uint32_t intervalMs)
{
  failoverCheckInterval = intervalMs;
  // Verbose logging removed for cleaner output
}

void NetworkMgr::setFailoverSwitchDelay(uint32_t delayMs)
{
  failoverSwitchDelay = delayMs;
  // Verbose logging removed for cleaner output
}

void NetworkMgr::setSignalStrengthCheckInterval(uint32_t intervalMs)
{
  signalStrengthCheckInterval = intervalMs;
  wifiSignalMetrics.rssiCheckInterval = intervalMs;
  // Verbose logging removed for cleaner output
}

// Configurable failover timeout getters
uint32_t NetworkMgr::getFailoverCheckInterval() const
{
  return failoverCheckInterval;
}

uint32_t NetworkMgr::getFailoverSwitchDelay() const
{
  return failoverSwitchDelay;
}

uint32_t NetworkMgr::getSignalStrengthCheckInterval() const
{
  return signalStrengthCheckInterval;
}

// Signal strength monitoring implementation
void NetworkMgr::updateWiFiSignalStrength()
{
  if (activeMode != "WIFI" || !wifiManager)
  {
    return;
  }

  unsigned long now = millis();
  if (now - wifiSignalMetrics.lastChecked >= wifiSignalMetrics.rssiCheckInterval)
  {
    wifiSignalMetrics.lastChecked = now;
    wifiSignalMetrics.rssi = getWiFiRSSI();
    wifiSignalMetrics.signalQuality = calculateSignalQuality(wifiSignalMetrics.rssi);
    // Verbose logging removed - signal metrics tracked internally
  }
}

int32_t NetworkMgr::getWiFiRSSI() const
{
  if (wifiManager && activeMode == "WIFI")
  {
    return WiFi.RSSI(); // Get current RSSI from WiFi
  }
  return 0;
}

uint8_t NetworkMgr::calculateSignalQuality(int32_t rssi) const
{
  // Convert RSSI (dBm) to quality percentage
  // RSSI range: -30 (excellent) to -120 (poor)
  // Quality: 100% at -30dBm, 0% at -120dBm and below

  if (rssi >= -30)
  {
    return 100;
  }
  else if (rssi <= -120)
  {
    return 0;
  }
  else
  {
    // Linear interpolation: ((-30) - rssi) / ((-30) - (-120)) * 100
    return (rssi + 120) * 100 / 90;
  }
}

int32_t NetworkMgr::getWiFiSignalStrength() const
{
  return wifiSignalMetrics.rssi;
}

uint8_t NetworkMgr::getWiFiSignalQuality() const
{
  return wifiSignalMetrics.signalQuality;
}

// Connection pooling implementation
NetworkMgr::PooledClientConnection *NetworkMgr::getPooledClient(const String &clientId)
{
  // Search for existing client in pool
  for (auto &conn : clientPool)
  {
    if (conn.clientId == clientId)
    {
      conn.lastUsed = millis();
      conn.requestCount++;
      // Verbose logging removed for cleaner output
      return &conn;
    }
  }

  // Client not in pool, add new one if space available
  if (clientPool.size() < maxPoolSize)
  {
    PooledClientConnection newConn;
    newConn.clientId = clientId;
    newConn.client = getActiveClient();
    newConn.isConnected = (newConn.client != nullptr);
    newConn.lastUsed = millis();
    newConn.requestCount = 0;

    clientPool.push_back(newConn);
    // Verbose logging removed for cleaner output
    return &clientPool.back();
  }
  else
  {
    LOG_NET_INFO("[NetworkMgr] WARNING: Client pool full (%ld/%d), cannot add: %s\n",
                  clientPool.size(), maxPoolSize, clientId.c_str());
  }

  return nullptr;
}

void NetworkMgr::releasePooledClient(const String &clientId)
{
  for (auto &conn : clientPool)
  {
    if (conn.clientId == clientId && conn.client)
    {
      conn.client->stop();
      conn.isConnected = false;
      // Verbose logging removed for cleaner output
      return;
    }
  }
}

void NetworkMgr::cleanupPooledClients()
{
  for (auto &conn : clientPool)
  {
    if (conn.client)
    {
      conn.client->stop();
    }
  }
  clientPool.clear();
  // Verbose logging removed for cleaner output
}

int NetworkMgr::getPooledClientCount() const
{
  return clientPool.size();
}

void NetworkMgr::getPoolStats(JsonObject &stats) const
{
  stats["pool_size"] = (int)clientPool.size();
  stats["max_pool_size"] = maxPoolSize;

  // Use modern ArduinoJson 7.x syntax (avoid deprecated methods)
  JsonArray clients = stats["clients"].to<JsonArray>();
  for (const auto &conn : clientPool)
  {
    JsonObject clientObj = clients.add<JsonObject>();
    clientObj["id"] = conn.clientId;
    clientObj["connected"] = conn.isConnected;
    clientObj["requests"] = conn.requestCount;
    clientObj["last_used_ago_ms"] = (unsigned long)(millis() - conn.lastUsed);
  }
}

void NetworkMgr::printNetworkStatus() const
{
  Serial.println("\n[NETWORK MGR] NETWORK FAILOVER STATUS REPORT\n");

  Serial.printf("  Current Active Mode: %s\n", activeMode.c_str());
  Serial.printf("  Primary Mode: %s\n", primaryMode.c_str());
  Serial.printf("  Overall Network Available: %s\n\n", networkAvailable ? "YES" : "NO");

  if (wifiManager && wifiManager->isAvailable())
  {
    Serial.printf("  WiFi Signal: RSSI=%ld dBm, Quality=%d%%\n\n",
                  wifiSignalMetrics.rssi, wifiSignalMetrics.signalQuality);
  }

  Serial.println();
}

NetworkMgr::~NetworkMgr()
{
  cleanupPooledClients(); // Clean up client pool
  cleanup();

  // FIXED Bug #6: Delete mutex
  if (modeMutex)
  {
    vSemaphoreDelete(modeMutex);
    modeMutex = nullptr;
  }
}