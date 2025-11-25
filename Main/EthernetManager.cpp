#include "EthernetManager.h"

EthernetManager *EthernetManager::instance = nullptr;

EthernetManager::EthernetManager()
    : initialized(false), referenceCount(0), refCountMutex(nullptr)
{
  generateMacAddress();
  // Create mutex for reference counting protection
  refCountMutex = xSemaphoreCreateMutex();
}

EthernetManager *EthernetManager::getInstance()
{
  // Thread-safe Meyers Singleton (C++11 guarantees thread-safe static init)
  // Avoids race condition in getInstance() that could create multiple instances
  static EthernetManager instance;
  static EthernetManager *ptr = &instance;
  return ptr;
}

void EthernetManager::generateMacAddress()
{
  // Generate MAC from ESP32 chip ID
  uint64_t chipid = ESP.getEfuseMac();
  mac[0] = 0x02; // Locally administered
  mac[1] = (chipid >> 32) & 0xFF;
  mac[2] = (chipid >> 24) & 0xFF;
  mac[3] = (chipid >> 16) & 0xFF;
  mac[4] = (chipid >> 8) & 0xFF;
  mac[5] = chipid & 0xFF;
}

bool EthernetManager::init(bool useDhcp, IPAddress staticIp, IPAddress gateway, IPAddress subnet)
{
  if (initialized)
  {
    // FIXED: Thread-safe referenceCount increment (was unprotected - race condition)
    if (refCountMutex && xSemaphoreTake(refCountMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
      referenceCount++;
      Serial.printf("Ethernet already initialized (refs: %d)\n", referenceCount);
      xSemaphoreGive(refCountMutex);
    }
    return true;
  }

  // Configure SPI pins for W5500
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);

  // Initialize Ethernet
  Ethernet.init(CS_PIN);

  if (useDhcp)
  {
    Serial.println("[ETHERNET] Starting with DHCP...");
    if (Ethernet.begin(mac) == 0)
    {
      Serial.println("[ETHERNET] ERROR: Failed to configure using DHCP");
      if (Ethernet.hardwareStatus() == EthernetNoHardware)
      {
        Serial.println("[ETHERNET] ERROR: Shield not found");
      }
      if (Ethernet.linkStatus() == LinkOFF)
      {
        Serial.println("[ETHERNET] ERROR: Cable not connected");
      }
      return false;
    }
    else
    {
      Serial.printf("[ETHERNET] Configured | Mode: DHCP | IP: %s\n", Ethernet.localIP().toString().c_str());
    }
  }
  else
  {
    Serial.printf("[ETHERNET] Starting with static IP: %s\n", staticIp.toString().c_str());
    Ethernet.begin(mac, staticIp, gateway, subnet);
    Serial.printf("[ETHERNET] Configured | Mode: Static | IP: %s\n", Ethernet.localIP().toString().c_str());
  }

  // Check for Ethernet hardware
  if (Ethernet.hardwareStatus() == EthernetNoHardware)
  {
    Serial.println("[ETHERNET] ERROR: Shield not found");
    return false;
  }

  if (Ethernet.linkStatus() == LinkOFF)
  {
    Serial.println("[ETHERNET] ERROR: Cable not connected");
    return false;
  }

  initialized = true;
  referenceCount = 1;
  Serial.println("[ETHERNET] Initialized successfully");
  return true;
}

void EthernetManager::addReference()
{
  // Protect with mutex to prevent race condition
  if (refCountMutex && xSemaphoreTake(refCountMutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    if (initialized)
    {
      referenceCount++;
      Serial.printf("Ethernet reference added (refs: %d)\n", referenceCount);
    }
    xSemaphoreGive(refCountMutex);
  }
}

void EthernetManager::removeReference()
{
  // Protect with mutex to prevent race condition
  if (refCountMutex && xSemaphoreTake(refCountMutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    if (referenceCount > 0)
    {
      referenceCount--;
      Serial.printf("Ethernet reference removed (refs: %d)\n", referenceCount);

      if (referenceCount == 0)
      {
        cleanup();
      }
    }
    xSemaphoreGive(refCountMutex);
  }
}

void EthernetManager::cleanup()
{
  referenceCount = 0;
  initialized = false;
  Serial.println("Ethernet resources cleaned up");
}

bool EthernetManager::isAvailable()
{
  if (!initialized)
    return false;

  // Check link status
  return Ethernet.linkStatus() == LinkON;
}

IPAddress EthernetManager::getLocalIP()
{
  if (initialized)
  {
    return Ethernet.localIP();
  }
  return IPAddress(0, 0, 0, 0);
}

void EthernetManager::getStatus(JsonObject &status)
{
  status["initialized"] = initialized;
  status["available"] = isAvailable();
  status["reference_count"] = referenceCount;

  if (initialized)
  {
    status["ip_address"] = getLocalIP().toString();
    status["link_status"] = (Ethernet.linkStatus() == LinkON) ? "connected" : "disconnected";
    status["hardware_status"] = (Ethernet.hardwareStatus() == EthernetW5500) ? "W5500" : "unknown";
  }
}

EthernetManager::~EthernetManager()
{
  cleanup();

  // Delete mutex to prevent memory leak
  if (refCountMutex)
  {
    vSemaphoreDelete(refCountMutex);
    refCountMutex = nullptr;
  }

  Serial.println("[ETHERNET] Manager destroyed, resources cleaned up");
}