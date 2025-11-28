/**
 * @file OTAHttps.cpp
 * @brief HTTPS OTA Transport Implementation
 * @version 1.0.0
 * @date 2025-11-26
 */

#include "DebugConfig.h"  // MUST BE FIRST

#include "OTAHttps.h"
#include "JsonDocumentPSRAM.h"  // For SpiRamJsonDocument
#include <esp_heap_caps.h>
#include <mbedtls/base64.h>
// v2.5.3: Unified SSLClient (BearSSL) for both WiFi and Ethernet

// v2.5.3: Trust anchors moved to GitHubTrustAnchors.h

// Singleton instance
OTAHttps* OTAHttps::instance = nullptr;

// ============================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================

OTAHttps::OTAHttps() :
    sslClient(nullptr),
    wifiBase(nullptr),
    wifiSecure(nullptr),
    ethBase(nullptr),
    ethSecure(nullptr),
    networkManager(nullptr),
    usingWiFi(true),
    connectTimeoutMs(OTA_HTTPS_CONNECT_TIMEOUT),
    readTimeoutMs(OTA_HTTPS_TIMEOUT_MS),
    maxRetries(OTA_RETRY_COUNT),
    retryDelayMs(OTA_RETRY_DELAY_MS),
    downloading(false),
    abortRequested(false),
    lastError(OTAError::NONE),
    updatePartition(nullptr),
    otaHandle(0),
    otaStarted(false),
    validator(nullptr),
    progressCallback(nullptr),
    downloadBuffer(nullptr),
    bufferSize(OTA_HTTPS_BUFFER_SIZE)
{
    httpMutex = xSemaphoreCreateMutex();

    // v2.5.3: Get NetworkManager for network status
    networkManager = NetworkMgr::getInstance();

    // Set default GitHub config
    githubConfig.owner = OTA_DEFAULT_GITHUB_OWNER;
    githubConfig.repo = OTA_DEFAULT_GITHUB_REPO;
    githubConfig.branch = OTA_DEFAULT_GITHUB_BRANCH;
    githubConfig.useReleases = true;
}

OTAHttps::~OTAHttps() {
    stop();
    if (httpMutex) {
        vSemaphoreDelete(httpMutex);
        httpMutex = nullptr;
    }
}

// ============================================
// SINGLETON
// ============================================

OTAHttps* OTAHttps::getInstance() {
    if (!instance) {
        void* ptr = heap_caps_malloc(sizeof(OTAHttps), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (ptr) {
            instance = new (ptr) OTAHttps();
        } else {
            instance = new OTAHttps();
        }
    }
    return instance;
}

// ============================================
// INITIALIZATION
// ============================================

bool OTAHttps::begin(OTAValidator* validatorInstance) {
    validator = validatorInstance;

    if (!initSecureClient()) {
        LOG_OTA_ERROR("Failed to init secure client\n");
        return false;
    }

    if (!allocateBuffer()) {
        LOG_OTA_ERROR("Failed to allocate download buffer\n");
        return false;
    }

    LOG_OTA_INFO("HTTPS OTA transport initialized\n");
    return true;
}

void OTAHttps::stop() {
    abortDownload();
    cleanupSecureClient();
    freeBuffer();
    validator = nullptr;
}

bool OTAHttps::initSecureClient() {
    cleanupSecureClient();

    // v2.5.3: Unified SSLClient (BearSSL) for both WiFi and Ethernet
    // WiFi -> SSLClient wrapping WiFiClient
    // Ethernet -> SSLClient wrapping EthernetClient

    // Get NetworkManager instance
    if (!networkManager) {
        networkManager = NetworkMgr::getInstance();
    }

    if (!networkManager) {
        LOG_OTA_ERROR("NetworkManager not available\n");
        return false;
    }

    // Detect which network interface is active
    String activeMode = networkManager->getCurrentMode();
    LOG_OTA_INFO("Network mode: %s\n", activeMode.c_str());

    // Check if using WiFi or Ethernet
    usingWiFi = (activeMode == "WIFI" || activeMode == "WiFi");

    if (!IS_PRODUCTION_MODE()) {
        Serial.printf("[OTA DEBUG] Network mode: %s, Using SSLClient (BearSSL) over %s\n",
                      activeMode.c_str(),
                      usingWiFi ? "WiFiClient" : "EthernetClient");
    }

    if (usingWiFi) {
        // ========== WiFi: Use SSLClient wrapping WiFiClient (BearSSL) ==========
        // v2.5.3: Changed from WiFiClientSecure (mbedTLS) to SSLClient (BearSSL)
        // BearSSL is much more memory efficient (~8KB vs ~32KB for mbedTLS)

        // Verify WiFi is actually connected
        if (WiFi.status() != WL_CONNECTED) {
            LOG_OTA_ERROR("WiFi not connected (status: %d)\n", WiFi.status());
            return false;
        }

        if (!IS_PRODUCTION_MODE()) {
            Serial.printf("[OTA DEBUG] WiFi connected, IP: %s, RSSI: %d dBm\n",
                          WiFi.localIP().toString().c_str(), WiFi.RSSI());
        }

        // Check available DRAM
        size_t freeDram = heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
        if (!IS_PRODUCTION_MODE()) {
            Serial.printf("[OTA DEBUG] Free DRAM before SSL: %u bytes\n", freeDram);
        }

        // BearSSL needs less memory, lower threshold to 15KB
        if (freeDram < 15000) {
            LOG_OTA_ERROR("Insufficient DRAM for SSL (%u bytes, need 15KB+)\n", freeDram);
            lastError = OTAError::NOT_ENOUGH_SPACE;
            lastErrorMessage = "Insufficient memory for SSL";
            return false;
        }

        // Create WiFiClient as base transport (not WiFiClientSecure!)
        wifiBase = new WiFiClient();
        if (!wifiBase) {
            LOG_OTA_ERROR("Failed to create WiFiClient\n");
            return false;
        }

        // Wrap WiFiClient with SSLClient (BearSSL) - same as Ethernet
        wifiSecure = new SSLClient(*wifiBase, TAs, (size_t)TAs_NUM, A0);
        if (!wifiSecure) {
            LOG_OTA_ERROR("Failed to create SSLClient for WiFi\n");
            delete wifiBase;
            wifiBase = nullptr;
            return false;
        }

        // Set timeout (SSLClient uses milliseconds)
        wifiSecure->setTimeout(readTimeoutMs);

        if (!IS_PRODUCTION_MODE()) {
            Serial.printf("[OTA DEBUG] SSLClient (BearSSL) for WiFi initialized, timeout: %lu ms\n", readTimeoutMs);
        }

        // Use SSLClient as the active client
        sslClient = wifiSecure;
    } else {
        // ========== Ethernet: Use SSLClient wrapping EthernetClient ==========
        // OPEnSLab SSLClient provides TLS over any Arduino Client

        // Create base EthernetClient
        ethBase = new EthernetClient();
        if (!ethBase) {
            LOG_OTA_ERROR("Failed to create EthernetClient\n");
            return false;
        }

        // Create SSLClient wrapping EthernetClient
        // SSLClient(Client& client, TAs, TAs_NUM, analog_pin_for_RNG)
        // Using A0 for random seed (important for SSL)
        ethSecure = new SSLClient(*ethBase, TAs, (size_t)TAs_NUM, A0);
        if (!ethSecure) {
            LOG_OTA_ERROR("Failed to create SSLClient\n");
            delete ethBase;
            ethBase = nullptr;
            return false;
        }

        // Set timeout (SSLClient uses milliseconds)
        ethSecure->setTimeout(readTimeoutMs);

        // Enable session caching for faster reconnects
        // ethSecure->setSessionCaching(true);

        // Use SSLClient as the active client
        sslClient = ethSecure;
    }

    LOG_OTA_INFO("SSL client initialized successfully\n");
    return true;
}

void OTAHttps::cleanupSecureClient() {
    // v2.5.3: Clean up all SSL clients
    sslClient = nullptr;  // Just clear the pointer, actual objects deleted below

    if (wifiSecure) {
        wifiSecure->stop();
        delete wifiSecure;
        wifiSecure = nullptr;
    }

    if (wifiBase) {
        wifiBase->stop();
        delete wifiBase;
        wifiBase = nullptr;
    }

    if (ethSecure) {
        ethSecure->stop();
        delete ethSecure;
        ethSecure = nullptr;
    }

    if (ethBase) {
        ethBase->stop();
        delete ethBase;
        ethBase = nullptr;
    }
}

bool OTAHttps::allocateBuffer() {
    freeBuffer();

    // Try PSRAM first
    downloadBuffer = (uint8_t*)heap_caps_malloc(bufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!downloadBuffer) {
        // Fallback to DRAM with smaller buffer
        bufferSize = 1024;
        downloadBuffer = (uint8_t*)malloc(bufferSize);
    }

    return downloadBuffer != nullptr;
}

void OTAHttps::freeBuffer() {
    if (downloadBuffer) {
        free(downloadBuffer);
        downloadBuffer = nullptr;
    }
    bufferSize = OTA_HTTPS_BUFFER_SIZE;
}

// ============================================
// CONFIGURATION
// ============================================

void OTAHttps::setGitHubConfig(const GitHubConfig& config) {
    githubConfig = config;
}

void OTAHttps::setGitHubRepo(const String& owner, const String& repo,
                             const String& branch) {
    githubConfig.owner = owner;
    githubConfig.repo = repo;
    githubConfig.branch = branch;
}

void OTAHttps::setGitHubToken(const String& token) {
    githubConfig.token = token;
}

void OTAHttps::setConnectTimeout(uint32_t timeoutMs) {
    connectTimeoutMs = timeoutMs;
}

void OTAHttps::setReadTimeout(uint32_t timeoutMs) {
    readTimeoutMs = timeoutMs;
    // v2.5.3: SSLClient uses milliseconds for setTimeout
    if (sslClient) {
        sslClient->setTimeout(timeoutMs);
    }
}

void OTAHttps::setRetryParams(uint8_t retries, uint32_t delayMs) {
    maxRetries = retries;
    retryDelayMs = delayMs;
}

void OTAHttps::setProgressCallback(OTAProgressCallback callback) {
    progressCallback = callback;
}

// ============================================
// URL BUILDERS
// ============================================

String OTAHttps::buildRawUrl(const String& path) {
    // https://raw.githubusercontent.com/{owner}/{repo}/{branch}/{path}
    String url = "https://";
    url += OTA_GITHUB_RAW_HOST;
    url += "/";
    url += githubConfig.owner;
    url += "/";
    url += githubConfig.repo;
    url += "/";
    url += githubConfig.branch;
    url += "/";
    url += path;
    return url;
}

String OTAHttps::buildReleaseUrl(const String& tag, const String& filename) {
    // https://github.com/{owner}/{repo}/releases/download/{tag}/{filename}
    String url = "https://";
    url += OTA_GITHUB_RELEASES_HOST;
    url += "/";
    url += githubConfig.owner;
    url += "/";
    url += githubConfig.repo;
    url += "/releases/download/";
    url += tag;
    url += "/";
    url += filename;
    return url;
}

String OTAHttps::buildApiUrl(const String& endpoint) {
    // https://api.github.com/repos/{owner}/{repo}/{endpoint}
    String url = "https://";
    url += OTA_GITHUB_API_HOST;
    url += "/repos/";
    url += githubConfig.owner;
    url += "/";
    url += githubConfig.repo;
    url += "/";
    url += endpoint;
    return url;
}

// ============================================
// HTTP HELPERS (v2.5.3: Manual HTTP without HTTPClient for multi-network support)
// ============================================

// Helper to parse URL into host and path
bool parseUrl(const String& url, String& host, String& path, uint16_t& port) {
    // Expected format: https://host/path
    int protoEnd = url.indexOf("://");
    if (protoEnd < 0) return false;

    String protocol = url.substring(0, protoEnd);
    port = (protocol == "https") ? 443 : 80;

    int hostStart = protoEnd + 3;
    int pathStart = url.indexOf('/', hostStart);
    if (pathStart < 0) {
        host = url.substring(hostStart);
        path = "/";
    } else {
        host = url.substring(hostStart, pathStart);
        path = url.substring(pathStart);
    }

    // Check for port in host
    int portPos = host.indexOf(':');
    if (portPos > 0) {
        port = host.substring(portPos + 1).toInt();
        host = host.substring(0, portPos);
    }

    return true;
}

bool OTAHttps::setupHttpClient(const String& url) {
    // v2.5.3: Check if network mode changed and reinitialize SSL client
    if (networkManager) {
        String currentMode = networkManager->getCurrentMode();
        bool currentlyWiFi = (currentMode == "WIFI" || currentMode == "WiFi");

        // If network mode changed, reinitialize SSL client
        if (sslClient && (currentlyWiFi != usingWiFi)) {
            LOG_OTA_INFO("Network mode changed, reinitializing SSL client\n");
            cleanupSecureClient();
        }
    }

    // Initialize SSL client if needed
    if (!sslClient) {
        if (!initSecureClient()) {
            LOG_OTA_ERROR("Failed to initialize SSL client\n");
            return false;
        }
    }

    // URL will be used directly in performRequest
    serverUrl = url;
    return true;
}

int OTAHttps::performRequest(const char* method) {
    // v2.5.3: Manual HTTP request using SSLClient (BearSSL) for both WiFi and Ethernet
    String host, path;
    uint16_t port;

    if (!parseUrl(serverUrl, host, path, port)) {
        LOG_OTA_ERROR("Invalid URL: %s\n", serverUrl.c_str());
        return -1;
    }

    if (!IS_PRODUCTION_MODE()) {
        Serial.printf("[OTA DEBUG] Connecting to %s:%d%s via %s (SSLClient/BearSSL)\n",
                      host.c_str(), port, path.c_str(),
                      usingWiFi ? "WiFi" : "Ethernet");
    }

    // Connect to server using SSLClient
    unsigned long connectStart = millis();
    bool connected = false;

    if (usingWiFi && wifiSecure) {
        // SSLClient wrapping WiFiClient
        connected = wifiSecure->connect(host.c_str(), port);
    } else if (!usingWiFi && ethSecure) {
        // SSLClient wrapping EthernetClient
        connected = ethSecure->connect(host.c_str(), port);
    }
    unsigned long connectTime = millis() - connectStart;

    if (!connected) {
        LOG_OTA_ERROR("SSL connection failed to %s:%d (took %lu ms)\n",
                      host.c_str(), port, connectTime);

        // Additional debug info
        if (!IS_PRODUCTION_MODE()) {
            if (usingWiFi) {
                Serial.printf("[OTA DEBUG] WiFi status: %d, RSSI: %d dBm\n",
                              WiFi.status(), WiFi.RSSI());
            }
            size_t freeDram = heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
            Serial.printf("[OTA DEBUG] Free DRAM at failure: %u bytes\n", freeDram);
        }
        return -1;
    }

    if (!IS_PRODUCTION_MODE()) {
        Serial.printf("[OTA DEBUG] Connected in %lu ms\n", connectTime);
    }

    // Build HTTP request
    String request = String(method) + " " + path + " HTTP/1.1\r\n";
    request += "Host: " + host + "\r\n";
    request += "User-Agent: ESP32-OTA/1.0\r\n";
    request += "Accept: application/octet-stream, application/json\r\n";
    request += "Connection: close\r\n";

    // Add auth header if token provided
    if (githubConfig.token.length() > 0) {
        request += "Authorization: Bearer " + githubConfig.token + "\r\n";
    }

    request += "\r\n";

    // Send request
    sslClient->print(request);

    // Wait for response
    unsigned long timeout = millis() + readTimeoutMs;
    while (sslClient->available() == 0 && millis() < timeout) {
        delay(10);
    }

    if (sslClient->available() == 0) {
        LOG_OTA_ERROR("Timeout waiting for response\n");
        sslClient->stop();
        return -2;
    }

    // Read status line
    String statusLine = sslClient->readStringUntil('\n');

    // Parse HTTP code (e.g., "HTTP/1.1 200 OK")
    int httpCode = 0;
    int spacePos = statusLine.indexOf(' ');
    if (spacePos > 0) {
        httpCode = statusLine.substring(spacePos + 1, spacePos + 4).toInt();
    }

    return httpCode;
}

bool OTAHttps::followRedirects(String& finalUrl) {
    // v2.5.3: Manual redirect handling with SSLClient
    int redirectCount = 0;
    while (redirectCount < OTA_HTTPS_MAX_REDIRECTS) {
        int httpCode = performRequest("HEAD");

        if (httpCode == 200 || httpCode == 206) {
            sslClient->stop();
            return true;
        }

        if (httpCode == 301 || httpCode == 302 || httpCode == 303 || httpCode == 307) {
            // Read headers to find Location
            String location = "";
            while (sslClient->connected()) {
                String line = sslClient->readStringUntil('\n');
                line.trim();
                if (line.length() == 0) break;

                if (line.startsWith("Location:") || line.startsWith("location:")) {
                    location = line.substring(10);
                    location.trim();
                }
            }

            sslClient->stop();

            if (location.length() == 0) {
                return false;
            }

            finalUrl = location;
            setupHttpClient(finalUrl);
            redirectCount++;
        } else {
            sslClient->stop();
            return false;
        }
    }
    return false;
}

// ============================================
// OTA PARTITION OPERATIONS
// ============================================

bool OTAHttps::beginOTAPartition(size_t firmwareSize) {
    // Get next OTA partition
    updatePartition = esp_ota_get_next_update_partition(NULL);
    if (!updatePartition) {
        lastError = OTAError::PARTITION_ERROR;
        lastErrorMessage = "No OTA partition found";
        LOG_OTA_ERROR("No OTA partition available\n");
        return false;
    }

    // Check partition size
    if (firmwareSize > updatePartition->size) {
        lastError = OTAError::NOT_ENOUGH_SPACE;
        lastErrorMessage = "Firmware too large for partition";
        LOG_OTA_ERROR("Firmware size %u > partition size %u\n",
                     firmwareSize, updatePartition->size);
        return false;
    }

    // Begin OTA
    esp_err_t err = esp_ota_begin(updatePartition, firmwareSize, &otaHandle);
    if (err != ESP_OK) {
        lastError = OTAError::FLASH_ERROR;
        lastErrorMessage = "Failed to begin OTA";
        LOG_OTA_ERROR("esp_ota_begin failed: %s\n", esp_err_to_name(err));
        return false;
    }

    otaStarted = true;
    LOG_OTA_INFO("OTA partition ready: %s (size %u)\n",
                updatePartition->label, updatePartition->size);
    return true;
}

bool OTAHttps::writeOTAData(const uint8_t* data, size_t len) {
    if (!otaStarted || !data || len == 0) {
        return false;
    }

    esp_err_t err = esp_ota_write(otaHandle, data, len);
    if (err != ESP_OK) {
        lastError = OTAError::FLASH_ERROR;
        lastErrorMessage = "Flash write failed";
        LOG_OTA_ERROR("esp_ota_write failed: %s\n", esp_err_to_name(err));
        return false;
    }

    return true;
}

bool OTAHttps::finalizeOTA() {
    if (!otaStarted) {
        return false;
    }

    esp_err_t err = esp_ota_end(otaHandle);
    if (err != ESP_OK) {
        lastError = OTAError::FLASH_ERROR;
        lastErrorMessage = "OTA end failed";
        LOG_OTA_ERROR("esp_ota_end failed: %s\n", esp_err_to_name(err));
        otaStarted = false;
        return false;
    }

    // Set boot partition
    err = esp_ota_set_boot_partition(updatePartition);
    if (err != ESP_OK) {
        lastError = OTAError::PARTITION_ERROR;
        lastErrorMessage = "Failed to set boot partition";
        LOG_OTA_ERROR("esp_ota_set_boot_partition failed: %s\n", esp_err_to_name(err));
        otaStarted = false;
        return false;
    }

    otaStarted = false;
    LOG_OTA_INFO("OTA finalized, boot partition set to: %s\n", updatePartition->label);
    return true;
}

void OTAHttps::abortOTA() {
    if (otaStarted) {
        esp_ota_abort(otaHandle);
        otaStarted = false;
        LOG_OTA_WARN("OTA aborted\n");
    }
}

// ============================================
// MANIFEST OPERATIONS
// ============================================

bool OTAHttps::fetchManifest(FirmwareManifest& manifest) {
    String url = buildRawUrl(OTA_MANIFEST_FILENAME);
    return fetchManifestFromUrl(url, manifest);
}

bool OTAHttps::fetchManifestFromUrl(const String& url, FirmwareManifest& manifest) {
    LOG_OTA_INFO("Fetching manifest: %s\n", url.c_str());

    // v2.5.3: Check network connectivity first
    if (networkManager && !networkManager->isAvailable()) {
        lastError = OTAError::NETWORK_ERROR;
        lastErrorMessage = "Network not connected";
        LOG_OTA_ERROR("Network not connected\n");
        return false;
    }

    // v2.5.3: Using SSLClient (BearSSL) for secure connection
    if (!IS_PRODUCTION_MODE()) {
        Serial.printf("[OTA DEBUG] Fetching: %s\n", url.c_str());
    }

    xSemaphoreTake(httpMutex, portMAX_DELAY);

    if (!setupHttpClient(url)) {
        xSemaphoreGive(httpMutex);
        return false;
    }

    int httpCode = performRequest("GET");

    if (httpCode != 200) {
        lastError = OTAError::SERVER_ERROR;
        lastErrorMessage = "Manifest fetch failed: HTTP " + String(httpCode);
        LOG_OTA_ERROR("Manifest fetch failed: %d\n", httpCode);
        sslClient->stop();
        xSemaphoreGive(httpMutex);
        return false;
    }

    // Read headers until empty line
    size_t contentLength = 0;
    while (sslClient->connected()) {
        String line = sslClient->readStringUntil('\n');
        line.trim();
        if (line.length() == 0) break;  // Empty line = end of headers

        // Parse Content-Length
        if (line.startsWith("Content-Length:") || line.startsWith("content-length:")) {
            contentLength = line.substring(16).toInt();
        }
    }

    // Read body
    String payload = "";
    unsigned long timeout = millis() + readTimeoutMs;
    while (sslClient->connected() && millis() < timeout) {
        while (sslClient->available()) {
            char c = sslClient->read();
            payload += c;
            if (contentLength > 0 && payload.length() >= contentLength) break;
        }
        if (contentLength > 0 && payload.length() >= contentLength) break;
        delay(1);
    }

    sslClient->stop();
    xSemaphoreGive(httpMutex);

    if (!IS_PRODUCTION_MODE()) {
        Serial.printf("[OTA DEBUG] Received %d bytes\n", payload.length());
    }

    return parseManifest(payload, manifest);
}

bool OTAHttps::parseManifest(const String& json, FirmwareManifest& manifest) {
    // Allocate JSON document in PSRAM
    SpiRamJsonDocument doc;

    DeserializationError error = deserializeJson(doc, json);
    if (error) {
        lastError = OTAError::MANIFEST_ERROR;
        lastErrorMessage = "JSON parse error: " + String(error.c_str());
        LOG_OTA_ERROR("Manifest JSON error: %s\n", error.c_str());
        return false;
    }

    // Parse manifest fields
    manifest.version = doc["version"].as<String>();
    manifest.buildNumber = doc["build_number"] | 0;
    manifest.releaseDate = doc["release_date"].as<String>();
    manifest.minVersion = doc["min_version"].as<String>();
    manifest.mandatory = doc["mandatory"] | false;

    // Parse changelog (can be array or string)
    if (doc["changelog"].is<JsonArray>()) {
        JsonArray changelogArr = doc["changelog"].as<JsonArray>();
        manifest.changelog = "";
        for (JsonVariant v : changelogArr) {
            if (manifest.changelog.length() > 0) {
                manifest.changelog += "\n";
            }
            manifest.changelog += "- " + v.as<String>();
        }
    } else {
        manifest.changelog = doc["changelog"].as<String>();
    }

    // Parse firmware section
    JsonObject firmware = doc["firmware"].as<JsonObject>();
    if (firmware.isNull()) {
        lastError = OTAError::MANIFEST_ERROR;
        lastErrorMessage = "Missing firmware section";
        return false;
    }

    manifest.firmwareSize = firmware["size"] | 0;
    manifest.sha256Hash = firmware["sha256"].as<String>();
    manifest.signature = firmware["signature"].as<String>();

    // Build firmware URL
    String filename = firmware["filename"].as<String>();
    if (filename.length() == 0) {
        filename = OTA_FIRMWARE_FILENAME;
    }

    // Check if full URL provided, otherwise build from config
    if (firmware["url"].is<String>() && firmware["url"].as<String>().startsWith("http")) {
        manifest.firmwareUrl = firmware["url"].as<String>();
    } else if (githubConfig.useReleases) {
        // Use release URL with version as tag
        manifest.firmwareUrl = buildReleaseUrl("v" + manifest.version, filename);
    } else {
        // Use raw file URL
        manifest.firmwareUrl = buildRawUrl(filename);
    }

    manifest.valid = manifest.version.length() > 0 && manifest.firmwareSize > 0;

    LOG_OTA_INFO("Manifest parsed: v%s (build %u), size %u\n",
                manifest.version.c_str(), manifest.buildNumber, manifest.firmwareSize);

    return manifest.valid;
}

// ============================================
// FIRMWARE DOWNLOAD
// ============================================

bool OTAHttps::downloadFirmware(const FirmwareManifest& manifest,
                                ValidationResult& result) {
    return downloadFirmwareFromUrl(manifest.firmwareUrl,
                                   manifest.firmwareSize,
                                   manifest.sha256Hash,
                                   manifest.signature,
                                   result);
}

bool OTAHttps::downloadFirmwareFromUrl(const String& url, size_t expectedSize,
                                        const String& expectedHash,
                                        const String& signature,
                                        ValidationResult& result) {
    if (downloading) {
        lastError = OTAError::INVALID_STATE;
        lastErrorMessage = "Download already in progress";
        return false;
    }

    LOG_OTA_INFO("Starting download: %s\n", url.c_str());
    LOG_OTA_INFO("Expected size: %u, hash: %s\n",
                expectedSize, expectedHash.substring(0, 16).c_str());

    xSemaphoreTake(httpMutex, portMAX_DELAY);
    downloading = true;
    abortRequested = false;

    // Reset progress
    progress.bytesDownloaded = 0;
    progress.totalBytes = expectedSize;
    progress.percent = 0;
    progress.inProgress = true;

    // v2.5.3: Setup SSLClient and perform manual HTTP request
    if (!setupHttpClient(url)) {
        downloading = false;
        progress.inProgress = false;
        xSemaphoreGive(httpMutex);
        return false;
    }

    // Perform GET request with redirect handling
    String currentUrl = url;
    int redirectCount = 0;
    int httpCode = 0;

    while (redirectCount < OTA_HTTPS_MAX_REDIRECTS) {
        httpCode = performRequest("GET");

        // Handle redirects (301, 302, 303, 307, 308)
        if (httpCode == 301 || httpCode == 302 || httpCode == 303 || httpCode == 307 || httpCode == 308) {
            // Read headers to find Location
            String location = "";
            while (sslClient->connected()) {
                String line = sslClient->readStringUntil('\n');
                line.trim();
                if (line.length() == 0) break;

                if (line.startsWith("Location:") || line.startsWith("location:")) {
                    location = line.substring(10);
                    location.trim();
                }
            }

            sslClient->stop();

            if (location.length() == 0) {
                lastError = OTAError::SERVER_ERROR;
                lastErrorMessage = "Redirect without Location header";
                LOG_OTA_ERROR("Redirect %d without Location header\n", httpCode);
                downloading = false;
                progress.inProgress = false;
                xSemaphoreGive(httpMutex);
                return false;
            }

            LOG_OTA_INFO("Redirect %d -> %s\n", httpCode, location.c_str());

            if (!IS_PRODUCTION_MODE()) {
                Serial.printf("[OTA DEBUG] Following redirect to: %s\n", location.c_str());
            }

            // Follow redirect
            currentUrl = location;
            if (!setupHttpClient(currentUrl)) {
                downloading = false;
                progress.inProgress = false;
                xSemaphoreGive(httpMutex);
                return false;
            }

            redirectCount++;
            continue;
        }

        // Got final response
        break;
    }

    if (redirectCount >= OTA_HTTPS_MAX_REDIRECTS) {
        lastError = OTAError::SERVER_ERROR;
        lastErrorMessage = "Too many redirects";
        LOG_OTA_ERROR("Too many redirects (max %d)\n", OTA_HTTPS_MAX_REDIRECTS);
        sslClient->stop();
        downloading = false;
        progress.inProgress = false;
        xSemaphoreGive(httpMutex);
        return false;
    }

    if (httpCode != 200) {
        lastError = OTAError::SERVER_ERROR;
        lastErrorMessage = "Download failed: HTTP " + String(httpCode);
        LOG_OTA_ERROR("Download failed: %d\n", httpCode);
        sslClient->stop();
        downloading = false;
        progress.inProgress = false;
        xSemaphoreGive(httpMutex);
        return false;
    }

    // v2.5.3: Read headers and get content length
    size_t contentLength = 0;
    while (sslClient->connected()) {
        String line = sslClient->readStringUntil('\n');
        line.trim();
        if (line.length() == 0) break;  // Empty line = end of headers

        // Parse Content-Length
        if (line.startsWith("Content-Length:") || line.startsWith("content-length:")) {
            contentLength = line.substring(16).toInt();
        }
    }

    if (contentLength <= 0 && expectedSize > 0) {
        contentLength = expectedSize;
    }

    if (contentLength == 0) {
        lastError = OTAError::SERVER_ERROR;
        lastErrorMessage = "Unknown content length";
        sslClient->stop();
        downloading = false;
        progress.inProgress = false;
        xSemaphoreGive(httpMutex);
        return false;
    }

    progress.totalBytes = contentLength;
    LOG_OTA_INFO("Content length: %u bytes\n", contentLength);

    // Begin OTA partition
    if (!beginOTAPartition(contentLength)) {
        sslClient->stop();
        downloading = false;
        progress.inProgress = false;
        xSemaphoreGive(httpMutex);
        return false;
    }

    // Start hash computation if validator available
    if (validator) {
        validator->hashBegin();
    }

    unsigned long startTime = millis();
    unsigned long lastProgressTime = startTime;

    // v2.5.3: Download loop using SSLClient directly
    while (sslClient->connected() && progress.bytesDownloaded < contentLength) {
        // Check abort
        if (abortRequested) {
            lastError = OTAError::ABORTED;
            lastErrorMessage = "Download aborted by user";
            abortOTA();
            sslClient->stop();
            downloading = false;
            progress.inProgress = false;
            xSemaphoreGive(httpMutex);
            return false;
        }

        size_t available = sslClient->available();
        if (available > 0) {
            size_t toRead = min(available, bufferSize);
            size_t bytesRead = sslClient->readBytes(downloadBuffer, toRead);

            if (bytesRead > 0) {
                // Write to OTA partition
                if (!writeOTAData(downloadBuffer, bytesRead)) {
                    abortOTA();
                    sslClient->stop();
                    downloading = false;
                    progress.inProgress = false;
                    xSemaphoreGive(httpMutex);
                    return false;
                }

                // Update hash
                if (validator) {
                    validator->hashUpdate(downloadBuffer, bytesRead);
                }

                // Update progress
                progress.bytesDownloaded += bytesRead;
                progress.percent = (progress.bytesDownloaded * 100) / contentLength;

                // Calculate speed
                unsigned long elapsed = millis() - startTime;
                if (elapsed > 0) {
                    progress.bytesPerSecond = (progress.bytesDownloaded * 1000) / elapsed;
                    if (progress.bytesPerSecond > 0) {
                        size_t remaining = contentLength - progress.bytesDownloaded;
                        progress.estimatedSecondsRemaining = remaining / progress.bytesPerSecond;
                    }
                }

                // Progress callback
                if (progressCallback && (millis() - lastProgressTime >= OTA_PROGRESS_INTERVAL_MS)) {
                    progressCallback(progress.percent, progress.bytesDownloaded, contentLength);
                    lastProgressTime = millis();
                }
            }
        }

        vTaskDelay(1);  // Feed watchdog
    }

    sslClient->stop();

    // Verify download complete
    if (progress.bytesDownloaded != contentLength) {
        lastError = OTAError::NETWORK_ERROR;
        lastErrorMessage = "Incomplete download";
        LOG_OTA_ERROR("Incomplete: %u / %u\n", progress.bytesDownloaded, contentLength);
        abortOTA();
        downloading = false;
        progress.inProgress = false;
        xSemaphoreGive(httpMutex);
        return false;
    }

    LOG_OTA_INFO("Download complete: %u bytes in %lu ms\n",
                progress.bytesDownloaded, millis() - startTime);

    // Validate if validator available
    if (validator && (expectedHash.length() > 0 || signature.length() > 0)) {
        // Decode signature from base64 if provided
        uint8_t* sigBytes = nullptr;
        size_t sigLen = 0;

        if (signature.length() > 0) {
            // Allocate buffer for decoded signature (base64 decodes to ~75% of original)
            size_t maxSigLen = (signature.length() * 3) / 4 + 4;
            sigBytes = (uint8_t*)malloc(maxSigLen);
            if (sigBytes) {
                int ret = mbedtls_base64_decode(sigBytes, maxSigLen, &sigLen,
                                                (const unsigned char*)signature.c_str(),
                                                signature.length());
                if (ret != 0) {
                    LOG_OTA_ERROR("Base64 decode failed: %d\n", ret);
                    free(sigBytes);
                    sigBytes = nullptr;
                    sigLen = 0;
                }
            }
        }

        if (!validator->validateStreaming(expectedHash.c_str(),
                                          sigBytes, sigLen, result)) {
            if (sigBytes) free(sigBytes);
            abortOTA();
            lastError = result.error;
            lastErrorMessage = result.message;
            downloading = false;
            progress.inProgress = false;
            xSemaphoreGive(httpMutex);
            return false;
        }

        if (sigBytes) free(sigBytes);
    }

    // Finalize OTA
    if (!finalizeOTA()) {
        downloading = false;
        progress.inProgress = false;
        xSemaphoreGive(httpMutex);
        return false;
    }

    result.valid = true;
    result.error = OTAError::NONE;
    result.message = "Download and validation successful";

    downloading = false;
    progress.inProgress = false;
    lastError = OTAError::NONE;

    xSemaphoreGive(httpMutex);

    LOG_OTA_INFO("Firmware ready for reboot\n");
    return true;
}

bool OTAHttps::downloadLatestRelease(ValidationResult& result) {
    // Fetch manifest first
    FirmwareManifest manifest;
    if (!fetchManifest(manifest)) {
        result.valid = false;
        result.error = lastError;
        result.message = lastErrorMessage;
        return false;
    }

    // Download firmware
    return downloadFirmware(manifest, result);
}

// ============================================
// STATUS & CONTROL
// ============================================

void OTAHttps::abortDownload() {
    abortRequested = true;

    // Wait for download to stop
    unsigned long timeout = millis() + 5000;
    while (downloading && millis() < timeout) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    abortOTA();
}

// ============================================
// VERSION CHECK
// ============================================

bool OTAHttps::checkForUpdate(FirmwareManifest& manifest) {
    if (!fetchManifest(manifest)) {
        return false;
    }

    if (!manifest.valid) {
        lastError = OTAError::MANIFEST_ERROR;
        return false;
    }

    // Check version with validator
    if (validator) {
        uint8_t major, minor, patch;
        if (parseVersion(manifest.version.c_str(), major, minor, patch)) {
            if (!validator->isVersionNewer(major, minor, patch, manifest.buildNumber)) {
                lastError = OTAError::ALREADY_UP_TO_DATE;
                lastErrorMessage = "Already running latest version";
                LOG_OTA_INFO("Already up to date\n");
                return false;
            }
        }
    }

    LOG_OTA_INFO("Update available: v%s\n", manifest.version.c_str());
    return true;
}
