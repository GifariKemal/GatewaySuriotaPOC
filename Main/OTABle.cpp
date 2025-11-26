/**
 * @file OTABle.cpp
 * @brief BLE OTA Transport Implementation
 * @version 1.0.0
 * @date 2025-11-26
 */

#include "DebugConfig.h"  // MUST BE FIRST
#include "OTABle.h"
#include <esp_heap_caps.h>

// Singleton instance
OTABle* OTABle::instance = nullptr;

// ============================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================

OTABle::OTABle() :
    pServer(nullptr),
    pOTAService(nullptr),
    pControlChar(nullptr),
    pDataChar(nullptr),
    pStatusChar(nullptr),
    state(BLEOTAState::IDLE),
    lastError(OTAError::NONE),
    serviceStarted(false),
    clientConnected(false),
    checksumProvided(false),
    updatePartition(nullptr),
    otaHandle(0),
    otaStarted(false),
    validator(nullptr),
    progressCallback(nullptr),
    completionCallback(nullptr),
    receiveBuffer(nullptr),
    receiveBufferSize(OTA_BLE_CHUNK_SIZE + 16),  // Chunk + header
    chunkTimeoutMs(OTA_BLE_CHUNK_TIMEOUT_MS),
    totalTimeoutMs(OTA_BLE_TIMEOUT_MS),
    transferStartTime(0)
{
    bleMutex = xSemaphoreCreateMutex();
    stats.reset();
    memset(expectedChecksum, 0, sizeof(expectedChecksum));
}

OTABle::~OTABle() {
    stop();
    if (bleMutex) {
        vSemaphoreDelete(bleMutex);
        bleMutex = nullptr;
    }
}

// ============================================
// SINGLETON
// ============================================

OTABle* OTABle::getInstance() {
    if (!instance) {
        void* ptr = heap_caps_malloc(sizeof(OTABle), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (ptr) {
            instance = new (ptr) OTABle();
        } else {
            instance = new OTABle();
        }
    }
    return instance;
}

// ============================================
// INITIALIZATION
// ============================================

bool OTABle::begin(BLEServer* server, OTAValidator* validatorInstance) {
    if (serviceStarted) {
        return true;
    }

    pServer = server;
    validator = validatorInstance;

    if (!pServer) {
        LOG_OTA_ERROR("BLE server required for OTA service\n");
        return false;
    }

    if (!allocateBuffer()) {
        LOG_OTA_ERROR("Failed to allocate receive buffer\n");
        return false;
    }

    if (!initBLEService()) {
        freeBuffer();
        LOG_OTA_ERROR("Failed to init BLE OTA service\n");
        return false;
    }

    serviceStarted = true;
    LOG_OTA_INFO("BLE OTA service initialized\n");
    return true;
}

void OTABle::stop() {
    if (!serviceStarted) return;

    abortTransfer();
    cleanupBLEService();
    freeBuffer();

    serviceStarted = false;
    validator = nullptr;
}

bool OTABle::initBLEService() {
    // Create OTA service
    pOTAService = pServer->createService(OTA_BLE_SERVICE_UUID);
    if (!pOTAService) {
        return false;
    }

    // Control characteristic (Write) - for commands
    pControlChar = pOTAService->createCharacteristic(
        OTA_BLE_CONTROL_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    pControlChar->setCallbacks(this);

    // Data characteristic (Write No Response) - for firmware chunks
    pDataChar = pOTAService->createCharacteristic(
        OTA_BLE_DATA_UUID,
        BLECharacteristic::PROPERTY_WRITE_NR
    );
    pDataChar->setCallbacks(this);

    // Status characteristic (Notify) - for progress/status
    pStatusChar = pOTAService->createCharacteristic(
        OTA_BLE_STATUS_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    // Note: BLE2902 descriptor is automatically added by NimBLE when NOTIFY is enabled

    // Start service
    pOTAService->start();

    LOG_OTA_INFO("BLE OTA service started\n");
    return true;
}

void OTABle::cleanupBLEService() {
    if (pOTAService) {
        pOTAService->stop();
        // Note: BLE service cleanup is handled by BLEServer
        pOTAService = nullptr;
        pControlChar = nullptr;
        pDataChar = nullptr;
        pStatusChar = nullptr;
    }
}

bool OTABle::allocateBuffer() {
    freeBuffer();

    // Try PSRAM first
    receiveBuffer = (uint8_t*)heap_caps_malloc(receiveBufferSize,
                                                MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!receiveBuffer) {
        receiveBuffer = (uint8_t*)malloc(receiveBufferSize);
    }

    return receiveBuffer != nullptr;
}

void OTABle::freeBuffer() {
    if (receiveBuffer) {
        free(receiveBuffer);
        receiveBuffer = nullptr;
    }
}

// ============================================
// BLE CALLBACKS
// ============================================

void OTABle::onWrite(BLECharacteristic* pCharacteristic) {
    String value = pCharacteristic->getValue();
    if (value.length() == 0) return;

    const uint8_t* data = (const uint8_t*)value.c_str();
    size_t len = value.length();
    uint8_t cmd = data[0];

    if (pCharacteristic == pControlChar) {
        // Command characteristic
        switch (cmd) {
            case OTA_BLE_CMD_START:
                handleStartCommand(data, len);
                break;
            case OTA_BLE_CMD_VERIFY:
                handleVerifyCommand(data, len);
                break;
            case OTA_BLE_CMD_APPLY:
                handleApplyCommand();
                break;
            case OTA_BLE_CMD_ABORT:
                handleAbortCommand();
                break;
            case OTA_BLE_CMD_STATUS:
                handleStatusCommand();
                break;
            default:
                LOG_OTA_WARN("Unknown OTA command: 0x%02X\n", cmd);
                sendResponse(OTA_BLE_RSP_ERROR, "Unknown command");
                break;
        }
    } else if (pCharacteristic == pDataChar) {
        // Data characteristic
        if (cmd == OTA_BLE_CMD_DATA) {
            handleDataCommand(data, len);
        }
    }
}

void OTABle::onConnect() {
    clientConnected = true;
    LOG_OTA_INFO("BLE OTA client connected\n");
}

void OTABle::onDisconnect() {
    clientConnected = false;
    LOG_OTA_INFO("BLE OTA client disconnected\n");

    // Abort any ongoing transfer
    if (state == BLEOTAState::RECEIVING) {
        LOG_OTA_WARN("Transfer interrupted by disconnect\n");
        abortTransfer();
    }
}

// ============================================
// COMMAND HANDLERS
// ============================================

void OTABle::handleStartCommand(const uint8_t* data, size_t len) {
    if (state != BLEOTAState::IDLE) {
        sendResponse(OTA_BLE_RSP_BUSY, "Transfer in progress");
        return;
    }

    // Minimum length: cmd(1) + size(4) = 5
    if (len < 5) {
        sendResponse(OTA_BLE_RSP_ERROR, "Invalid start command");
        return;
    }

    xSemaphoreTake(bleMutex, portMAX_DELAY);

    // Parse command
    uint32_t totalSize = *((uint32_t*)(data + 1));
    uint16_t chunkSize = OTA_BLE_CHUNK_SIZE;  // Default

    if (len >= 7) {
        chunkSize = *((uint16_t*)(data + 5));
    }

    // Check for checksum (optional, at offset 7)
    checksumProvided = false;
    if (len >= 7 + OTA_HASH_SIZE) {
        memcpy(expectedChecksum, data + 7, OTA_HASH_SIZE);
        checksumProvided = true;
        LOG_OTA_INFO("Checksum provided with start command\n");
    }

    LOG_OTA_INFO("OTA Start: size=%u, chunk=%u\n", totalSize, chunkSize);

    // Validate size
    if (totalSize == 0 || totalSize > OTA_MAX_FIRMWARE_SIZE) {
        sendResponse(OTA_BLE_RSP_SIZE_ERROR, "Invalid firmware size");
        xSemaphoreGive(bleMutex);
        return;
    }

    // Begin OTA partition
    if (!beginOTAPartition(totalSize)) {
        sendResponse(OTA_BLE_RSP_FLASH_ERROR, "Failed to prepare flash");
        xSemaphoreGive(bleMutex);
        return;
    }

    // Initialize hash computation
    if (validator) {
        validator->hashBegin();
    }

    // Reset stats
    stats.reset();
    stats.totalBytes = totalSize;
    stats.chunksExpected = (totalSize + chunkSize - 1) / chunkSize;
    stats.startTime = millis();
    transferStartTime = stats.startTime;

    setState(BLEOTAState::RECEIVING);
    sendResponse(OTA_BLE_RSP_OK, "Ready to receive");

    xSemaphoreGive(bleMutex);
}

void OTABle::handleDataCommand(const uint8_t* data, size_t len) {
    if (state != BLEOTAState::RECEIVING) {
        return;  // Silently ignore (Write No Response)
    }

    // Minimum: cmd(1) + seq(2) + data(1) = 4
    if (len < 4) {
        stats.errorCount++;
        return;
    }

    xSemaphoreTake(bleMutex, portMAX_DELAY);

    // Parse command
    uint16_t seqNum = *((uint16_t*)(data + 1));
    const uint8_t* chunkData = data + 3;
    size_t chunkLen = len - 3;

    // Check sequence
    if (seqNum != stats.lastSeqNum + 1 && stats.lastSeqNum != 0xFFFF) {
        // Out of order - could implement retry logic here
        LOG_OTA_WARN("Sequence mismatch: expected %u, got %u\n",
                    stats.lastSeqNum + 1, seqNum);
        stats.errorCount++;
        // Continue anyway for now
    }

    // Write to flash
    if (!writeOTAData(chunkData, chunkLen)) {
        lastError = OTAError::FLASH_ERROR;
        lastErrorMessage = "Flash write failed";
        setState(BLEOTAState::ERROR);
        abortOTA();
        sendError(OTA_BLE_RSP_FLASH_ERROR);
        xSemaphoreGive(bleMutex);
        return;
    }

    // Update hash
    if (validator) {
        validator->hashUpdate(chunkData, chunkLen);
    }

    // Update stats
    stats.bytesReceived += chunkLen;
    stats.chunksReceived++;
    stats.lastSeqNum = seqNum;
    stats.lastChunkTime = millis();

    // Send ACK every N chunks
    if (stats.chunksReceived % OTA_BLE_ACK_INTERVAL == 0) {
        sendProgress();
    }

    // Progress callback
    if (progressCallback) {
        uint8_t progress = getProgress();
        progressCallback(progress, stats.bytesReceived, stats.totalBytes);
    }

    // Check if complete
    if (stats.bytesReceived >= stats.totalBytes) {
        LOG_OTA_INFO("All data received: %u bytes in %lu ms\n",
                    stats.bytesReceived, millis() - stats.startTime);
        setState(BLEOTAState::VERIFYING);
        sendProgress();
    }

    xSemaphoreGive(bleMutex);
}

void OTABle::handleVerifyCommand(const uint8_t* data, size_t len) {
    // Can also receive checksum with verify command
    if (len > 1 && len >= 1 + OTA_HASH_SIZE) {
        memcpy(expectedChecksum, data + 1, OTA_HASH_SIZE);
        checksumProvided = true;
    }

    if (state != BLEOTAState::VERIFYING && state != BLEOTAState::RECEIVING) {
        sendResponse(OTA_BLE_RSP_INVALID_STATE, "Not ready to verify");
        return;
    }

    xSemaphoreTake(bleMutex, portMAX_DELAY);

    LOG_OTA_INFO("Verifying firmware...\n");

    // Finalize hash
    uint8_t computedHash[OTA_HASH_SIZE];
    if (validator) {
        validator->hashFinalize(computedHash);

        // Verify checksum if provided
        if (checksumProvided) {
            if (!validator->compareHash(computedHash, expectedChecksum)) {
                char hexHash[65];
                validator->hashToHex(computedHash, hexHash);
                LOG_OTA_ERROR("Checksum mismatch! Computed: %s\n", hexHash);

                lastError = OTAError::CHECKSUM_FAILED;
                lastErrorMessage = "Checksum verification failed";
                setState(BLEOTAState::ERROR);
                abortOTA();
                sendError(OTA_BLE_RSP_CHECKSUM_FAIL);
                xSemaphoreGive(bleMutex);
                return;
            }
            LOG_OTA_INFO("Checksum verified\n");
        } else {
            LOG_OTA_WARN("No checksum provided - skipping verification\n");
        }
    }

    setState(BLEOTAState::COMPLETE);
    sendResponse(OTA_BLE_RSP_OK, "Verification passed");

    xSemaphoreGive(bleMutex);
}

void OTABle::handleApplyCommand() {
    if (state != BLEOTAState::COMPLETE) {
        sendResponse(OTA_BLE_RSP_INVALID_STATE, "Not ready to apply");
        return;
    }

    xSemaphoreTake(bleMutex, portMAX_DELAY);

    LOG_OTA_INFO("Applying firmware update...\n");

    setState(BLEOTAState::APPLYING);

    // Finalize OTA
    if (!finalizeOTA()) {
        lastError = OTAError::FLASH_ERROR;
        lastErrorMessage = "Failed to finalize OTA";
        setState(BLEOTAState::ERROR);
        sendError(OTA_BLE_RSP_FLASH_ERROR);
        xSemaphoreGive(bleMutex);
        return;
    }

    sendResponse(OTA_BLE_RSP_OK, "Update applied, rebooting...");

    // Completion callback
    if (completionCallback) {
        completionCallback(true, OTAError::NONE, "Update successful");
    }

    xSemaphoreGive(bleMutex);

    // Delay to allow response to be sent
    vTaskDelay(pdMS_TO_TICKS(1000));

    LOG_OTA_INFO("Rebooting...\n");
    ESP.restart();
}

void OTABle::handleAbortCommand() {
    LOG_OTA_INFO("OTA abort requested\n");
    abortTransfer();
    sendResponse(OTA_BLE_RSP_OK, "Transfer aborted");
}

void OTABle::handleStatusCommand() {
    sendProgress();
}

// ============================================
// OTA PARTITION OPERATIONS
// ============================================

bool OTABle::beginOTAPartition(size_t firmwareSize) {
    updatePartition = esp_ota_get_next_update_partition(NULL);
    if (!updatePartition) {
        LOG_OTA_ERROR("No OTA partition available\n");
        return false;
    }

    if (firmwareSize > updatePartition->size) {
        LOG_OTA_ERROR("Firmware too large: %u > %u\n", firmwareSize, updatePartition->size);
        return false;
    }

    esp_err_t err = esp_ota_begin(updatePartition, firmwareSize, &otaHandle);
    if (err != ESP_OK) {
        LOG_OTA_ERROR("esp_ota_begin failed: %s\n", esp_err_to_name(err));
        return false;
    }

    otaStarted = true;
    LOG_OTA_INFO("OTA partition ready: %s\n", updatePartition->label);
    return true;
}

bool OTABle::writeOTAData(const uint8_t* data, size_t len) {
    if (!otaStarted) return false;

    esp_err_t err = esp_ota_write(otaHandle, data, len);
    if (err != ESP_OK) {
        LOG_OTA_ERROR("esp_ota_write failed: %s\n", esp_err_to_name(err));
        return false;
    }

    return true;
}

bool OTABle::finalizeOTA() {
    if (!otaStarted) return false;

    esp_err_t err = esp_ota_end(otaHandle);
    if (err != ESP_OK) {
        LOG_OTA_ERROR("esp_ota_end failed: %s\n", esp_err_to_name(err));
        otaStarted = false;
        return false;
    }

    err = esp_ota_set_boot_partition(updatePartition);
    if (err != ESP_OK) {
        LOG_OTA_ERROR("esp_ota_set_boot_partition failed: %s\n", esp_err_to_name(err));
        otaStarted = false;
        return false;
    }

    otaStarted = false;
    return true;
}

void OTABle::abortOTA() {
    if (otaStarted) {
        esp_ota_abort(otaHandle);
        otaStarted = false;
    }
}

// ============================================
// RESPONSE HELPERS
// ============================================

void OTABle::sendResponse(uint8_t code, const char* message) {
    if (!pStatusChar) return;

    BLEOTAStatusResponse response;
    response.state = static_cast<uint8_t>(state);
    response.progress = getProgress();
    response.bytesReceived = stats.bytesReceived;
    response.totalBytes = stats.totalBytes;
    response.errorCode = code;

    pStatusChar->setValue((uint8_t*)&response, sizeof(response));
    pStatusChar->notify();

    if (message) {
        LOG_OTA_DEBUG("Response: 0x%02X - %s\n", code, message);
    }
}

void OTABle::sendProgress() {
    sendResponse(OTA_BLE_RSP_OK);
}

void OTABle::sendError(uint8_t errorCode) {
    sendResponse(errorCode, "Error");

    if (completionCallback) {
        completionCallback(false, lastError, lastErrorMessage.c_str());
    }
}

// ============================================
// STATE MANAGEMENT
// ============================================

void OTABle::setState(BLEOTAState newState) {
    if (state != newState) {
        LOG_OTA_DEBUG("State: %d -> %d\n", static_cast<int>(state), static_cast<int>(newState));
        state = newState;
    }
}

void OTABle::resetTransfer() {
    xSemaphoreTake(bleMutex, portMAX_DELAY);

    abortOTA();
    stats.reset();
    checksumProvided = false;
    memset(expectedChecksum, 0, sizeof(expectedChecksum));
    transferStartTime = 0;
    setState(BLEOTAState::IDLE);
    lastError = OTAError::NONE;
    lastErrorMessage = "";

    xSemaphoreGive(bleMutex);
}

bool OTABle::checkTimeout() {
    if (state != BLEOTAState::RECEIVING) {
        return false;
    }

    unsigned long now = millis();

    // Check chunk timeout
    if (stats.lastChunkTime > 0 && (now - stats.lastChunkTime) > chunkTimeoutMs) {
        LOG_OTA_ERROR("Chunk timeout\n");
        lastError = OTAError::TIMEOUT;
        lastErrorMessage = "Chunk receive timeout";
        return true;
    }

    // Check total timeout
    if (transferStartTime > 0 && (now - transferStartTime) > totalTimeoutMs) {
        LOG_OTA_ERROR("Total transfer timeout\n");
        lastError = OTAError::TIMEOUT;
        lastErrorMessage = "Transfer timeout";
        return true;
    }

    return false;
}

// ============================================
// PUBLIC METHODS
// ============================================

uint8_t OTABle::getProgress() const {
    if (stats.totalBytes == 0) return 0;
    return (stats.bytesReceived * 100) / stats.totalBytes;
}

void OTABle::setProgressCallback(OTAProgressCallback callback) {
    progressCallback = callback;
}

void OTABle::setCompletionCallback(OTACompletionCallback callback) {
    completionCallback = callback;
}

void OTABle::setChunkTimeout(uint32_t timeoutMs) {
    chunkTimeoutMs = timeoutMs;
}

void OTABle::setTotalTimeout(uint32_t timeoutMs) {
    totalTimeoutMs = timeoutMs;
}

void OTABle::abortTransfer() {
    resetTransfer();
    LOG_OTA_INFO("Transfer aborted\n");
}

void OTABle::process() {
    // Check for timeouts
    if (checkTimeout()) {
        setState(BLEOTAState::ERROR);
        abortOTA();
        sendError(OTA_BLE_RSP_ERROR);
    }
}
