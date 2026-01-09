#include "ConfigManager.h"

#include <esp_heap_caps.h>

#include <new>
#include <vector>

#include "AtomicFileOps.h"  //Atomic operations
#include "DebugConfig.h"    // MUST BE FIRST for LOG_* macros
#include "QueueManager.h"   // For flushDeviceData on delete

// #define DEBUG_CONFIG_MANAGER // Uncomment to enable detailed debug logs
// Uncomment untuk enable atomic write logging
// #define ATOMIC_OPS_DEBUG

const char* ConfigManager::DEVICES_FILE = "/devices.json";
const char* ConfigManager::REGISTERS_FILE = "/registers.json";

ConfigManager::ConfigManager()
    : devicesCache(nullptr),
      devicesShadowCache(nullptr),
      registersCache(nullptr),
      registersShadowCache(nullptr),
      devicesCacheValid(false),
      registersCacheValid(false),
      cacheMutex(nullptr),
      fileMutex(nullptr),
      atomicFileOps(nullptr) {
  // Create thread safety mutexes
  cacheMutex = xSemaphoreCreateMutex();
  fileMutex = xSemaphoreCreateMutex();

  // v2.5.34 FIX: Critical error - must have mutexes for thread safety
  if (!cacheMutex || !fileMutex) {
    LOG_CONFIG_INFO(
        "[CONFIG] CRITICAL: Failed to create synchronization mutexes - system "
        "unstable!");
    // Try to create missing mutexes one more time
    if (!cacheMutex) cacheMutex = xSemaphoreCreateMutex();
    if (!fileMutex) fileMutex = xSemaphoreCreateMutex();

    // If still failed, log fatal error (system will likely crash on first mutex
    // use)
    if (!cacheMutex || !fileMutex) {
      LOG_CONFIG_INFO(
          "[CONFIG] FATAL: Mutex creation failed after retry - expect "
          "crashes!");
    }
  }

  // Initialize atomic file operations
  atomicFileOps = new AtomicFileOps();
  if (!atomicFileOps) {
    LOG_CONFIG_INFO("[CONFIG] ERROR: Failed to allocate AtomicFileOps");
  }

  // Initialize PRIMARY cache in PSRAM
  devicesCache = (JsonDocument*)heap_caps_malloc(
      sizeof(JsonDocument), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (devicesCache) {
    new (devicesCache) JsonDocument();
  } else {
    devicesCache = new JsonDocument();
  }

  registersCache = (JsonDocument*)heap_caps_malloc(
      sizeof(JsonDocument), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (registersCache) {
    new (registersCache) JsonDocument();
  } else {
    registersCache = new JsonDocument();
  }

  // SHADOW COPY OPTIMIZATION (v2.3.8): Initialize SHADOW cache in PSRAM
  devicesShadowCache = (JsonDocument*)heap_caps_malloc(
      sizeof(JsonDocument), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (devicesShadowCache) {
    new (devicesShadowCache) JsonDocument();
    LOG_CONFIG_INFO("[CONFIG] Devices shadow cache allocated in PSRAM");
  } else {
    devicesShadowCache = new JsonDocument();  // Fallback to DRAM
    LOG_CONFIG_INFO("[CONFIG] WARNING: Devices shadow cache fallback to DRAM");
  }

  registersShadowCache = (JsonDocument*)heap_caps_malloc(
      sizeof(JsonDocument), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (registersShadowCache) {
    new (registersShadowCache) JsonDocument();
    LOG_CONFIG_INFO("[CONFIG] Registers shadow cache allocated in PSRAM");
  } else {
    registersShadowCache = new JsonDocument();  // Fallback to DRAM
    LOG_CONFIG_INFO(
        "[CONFIG] WARNING: Registers shadow cache fallback to DRAM");
  }
}

ConfigManager::~ConfigManager() {
  // Clean up atomic file operations first
  if (atomicFileOps) {
    delete atomicFileOps;
    atomicFileOps = nullptr;
  }

  // Clean up mutexes
  if (cacheMutex) {
    vSemaphoreDelete(cacheMutex);
    cacheMutex = nullptr;
  }
  if (fileMutex) {
    vSemaphoreDelete(fileMutex);
    fileMutex = nullptr;
  }

  // Clean up PRIMARY cache documents
  if (devicesCache) {
    devicesCache->~JsonDocument();
    heap_caps_free(devicesCache);
    devicesCache = nullptr;
  }
  if (registersCache) {
    registersCache->~JsonDocument();
    heap_caps_free(registersCache);
    registersCache = nullptr;
  }

  // SHADOW COPY OPTIMIZATION (v2.3.8): Clean up SHADOW cache documents
  if (devicesShadowCache) {
    devicesShadowCache->~JsonDocument();
    heap_caps_free(devicesShadowCache);
    devicesShadowCache = nullptr;
  }
  if (registersShadowCache) {
    registersShadowCache->~JsonDocument();
    heap_caps_free(registersShadowCache);
    registersShadowCache = nullptr;
  }
}

bool ConfigManager::begin() {
  if (!LittleFS.begin(true)) {
    // v2.5.35: Use DEV_MODE check to prevent log leak in production
    DEV_SERIAL_PRINTLN("LittleFS Mount Failed");
    return false;
  }

  // Initialize atomic file operations
  if (atomicFileOps) {
    if (!atomicFileOps->begin()) {
      LOG_CONFIG_INFO("[CONFIG] ERROR: Failed to initialize AtomicFileOps");
      return false;
    }
  } else {
    LOG_CONFIG_INFO("[CONFIG] ERROR: AtomicFileOps not allocated");
    return false;
  }

  if (!LittleFS.exists(DEVICES_FILE)) {
    JsonDocument doc;
    doc.to<JsonObject>();
    saveJson(DEVICES_FILE, doc);
    // v2.5.35: Use DEV_MODE check to prevent log leak in production
    DEV_SERIAL_PRINTLN("Created empty devices file");
  }
  if (!LittleFS.exists(REGISTERS_FILE)) {
    JsonDocument doc;
    doc.to<JsonObject>();
    saveJson(REGISTERS_FILE, doc);
    // v2.5.35: Use DEV_MODE check to prevent log leak in production
    DEV_SERIAL_PRINTLN("Created empty registers file");
  }

  // Initialize cache as invalid - will be loaded on first access
  devicesCacheValid = false;
  registersCacheValid = false;

  // Clear cache content
  devicesCache->clear();
  registersCache->clear();

  LOG_CONFIG_INFO(
      "[CONFIG] ConfigManager initialized with atomic write protection");
  return true;
}

String ConfigManager::generateId(const String& prefix) {
  return prefix + String(random(100000, 999999), HEX).substring(0, 6);
}

bool ConfigManager::saveJson(const String& filename, const JsonDocument& doc) {
  // v2.5.1 FIX: Use PSRAM buffer instead of String to prevent DRAM exhaustion
  // Previous code used Arduino String which allocates in DRAM
  // With 45+ registers (~20KB JSON), this caused DRAM to drop to critical
  // levels New approach: Allocate serialization buffer in PSRAM

  // Step 1: Calculate required buffer size
  size_t jsonSize = measureJson(doc);
  if (jsonSize == 0) {
    LOG_CONFIG_INFO("[CONFIG] ERROR: Empty JSON document for %s\n",
                    filename.c_str());
    return false;
  }

  // Step 2: Allocate buffer in PSRAM (with 10% margin for safety)
  size_t bufferSize = jsonSize + (jsonSize / 10) + 16;
  bool usedPsram =
      false;  // v2.5.34 FIX: Track allocation source to use correct free()
  char* psramBuffer =
      (char*)heap_caps_malloc(bufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  if (psramBuffer) {
    usedPsram = true;
  } else {
    // Fallback to DRAM only if PSRAM fails (shouldn't happen with 8MB PSRAM)
    LOG_CONFIG_INFO(
        "[CONFIG] WARNING: PSRAM alloc failed for %u bytes, using DRAM\n",
        bufferSize);
    psramBuffer = (char*)malloc(bufferSize);
    if (!psramBuffer) {
      LOG_CONFIG_INFO(
          "[CONFIG] ERROR: Failed to allocate %u bytes for JSON "
          "serialization\n",
          bufferSize);
      return false;
    }
  }

  // Step 3: Serialize JSON to PSRAM buffer
  size_t written = serializeJson(doc, psramBuffer, bufferSize);
  if (written == 0) {
    // v2.5.34 FIX: Use correct free based on allocation source
    if (usedPsram)
      heap_caps_free(psramBuffer);
    else
      free(psramBuffer);
    LOG_CONFIG_INFO("[CONFIG] ERROR: Failed to serialize JSON for %s\n",
                    filename.c_str());
    return false;
  }

  // Step 4: Acquire mutex with REDUCED timeout (2s instead of 5s)
  if (xSemaphoreTake(fileMutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
    // v2.5.34 FIX: Use correct free based on allocation source
    if (usedPsram)
      heap_caps_free(psramBuffer);
    else
      free(psramBuffer);
    LOG_CONFIG_INFO("[CONFIG] ERROR: saveJson(%s) - mutex timeout (2s)\n",
                    filename.c_str());
    return false;
  }

  // Step 5: Write to file
  bool success = false;

  if (atomicFileOps) {
    // AtomicFileOps has its own internal mutex (walMutex)
    xSemaphoreGive(fileMutex);

    // v2.5.1 FIX: Pass the already-serialized doc directly to atomicFileOps
    // Avoid re-deserialization which doubles memory usage
    success = atomicFileOps->writeAtomic(filename, doc);
  } else {
    // Fallback: direct write using PSRAM buffer
    LOG_CONFIG_INFO(
        "[CONFIG] WARNING: AtomicFileOps not available, using direct write");

    File file = LittleFS.open(filename, "w");
    if (!file) {
      xSemaphoreGive(fileMutex);
      // v2.5.34 FIX: Use correct free based on allocation source
      if (usedPsram)
        heap_caps_free(psramBuffer);
      else
        free(psramBuffer);
      return false;
    }

    // Write from PSRAM buffer (no DRAM String involved!)
    file.write((const uint8_t*)psramBuffer, written);
    file.close();
    success = true;

    xSemaphoreGive(fileMutex);
  }

  // Step 6: Free buffer using correct deallocator
  // v2.5.34 FIX: Use correct free based on allocation source
  if (usedPsram)
    heap_caps_free(psramBuffer);
  else
    free(psramBuffer);

  return success;
}

bool ConfigManager::loadJson(const String& filename, JsonDocument& doc) {
  // Mutex protection for file I/O
  if (xSemaphoreTake(fileMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
    LOG_CONFIG_INFO("[CONFIG] ERROR: loadJson(%s) - mutex timeout\n",
                    filename.c_str());
    return false;
  }

  File file = LittleFS.open(filename, "r");
  if (!file) {
    xSemaphoreGive(fileMutex);
    return false;
  }

  size_t fileSize = file.size();

  // Phase 4: Adaptive PSRAM allocation based on document size
  char* buffer = nullptr;
  bool usePsram = shouldUsePsram(fileSize);  // Only use PSRAM for files > 1KB

  if (usePsram && hasEnoughPsramFor(fileSize + 1)) {
    // Allocate from PSRAM for large files
    buffer = (char*)heap_caps_malloc(fileSize + 1,
                                     MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buffer) {
      LOG_CONFIG_INFO("[CONFIG] Allocated %u bytes from PSRAM for %s\n",
                      fileSize, filename.c_str());
    }
  }

  // Fallback: if PSRAM allocation failed or file is small, use DRAM or stream
  // parsing
  if (!buffer && fileSize > 1024) {
    // Only log if we intended to use PSRAM but couldn't
    logMemoryStats("loadJson PSRAM fallback");
    LOG_CONFIG_INFO(
        "[CONFIG] WARNING: PSRAM allocation failed for %s (%u bytes), using "
        "stream parsing\n",
        filename.c_str(), fileSize);
  }

  DeserializationError error;

  if (buffer) {
    // Use PSRAM buffer
    file.readBytes(buffer, fileSize);
    buffer[fileSize] = '\0';
    error = deserializeJson(doc, buffer);
    heap_caps_free(buffer);
  } else {
    // Direct stream parsing (uses less memory during parsing)
    error = deserializeJson(doc, file);
  }

  file.close();
  xSemaphoreGive(fileMutex);

  if (error != DeserializationError::Ok) {
    LOG_CONFIG_INFO("[CONFIG] ERROR: JSON deserialization failed for %s: %s\n",
                    filename.c_str(), error.c_str());
    return false;
  }

  return true;
}

String ConfigManager::createDevice(JsonObjectConst config) {
  if (!loadDevicesCache()) return "";

  // Mutex protection for cache modification
  if (xSemaphoreTake(cacheMutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
    LOG_CONFIG_INFO("[CONFIG] ERROR: createDevice - mutex timeout");
    return "";
  }

  // v2.5.39 FIX: ALWAYS generate new device_id, ignore any device_id from
  // config This fixes bug where mobile app accidentally sends device_id causing
  // overwrite Previous "BUG #32 FIX" allowed device_id from config which caused
  // this issue
  String deviceId = generateId("D");

  // v2.5.39: Collision check - regenerate if ID already exists (extremely rare:
  // 1 in 16 million)
  int maxRetries = 5;
  while (devicesCache->as<JsonObject>()[deviceId] && maxRetries > 0) {
    LOG_CONFIG_INFO(
        "[CONFIG] WARNING: Generated ID %s already exists, regenerating...\n",
        deviceId.c_str());
    deviceId = generateId("D");
    maxRetries--;
  }

  if (maxRetries == 0) {
    LOG_CONFIG_INFO(
        "[CONFIG] ERROR: Failed to generate unique device_id after 5 "
        "attempts\n");
    xSemaphoreGive(cacheMutex);
    return "";
  }

  LOG_CONFIG_INFO("[CONFIG] Generated new device_id: %s\n", deviceId.c_str());

  JsonObject device = (*devicesCache)[deviceId].to<JsonObject>();

  // Copy config with proper type conversion
  for (JsonPairConst kv : config) {
    String key = kv.key().c_str();
    if (key == "slave_id" || key == "port" || key == "timeout" ||
        key == "retry_count" || key == "refresh_rate_ms" ||
        key == "baud_rate" || key == "data_bits" || key == "stop_bits" ||
        key == "serial_port") {
      // Convert string numbers to integers
      int value = kv.value().is<String>() ? kv.value().as<String>().toInt()
                                          : kv.value().as<int>();
      device[kv.key()] = value;
    } else {
      device[kv.key()] = kv.value();
    }
  }

  // BUG #32 FIX: Ensure device_id is set (already correct from above logic)
  device["device_id"] = deviceId;

  // BUG #32 FIX: Initialize registers array if not present (for new device
  // creation) For restore operations, registers are already copied in the loop
  // above
  int registerCount = 0;
  JsonVariant registersVariant = device["registers"];
  if (registersVariant.isNull() || !registersVariant.is<JsonArray>()) {
    // New device creation - initialize empty registers array
    device["registers"].to<JsonArray>();
    registerCount = 0;
    LOG_CONFIG_INFO("[CONFIG] Created device %s with empty registers array\n",
                    deviceId.c_str());
  } else {
    // Restore operation - registers already copied
    registerCount = device["registers"].as<JsonArray>().size();
    LOG_CONFIG_INFO(
        "[CONFIG] Created device %s with %d registers (from restore)\n",
        deviceId.c_str(), registerCount);
  }

  // Save to file and keep cache valid
  if (saveJson(DEVICES_FILE, *devicesCache)) {
    // SHADOW COPY OPTIMIZATION (v2.3.8): Update shadow copy after successful
    // write
    updateDevicesShadowCopy();

    // v2.5.2: Success log only in development mode
    if (!IS_PRODUCTION_MODE()) {
      Serial.printf("Device %s created and cache updated (shadow synced)\n",
                    deviceId.c_str());
    }
    lastDevicesCacheTime = millis();  // Update TTL timestamp
    xSemaphoreGive(cacheMutex);
    return deviceId;
  }

  xSemaphoreGive(cacheMutex);
  invalidateDevicesCache();
  return "";
}

bool ConfigManager::readDevice(const String& deviceId, JsonObject& result,
                               bool minimal) {
  // SHADOW COPY OPTIMIZATION (v2.3.8): Use shadow copy for reads (lock-free)
  // Only take mutex briefly to check validity and copy pointer
  if (xSemaphoreTake(cacheMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    LOG_CONFIG_INFO(
        "[CONFIG] ERROR: readDevice - mutex timeout (fallback to cache load)");
    // Fallback to old behavior if mutex timeout
    if (!loadDevicesCache()) {
      Serial.println("Failed to load devices cache for readDevice");
      return false;
    }
  }

  // Quick validity check (no file I/O inside mutex!)
  if (!devicesCacheValid) {
    xSemaphoreGive(cacheMutex);
    // Reload cache if invalid
    if (!loadDevicesCache()) {
      Serial.println("Failed to load devices cache for readDevice");
      return false;
    }
    // Re-acquire mutex after reload
    if (xSemaphoreTake(cacheMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
      LOG_CONFIG_INFO(
          "[CONFIG] ERROR: readDevice - mutex timeout after reload");
      return false;
    }
  }

#ifdef DEBUG_CONFIG_MANAGER
  Serial.printf("[DEBUG] Looking for device ID: '%s'\n", deviceId.c_str());
  Serial.printf("[DEBUG] Device ID length: %d\n", deviceId.length());
#endif

  // Read from SHADOW copy (inside brief mutex lock)
  if (devicesShadowCache->as<JsonObject>()[deviceId]) {
    JsonObject device = (*devicesShadowCache)[deviceId];
    int registerCount = 0;

    for (JsonPair kv : device) {
      // If minimal mode, skip registers array and count them instead
      if (minimal && kv.key() == "registers") {
        if (kv.value().is<JsonArray>()) {
          registerCount = kv.value().as<JsonArray>().size();
        }
        continue;  // Skip adding registers array to result
      }

      result[kv.key()] = kv.value();
    }

    // Add register count in minimal mode
    if (minimal) {
      result["register_count"] = registerCount;
      LOG_CONFIG_INFO(
          "[CONFIG] Device %s read in MINIMAL mode (register_count=%d) from "
          "SHADOW\n",
          deviceId.c_str(), registerCount);
    }

    // Release mutex ASAP (no file I/O was done!)
    xSemaphoreGive(cacheMutex);

#ifdef DEBUG_CONFIG_MANAGER
    Serial.printf("Device %s read from SHADOW cache\n", deviceId.c_str());
#endif
    return true;
  }

  xSemaphoreGive(cacheMutex);

#ifdef DEBUG_CONFIG_MANAGER
  // Debug: Show all available keys
  Serial.printf("Device %s not found in cache. Available devices:\n",
                deviceId.c_str());
  for (JsonPair kv : devicesCache->as<JsonObject>()) {
    Serial.printf("  - '%s' (length: %d)\n", kv.key().c_str(),
                  String(kv.key().c_str()).length());
  }
#endif
  return false;
}

bool ConfigManager::updateDevice(const String& deviceId,
                                 JsonObjectConst config) {
  if (!loadDevicesCache()) return false;

  if (!devicesCache->as<JsonObject>()[deviceId]) {
    Serial.printf("Device %s not found for update\n", deviceId.c_str());
    return false;
  }

  JsonObject device = (*devicesCache)[deviceId];

  // Update device configuration while preserving device_id and registers
  JsonArray existingRegisters = device["registers"];

  // Update all config fields with proper type conversion
  for (JsonPairConst kv : config) {
    String key = kv.key().c_str();
    if (key == "slave_id" || key == "port" || key == "timeout" ||
        key == "retry_count" || key == "refresh_rate_ms" ||
        key == "baud_rate" || key == "data_bits" || key == "stop_bits" ||
        key == "serial_port") {
      // Convert string numbers to integers
      int value = kv.value().is<String>() ? kv.value().as<String>().toInt()
                                          : kv.value().as<int>();
      device[kv.key()] = value;
    } else {
      device[kv.key()] = kv.value();
    }
  }

  // Ensure device_id and registers are preserved
  device["device_id"] = deviceId;
  if (!device["registers"]) {
    device["registers"] = existingRegisters;
  }

  // Save to file and keep cache valid
  if (saveJson(DEVICES_FILE, *devicesCache)) {
    // SHADOW COPY OPTIMIZATION (v2.3.8): Update shadow copy after successful
    // write
    updateDevicesShadowCopy();

    Serial.printf("Device %s updated successfully (shadow synced)\n",
                  deviceId.c_str());
    return true;
  }

  invalidateDevicesCache();
  return false;
}

bool ConfigManager::deleteDevice(const String& deviceId) {
  if (!loadDevicesCache()) return false;

  if (devicesCache->as<JsonObject>()[deviceId]) {
    devicesCache->remove(deviceId);
    if (saveJson(DEVICES_FILE, *devicesCache)) {
      // SHADOW COPY OPTIMIZATION (v2.3.8): Update shadow copy after successful
      // delete
      updateDevicesShadowCopy();

      // Priority 1: Flush queue data for deleted device to prevent orphaned
      // data
      QueueManager* queueMgr = QueueManager::getInstance();
      if (queueMgr) {
        int flushedCount = queueMgr->flushDeviceData(deviceId);
        if (flushedCount > 0) {
          LOG_CONFIG_INFO(
              "[CONFIG] Flushed %d orphaned queue entries for device: %s\n",
              flushedCount, deviceId.c_str());
        }
      }

      Serial.printf("Device %s deleted (shadow synced)\n", deviceId.c_str());
      invalidateDevicesCache();
      return true;
    }
    invalidateDevicesCache();
  }
  return false;
}

void ConfigManager::listDevices(JsonArray& devices) {
  if (!loadDevicesCache()) {
    Serial.println("Failed to load devices cache for listDevices");
    return;
  }

  JsonObject devicesObj = devicesCache->as<JsonObject>();
  int count = 0;

#ifdef DEBUG_CONFIG_MANAGER
  Serial.printf("[DEBUG] Cache size: %d devices\n", devicesObj.size());
#endif

  for (JsonPair kv : devicesObj) {
    const char* keyPtr = kv.key().c_str();
    String deviceId = String(keyPtr);

#ifdef DEBUG_CONFIG_MANAGER
    Serial.printf("[DEBUG] Raw key: '%s', String ID: '%s' (len: %d)\n", keyPtr,
                  deviceId.c_str(), deviceId.length());
#endif

    // Validate device ID before adding
    if (deviceId.length() > 0 && deviceId != "{}" &&
        deviceId.indexOf('{') == -1) {
      devices.add(deviceId);
      count++;
#ifdef DEBUG_CONFIG_MANAGER
      Serial.printf("[DEBUG] Added valid device ID: '%s'\n", deviceId.c_str());
#endif
    } else {
#ifdef DEBUG_CONFIG_MANAGER
      Serial.printf("[DEBUG] Skipped invalid device ID: '%s'\n",
                    deviceId.c_str());
#endif
    }
  }
#ifdef DEBUG_CONFIG_MANAGER
  Serial.printf("Listed %d devices from cache\n", count);
#endif
}

void ConfigManager::getDevicesSummary(JsonArray& summary) {
  JsonDocument devices;
  if (!loadJson(DEVICES_FILE, devices)) return;

  for (JsonPair kv : devices.as<JsonObject>()) {
    JsonObject device = kv.value();
    JsonObject deviceSummary = summary.add<JsonObject>();

    deviceSummary["device_id"] = kv.key();
    deviceSummary["device_name"] = device["device_name"];
    deviceSummary["protocol"] = device["protocol"];
    deviceSummary["slave_id"] = device["slave_id"];
    deviceSummary["register_count"] = device["registers"].size();

    // v1.0.2: Include connection identifiers for proper slave_id validation
    // Mobile app needs these to check uniqueness per bus/endpoint (not global)
    // - RTU: slave_id must be unique per serial_port (RS-485 bus)
    // - TCP: slave_id must be unique per ip (endpoint)
    if (device["serial_port"].is<int>()) {
      deviceSummary["serial_port"] = device["serial_port"];
    }
    if (device["ip"].is<const char*>() || device["ip"].is<String>()) {
      deviceSummary["ip"] = device["ip"];
    }
  }
}

void ConfigManager::getAllDevicesWithRegisters(JsonArray& result,
                                               bool minimalFields) {
  LOG_CONFIG_INFO("[GET_ALL_DEVICES_WITH_REGISTERS] Starting (minimal=%s)...\n",
                  minimalFields ? "true" : "false");

  JsonDocument devices;
  if (!loadJson(DEVICES_FILE, devices)) {
    LOG_CONFIG_INFO(
        "[GET_ALL_DEVICES_WITH_REGISTERS] ERROR: Failed to load devices file");
    return;
  }

  // Check if devices object is empty
  JsonObject devicesObj = devices.as<JsonObject>();
  if (devicesObj.size() == 0) {
    LOG_CONFIG_INFO(
        "[GET_ALL_DEVICES_WITH_REGISTERS] Devices file is empty - no devices "
        "configured");
    return;
  }

  LOG_CONFIG_INFO("[GET_ALL_DEVICES_WITH_REGISTERS] Found %d devices in file\n",
                  devicesObj.size());

  for (JsonPair kv : devicesObj) {
    const char* deviceId = kv.key().c_str();  // BUG #31: const char* instead of
                                              // String (zero allocation!)
    JsonObject device = kv.value();
    JsonObject deviceWithRegs = result.add<JsonObject>();

    // Add device ID first
    deviceWithRegs["device_id"] = deviceId;

    // Copy ALL device fields (except registers, handle separately)
    for (JsonPair deviceKv : device) {
      String key = deviceKv.key().c_str();

      if (key == "registers") {
        // Skip registers, will be handled separately below
        continue;
      }

      if (minimalFields) {
        // In minimal mode, only copy essential fields for MQTT timeout
        // calculation CRITICAL FIX: Must include refresh_rate_ms and baud_rate
        // for adaptive timeout v1.0.2: Added "ip" for proper slave_id
        // validation in mobile app Mobile app needs ip (TCP) and serial_port
        // (RTU) to validate slave_id uniqueness per bus/endpoint
        if (key == "device_name" || key == "protocol" ||
            key == "refresh_rate_ms" || key == "baud_rate" ||
            key == "slave_id" || key == "serial_port" || key == "ip") {
          deviceWithRegs[deviceKv.key()] = deviceKv.value();
        }
      } else {
        // In full mode, copy ALL fields (timeout, retry_count, enabled, ip,
        // serial_port, baud_rate, etc.)
        deviceWithRegs[deviceKv.key()] = deviceKv.value();
      }
    }

    // Add all registers
    JsonArray registers = deviceWithRegs["registers"].to<JsonArray>();

    if (device["registers"].is<JsonArray>()) {
      JsonArray deviceRegisters = device["registers"];

      if (deviceRegisters.size() == 0) {
        LOG_CONFIG_INFO(
            "[GET_ALL_DEVICES_WITH_REGISTERS] Device %s has empty registers "
            "array\n",
            deviceId);  // BUG #31: removed .c_str()
      }

      for (JsonObject reg : deviceRegisters) {
        JsonObject registerInfo = registers.add<JsonObject>();

        // Always include essential fields for MQTT customize mode
        registerInfo["register_id"] = reg["register_id"];
        registerInfo["register_name"] = reg["register_name"];

        if (!minimalFields) {
          // Include all fields for detailed view
          registerInfo["address"] = reg["address"];
          registerInfo["data_type"] = reg["data_type"];
          registerInfo["function_code"] = reg["function_code"];
          registerInfo["unit"] = reg["unit"];
          registerInfo["description"] = reg["description"];
          registerInfo["scale"] = reg["scale"];
          registerInfo["offset"] = reg["offset"];
          registerInfo["decimals"] = reg["decimals"] | -1;  // v1.0.7
          registerInfo["register_index"] = reg["register_index"];
        }
      }
    } else {
      LOG_CONFIG_INFO(
          "[GET_ALL_DEVICES_WITH_REGISTERS] Device %s has no registers array\n",
          deviceId);  // BUG #31: removed .c_str()
    }

    LOG_CONFIG_INFO(
        "[GET_ALL_DEVICES_WITH_REGISTERS] Added device %s with %d registers\n",
        deviceId, registers.size());  // BUG #31: removed .c_str()
  }

  LOG_CONFIG_INFO("[GET_ALL_DEVICES_WITH_REGISTERS] Total devices: %d\n",
                  result.size());
}

String ConfigManager::createRegister(const String& deviceId,
                                     JsonObjectConst config, String* errorMsg) {
  // Helper lambda to set error message
  auto setError = [&errorMsg](const String& msg) {
    if (errorMsg) *errorMsg = msg;
  };

  // v2.5.2: Concise logging only in development mode
  int address = config["address"].is<String>()
                    ? config["address"].as<String>().toInt()
                    : config["address"].as<int>();
  if (!IS_PRODUCTION_MODE()) {
    Serial.printf("[CREATE_REG] Device %s: %s (addr %d)\n", deviceId.c_str(),
                  config["register_name"].as<String>().c_str(), address);
  }

  if (!loadDevicesCache()) {
    if (!IS_PRODUCTION_MODE()) {
      Serial.println("[CREATE_REGISTER] Failed to load devices cache");
    }
    setError("Failed to load devices cache");
    return "";
  }

  if (!devicesCache->as<JsonObject>()[deviceId]) {
    if (!IS_PRODUCTION_MODE()) {
      Serial.printf("[CREATE_REGISTER] Device %s not found in cache\n",
                    deviceId.c_str());
    }
    setError("Device '" + deviceId + "' not found");
    return "";
  }

  // Validate required fields (use isNull() to check existence, not truthiness)
  if (config["address"].isNull() || config["register_name"].isNull()) {
    if (!IS_PRODUCTION_MODE()) {
      Serial.println(
          "[CREATE_REGISTER] Missing required register fields: address or "
          "register_name");
    }
    setError("Missing required fields: address or register_name");
    return "";
  }

  String registerId = generateId("R");
  JsonObject device = (*devicesCache)[deviceId];

  // Ensure registers array exists (use proper null check for ArduinoJson v7)
  JsonVariant registersVariant = device["registers"];
  if (registersVariant.isNull() || !registersVariant.is<JsonArray>()) {
    device["registers"].to<JsonArray>();
    if (!IS_PRODUCTION_MODE()) {
      Serial.println("Created registers array for device");
    }
  }

  JsonArray registers = device["registers"].as<JsonArray>();

  // Address already parsed at the beginning for logging
  if (address < 0) {
    if (!IS_PRODUCTION_MODE()) {
      Serial.printf("[CREATE_REG] Invalid address: %d\n", address);
    }
    setError("Invalid address: " + String(address));
    return "";
  }

  // Check for duplicate address
  for (JsonVariant regVar : registers) {
    JsonObject existingReg = regVar.as<JsonObject>();
    int existingAddress = existingReg["address"].is<String>()
                              ? existingReg["address"].as<String>().toInt()
                              : existingReg["address"].as<int>();

    if (existingAddress == address) {
      if (!IS_PRODUCTION_MODE()) {
        Serial.printf("Register address %d already exists in device %s\n",
                      address, deviceId.c_str());
      }
      setError("Register address " + String(address) +
               " already exists in device " + deviceId);
      return "";
    }
  }

  JsonObject newRegister = registers.add<JsonObject>();
  for (JsonPairConst kv : config) {
    String key = kv.key().c_str();
    if (key == "address") {
      // Always store address as integer
      newRegister[kv.key()] = address;
    } else if (key == "function_code") {
      // Convert string numbers to integers
      int value = kv.value().is<String>() ? kv.value().as<String>().toInt()
                                          : kv.value().as<int>();
      newRegister[kv.key()] = value;
    } else if (key == "scale" || key == "offset") {
      // Convert to float for calibration values
      float value = kv.value().is<String>() ? kv.value().as<String>().toFloat()
                                            : kv.value().as<float>();
      newRegister[kv.key()] = value;
    } else if (key == "decimals") {
      // v1.0.7: Convert and validate decimals (-1 to 6)
      int value = kv.value().is<String>() ? kv.value().as<String>().toInt()
                                          : kv.value().as<int>();
      // Clamp to valid range: -1 (auto) to 6 (max precision)
      if (value < -1) value = -1;
      if (value > 6) value = 6;
      newRegister[kv.key()] = value;
    } else if (key == "writable") {
      // v1.0.8: Boolean flag for write capability
      bool value = kv.value().is<String>()
                       ? (kv.value().as<String>() == "true" ||
                          kv.value().as<String>() == "1")
                       : kv.value().as<bool>();
      newRegister[kv.key()] = value;
    } else if (key == "min_value" || key == "max_value") {
      // v1.0.8: Float values for write validation bounds
      float value = kv.value().is<String>() ? kv.value().as<String>().toFloat()
                                            : kv.value().as<float>();
      newRegister[kv.key()] = value;
    } else if (key == "mqtt_subscribe") {
      // v1.1.0: MQTT Subscribe Control - nested object for per-register MQTT
      // control
      if (kv.value().is<JsonObjectConst>()) {
        JsonObject mqttSub = newRegister["mqtt_subscribe"].to<JsonObject>();
        JsonObjectConst srcObj = kv.value().as<JsonObjectConst>();
        // enabled: boolean (required)
        mqttSub["enabled"] = srcObj["enabled"] | false;
        // topic_suffix: string (optional, defaults to register_id)
        if (!srcObj["topic_suffix"].isNull()) {
          mqttSub["topic_suffix"] = srcObj["topic_suffix"].as<String>();
        }
        // qos: integer 0-2 (optional, defaults to 1)
        int qos = srcObj["qos"] | 1;
        if (qos < 0) qos = 0;
        if (qos > 2) qos = 2;
        mqttSub["qos"] = qos;
      }
    } else {
      newRegister[kv.key()] = kv.value();
    }
  }
  newRegister["register_id"] = registerId;

  // Auto-generate register_index based on position in array (1-based for
  // user-friendly UI)
  int registerIndex =
      registers.size();  // Size after add = current position (1-based)
  newRegister["register_index"] = registerIndex;

  // Set default calibration values if not provided
  if (newRegister["scale"].isNull()) {
    newRegister["scale"] = 1.0;
  }
  if (newRegister["offset"].isNull()) {
    newRegister["offset"] = 0.0;
  }
  if (newRegister["unit"].isNull()) {
    newRegister["unit"] = "";
  }
  // v1.0.7: Add default decimals value for display precision control
  // -1 = auto (no rounding), 0-6 = fixed decimal places
  if (newRegister["decimals"].isNull()) {
    newRegister["decimals"] = -1;
  }
  // v1.0.8: Add default writable value (false = read-only by default)
  if (newRegister["writable"].isNull()) {
    newRegister["writable"] = false;
  }
  // v1.0.8: min_value and max_value are optional, no defaults needed
  // If not set, validation is skipped in writeRegisterValue()

  // Save to file and keep cache valid
  if (saveJson(DEVICES_FILE, *devicesCache)) {
    // v2.5.40 FIX: Update shadow copy after register creation
    // Without this, Modbus services read stale data from shadow cache
    updateDevicesShadowCopy();

    // v2.5.2: Success log only in development mode
    if (!IS_PRODUCTION_MODE()) {
      Serial.printf("[CREATE_REG] OK: %s (ID: %s, idx: %d) shadow synced\n",
                    newRegister["register_name"].as<String>().c_str(),
                    registerId.c_str(), registerIndex);
    }
    return registerId;
  } else {
    // v2.5.2: Error log only in development mode
    if (!IS_PRODUCTION_MODE()) {
      Serial.println("[CREATE_REG] FAIL: Save error");
    }
    setError("Failed to save register to storage");
    invalidateDevicesCache();
  }
  return "";
}

bool ConfigManager::listRegisters(const String& deviceId,
                                  JsonArray& registers) {
  if (!loadDevicesCache()) {
    Serial.println("Failed to load devices cache for listRegisters");
    return false;
  }

  if (devicesCache->as<JsonObject>()[deviceId]) {
    JsonObject device = (*devicesCache)[deviceId];
    if (device["registers"]) {
      JsonArray deviceRegisters = device["registers"];
      Serial.printf("Device %s has %d registers in cache\n", deviceId.c_str(),
                    deviceRegisters.size());
      for (JsonVariant reg : deviceRegisters) {
        registers.add(reg);
      }
      return true;
    } else {
      Serial.printf("Device %s has no registers array\n", deviceId.c_str());
    }
  } else {
    Serial.printf("Device %s not found in cache\n", deviceId.c_str());
  }
  return false;
}

bool ConfigManager::getRegistersSummary(const String& deviceId,
                                        JsonArray& summary) {
  if (!loadDevicesCache()) {
    Serial.println("Failed to load devices cache for getRegistersSummary");
    return false;
  }

  if (devicesCache->as<JsonObject>()[deviceId]) {
    JsonObject device = (*devicesCache)[deviceId];
    if (device["registers"]) {
      JsonArray registers = device["registers"];
      for (JsonVariant reg : registers) {
        JsonObject regSummary = summary.add<JsonObject>();
        regSummary["register_id"] = reg["register_id"];
        regSummary["register_name"] = reg["register_name"];
        regSummary["address"] = reg["address"];
        regSummary["data_type"] = reg["data_type"];
        regSummary["description"] = reg["description"];
        regSummary["scale"] = reg["scale"] | 1.0;
        regSummary["offset"] = reg["offset"] | 0.0;
        regSummary["decimals"] = reg["decimals"] | -1;  // v1.0.7
        regSummary["unit"] = reg["unit"] | "";
        regSummary["writable"] = reg["writable"] | false;  // v1.0.8
        // v1.0.8: Include min/max if present (optional fields)
        if (!reg["min_value"].isNull()) {
          regSummary["min_value"] = reg["min_value"];
        }
        if (!reg["max_value"].isNull()) {
          regSummary["max_value"] = reg["max_value"];
        }
        // v1.1.0: Include mqtt_subscribe if present
        if (!reg["mqtt_subscribe"].isNull()) {
          JsonObject mqttSub = regSummary["mqtt_subscribe"].to<JsonObject>();
          mqttSub["enabled"] = reg["mqtt_subscribe"]["enabled"] | false;
          if (!reg["mqtt_subscribe"]["topic_suffix"].isNull()) {
            mqttSub["topic_suffix"] =
                reg["mqtt_subscribe"]["topic_suffix"].as<String>();
          }
          mqttSub["qos"] = reg["mqtt_subscribe"]["qos"] | 1;
        }
      }
      return true;
    }
  }
  return false;
}

bool ConfigManager::updateRegister(const String& deviceId,
                                   const String& registerId,
                                   JsonObjectConst config) {
  if (!loadDevicesCache()) return false;

  if (!devicesCache->as<JsonObject>()[deviceId]) {
    Serial.printf("Device %s not found for register update\n",
                  deviceId.c_str());
    return false;
  }

  JsonObject device = (*devicesCache)[deviceId];
  if (!device["registers"]) {
    Serial.printf("No registers found for device %s\n", deviceId.c_str());
    return false;
  }

  JsonArray registers = device["registers"];
  for (JsonVariant regVar : registers) {
    JsonObject reg = regVar.as<JsonObject>();
    if (reg["register_id"] == registerId) {
      // Check for duplicate address if address is being updated
      if (config["address"]) {
        int newAddress = config["address"].is<String>()
                             ? config["address"].as<String>().toInt()
                             : config["address"].as<int>();

        int currentAddress = reg["address"].is<String>()
                                 ? reg["address"].as<String>().toInt()
                                 : reg["address"].as<int>();

        if (newAddress != currentAddress) {
          for (JsonVariant otherRegVar : registers) {
            JsonObject otherReg = otherRegVar.as<JsonObject>();
            if (otherReg["register_id"] != registerId) {
              int otherAddress = otherReg["address"].is<String>()
                                     ? otherReg["address"].as<String>().toInt()
                                     : otherReg["address"].as<int>();

              if (otherAddress == newAddress) {
                Serial.printf("Address %d already exists in another register\n",
                              newAddress);
                return false;
              }
            }
          }
        }
      }

      // Update register configuration while preserving register_id
      for (JsonPairConst kv : config) {
        String key = kv.key().c_str();
        if (key == "address" || key == "function_code") {
          int value = kv.value().is<String>() ? kv.value().as<String>().toInt()
                                              : kv.value().as<int>();
          reg[kv.key()] = value;
        } else if (key == "scale" || key == "offset") {
          // Convert to float for calibration values
          float value = kv.value().is<String>()
                            ? kv.value().as<String>().toFloat()
                            : kv.value().as<float>();
          reg[kv.key()] = value;
        } else if (key == "decimals") {
          // v1.0.7: Convert and validate decimals (-1 to 6)
          int value = kv.value().is<String>() ? kv.value().as<String>().toInt()
                                              : kv.value().as<int>();
          // Clamp to valid range: -1 (auto) to 6 (max precision)
          if (value < -1) value = -1;
          if (value > 6) value = 6;
          reg[kv.key()] = value;
        } else if (key == "writable") {
          // v1.0.8: Boolean flag for write capability
          bool value = kv.value().is<String>()
                           ? (kv.value().as<String>() == "true" ||
                              kv.value().as<String>() == "1")
                           : kv.value().as<bool>();
          reg[kv.key()] = value;
        } else if (key == "min_value" || key == "max_value") {
          // v1.0.8: Float values for write validation bounds
          float value = kv.value().is<String>()
                            ? kv.value().as<String>().toFloat()
                            : kv.value().as<float>();
          reg[kv.key()] = value;
        } else if (key == "mqtt_subscribe") {
          // v1.1.0: MQTT Subscribe Control - nested object for per-register
          // MQTT control
          if (kv.value().is<JsonObjectConst>()) {
            // Remove existing mqtt_subscribe if present
            reg.remove("mqtt_subscribe");
            JsonObject mqttSub = reg["mqtt_subscribe"].to<JsonObject>();
            JsonObjectConst srcObj = kv.value().as<JsonObjectConst>();
            // enabled: boolean (required)
            mqttSub["enabled"] = srcObj["enabled"] | false;
            // topic_suffix: string (optional, defaults to register_id)
            if (!srcObj["topic_suffix"].isNull()) {
              mqttSub["topic_suffix"] = srcObj["topic_suffix"].as<String>();
            }
            // qos: integer 0-2 (optional, defaults to 1)
            int qos = srcObj["qos"] | 1;
            if (qos < 0) qos = 0;
            if (qos > 2) qos = 2;
            mqttSub["qos"] = qos;
          }
        } else {
          reg[kv.key()] = kv.value();
        }
      }
      reg["register_id"] = registerId;  // Ensure register_id is preserved

      // Save to file and keep cache valid
      if (saveJson(DEVICES_FILE, *devicesCache)) {
        // v2.5.40 FIX: Update shadow copy after register update
        // CRITICAL: Without this, calibration (offset/scale) changes are NOT
        // applied! Modbus services read from shadow cache, not directly from
        // file
        updateDevicesShadowCopy();

        Serial.printf("Register %s updated successfully (shadow synced)\n",
                      registerId.c_str());
        return true;
      }

      invalidateDevicesCache();
      return false;
    }
  }

  Serial.printf("Register %s not found in device %s\n", registerId.c_str(),
                deviceId.c_str());
  return false;
}

bool ConfigManager::deleteRegister(const String& deviceId,
                                   const String& registerId) {
  if (!loadDevicesCache()) {
    Serial.println("Failed to load devices cache for deleteRegister");
    return false;
  }

  if (!devicesCache->as<JsonObject>()[deviceId]) {
    Serial.printf("Device %s not found for register deletion\n",
                  deviceId.c_str());
    return false;
  }

  JsonObject device = (*devicesCache)[deviceId];
  if (!device["registers"]) {
    Serial.printf("No registers found for device %s\n", deviceId.c_str());
    return false;
  }

  JsonArray registers = device["registers"];
  for (int i = 0; i < registers.size(); i++) {
    if (registers[i]["register_id"] == registerId) {
      registers.remove(i);

      // Re-index remaining registers to maintain sequential order (1-based)
      for (int j = 0; j < registers.size(); j++) {
        registers[j]["register_index"] = j + 1;  // 1-based index
      }

      // Save to file and keep cache valid
      if (saveJson(DEVICES_FILE, *devicesCache)) {
        // v2.5.40 FIX: Update shadow copy after register deletion
        // Without this, Modbus services still see deleted register in shadow
        // cache
        updateDevicesShadowCopy();

        Serial.printf(
            "Register %s deleted successfully, re-indexed %d remaining "
            "registers (shadow synced)\n",
            registerId.c_str(), registers.size());
        return true;
      }

      invalidateDevicesCache();
      return false;
    }
  }

  Serial.printf("Register %s not found in device %s\n", registerId.c_str(),
                deviceId.c_str());
  return false;
}

bool ConfigManager::loadDevicesCache() {
  // Mutex protection for thread-safe cache access
  if (xSemaphoreTake(cacheMutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
    LOG_CONFIG_INFO("[CONFIG] ERROR: loadDevicesCache - mutex timeout");
    return false;
  }

  // FIXED BUG #16: Check cache TTL before returning cached data
  // Previous code only checked devicesCacheValid, ignoring TTL expiration
  if (devicesCacheValid) {
    // Check if cache has expired (TTL = 10 minutes)
    unsigned long now = millis();
    if ((now - lastDevicesCacheTime) >= CACHE_TTL_MS) {
      LOG_CONFIG_INFO(
          "[CACHE] Devices cache expired (%lu ms old, TTL: %lu ms). "
          "Reloading...\n",
          (now - lastDevicesCacheTime), CACHE_TTL_MS);
      devicesCacheValid = false;  // Invalidate and reload
    } else {
      // Cache still valid
      xSemaphoreGive(cacheMutex);
      return true;
    }
  }

  LOG_CONFIG_INFO("[CACHE] Loading devices cache from file...");
  logMemoryStats("before loadDevicesCache");  // Phase 4: Memory tracking

  // If the file doesn't exist, create an empty JSON object in the cache and
  // exit.
  if (!LittleFS.exists(DEVICES_FILE)) {
    Serial.println("Devices file not found. Initializing empty cache.");
    devicesCache->clear();
    devicesCache->to<JsonObject>();
    devicesCacheValid = true;
    lastDevicesCacheTime = millis();              // Update TTL timestamp
    logMemoryStats("after empty devices cache");  // Phase 4: Memory tracking
    xSemaphoreGive(cacheMutex);
    return true;
  }

  // Clear the cache before loading new data from the file.
  devicesCache->clear();

  // Attempt to load and parse the JSON from the file.
  if (loadJson(DEVICES_FILE, *devicesCache)) {
    // Migration: Auto-generate register_index for existing registers without it
    bool needsSave = false;
    JsonObject devices = devicesCache->as<JsonObject>();
    for (JsonPair devicePair : devices) {
      JsonObject device = devicePair.value();
      if (device["registers"]) {
        JsonArray registers = device["registers"];
        for (int i = 0; i < registers.size(); i++) {
          JsonObject reg = registers[i];

          // Migration 1: Auto-generate register_index
          if (reg["register_index"].isNull()) {
            reg["register_index"] = i + 1;  // Auto-generate 1-based index
            needsSave = true;
            Serial.printf(
                "[MIGRATION] Added register_index=%d to register %s\n", i + 1,
                reg["register_id"].as<String>().c_str());
          }

          // Migration 2: Add default calibration values if missing
          if (reg["scale"].isNull()) {
            reg["scale"] = 1.0;
            needsSave = true;
            Serial.printf("[MIGRATION] Added scale=1.0 to register %s\n",
                          reg["register_id"].as<String>().c_str());
          }
          if (reg["offset"].isNull()) {
            reg["offset"] = 0.0;
            needsSave = true;
            Serial.printf("[MIGRATION] Added offset=0.0 to register %s\n",
                          reg["register_id"].as<String>().c_str());
          }
          if (reg["unit"].isNull()) {
            reg["unit"] = "";
            needsSave = true;
            Serial.printf("[MIGRATION] Added unit=\"\" to register %s\n",
                          reg["register_id"].as<String>().c_str());
          }
          // v1.0.7 Migration: Add decimals field for display precision control
          // -1 = auto (no rounding, use full precision)
          if (reg["decimals"].isNull()) {
            reg["decimals"] = -1;
            needsSave = true;
            Serial.printf("[MIGRATION] Added decimals=-1 to register %s\n",
                          reg["register_id"].as<String>().c_str());
          }

          // Migration 3: Remove old refresh_rate_ms field (Opsi A - clean
          // migration)
          if (!reg["refresh_rate_ms"].isNull()) {
            reg.remove("refresh_rate_ms");
            needsSave = true;
            Serial.printf(
                "[MIGRATION] Removed refresh_rate_ms from register %s (now "
                "using device-level polling)\n",
                reg["register_id"].as<String>().c_str());
          }
        }
      }
    }

    // Save if migration was applied
    if (needsSave) {
      Serial.println(
          "[MIGRATION] Saving devices.json with calibration fields and removed "
          "refresh_rate_ms...");
      saveJson(DEVICES_FILE, *devicesCache);
    }

    devicesCacheValid = true;
    lastDevicesCacheTime = millis();  // Update TTL timestamp

    // SHADOW COPY OPTIMIZATION (v2.3.8): Update shadow copy for lock-free reads
    // Shadow copy is updated INSIDE mutex (safe), but reads can access it with
    // minimal locking
    updateDevicesShadowCopy();

    LOG_CONFIG_INFO(
        "[CACHE] Loaded successfully | Devices: %d (shadow copy updated)\n",
        devices.size());
    logMemoryStats(
        "after loadDevicesCache success");  // Phase 4: Memory tracking
    xSemaphoreGive(cacheMutex);
    return true;
  }

  // If parsing fails, log the error and create an empty cache to ensure stable
  // operation.
  Serial.println(
      "ERROR: Failed to parse devices.json. Initializing empty cache to "
      "prevent data corruption.");
  devicesCache->clear();
  devicesCache->to<JsonObject>();
  devicesCacheValid =
      true;  // Mark as valid to prevent repeated failed parsing attempts.
  lastDevicesCacheTime = millis();                   // Update TTL timestamp
  logMemoryStats("after loadDevicesCache failure");  // Phase 4: Memory tracking
  xSemaphoreGive(cacheMutex);
  return false;  // Indicate that loading failed.
}

bool ConfigManager::loadRegistersCache() {
  // Mutex protection for thread-safe cache access
  if (xSemaphoreTake(cacheMutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
    LOG_CONFIG_INFO("[CONFIG] ERROR: loadRegistersCache - mutex timeout");
    return false;
  }

  if (registersCacheValid) {
    xSemaphoreGive(cacheMutex);
    return true;
  }

  // Check if file exists
  if (!LittleFS.exists(REGISTERS_FILE)) {
    Serial.println("Registers file does not exist, creating empty cache");
    registersCache->clear();
    registersCache->to<JsonObject>();
    registersCacheValid = true;
    lastRegistersCacheTime = millis();  // Update TTL timestamp
    xSemaphoreGive(cacheMutex);
    return true;
  }

  registersCache->clear();

  if (loadJson(REGISTERS_FILE, *registersCache)) {
    registersCacheValid = true;
    lastRegistersCacheTime = millis();  // Update TTL timestamp
    xSemaphoreGive(cacheMutex);
    return true;
  }

  xSemaphoreGive(cacheMutex);
  return false;
}

void ConfigManager::invalidateDevicesCache() {
  // Mutex protection for atomic invalidation
  if (xSemaphoreTake(cacheMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
    devicesCacheValid = false;
    lastDevicesCacheTime = 0;  // Reset TTL timestamp

    // BUGFIX: Clear cache memory to prevent memory leak after device deletion
    // Without this, deleted device data remains in memory until next cache load
    if (devicesCache) {
      devicesCache->clear();
      LOG_CONFIG_INFO("[CACHE] Devices cache cleared to free memory");
    }

    xSemaphoreGive(cacheMutex);
  } else {
    LOG_CONFIG_INFO(
        "[CONFIG] WARNING: invalidateDevicesCache - mutex timeout, proceeding "
        "without lock");
    devicesCacheValid = false;
  }
}

void ConfigManager::invalidateRegistersCache() {
  // Mutex protection for atomic invalidation
  if (xSemaphoreTake(cacheMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
    registersCacheValid = false;
    lastRegistersCacheTime = 0;  // Reset TTL timestamp

    // BUGFIX: Clear cache memory to prevent memory leak after register deletion
    if (registersCache) {
      registersCache->clear();
      LOG_CONFIG_INFO("[CACHE] Registers cache cleared to free memory");
    }

    xSemaphoreGive(cacheMutex);
  } else {
    LOG_CONFIG_INFO(
        "[CONFIG] WARNING: invalidateRegistersCache - mutex timeout, "
        "proceeding without lock");
    registersCacheValid = false;
  }
}

// TTL expiry checker
bool ConfigManager::isCacheExpired(unsigned long lastUpdateTime) {
  unsigned long currentTime = millis();

  // Handle millis() overflow (happens every ~49 days)
  if (currentTime < lastUpdateTime) {
    // Overflow occurred - consider cache expired to be safe
    return true;
  }

  return (currentTime - lastUpdateTime) >= CACHE_TTL_MS;
}

// Phase 4 - PSRAM Capacity Helpers

size_t ConfigManager::getAvailablePsramSize() const {
  // Get free PSRAM heap size
  size_t availablePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  return availablePsram;
}

bool ConfigManager::shouldUsePsram(size_t dataSize) const {
  // Use PSRAM for:
  // 1. Documents > 1KB (to free up main RAM)
  // 2. When PSRAM has sufficient free space (maintain 10KB buffer)

  if (dataSize < 1024) {
    return false;  // Small documents stay in main RAM
  }

  size_t availablePsram = getAvailablePsramSize();
  size_t minimumBufferSize = 10240;  // Keep 10KB free in PSRAM

  return availablePsram > (dataSize + minimumBufferSize);
}

bool ConfigManager::hasEnoughPsramFor(size_t requiredSize) const {
  size_t availablePsram = getAvailablePsramSize();
  size_t minimumBufferSize = 10240;  // Keep 10KB free for system operations

  return availablePsram > (requiredSize + minimumBufferSize);
}

void ConfigManager::logMemoryStats(const String& context) const {
  // Log heap statistics for debugging
  size_t freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  size_t freeDram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  size_t totalPsram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);

  LOG_CONFIG_INFO(
      "[CONFIG] Memory [%s]: PSRAM %u/%u free (%.1f%%) | DRAM %u free\n",
      context.c_str(), freePsram, totalPsram,
      (totalPsram > 0) ? (100.0f * freePsram / totalPsram) : 0.0f, freeDram);
}

void ConfigManager::refreshCache() {
  LOG_CONFIG_INFO("[CACHE] Forcing cache refresh...");

  // Force invalidate
  devicesCacheValid = false;
  registersCacheValid = false;

  // Clear cache content
  devicesCache->clear();
  registersCache->clear();

  // Reload from files
  bool devicesLoaded = loadDevicesCache();
  bool registersLoaded = loadRegistersCache();

  LOG_CONFIG_INFO("[CACHE] Refresh complete - Devices: %s, Registers: %s\n",
                  devicesLoaded ? "OK" : "FAIL",
                  registersLoaded ? "OK" : "FAIL");
}

void ConfigManager::clearCache() {
  // v2.3.6 OPTIMIZATION: Clear caches without reload
  // Purpose: Free DRAM after restore operations
  // Unlike refreshCache(), this does NOT reload from files
  // Cache will be lazily reloaded on next access

  LOG_CONFIG_INFO("[CACHE] Clearing caches to free DRAM...");

  // Invalidate cache flags
  devicesCacheValid = false;
  registersCacheValid = false;

  // Free JsonDocument memory
  devicesCache->clear();
  registersCache->clear();

  LOG_CONFIG_INFO(
      "[CACHE] Devices and registers caches cleared (will reload on next "
      "access)");
}

void ConfigManager::debugDevicesFile() {
  Serial.println("\n[CONFIG] DEVICES FILE DEBUG");

  if (!LittleFS.exists(DEVICES_FILE)) {
    Serial.println("  Status: File does not exist");
    return;
  }

  File file = LittleFS.open(DEVICES_FILE, "r");
  if (!file) {
    Serial.println("  Status: Failed to open file");
    return;
  }

  Serial.printf("  File size: %d bytes\n", file.size());
  Serial.println("  Content:");
  Serial.println();

  while (file.available()) {
    Serial.write(file.read());
  }
  Serial.println("\n");

  file.close();
}

void ConfigManager::fixCorruptDeviceIds() {
  Serial.println("\n[CONFIG] FIXING CORRUPT DEVICE IDS");

  JsonDocument originalDoc;
  if (!loadJson(DEVICES_FILE, originalDoc)) {
    Serial.println("  Status: Failed to load devices file");
    return;
  }

  JsonDocument fixedDoc;
  JsonObject fixedDevices = fixedDoc.to<JsonObject>();

  bool foundCorruption = false;

  for (JsonPair kv : originalDoc.as<JsonObject>()) {
    const char* keyPtr = kv.key().c_str();
    String deviceId = String(keyPtr);

    // Check if device ID is corrupt
    if (deviceId.isEmpty() || deviceId == "{}" || deviceId.indexOf('{') != -1 ||
        deviceId.length() < 3) {
      Serial.printf("  Found corrupt device ID: '%s' - generating new ID\n",
                    deviceId.c_str());

      // Generate new device ID
      String newDeviceId = generateId("D");
      JsonObject deviceObj = kv.value().as<JsonObject>();
      deviceObj["device_id"] = newDeviceId;

      fixedDevices[newDeviceId] = deviceObj;
      foundCorruption = true;

      Serial.printf("  Replaced with new ID: %s\n", newDeviceId.c_str());
    } else {
      // Keep valid device ID
      fixedDevices[deviceId] = kv.value();
      Serial.printf("  Kept valid device ID: %s\n", deviceId.c_str());
    }
  }

  if (foundCorruption) {
    Serial.println("  Saving fixed devices file...");
    if (saveJson(DEVICES_FILE, fixedDoc)) {
      Serial.println("  Status: Fixed devices file saved successfully");
      // Force invalidate cache to reload fixed data
      invalidateDevicesCache();
      devicesCacheValid = false;
    } else {
      Serial.println("  Status: Failed to save fixed devices file");
    }
  } else {
    Serial.println("  Status: No corruption found in device IDs");
  }

  // Always invalidate cache after this operation
  invalidateDevicesCache();
  devicesCacheValid = false;
  Serial.println();
}

void ConfigManager::removeCorruptKeys() {
  Serial.println("\n[CONFIG] REMOVING CORRUPT KEYS");

  // Force load current cache
  devicesCacheValid = false;
  if (!loadDevicesCache()) {
    Serial.println("  Status: Failed to load cache for corrupt key removal");
    return;
  }

  JsonObject devicesObj = devicesCache->as<JsonObject>();

  // Find and remove corrupt keys
  std::vector<String> keysToRemove;

  for (JsonPair kv : devicesObj) {
    const char* keyPtr = kv.key().c_str();
    String deviceId = String(keyPtr);

    if (deviceId.isEmpty() || deviceId == "{}" || deviceId.indexOf('{') != -1 ||
        deviceId.length() < 3) {
      keysToRemove.push_back(deviceId);
      Serial.printf("  Marking corrupt key for removal: '%s'\n",
                    deviceId.c_str());
    }
  }

  // Remove corrupt keys
  for (const String& key : keysToRemove) {
    devicesCache->remove(key);
    Serial.printf("  Removed corrupt key: '%s'\n", key.c_str());
  }

  if (keysToRemove.size() > 0) {
    // Save cleaned cache
    if (saveJson(DEVICES_FILE, *devicesCache)) {
      Serial.printf("  Status: Removed %d corrupt keys and saved file\n",
                    keysToRemove.size());
    } else {
      Serial.println("  Status: Failed to save cleaned devices file");
    }
  } else {
    Serial.println("  Status: No corrupt keys found to remove");
  }
  Serial.println();
}

void ConfigManager::clearAllConfigurations() {
  Serial.println("Clearing all device and register configurations...");
  JsonDocument emptyDoc;
  emptyDoc.to<JsonObject>();
  saveJson(DEVICES_FILE, emptyDoc);
  saveJson(REGISTERS_FILE, emptyDoc);
  invalidateDevicesCache();
  invalidateRegistersCache();
  Serial.println("All configurations cleared");
}

// ============================================================================
// SHADOW COPY OPTIMIZATION (v2.3.8)
// ============================================================================

/**
 * Update devices shadow copy from primary cache
 * MUST be called inside cacheMutex lock!
 * This creates a read-only copy for lock-free runtime reads
 */
void ConfigManager::updateDevicesShadowCopy() {
  // CRITICAL: This function MUST be called while holding cacheMutex!
  // Caller is responsible for mutex management

  if (!devicesShadowCache || !devicesCache) {
    LOG_CONFIG_INFO("[CONFIG] ERROR: Shadow cache or primary cache is null");
    return;
  }

  // Clear shadow copy
  devicesShadowCache->clear();

  // Deep copy from primary cache to shadow cache
  // ArduinoJson v7 provides efficient set() method for deep copy
  devicesShadowCache->set(*devicesCache);

  LOG_CONFIG_INFO(
      "[CONFIG] Devices shadow copy updated (lock-free reads enabled)");
}

/**
 * Update registers shadow copy from primary cache
 * MUST be called inside cacheMutex lock!
 * This creates a read-only copy for lock-free runtime reads
 */
void ConfigManager::updateRegistersShadowCopy() {
  // CRITICAL: This function MUST be called while holding cacheMutex!
  // Caller is responsible for mutex management

  if (!registersShadowCache || !registersCache) {
    LOG_CONFIG_INFO("[CONFIG] ERROR: Shadow cache or primary cache is null");
    return;
  }

  // Clear shadow copy
  registersShadowCache->clear();

  // Deep copy from primary cache to shadow cache
  registersShadowCache->set(*registersCache);

  LOG_CONFIG_INFO(
      "[CONFIG] Registers shadow copy updated (lock-free reads enabled)");
}