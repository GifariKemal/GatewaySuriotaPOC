/**
 * @file OTACrudBridge.h
 * @brief Bridge between application code and OTAManager to avoid ESP_SSLClient
 * linker issues
 * @version 2.5.35
 * @date 2025-12-12
 *
 * This file provides function declarations that allow other modules to call OTA
 * functions without directly including OTAManager.h (which pulls in
 * ESP_SSLClient headers).
 *
 * The actual implementations are in OTACrudBridge.cpp which is the ONLY file
 * that includes OTAManager.h. This prevents multiple definition linker errors
 * from ESP_SSLClient's Helper.h which has non-inline function definitions.
 *
 * IMPORTANT: All access to OTAManager must go through this bridge!
 */

#ifndef OTA_CRUD_BRIDGE_H
#define OTA_CRUD_BRIDGE_H

#include "JsonDocumentPSRAM.h"  // v2.5.35: Use SpiRamJsonDocument (JsonDocument is #define'd to it)

// Forward declaration - DO NOT include OTAManager.h here!
class OTAManager;
class BLEManager;  // v2.5.37: For OTA progress notifications

namespace OTACrudBridge {

// ============================================
// INITIALIZATION FUNCTIONS (called from Main.ino)
// ============================================

/**
 * @brief Get OTAManager singleton instance
 * @return Pointer to OTAManager instance
 */
OTAManager* getInstance();

/**
 * @brief Initialize OTA Manager
 * @param otaManager Pointer to OTAManager instance
 * @return true if successful
 */
bool begin(OTAManager* otaManager);

/**
 * @brief Set current firmware version
 * @param otaManager Pointer to OTAManager instance
 * @param version Firmware version string
 * @param buildNumber Build number
 */
void setCurrentVersion(OTAManager* otaManager, const String& version,
                       uint32_t buildNumber);

/**
 * @brief Mark firmware as valid (prevent rollback)
 * @param otaManager Pointer to OTAManager instance
 * @return true if successful
 */
bool markFirmwareValid(OTAManager* otaManager);

/**
 * @brief Process OTA tasks (call periodically from main loop)
 * @param otaManager Pointer to OTAManager instance
 */
void process(OTAManager* otaManager);

/**
 * @brief Set BLE Manager for OTA progress notifications (v2.5.37)
 * @param otaManager Pointer to OTAManager instance
 * @param bleManager Pointer to BLEManager instance
 */
void setBLENotificationManager(OTAManager* otaManager, BLEManager* bleManager);

// ============================================
// CRUD HANDLER FUNCTIONS
// ============================================

/**
 * @brief Check for firmware updates
 * @param otaManager Pointer to OTAManager instance
 * @param response JSON response object to fill
 * @return true if check succeeded (not if update available)
 */
bool checkUpdate(OTAManager* otaManager, JsonDocument& response);

/**
 * @brief Start OTA update
 * @param otaManager Pointer to OTAManager instance
 * @param customUrl Optional custom URL (empty string for GitHub)
 * @param response JSON response object to fill
 * @return true if update started successfully
 */
bool startUpdate(OTAManager* otaManager, const String& customUrl,
                 JsonDocument& response);

/**
 * @brief Get OTA status
 * @param otaManager Pointer to OTAManager instance
 * @param response JSON response object to fill
 */
void getStatus(OTAManager* otaManager, JsonDocument& response);

/**
 * @brief Abort current OTA update
 * @param otaManager Pointer to OTAManager instance
 * @param response JSON response object to fill
 */
void abortUpdate(OTAManager* otaManager, JsonDocument& response);

/**
 * @brief Confirm and apply pending update
 * @param otaManager Pointer to OTAManager instance
 * @param response JSON response object to fill
 */
void confirmUpdate(OTAManager* otaManager, JsonDocument& response);

/**
 * @brief Enable BLE OTA mode
 * @param otaManager Pointer to OTAManager instance
 * @param response JSON response object to fill
 */
void enableBleOta(OTAManager* otaManager, JsonDocument& response);

/**
 * @brief Disable BLE OTA mode
 * @param otaManager Pointer to OTAManager instance
 * @param response JSON response object to fill
 */
void disableBleOta(OTAManager* otaManager, JsonDocument& response);

/**
 * @brief Rollback firmware
 * @param otaManager Pointer to OTAManager instance
 * @param target "factory" or "previous"
 * @param response JSON response object to fill
 */
void rollback(OTAManager* otaManager, const String& target,
              JsonDocument& response);

/**
 * @brief Get OTA configuration
 * @param otaManager Pointer to OTAManager instance
 * @param response JSON response object to fill
 */
void getConfig(OTAManager* otaManager, JsonDocument& response);

/**
 * @brief Set GitHub repository settings
 * @param otaManager Pointer to OTAManager instance
 * @param owner Repository owner
 * @param repo Repository name
 * @param branch Branch name
 * @param response JSON response object to fill
 */
void setGitHubRepo(OTAManager* otaManager, const String& owner,
                   const String& repo, const String& branch,
                   JsonDocument& response);

/**
 * @brief Set GitHub token
 * @param otaManager Pointer to OTAManager instance
 * @param token GitHub personal access token
 * @param response JSON response object to fill
 */
void setGitHubToken(OTAManager* otaManager, const String& token,
                    JsonDocument& response);

}  // namespace OTACrudBridge

#endif  // OTA_CRUD_BRIDGE_H
