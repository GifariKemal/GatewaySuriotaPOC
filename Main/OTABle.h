/**
 * @file OTABle.h
 * @brief BLE OTA Transport Layer
 * @version 1.0.0
 * @date 2025-11-26
 *
 * Provides BLE firmware transfer for local OTA updates:
 * - Dedicated OTA BLE Service
 * - 244-byte chunk transfer (MTU-safe)
 * - Progress notifications
 * - Checksum verification
 * - Command/Response protocol
 *
 * BLE Commands (from OTA_UPDATE.md):
 * - OTA_START (0x50): Initialize OTA with size/checksum
 * - OTA_DATA (0x51): Send firmware chunks
 * - OTA_VERIFY (0x52): Verify complete firmware
 * - OTA_APPLY (0x53): Apply update and reboot
 * - OTA_ABORT (0x54): Cancel OTA
 * - OTA_STATUS (0x55): Get progress
 */

#ifndef OTA_BLE_H
#define OTA_BLE_H

#include "DebugConfig.h"  // MUST BE FIRST
#include "OTAConfig.h"
#include "OTAValidator.h"
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

// Forward declarations
class OTABle;
class OTAValidator;

/**
 * @brief BLE OTA Transfer State
 */
enum class BLEOTAState : uint8_t {
    IDLE = 0,           // Ready to receive OTA
    RECEIVING = 1,      // Receiving firmware chunks
    VERIFYING = 2,      // Verifying firmware
    APPLYING = 3,       // Writing to flash
    COMPLETE = 4,       // Ready to reboot
    ERROR = 5           // Error occurred
};

/**
 * @brief BLE OTA Start Command Payload
 */
#pragma pack(push, 1)
struct BLEOTAStartCmd {
    uint8_t cmd;            // 0x50
    uint32_t totalSize;     // Total firmware size
    uint16_t chunkSize;     // Expected chunk size
    uint8_t checksum[32];   // SHA-256 checksum
};

struct BLEOTADataCmd {
    uint8_t cmd;            // 0x51
    uint16_t seqNum;        // Sequence number
    uint8_t data[];         // Variable length data
};

struct BLEOTAStatusResponse {
    uint8_t state;          // BLEOTAState
    uint8_t progress;       // 0-100%
    uint32_t bytesReceived;
    uint32_t totalBytes;
    uint8_t errorCode;
};
#pragma pack(pop)

/**
 * @brief BLE OTA Transfer Statistics
 */
struct BLEOTAStats {
    uint32_t chunksReceived;
    uint32_t chunksExpected;
    uint32_t bytesReceived;
    uint32_t totalBytes;
    uint32_t errorCount;
    uint32_t retryCount;
    unsigned long startTime;
    unsigned long lastChunkTime;
    uint16_t lastSeqNum;

    void reset() {
        chunksReceived = 0;
        chunksExpected = 0;
        bytesReceived = 0;
        totalBytes = 0;
        errorCount = 0;
        retryCount = 0;
        startTime = 0;
        lastChunkTime = 0;
        lastSeqNum = 0xFFFF;
    }
};

/**
 * @brief BLE OTA Transport
 *
 * Handles firmware transfer via BLE:
 * - Separate OTA service (doesn't interfere with main BLE service)
 * - Chunked transfer with acknowledgments
 * - Progress notifications
 * - Error recovery
 */
class OTABle : public BLECharacteristicCallbacks {
private:
    static OTABle* instance;

    // BLE objects
    BLEServer* pServer;
    BLEService* pOTAService;
    BLECharacteristic* pControlChar;    // Command input
    BLECharacteristic* pDataChar;       // Data input (Write No Response)
    BLECharacteristic* pStatusChar;     // Status notifications

    // State
    BLEOTAState state;
    OTAError lastError;
    String lastErrorMessage;
    bool serviceStarted;
    bool clientConnected;

    // Transfer state
    BLEOTAStats stats;
    uint8_t expectedChecksum[OTA_HASH_SIZE];
    bool checksumProvided;

    // OTA partition
    const esp_partition_t* updatePartition;
    esp_ota_handle_t otaHandle;
    bool otaStarted;

    // Validator reference
    OTAValidator* validator;

    // Callbacks
    OTAProgressCallback progressCallback;
    OTACompletionCallback completionCallback;

    // Thread safety
    SemaphoreHandle_t bleMutex;

    // Buffer for receiving chunks
    uint8_t* receiveBuffer;
    size_t receiveBufferSize;

    // Timeout handling
    unsigned long chunkTimeoutMs;
    unsigned long totalTimeoutMs;
    unsigned long transferStartTime;

    // Private constructor (singleton)
    OTABle();
    ~OTABle();

    // Internal methods
    bool initBLEService();
    void cleanupBLEService();
    bool allocateBuffer();
    void freeBuffer();

    // Command handlers
    void handleStartCommand(const uint8_t* data, size_t len);
    void handleDataCommand(const uint8_t* data, size_t len);
    void handleVerifyCommand(const uint8_t* data, size_t len);
    void handleApplyCommand();
    void handleAbortCommand();
    void handleStatusCommand();

    // OTA operations
    bool beginOTAPartition(size_t firmwareSize);
    bool writeOTAData(const uint8_t* data, size_t len);
    bool finalizeOTA();
    void abortOTA();

    // Response helpers
    void sendResponse(uint8_t code, const char* message = nullptr);
    void sendProgress();
    void sendError(uint8_t errorCode);

    // State management
    void setState(BLEOTAState newState);
    void resetTransfer();
    bool checkTimeout();

public:
    /**
     * @brief Get singleton instance
     */
    static OTABle* getInstance();

    /**
     * @brief Initialize BLE OTA service
     * @param server Existing BLE server (or nullptr to create new)
     * @param validatorInstance OTA validator instance
     * @return true if successful
     */
    bool begin(BLEServer* server, OTAValidator* validatorInstance);

    /**
     * @brief Stop BLE OTA service
     */
    void stop();

    /**
     * @brief Check if service is running
     */
    bool isRunning() const { return serviceStarted; }

    /**
     * @brief Check if client is connected
     */
    bool isConnected() const { return clientConnected; }

    // ============================================
    // STATE & PROGRESS
    // ============================================

    /**
     * @brief Get current state
     */
    BLEOTAState getState() const { return state; }

    /**
     * @brief Get current progress (0-100)
     */
    uint8_t getProgress() const;

    /**
     * @brief Get transfer statistics
     */
    BLEOTAStats getStats() const { return stats; }

    /**
     * @brief Get last error
     */
    OTAError getLastError() const { return lastError; }

    /**
     * @brief Get last error message
     */
    String getLastErrorMessage() const { return lastErrorMessage; }

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

    // ============================================
    // CONFIGURATION
    // ============================================

    /**
     * @brief Set chunk timeout
     */
    void setChunkTimeout(uint32_t timeoutMs);

    /**
     * @brief Set total transfer timeout
     */
    void setTotalTimeout(uint32_t timeoutMs);

    // ============================================
    // CONTROL
    // ============================================

    /**
     * @brief Abort current transfer
     */
    void abortTransfer();

    /**
     * @brief Process timeouts (call periodically)
     */
    void process();

    // ============================================
    // BLE CALLBACKS
    // ============================================

    void onWrite(BLECharacteristic* pCharacteristic) override;

    /**
     * @brief Handle client connection (called by BLEManager)
     */
    void onConnect();

    /**
     * @brief Handle client disconnection
     */
    void onDisconnect();

    // Singleton - no copy
    OTABle(const OTABle&) = delete;
    OTABle& operator=(const OTABle&) = delete;
};

#endif // OTA_BLE_H
