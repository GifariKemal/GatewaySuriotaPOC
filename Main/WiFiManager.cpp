#include "WiFiManager.h"

WiFiManager *WiFiManager::instance = nullptr;

WiFiManager::WiFiManager()
    : initialized(false), referenceCount(0), refCountMutex(nullptr)
{
  // FIXED: Create mutex for thread-safe referenceCount operations
  refCountMutex = xSemaphoreCreateMutex();
  if (!refCountMutex)
  {
    Serial.println("[WiFi] ERROR: Failed to create refCountMutex");
  }
}

WiFiManager *WiFiManager::getInstance()
{
  // Thread-safe Meyers Singleton (C++11 guarantees thread-safe static init)
  // Avoids race condition in getInstance() that could create multiple instances
  static WiFiManager instance;
  static WiFiManager *ptr = &instance;
  return ptr;
}

bool WiFiManager::init(const String &ssidParam, const String &passwordParam)
{
  if (initialized)
  {
    // FIXED: Thread-safe referenceCount increment
    if (refCountMutex && xSemaphoreTake(refCountMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
      referenceCount++;
      Serial.printf("[WiFi] Already initialized (refs: %d)\n", referenceCount);
      xSemaphoreGive(refCountMutex);
    }
    return true;
  }

  ssid = ssidParam;
  password = passwordParam;

  // Check if already connected to same network
  if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == ssid)
  {
    initialized = true;
    referenceCount = 1;
    Serial.printf("[WiFi] Already connected to: %s\n", ssid.c_str());
    return true;
  }

  // Disconnect if connected to different network
  if (WiFi.status() == WL_CONNECTED)
  {
    WiFi.disconnect();
    delay(1000);
  }

  Serial.printf("[WiFi] Connecting to: %s\n", ssid.c_str());
  WiFi.begin(ssid.c_str(), password.c_str());

  // Wait for connection with timeout
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20)
  {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    initialized = true;
    referenceCount = 1;
    Serial.printf("\n[WiFi] Connected | IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
  }
  else
  {
    Serial.println("\n[WiFi] ERROR: Connection failed");
    return false;
  }
}

void WiFiManager::addReference()
{
  // FIXED: Thread-safe referenceCount increment
  if (refCountMutex && xSemaphoreTake(refCountMutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    if (initialized)
    {
      referenceCount++;
      Serial.printf("[WiFi] Reference added (refs: %d)\n", referenceCount);
    }
    xSemaphoreGive(refCountMutex);
  }
}

void WiFiManager::removeReference()
{
  // FIXED: Thread-safe referenceCount decrement
  if (refCountMutex && xSemaphoreTake(refCountMutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    if (referenceCount > 0)
    {
      referenceCount--;
      Serial.printf("[WiFi] Reference removed (refs: %d)\n", referenceCount);

      if (referenceCount == 0)
      {
        xSemaphoreGive(refCountMutex);
        cleanup();
        return;
      }
    }
    xSemaphoreGive(refCountMutex);
  }
}

void WiFiManager::cleanup()
{
  if (initialized)
  {
    WiFi.disconnect();
    initialized = false;
  }
  referenceCount = 0;
  Serial.println("[WiFi] Resources cleaned up");
}

bool WiFiManager::isAvailable()
{
  return initialized && WiFi.status() == WL_CONNECTED;
}

IPAddress WiFiManager::getLocalIP()
{
  if (initialized)
  {
    return WiFi.localIP();
  }
  return IPAddress(0, 0, 0, 0);
}

String WiFiManager::getSSID()
{
  return ssid;
}

void WiFiManager::getStatus(JsonObject &status)
{
  status["initialized"] = initialized;
  status["available"] = isAvailable();
  status["reference_count"] = referenceCount;
  status["ssid"] = ssid;

  if (initialized)
  {
    status["ip_address"] = getLocalIP().toString();
    status["rssi"] = WiFi.RSSI();

    String statusStr;
    switch (WiFi.status())
    {
    case WL_CONNECTED:
      statusStr = "connected";
      break;
    case WL_DISCONNECTED:
      statusStr = "disconnected";
      break;
    case WL_CONNECTION_LOST:
      statusStr = "connection_lost";
      break;
    case WL_CONNECT_FAILED:
      statusStr = "connect_failed";
      break;
    default:
      statusStr = "unknown";
      break;
    }
    status["connection_status"] = statusStr;
  }
}

WiFiManager::~WiFiManager()
{
  cleanup();

  // FIXED: Delete mutex to prevent memory leak
  if (refCountMutex)
  {
    vSemaphoreDelete(refCountMutex);
    refCountMutex = nullptr;
  }

  Serial.println("[WiFi] Manager destroyed, resources cleaned up");
}