#include "WiFiManager.h"

#include "DebugConfig.h"  // MUST BE FIRST for LOG_* macros

WiFiManager* WiFiManager::instance = nullptr;

WiFiManager::WiFiManager()
    : initialized(false),
      configStored(false),
      referenceCount(0),
      refCountMutex(nullptr),
      lastReconnectAttempt(0),
      reconnectCount(0) {
  // FIXED: Create mutex for thread-safe referenceCount operations
  refCountMutex = xSemaphoreCreateMutex();
  if (!refCountMutex) {
    LOG_NET_INFO("[WiFi] ERROR: Failed to create refCountMutex");
  }
}

WiFiManager* WiFiManager::getInstance() {
  // Thread-safe Meyers Singleton (C++11 guarantees thread-safe static init)
  // Avoids race condition in getInstance() that could create multiple instances
  static WiFiManager instance;
  static WiFiManager* ptr = &instance;
  return ptr;
}

bool WiFiManager::init(const String& ssidParam, const String& passwordParam) {
  if (initialized) {
    // FIXED: Thread-safe referenceCount increment
    if (refCountMutex &&
        xSemaphoreTake(refCountMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      referenceCount++;
      LOG_NET_INFO("[WiFi] Already initialized (refs: %d)\n", referenceCount);
      xSemaphoreGive(refCountMutex);
    }
    return true;
  }

  ssid = ssidParam;
  password = passwordParam;
  configStored = true;  // v2.5.33: Mark credentials as stored for reconnect

  // Check if already connected to same network
  if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == ssid) {
    initialized = true;
    referenceCount = 1;
    LOG_NET_INFO("[WiFi] Already connected to: %s\n", ssid.c_str());
    return true;
  }

  // Disconnect if connected to different network
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect();
    delay(1000);
  }

  LOG_NET_INFO("[WiFi] Connecting to: %s\n", ssid.c_str());
  WiFi.begin(ssid.c_str(), password.c_str());

  // Wait for connection with timeout
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    // v2.5.35: Use DEV_MODE check to prevent log leak in production
    DEV_SERIAL_PRINT(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    initialized = true;
    referenceCount = 1;
    LOG_NET_INFO("\n[WiFi] Connected | IP: %s\n",
                 WiFi.localIP().toString().c_str());
    return true;
  } else {
    // v2.5.35: Use DEV_MODE check to prevent log leak in production
    DEV_SERIAL_PRINTLN("\n[WiFi] ERROR: Connection failed");
    return false;
  }
}

void WiFiManager::addReference() {
  // FIXED: Thread-safe referenceCount increment
  if (refCountMutex &&
      xSemaphoreTake(refCountMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    if (initialized) {
      referenceCount++;
      LOG_NET_INFO("[WiFi] Reference added (refs: %d)\n", referenceCount);
    }
    xSemaphoreGive(refCountMutex);
  }
}

void WiFiManager::removeReference() {
  // FIXED: Thread-safe referenceCount decrement
  if (refCountMutex &&
      xSemaphoreTake(refCountMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    if (referenceCount > 0) {
      referenceCount--;
      LOG_NET_INFO("[WiFi] Reference removed (refs: %d)\n", referenceCount);

      if (referenceCount == 0) {
        xSemaphoreGive(refCountMutex);
        cleanup();
        return;
      }
    }
    xSemaphoreGive(refCountMutex);
  }
}

void WiFiManager::cleanup() {
  if (initialized) {
    WiFi.disconnect();
    initialized = false;
  }
  referenceCount = 0;
  LOG_NET_INFO("[WiFi] Resources cleaned up");
}

bool WiFiManager::isAvailable() {
  return initialized && WiFi.status() == WL_CONNECTED;
}

IPAddress WiFiManager::getLocalIP() {
  if (initialized) {
    return WiFi.localIP();
  }
  return IPAddress(0, 0, 0, 0);
}

String WiFiManager::getSSID() { return ssid; }

void WiFiManager::getStatus(JsonObject& status) {
  status["initialized"] = initialized;
  status["available"] = isAvailable();
  status["reference_count"] = referenceCount;
  status["ssid"] = ssid;

  if (initialized) {
    status["ip_address"] = getLocalIP().toString();
    status["rssi"] = WiFi.RSSI();

    String statusStr;
    switch (WiFi.status()) {
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

// v2.5.33: Check if credentials are stored for reconnect
bool WiFiManager::hasStoredConfig() const {
  return configStored && ssid.length() > 0;
}

// v2.5.33: Attempt to reconnect using stored credentials
bool WiFiManager::tryReconnect() {
  if (!configStored || ssid.isEmpty()) {
    return false;  // No credentials stored
  }

  if (initialized && WiFi.status() == WL_CONNECTED) {
    return true;  // Already connected
  }

  reconnectCount++;
  lastReconnectAttempt = millis();

  LOG_NET_INFO("[WiFi] Reconnect attempt #%lu to: %s\n", reconnectCount,
               ssid.c_str());

  // Disconnect if in bad state
  if (WiFi.status() != WL_DISCONNECTED && WiFi.status() != WL_IDLE_STATUS) {
    WiFi.disconnect();
    delay(100);
  }

  // Attempt connection
  WiFi.begin(ssid.c_str(), password.c_str());

  // Wait for connection with shorter timeout (5 seconds for reconnect)
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(500);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    initialized = true;
    referenceCount = 1;
    LOG_NET_INFO("[WiFi] Reconnected successfully | IP: %s (attempt #%lu)\n",
                 WiFi.localIP().toString().c_str(), reconnectCount);
    return true;
  } else {
    LOG_NET_INFO("[WiFi] Reconnect failed (attempt #%lu), will retry later\n",
                 reconnectCount);
    return false;
  }
}

WiFiManager::~WiFiManager() {
  cleanup();

  // FIXED: Delete mutex to prevent memory leak
  if (refCountMutex) {
    vSemaphoreDelete(refCountMutex);
    refCountMutex = nullptr;
  }

  LOG_NET_INFO("[WiFi] Manager destroyed, resources cleaned up");
}