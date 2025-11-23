#include "ServerConfig.h"
#include <esp_heap_caps.h>
#include <new>

const char *ServerConfig::CONFIG_FILE = "/server_config.json";

ServerConfig::ServerConfig() : suppressRestart(false)
{
  // Allocate config in PSRAM
  config = (JsonDocument *)heap_caps_malloc(sizeof(JsonDocument), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (config)
  {
    new (config) JsonDocument();
  }
  else
  {
    config = new JsonDocument();
  }
  createDefaultConfig();
}

ServerConfig::~ServerConfig()
{
  if (config)
  {
    config->~JsonDocument();
    heap_caps_free(config);
  }
}

bool ServerConfig::begin()
{
  if (!loadConfig())
  {
    Serial.println("No server config found, using defaults");
    return saveConfig();
  }
  Serial.println("[SERVER] Config initialized");
  return true;
}

void ServerConfig::restartDeviceTask(void *parameter)
{
  Serial.println("[RESTART] Device will restart in 5 seconds after server config update...");
  vTaskDelay(pdMS_TO_TICKS(5000));
  Serial.println("[RESTART] Restarting device now!");
  ESP.restart();
}

void ServerConfig::scheduleDeviceRestart()
{
  Serial.println("[RESTART] Scheduling device restart after server config update");
  xTaskCreate(
      restartDeviceTask,
      "SERVER_RESTART_TASK",
      2048,
      nullptr,
      1,
      nullptr);
}

void ServerConfig::createDefaultConfig()
{
  config->clear();
  JsonObject root = config->to<JsonObject>();

  // Communication config (mobile app structure)
  JsonObject comm = root["communication"].to<JsonObject>();
  comm["mode"] = "ETH";  // Mobile app expects this field

  // WiFi at root level (mobile app structure)
  JsonObject wifi = root["wifi"].to<JsonObject>();
  wifi["enabled"] = true;
  wifi["ssid"] = "";
  wifi["password"] = "";

  // Ethernet at root level (mobile app structure)
  JsonObject ethernet = root["ethernet"].to<JsonObject>();
  ethernet["enabled"] = true;
  ethernet["use_dhcp"] = true;
  ethernet["static_ip"] = "";
  ethernet["gateway"] = "";
  ethernet["subnet"] = "";

  // Protocol
  root["protocol"] = "mqtt";

  // MQTT config with publish modes
  JsonObject mqtt = root["mqtt_config"].to<JsonObject>();
  mqtt["enabled"] = true;
  mqtt["broker_address"] = "broker.hivemq.com";
  mqtt["broker_port"] = 1883;
  mqtt["client_id"] = "";
  mqtt["username"] = "";
  mqtt["password"] = "";
  mqtt["topic_publish"] = "v1/devices/me/telemetry";  // Top level for mobile app compatibility
  mqtt["topic_subscribe"] = "";  // Top level for mobile app compatibility

  // FIXED: Increased default keep_alive from 60s to 120s
  // This prevents MQTT disconnections during long Modbus RTU polling cycles (50s+)
  mqtt["keep_alive"] = 120;
  mqtt["clean_session"] = true;
  mqtt["use_tls"] = false;
  mqtt["publish_mode"] = "default";  // "default" or "customize"

  // Default mode configuration (for MQTT modes feature)
  JsonObject defaultMode = mqtt["default_mode"].to<JsonObject>();
  defaultMode["enabled"] = true;
  defaultMode["topic_publish"] = "v1/devices/me/telemetry";
  defaultMode["topic_subscribe"] = "";
  defaultMode["interval"] = 5;
  defaultMode["interval_unit"] = "s";  // "ms" (milliseconds), "s" (seconds), "m" (minutes)

  // Customize mode configuration (for MQTT modes feature)
  JsonObject customizeMode = mqtt["customize_mode"].to<JsonObject>();
  customizeMode["enabled"] = false;
  customizeMode["custom_topics"].to<JsonArray>();

  // HTTP config
  JsonObject http = root["http_config"].to<JsonObject>();
  http["enabled"] = false;
  http["endpoint_url"] = "https://api.example.com/data";
  http["method"] = "POST";
  http["body_format"] = "json";
  http["timeout"] = 5000;
  http["retry"] = 3;
  http["interval"] = 5;           // HTTP transmission interval
  http["interval_unit"] = "s";    // "ms", "s", or "m"

  JsonObject headers = http["headers"].to<JsonObject>();
  headers["Authorization"] = "Bearer token";
  headers["Content-Type"] = "application/json";
}

bool ServerConfig::saveConfig()
{
  File file = LittleFS.open(CONFIG_FILE, "w");
  if (!file)
    return false;

  serializeJson(*config, file);
  file.close();
  return true;
}

bool ServerConfig::loadConfig()
{
  File file = LittleFS.open(CONFIG_FILE, "r");
  if (!file)
    return false;

  DeserializationError error = deserializeJson(*config, file);
  file.close();

  if (error)
  {
    Serial.println("Failed to parse server config");
    return false;
  }

  return validateConfig(*config);
}

bool ServerConfig::validateConfig(const JsonDocument &cfg)
{
  if (!cfg["communication"] || !cfg["protocol"])
  {
    return false;
  }
  return true;
}

bool ServerConfig::getConfig(JsonObject &result)
{
  // Copy all config fields
  for (JsonPair kv : config->as<JsonObject>())
  {
    result[kv.key()] = kv.value();
  }

  // Defensive: Ensure critical MQTT fields have default values if missing/null
  if (!result["mqtt_config"])
  {
    result["mqtt_config"].to<JsonObject>();
  }
  JsonObject mqtt = result["mqtt_config"];

  if (mqtt["enabled"].isNull()) mqtt["enabled"] = true;
  if (mqtt["broker_address"].isNull()) mqtt["broker_address"] = "broker.hivemq.com";
  if (mqtt["broker_port"].isNull()) mqtt["broker_port"] = 1883;
  if (mqtt["client_id"].isNull()) mqtt["client_id"] = "";
  if (mqtt["username"].isNull()) mqtt["username"] = "";
  if (mqtt["password"].isNull()) mqtt["password"] = "";
  if (mqtt["topic_publish"].isNull()) mqtt["topic_publish"] = "v1/devices/me/telemetry";  // Top level for mobile app
  if (mqtt["topic_subscribe"].isNull()) mqtt["topic_subscribe"] = "";  // Top level for mobile app

  // FIXED: Increased default keep_alive from 60s to 120s
  if (mqtt["keep_alive"].isNull()) mqtt["keep_alive"] = 120;
  if (mqtt["clean_session"].isNull()) mqtt["clean_session"] = true;
  if (mqtt["use_tls"].isNull()) mqtt["use_tls"] = false;
  if (mqtt["publish_mode"].isNull()) mqtt["publish_mode"] = "default";

  // Ensure default_mode exists (for MQTT modes feature)
  if (!mqtt["default_mode"])
  {
    JsonObject defaultMode = mqtt["default_mode"].to<JsonObject>();
    defaultMode["enabled"] = true;
    defaultMode["topic_publish"] = "v1/devices/me/telemetry";
    defaultMode["topic_subscribe"] = "";
    defaultMode["interval"] = 5;
    defaultMode["interval_unit"] = "s";
  }

  // Ensure customize_mode exists (for MQTT modes feature)
  if (!mqtt["customize_mode"])
  {
    JsonObject customizeMode = mqtt["customize_mode"].to<JsonObject>();
    customizeMode["enabled"] = false;
    customizeMode["custom_topics"].to<JsonArray>();
  }

  // Defensive: Ensure communication config exists (mobile app structure)
  if (!result["communication"])
  {
    result["communication"].to<JsonObject>();
  }
  JsonObject comm = result["communication"];
  if (comm["mode"].isNull()) comm["mode"] = "ETH";  // Mobile app expects this field

  // WiFi at root level (mobile app structure)
  if (!result["wifi"])
  {
    result["wifi"].to<JsonObject>();
  }
  JsonObject wifi = result["wifi"];
  if (wifi["enabled"].isNull()) wifi["enabled"] = true;
  if (wifi["ssid"].isNull()) wifi["ssid"] = "";
  if (wifi["password"].isNull()) wifi["password"] = "";

  // Ethernet at root level (mobile app structure)
  if (!result["ethernet"])
  {
    result["ethernet"].to<JsonObject>();
  }
  JsonObject ethernet = result["ethernet"];
  if (ethernet["enabled"].isNull()) ethernet["enabled"] = true;
  if (ethernet["use_dhcp"].isNull()) ethernet["use_dhcp"] = true;
  if (ethernet["static_ip"].isNull()) ethernet["static_ip"] = "";
  if (ethernet["gateway"].isNull()) ethernet["gateway"] = "";
  if (ethernet["subnet"].isNull()) ethernet["subnet"] = "";

  // Defensive: Ensure protocol exists
  if (result["protocol"].isNull()) result["protocol"] = "mqtt";

  // Defensive: Ensure http_config exists
  if (!result["http_config"])
  {
    result["http_config"].to<JsonObject>();
  }
  JsonObject http = result["http_config"];

  // Ensure individual fields exist
  if (http["enabled"].isNull()) http["enabled"] = false;
  if (http["endpoint_url"].isNull()) http["endpoint_url"] = "";
  if (http["method"].isNull()) http["method"] = "POST";  // CRITICAL for mobile app!
  if (http["body_format"].isNull()) http["body_format"] = "json";  // CRITICAL for mobile app!
  if (http["timeout"].isNull()) http["timeout"] = 5000;
  if (http["retry"].isNull()) http["retry"] = 3;
  if (http["interval"].isNull()) http["interval"] = 5;  // v2.2.0: HTTP transmission interval
  if (http["interval_unit"].isNull()) http["interval_unit"] = "s";  // v2.2.0: Interval unit
  if (!http["headers"])
  {
    http["headers"].to<JsonObject>();
  }

  return true;
}

bool ServerConfig::updateConfig(JsonObjectConst newConfig)
{
  // Create temporary config for validation
  JsonDocument tempConfig;
  tempConfig.set(newConfig);

  if (!validateConfig(tempConfig))
  {
    return false;
  }

  // Update main config
  config->set(newConfig);

  // Sync topic_publish/topic_subscribe from top level to default_mode if not present
  JsonObject mqtt = (*config)["mqtt_config"].as<JsonObject>();
  if (mqtt)
  {
    // If mobile app sends top-level topic fields but no default_mode, sync them
    if (!mqtt["default_mode"])
    {
      mqtt["default_mode"].to<JsonObject>();
    }
    JsonObject defaultMode = mqtt["default_mode"];

    // Copy top-level topics to default_mode if they exist
    if (!mqtt["topic_publish"].isNull() && defaultMode["topic_publish"].isNull())
    {
      defaultMode["topic_publish"] = mqtt["topic_publish"];
    }
    if (!mqtt["topic_subscribe"].isNull() && defaultMode["topic_subscribe"].isNull())
    {
      defaultMode["topic_subscribe"] = mqtt["topic_subscribe"];
    }

    // Ensure default_mode has all required fields with defaults
    if (defaultMode["enabled"].isNull()) defaultMode["enabled"] = true;
    if (defaultMode["interval"].isNull()) defaultMode["interval"] = 5;
    if (defaultMode["interval_unit"].isNull()) defaultMode["interval_unit"] = "s";

    // Ensure customize_mode exists
    if (!mqtt["customize_mode"])
    {
      JsonObject customizeMode = mqtt["customize_mode"].to<JsonObject>();
      customizeMode["enabled"] = false;
      customizeMode["custom_topics"].to<JsonArray>();
    }
  }

  if (saveConfig())
  {
    Serial.println("Server configuration updated successfully");

    // Check if restart should be suppressed (e.g., during factory reset)
    if (!suppressRestart)
    {
      scheduleDeviceRestart();
    }
    else
    {
      Serial.println("[RESTART] Restart suppressed (factory reset in progress)");
    }

    return true;
  }
  return false;
}

bool ServerConfig::getCommunicationConfig(JsonObject &result)
{
  if (config->as<JsonObject>()["communication"])
  {
    JsonObject comm = (*config)["communication"];
    for (JsonPair kv : comm)
    {
      result[kv.key()] = kv.value();
    }
    return true;
  }
  return false;
}

String ServerConfig::getProtocol()
{
  return (*config)["protocol"] | "mqtt";
}

bool ServerConfig::getMqttConfig(JsonObject &result)
{
  if (config->as<JsonObject>()["mqtt_config"])
  {
    JsonObject mqtt = (*config)["mqtt_config"];
    for (JsonPair kv : mqtt)
    {
      result[kv.key()] = kv.value();
    }
    return true;
  }
  return false;
}

bool ServerConfig::getHttpConfig(JsonObject &result)
{
  if (config->as<JsonObject>()["http_config"])
  {
    JsonObject http = (*config)["http_config"];
    for (JsonPair kv : http)
    {
      result[kv.key()] = kv.value();
    }
    return true;
  }
  return false;
}

bool ServerConfig::getWifiConfig(JsonObject &result)
{
  if (config->as<JsonObject>()["communication"])
  {
    JsonObject comm = (*config)["communication"];
    if (comm["wifi"])
    {
      JsonObject wifi = comm["wifi"];
      for (JsonPair kv : wifi)
      {
        result[kv.key()] = kv.value();
      }
      return true;
    }
  }
  return false;
}

bool ServerConfig::getEthernetConfig(JsonObject &result)
{
  if (config->as<JsonObject>()["communication"])
  {
    JsonObject comm = (*config)["communication"];
    if (comm["ethernet"])
    {
      JsonObject ethernet = comm["ethernet"];
      for (JsonPair kv : ethernet)
      {
        result[kv.key()] = kv.value();
      }
      return true;
    }
  }
  return false;
}

String ServerConfig::getPrimaryNetworkMode()
{
  if (config->as<JsonObject>()["communication"])
  {
    JsonObject comm = (*config)["communication"];
    return comm["primary_network_mode"] | "ETH"; // Default to ETH if not specified
  }
  return "ETH"; // Default if communication object is missing
}

String ServerConfig::getCommunicationMode()
{
  if (config->as<JsonObject>()["communication"])
  {
    JsonObject comm = (*config)["communication"];
    return comm["mode"] | ""; // Return old 'mode' field if present, else empty
  }
  return "";
}