/**
 * @file OTAManager.cpp
 * @brief OTA Update Manager Implementation
 * @version 1.0.0
 * @date 2025-11-26
 */

#include "DebugConfig.h"  // MUST BE FIRST
#include "OTAManager.h"
#include "JsonDocumentPSRAM.h"
#include <LittleFS.h>
#include <esp_heap_caps.h>
#include <ArduinoJson.h>

// Singleton instance
OTAManager* OTAManager::instance = nullptr;

// Config file path
static const char* OTA_CONFIG_FILE = "/ota_config.json";

// Static callback instance for adapter
static OTAManager* callbackInstance = nullptr;

// ============================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================

OTAManager::OTAManager() :
    validator(nullptr),
    httpsTransport(nullptr),
    bleTransport(nullptr),
    configLoaded(false),
    currentState(OTAState::IDLE),
    updateMode(OTAUpdateMode::NONE),
    lastError(OTAError::NONE),
    currentBuildNumber(0),
    targetBuildNumber(0),
    progress(0),
    bytesDownloaded(0),
    totalBytes(0),
    updateAvailable(false),
    updateMandatory(false),
    lastCheckTime(0),
    lastUpdateTime(0),
    checkIntervalMs(OTA_CHECK_INTERVAL_HOURS * 3600000UL),
    bootCount(0),
    bootValidated(false),
    progressCallback(nullptr),
    completionCallback(nullptr),
    stateCallback(nullptr),
    otaTaskHandle(nullptr),
    checkTaskHandle(nullptr),
    bleServer(nullptr)
{
    managerMutex = xSemaphoreCreateMutex();
}

OTAManager::~OTAManager() {
    stop();
    if (managerMutex) {
        vSemaphoreDelete(managerMutex);
        managerMutex = nullptr;
    }
}

// ============================================
// SINGLETON
// ============================================

OTAManager* OTAManager::getInstance() {
    if (!instance) {
        void* ptr = heap_caps_malloc(sizeof(OTAManager), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (ptr) {
            instance = new (ptr) OTAManager();
        } else {
            instance = new OTAManager();
        }
    }
    return instance;
}

// ============================================
// INITIALIZATION
// ============================================

bool OTAManager::begin(BLEServer* server) {
    LOG_OTA_INFO("Initializing OTA Manager...\n");

    bleServer = server;
    callbackInstance = this;

    // Load configuration
    if (!loadConfiguration()) {
        LOG_OTA_WARN("Using default OTA configuration\n");
    }

    // Initialize validator
    validator = OTAValidator::getInstance();
    if (!validator->begin()) {
        LOG_OTA_ERROR("Failed to init OTA validator\n");
        return false;
    }

    // Set current version in validator
    uint8_t major, minor, patch;
    if (parseVersion(currentVersion.c_str(), major, minor, patch)) {
        validator->setCurrentVersion(major, minor, patch, currentBuildNumber);
    }

    // Initialize HTTPS transport
    httpsTransport = OTAHttps::getInstance();
    if (!httpsTransport->begin(validator)) {
        LOG_OTA_ERROR("Failed to init HTTPS transport\n");
        return false;
    }

    // Apply GitHub config
    GitHubConfig ghConfig;
    ghConfig.owner = config.githubOwner;
    ghConfig.repo = config.githubRepo;
    ghConfig.branch = config.githubBranch;
    ghConfig.token = config.githubToken;
    ghConfig.useReleases = config.useReleases;
    httpsTransport->setGitHubConfig(ghConfig);
    httpsTransport->setProgressCallback(progressCallbackAdapter);

    // Initialize BLE OTA if server provided and enabled
    if (bleServer && config.bleOtaEnabled) {
        bleTransport = OTABle::getInstance();
        if (!bleTransport->begin(bleServer, validator)) {
            LOG_OTA_WARN("BLE OTA init failed - continuing without BLE OTA\n");
            bleTransport = nullptr;
        } else {
            bleTransport->setProgressCallback(progressCallbackAdapter);
        }
    }

    // Handle boot validation (rollback protection)
    handleBootValidation();

    // Set check interval
    checkIntervalMs = config.checkIntervalHours * 3600000UL;

    LOG_OTA_INFO("OTA Manager initialized. Version: %s (build %u)\n",
                currentVersion.c_str(), currentBuildNumber);

    return true;
}

void OTAManager::stop() {
    // Stop tasks
    if (otaTaskHandle) {
        vTaskDelete(otaTaskHandle);
        otaTaskHandle = nullptr;
    }
    if (checkTaskHandle) {
        vTaskDelete(checkTaskHandle);
        checkTaskHandle = nullptr;
    }

    // Stop transports
    if (httpsTransport) {
        httpsTransport->stop();
    }
    if (bleTransport) {
        bleTransport->stop();
    }
    if (validator) {
        validator->stop();
    }

    callbackInstance = nullptr;
}

// ============================================
// CONFIGURATION
// ============================================

bool OTAManager::loadConfiguration() {
    if (!LittleFS.exists(OTA_CONFIG_FILE)) {
        LOG_OTA_INFO("OTA config file not found, using defaults\n");
        return false;
    }

    File f = LittleFS.open(OTA_CONFIG_FILE, "r");
    if (!f) {
        return false;
    }

    SpiRamJsonDocument doc;
    DeserializationError error = deserializeJson(doc, f);
    f.close();

    if (error) {
        LOG_OTA_ERROR("OTA config JSON error: %s\n", error.c_str());
        return false;
    }

    // Parse configuration
    config.enabled = doc["ota_enabled"] | true;

    // GitHub section
    JsonObject github = doc["github"].as<JsonObject>();
    if (!github.isNull()) {
        config.githubOwner = github["owner"].as<String>();
        config.githubRepo = github["repo"].as<String>();
        config.githubBranch = github["branch"] | "main";
        config.githubToken = github["token"].as<String>();
        config.useReleases = github["use_releases"] | true;
    }

    // Server section
    JsonObject server = doc["server"].as<JsonObject>();
    if (!server.isNull()) {
        config.checkIntervalHours = server["check_interval_hours"] | OTA_CHECK_INTERVAL_HOURS;
        config.retryCount = server["retry_count"] | OTA_RETRY_COUNT;
        config.retryDelayMs = server["retry_delay_ms"] | OTA_RETRY_DELAY_MS;
        config.timeoutMs = server["timeout_ms"] | OTA_HTTPS_TIMEOUT_MS;
    }

    // Update section
    JsonObject update = doc["update"].as<JsonObject>();
    if (!update.isNull()) {
        config.autoUpdate = update["auto_update"] | false;
        config.requireEthernet = update["require_ethernet"] | false;

        JsonObject hours = update["allowed_hours"].as<JsonObject>();
        if (!hours.isNull()) {
            config.restrictUpdateHours = true;
            config.allowedHourStart = hours["start"] | 2;
            config.allowedHourEnd = hours["end"] | 5;
        }
    }

    // Security section
    JsonObject security = doc["security"].as<JsonObject>();
    if (!security.isNull()) {
        config.verifySignature = security["verify_signature"] | true;
        config.allowDowngrade = security["allow_downgrade"] | false;
        config.requireHttps = security["require_https"] | true;
    }

    // BLE OTA section
    JsonObject bleOta = doc["ble_ota"].as<JsonObject>();
    if (!bleOta.isNull()) {
        config.bleOtaEnabled = bleOta["enabled"] | true;
        config.bleRequireButton = bleOta["require_button_press"] | true;
        config.bleTimeoutMs = bleOta["timeout_ms"] | OTA_BLE_TIMEOUT_MS;
    }

    configLoaded = true;
    LOG_OTA_INFO("OTA config loaded from %s\n", OTA_CONFIG_FILE);
    return true;
}

bool OTAManager::saveConfiguration() {
    SpiRamJsonDocument doc;

    doc["ota_enabled"] = config.enabled;

    JsonObject github = doc["github"].to<JsonObject>();
    github["owner"] = config.githubOwner;
    github["repo"] = config.githubRepo;
    github["branch"] = config.githubBranch;
    github["token"] = config.githubToken;
    github["use_releases"] = config.useReleases;

    JsonObject server = doc["server"].to<JsonObject>();
    server["check_interval_hours"] = config.checkIntervalHours;
    server["retry_count"] = config.retryCount;
    server["retry_delay_ms"] = config.retryDelayMs;
    server["timeout_ms"] = config.timeoutMs;

    JsonObject update = doc["update"].to<JsonObject>();
    update["auto_update"] = config.autoUpdate;
    update["require_ethernet"] = config.requireEthernet;

    JsonObject hours = update["allowed_hours"].to<JsonObject>();
    hours["start"] = config.allowedHourStart;
    hours["end"] = config.allowedHourEnd;

    JsonObject security = doc["security"].to<JsonObject>();
    security["verify_signature"] = config.verifySignature;
    security["allow_downgrade"] = config.allowDowngrade;
    security["require_https"] = config.requireHttps;

    JsonObject bleOta = doc["ble_ota"].to<JsonObject>();
    bleOta["enabled"] = config.bleOtaEnabled;
    bleOta["require_button_press"] = config.bleRequireButton;
    bleOta["timeout_ms"] = config.bleTimeoutMs;

    File f = LittleFS.open(OTA_CONFIG_FILE, "w");
    if (!f) {
        return false;
    }

    serializeJsonPretty(doc, f);
    f.close();
    return true;
}

void OTAManager::applyConfiguration() {
    if (httpsTransport) {
        GitHubConfig ghConfig;
        ghConfig.owner = config.githubOwner;
        ghConfig.repo = config.githubRepo;
        ghConfig.branch = config.githubBranch;
        ghConfig.token = config.githubToken;
        ghConfig.useReleases = config.useReleases;
        httpsTransport->setGitHubConfig(ghConfig);
        httpsTransport->setReadTimeout(config.timeoutMs);
        httpsTransport->setRetryParams(config.retryCount, config.retryDelayMs);
    }

    if (bleTransport) {
        bleTransport->setTotalTimeout(config.bleTimeoutMs);
    }

    checkIntervalMs = config.checkIntervalHours * 3600000UL;
}

bool OTAManager::reloadConfiguration() {
    if (loadConfiguration()) {
        applyConfiguration();
        return true;
    }
    return false;
}

// ============================================
// BOOT VALIDATION
// ============================================

void OTAManager::handleBootValidation() {
    Preferences prefs;
    prefs.begin(OTA_NVS_NAMESPACE, false);

    bootCount = prefs.getUChar(OTA_NVS_BOOT_COUNT, 0);

    if (bootCount >= OTA_MAX_BOOT_FAILURES) {
        LOG_OTA_ERROR("Boot loop detected (%d failures)! Rolling back...\n", bootCount);
        prefs.putUChar(OTA_NVS_BOOT_COUNT, 0);
        prefs.end();

        if (rollbackToPrevious()) {
            LOG_OTA_INFO("Rollback successful, rebooting...\n");
            ESP.restart();
        } else {
            LOG_OTA_ERROR("Rollback failed!\n");
        }
        return;
    }

    // Increment boot count
    prefs.putUChar(OTA_NVS_BOOT_COUNT, bootCount + 1);
    prefs.end();

    LOG_OTA_DEBUG("Boot count: %d\n", bootCount + 1);

    // Schedule validation timer (60 seconds)
    // In a real implementation, this would be done with a FreeRTOS timer
    // For now, we'll validate on first successful MQTT/HTTP connection
}

void OTAManager::incrementBootCount() {
    Preferences prefs;
    prefs.begin(OTA_NVS_NAMESPACE, false);
    bootCount = prefs.getUChar(OTA_NVS_BOOT_COUNT, 0);
    prefs.putUChar(OTA_NVS_BOOT_COUNT, bootCount + 1);
    prefs.end();
}

void OTAManager::resetBootCount() {
    Preferences prefs;
    prefs.begin(OTA_NVS_NAMESPACE, false);
    prefs.putUChar(OTA_NVS_BOOT_COUNT, 0);
    prefs.end();
    bootCount = 0;
}

uint8_t OTAManager::getBootCount() {
    Preferences prefs;
    prefs.begin(OTA_NVS_NAMESPACE, true);
    uint8_t count = prefs.getUChar(OTA_NVS_BOOT_COUNT, 0);
    prefs.end();
    return count;
}

// ============================================
// STATE MANAGEMENT
// ============================================

void OTAManager::setState(OTAState newState) {
    if (currentState != newState) {
        OTAState oldState = currentState;
        currentState = newState;

        LOG_OTA_INFO("OTA State: %s -> %s\n",
                    getOTAStateName(oldState), getOTAStateName(newState));

        if (stateCallback) {
            stateCallback(oldState, newState);
        }
    }
}

void OTAManager::setError(OTAError error, const String& message) {
    lastError = error;
    lastErrorMessage = message;
    LOG_OTA_ERROR("OTA Error: %s - %s\n",
                 getOTAErrorDescription(error), message.c_str());
}

void OTAManager::clearError() {
    lastError = OTAError::NONE;
    lastErrorMessage = "";
}

// ============================================
// VERSION INFO
// ============================================

void OTAManager::setCurrentVersion(const String& version, uint32_t buildNumber) {
    currentVersion = version;
    currentBuildNumber = buildNumber;

    if (validator) {
        uint8_t major, minor, patch;
        if (parseVersion(version.c_str(), major, minor, patch)) {
            validator->setCurrentVersion(major, minor, patch, buildNumber);
        }
    }
}

// ============================================
// UPDATE OPERATIONS
// ============================================

bool OTAManager::checkForUpdate() {
    if (currentState != OTAState::IDLE) {
        setError(OTAError::INVALID_STATE, "Update already in progress");
        return false;
    }

    if (!config.enabled) {
        setError(OTAError::INVALID_STATE, "OTA disabled");
        return false;
    }

    xSemaphoreTake(managerMutex, portMAX_DELAY);

    setState(OTAState::CHECKING);
    clearError();

    bool result = httpsTransport->checkForUpdate(pendingManifest);
    lastCheckTime = millis();

    if (result) {
        updateAvailable = true;
        updateMandatory = pendingManifest.mandatory;
        targetVersion = pendingManifest.version;

        LOG_OTA_INFO("Update available: v%s -> v%s\n",
                    currentVersion.c_str(), targetVersion.c_str());

        // Auto-update if enabled
        if (config.autoUpdate && !updateMandatory) {
            LOG_OTA_INFO("Auto-update disabled for non-mandatory updates\n");
        }
    } else {
        updateAvailable = false;
        if (httpsTransport->getLastError() != OTAError::ALREADY_UP_TO_DATE) {
            setError(httpsTransport->getLastError(),
                    httpsTransport->getLastErrorMessage());
        }
    }

    setState(OTAState::IDLE);
    xSemaphoreGive(managerMutex);

    return updateAvailable;
}

bool OTAManager::startUpdate() {
    if (!updateAvailable || !pendingManifest.valid) {
        // Check for update first
        if (!checkForUpdate()) {
            return false;
        }
    }

    return performHttpsUpdate();
}

bool OTAManager::startUpdateFromUrl(const String& url) {
    if (currentState != OTAState::IDLE) {
        setError(OTAError::INVALID_STATE, "Update already in progress");
        return false;
    }

    xSemaphoreTake(managerMutex, portMAX_DELAY);

    updateMode = OTAUpdateMode::HTTPS_CUSTOM;
    setState(OTAState::DOWNLOADING);
    clearError();

    ValidationResult result;
    bool success = httpsTransport->downloadFirmwareFromUrl(url, 0, "", "", result);

    if (success) {
        setState(OTAState::VALIDATING);
        handleUpdateComplete(true);
    } else {
        setError(httpsTransport->getLastError(), httpsTransport->getLastErrorMessage());
        setState(OTAState::ERROR);
        handleUpdateComplete(false);
    }

    xSemaphoreGive(managerMutex);
    return success;
}

bool OTAManager::performHttpsUpdate(const String& url) {
    if (currentState != OTAState::IDLE && currentState != OTAState::CHECKING) {
        return false;
    }

    xSemaphoreTake(managerMutex, portMAX_DELAY);

    updateMode = url.length() > 0 ? OTAUpdateMode::HTTPS_CUSTOM : OTAUpdateMode::HTTPS_GITHUB;
    setState(OTAState::DOWNLOADING);
    clearError();

    ValidationResult result;
    bool success;

    if (url.length() > 0) {
        success = httpsTransport->downloadFirmwareFromUrl(url, 0, "", "", result);
    } else {
        success = httpsTransport->downloadFirmware(pendingManifest, result);
    }

    if (success) {
        setState(OTAState::VALIDATING);
        // Validation done during download
        handleUpdateComplete(true);
    } else {
        setError(httpsTransport->getLastError(), httpsTransport->getLastErrorMessage());
        setState(OTAState::ERROR);
        handleUpdateComplete(false);
    }

    xSemaphoreGive(managerMutex);
    return success;
}

void OTAManager::abortUpdate() {
    if (httpsTransport) {
        httpsTransport->abortDownload();
    }
    if (bleTransport) {
        bleTransport->abortTransfer();
    }

    setState(OTAState::IDLE);
    updateMode = OTAUpdateMode::NONE;
}

void OTAManager::applyUpdate() {
    LOG_OTA_INFO("Applying update and rebooting...\n");
    setState(OTAState::REBOOTING);
    lastUpdateTime = millis();

    // Save update time
    Preferences prefs;
    prefs.begin(OTA_NVS_NAMESPACE, false);
    prefs.putULong(OTA_NVS_UPDATE_TIME, lastUpdateTime);
    prefs.putString(OTA_NVS_LAST_VERSION, targetVersion);
    prefs.end();

    vTaskDelay(pdMS_TO_TICKS(500));
    ESP.restart();
}

void OTAManager::handleUpdateComplete(bool success) {
    progress = success ? 100 : 0;

    if (completionCallback) {
        completionCallback(success, lastError,
                          success ? "Update ready" : lastErrorMessage.c_str());
    }

    if (success) {
        LOG_OTA_INFO("Update complete. Call applyUpdate() to reboot.\n");
    }
}

// ============================================
// PROGRESS CALLBACK
// ============================================

void OTAManager::progressCallbackAdapter(uint8_t prog, size_t current, size_t total) {
    if (callbackInstance) {
        callbackInstance->progress = prog;
        callbackInstance->bytesDownloaded = current;
        callbackInstance->totalBytes = total;

        if (callbackInstance->progressCallback) {
            callbackInstance->progressCallback(prog, current, total);
        }
    }
}

// ============================================
// BLE OTA
// ============================================

void OTAManager::enableBleOta() {
    if (bleTransport) {
        LOG_OTA_INFO("BLE OTA enabled\n");
        updateMode = OTAUpdateMode::BLE;
    }
}

void OTAManager::disableBleOta() {
    if (bleTransport && updateMode == OTAUpdateMode::BLE) {
        bleTransport->abortTransfer();
        updateMode = OTAUpdateMode::NONE;
    }
}

bool OTAManager::isBleOtaActive() const {
    return bleTransport && bleTransport->getState() != BLEOTAState::IDLE;
}

// ============================================
// STATUS
// ============================================

OTAStatus OTAManager::getStatus() const {
    OTAStatus status;
    status.state = currentState;
    status.mode = updateMode;
    status.lastError = lastError;
    status.lastErrorMessage = lastErrorMessage;
    status.progress = progress;
    status.bytesDownloaded = bytesDownloaded;
    status.totalBytes = totalBytes;
    status.currentVersion = currentVersion;
    status.targetVersion = targetVersion;
    status.updateAvailable = updateAvailable;
    status.updateMandatory = updateMandatory;
    status.lastCheckTime = lastCheckTime;
    status.lastUpdateTime = lastUpdateTime;
    return status;
}

// ============================================
// CALLBACKS
// ============================================

void OTAManager::setProgressCallback(OTAProgressCallback callback) {
    progressCallback = callback;
}

void OTAManager::setCompletionCallback(OTACompletionCallback callback) {
    completionCallback = callback;
    if (httpsTransport) {
        // Also pass to transport completion
    }
    if (bleTransport) {
        bleTransport->setCompletionCallback(callback);
    }
}

void OTAManager::setStateCallback(OTAStateCallback callback) {
    stateCallback = callback;
}

// ============================================
// ROLLBACK
// ============================================

bool OTAManager::markFirmwareValid() {
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err == ESP_OK) {
        resetBootCount();
        bootValidated = true;
        LOG_OTA_INFO("Firmware marked as valid\n");
        return true;
    }
    LOG_OTA_ERROR("Failed to mark firmware valid: %s\n", esp_err_to_name(err));
    return false;
}

bool OTAManager::rollbackToPrevious() {
    const esp_partition_t* partition = esp_ota_get_last_invalid_partition();
    if (partition) {
        esp_err_t err = esp_ota_set_boot_partition(partition);
        if (err == ESP_OK) {
            LOG_OTA_INFO("Rollback to previous: %s\n", partition->label);
            return true;
        }
    }
    return rollbackToFactory();
}

bool OTAManager::rollbackToFactory() {
    const esp_partition_t* factory = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
    if (factory) {
        esp_err_t err = esp_ota_set_boot_partition(factory);
        if (err == ESP_OK) {
            LOG_OTA_INFO("Rollback to factory: %s\n", factory->label);
            return true;
        }
    }
    LOG_OTA_ERROR("No factory partition found\n");
    return false;
}

bool OTAManager::isRollbackPending() const {
    return !bootValidated && bootCount > 0;
}

// ============================================
// MQTT INTEGRATION
// ============================================

String OTAManager::handleMqttCommand(const String& action, const String& params) {
    SpiRamJsonDocument responseDoc;
    responseDoc["action"] = action;

    if (action == "check_update") {
        bool available = checkForUpdate();
        responseDoc["status"] = "ok";
        responseDoc["update_available"] = available;
        if (available) {
            responseDoc["version"] = pendingManifest.version;
            responseDoc["mandatory"] = pendingManifest.mandatory;
        }
    }
    else if (action == "start_update") {
        if (startUpdate()) {
            responseDoc["status"] = "ok";
            responseDoc["message"] = "Update started";
        } else {
            responseDoc["status"] = "error";
            responseDoc["error"] = getOTAErrorDescription(lastError);
        }
    }
    else if (action == "abort_update") {
        abortUpdate();
        responseDoc["status"] = "ok";
    }
    else if (action == "apply_update") {
        responseDoc["status"] = "ok";
        responseDoc["message"] = "Rebooting...";
        // Serialize before reboot
        String response;
        serializeJson(responseDoc, response);
        applyUpdate();
        return response;
    }
    else if (action == "get_status") {
        return getStatusJson();
    }
    else {
        responseDoc["status"] = "error";
        responseDoc["error"] = "Unknown action";
    }

    String response;
    serializeJson(responseDoc, response);
    return response;
}

String OTAManager::getStatusJson() const {
    SpiRamJsonDocument doc;

    doc["state"] = getOTAStateName(currentState);
    doc["progress"] = progress;
    doc["current_version"] = currentVersion;
    doc["update_available"] = updateAvailable;

    if (updateAvailable) {
        doc["target_version"] = targetVersion;
        doc["mandatory"] = updateMandatory;
    }

    if (lastError != OTAError::NONE) {
        doc["error"] = getOTAErrorDescription(lastError);
        doc["error_message"] = lastErrorMessage;
    }

    String json;
    serializeJson(doc, json);
    return json;
}

// ============================================
// BLE CRUD INTEGRATION
// ============================================

bool OTAManager::handleCrudCommand(const String& command, const JsonObject& params,
                                   JsonObject& response) {
    if (command == "ota_check") {
        bool available = checkForUpdate();
        response["status"] = "ok";
        response["update_available"] = available;
        if (available) {
            response["version"] = pendingManifest.version;
            response["size"] = pendingManifest.firmwareSize;
            response["mandatory"] = pendingManifest.mandatory;
            response["changelog"] = pendingManifest.changelog;
        }
        return true;
    }
    else if (command == "ota_start") {
        if (startUpdate()) {
            response["status"] = "ok";
            response["message"] = "Update started";
        } else {
            response["status"] = "error";
            response["error"] = getOTAErrorDescription(lastError);
        }
        return true;
    }
    else if (command == "ota_abort") {
        abortUpdate();
        response["status"] = "ok";
        return true;
    }
    else if (command == "ota_apply") {
        response["status"] = "ok";
        response["message"] = "Rebooting...";
        // Delay apply to allow response
        xTaskCreate([](void* p) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            OTAManager::getInstance()->applyUpdate();
        }, "ota_apply", 2048, nullptr, 1, nullptr);
        return true;
    }
    else if (command == "ota_status") {
        OTAStatus status = getStatus();
        response["status"] = "ok";
        response["state"] = getOTAStateName(status.state);
        response["progress"] = status.progress;
        response["current_version"] = status.currentVersion;
        response["update_available"] = status.updateAvailable;
        if (status.updateAvailable) {
            response["target_version"] = status.targetVersion;
        }
        return true;
    }
    else if (command == "ota_config") {
        // Get/set configuration
        if (params["github_repo"].is<const char*>()) {
            setGitHubRepo(params["github_owner"] | config.githubOwner,
                         params["github_repo"].as<String>(),
                         params["github_branch"] | config.githubBranch);
            saveConfiguration();
        }
        response["status"] = "ok";
        response["github_owner"] = config.githubOwner;
        response["github_repo"] = config.githubRepo;
        response["github_branch"] = config.githubBranch;
        response["auto_update"] = config.autoUpdate;
        return true;
    }

    return false;
}

// ============================================
// PROCESS LOOP
// ============================================

void OTAManager::process() {
    // Process BLE OTA timeouts
    if (bleTransport) {
        bleTransport->process();
    }

    // Auto-check for updates (if enabled and interval elapsed)
    if (config.enabled && checkIntervalMs > 0) {
        if (millis() - lastCheckTime > checkIntervalMs) {
            if (currentState == OTAState::IDLE) {
                LOG_OTA_INFO("Periodic update check...\n");
                checkForUpdate();
            }
        }
    }
}

// ============================================
// CONFIGURATION SETTERS
// ============================================

void OTAManager::setGitHubRepo(const String& owner, const String& repo,
                               const String& branch) {
    config.githubOwner = owner;
    config.githubRepo = repo;
    config.githubBranch = branch;
    applyConfiguration();
}

void OTAManager::setGitHubToken(const String& token) {
    config.githubToken = token;
    applyConfiguration();
}

void OTAManager::setAutoUpdate(bool enabled) {
    config.autoUpdate = enabled;
}

void OTAManager::setCheckInterval(uint32_t hours) {
    config.checkIntervalHours = hours;
    checkIntervalMs = hours * 3600000UL;
}
