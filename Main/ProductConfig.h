/**
 * @file ProductConfig.h
 * @brief Centralized Product Identity Configuration
 * @version 2.5.34
 * @date 2025-12-06
 *
 * ============================================================================
 * PRODUCT CONFIGURATION - EDIT HERE FOR ALL IDENTITY SETTINGS
 * ============================================================================
 *
 * This file contains all product identity settings in one place:
 * - Firmware Version
 * - Product Model & Variant
 * - BLE Name Format
 * - Serial Number Format
 * - Hardware Configuration
 *
 * USAGE:
 *   #include "ProductConfig.h"
 *
 *   Serial.println(FIRMWARE_VERSION);      // "2.5.32"
 *   Serial.println(PRODUCT_MODEL);         // "SRT-MGATE-1210"
 *   Serial.println(PRODUCT_VARIANT);       // "P" or ""
 *   Serial.println(PRODUCT_FULL_MODEL);    // "MGate-1210(P)" or "MGate-1210"
 *
 * ============================================================================
 */

#ifndef PRODUCT_CONFIG_H
#define PRODUCT_CONFIG_H

// ============================================================================
// FIRMWARE VERSION
// ============================================================================
// Format: MAJOR.MINOR.PATCH
// Update this for every release

#define FIRMWARE_VERSION        "2.5.35"
#define FIRMWARE_VERSION_MAJOR  2
#define FIRMWARE_VERSION_MINOR  5
#define FIRMWARE_VERSION_PATCH  35

// Build number: Derived from version (2.5.35 = 2535)
// Used for OTA version comparison
#define FIRMWARE_BUILD_NUMBER   2535

// ============================================================================
// PRODUCT MODEL & VARIANT
// ============================================================================
// Model: Base product identifier
// Variant: Hardware variant (P = POE, empty = Non-POE)

#define PRODUCT_MODEL           "SRT-MGATE-1210"
#define PRODUCT_MODEL_SHORT     "MGate-1210"

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  VARIANT SELECTION - UNCOMMENT ONE OPTION                               │
// └─────────────────────────────────────────────────────────────────────────┘

// Option 1: POE Variant (MGate-1210(P))
#define PRODUCT_VARIANT         "P"
#define PRODUCT_IS_POE          1

// Option 2: Non-POE Variant (MGate-1210) - Uncomment below and comment above
// #define PRODUCT_VARIANT      ""
// #define PRODUCT_IS_POE       0

// ============================================================================
// BLE NAME FORMAT
// ============================================================================
// Format: MGate-1210(P)XXXX or MGate-1210XXXX
// Where XXXX is derived from MAC address (last 2 bytes = 4 hex chars)
//
// Examples:
//   POE:     MGate-1210(P)A716
//   Non-POE: MGate-1210C726

#define BLE_NAME_PREFIX         "MGate-1210"

// UID length from MAC address (4 = last 2 bytes as hex)
#define BLE_UID_LENGTH          4

// Maximum BLE name length (must be <= 29 for BLE spec)
#define BLE_NAME_MAX_LENGTH     24

// ============================================================================
// SERIAL NUMBER FORMAT
// ============================================================================
// Format: SRT-MGATE1210P-YYYYMMDD-XXXXXX (18+ digits)
// Where:
//   SRT = Company prefix
//   MGATE1210P = Model without dashes + variant
//   YYYYMMDD = Manufacturing date (or first boot date)
//   XXXXXX = Unique ID from MAC

#define SERIAL_PREFIX           "SRT"
#define SERIAL_MODEL_CODE       "MGATE1210"

// ============================================================================
// HARDWARE CONFIGURATION
// ============================================================================

// RS485 Ports
#define RS485_PORT_COUNT        2
#define RS485_PORT1_RX          16
#define RS485_PORT1_TX          17
#define RS485_PORT1_DE          18
#define RS485_PORT2_RX          19
#define RS485_PORT2_TX          20
#define RS485_PORT2_DE          21

// Ethernet (W5500)
#define ETH_CS_PIN              5
#define ETH_INT_PIN             4
#define ETH_RST_PIN             -1

// LED Indicators
#define LED_STATUS_PIN          2
#define LED_NETWORK_PIN         -1
#define LED_ACTIVITY_PIN        -1

// Button
#define BUTTON_PIN              0
#define BUTTON_ACTIVE_LOW       1

// ============================================================================
// MANUFACTURER INFO
// ============================================================================

#define MANUFACTURER_NAME       "SURIOTA"
#define MANUFACTURER_URL        "www.suriota.com"
#define SUPPORT_EMAIL           "support@suriota.com"

// ============================================================================
// HELPER MACROS - DO NOT EDIT BELOW THIS LINE
// ============================================================================

// Generate full model string based on variant
#if PRODUCT_IS_POE
    #define PRODUCT_FULL_MODEL      "MGate-1210(P)"
    #define SERIAL_VARIANT_CODE     "P"
#else
    #define PRODUCT_FULL_MODEL      "MGate-1210"
    #define SERIAL_VARIANT_CODE     ""
#endif

// Version string helpers
#define STRINGIFY(x)            #x
#define TOSTRING(x)             STRINGIFY(x)

#define FIRMWARE_VERSION_STRING FIRMWARE_VERSION
#define FIRMWARE_BUILD_STRING   TOSTRING(FIRMWARE_BUILD_NUMBER)

// ============================================================================
// RUNTIME HELPERS (C++ Class)
// ============================================================================

#ifdef __cplusplus

#include <Arduino.h>

/**
 * @brief Static helper class for product configuration
 */
class ProductInfo {
public:
    /**
     * @brief Generate BLE name from MAC address
     * @param mac MAC address array (6 bytes)
     * @param buffer Output buffer (must be at least BLE_NAME_MAX_LENGTH)
     * @return Pointer to buffer
     *
     * Output format:
     *   POE:     "MGate-1210(P)A716"
     *   Non-POE: "MGate-1210C726"
     */
    static const char* generateBLEName(const uint8_t* mac, char* buffer) {
        // Generate UID from last 2 bytes of MAC (4 hex chars)
        char uid[5];
        snprintf(uid, sizeof(uid), "%02X%02X", mac[4], mac[5]);

        #if PRODUCT_IS_POE
            snprintf(buffer, BLE_NAME_MAX_LENGTH, "%s(P)%s", BLE_NAME_PREFIX, uid);
        #else
            snprintf(buffer, BLE_NAME_MAX_LENGTH, "%s%s", BLE_NAME_PREFIX, uid);
        #endif

        return buffer;
    }

    /**
     * @brief Generate serial number
     * @param mac MAC address array (6 bytes)
     * @param dateCode Date code string (YYYYMMDD format, or nullptr for auto)
     * @param buffer Output buffer (must be at least 24 chars)
     * @return Pointer to buffer
     *
     * Output format: "SRT-MGATE1210P-20251205-3AC9A7"
     */
    static const char* generateSerialNumber(const uint8_t* mac, const char* dateCode, char* buffer) {
        // Generate unique ID from MAC (last 3 bytes = 6 hex chars)
        char uid[7];
        snprintf(uid, sizeof(uid), "%02X%02X%02X", mac[3], mac[4], mac[5]);

        // Use provided date or placeholder
        const char* date = dateCode ? dateCode : "00000000";

        snprintf(buffer, 32, "%s-%s%s-%s-%s",
                 SERIAL_PREFIX,
                 SERIAL_MODEL_CODE,
                 SERIAL_VARIANT_CODE,
                 date,
                 uid);

        return buffer;
    }

    /**
     * @brief Get firmware version as string
     */
    static const char* getFirmwareVersion() {
        return FIRMWARE_VERSION;
    }

    /**
     * @brief Get firmware build number
     */
    static uint32_t getBuildNumber() {
        return FIRMWARE_BUILD_NUMBER;
    }

    /**
     * @brief Get full product model string
     */
    static const char* getProductModel() {
        return PRODUCT_FULL_MODEL;
    }

    /**
     * @brief Check if this is POE variant
     */
    static bool isPOE() {
        return PRODUCT_IS_POE == 1;
    }

    /**
     * @brief Get manufacturer name
     */
    static const char* getManufacturer() {
        return MANUFACTURER_NAME;
    }
};

#endif // __cplusplus

#endif // PRODUCT_CONFIG_H
