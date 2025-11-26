/**
 * @file OTAConfig.h
 * @brief OTA Update System Configuration
 * @version 1.0.0
 * @date 2025-11-26
 *
 * Configuration constants, partition layout, and GitHub integration settings
 * for the SRT-MGATE-1210 OTA Update System.
 *
 * Features:
 * - HTTPS OTA from GitHub (public/private repos)
 * - BLE OTA transport
 * - ECDSA P-256 signature verification
 * - SHA-256 checksum validation
 * - Anti-rollback protection
 * - Automatic rollback on boot failure
 */

#ifndef OTA_CONFIG_H
#define OTA_CONFIG_H

#include <Arduino.h>

// ============================================
// OTA VERSION & IDENTIFICATION
// ============================================
#define OTA_SYSTEM_VERSION "1.0.0"
#define OTA_FIRMWARE_MAGIC "SRTA"      // 4-byte magic header for firmware packages
#define OTA_FIRMWARE_MAGIC_LEN 4

// ============================================
// PARTITION LAYOUT (16MB Flash)
// ============================================
// Based on OTA_UPDATE.md partition scheme
//
// | Partition    | Offset     | Size   |
// |--------------|------------|--------|
// | Bootloader   | 0x0000     | 32 KB  |
// | Partition Tbl| 0x8000     | 4 KB   |
// | NVS          | 0x9000     | 48 KB  |
// | OTA Data     | 0x15000    | 4 KB   |
// | Factory App  | 0x20000    | 3 MB   |
// | OTA_0 App    | 0x320000   | 3 MB   |
// | OTA_1 App    | 0x620000   | 3 MB   |
// | LittleFS     | 0x920000   | 7 MB   |

#define OTA_PARTITION_SIZE      (3 * 1024 * 1024)   // 3MB per OTA partition
#define OTA_MAX_FIRMWARE_SIZE   (OTA_PARTITION_SIZE - 4096)  // Leave 4KB safety margin

// ============================================
// GITHUB INTEGRATION
// ============================================
// Firmware delivery via GitHub Releases or raw repository

// GitHub API settings
#define OTA_GITHUB_API_HOST         "api.github.com"
#define OTA_GITHUB_RAW_HOST         "raw.githubusercontent.com"
#define OTA_GITHUB_RELEASES_HOST    "github.com"

// Repository configuration (configurable via ota_config.json)
#define OTA_DEFAULT_GITHUB_OWNER    "GifariKemal"
#define OTA_DEFAULT_GITHUB_REPO     "GatewaySuriotaPOC-Firmware"
#define OTA_DEFAULT_GITHUB_BRANCH   "main"

// File paths in repository
#define OTA_MANIFEST_FILENAME       "firmware_manifest.json"
#define OTA_FIRMWARE_FILENAME       "firmware.bin"
#define OTA_SIGNATURE_FILENAME      "firmware.sig"

// URL Templates (filled at runtime)
// Raw: https://raw.githubusercontent.com/{owner}/{repo}/{branch}/{path}
// Release: https://github.com/{owner}/{repo}/releases/download/{tag}/{filename}

// ============================================
// HTTPS/TLS CONFIGURATION
// ============================================
#define OTA_HTTPS_TIMEOUT_MS        60000    // 60 seconds for download timeout
#define OTA_HTTPS_CONNECT_TIMEOUT   10000    // 10 seconds for connection
#define OTA_HTTPS_BUFFER_SIZE       4096     // Download buffer size (PSRAM)
#define OTA_HTTPS_MAX_REDIRECTS     3        // Maximum HTTP redirects to follow

// GitHub requires TLS 1.2+
#define OTA_TLS_MIN_VERSION         MBEDTLS_SSL_MINOR_VERSION_3  // TLS 1.2

// Root CA for GitHub (DigiCert Global Root CA)
// This will be loaded from file or embedded
#define OTA_GITHUB_ROOT_CA_FILE     "/github_root_ca.pem"

// ============================================
// BLE OTA CONFIGURATION
// ============================================
// Service and Characteristic UUIDs (from OTA_UPDATE.md)
#define OTA_BLE_SERVICE_UUID        "0000FF00-0000-1000-8000-00805F9B34FB"
#define OTA_BLE_CONTROL_UUID        "0000FF01-0000-1000-8000-00805F9B34FB"  // Write
#define OTA_BLE_DATA_UUID           "0000FF02-0000-1000-8000-00805F9B34FB"  // Write No Response
#define OTA_BLE_STATUS_UUID         "0000FF03-0000-1000-8000-00805F9B34FB"  // Notify

// BLE OTA Commands (from OTA_UPDATE.md)
#define OTA_BLE_CMD_START           0x50    // Initialize OTA
#define OTA_BLE_CMD_DATA            0x51    // Send firmware chunk
#define OTA_BLE_CMD_VERIFY          0x52    // Verify complete firmware
#define OTA_BLE_CMD_APPLY           0x53    // Apply update and reboot
#define OTA_BLE_CMD_ABORT           0x54    // Cancel OTA process
#define OTA_BLE_CMD_STATUS          0x55    // Get OTA progress

// BLE OTA Response Codes
#define OTA_BLE_RSP_OK              0x00
#define OTA_BLE_RSP_ERROR           0x01
#define OTA_BLE_RSP_BUSY            0x02
#define OTA_BLE_RSP_INVALID_STATE   0x03
#define OTA_BLE_RSP_CHECKSUM_FAIL   0x04
#define OTA_BLE_RSP_SIGNATURE_FAIL  0x05
#define OTA_BLE_RSP_FLASH_ERROR     0x06
#define OTA_BLE_RSP_SIZE_ERROR      0x07

// BLE Transfer settings
#define OTA_BLE_CHUNK_SIZE          244     // MTU - 3 bytes overhead (matches BLEManager)
#define OTA_BLE_ACK_INTERVAL        10      // ACK every 10 chunks
#define OTA_BLE_TIMEOUT_MS          300000  // 5 minutes total timeout
#define OTA_BLE_CHUNK_TIMEOUT_MS    5000    // 5 seconds per chunk timeout

// ============================================
// SECURITY CONFIGURATION
// ============================================
// Signature verification (ECDSA P-256 / secp256r1)
#define OTA_SIGNATURE_ALGORITHM     MBEDTLS_ECP_DP_SECP256R1
#define OTA_SIGNATURE_SIZE          64      // ECDSA P-256 signature size (r + s)
#define OTA_HASH_SIZE               32      // SHA-256 hash size

// Firmware package header structure (from OTA_UPDATE.md)
// Total header: 32 bytes magic/version/size + 64 bytes signature = 96 bytes
#define OTA_HEADER_SIZE             32
#define OTA_PACKAGE_HEADER_SIZE     (OTA_HEADER_SIZE + OTA_SIGNATURE_SIZE)

// Public key storage
#define OTA_PUBLIC_KEY_FILE         "/ota_public_key.pem"
#define OTA_PUBLIC_KEY_SIZE         91      // Compressed public key size

// Enable/disable signature verification (ALWAYS enable in production!)
#define OTA_VERIFY_SIGNATURE        1
#define OTA_VERIFY_CHECKSUM         1

// Anti-rollback protection
#define OTA_ALLOW_DOWNGRADE         0       // 0 = reject downgrades, 1 = allow
#define OTA_BOOT_VALIDATION_MS      60000   // 60 seconds to validate new firmware

// ============================================
// STATE MACHINE & TIMING
// ============================================
// OTA States
enum class OTAState : uint8_t {
    IDLE = 0,           // No OTA in progress
    CHECKING = 1,       // Checking for update
    DOWNLOADING = 2,    // Downloading firmware
    VALIDATING = 3,     // Validating signature/checksum
    APPLYING = 4,       // Writing to flash
    REBOOTING = 5,      // Preparing to reboot
    ERROR = 6           // Error occurred
};

// OTA Error Codes (700-799 range for OTA domain)
enum class OTAError : uint16_t {
    NONE = 700,
    NETWORK_ERROR = 701,
    SERVER_ERROR = 702,
    INVALID_URL = 703,
    INVALID_FIRMWARE = 704,
    SIGNATURE_FAILED = 705,
    CHECKSUM_FAILED = 706,
    FLASH_ERROR = 707,
    NOT_ENOUGH_SPACE = 708,
    DOWNGRADE_REJECTED = 709,
    TIMEOUT = 710,
    ABORTED = 711,
    ALREADY_UP_TO_DATE = 712,
    MANIFEST_ERROR = 713,
    GITHUB_AUTH_FAILED = 714,
    PARTITION_ERROR = 715,
    ROLLBACK_REQUIRED = 716,
    BLE_TRANSFER_ERROR = 717,
    INVALID_STATE = 718,
    MEMORY_ERROR = 719,
    UNKNOWN = 799
};

// Update check settings
#define OTA_CHECK_INTERVAL_HOURS    24      // Check every 24 hours (0 = manual only)
#define OTA_RETRY_COUNT             3       // Retry failed downloads
#define OTA_RETRY_DELAY_MS          30000   // 30 seconds between retries

// Progress reporting
#define OTA_PROGRESS_INTERVAL_MS    1000    // Report progress every 1 second
#define OTA_PROGRESS_INTERVAL_BYTES 65536   // Report progress every 64KB

// ============================================
// NVS STORAGE KEYS
// ============================================
#define OTA_NVS_NAMESPACE           "ota"
#define OTA_NVS_BOOT_COUNT          "boot_count"
#define OTA_NVS_LAST_VERSION        "last_ver"
#define OTA_NVS_BUILD_NUMBER        "build_num"
#define OTA_NVS_UPDATE_TIME         "update_time"
#define OTA_NVS_ROLLBACK_COUNT      "rollback_cnt"

// Boot validation
#define OTA_MAX_BOOT_FAILURES       3       // Rollback after 3 failed boots

// ============================================
// CONFIG FILE STRUCTURE
// ============================================
// File: /ota_config.json
/*
{
  "ota_enabled": true,
  "github": {
    "owner": "GifariKemal",
    "repo": "GatewaySuriotaPOC-Firmware",
    "branch": "main",
    "token": "",  // Optional: For private repos
    "use_releases": true
  },
  "server": {
    "check_interval_hours": 24,
    "retry_count": 3,
    "retry_delay_ms": 30000,
    "timeout_ms": 60000
  },
  "update": {
    "auto_update": false,
    "allowed_hours": { "start": 2, "end": 5 },
    "require_ethernet": false
  },
  "security": {
    "verify_signature": true,
    "allow_downgrade": false,
    "require_https": true
  },
  "ble_ota": {
    "enabled": true,
    "require_button_press": true,
    "timeout_ms": 300000
  }
}
*/

// ============================================
// FIRMWARE MANIFEST STRUCTURE
// ============================================
// File: firmware_manifest.json (on GitHub)
/*
{
  "version": "2.4.0",
  "build_number": 240,
  "release_date": "2025-11-26T10:00:00Z",
  "min_version": "2.0.0",
  "changelog": [
    "Added OTA update support",
    "Fixed memory leak",
    "Improved performance"
  ],
  "firmware": {
    "filename": "firmware.bin",
    "size": 1856000,
    "sha256": "a1b2c3d4e5f6...",
    "signature": "base64_encoded_signature..."
  },
  "mandatory": false
}
*/

// ============================================
// FIRMWARE PACKAGE HEADER STRUCTURE
// ============================================
#pragma pack(push, 1)
struct OTAFirmwareHeader {
    char magic[4];          // "SRTA"
    uint8_t majorVersion;   // Major version
    uint8_t minorVersion;   // Minor version
    uint8_t patchVersion;   // Patch version
    uint8_t reserved1;      // Reserved (alignment)
    uint32_t buildNumber;   // Build number (always incrementing)
    uint32_t firmwareSize;  // Size of firmware binary (excluding header)
    uint32_t headerCRC;     // CRC32 of header (excluding this field)
    uint16_t flags;         // Feature flags
    uint16_t reserved2;     // Reserved (alignment)
    uint8_t reserved3[8];   // Reserved for future use
    // Total: 32 bytes
};

struct OTAPackageHeader {
    OTAFirmwareHeader header;           // 32 bytes
    uint8_t signature[OTA_SIGNATURE_SIZE];  // 64 bytes ECDSA signature
    // Total: 96 bytes
};
#pragma pack(pop)

// Header flags
#define OTA_FLAG_MANDATORY      0x0001  // Mandatory update
#define OTA_FLAG_ENCRYPTED      0x0002  // Firmware is encrypted (future)
#define OTA_FLAG_COMPRESSED     0x0004  // Firmware is compressed (future)
#define OTA_FLAG_DELTA          0x0008  // Delta update (future)

// ============================================
// CALLBACK TYPES
// ============================================
typedef void (*OTAProgressCallback)(uint8_t progress, size_t current, size_t total);
typedef void (*OTACompletionCallback)(bool success, OTAError error, const char* message);
typedef void (*OTAStateCallback)(OTAState oldState, OTAState newState);

// ============================================
// LOGGING MACROS
// ============================================
// These will be defined properly in the implementation files
// after including DebugConfig.h

#ifndef LOG_OTA_ERROR
#define LOG_OTA_ERROR(fmt, ...) LOG_ERROR_F("OTA", fmt, ##__VA_ARGS__)
#define LOG_OTA_WARN(fmt, ...)  LOG_WARN_F("OTA", fmt, ##__VA_ARGS__)
#define LOG_OTA_INFO(fmt, ...)  LOG_INFO_F("OTA", fmt, ##__VA_ARGS__)
#define LOG_OTA_DEBUG(fmt, ...) LOG_DEBUG_F("OTA", fmt, ##__VA_ARGS__)
#endif

// ============================================
// HELPER FUNCTIONS
// ============================================

/**
 * @brief Get OTA state name string
 */
inline const char* getOTAStateName(OTAState state) {
    switch (state) {
        case OTAState::IDLE:        return "IDLE";
        case OTAState::CHECKING:    return "CHECKING";
        case OTAState::DOWNLOADING: return "DOWNLOADING";
        case OTAState::VALIDATING:  return "VALIDATING";
        case OTAState::APPLYING:    return "APPLYING";
        case OTAState::REBOOTING:   return "REBOOTING";
        case OTAState::ERROR:       return "ERROR";
        default:                    return "UNKNOWN";
    }
}

/**
 * @brief Get OTA error description
 */
inline const char* getOTAErrorDescription(OTAError error) {
    switch (error) {
        case OTAError::NONE:                return "No error";
        case OTAError::NETWORK_ERROR:       return "Network unavailable";
        case OTAError::SERVER_ERROR:        return "Server error";
        case OTAError::INVALID_URL:         return "Invalid URL";
        case OTAError::INVALID_FIRMWARE:    return "Invalid firmware format";
        case OTAError::SIGNATURE_FAILED:    return "Signature verification failed";
        case OTAError::CHECKSUM_FAILED:     return "Checksum verification failed";
        case OTAError::FLASH_ERROR:         return "Flash write error";
        case OTAError::NOT_ENOUGH_SPACE:    return "Not enough space";
        case OTAError::DOWNGRADE_REJECTED:  return "Downgrade rejected";
        case OTAError::TIMEOUT:             return "Operation timeout";
        case OTAError::ABORTED:             return "Update aborted";
        case OTAError::ALREADY_UP_TO_DATE:  return "Already up to date";
        case OTAError::MANIFEST_ERROR:      return "Manifest parse error";
        case OTAError::GITHUB_AUTH_FAILED:  return "GitHub authentication failed";
        case OTAError::PARTITION_ERROR:     return "Partition error";
        case OTAError::ROLLBACK_REQUIRED:   return "Rollback required";
        case OTAError::BLE_TRANSFER_ERROR:  return "BLE transfer error";
        case OTAError::INVALID_STATE:       return "Invalid state";
        case OTAError::MEMORY_ERROR:        return "Memory allocation error";
        default:                            return "Unknown error";
    }
}

/**
 * @brief Validate firmware header magic
 */
inline bool isValidFirmwareMagic(const char* magic) {
    return memcmp(magic, OTA_FIRMWARE_MAGIC, OTA_FIRMWARE_MAGIC_LEN) == 0;
}

/**
 * @brief Compare firmware versions
 * @return -1 if v1 < v2, 0 if equal, 1 if v1 > v2
 */
inline int compareVersions(uint8_t major1, uint8_t minor1, uint8_t patch1,
                          uint8_t major2, uint8_t minor2, uint8_t patch2) {
    if (major1 != major2) return (major1 > major2) ? 1 : -1;
    if (minor1 != minor2) return (minor1 > minor2) ? 1 : -1;
    if (patch1 != patch2) return (patch1 > patch2) ? 1 : -1;
    return 0;
}

/**
 * @brief Parse version string to components
 * @param version Version string (e.g., "2.3.15")
 * @param major Output major version
 * @param minor Output minor version
 * @param patch Output patch version
 * @return true if parsing successful
 */
inline bool parseVersion(const char* version, uint8_t& major, uint8_t& minor, uint8_t& patch) {
    if (!version) return false;
    int maj, min, pat;
    if (sscanf(version, "%d.%d.%d", &maj, &min, &pat) == 3) {
        major = (uint8_t)maj;
        minor = (uint8_t)min;
        patch = (uint8_t)pat;
        return true;
    }
    return false;
}

#endif // OTA_CONFIG_H
