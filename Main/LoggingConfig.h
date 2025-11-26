#ifndef LOGGING_CONFIG_H
#define LOGGING_CONFIG_H

#include "JsonDocumentPSRAM.h" // BUG #31: MUST BE BEFORE ArduinoJson.h
#include <ArduinoJson.h>
#include <LittleFS.h>

class LoggingConfig
{
private:
  static const char *CONFIG_FILE;
  JsonDocument *config; // Pointer to JsonDocument (proper way)

  bool saveConfig();
  bool loadConfig();
  bool validateConfig(const JsonDocument &config);
  void createDefaultConfig();

public:
  LoggingConfig();
  ~LoggingConfig(); // Destructor - cleanup resources

  bool begin();

  // Configuration operations
  bool getConfig(JsonObject &result);
  bool updateConfig(JsonObjectConst newConfig);

  // Specific getters
  String getLoggingRetention();
  String getLoggingInterval();

  // Production mode management
  void setProductionMode(uint8_t mode);
  uint8_t getProductionMode();
  bool save(); // Public save method for external use
};

#endif