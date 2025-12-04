/**
 * GatewayConfig.h - Gateway Identity Configuration
 *
 * v2.5.31: Multi-Gateway Support
 * - Unique BLE name generation from MAC address
 * - User-configurable friendly name
 * - Gateway identification for mobile apps
 *
 * Config file: /gateway_config.json
 */

#ifndef GATEWAY_CONFIG_H
#define GATEWAY_CONFIG_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "DebugConfig.h"
#include <esp_mac.h>

class GatewayConfig {
private:
    static GatewayConfig* instance;

    // Gateway identification
    String bleName;           // Auto-generated: "SURIOTA-XXXXXX" (from MAC)
    String friendlyName;      // User-configurable: "Panel Listrik Gedung A"
    String location;          // Optional: "Lt.1 Ruang Panel"
    uint8_t macAddress[6];    // BT MAC address
    String macString;         // MAC as string "AA:BB:CC:DD:EE:FF"

    static const char* CONFIG_FILE;

    GatewayConfig();

    // Generate BLE name from MAC address
    void generateBLEName();

public:
    static GatewayConfig* getInstance();

    // Initialization
    bool begin();

    // Load/Save config
    bool load();
    bool save();

    // Getters
    const char* getBLEName() const { return bleName.c_str(); }
    const char* getFriendlyName() const { return friendlyName.c_str(); }
    const char* getLocation() const { return location.c_str(); }
    const char* getMACString() const { return macString.c_str(); }
    const uint8_t* getMACAddress() const { return macAddress; }

    // Get last 6 hex chars of MAC (for unique ID)
    String getShortMAC() const;

    // Setters (save to config file)
    bool setFriendlyName(const String& name);
    bool setLocation(const String& loc);

    // Get full gateway info as JSON
    void getGatewayInfo(JsonDocument& doc) const;

    // Singleton - prevent copying
    GatewayConfig(const GatewayConfig&) = delete;
    GatewayConfig& operator=(const GatewayConfig&) = delete;
};

#endif // GATEWAY_CONFIG_H
