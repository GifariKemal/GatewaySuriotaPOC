#include "RTCManager.h"
#include "NetworkManager.h"

RTCManager *RTCManager::instance = nullptr;

RTCManager::RTCManager()
    : initialized(false), syncRunning(false), syncTaskHandle(nullptr), lastNtpSync(0) {}

RTCManager *RTCManager::getInstance()
{
  if (!instance)
  {
    instance = new RTCManager();
  }
  return instance;
}

bool RTCManager::init()
{
  if (initialized)
  {
    Serial.println("RTC already initialized");
    return true;
  }

  Wire.begin(5, 6); // SDA=5, SCL=6

  if (!rtc.begin())
  {
    Serial.println("Couldn't find RTC");
    return false;
  }

  if (rtc.lostPower())
  {
    Serial.println("RTC lost power, setting time from compile time");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  DateTime rtcTime = rtc.now();

  // Check if RTC time is invalid or too old (before 2024)
  if (rtcTime.year() < 2024)
  {
    Serial.printf("RTC has invalid time (year=%d), setting from compile time\n", rtcTime.year());
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    rtcTime = rtc.now();
  }

  Serial.printf("RTC current time: %04d-%02d-%02d %02d:%02d:%02d\n",
                rtcTime.year(), rtcTime.month(), rtcTime.day(),
                rtcTime.hour(), rtcTime.minute(), rtcTime.second());

  initialized = true;
  Serial.println("RTC initialized successfully");

  // Force NTP sync on boot to get accurate time from network
  if (syncWithNTP())
  {
    lastNtpSync = millis();
    Serial.println("Initial NTP sync successful");
  }
  else
  {
    Serial.println("Initial NTP sync failed - using RTC time");
    updateSystemTime(rtcTime);
  }

  return true;
}

void RTCManager::startSync()
{
  if (!initialized || syncRunning)
    return;

  syncRunning = true;
  xTaskCreatePinnedToCore(
      timeSyncTask,
      "RTC_SYNC_TASK",
      4096,
      this,
      1,
      &syncTaskHandle,
      1);
  Serial.println("RTC sync service started");
}

void RTCManager::stopSync()
{
  syncRunning = false;
  if (syncTaskHandle)
  {
    vTaskDelete(syncTaskHandle);
    syncTaskHandle = nullptr;
  }
  Serial.println("RTC sync service stopped");
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
    if (millis() - lastNtpSync > 1800000 || lastNtpSync == 0)
    {
      if (syncWithNTP())
      {
        lastNtpSync = millis();
        Serial.println("NTP sync successful");
        vTaskDelay(pdMS_TO_TICKS(1800000)); // 30 minutes
      }
      else
      {
        Serial.println("NTP sync failed, retrying in 1 minute");
        vTaskDelay(pdMS_TO_TICKS(60000)); // 1 minute
      }
    }
    else
    {
      vTaskDelay(pdMS_TO_TICKS(10000)); // Check every 10 seconds
    }
  }
}

bool RTCManager::syncWithNTP()
{
  NetworkMgr *networkMgr = NetworkMgr::getInstance();
  if (!networkMgr->isAvailable())
  {
    Serial.println("[RTC] No network available for NTP sync");
    return false;
  }

  // Reset system time to epoch before NTP sync
  struct timeval tv_reset;
  tv_reset.tv_sec = 0;
  tv_reset.tv_usec = 0;
  settimeofday(&tv_reset, NULL);

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, "time.nist.gov", "time.google.com");
  vTaskDelay(pdMS_TO_TICKS(2000));  // Reduced from 3s to 2s

  struct tm timeinfo = {0};
  int attempts = 0;
  bool timeValid = false;

  while (attempts < 10)  // Reduced from 20 to 10 attempts (faster startup)
  {
    if (getLocalTime(&timeinfo))
    {
      if (timeinfo.tm_year + 1900 >= 2024)
      {
        timeValid = true;
        break;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
    attempts++;
  }

  if (!timeValid)
  {
    Serial.printf("[RTC] NTP sync failed after %d attempts\n", attempts);
    return false;
  }

  DateTime ntpTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                   timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

  rtc.adjust(ntpTime);
  updateSystemTime(ntpTime);

  Serial.printf("[RTC] NTP sync: %04d-%02d-%02d %02d:%02d:%02d (WIB/GMT+7)\n",
                ntpTime.year(), ntpTime.month(), ntpTime.day(),
                ntpTime.hour(), ntpTime.minute(), ntpTime.second());

  return true;
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

  Serial.printf("RTC time set: %04d-%02d-%02d %02d:%02d:%02d\n",
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
}