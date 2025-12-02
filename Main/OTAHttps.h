/**
 * @file OTAHttps.h
 * @brief HTTPS OTA Transport Layer - GitHub Integration
 * @version 2.5.20
 * @date 2025-12-02
 *
 * Provides HTTPS firmware download from GitHub:
 * - GitHub Releases download
 * - GitHub Raw file download
 * - Private repository support (token auth)
 * - Streaming download with progress
 * - TLS 1.2+ security
 * - Manifest parsing
 *
 * v2.5.20: Increased connection timeout (5s), reduced SSL buffer (8KB)
 * v2.5.15: Resume download support, retry count, progress bar display
 * v2.0.0: Switched to ESP_SSLClient (mobizt) with PSRAM support
 */

#ifndef OTA_HTTPS_H
#define OTA_HTTPS_H

#include "DebugConfig.h"  // MUST BE FIRST
#include "OTAConfig.h"
#include "OTAValidator.h"
#include "NetworkManager.h"  // v2.5.3: For multi-network support (WiFi + Ethernet)
#include <Arduino.h>
#include <WiFi.h>              // v2.5.3: For WiFiClient base transport

// ============================================
// v2.5.9: ESP_SSLClient Configuration (MUST BE BEFORE #include)
// ============================================
// Enable PSRAM for SSL buffers - this is the KEY fix for "record too large" error
// ESP32-S3 has 8MB PSRAM, plenty for SSL buffers
#define ENABLE_PSRAM

// Use 8KB buffers for stable connections (reduced from 16KB for slow networks)
#define ESP_SSLCLIENT_BUFFER_SIZE 8192

// Enable debug for troubleshooting (set to 0 for production)
#define ESP_SSLCLIENT_ENABLE_DEBUG 0

#include <ESP_SSLClient.h>      // v2.5.9: mobizt ESP_SSLClient with PSRAM support

#include "GitHubTrustAnchors.h" // GitHub root CA certificates
#include <Update.h>
#include <ArduinoJson.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
// v2.5.9: ESP_SSLClient (mobizt) - PSRAM support for large SSL buffers

// Forward declarations
class OTAHttps;
class OTAValidator;

/**
 * @brief Firmware manifest from GitHub
 */
struct FirmwareManifest {
    String version;
    uint32_t buildNumber;
    String releaseDate;
    String minVersion;
    String changelog;

    String firmwareUrl;
    uint32_t firmwareSize;
    String sha256Hash;
    String signature;  // Base64 encoded

    bool mandatory;
    bool valid;

    FirmwareManifest() : buildNumber(0), firmwareSize(0),
                         mandatory(false), valid(false) {}
};

/**
 * @brief GitHub repository configuration
 */
struct GitHubConfig {
    String owner;
    String repo;
    String branch;
    String token;       // Optional: for private repos
    bool useReleases;   // true = use releases, false = use raw files

    GitHubConfig() : branch("main"), useReleases(true) {}
};

/**
 * @brief Download progress information
 */
struct DownloadProgress {
    size_t bytesDownloaded;
    size_t totalBytes;
    uint8_t percent;
    uint32_t bytesPerSecond;
    uint32_t estimatedSecondsRemaining;
    bool inProgress;

    DownloadProgress() : bytesDownloaded(0), totalBytes(0), percent(0),
                        bytesPerSecond(0), estimatedSecondsRemaining(0),
                        inProgress(false) {}
};

/**
 * @brief HTTPS OTA Transport
 *
 * Handles firmware download from GitHub repositories:
 * - Manifest fetching and parsing
 * - Firmware binary download
 * - Streaming to ESP32 OTA partition
 * - Progress callbacks
 * - Error handling and retry logic
 */
class OTAHttps {
private:
    static OTAHttps* instance;

    // HTTP clients - v2.5.9: ESP_SSLClient (mobizt) with PSRAM support
    Client* sslClient;               // Active SSL client (polymorphic)
    WiFiClient* wifiBase;            // Base client for WiFi (wrapped by SSL)
    ESP_SSLClient* wifiSecure;       // ESP_SSLClient wrapping WiFiClient
    EthernetClient* ethBase;         // Base client for Ethernet
    ESP_SSLClient* ethSecure;        // ESP_SSLClient wrapping EthernetClient
    NetworkMgr* networkManager;      // For network status
    bool usingWiFi;                  // Track which interface is active

    // Configuration
    GitHubConfig githubConfig;
    String serverUrl;
    uint32_t connectTimeoutMs;
    uint32_t readTimeoutMs;
    uint8_t maxRetries;
    uint32_t retryDelayMs;

    // State
    bool downloading;
    bool abortRequested;
    DownloadProgress progress;
    OTAError lastError;
    String lastErrorMessage;

    // v2.5.15: Resume and retry state
    uint8_t currentRetryCount;
    size_t resumeFromByte;
    String lastDownloadUrl;
    uint8_t lastProgressPercent;  // For progress bar update tracking

    // OTA partition
    const esp_partition_t* updatePartition;
    esp_ota_handle_t otaHandle;
    bool otaStarted;

    // Validator reference
    OTAValidator* validator;

    // Callbacks
    OTAProgressCallback progressCallback;

    // Thread safety
    SemaphoreHandle_t httpMutex;

    // Buffer (allocated in PSRAM)
    uint8_t* downloadBuffer;
    size_t bufferSize;

    // Private constructor (singleton)
    OTAHttps();
    ~OTAHttps();

    // Internal methods
    bool initSecureClient();
    void cleanupSecureClient();
    bool allocateBuffer();
    void freeBuffer();

    // URL builders
    String buildRawUrl(const String& path);
    String buildReleaseUrl(const String& tag, const String& filename);
    String buildApiUrl(const String& endpoint);

    // HTTP helpers
    bool setupHttpClient(const String& url);
    int performRequest(const char* method = "GET", size_t rangeStart = 0);

    // Download helpers
    bool beginOTAPartition(size_t firmwareSize);
    bool writeOTAData(const uint8_t* data, size_t len);
    bool finalizeOTA();
    void abortOTA();

    // v2.5.15: Progress display (dev mode only)
    void printProgressBar(uint8_t percent, size_t downloaded, size_t total, uint32_t speed);

    // v2.5.15: Internal download with resume support
    bool downloadWithRetry(const String& url, size_t expectedSize,
                          const String& expectedHash, const String& signature,
                          ValidationResult& result);

public:
    /**
     * @brief Get singleton instance
     */
    static OTAHttps* getInstance();

    /**
     * @brief Initialize HTTPS transport
     * @param validatorInstance OTA validator instance
     * @return true if successful
     */
    bool begin(OTAValidator* validatorInstance);

    /**
     * @brief Stop and cleanup
     */
    void stop();

    // ============================================
    // CONFIGURATION
    // ============================================

    /**
     * @brief Set GitHub repository configuration
     */
    void setGitHubConfig(const GitHubConfig& config);

    /**
     * @brief Set GitHub repository (simple)
     */
    void setGitHubRepo(const String& owner, const String& repo,
                       const String& branch = "main");

    /**
     * @brief Set GitHub authentication token (for private repos)
     */
    void setGitHubToken(const String& token);

    /**
     * @brief Set connection timeout
     */
    void setConnectTimeout(uint32_t timeoutMs);

    /**
     * @brief Set read timeout
     */
    void setReadTimeout(uint32_t timeoutMs);

    /**
     * @brief Set retry parameters
     */
    void setRetryParams(uint8_t maxRetries, uint32_t delayMs);

    /**
     * @brief Set progress callback
     */
    void setProgressCallback(OTAProgressCallback callback);

    // ============================================
    // MANIFEST OPERATIONS
    // ============================================

    /**
     * @brief Fetch and parse firmware manifest from GitHub
     * @param manifest Output manifest structure
     * @return true if successful
     */
    bool fetchManifest(FirmwareManifest& manifest);

    /**
     * @brief Fetch manifest from specific URL
     * @param url Manifest URL
     * @param manifest Output manifest structure
     * @return true if successful
     */
    bool fetchManifestFromUrl(const String& url, FirmwareManifest& manifest);

    /**
     * @brief Parse manifest JSON
     * @param json JSON string
     * @param manifest Output manifest structure
     * @return true if successful
     */
    bool parseManifest(const String& json, FirmwareManifest& manifest);

    // ============================================
    // FIRMWARE DOWNLOAD
    // ============================================

    /**
     * @brief Download firmware from manifest
     * @param manifest Firmware manifest
     * @param result Output validation result
     * @return true if download and validation successful
     */
    bool downloadFirmware(const FirmwareManifest& manifest,
                         ValidationResult& result);

    /**
     * @brief Download firmware from URL
     * @param url Firmware URL
     * @param expectedSize Expected file size (0 = unknown)
     * @param expectedHash Expected SHA-256 hash (hex string, empty = skip)
     * @param signature Base64 signature (empty = skip)
     * @param result Output validation result
     * @return true if successful
     */
    bool downloadFirmwareFromUrl(const String& url, size_t expectedSize,
                                 const String& expectedHash,
                                 const String& signature,
                                 ValidationResult& result);

    /**
     * @brief Download latest release from GitHub
     * @param result Output validation result
     * @return true if successful
     */
    bool downloadLatestRelease(ValidationResult& result);

    // ============================================
    // STATUS & CONTROL
    // ============================================

    /**
     * @brief Abort current download
     */
    void abortDownload();

    /**
     * @brief Check if download is in progress
     */
    bool isDownloading() const { return downloading; }

    /**
     * @brief Get current download progress
     */
    DownloadProgress getProgress() const { return progress; }

    /**
     * @brief Get last error
     */
    OTAError getLastError() const { return lastError; }

    /**
     * @brief Get last error message
     */
    String getLastErrorMessage() const { return lastErrorMessage; }

    // ============================================
    // VERSION CHECK
    // ============================================

    /**
     * @brief Check if update is available
     * @param manifest Output manifest if update available
     * @return true if newer version available
     */
    bool checkForUpdate(FirmwareManifest& manifest);

    /**
     * @brief Get current GitHub config
     */
    GitHubConfig getGitHubConfig() const { return githubConfig; }

    // Singleton - no copy
    OTAHttps(const OTAHttps&) = delete;
    OTAHttps& operator=(const OTAHttps&) = delete;
};

#endif // OTA_HTTPS_H
