/**
 * @file OTAHttps.cpp
 * @brief HTTPS OTA Transport Implementation
 * @version 2.5.11
 * @date 2025-11-29
 *
 * v2.5.11: Private repo OTA support via GitHub API
 *          Fixed HTTP response header/body parsing
 * v2.0.0:  Switched to ESP_SSLClient (mobizt) with PSRAM support
 *          Solves "record too large" error by using PSRAM for SSL buffers
 */

#include "DebugConfig.h"  // MUST BE FIRST

#include "OTAHttps.h"
#include "JsonDocumentPSRAM.h"  // For SpiRamJsonDocument
#include <esp_heap_caps.h>
#include <mbedtls/base64.h>
// v2.5.9: ESP_SSLClient (mobizt) with PSRAM support for large SSL buffers

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

    // Get NetworkManager for network status
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

    // v2.5.9: ESP_SSLClient (mobizt) with PSRAM support
    // Uses PSRAM for SSL buffers - solves "record too large" error

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

    // Log memory status
    size_t freeDram = heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    size_t freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    LOG_OTA_DEBUG("Network mode: %s, Using ESP_SSLClient over %s\n",
                  activeMode.c_str(), usingWiFi ? "WiFiClient" : "EthernetClient");
    LOG_OTA_DEBUG("Free DRAM: %u bytes, Free PSRAM: %u bytes\n", freeDram, freePsram);

    // Check PSRAM availability (SSL buffers go here)
    if (freePsram < 50000) {
        LOG_OTA_ERROR("Insufficient PSRAM for SSL (%u bytes, need 50KB+)\n", freePsram);
        lastError = OTAError::NOT_ENOUGH_SPACE;
        lastErrorMessage = "Insufficient PSRAM for SSL";
        return false;
    }

    if (usingWiFi) {
        // ========== WiFi: ESP_SSLClient wrapping WiFiClient ==========
        // Verify WiFi is actually connected
        if (WiFi.status() != WL_CONNECTED) {
            LOG_OTA_ERROR("WiFi not connected (status: %d)\n", WiFi.status());
            return false;
        }

        LOG_OTA_DEBUG("WiFi connected, IP: %s, RSSI: %d dBm\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());

        // Create WiFiClient as base transport
        wifiBase = new WiFiClient();
        if (!wifiBase) {
            LOG_OTA_ERROR("Failed to create WiFiClient\n");
            return false;
        }

        // Create ESP_SSLClient (allocate in PSRAM if possible)
        void* ptr = heap_caps_malloc(sizeof(ESP_SSLClient), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (ptr) {
            wifiSecure = new (ptr) ESP_SSLClient();
        } else {
            wifiSecure = new ESP_SSLClient();
        }

        if (!wifiSecure) {
            LOG_OTA_ERROR("Failed to create ESP_SSLClient for WiFi\n");
            delete wifiBase;
            wifiBase = nullptr;
            return false;
        }

        // Configure ESP_SSLClient
        wifiSecure->setClient(wifiBase);
        wifiSecure->setBufferSizes(ESP_SSLCLIENT_BUFFER_SIZE, 1024);  // RX 16KB (PSRAM), TX 1KB
        wifiSecure->setCACert(GITHUB_ROOT_CA);  // Use PEM format certificate
        wifiSecure->setDebugLevel(0);  // Disable SSL debug in production
        wifiSecure->setTimeout(readTimeoutMs);

        LOG_OTA_DEBUG("ESP_SSLClient for WiFi configured, RX buffer: %d bytes (PSRAM)\n",
                      ESP_SSLCLIENT_BUFFER_SIZE);

        // Use ESP_SSLClient as the active client
        sslClient = wifiSecure;
    } else {
        // ========== Ethernet: ESP_SSLClient wrapping EthernetClient ==========
        // Create base EthernetClient
        ethBase = new EthernetClient();
        if (!ethBase) {
            LOG_OTA_ERROR("Failed to create EthernetClient\n");
            return false;
        }

        // Create ESP_SSLClient (allocate in PSRAM if possible)
        void* ptr = heap_caps_malloc(sizeof(ESP_SSLClient), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (ptr) {
            ethSecure = new (ptr) ESP_SSLClient();
        } else {
            ethSecure = new ESP_SSLClient();
        }

        if (!ethSecure) {
            LOG_OTA_ERROR("Failed to create ESP_SSLClient for Ethernet\n");
            delete ethBase;
            ethBase = nullptr;
            return false;
        }

        // Configure ESP_SSLClient
        ethSecure->setClient(ethBase);
        ethSecure->setBufferSizes(ESP_SSLCLIENT_BUFFER_SIZE, 1024);  // RX 16KB (PSRAM), TX 1KB
        ethSecure->setCACert(GITHUB_ROOT_CA);  // Use PEM format certificate
        ethSecure->setDebugLevel(0);  // Disable SSL debug in production
        ethSecure->setTimeout(readTimeoutMs);

        LOG_OTA_DEBUG("ESP_SSLClient for Ethernet configured, RX buffer: %d bytes (PSRAM)\n",
                      ESP_SSLCLIENT_BUFFER_SIZE);

        // Use ESP_SSLClient as the active client
        sslClient = ethSecure;
    }

    LOG_OTA_INFO("ESP_SSLClient initialized (PSRAM enabled)\n");
    return true;
}

void OTAHttps::cleanupSecureClient() {
    // v2.5.9: Clean up all ESP_SSLClient instances
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
    // v2.5.9: ESP_SSLClient uses milliseconds for setTimeout
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
    // v2.5.11: For private repos, use GitHub API instead of raw.githubusercontent.com
    // raw.githubusercontent.com doesn't accept authentication headers or token-in-URL
    // GitHub API: https://api.github.com/repos/{owner}/{repo}/contents/{path}?ref={branch}
    // Public repos: https://raw.githubusercontent.com/{owner}/{repo}/{branch}/{path}

    bool hasToken = githubConfig.token.length() > 0;

    if (hasToken) {
        // Private repo: Use GitHub API
        String url = "https://";
        url += OTA_GITHUB_API_HOST;  // api.github.com
        url += "/repos/";
        url += githubConfig.owner;
        url += "/";
        url += githubConfig.repo;
        url += "/contents/";
        url += path;
        url += "?ref=";
        url += githubConfig.branch;

        LOG_OTA_INFO("Private repo: using GitHub API\n");
        LOG_OTA_INFO("API URL: %s\n", url.c_str());
        return url;
    } else {
        // Public repo: Use raw.githubusercontent.com (no auth needed)
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

        LOG_OTA_INFO("Public repo: using raw URL\n");
        LOG_OTA_INFO("Raw URL: %s\n", url.c_str());
        return url;
    }
}

String OTAHttps::buildReleaseUrl(const String& tag, const String& filename) {
    // v2.5.11: For private repos, auth is handled via Authorization header in performRequest()
    // Format: https://github.com/{owner}/{repo}/releases/download/{tag}/{filename}
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

    LOG_OTA_INFO("Release URL: %s\n", url.c_str());
    if (githubConfig.token.length() > 0) {
        LOG_OTA_INFO("Private repo: auth via header\n");
    }

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
// HTTP HELPERS (Manual HTTP without HTTPClient for multi-network support)
// ============================================

// Helper to parse URL into host and path
bool parseUrl(const String& url, String& host, String& path, uint16_t& port) {
    // Format: https://host/path
    int protoEnd = url.indexOf("://");
    if (protoEnd < 0) return false;

    String protocol = url.substring(0, protoEnd);
    port = (protocol == "https") ? 443 : 80;

    int hostStart = protoEnd + 3;
    int pathStart = url.indexOf('/', hostStart);

    String hostPart;
    if (pathStart < 0) {
        hostPart = url.substring(hostStart);
        path = "/";
    } else {
        hostPart = url.substring(hostStart, pathStart);
        path = url.substring(pathStart);
    }

    // v2.5.11: Check for credentials in URL (token@host format)
    int atPos = hostPart.indexOf('@');
    if (atPos > 0) {
        // URL has credentials, extract just the host part
        host = hostPart.substring(atPos + 1);
    } else {
        host = hostPart;
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
    // v2.5.9: Check if network mode changed and reinitialize SSL client
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
    // v2.5.9: Manual HTTP request using ESP_SSLClient (mobizt) with PSRAM
    String host, path;
    uint16_t port;

    if (!parseUrl(serverUrl, host, path, port)) {
        LOG_OTA_ERROR("Invalid URL: %s\n", serverUrl.c_str());
        return -1;
    }

    LOG_OTA_DEBUG("Connecting to %s:%d%s via %s\n",
                  host.c_str(), port, path.c_str(), usingWiFi ? "WiFi" : "Ethernet");

    // Connect to server using ESP_SSLClient
    unsigned long connectStart = millis();
    bool connected = false;

    if (usingWiFi && wifiSecure) {
        // ESP_SSLClient wrapping WiFiClient
        connected = wifiSecure->connect(host.c_str(), port);
    } else if (!usingWiFi && ethSecure) {
        // ESP_SSLClient wrapping EthernetClient
        connected = ethSecure->connect(host.c_str(), port);
    }
    unsigned long connectTime = millis() - connectStart;

    if (!connected) {
        LOG_OTA_ERROR("SSL connection failed to %s:%d (took %lu ms)\n",
                      host.c_str(), port, connectTime);

        // Additional debug info
        if (usingWiFi) {
            LOG_OTA_DEBUG("WiFi status: %d, RSSI: %d dBm\n", WiFi.status(), WiFi.RSSI());
        }
        size_t freeDramFail = heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
        LOG_OTA_DEBUG("Free DRAM at failure: %u bytes\n", freeDramFail);
        return -1;
    }

    LOG_OTA_DEBUG("Connected in %lu ms\n", connectTime);

    // Build HTTP request
    String request = String(method) + " " + path + " HTTP/1.1\r\n";
    request += "Host: " + host + "\r\n";
    request += "User-Agent: ESP32-OTA/1.0\r\n";

    // v2.5.11: For GitHub API (private repos), use proper authentication
    // GitHub API requires: Authorization: token {token} (NOT Bearer!)
    // Also needs Accept header for raw content
    bool isGitHubApi = (host == String(OTA_GITHUB_API_HOST));
    bool isGitHub = (host == String(OTA_GITHUB_RELEASES_HOST));

    if (githubConfig.token.length() > 0 && (isGitHubApi || isGitHub)) {
        // GitHub API/Release authentication
        request += "Authorization: token " + githubConfig.token + "\r\n";

        if (isGitHubApi) {
            // API requests need special Accept header for raw content
            request += "Accept: application/vnd.github.v3.raw\r\n";
            LOG_OTA_DEBUG("Using GitHub API auth\n");
        } else {
            // Release downloads
            request += "Accept: application/octet-stream\r\n";
            LOG_OTA_DEBUG("Using GitHub Release auth\n");
        }
    } else {
        // Standard Accept header for public repos
        request += "Accept: application/octet-stream, application/json\r\n";
    }

    request += "Connection: close\r\n";
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

    // Check network connectivity first
    if (networkManager && !networkManager->isAvailable()) {
        lastError = OTAError::NETWORK_ERROR;
        lastErrorMessage = "Network not connected";
        LOG_OTA_ERROR("Network not connected\n");
        return false;
    }

    LOG_OTA_DEBUG("Fetching: %s\n", url.c_str());

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

    // v2.5.11: Read ALL data first (headers + body), then parse
    // This is more robust than line-by-line reading which can fail with slow connections
    String rawResponse = "";
    unsigned long timeout = millis() + readTimeoutMs;

    while ((sslClient->connected() || sslClient->available()) && millis() < timeout) {
        while (sslClient->available()) {
            char c = sslClient->read();
            rawResponse += c;
        }
        delay(1);
    }

    LOG_OTA_DEBUG("Raw response: %d bytes\n", rawResponse.length());

    // Find header/body separator: \r\n\r\n
    int bodyStart = rawResponse.indexOf("\r\n\r\n");
    String payload = "";

    if (bodyStart > 0) {
        payload = rawResponse.substring(bodyStart + 4);  // Skip \r\n\r\n
        LOG_OTA_DEBUG("Headers: %d bytes, Body: %d bytes\n", bodyStart, payload.length());
    } else {
        // Fallback: try to find JSON start directly
        int jsonStart = rawResponse.indexOf('{');
        if (jsonStart >= 0) {
            payload = rawResponse.substring(jsonStart);
            LOG_OTA_DEBUG("Found JSON at offset %d\n", jsonStart);
        } else {
            payload = rawResponse;  // Use as-is
        }
    }

    sslClient->stop();
    xSemaphoreGive(httpMutex);

    LOG_OTA_DEBUG("Received %d bytes\n", payload.length());

    // Debug: show first 100 chars of payload
    if (payload.length() > 0) {
        String preview = payload.substring(0, min((size_t)100, payload.length()));
        LOG_OTA_DEBUG("Payload preview: %s\n", preview.c_str());
    } else {
        LOG_OTA_ERROR("Empty payload received\n");
    }

    return parseManifest(payload, manifest);
}

bool OTAHttps::parseManifest(const String& json, FirmwareManifest& manifest) {
    // Allocate JSON document in PSRAM
    SpiRamJsonDocument doc;

    // v2.5.11: Find JSON start - skip any remaining headers that weren't properly skipped
    String jsonBody = json;
    int jsonStart = json.indexOf('{');
    if (jsonStart > 0) {
        jsonBody = json.substring(jsonStart);
        LOG_OTA_DEBUG("Skipped %d bytes before JSON start\n", jsonStart);
    }

    DeserializationError error = deserializeJson(doc, jsonBody);
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

    // Setup ESP_SSLClient and perform manual HTTP request
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

            LOG_OTA_DEBUG("Redirect %d -> %s\n", httpCode, location.c_str());

            // v2.5.7: Full SSL client reinit for redirect to different host
            // This ensures clean buffers and proper memory allocation
            cleanupSecureClient();

            // Small delay to allow memory to be freed
            vTaskDelay(pdMS_TO_TICKS(100));

            // Follow redirect with fresh SSL client
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

    // Read headers and get content length
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
    unsigned long lastDataTime = startTime;  // v2.5.6: Track last data received time

    // v2.5.6: Robust download loop - don't exit just because connected() is false
    // Keep reading as long as: (1) data available, OR (2) connected and not timed out
    while (progress.bytesDownloaded < contentLength) {
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
                lastDataTime = millis();  // Reset timeout on data received

                // Write to OTA partition
                if (!writeOTAData(downloadBuffer, bytesRead)) {
                    abortOTA();
                    sslClient->stop();
                    downloading = false;
                    progress.inProgress = false;
                    xSemaphoreGive(httpMutex);
                    return false;
                }

                // Yield after flash write to prevent watchdog timeout
                vTaskDelay(1);

                // Update hash
                if (validator) {
                    validator->hashUpdate(downloadBuffer, bytesRead);
                    vTaskDelay(1);  // Yield after hash computation
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

                // Log progress every 10%
                if (progress.percent % 10 == 0) {
                    static uint8_t lastLoggedPercent = 0;
                    if (progress.percent != lastLoggedPercent) {
                        LOG_OTA_DEBUG("Progress: %u%% (%u / %u bytes)\n",
                                      progress.percent, progress.bytesDownloaded, contentLength);
                        lastLoggedPercent = progress.percent;
                    }
                }
            }
        } else {
            // No data available - check timeout
            unsigned long noDataDuration = millis() - lastDataTime;

            // If no data for too long, check if connection is still alive
            if (noDataDuration > readTimeoutMs) {
                LOG_OTA_ERROR("Download timeout: no data for %lu ms\n", noDataDuration);
                break;
            }

            // If disconnected and no data, exit
            if (!sslClient->connected() && noDataDuration > 1000) {
                LOG_OTA_WARN("Connection closed, no data for %lu ms\n", noDataDuration);
                break;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));  // Feed watchdog - increased from 1 to 10ms
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
        // v2.5.9: Decode signature from hex format (DER/ASN.1 encoded)
        uint8_t* sigBytes = nullptr;
        size_t sigLen = 0;

        if (signature.length() > 0) {
            // Hex string is 2 chars per byte
            sigLen = signature.length() / 2;
            sigBytes = (uint8_t*)malloc(sigLen);
            if (sigBytes) {
                bool decodeOk = true;
                for (size_t i = 0; i < sigLen && decodeOk; i++) {
                    char hex[3] = {signature[i*2], signature[i*2+1], 0};
                    char* endptr;
                    long val = strtol(hex, &endptr, 16);
                    if (*endptr != '\0') {
                        decodeOk = false;
                    } else {
                        sigBytes[i] = (uint8_t)val;
                    }
                }
                if (!decodeOk) {
                    LOG_OTA_ERROR("Hex signature decode failed\n");
                    free(sigBytes);
                    sigBytes = nullptr;
                    sigLen = 0;
                } else {
                    LOG_OTA_INFO("Signature decoded: %u bytes (DER format)\n", sigLen);
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
