#include "EthernetManager.h"

#include "DebugConfig.h"  // MUST BE FIRST for DEV_SERIAL_* macros

EthernetManager* EthernetManager::instance = nullptr;

EthernetManager::EthernetManager()
    : initialized(false),
      configStored(false),
      referenceCount(0),
      refCountMutex(nullptr),
      storedUseDhcp(true),
      lastReconnectAttempt(0),
      reconnectCount(0) {
  generateMacAddress();
  // Create mutex for reference counting protection
  refCountMutex = xSemaphoreCreateMutex();
}

EthernetManager* EthernetManager::getInstance() {
  // Thread-safe Meyers Singleton (C++11 guarantees thread-safe static init)
  // Avoids race condition in getInstance() that could create multiple instances
  static EthernetManager instance;
  static EthernetManager* ptr = &instance;
  return ptr;
}

void EthernetManager::generateMacAddress() {
  // Generate MAC from ESP32 chip ID
  uint64_t chipid = ESP.getEfuseMac();
  mac[0] = 0x02;  // Locally administered
  mac[1] = (chipid >> 32) & 0xFF;
  mac[2] = (chipid >> 24) & 0xFF;
  mac[3] = (chipid >> 16) & 0xFF;
  mac[4] = (chipid >> 8) & 0xFF;
  mac[5] = chipid & 0xFF;
}

bool EthernetManager::init(bool useDhcp, IPAddress staticIp, IPAddress gateway,
                           IPAddress subnet) {
  if (initialized) {
    // FIXED: Thread-safe referenceCount increment (was unprotected - race
    // condition)
    if (refCountMutex &&
        xSemaphoreTake(refCountMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      referenceCount++;
      // v2.5.35: Use DEV_MODE check to prevent log leak in production
      DEV_SERIAL_PRINTF("Ethernet already initialized (refs: %d)\n",
                        referenceCount);
      xSemaphoreGive(refCountMutex);
    }
    return true;
  }

  // v2.5.33: Store config for reconnect
  storedUseDhcp = useDhcp;
  storedStaticIp = staticIp;
  storedGateway = gateway;
  storedSubnet = subnet;
  configStored = true;

  // Configure SPI pins for W5500
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);

  // Initialize Ethernet
  Ethernet.init(CS_PIN);

  // v2.5.35: Use DEV_MODE check for all Ethernet logs to prevent leak in
  // production
  if (useDhcp) {
    DEV_SERIAL_PRINTLN("[ETHERNET] Starting with DHCP...");
    if (Ethernet.begin(mac) == 0) {
      DEV_SERIAL_PRINTLN("[ETHERNET] ERROR: Failed to configure using DHCP");
      if (Ethernet.hardwareStatus() == EthernetNoHardware) {
        DEV_SERIAL_PRINTLN("[ETHERNET] ERROR: Shield not found");
      }
      if (Ethernet.linkStatus() == LinkOFF) {
        DEV_SERIAL_PRINTLN("[ETHERNET] ERROR: Cable not connected");
      }
      return false;
    } else {
      DEV_SERIAL_PRINTF("[ETHERNET] Configured | Mode: DHCP | IP: %s\n",
                        Ethernet.localIP().toString().c_str());
    }
  } else {
    DEV_SERIAL_PRINTF("[ETHERNET] Starting with static IP: %s\n",
                      staticIp.toString().c_str());
    Ethernet.begin(mac, staticIp, gateway, subnet);
    DEV_SERIAL_PRINTF("[ETHERNET] Configured | Mode: Static | IP: %s\n",
                      Ethernet.localIP().toString().c_str());
  }

  // Check for Ethernet hardware
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    DEV_SERIAL_PRINTLN("[ETHERNET] ERROR: Shield not found");
    return false;
  }

  if (Ethernet.linkStatus() == LinkOFF) {
    DEV_SERIAL_PRINTLN("[ETHERNET] ERROR: Cable not connected");
    return false;
  }

  initialized = true;
  referenceCount = 1;
  DEV_SERIAL_PRINTLN("[ETHERNET] Initialized successfully");
  return true;
}

void EthernetManager::addReference() {
  // Protect with mutex to prevent race condition
  if (refCountMutex &&
      xSemaphoreTake(refCountMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    if (initialized) {
      referenceCount++;
      // v2.5.35: Use DEV_MODE check to prevent log leak in production
      DEV_SERIAL_PRINTF("Ethernet reference added (refs: %d)\n",
                        referenceCount);
    }
    xSemaphoreGive(refCountMutex);
  }
}

void EthernetManager::removeReference() {
  // Protect with mutex to prevent race condition
  if (refCountMutex &&
      xSemaphoreTake(refCountMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    if (referenceCount > 0) {
      referenceCount--;
      // v2.5.35: Use DEV_MODE check to prevent log leak in production
      DEV_SERIAL_PRINTF("Ethernet reference removed (refs: %d)\n",
                        referenceCount);

      if (referenceCount == 0) {
        cleanup();
      }
    }
    xSemaphoreGive(refCountMutex);
  }
}

void EthernetManager::cleanup() {
  referenceCount = 0;
  initialized = false;
  // v2.5.35: Use DEV_MODE check to prevent log leak in production
  DEV_SERIAL_PRINTLN("Ethernet resources cleaned up");
}

bool EthernetManager::isAvailable() {
  if (!initialized) return false;

  // Check link status
  return Ethernet.linkStatus() == LinkON;
}

IPAddress EthernetManager::getLocalIP() {
  if (initialized) {
    return Ethernet.localIP();
  }
  return IPAddress(0, 0, 0, 0);
}

void EthernetManager::getStatus(JsonObject& status) {
  status["initialized"] = initialized;
  status["available"] = isAvailable();
  status["reference_count"] = referenceCount;

  if (initialized) {
    status["ip_address"] = getLocalIP().toString();
    status["link_status"] =
        (Ethernet.linkStatus() == LinkON) ? "connected" : "disconnected";
    status["hardware_status"] =
        (Ethernet.hardwareStatus() == EthernetW5500) ? "W5500" : "unknown";
  }
}

// v2.5.33: Check if config is stored for reconnect
bool EthernetManager::hasStoredConfig() const { return configStored; }

// v2.5.33: Attempt to reconnect using stored config
bool EthernetManager::tryReconnect() {
  if (!configStored) {
    return false;  // No config stored
  }

  if (initialized && Ethernet.linkStatus() == LinkON) {
    return true;  // Already connected
  }

  reconnectCount++;
  lastReconnectAttempt = millis();

  // v2.5.35: Use DEV_MODE check for all reconnect logs to prevent leak in
  // production
  DEV_SERIAL_PRINTF("[ETHERNET] Reconnect attempt #%lu\n", reconnectCount);

  // Check hardware first
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    DEV_SERIAL_PRINTLN("[ETHERNET] Reconnect failed: Shield not found");
    return false;
  }

  // Check link status
  if (Ethernet.linkStatus() == LinkOFF) {
    DEV_SERIAL_PRINTF(
        "[ETHERNET] Reconnect failed: Cable not connected (attempt #%lu)\n",
        reconnectCount);
    return false;
  }

  // Link is ON, try to get IP
  if (storedUseDhcp) {
    DEV_SERIAL_PRINTLN("[ETHERNET] Reconnecting with DHCP...");
    if (Ethernet.begin(mac) == 0) {
      DEV_SERIAL_PRINTF(
          "[ETHERNET] DHCP failed (attempt #%lu), will retry later\n",
          reconnectCount);
      return false;
    }
  } else {
    DEV_SERIAL_PRINTF("[ETHERNET] Reconnecting with static IP: %s\n",
                      storedStaticIp.toString().c_str());
    Ethernet.begin(mac, storedStaticIp, storedGateway, storedSubnet);
  }

  // Verify we got an IP
  if (Ethernet.localIP() == IPAddress(0, 0, 0, 0)) {
    DEV_SERIAL_PRINTF(
        "[ETHERNET] Reconnect failed: No IP assigned (attempt #%lu)\n",
        reconnectCount);
    return false;
  }

  initialized = true;
  referenceCount = 1;
  DEV_SERIAL_PRINTF(
      "[ETHERNET] Reconnected successfully | IP: %s (attempt #%lu)\n",
      Ethernet.localIP().toString().c_str(), reconnectCount);
  return true;
}

EthernetManager::~EthernetManager() {
  cleanup();

  // Delete mutex to prevent memory leak
  if (refCountMutex) {
    vSemaphoreDelete(refCountMutex);
    refCountMutex = nullptr;
  }

  // v2.5.35: Use DEV_MODE check to prevent log leak in production
  DEV_SERIAL_PRINTLN("[ETHERNET] Manager destroyed, resources cleaned up");
}