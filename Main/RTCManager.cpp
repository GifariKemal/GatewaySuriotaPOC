#include "RTCManager.h"
#include "NetworkManager.h"

RTCManager *RTCManager::instance = nullptr;

RTCManager::RTCManager()
    : initialized(false), syncRunning(false), syncTaskHandle(nullptr), lastNtpSync(0), ntpClient(nullptr) {}

RTCManager *RTCManager::getInstance()
{
  // Thread-safe Meyers Singleton (C++11 guarantees thread-safe static init)
  static RTCManager instance;
  static RTCManager *ptr = &instance;
  return ptr;
}

bool RTCManager::init()
{
  if (initialized)
  {
    Serial.println("[RTC] Already initialized");
    return true;
  }

  Wire.begin(5, 6); // SDA=5, SCL=6

  if (!rtc.begin())
  {
    Serial.println("[RTC] ERROR: Couldn't find RTC");
    return false;
  }

  if (rtc.lostPower())
  {
    Serial.println("[RTC] Lost power, setting time from compile time");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  DateTime rtcTime = rtc.now();

  // Check if RTC time is invalid or too old (before 2024)
  if (rtcTime.year() < 2024)
  {
    Serial.printf("[RTC] Invalid time (year=%d), setting from compile time\n", rtcTime.year());
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    rtcTime = rtc.now();
  }

  Serial.printf("[RTC] Current time: %04d-%02d-%02d %02d:%02d:%02d\n",
                rtcTime.year(), rtcTime.month(), rtcTime.day(),
                rtcTime.hour(), rtcTime.minute(), rtcTime.second());

  initialized = true;
  Serial.println("[RTC] Initialized successfully");

  // Update system time from RTC immediately
  updateSystemTime(rtcTime);

  // NTP sync will be handled by startSync() task
  // This prevents blocking during boot if network is not ready

  return true;
}

void RTCManager::startSync()
{
  if (!initialized || syncRunning)
    return;

  syncRunning = true;
  // OPTIMIZATION: Moved to Core 0 to balance load (background sync task)
  xTaskCreatePinnedToCore(
      timeSyncTask,
      "RTC_SYNC_TASK",
      4096,
      this,
      1,
      &syncTaskHandle,
      0); // Core 0 (moved from Core 1 for load balancing)
  Serial.println("[RTC] Sync service started");
}

void RTCManager::stopSync()
{
  syncRunning = false;
  if (syncTaskHandle)
  {
    vTaskDelete(syncTaskHandle);
    syncTaskHandle = nullptr;
  }
  Serial.println("[RTC] Sync service stopped");
}

void RTCManager::timeSyncTask(void *parameter)
{
  RTCManager *manager = static_cast<RTCManager *>(parameter);
  manager->timeSyncLoop();
}

void RTCManager::timeSyncLoop()
{
  while (syncRunning)
  {
    unsigned long now = millis();

    // Check if it's time to sync (first run or after interval)
    if (lastNtpSync == 0 || (now - lastNtpSync >= ntpUpdateInterval))
    {
      if (syncWithNTP())
      {
        lastNtpSync = now;
        Serial.println("[RTC] NTP sync successful");
        vTaskDelay(pdMS_TO_TICKS(ntpUpdateInterval)); // Wait full interval
      }
      else
      {
        Serial.println("[RTC] NTP sync failed, retrying in 1 minute");
        vTaskDelay(pdMS_TO_TICKS(60000)); // Retry in 1 minute
      }
    }
    else
    {
      // Check every 10 seconds if it's time to sync
      vTaskDelay(pdMS_TO_TICKS(10000));
    }
  }
}

bool RTCManager::syncWithNTP()
{
  NetworkMgr *networkMgr = NetworkMgr::getInstance();
  if (!networkMgr || !networkMgr->isAvailable())
  {
    Serial.println("[RTC] No network available for NTP sync");
    return false;
  }

  // Check internet connectivity before attempting NTP sync
  if (!checkInternetConnectivity())
  {
    Serial.println("[RTC] No internet connectivity, skipping NTP sync");
    return false;
  }

  String currentMode = networkMgr->getCurrentMode();
  Serial.printf("[RTC] Attempting NTP sync via %s\n", currentMode.c_str());

  // Clean up previous NTP client if exists
  if (ntpClient)
  {
    delete ntpClient;
    ntpClient = nullptr;
  }

  // Create NTP client based on active network mode
  if (currentMode == "WIFI")
  {
    ntpClient = new NTPClient(wifiUdp, ntpServer, gmtOffset_sec, 60000);
  }
  else if (currentMode == "ETH")
  {
    ntpClient = new NTPClient(ethernetUdp, ntpServer, gmtOffset_sec, 60000);
  }
  else
  {
    Serial.println("[RTC] Unknown network mode, cannot sync NTP");
    return false;
  }

  // Start NTP client
  ntpClient->begin();

  // Try to update with timeout
  unsigned long startTime = millis();
  bool success = false;

  while (millis() - startTime < ntpTimeout)
  {
    if (ntpClient->forceUpdate())
    {
      success = true;
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(500)); // Check every 500ms
  }

  if (!success)
  {
    Serial.printf("[RTC] NTP sync timeout after %lums via %s\n", ntpTimeout, currentMode.c_str());
    ntpClient->end();
    delete ntpClient;
    ntpClient = nullptr;
    return false;
  }

  // Get NTP time (already adjusted with GMT+7 offset by NTPClient)
  unsigned long epochTime = ntpClient->getEpochTime();

  // NTPClient already applies timeOffset, so epochTime is already in GMT+7
  // Convert directly to DateTime using epoch time
  DateTime ntpTime(epochTime);

  // Update RTC and system time
  rtc.adjust(ntpTime);
  updateSystemTime(ntpTime);

  Serial.printf("[RTC] NTP sync: %04d-%02d-%02d %02d:%02d:%02d (WIB/GMT+7) via %s\n",
                ntpTime.year(), ntpTime.month(), ntpTime.day(),
                ntpTime.hour(), ntpTime.minute(), ntpTime.second(),
                currentMode.c_str());

  // Clean up NTP client
  ntpClient->end();
  delete ntpClient;
  ntpClient = nullptr;

  return true;
}

bool RTCManager::checkInternetConnectivity()
{
  NetworkMgr *networkMgr = NetworkMgr::getInstance();
  if (!networkMgr || !networkMgr->isAvailable())
  {
    return false;
  }

  String currentMode = networkMgr->getCurrentMode();

  if (currentMode == "WIFI")
  {
    // Try DNS lookup via WiFi
    IPAddress testIP;
    int result = WiFi.hostByName("pool.ntp.org", testIP);
    if (result == 1)
    {
      Serial.println("[RTC] Internet connectivity confirmed via WiFi");
      return true;
    }
    Serial.println("[RTC] Internet connectivity check failed via WiFi");
    return false;
  }
  else if (currentMode == "ETH")
  {
    // For Ethernet, try to connect to Google DNS (8.8.8.8:53) to verify internet
    EthernetClient testClient;
    IPAddress googleDNS(8, 8, 8, 8);

    if (testClient.connect(googleDNS, 53))
    {
      testClient.stop();
      Serial.println("[RTC] Internet connectivity confirmed via Ethernet");
      return true;
    }

    Serial.println("[RTC] Internet connectivity check failed via Ethernet");
    return false;
  }

  Serial.printf("[RTC] Unknown network mode: %s\n", currentMode.c_str());
  return false;
}

void RTCManager::updateSystemTime(DateTime rtcTime)
{
  struct timeval tv;
  tv.tv_sec = rtcTime.unixtime();
  tv.tv_usec = 0;
  settimeofday(&tv, NULL);
}

DateTime RTCManager::getCurrentTime()
{
  if (!initialized)
  {
    return DateTime();
  }
  return rtc.now();
}

bool RTCManager::setTime(DateTime newTime)
{
  if (!initialized)
    return false;

  rtc.adjust(newTime);
  updateSystemTime(newTime);

  Serial.printf("[RTC] Time set: %04d-%02d-%02d %02d:%02d:%02d\n",
                newTime.year(), newTime.month(), newTime.day(),
                newTime.hour(), newTime.minute(), newTime.second());

  return true;
}

bool RTCManager::forceNtpSync()
{
  if (!initialized)
    return false;

  bool result = syncWithNTP();
  if (result)
  {
    lastNtpSync = millis();
  }

  return result;
}

void RTCManager::getStatus(JsonObject &status)
{
  status["initialized"] = initialized;
  status["sync_running"] = syncRunning;

  if (initialized)
  {
    DateTime current = getCurrentTime();
    status["current_time"] = String(current.year()) + "-" + String(current.month()) + "-" + String(current.day()) + " " + String(current.hour()) + ":" + String(current.minute()) + ":" + String(current.second());

    status["last_ntp_sync"] = lastNtpSync;
    status["rtc_temperature"] = rtc.getTemperature();
  }
}

RTCManager::~RTCManager()
{
  stopSync();

  // Clean up NTP client
  if (ntpClient)
  {
    ntpClient->end();
    delete ntpClient;
    ntpClient = nullptr;
  }
}