/**
 * GatewayConfig.h - Gateway Identity Configuration
 *
 * v2.5.32: Centralized Product Configuration
 * - Uses ProductConfig.h for all identity settings
 * - BLE name format: MGate-1210(P)-XXXX or MGate-1210-XXXX
 * - User-configurable friendly name and location
 * - Gateway identification for mobile apps
 *
 * Config file: /gateway_config.json
 */

#ifndef GATEWAY_CONFIG_H
#define GATEWAY_CONFIG_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <esp_mac.h>

#include "DebugConfig.h"
#include "ProductConfig.h"

class GatewayConfig {
 private:
  static GatewayConfig* instance;

  // Gateway identification
  String bleName;  // Auto-generated from ProductConfig: "MGate-1210(P)-A716"
  String friendlyName;    // User-configurable: "Panel Listrik Gedung A"
  String location;        // Optional: "Lt.1 Ruang Panel"
  String serialNumber;    // Auto-generated: "SRT-MGATE1210P-20251205-3AC9A7"
  uint8_t macAddress[6];  // BT MAC address
  String macString;       // MAC as string "AA:BB:CC:DD:EE:FF"

  static const char* CONFIG_FILE;

  GatewayConfig();

  // Generate BLE name from MAC address using ProductConfig
  void generateBLEName();

  // Generate serial number from MAC address
  void generateSerialNumber();

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
  const char* getSerialNumber() const { return serialNumber.c_str(); }
  const char* getMACString() const { return macString.c_str(); }
  const uint8_t* getMACAddress() const { return macAddress; }

  // Get UID part of BLE name (last 4 hex chars from MAC)
  String getUID() const;

  // Get last 6 hex chars of MAC (for compatibility)
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

#endif  // GATEWAY_CONFIG_H
