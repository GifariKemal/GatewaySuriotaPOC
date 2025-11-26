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
#include <WiFi.h>
#include <mbedtls/base64.h>

// Singleton instance
OTAHttps* OTAHttps::instance = nullptr;

// GitHub Root CA (DigiCert Global Root CA - used by GitHub)
// Valid until 2031
static const char* GITHUB_ROOT_CA = R"(
-----BEGIN CERTIFICATE-----
MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD
QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB
CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97
nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt
43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P
T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4
gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO
BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR
TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw
DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr
hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg
06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF
PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls
YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk
CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=
-----END CERTIFICATE-----
)";

// ============================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================

OTAHttps::OTAHttps() :
    secureClient(nullptr),
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

    secureClient = new WiFiClientSecure();
    if (!secureClient) {
        return false;
    }

    // Set root CA for GitHub
    secureClient->setCACert(GITHUB_ROOT_CA);

    // Set timeouts
    secureClient->setTimeout(readTimeoutMs / 1000);

    return true;
}

void OTAHttps::cleanupSecureClient() {
    if (secureClient) {
        secureClient->stop();
        delete secureClient;
        secureClient = nullptr;
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
    if (secureClient) {
        secureClient->setTimeout(timeoutMs / 1000);
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
// HTTP HELPERS
// ============================================

bool OTAHttps::setupHttpClient(const String& url) {
    httpClient.end();  // Cleanup previous

    if (!secureClient) {
        initSecureClient();
    }

    httpClient.begin(*secureClient, url);
    httpClient.setConnectTimeout(connectTimeoutMs);
    httpClient.setTimeout(readTimeoutMs);

    // Set headers
    httpClient.addHeader("User-Agent", "ESP32-OTA/1.0");
    httpClient.addHeader("Accept", "application/octet-stream, application/json");

    // Add auth header if token provided
    addAuthHeader();

    return true;
}

void OTAHttps::addAuthHeader() {
    if (githubConfig.token.length() > 0) {
        String authHeader = "Bearer " + githubConfig.token;
        httpClient.addHeader("Authorization", authHeader);
    }
}

int OTAHttps::performRequest(const char* method) {
    int httpCode;
    if (strcmp(method, "GET") == 0) {
        httpCode = httpClient.GET();
    } else if (strcmp(method, "HEAD") == 0) {
        httpCode = httpClient.sendRequest("HEAD");
    } else {
        return -1;
    }
    return httpCode;
}

bool OTAHttps::followRedirects(String& finalUrl) {
    int redirectCount = 0;
    while (redirectCount < OTA_HTTPS_MAX_REDIRECTS) {
        int httpCode = performRequest("HEAD");

        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_PARTIAL_CONTENT) {
            return true;
        }

        if (httpCode == HTTP_CODE_MOVED_PERMANENTLY ||
            httpCode == HTTP_CODE_FOUND ||
            httpCode == HTTP_CODE_SEE_OTHER ||
            httpCode == HTTP_CODE_TEMPORARY_REDIRECT) {

            String location = httpClient.header("Location");
            if (location.length() == 0) {
                return false;
            }

            finalUrl = location;
            httpClient.end();
            setupHttpClient(finalUrl);
            redirectCount++;
        } else {
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

    xSemaphoreTake(httpMutex, portMAX_DELAY);

    setupHttpClient(url);
    int httpCode = performRequest("GET");

    if (httpCode != HTTP_CODE_OK) {
        lastError = OTAError::SERVER_ERROR;
        lastErrorMessage = "Manifest fetch failed: HTTP " + String(httpCode);
        LOG_OTA_ERROR("Manifest fetch failed: %d\n", httpCode);
        httpClient.end();
        xSemaphoreGive(httpMutex);
        return false;
    }

    String payload = httpClient.getString();
    httpClient.end();
    xSemaphoreGive(httpMutex);

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

    // Setup HTTP client
    setupHttpClient(url);

    // Perform request
    int httpCode = httpClient.GET();

    if (httpCode != HTTP_CODE_OK) {
        lastError = OTAError::SERVER_ERROR;
        lastErrorMessage = "Download failed: HTTP " + String(httpCode);
        LOG_OTA_ERROR("Download failed: %d\n", httpCode);
        httpClient.end();
        downloading = false;
        progress.inProgress = false;
        xSemaphoreGive(httpMutex);
        return false;
    }

    // Get content length
    size_t contentLength = httpClient.getSize();
    if (contentLength <= 0 && expectedSize > 0) {
        contentLength = expectedSize;
    }

    if (contentLength == 0) {
        lastError = OTAError::SERVER_ERROR;
        lastErrorMessage = "Unknown content length";
        httpClient.end();
        downloading = false;
        progress.inProgress = false;
        xSemaphoreGive(httpMutex);
        return false;
    }

    progress.totalBytes = contentLength;

    // Begin OTA partition
    if (!beginOTAPartition(contentLength)) {
        httpClient.end();
        downloading = false;
        progress.inProgress = false;
        xSemaphoreGive(httpMutex);
        return false;
    }

    // Start hash computation if validator available
    if (validator) {
        validator->hashBegin();
    }

    // Get stream
    Stream* stream = httpClient.getStreamPtr();
    unsigned long startTime = millis();
    unsigned long lastProgressTime = startTime;

    // Download loop
    while (httpClient.connected() && progress.bytesDownloaded < contentLength) {
        // Check abort
        if (abortRequested) {
            lastError = OTAError::ABORTED;
            lastErrorMessage = "Download aborted by user";
            abortOTA();
            httpClient.end();
            downloading = false;
            progress.inProgress = false;
            xSemaphoreGive(httpMutex);
            return false;
        }

        size_t available = stream->available();
        if (available > 0) {
            size_t toRead = min(available, bufferSize);
            size_t bytesRead = stream->readBytes(downloadBuffer, toRead);

            if (bytesRead > 0) {
                // Write to OTA partition
                if (!writeOTAData(downloadBuffer, bytesRead)) {
                    abortOTA();
                    httpClient.end();
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

    httpClient.end();

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
