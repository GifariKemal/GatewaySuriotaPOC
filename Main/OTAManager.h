/**
 * @file OTAManager.h
 * @brief OTA Update Manager - Main State Machine Controller
 * @version 1.0.0
 * @date 2025-11-26
 *
 * Central controller for OTA updates:
 * - State machine (IDLE → CHECKING → DOWNLOADING → VALIDATING → APPLYING → REBOOTING)
 * - HTTPS OTA from GitHub (public/private repos)
 * - BLE OTA from smartphone
 * - MQTT OTA commands
 * - Automatic rollback on boot failure
 * - Version management and anti-rollback protection
 *
 * Integration:
 * - OTAValidator: Signature/checksum verification
 * - OTAHttps: GitHub download transport
 * - OTABle: BLE transfer transport
 */

#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include "DebugConfig.h"  // MUST BE FIRST
#include "OTAConfig.h"
#include "OTAValidator.h"
#include "OTAHttps.h"
#include "OTABle.h"
#include <Arduino.h>
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// Forward declarations
class OTAManager;
class BLEServer;

/**
 * @brief OTA Update Mode
 */
enum class OTAUpdateMode : uint8_t {
    NONE = 0,
    HTTPS_GITHUB = 1,   // Download from GitHub
    HTTPS_CUSTOM = 2,   // Download from custom URL
    BLE = 3,            // BLE transfer from app
    MQTT_TRIGGER = 4    // MQTT triggered update
};

/**
 * @brief OTA Configuration (loaded from ota_config.json)
 */
struct OTAConfiguration {
    // General
    bool enabled;
    bool autoUpdate;

    // GitHub
    String githubOwner;
    String githubRepo;
    String githubBranch;
    String githubToken;
    bool useReleases;

    // Check settings
    uint32_t checkIntervalHours;
    uint8_t retryCount;
    uint32_t retryDelayMs;
    uint32_t timeoutMs;

    // Update window
    bool restrictUpdateHours;
    uint8_t allowedHourStart;
    uint8_t allowedHourEnd;

    // Requirements
    bool requireEthernet;

    // Security
    bool verifySignature;
    bool allowDowngrade;
    bool requireHttps;

    // BLE OTA
    bool bleOtaEnabled;
    bool bleRequireButton;
    uint32_t bleTimeoutMs;

    // Defaults
    OTAConfiguration() :
        enabled(true), autoUpdate(false),
        githubOwner(OTA_DEFAULT_GITHUB_OWNER),
        githubRepo(OTA_DEFAULT_GITHUB_REPO),
        githubBranch(OTA_DEFAULT_GITHUB_BRANCH),
        useReleases(true),
        checkIntervalHours(OTA_CHECK_INTERVAL_HOURS),
        retryCount(OTA_RETRY_COUNT),
        retryDelayMs(OTA_RETRY_DELAY_MS),
        timeoutMs(OTA_HTTPS_TIMEOUT_MS),
        restrictUpdateHours(false),
        allowedHourStart(2), allowedHourEnd(5),
        requireEthernet(false),
        verifySignature(OTA_VERIFY_SIGNATURE),
        allowDowngrade(OTA_ALLOW_DOWNGRADE),
        requireHttps(true),
        bleOtaEnabled(true), bleRequireButton(true),
        bleTimeoutMs(OTA_BLE_TIMEOUT_MS) {}
};

/**
 * @brief OTA Status for external queries
 */
struct OTAStatus {
    OTAState state;
    OTAUpdateMode mode;
    OTAError lastError;
    String lastErrorMessage;

    uint8_t progress;
    size_t bytesDownloaded;
    size_t totalBytes;

    String currentVersion;
    String targetVersion;

    bool updateAvailable;
    bool updateMandatory;

    unsigned long lastCheckTime;
    unsigned long lastUpdateTime;
};

/**
 * @brief OTA Update Manager
 *
 * Singleton class providing central OTA management:
 * - Configuration loading/saving
 * - Version checking
 * - Update orchestration
 * - Rollback handling
 * - MQTT/BLE command integration
 */
class OTAManager {
private:
    static OTAManager* instance;

    // Sub-components
    OTAValidator* validator;
    OTAHttps* httpsTransport;
    OTABle* bleTransport;

    // Configuration
    OTAConfiguration config;
    bool configLoaded;

    // State
    OTAState currentState;
    OTAUpdateMode updateMode;
    OTAError lastError;
    String lastErrorMessage;

    // Version info
    String currentVersion;
    uint32_t currentBuildNumber;
    String targetVersion;
    uint32_t targetBuildNumber;

    // Progress
    uint8_t progress;
    size_t bytesDownloaded;
    size_t totalBytes;

    // Update info
    bool updateAvailable;
    bool updateMandatory;
    FirmwareManifest pendingManifest;

    // Timing
    unsigned long lastCheckTime;
    unsigned long lastUpdateTime;
    unsigned long checkIntervalMs;

    // Boot validation
    uint8_t bootCount;
    bool bootValidated;

    // Callbacks
    OTAProgressCallback progressCallback;
    OTACompletionCallback completionCallback;
    OTAStateCallback stateCallback;

    // Thread safety
    SemaphoreHandle_t managerMutex;
    TaskHandle_t otaTaskHandle;
    TaskHandle_t checkTaskHandle;

    // BLE server reference
    BLEServer* bleServer;

    // Private constructor (singleton)
    OTAManager();
    ~OTAManager();

    // Configuration
    bool loadConfiguration();
    bool saveConfiguration();
    void applyConfiguration();

    // Boot validation
    void handleBootValidation();
    void incrementBootCount();
    void resetBootCount();
    uint8_t getBootCount();

    // State management
    void setState(OTAState newState);
    void setError(OTAError error, const String& message);
    void clearError();

    // Internal operations
    bool performVersionCheck();
    bool performHttpsUpdate(const String& url = "");
    bool performBleUpdate();
    void handleUpdateComplete(bool success);

    // Task functions
    static void otaTaskFunction(void* param);
    static void checkTaskFunction(void* param);

    // Progress callback adapter
    static void progressCallbackAdapter(uint8_t progress, size_t current, size_t total);

public:
    /**
     * @brief Get singleton instance
     */
    static OTAManager* getInstance();

    /**
     * @brief Initialize OTA Manager
     * @param server BLE server for OTA service (optional)
     * @return true if successful
     */
    bool begin(BLEServer* server = nullptr);

    /**
     * @brief Stop OTA Manager
     */
    void stop();

    // ============================================
    // CONFIGURATION
    // ============================================

    /**
     * @brief Load configuration from file
     */
    bool reloadConfiguration();

    /**
     * @brief Get current configuration
     */
    OTAConfiguration getConfiguration() const { return config; }

    /**
     * @brief Set GitHub repository
     */
    void setGitHubRepo(const String& owner, const String& repo,
                       const String& branch = "main");

    /**
     * @brief Set GitHub token for private repos
     */
    void setGitHubToken(const String& token);

    /**
     * @brief Enable/disable automatic updates
     */
    void setAutoUpdate(bool enabled);

    /**
     * @brief Set check interval
     */
    void setCheckInterval(uint32_t hours);

    // ============================================
    // VERSION INFO
    // ============================================

    /**
     * @brief Set current firmware version
     */
    void setCurrentVersion(const String& version, uint32_t buildNumber);

    /**
     * @brief Get current firmware version
     */
    String getCurrentVersion() const { return currentVersion; }

    /**
     * @brief Get current build number
     */
    uint32_t getCurrentBuildNumber() const { return currentBuildNumber; }

    // ============================================
    // UPDATE OPERATIONS
    // ============================================

    /**
     * @brief Check for available updates
     * @return true if update available
     */
    bool checkForUpdate();

    /**
     * @brief Start update from GitHub
     * @return true if update started
     */
    bool startUpdate();

    /**
     * @brief Start update from custom URL
     * @param url Firmware URL
     * @return true if update started
     */
    bool startUpdateFromUrl(const String& url);

    /**
     * @brief Abort current update
     */
    void abortUpdate();

    /**
     * @brief Apply pending update and reboot
     */
    void applyUpdate();

    // ============================================
    // BLE OTA
    // ============================================

    /**
     * @brief Enable BLE OTA mode (for button press)
     */
    void enableBleOta();

    /**
     * @brief Disable BLE OTA mode
     */
    void disableBleOta();

    /**
     * @brief Check if BLE OTA is active
     */
    bool isBleOtaActive() const;

    // ============================================
    // STATUS & PROGRESS
    // ============================================

    /**
     * @brief Get current OTA status
     */
    OTAStatus getStatus() const;

    /**
     * @brief Get current state
     */
    OTAState getState() const { return currentState; }

    /**
     * @brief Get last error
     */
    OTAError getLastError() const { return lastError; }

    /**
     * @brief Get last error message
     */
    String getLastErrorMessage() const { return lastErrorMessage; }

    /**
     * @brief Get progress (0-100)
     */
    uint8_t getProgress() const { return progress; }

    /**
     * @brief Check if update is available
     */
    bool isUpdateAvailable() const { return updateAvailable; }

    /**
     * @brief Get pending manifest info
     */
    FirmwareManifest getPendingManifest() const { return pendingManifest; }

    // ============================================
    // CALLBACKS
    // ============================================

    /**
     * @brief Set progress callback
     */
    void setProgressCallback(OTAProgressCallback callback);

    /**
     * @brief Set completion callback
     */
    void setCompletionCallback(OTACompletionCallback callback);

    /**
     * @brief Set state change callback
     */
    void setStateCallback(OTAStateCallback callback);

    // ============================================
    // ROLLBACK
    // ============================================

    /**
     * @brief Mark current firmware as valid (prevent rollback)
     */
    bool markFirmwareValid();

    /**
     * @brief Rollback to previous firmware
     */
    bool rollbackToPrevious();

    /**
     * @brief Rollback to factory firmware
     */
    bool rollbackToFactory();

    /**
     * @brief Check if rollback is pending
     */
    bool isRollbackPending() const;

    // ============================================
    // MQTT INTEGRATION
    // ============================================

    /**
     * @brief Handle MQTT OTA command
     * @param action Command action (check_update, start_update, etc.)
     * @param params Optional parameters (JSON string)
     * @return JSON response string
     */
    String handleMqttCommand(const String& action, const String& params = "");

    /**
     * @brief Get OTA status as JSON for MQTT
     */
    String getStatusJson() const;

    // ============================================
    // BLE CRUD INTEGRATION
    // ============================================

    /**
     * @brief Handle BLE CRUD OTA command
     * @param command Command name
     * @param params Command parameters (JsonObject)
     * @param response Response object to fill
     * @return true if command handled
     */
    bool handleCrudCommand(const String& command, const JsonObject& params,
                          JsonObject& response);

    // ============================================
    // PROCESS LOOP
    // ============================================

    /**
     * @brief Process OTA tasks (call periodically from main loop)
     */
    void process();

    // Singleton - no copy
    OTAManager(const OTAManager&) = delete;
    OTAManager& operator=(const OTAManager&) = delete;
};

#endif // OTA_MANAGER_H
