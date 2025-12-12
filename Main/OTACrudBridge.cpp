/**
 * @file OTACrudBridge.cpp
 * @brief Implementation of OTA bridge functions
 * @version 2.5.35
 * @date 2025-12-12
 *
 * This file contains all OTA operations and is the ONLY file that includes OTAManager.h.
 * This prevents ESP_SSLClient linker errors caused by non-inline functions in Helper.h.
 *
 * IMPORTANT: This is the ONLY .cpp file that should include OTAManager.h!
 * All other files must use OTACrudBridge.h for OTA access.
 */

#include "DebugConfig.h"  // MUST BE FIRST
#include "OTACrudBridge.h"
#include "OTAManager.h"   // ONLY include here - causes ESP_SSLClient to be pulled in

namespace OTACrudBridge {

// ============================================
// INITIALIZATION FUNCTIONS
// ============================================

OTAManager* getInstance() {
    return OTAManager::getInstance();
}

bool begin(OTAManager* otaManager) {
    if (!otaManager) return false;
    return otaManager->begin(nullptr);
}

void setCurrentVersion(OTAManager* otaManager, const String& version, uint32_t buildNumber) {
    if (otaManager) {
        otaManager->setCurrentVersion(version, buildNumber);
    }
}

bool markFirmwareValid(OTAManager* otaManager) {
    if (!otaManager) return false;
    return otaManager->markFirmwareValid();
}

void process(OTAManager* otaManager) {
    if (otaManager) {
        otaManager->process();
    }
}

// ============================================
// CRUD HANDLER FUNCTIONS
// ============================================

bool checkUpdate(OTAManager* otaManager, JsonDocument& response) {
    if (!otaManager) {
        response["status"] = "error";
        response["error_message"] = "OTA Manager not initialized";
        return false;
    }

    response["status"] = "ok";
    response["command"] = "check_update";

    LOG_OTA_INFO("[OTA] Checking for firmware updates...");
    bool updateAvailable = otaManager->checkForUpdate();

    OTAStatus otaStatus = otaManager->getStatus();
    response["update_available"] = updateAvailable;
    response["current_version"] = otaStatus.currentVersion;

    if (updateAvailable) {
        response["target_version"] = otaStatus.targetVersion;
        response["mandatory"] = otaStatus.updateMandatory;

        FirmwareManifest manifest = otaManager->getPendingManifest();
        if (!manifest.version.isEmpty()) {
            JsonObject manifestObj = response["manifest"].to<JsonObject>();
            manifestObj["version"] = manifest.version;
            manifestObj["size"] = manifest.firmwareSize;
            manifestObj["release_notes"] = manifest.changelog;
        }

        LOG_OTA_INFO("[OTA] Update available: %s -> %s",
                     otaStatus.currentVersion.c_str(), otaStatus.targetVersion.c_str());
    } else {
        LOG_OTA_INFO("[OTA] No update available. Current version: %s",
                     otaStatus.currentVersion.c_str());
    }

    return true;
}

bool startUpdate(OTAManager* otaManager, const String& customUrl, JsonDocument& response) {
    if (!otaManager) {
        response["status"] = "error";
        response["error_message"] = "OTA Manager not initialized";
        return false;
    }

    response["status"] = "ok";
    response["command"] = "start_update";

    bool started = false;
    if (!customUrl.isEmpty()) {
        LOG_OTA_INFO("[OTA] Starting update from custom URL: %s", customUrl.c_str());
        started = otaManager->startUpdateFromUrl(customUrl);
    } else {
        LOG_OTA_INFO("[OTA] Starting update from GitHub...");
        started = otaManager->startUpdate();
    }

    if (started) {
        response["message"] = "OTA update started. Monitor progress with ota_status command.";
        response["update_mode"] = "https";
        LOG_OTA_INFO("[OTA] Update started successfully");
        return true;
    } else {
        OTAError error = otaManager->getLastError();
        String errorMsg = otaManager->getLastErrorMessage();
        response["status"] = "error";
        response["error_code"] = (uint8_t)error;
        response["error_message"] = errorMsg;
        LOG_OTA_INFO("[OTA] Failed to start update: %s", errorMsg.c_str());
        return false;
    }
}

void getStatus(OTAManager* otaManager, JsonDocument& response) {
    if (!otaManager) {
        response["status"] = "error";
        response["error_message"] = "OTA Manager not initialized";
        return;
    }

    response["status"] = "ok";
    response["command"] = "ota_status";

    OTAStatus otaStatus = otaManager->getStatus();

    response["state"] = (uint8_t)otaStatus.state;
    response["state_name"] = [](OTAState s) {
        switch(s) {
            case OTAState::IDLE: return "IDLE";
            case OTAState::CHECKING: return "CHECKING";
            case OTAState::DOWNLOADING: return "DOWNLOADING";
            case OTAState::VALIDATING: return "VALIDATING";
            case OTAState::APPLYING: return "APPLYING";
            case OTAState::REBOOTING: return "REBOOTING";
            case OTAState::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }(otaStatus.state);

    response["progress"] = otaStatus.progress;
    response["bytes_downloaded"] = otaStatus.bytesDownloaded;
    response["total_bytes"] = otaStatus.totalBytes;
    response["current_version"] = otaStatus.currentVersion;
    response["target_version"] = otaStatus.targetVersion;
    response["update_available"] = otaStatus.updateAvailable;

    if (otaStatus.lastError != OTAError::NONE) {
        response["last_error"] = (uint8_t)otaStatus.lastError;
        response["last_error_message"] = otaStatus.lastErrorMessage;
    }
}

void abortUpdate(OTAManager* otaManager, JsonDocument& response) {
    if (!otaManager) {
        response["status"] = "error";
        response["error_message"] = "OTA Manager not initialized";
        return;
    }

    otaManager->abortUpdate();
    response["status"] = "ok";
    response["command"] = "abort_update";
    response["message"] = "OTA update aborted";
    LOG_OTA_INFO("[OTA] Update aborted by user");
}

void confirmUpdate(OTAManager* otaManager, JsonDocument& response) {
    if (!otaManager) {
        response["status"] = "error";
        response["error_message"] = "OTA Manager not initialized";
        return;
    }

    response["status"] = "ok";
    response["command"] = "apply_update";

    OTAState state = otaManager->getState();
    OTAStatus status = otaManager->getStatus();

    // v2.5.35: Handle VALIDATING state (download complete, ready for reboot)
    // Valid states for apply: VALIDATING (just finished download) or IDLE with pending update
    if (state == OTAState::VALIDATING ||
        (state == OTAState::IDLE && status.updateAvailable)) {
        response["message"] = "Applying firmware update and rebooting...";
        LOG_OTA_INFO("[OTA] Applying update and rebooting...");

        // Send response before reboot
        vTaskDelay(pdMS_TO_TICKS(100));

        // Apply and reboot
        otaManager->applyUpdate();
    } else if (state == OTAState::IDLE) {
        response["status"] = "error";
        response["error_message"] = "No firmware ready to apply. Run check_update and start_update first.";
        response["current_state"] = "IDLE";
    } else {
        response["status"] = "error";
        response["error_message"] = "Cannot apply update in current state";
        response["current_state"] = (uint8_t)state;
    }
}

void enableBleOta(OTAManager* otaManager, JsonDocument& response) {
    if (!otaManager) {
        response["status"] = "error";
        response["error_message"] = "OTA Manager not initialized";
        return;
    }

    otaManager->enableBleOta();
    response["status"] = "ok";
    response["command"] = "enable_ble_ota";
    response["message"] = "BLE OTA mode enabled. Device is ready to receive firmware via BLE.";
    LOG_OTA_INFO("[OTA] BLE OTA mode enabled");
}

void disableBleOta(OTAManager* otaManager, JsonDocument& response) {
    if (!otaManager) {
        response["status"] = "error";
        response["error_message"] = "OTA Manager not initialized";
        return;
    }

    otaManager->disableBleOta();
    response["status"] = "ok";
    response["command"] = "disable_ble_ota";
    response["message"] = "BLE OTA mode disabled";
    LOG_OTA_INFO("[OTA] BLE OTA mode disabled");
}

void rollback(OTAManager* otaManager, const String& target, JsonDocument& response) {
    if (!otaManager) {
        response["status"] = "error";
        response["error_message"] = "OTA Manager not initialized";
        return;
    }

    response["command"] = "rollback";

    bool success = false;
    if (target == "factory") {
        LOG_OTA_INFO("[OTA] Rolling back to factory firmware...");
        success = otaManager->rollbackToFactory();
        response["target"] = "factory";
    } else {
        LOG_OTA_INFO("[OTA] Rolling back to previous firmware...");
        success = otaManager->rollbackToPrevious();
        response["target"] = "previous";
    }

    if (success) {
        response["status"] = "ok";
        response["message"] = "Rollback initiated. Device will reboot...";
    } else {
        response["status"] = "error";
        response["error_message"] = otaManager->getLastErrorMessage();
    }
}

void getConfig(OTAManager* otaManager, JsonDocument& response) {
    if (!otaManager) {
        response["status"] = "error";
        response["error_message"] = "OTA Manager not initialized";
        return;
    }

    response["status"] = "ok";
    response["command"] = "get_ota_config";

    OTAConfiguration config = otaManager->getConfiguration();
    JsonObject configObj = response["config"].to<JsonObject>();
    configObj["enabled"] = config.enabled;
    configObj["auto_update"] = config.autoUpdate;
    configObj["github_owner"] = config.githubOwner;
    configObj["github_repo"] = config.githubRepo;
    configObj["github_branch"] = config.githubBranch;
    configObj["check_interval_hours"] = config.checkIntervalHours;
    configObj["verify_signature"] = config.verifySignature;
    configObj["allow_downgrade"] = config.allowDowngrade;
    configObj["ble_ota_enabled"] = config.bleOtaEnabled;
}

void setGitHubRepo(OTAManager* otaManager, const String& owner, const String& repo,
                   const String& branch, JsonDocument& response) {
    if (!otaManager) {
        response["status"] = "error";
        response["error_message"] = "OTA Manager not initialized";
        return;
    }

    otaManager->setGitHubRepo(owner, repo, branch);
    response["status"] = "ok";
    response["command"] = "set_github_repo";
    response["message"] = "GitHub repository updated";

    JsonObject configObj = response["config"].to<JsonObject>();
    configObj["owner"] = owner;
    configObj["repo"] = repo;
    configObj["branch"] = branch;

    LOG_OTA_INFO("[OTA] GitHub repo updated: %s/%s (branch: %s)",
                 owner.c_str(), repo.c_str(), branch.c_str());
}

void setGitHubToken(OTAManager* otaManager, const String& token, JsonDocument& response) {
    if (!otaManager) {
        response["status"] = "error";
        response["error_message"] = "OTA Manager not initialized";
        return;
    }

    otaManager->setGitHubToken(token);
    response["status"] = "ok";
    response["command"] = "set_github_token";
    response["message"] = "GitHub token updated";
    response["token_set"] = !token.isEmpty();

    LOG_OTA_INFO("[OTA] GitHub token %s", token.isEmpty() ? "cleared" : "updated");
}

} // namespace OTACrudBridge
