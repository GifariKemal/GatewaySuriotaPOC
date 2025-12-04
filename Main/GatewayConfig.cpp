/**
 * GatewayConfig.cpp - Gateway Identity Configuration Implementation
 *
 * v2.5.31: Multi-Gateway Support
 */

#include "GatewayConfig.h"
#include "MemoryManager.h"

// Static members
GatewayConfig* GatewayConfig::instance = nullptr;
const char* GatewayConfig::CONFIG_FILE = "/gateway_config.json";

GatewayConfig::GatewayConfig()
    : bleName("SURIOTA-000000"),
      friendlyName(""),
      location(""),
      macString("00:00:00:00:00:00")
{
    memset(macAddress, 0, sizeof(macAddress));
}

GatewayConfig* GatewayConfig::getInstance() {
    if (!instance) {
        instance = new GatewayConfig();
    }
    return instance;
}

bool GatewayConfig::begin() {
    // Get BT MAC address
    esp_err_t err = esp_read_mac(macAddress, ESP_MAC_BT);
    if (err != ESP_OK) {
        // Fallback to WiFi MAC if BT MAC fails
        err = esp_read_mac(macAddress, ESP_MAC_WIFI_STA);
        if (err != ESP_OK) {
            LOG_CONFIG_INFO("[GW] ERROR: Failed to read MAC address");
            return false;
        }
    }

    // Format MAC as string
    char macBuf[18];
    snprintf(macBuf, sizeof(macBuf), "%02X:%02X:%02X:%02X:%02X:%02X",
             macAddress[0], macAddress[1], macAddress[2],
             macAddress[3], macAddress[4], macAddress[5]);
    macString = String(macBuf);

    // Generate unique BLE name from MAC
    generateBLEName();

    // Load saved config (friendly_name, location)
    load();

    LOG_CONFIG_INFO("[GW] Gateway initialized - BLE: %s, MAC: %s",
                    bleName.c_str(), macString.c_str());

    if (friendlyName.length() > 0) {
        LOG_CONFIG_INFO("[GW] Friendly name: %s", friendlyName.c_str());
    }

    return true;
}

void GatewayConfig::generateBLEName() {
    // Format: "SURIOTA-XXXXXX" where XXXXXX = last 3 bytes of MAC (6 hex chars)
    char nameBuf[20];
    snprintf(nameBuf, sizeof(nameBuf), "SURIOTA-%02X%02X%02X",
             macAddress[3], macAddress[4], macAddress[5]);
    bleName = String(nameBuf);
}

String GatewayConfig::getShortMAC() const {
    char shortMac[7];
    snprintf(shortMac, sizeof(shortMac), "%02X%02X%02X",
             macAddress[3], macAddress[4], macAddress[5]);
    return String(shortMac);
}

bool GatewayConfig::load() {
    if (!LittleFS.exists(CONFIG_FILE)) {
        LOG_CONFIG_INFO("[GW] Config file not found, using defaults");
        return true; // Not an error - will use defaults
    }

    File file = LittleFS.open(CONFIG_FILE, "r");
    if (!file) {
        LOG_CONFIG_INFO("[GW] ERROR: Failed to open config file");
        return false;
    }

    auto doc = make_psram_unique<JsonDocument>();
    DeserializationError error = deserializeJson(*doc, file);
    file.close();

    if (error) {
        LOG_CONFIG_INFO("[GW] ERROR: Failed to parse config: %s", error.c_str());
        return false;
    }

    // Load saved values
    friendlyName = (*doc)["friendly_name"] | "";
    location = (*doc)["location"] | "";

    LOG_CONFIG_INFO("[GW] Config loaded from file");
    return true;
}

bool GatewayConfig::save() {
    auto doc = make_psram_unique<JsonDocument>();

    (*doc)["friendly_name"] = friendlyName;
    (*doc)["location"] = location;
    // Note: BLE name and MAC are auto-generated, not saved

    // Atomic write pattern (WAL)
    String walFile = String(CONFIG_FILE) + ".wal";

    File file = LittleFS.open(walFile, "w");
    if (!file) {
        LOG_CONFIG_INFO("[GW] ERROR: Failed to create WAL file");
        return false;
    }

    if (serializeJson(*doc, file) == 0) {
        file.close();
        LittleFS.remove(walFile);
        LOG_CONFIG_INFO("[GW] ERROR: Failed to write config");
        return false;
    }
    file.close();

    // Atomic rename
    if (LittleFS.exists(CONFIG_FILE)) {
        LittleFS.remove(CONFIG_FILE);
    }

    if (!LittleFS.rename(walFile, CONFIG_FILE)) {
        LOG_CONFIG_INFO("[GW] ERROR: Failed to rename WAL file");
        return false;
    }

    LOG_CONFIG_INFO("[GW] Config saved successfully");
    return true;
}

bool GatewayConfig::setFriendlyName(const String& name) {
    // Validate name length (max 32 chars)
    if (name.length() > 32) {
        LOG_CONFIG_INFO("[GW] ERROR: Friendly name too long (max 32 chars)");
        return false;
    }

    friendlyName = name;
    return save();
}

bool GatewayConfig::setLocation(const String& loc) {
    // Validate location length (max 64 chars)
    if (loc.length() > 64) {
        LOG_CONFIG_INFO("[GW] ERROR: Location too long (max 64 chars)");
        return false;
    }

    location = loc;
    return save();
}

void GatewayConfig::getGatewayInfo(JsonDocument& doc) const {
    doc["ble_name"] = bleName;
    doc["mac"] = macString;
    doc["short_mac"] = getShortMAC();
    doc["friendly_name"] = friendlyName;
    doc["location"] = location;

    // Add firmware info (extern from Main.ino)
    extern const char* FIRMWARE_VERSION_STR;
    extern const char* DEVICE_ID_STR;

    doc["firmware"] = FIRMWARE_VERSION_STR;
    doc["model"] = DEVICE_ID_STR;
}
