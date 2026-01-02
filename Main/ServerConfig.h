#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H

#include "JsonDocumentPSRAM.h" // BUG #31: MUST BE BEFORE ArduinoJson.h
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <esp_heap_caps.h>

// v1.0.2: Validation result structure for detailed error reporting
struct ConfigValidationResult {
  bool isValid;
  int errorCode;
  String message;
  String field;       // Which field caused the error (e.g., "wifi.ssid")
  String suggestion;  // Helpful suggestion for the user

  ConfigValidationResult() : isValid(true), errorCode(500), message(""), field(""), suggestion("") {}

  ConfigValidationResult(bool valid, int code, const String& msg, const String& fld = "", const String& sug = "")
    : isValid(valid), errorCode(code), message(msg), field(fld), suggestion(sug) {}

  // Helper for success
  static ConfigValidationResult success() {
    return ConfigValidationResult(true, 500, "Validation passed");
  }

  // Helper for error
  static ConfigValidationResult error(int code, const String& msg, const String& field = "", const String& suggestion = "") {
    return ConfigValidationResult(false, code, msg, field, suggestion);
  }
};

class ServerConfig
{
private:
  static const char *CONFIG_FILE;
  JsonDocument *config; // Changed from DynamicJsonDocument
  bool suppressRestart; // Flag to suppress auto-restart (e.g., during factory reset)
  ConfigValidationResult lastValidationResult; // Store last validation result

  bool saveConfig();
  bool loadConfig();
  bool validateConfig(const JsonDocument &config);
  void createDefaultConfig();
  static void restartDeviceTask(void *parameter);
  void scheduleDeviceRestart();

  // v1.0.2: Enhanced validation helpers
  ConfigValidationResult validateConfigEnhanced(const JsonDocument &cfg);
  bool isValidIPv4(const String& ip);
  bool isValidURL(const String& url);

public:
  ServerConfig();
  ~ServerConfig();

  bool begin();

  // Configuration operations
  bool getConfig(JsonObject &result);
  bool updateConfig(JsonObjectConst newConfig);

  // v1.0.2: Get last validation result for detailed error reporting
  const ConfigValidationResult& getLastValidationResult() const { return lastValidationResult; }

  // Restart control
  void setSuppressRestart(bool suppress) { suppressRestart = suppress; }

  // Specific config getters
  bool getCommunicationConfig(JsonObject &result);
  String getProtocol();
  bool getMqttConfig(JsonObject &result);
  bool getHttpConfig(JsonObject &result);
  bool getWifiConfig(JsonObject &result);
  bool getEthernetConfig(JsonObject &result);
  String getPrimaryNetworkMode();
  String getCommunicationMode();
};

#endif