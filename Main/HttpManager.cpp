#include "DebugConfig.h"  // MUST BE FIRST for LOG_* macros
#include "HttpManager.h"
#include "LEDManager.h"

HttpManager *HttpManager::instance = nullptr;

HttpManager::HttpManager(ConfigManager *config, ServerConfig *serverCfg, NetworkMgr *netMgr)
    : configManager(config), queueManager(nullptr), serverConfig(serverCfg), networkManager(netMgr),
      running(false), taskHandle(nullptr), timeout(10000), retryCount(3), lastSendAttempt(0),
      lastDataTransmission(0), dataIntervalMs(5000)
{ // Default 5000ms (5 seconds)
  queueManager = QueueManager::getInstance();
}

HttpManager *HttpManager::getInstance(ConfigManager *config, ServerConfig *serverCfg, NetworkMgr *netMgr)
{
  if (instance == nullptr && config != nullptr && serverCfg != nullptr && netMgr != nullptr)
  {
    instance = new HttpManager(config, serverCfg, netMgr);
  }
  return instance;
}

bool HttpManager::init()
{
  LOG_NET_INFO("[HTTP] Initializing manager...");

  if (!configManager || !queueManager || !serverConfig || !networkManager)
  {
    LOG_NET_INFO("[HTTP] ERROR: ConfigManager, QueueManager, ServerConfig, or NetworkManager is null");
    return false;
  }

  loadHttpConfig();
  LOG_NET_INFO("[HTTP] Manager initialized");
  return true;
}

void HttpManager::start()
{
  LOG_NET_INFO("[HTTP] Starting manager...");

  if (running)
  {
    return;
  }

  running = true;
  BaseType_t result = xTaskCreatePinnedToCore(
      httpTask,
      "HTTP_TASK",
      8192,
      this,
      1,
      &taskHandle,
      0);

  if (result == pdPASS)
  {
    LOG_NET_INFO("[HTTP] Manager started successfully");
  }
  else
  {
    LOG_NET_INFO("[HTTP] ERROR: Failed to create HTTP task");
    running = false;
    taskHandle = nullptr;
  }
}

void HttpManager::stop()
{
  running = false;

  // Give task time to exit gracefully (checks 'running' flag in loop)
  if (taskHandle)
  {
    vTaskDelay(pdMS_TO_TICKS(50)); // Brief delay for current iteration to complete
    vTaskDelete(taskHandle);
    taskHandle = nullptr;
  }

  LOG_NET_INFO("[HTTP] Manager stopped");
}

void HttpManager::httpTask(void *parameter)
{
  HttpManager *manager = static_cast<HttpManager *>(parameter);
  manager->httpLoop();
}

void HttpManager::httpLoop()
{
  bool networkWasAvailable = false;

  LOG_NET_INFO("[HTTP] Task started");

  while (running)
  {
    // Check network availability
    bool networkAvailable = isNetworkAvailable();

    if (!networkAvailable)
    {
      if (networkWasAvailable)
      {
        LOG_NET_INFO("[HTTP] Network disconnected");
        networkWasAvailable = false;

        // Update LED status - network lost
        if (ledManager)
        {
          ledManager->setHttpConnectionStatus(false);
        }
      }
      LOG_NET_INFO("[HTTP] Waiting for network | Mode: %s | IP: %s\n",
                    networkManager->getCurrentMode().c_str(),
                    networkManager->getLocalIP().toString().c_str());

      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }
    else if (!networkWasAvailable)
    {
      LOG_NET_INFO("[HTTP] Network available | Mode: %s | IP: %s\n",
                    networkManager->getCurrentMode().c_str(),
                    networkManager->getLocalIP().toString().c_str());
      networkWasAvailable = true;

      // Update LED status - network available and endpoint configured
      if (ledManager && !endpointUrl.isEmpty())
      {
        ledManager->setHttpConnectionStatus(true);
      }
    }

    // Process queue data
    publishQueueData();

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

bool HttpManager::sendHttpRequest(const JsonObject &data)
{
  if (endpointUrl.isEmpty())
  {
    LOG_NET_INFO("[HTTP] No endpoint URL configured");
    return false;
  }

  LOG_NET_INFO("[HTTP] Sending request to %s\n", endpointUrl.c_str());

  Client *activeClient = networkManager->getActiveClient();
  if (!activeClient)
  {
    LOG_NET_INFO("[HTTP] No active network client available.");
    return false;
  }

  HTTPClient httpClient;         // Create local instance
  httpClient.begin(endpointUrl); // Let HTTPClient manage the client internally
  httpClient.setTimeout(timeout);

  // Set headers from configuration
  JsonDocument configDoc;
  JsonObject httpConfig = configDoc.to<JsonObject>();

  if (serverConfig->getHttpConfig(httpConfig))
  {
    JsonObject configHeaders = httpConfig["headers"];
    for (JsonPair header : configHeaders)
    {
      httpClient.addHeader(header.key().c_str(), header.value().as<String>());
      LOG_NET_INFO("[HTTP] Header: %s = %s\n", header.key().c_str(), header.value().as<String>().c_str());
    }
  }

  // Default headers
  httpClient.addHeader("Content-Type", "application/json");

  // Prepare payload
  String payload;
  serializeJson(data, payload);

  int httpResponseCode = -1;
  int attempts = 0;

  while (attempts < retryCount && httpResponseCode < 0)
  {
    attempts++;

    if (method == "POST")
    {
      httpResponseCode = httpClient.POST(payload);
    }
    else if (method == "PUT")
    {
      httpResponseCode = httpClient.PUT(payload);
    }
    else if (method == "PATCH")
    {
      httpResponseCode = httpClient.PATCH(payload);
    }
    else
    {
      LOG_NET_INFO("[HTTP] Unsupported method: %s\n", method.c_str());
      httpClient.end();
      return false;
    }

    if (httpResponseCode > 0)
    {
      LOG_NET_INFO("[HTTP] Response code: %d\n", httpResponseCode);

      if (httpResponseCode >= 200 && httpResponseCode < 300)
      {
        String response = httpClient.getString();
        LOG_NET_INFO("[HTTP] Success: %s\n", response.c_str());
        httpClient.end();
        if (ledManager)
        {
          ledManager->notifyDataTransmission();
        }
        return true;
      }
      else
      {
        String response = httpClient.getString();
        LOG_NET_INFO("[HTTP] Error response: %s\n", response.c_str());
      }
    }
    else
    {
      LOG_NET_INFO("[HTTP] Request failed, error: %s\n", httpClient.errorToString(httpResponseCode).c_str());
    }

    if (attempts < retryCount)
    {
      LOG_NET_INFO("[HTTP] Retrying in 2 seconds... (attempt %d/%d)\n", attempts + 1, retryCount);
      vTaskDelay(pdMS_TO_TICKS(2000));
    }
  }

  httpClient.end();
  return false;
}

void HttpManager::loadHttpConfig()
{
  JsonDocument configDoc;
  JsonObject httpConfig = configDoc.to<JsonObject>();

  LOG_NET_INFO("[HTTP] Loading HTTP configuration...");

  if (serverConfig->getHttpConfig(httpConfig))
  {
    bool enabled = httpConfig["enabled"] | false;
    if (!enabled)
    {
      LOG_NET_INFO("[HTTP] HTTP config disabled, clearing endpoint");
      endpointUrl = "";
      return;
    }

    endpointUrl = httpConfig["endpoint_url"] | "";
    endpointUrl.trim(); // FIXED Bug #9: Trim whitespace to avoid isEmpty() false assumption
    method = httpConfig["method"] | "POST";
    bodyFormat = httpConfig["body_format"] | "json";
    timeout = httpConfig["timeout"] | 10000;
    retryCount = httpConfig["retry"] | 3;

    LOG_NET_INFO("[HTTP] Config loaded | URL: %s | Method: %s | Timeout: %d | Retry: %d\n",
                  endpointUrl.c_str(), method.c_str(), timeout, retryCount);
  }
  else
  {
    LOG_NET_INFO("[HTTP] Failed to load HTTP config");
    endpointUrl = "";
    method = "POST";
    timeout = 10000;
    retryCount = 3;
  }

  // Load data transmission interval from http_config (v2.2.0+)
  JsonDocument httpConfigDoc;
  JsonObject httpConfigObj = httpConfigDoc.to<JsonObject>();
  if (serverConfig->getHttpConfig(httpConfigObj))
  {
    uint32_t intervalValue = httpConfigObj["interval"] | 5;
    String intervalUnit = httpConfigObj["interval_unit"] | "s";

    // Convert to milliseconds based on unit (ms, s, m)
    if (intervalUnit == "s")
    {
      dataIntervalMs = intervalValue * 1000; // seconds to ms
    }
    else if (intervalUnit == "m")
    {
      dataIntervalMs = intervalValue * 60000; // minutes to ms (60 * 1000)
    }
    else if (intervalUnit == "ms")
    {
      dataIntervalMs = intervalValue; // Already in ms
    }
    else
    {
      dataIntervalMs = intervalValue * 1000; // Default to seconds
    }

    LOG_NET_INFO("[HTTP] Data transmission interval set to: %lu ms (%u %s)\n",
                  dataIntervalMs, intervalValue, intervalUnit.c_str());
  }
  else
  {
    dataIntervalMs = 5000; // Default 5 seconds
    LOG_NET_INFO("[HTTP] Failed to load http_config interval, using default 5000ms");
  }

  // Initialize transmission timestamp to allow immediate first publish
  lastDataTransmission = 0;
}

void HttpManager::publishQueueData()
{
  if (endpointUrl.isEmpty())
  {
    return;
  }

  // Level 3: Check if data transmission interval has elapsed
  unsigned long now = millis();
  if ((now - lastDataTransmission) < dataIntervalMs)
  {
    // Interval not yet elapsed, don't publish yet
    return;
  }

  // Interval has elapsed, ready to publish
  //  Only dequeue and publish ONE batch of data (all items currently in queue)

  int sendCount = 0;
  bool anySent = false;

  // Process all available items in queue this cycle
  while (sendCount < 5)
  { // Limit to 5 per call to avoid blocking
    JsonDocument dataDoc;
    JsonObject dataPoint = dataDoc.to<JsonObject>();

    if (!queueManager->dequeue(dataPoint))
    {
      break; // No more data in queue
    }

    // Send HTTP request
    if (sendHttpRequest(dataPoint))
    {
      LOG_NET_INFO("[HTTP] Data sent successfully (Batch @ %lu ms)\n", now);
      anySent = true;
      sendCount++;
    }
    else
    {
      LOG_NET_INFO("[HTTP] Failed to send data, re-queuing\n");
      queueManager->enqueue(dataPoint);
      break;
    }

    vTaskDelay(pdMS_TO_TICKS(100)); // Small delay between requests
  }

  // Update transmission timestamp after successful send cycle
  if (anySent)
  {
    lastDataTransmission = now;
    LOG_NET_INFO("[HTTP] Next transmission in %lu ms\n", dataIntervalMs);
  }
}

bool HttpManager::isNetworkAvailable()
{
  if (!networkManager)
    return false;

  // Check if network manager says available
  if (!networkManager->isAvailable())
  {
    return false;
  }

  // Check actual IP from network manager
  IPAddress localIP = networkManager->getLocalIP();
  if (localIP == IPAddress(0, 0, 0, 0))
  {
    LOG_NET_INFO("[HTTP] Network manager available but no IP (%s)\n", networkManager->getCurrentMode().c_str());
    return false;
  }

  return true;
}

void HttpManager::debugNetworkConnectivity()
{
  Serial.println("\n[HTTP] NETWORK DEBUG");
  Serial.printf("  Current Mode: %s\n", networkManager->getCurrentMode().c_str());
  Serial.printf("  Network Available: %s\n", networkManager->isAvailable() ? "YES" : "NO");
  Serial.printf("  Local IP: %s\n", networkManager->getLocalIP().toString().c_str());

  // Show specific network details
  String mode = networkManager->getCurrentMode();
  if (mode == "WIFI")
  {
    Serial.printf("  WiFi Status: %d\n", WiFi.status());
    Serial.printf("  WiFi SSID: %s\n", WiFi.SSID().c_str());
    Serial.printf("  WiFi RSSI: %d dBm\n\n", WiFi.RSSI());
  }
  else if (mode == "ETH")
  {
    Serial.println("  Using Ethernet connection\n");
  }
}

void HttpManager::getStatus(JsonObject &status)
{
  status["running"] = running;
  status["service_type"] = "http_manager";
  status["network_available"] = isNetworkAvailable();
  status["endpoint_url"] = endpointUrl;
  status["method"] = method;
  status["timeout"] = timeout;
  status["retry_count"] = retryCount;
  status["queue_size"] = queueManager->size();
  status["data_interval_ms"] = dataIntervalMs; // Include current data interval in status
}

// Update data transmission interval at runtime (called when server config changes) - v2.2.0+
void HttpManager::updateDataTransmissionInterval()
{
  if (!serverConfig)
  {
    LOG_NET_INFO("[HTTP] ServerConfig is null, cannot update data interval");
    return;
  }

  JsonDocument httpConfigDoc;
  JsonObject httpConfigObj = httpConfigDoc.to<JsonObject>();

  if (serverConfig->getHttpConfig(httpConfigObj))
  {
    uint32_t intervalValue = httpConfigObj["interval"] | 5;
    String intervalUnit = httpConfigObj["interval_unit"] | "s";

    // Convert to milliseconds based on unit (ms, s, m)
    uint32_t newInterval;
    if (intervalUnit == "s")
    {
      newInterval = intervalValue * 1000; // seconds to ms
    }
    else if (intervalUnit == "m")
    {
      newInterval = intervalValue * 60000; // minutes to ms (60 * 1000)
    }
    else if (intervalUnit == "ms")
    {
      newInterval = intervalValue; // Already in ms
    }
    else
    {
      newInterval = intervalValue * 1000; // Default to seconds
    }

    if (newInterval != dataIntervalMs)
    {
      LOG_NET_INFO("[HTTP] Data transmission interval updated: %lu ms -> %lu ms (%u %s)\n",
                    dataIntervalMs, newInterval, intervalValue, intervalUnit.c_str());
      dataIntervalMs = newInterval;
      // Reset transmission timer to allow immediate publish with new interval
      lastDataTransmission = 0;
    }
    else
    {
      LOG_NET_INFO("[HTTP] Data transmission interval unchanged: %lu ms\n", dataIntervalMs);
    }
  }
  else
  {
    LOG_NET_INFO("[HTTP] Failed to load updated interval from http_config");
  }
}

HttpManager::~HttpManager()
{
  stop();
}