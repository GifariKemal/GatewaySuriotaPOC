#include "ServerConfig.h"

#include <esp_heap_caps.h>

#include <new>

#include "DebugConfig.h"  // MUST BE FIRST for LOG_* macros

const char* ServerConfig::CONFIG_FILE = "/server_config.json";

ServerConfig::ServerConfig() : suppressRestart(false) {
  // Allocate config in PSRAM
  config = (JsonDocument*)heap_caps_malloc(sizeof(JsonDocument),
                                           MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (config) {
    new (config) JsonDocument();
  } else {
    config = new JsonDocument();
  }
  createDefaultConfig();
}

ServerConfig::~ServerConfig() {
  if (config) {
    config->~JsonDocument();
    heap_caps_free(config);
  }
}

bool ServerConfig::begin() {
  if (!loadConfig()) {
    Serial.println("No server config found, using defaults");
    return saveConfig();
  }
  LOG_CONFIG_INFO("[SERVER] Config initialized");
  return true;
}

void ServerConfig::restartDeviceTask(void* parameter) {
  // v2.3.6 OPTIMIZED: Reduced from 20s to 10s after threshold optimization
  // - Post-restore backup now takes ~1.5s (optimized from 3.5s via 25KB
  // threshold)
  // - Python processing: ~1s, Script delay: ~3s = Total ~5.5s
  // - 10-second delay provides 4.5s safety margin (adequate for production)
  Serial.println(
      "[RESTART] Device will restart in 10 seconds after server config "
      "update...");
  vTaskDelay(
      pdMS_TO_TICKS(10000));  // 10 seconds (was 20s in v2.3.5, 5s before that)
  Serial.println("[RESTART] Restarting device now!");
  ESP.restart();
}

void ServerConfig::scheduleDeviceRestart() {
  Serial.println(
      "[RESTART] Scheduling device restart after server config update");
  xTaskCreate(restartDeviceTask, "SERVER_RESTART_TASK", 2048, nullptr, 1,
              nullptr);
}

void ServerConfig::createDefaultConfig() {
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
  mqtt["topic_publish"] =
      "v1/devices/me/telemetry";  // Top level for mobile app compatibility
  mqtt["topic_subscribe"] = "";   // Top level for mobile app compatibility

  // FIXED: Increased default keep_alive from 60s to 120s
  // This prevents MQTT disconnections during long Modbus RTU polling cycles
  // (50s+)
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
  defaultMode["interval_unit"] =
      "s";  // "ms" (milliseconds), "s" (seconds), "m" (minutes)

  // Customize mode configuration (for MQTT modes feature)
  JsonObject customizeMode = mqtt["customize_mode"].to<JsonObject>();
  customizeMode["enabled"] = false;
  customizeMode["custom_topics"].to<JsonArray>();

  // v1.1.0: Subscribe control configuration (remote write via MQTT)
  JsonObject subscribeControl = mqtt["subscribe_control"].to<JsonObject>();
  subscribeControl["enabled"] = false;
  subscribeControl["topic_prefix"] = "";  // Auto-generated if empty
  subscribeControl["response_enabled"] = true;
  subscribeControl["default_qos"] = 1;

  // HTTP config
  JsonObject http = root["http_config"].to<JsonObject>();
  http["enabled"] = false;
  http["endpoint_url"] = "https://api.example.com/data";
  http["method"] = "POST";
  http["body_format"] = "json";
  http["timeout"] = 5000;
  http["retry"] = 3;
  http["interval"] = 5;         // HTTP transmission interval
  http["interval_unit"] = "s";  // "ms", "s", or "m"

  JsonObject headers = http["headers"].to<JsonObject>();
  headers["Authorization"] = "Bearer token";
  headers["Content-Type"] = "application/json";
}

bool ServerConfig::saveConfig() {
  File file = LittleFS.open(CONFIG_FILE, "w");
  if (!file) return false;

  serializeJson(*config, file);
  file.close();
  return true;
}

bool ServerConfig::loadConfig() {
  File file = LittleFS.open(CONFIG_FILE, "r");
  if (!file) return false;

  DeserializationError error = deserializeJson(*config, file);
  file.close();

  if (error) {
    Serial.println("Failed to parse server config");
    return false;
  }

  return validateConfig(*config);
}

bool ServerConfig::validateConfig(const JsonDocument& cfg) {
  // Use enhanced validation and store result
  lastValidationResult = validateConfigEnhanced(cfg);
  return lastValidationResult.isValid;
}

// v1.0.2: Helper to validate IPv4 address format
bool ServerConfig::isValidIPv4(const String& ip) {
  if (ip.isEmpty()) return false;

  int parts[4];
  int count = sscanf(ip.c_str(), "%d.%d.%d.%d", &parts[0], &parts[1], &parts[2],
                     &parts[3]);
  if (count != 4) return false;

  for (int i = 0; i < 4; i++) {
    if (parts[i] < 0 || parts[i] > 255) return false;
  }

  // Also check format - no extra characters
  char reconstructed[16];
  snprintf(reconstructed, sizeof(reconstructed), "%d.%d.%d.%d", parts[0],
           parts[1], parts[2], parts[3]);
  return ip.equals(reconstructed);
}

// v1.0.2: Helper to validate URL format
bool ServerConfig::isValidURL(const String& url) {
  if (url.isEmpty()) return false;
  return url.startsWith("http://") || url.startsWith("https://");
}

// v1.0.2: Enhanced validation with detailed error messages
ConfigValidationResult ServerConfig::validateConfigEnhanced(
    const JsonDocument& cfg) {
  // 1. Check required top-level fields
  if (!cfg["communication"]) {
    return ConfigValidationResult::error(
        509, "Missing required field: communication", "communication",
        "Include communication object with mode field");
  }

  if (!cfg["protocol"]) {
    return ConfigValidationResult::error(
        509, "Missing required field: protocol", "protocol",
        "Set protocol to 'mqtt' or 'http'");
  }

  // 2. Validate protocol value
  // v1.0.6 FIX: Case-insensitive comparison
  String protocol = cfg["protocol"] | "";
  if (!protocol.equalsIgnoreCase("mqtt") && !protocol.equalsIgnoreCase("http")) {
    return ConfigValidationResult::error(
        509, "Invalid protocol value. Must be 'mqtt' or 'http'", "protocol",
        "Set protocol to 'mqtt' or 'http'");
  }
  bool isMqttProtocol = protocol.equalsIgnoreCase("mqtt");
  bool isHttpProtocol = protocol.equalsIgnoreCase("http");

  // 3. Validate communication mode
  JsonObjectConst comm = cfg["communication"];
  String mode = comm["mode"] | "";
  if (mode.isEmpty()) {
    return ConfigValidationResult::error(509, "Missing communication mode",
                                         "communication.mode",
                                         "Set mode to 'ETH' or 'WiFi'");
  }

  // v1.0.6 FIX: Case-insensitive comparison for communication mode
  // Accept: "WIFI", "WiFi", "wifi" → WiFi mode
  // Accept: "ETH", "eth", "Eth" → ETH mode
  if (!mode.equalsIgnoreCase("ETH") && !mode.equalsIgnoreCase("WiFi") &&
      !mode.equalsIgnoreCase("WIFI")) {
    return ConfigValidationResult::error(
        509, "Invalid communication mode. Must be 'ETH' or 'WiFi'",
        "communication.mode", "Set mode to 'ETH' or 'WiFi'");
  }

  // Normalize mode for consistent checks below
  bool isWiFiMode = mode.equalsIgnoreCase("WiFi") || mode.equalsIgnoreCase("WIFI");
  bool isEthMode = mode.equalsIgnoreCase("ETH");

  // 4. WiFi validation (when mode is WiFi)
  if (isWiFiMode) {
    JsonObjectConst wifi = cfg["wifi"];
    if (!wifi) {
      return ConfigValidationResult::error(
          509, "WiFi configuration required when mode is 'WiFi'", "wifi",
          "Include wifi object with enabled, ssid, and password");
    }

    bool wifiEnabled = wifi["enabled"] | false;
    if (!wifiEnabled) {
      return ConfigValidationResult::error(
          509, "WiFi must be enabled when communication mode is 'WiFi'",
          "wifi.enabled", "Set wifi.enabled to true");
    }

    String ssid = wifi["ssid"] | "";
    if (ssid.isEmpty()) {
      return ConfigValidationResult::error(
          509, "WiFi SSID is required when WiFi is enabled", "wifi.ssid",
          "Provide a valid WiFi network name");
    }

    if (ssid.length() > 32) {
      return ConfigValidationResult::error(
          509, "WiFi SSID too long. Maximum 32 characters", "wifi.ssid",
          "Use an SSID with 32 characters or less");
    }

    String password = wifi["password"] | "";
    // Password can be empty for open networks, but if provided must be 8+ chars
    if (!password.isEmpty() && password.length() < 8) {
      return ConfigValidationResult::error(
          509, "WiFi password must be at least 8 characters", "wifi.password",
          "Use a password with 8 or more characters, or leave empty for open "
          "networks");
    }
  }

  // 5. Ethernet validation (when mode is ETH)
  if (isEthMode) {
    JsonObjectConst eth = cfg["ethernet"];
    if (!eth) {
      return ConfigValidationResult::error(
          509, "Ethernet configuration required when mode is 'ETH'", "ethernet",
          "Include ethernet object with enabled and network settings");
    }

    bool ethEnabled = eth["enabled"] | false;
    if (!ethEnabled) {
      return ConfigValidationResult::error(
          509, "Ethernet must be enabled when communication mode is 'ETH'",
          "ethernet.enabled", "Set ethernet.enabled to true");
    }

    bool useDhcp = eth["use_dhcp"] | true;
    if (!useDhcp) {
      // Static IP configuration required
      String staticIp = eth["static_ip"] | "";
      String gateway = eth["gateway"] | "";
      String subnet = eth["subnet"] | "";

      if (staticIp.isEmpty()) {
        return ConfigValidationResult::error(
            509, "Static IP is required when DHCP is disabled",
            "ethernet.static_ip",
            "Provide a valid IP address (e.g., 192.168.1.100)");
      }

      if (!isValidIPv4(staticIp)) {
        return ConfigValidationResult::error(
            509, "Invalid static IP format. Use xxx.xxx.xxx.xxx",
            "ethernet.static_ip",
            "Provide a valid IPv4 address (e.g., 192.168.1.100)");
      }

      if (gateway.isEmpty()) {
        return ConfigValidationResult::error(
            509, "Gateway IP is required when DHCP is disabled",
            "ethernet.gateway",
            "Provide a valid gateway IP (e.g., 192.168.1.1)");
      }

      if (!isValidIPv4(gateway)) {
        return ConfigValidationResult::error(
            509, "Invalid gateway IP format. Use xxx.xxx.xxx.xxx",
            "ethernet.gateway",
            "Provide a valid IPv4 address (e.g., 192.168.1.1)");
      }

      if (subnet.isEmpty()) {
        return ConfigValidationResult::error(
            509, "Subnet mask is required when DHCP is disabled",
            "ethernet.subnet",
            "Provide a valid subnet mask (e.g., 255.255.255.0)");
      }

      if (!isValidIPv4(subnet)) {
        return ConfigValidationResult::error(
            509, "Invalid subnet mask format. Use xxx.xxx.xxx.xxx",
            "ethernet.subnet",
            "Provide a valid subnet mask (e.g., 255.255.255.0)");
      }
    }
  }

  // 6. MQTT validation (when protocol is mqtt)
  if (isMqttProtocol) {
    JsonObjectConst mqtt = cfg["mqtt_config"];
    // mqtt_config is optional, but if enabled, must have valid broker
    if (mqtt) {
      bool mqttEnabled = mqtt["enabled"] | false;
      if (mqttEnabled) {
        String broker = mqtt["broker_address"] | "";
        if (broker.isEmpty()) {
          return ConfigValidationResult::error(
              509, "MQTT broker address is required when MQTT is enabled",
              "mqtt_config.broker_address",
              "Provide a valid broker hostname or IP address");
        }

        int port = mqtt["broker_port"] | 1883;
        if (port < 1 || port > 65535) {
          return ConfigValidationResult::error(
              509, "MQTT port must be between 1 and 65535",
              "mqtt_config.broker_port",
              "Use port 1883 for standard MQTT or 8883 for TLS");
        }

        int keepAlive = mqtt["keep_alive"] | 120;
        if (keepAlive < 30 || keepAlive > 3600) {
          return ConfigValidationResult::error(
              509, "MQTT keep_alive must be between 30 and 3600 seconds",
              "mqtt_config.keep_alive", "Recommended value is 120 seconds");
        }

        // Validate publish_mode if present
        // v1.0.6 FIX: Case-insensitive comparison
        String publishMode = mqtt["publish_mode"] | "default";
        if (!publishMode.equalsIgnoreCase("default") &&
            !publishMode.equalsIgnoreCase("customize")) {
          return ConfigValidationResult::error(
              509, "Invalid publish_mode. Must be 'default' or 'customize'",
              "mqtt_config.publish_mode",
              "Set publish_mode to 'default' or 'customize'");
        }

        // Validate interval_unit if present
        // v1.0.6 FIX: Case-insensitive comparison
        JsonObjectConst defaultMode = mqtt["default_mode"];
        if (defaultMode) {
          String intervalUnit = defaultMode["interval_unit"] | "s";
          if (!intervalUnit.equalsIgnoreCase("ms") &&
              !intervalUnit.equalsIgnoreCase("s") &&
              !intervalUnit.equalsIgnoreCase("m")) {
            return ConfigValidationResult::error(
                509, "Invalid interval_unit. Must be 'ms', 's', or 'm'",
                "mqtt_config.default_mode.interval_unit",
                "Use 'ms' for milliseconds, 's' for seconds, or 'm' for "
                "minutes");
          }

          int interval = defaultMode["interval"] | 5;
          if (interval < 1 || interval > 3600) {
            return ConfigValidationResult::error(
                509, "MQTT interval must be between 1 and 3600",
                "mqtt_config.default_mode.interval",
                "Set a reasonable data transmission interval");
          }
        }
      }
    }
  }

  // 7. HTTP validation (when protocol is http)
  if (isHttpProtocol) {
    JsonObjectConst http = cfg["http_config"];
    // http_config is optional, but if enabled, must have valid URL
    if (http) {
      bool httpEnabled = http["enabled"] | false;
      if (httpEnabled) {
        String url = http["endpoint_url"] | "";
        if (url.isEmpty()) {
          return ConfigValidationResult::error(
              509, "HTTP endpoint URL is required when HTTP is enabled",
              "http_config.endpoint_url",
              "Provide a valid URL starting with http:// or https://");
        }

        if (!isValidURL(url)) {
          return ConfigValidationResult::error(
              509, "HTTP endpoint must start with http:// or https://",
              "http_config.endpoint_url",
              "Provide a valid URL (e.g., https://api.example.com/data)");
        }

        // Validate HTTP method
        // v1.0.6 FIX: Case-insensitive comparison
        String method = http["method"] | "POST";
        if (!method.equalsIgnoreCase("GET") && !method.equalsIgnoreCase("POST") &&
            !method.equalsIgnoreCase("PUT") && !method.equalsIgnoreCase("PATCH") &&
            !method.equalsIgnoreCase("DELETE")) {
          return ConfigValidationResult::error(
              509,
              "Invalid HTTP method. Must be GET, POST, PUT, PATCH, or DELETE",
              "http_config.method",
              "Use a valid HTTP method (POST is recommended for telemetry)");
        }

        // Validate timeout
        int timeout = http["timeout"] | 5000;
        if (timeout < 1000 || timeout > 30000) {
          return ConfigValidationResult::error(
              509, "HTTP timeout must be between 1000 and 30000 milliseconds",
              "http_config.timeout",
              "Recommended timeout is 5000ms (5 seconds)");
        }

        // Validate retry count
        int retry = http["retry"] | 3;
        if (retry < 0 || retry > 10) {
          return ConfigValidationResult::error(
              509, "HTTP retry count must be between 0 and 10",
              "http_config.retry", "Recommended retry count is 3");
        }

        // Validate interval_unit if present
        // v1.0.6 FIX: Case-insensitive comparison
        String intervalUnit = http["interval_unit"] | "s";
        if (!intervalUnit.equalsIgnoreCase("ms") &&
            !intervalUnit.equalsIgnoreCase("s") &&
            !intervalUnit.equalsIgnoreCase("m")) {
          return ConfigValidationResult::error(
              509, "Invalid interval_unit. Must be 'ms', 's', or 'm'",
              "http_config.interval_unit",
              "Use 'ms' for milliseconds, 's' for seconds, or 'm' for minutes");
        }

        int interval = http["interval"] | 5;
        if (interval < 1 || interval > 3600) {
          return ConfigValidationResult::error(
              509, "HTTP interval must be between 1 and 3600",
              "http_config.interval",
              "Set a reasonable data transmission interval");
        }
      }
    }
  }

  // All validations passed
  LOG_CONFIG_INFO("[SERVER] Configuration validation passed");
  return ConfigValidationResult::success();
}

bool ServerConfig::getConfig(JsonObject& result) {
  // Copy all config fields
  for (JsonPair kv : config->as<JsonObject>()) {
    result[kv.key()] = kv.value();
  }

  // Defensive: Ensure critical MQTT fields have default values if missing/null
  if (!result["mqtt_config"]) {
    result["mqtt_config"].to<JsonObject>();
  }
  JsonObject mqtt = result["mqtt_config"];

  if (mqtt["enabled"].isNull()) mqtt["enabled"] = true;
  if (mqtt["broker_address"].isNull())
    mqtt["broker_address"] = "broker.hivemq.com";
  if (mqtt["broker_port"].isNull()) mqtt["broker_port"] = 1883;
  if (mqtt["client_id"].isNull()) mqtt["client_id"] = "";
  if (mqtt["username"].isNull()) mqtt["username"] = "";
  if (mqtt["password"].isNull()) mqtt["password"] = "";
  if (mqtt["topic_publish"].isNull())
    mqtt["topic_publish"] =
        "v1/devices/me/telemetry";  // Top level for mobile app
  if (mqtt["topic_subscribe"].isNull())
    mqtt["topic_subscribe"] = "";  // Top level for mobile app

  // FIXED: Increased default keep_alive from 60s to 120s
  if (mqtt["keep_alive"].isNull()) mqtt["keep_alive"] = 120;
  if (mqtt["clean_session"].isNull()) mqtt["clean_session"] = true;
  if (mqtt["use_tls"].isNull()) mqtt["use_tls"] = false;
  if (mqtt["publish_mode"].isNull()) mqtt["publish_mode"] = "default";

  // Ensure default_mode exists (for MQTT modes feature)
  if (!mqtt["default_mode"]) {
    JsonObject defaultMode = mqtt["default_mode"].to<JsonObject>();
    defaultMode["enabled"] = true;
    defaultMode["topic_publish"] = "v1/devices/me/telemetry";
    defaultMode["topic_subscribe"] = "";
    defaultMode["interval"] = 5;
    defaultMode["interval_unit"] = "s";
  }

  // Ensure customize_mode exists (for MQTT modes feature)
  if (!mqtt["customize_mode"]) {
    JsonObject customizeMode = mqtt["customize_mode"].to<JsonObject>();
    customizeMode["enabled"] = false;
    customizeMode["custom_topics"].to<JsonArray>();
  }

  // v1.1.0: Ensure subscribe_control exists (remote write via MQTT)
  if (!mqtt["subscribe_control"]) {
    JsonObject subscribeControl = mqtt["subscribe_control"].to<JsonObject>();
    subscribeControl["enabled"] = false;
    subscribeControl["topic_prefix"] = "";
    subscribeControl["response_enabled"] = true;
    subscribeControl["default_qos"] = 1;
  }

  // Defensive: Ensure communication config exists (mobile app structure)
  if (!result["communication"]) {
    result["communication"].to<JsonObject>();
  }
  JsonObject comm = result["communication"];
  if (comm["mode"].isNull())
    comm["mode"] = "ETH";  // Mobile app expects this field

  // WiFi at root level (mobile app structure)
  if (!result["wifi"]) {
    result["wifi"].to<JsonObject>();
  }
  JsonObject wifi = result["wifi"];
  if (wifi["enabled"].isNull()) wifi["enabled"] = true;
  if (wifi["ssid"].isNull()) wifi["ssid"] = "";
  if (wifi["password"].isNull()) wifi["password"] = "";

  // Ethernet at root level (mobile app structure)
  if (!result["ethernet"]) {
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
  if (!result["http_config"]) {
    result["http_config"].to<JsonObject>();
  }
  JsonObject http = result["http_config"];

  // Ensure individual fields exist
  if (http["enabled"].isNull()) http["enabled"] = false;
  if (http["endpoint_url"].isNull()) http["endpoint_url"] = "";
  if (http["method"].isNull())
    http["method"] = "POST";  // CRITICAL for mobile app!
  if (http["body_format"].isNull())
    http["body_format"] = "json";  // CRITICAL for mobile app!
  if (http["timeout"].isNull()) http["timeout"] = 5000;
  if (http["retry"].isNull()) http["retry"] = 3;
  if (http["interval"].isNull())
    http["interval"] = 5;  // v2.2.0: HTTP transmission interval
  if (http["interval_unit"].isNull())
    http["interval_unit"] = "s";  // v2.2.0: Interval unit
  if (!http["headers"]) {
    http["headers"].to<JsonObject>();
  }

  return true;
}

bool ServerConfig::updateConfig(JsonObjectConst newConfig) {
  // Create temporary config for validation
  JsonDocument tempConfig;
  tempConfig.set(newConfig);

  if (!validateConfig(tempConfig)) {
    return false;
  }

  // Update main config
  config->set(newConfig);

  // Sync topic_publish/topic_subscribe from top level to default_mode if not
  // present
  JsonObject mqtt = (*config)["mqtt_config"].as<JsonObject>();
  if (mqtt) {
    // If mobile app sends top-level topic fields but no default_mode, sync them
    if (!mqtt["default_mode"]) {
      mqtt["default_mode"].to<JsonObject>();
    }
    JsonObject defaultMode = mqtt["default_mode"];

    // Copy top-level topics to default_mode if they exist
    if (!mqtt["topic_publish"].isNull() &&
        defaultMode["topic_publish"].isNull()) {
      defaultMode["topic_publish"] = mqtt["topic_publish"];
    }
    if (!mqtt["topic_subscribe"].isNull() &&
        defaultMode["topic_subscribe"].isNull()) {
      defaultMode["topic_subscribe"] = mqtt["topic_subscribe"];
    }

    // Ensure default_mode has all required fields with defaults
    if (defaultMode["enabled"].isNull()) defaultMode["enabled"] = true;
    if (defaultMode["interval"].isNull()) defaultMode["interval"] = 5;
    if (defaultMode["interval_unit"].isNull())
      defaultMode["interval_unit"] = "s";

    // Ensure customize_mode exists
    if (!mqtt["customize_mode"]) {
      JsonObject customizeMode = mqtt["customize_mode"].to<JsonObject>();
      customizeMode["enabled"] = false;
      customizeMode["custom_topics"].to<JsonArray>();
    }

    // v1.1.0: Ensure subscribe_control exists
    if (!mqtt["subscribe_control"]) {
      JsonObject subscribeControl = mqtt["subscribe_control"].to<JsonObject>();
      subscribeControl["enabled"] = false;
      subscribeControl["topic_prefix"] = "";
      subscribeControl["response_enabled"] = true;
      subscribeControl["default_qos"] = 1;
    }
  }

  if (saveConfig()) {
    Serial.println("Server configuration updated successfully");

    // Check if restart should be suppressed (e.g., during factory reset)
    if (!suppressRestart) {
      scheduleDeviceRestart();
    } else {
      Serial.println(
          "[RESTART] Restart suppressed (factory reset in progress)");
    }

    return true;
  }
  return false;
}

bool ServerConfig::getCommunicationConfig(JsonObject& result) {
  if (config->as<JsonObject>()["communication"]) {
    JsonObject comm = (*config)["communication"];
    for (JsonPair kv : comm) {
      result[kv.key()] = kv.value();
    }
    return true;
  }
  return false;
}

String ServerConfig::getProtocol() { return (*config)["protocol"] | "mqtt"; }

bool ServerConfig::getMqttConfig(JsonObject& result) {
  if (config->as<JsonObject>()["mqtt_config"]) {
    JsonObject mqtt = (*config)["mqtt_config"];
    for (JsonPair kv : mqtt) {
      result[kv.key()] = kv.value();
    }
    return true;
  }
  return false;
}

bool ServerConfig::getHttpConfig(JsonObject& result) {
  if (config->as<JsonObject>()["http_config"]) {
    JsonObject http = (*config)["http_config"];
    for (JsonPair kv : http) {
      result[kv.key()] = kv.value();
    }
    return true;
  }
  return false;
}

bool ServerConfig::getWifiConfig(JsonObject& result) {
  if (config->as<JsonObject>()["communication"]) {
    JsonObject comm = (*config)["communication"];
    if (comm["wifi"]) {
      JsonObject wifi = comm["wifi"];
      for (JsonPair kv : wifi) {
        result[kv.key()] = kv.value();
      }
      return true;
    }
  }
  return false;
}

bool ServerConfig::getEthernetConfig(JsonObject& result) {
  if (config->as<JsonObject>()["communication"]) {
    JsonObject comm = (*config)["communication"];
    if (comm["ethernet"]) {
      JsonObject ethernet = comm["ethernet"];
      for (JsonPair kv : ethernet) {
        result[kv.key()] = kv.value();
      }
      return true;
    }
  }
  return false;
}

String ServerConfig::getPrimaryNetworkMode() {
  if (config->as<JsonObject>()["communication"]) {
    JsonObject comm = (*config)["communication"];
    return comm["primary_network_mode"] |
           "ETH";  // Default to ETH if not specified
  }
  return "ETH";  // Default if communication object is missing
}

String ServerConfig::getCommunicationMode() {
  if (config->as<JsonObject>()["communication"]) {
    JsonObject comm = (*config)["communication"];
    return comm["mode"] | "";  // Return old 'mode' field if present, else empty
  }
  return "";
}