#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "JsonDocumentPSRAM.h"  // BUG #31: MUST BE BEFORE ArduinoJson.h
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "AtomicFileOps.h" // Atomic file operations

class ConfigManager
{
private:
  static const char *DEVICES_FILE;
  static const char *REGISTERS_FILE;

  // Thread safety primitives
  SemaphoreHandle_t cacheMutex;
  SemaphoreHandle_t fileMutex;

  // Cache for devices and registers
  JsonDocument *devicesCache;   // Changed from DynamicJsonDocument
  JsonDocument *registersCache; // Changed from DynamicJsonDocument
  bool devicesCacheValid;
  bool registersCacheValid;

  // Cache TTL tracking
  unsigned long lastDevicesCacheTime = 0;
  unsigned long lastRegistersCacheTime = 0;
  static const unsigned long CACHE_TTL_MS = 600000; // 10 minutes

  // Private helper for TTL check
  bool isCacheExpired(unsigned long lastUpdateTime);

  // PSRAM capacity helpers (Phase 4)
  size_t getAvailablePsramSize() const;
  bool shouldUsePsram(size_t dataSize) const;
  bool hasEnoughPsramFor(size_t requiredSize) const;
  void logMemoryStats(const String &context) const;

  String generateId(const String &prefix);
  bool saveJson(const String &filename, const JsonDocument &doc);
  bool loadJson(const String &filename, JsonDocument &doc);
  void invalidateDevicesCache();
  void invalidateRegistersCache();
  bool loadDevicesCache();
  bool loadRegistersCache();

  // Atomic file operations pointer
  AtomicFileOps *atomicFileOps = nullptr;

public:
  ConfigManager();
  ~ConfigManager();

  bool begin();

  // Device operations
  String createDevice(JsonObjectConst config);
  bool readDevice(const String &deviceId, JsonObject &result, bool minimal = false);
  bool updateDevice(const String &deviceId, JsonObjectConst config);
  bool deleteDevice(const String &deviceId);
  void listDevices(JsonArray &devices);
  void getDevicesSummary(JsonArray &summary);
  void getAllDevicesWithRegisters(JsonArray &result, bool minimalFields = false);  // New: Get all devices with their registers

  // Clear all configurations
  void clearAllConfigurations();

  // Cache management
  void refreshCache();
  void clearCache();  // v2.3.6: Clear caches without reload (for DRAM optimization)
  void debugDevicesFile();
  void fixCorruptDeviceIds();
  void removeCorruptKeys();

  // Register operations
  String createRegister(const String &deviceId, JsonObjectConst config);
  bool listRegisters(const String &deviceId, JsonArray &registers);
  bool getRegistersSummary(const String &deviceId, JsonArray &summary);
  bool updateRegister(const String &deviceId, const String &registerId, JsonObjectConst config);
  bool deleteRegister(const String &deviceId, const String &registerId);
};

#endif