# OTA Update System - SRT-MGATE-1210

**Version:** 1.0.0 | **Created:** November 25, 2025 | **Author:** Suriota R&D Team

---

## 1. Overview

OTA (Over-The-Air) update memungkinkan firmware gateway di-update tanpa akses fisik ke perangkat. Sistem ini mendukung dua metode update:

| Method        | Use Case                    | Security                | Speed                |
| ------------- | --------------------------- | ----------------------- | -------------------- |
| **HTTPS OTA** | Remote update dari cloud    | High (TLS + Signature)  | Fast (WiFi/Ethernet) |
| **BLE OTA**   | Local update via smartphone | Medium (BLE encryption) | Slow (~50KB/s)       |

---

## 2. Architecture

### 2.1 Partition Scheme (16MB Flash)

```
┌─────────────────────────────────────────────────────────┐
│                    16MB Flash Layout                     │
├─────────────────────────────────────────────────────────┤
│  Bootloader    │  0x0000 - 0x7FFF     │    32 KB       │
├────────────────┼─────────────────────┼─────────────────┤
│  Partition Tbl │  0x8000 - 0x8FFF     │     4 KB       │
├────────────────┼─────────────────────┼─────────────────┤
│  NVS           │  0x9000 - 0x14FFF    │    48 KB       │
├────────────────┼─────────────────────┼─────────────────┤
│  OTA Data      │  0x15000 - 0x15FFF   │     4 KB       │
├────────────────┼─────────────────────┼─────────────────┤
│  Factory App   │  0x20000 - 0x31FFFF  │   3 MB         │
├────────────────┼─────────────────────┼─────────────────┤
│  OTA_0 App     │  0x320000 - 0x61FFFF │   3 MB         │
├────────────────┼─────────────────────┼─────────────────┤
│  OTA_1 App     │  0x620000 - 0x91FFFF │   3 MB         │
├────────────────┼─────────────────────┼─────────────────┤
│  LittleFS      │  0x920000 - 0xFFFFFF │   7 MB         │
└────────────────┴─────────────────────┴─────────────────┘
```

**Partition CSV:**

```csv
# Name,    Type, SubType, Offset,   Size,    Flags
nvs,       data, nvs,     0x9000,   0xC000,
otadata,   data, ota,     0x15000,  0x1000,
factory,   app,  factory, 0x20000,  0x300000,
ota_0,     app,  ota_0,   0x320000, 0x300000,
ota_1,     app,  ota_1,   0x620000, 0x300000,
littlefs,  data, spiffs,  0x920000, 0x6E0000,
```

### 2.2 Component Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      OTA Manager                             │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │ Version     │  │ Download    │  │ Update              │  │
│  │ Checker     │  │ Handler     │  │ Validator           │  │
│  └──────┬──────┘  └──────┬──────┘  └──────────┬──────────┘  │
│         │                │                     │             │
│  ┌──────▼────────────────▼─────────────────────▼──────────┐ │
│  │                   OTA State Machine                     │ │
│  │  IDLE → CHECKING → DOWNLOADING → VALIDATING → APPLYING │ │
│  └─────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
           │                    │
           ▼                    ▼
    ┌─────────────┐      ┌─────────────┐
    │ HTTPS OTA   │      │  BLE OTA    │
    │ Transport   │      │  Transport  │
    └─────────────┘      └─────────────┘
```

---

## 3. OTA Flow Diagrams

### 3.1 HTTPS OTA Flow (Remote Update)

```
┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐
│  Device  │     │  Cloud   │     │ Firmware │     │  Device  │
│ (Gateway)│     │  Server  │     │  Server  │     │ (Gateway)│
└────┬─────┘     └────┬─────┘     └────┬─────┘     └────┬─────┘
     │                │                │                │
     │ 1. Check Version               │                │
     │ ─────────────────────────────► │                │
     │                │                │                │
     │ 2. Version Info (JSON)         │                │
     │ ◄───────────────────────────── │                │
     │                │                │                │
     │ [If new version available]     │                │
     │                │                │                │
     │ 3. Request Firmware Binary     │                │
     │ ─────────────────────────────► │                │
     │                │                │                │
     │ 4. Stream Binary (chunked)     │                │
     │ ◄═══════════════════════════════                │
     │    (with progress callback)    │                │
     │                │                │                │
     │ 5. Verify Signature            │                │
     │ ────────────┐  │                │                │
     │             │  │                │                │
     │ ◄───────────┘  │                │                │
     │                │                │                │
     │ 6. Apply Update & Reboot       │                │
     │ ────────────┐  │                │                │
     │             │  │                │                │
     │ ◄───────────┘  │                │                │
     │                │                │                │
     │ 7. Report Update Status        │                │
     │ ─────────────────────────────► │                │
     │                │                │                │
```

### 3.2 BLE OTA Flow (Local Update)

```
┌──────────┐                              ┌──────────┐
│Smartphone│                              │  Device  │
│  (App)   │                              │ (Gateway)│
└────┬─────┘                              └────┬─────┘
     │                                         │
     │ 1. Connect BLE                          │
     │ ───────────────────────────────────────►│
     │                                         │
     │ 2. Request Device Info                  │
     │ ───────────────────────────────────────►│
     │                                         │
     │ 3. Device Info (version, etc)           │
     │ ◄───────────────────────────────────────│
     │                                         │
     │ 4. Start OTA Command                    │
     │ ───────────────────────────────────────►│
     │                                         │
     │ 5. OTA Ready ACK                        │
     │ ◄───────────────────────────────────────│
     │                                         │
     │ 6. Send Firmware Chunks (MTU-3 bytes)   │
     │ ═══════════════════════════════════════►│
     │    [Repeat until complete]              │
     │                                         │
     │ 7. Chunk ACK / Progress                 │
     │ ◄───────────────────────────────────────│
     │                                         │
     │ 8. Send Checksum                        │
     │ ───────────────────────────────────────►│
     │                                         │
     │ 9. Validation Result                    │
     │ ◄───────────────────────────────────────│
     │                                         │
     │ 10. Reboot Command                      │
     │ ───────────────────────────────────────►│
     │                                         │
```

### 3.3 State Machine

```
                    ┌─────────────────┐
                    │                 │
         ┌──────────│      IDLE       │◄─────────────┐
         │          │                 │              │
         │          └────────┬────────┘              │
         │                   │                       │
         │            Check Trigger                  │
         │           (Timer/Manual/                  │
         │            MQTT/BLE)                      │
         │                   │                       │
         │                   ▼                       │
         │          ┌─────────────────┐              │
         │          │                 │              │
         │          │    CHECKING     │──No Update──►│
         │          │    VERSION      │              │
         │          │                 │              │
         │          └────────┬────────┘              │
         │                   │                       │
         │           Update Available                │
         │                   │                       │
         │                   ▼                       │
         │          ┌─────────────────┐              │
         │          │                 │              │
  Error  │          │  DOWNLOADING    │──Error──────►│
    ─────┤          │                 │              │
         │          └────────┬────────┘              │
         │                   │                       │
         │            Download Complete              │
         │                   │                       │
         │                   ▼                       │
         │          ┌─────────────────┐              │
         │          │                 │              │
         │          │   VALIDATING    │──Invalid────►│
         │          │   (Checksum/    │              │
         │          │    Signature)   │              │
         │          └────────┬────────┘              │
         │                   │                       │
         │                Valid                      │
         │                   │                       │
         │                   ▼                       │
         │          ┌─────────────────┐              │
         │          │                 │              │
         │          │    APPLYING     │──Error──────►│
         │          │                 │              │
         │          └────────┬────────┘              │
         │                   │                       │
         │               Success                     │
         │                   │                       │
         │                   ▼                       │
         │          ┌─────────────────┐              │
         │          │                 │              │
         └──────────│   REBOOTING     │──────────────┘
                    │                 │
                    └─────────────────┘
```

---

## 4. Security Model

### 4.1 Security Layers

```
┌─────────────────────────────────────────────────────────────┐
│                    Security Stack                            │
├─────────────────────────────────────────────────────────────┤
│  Layer 4: Firmware Signature (ECDSA P-256)                  │
│           - Signed by Suriota private key                   │
│           - Verified by embedded public key                 │
├─────────────────────────────────────────────────────────────┤
│  Layer 3: Integrity Check (SHA-256)                         │
│           - Hash verification after download                │
│           - Prevents corruption during transfer             │
├─────────────────────────────────────────────────────────────┤
│  Layer 2: Transport Security                                │
│           - HTTPS with TLS 1.2+ (remote)                    │
│           - BLE encryption (local)                          │
├─────────────────────────────────────────────────────────────┤
│  Layer 1: Version Control                                   │
│           - Prevent downgrade attacks                       │
│           - Semantic versioning check                       │
└─────────────────────────────────────────────────────────────┘
```

### 4.2 Firmware Signing Process

```
Build Server                              Device
     │                                       │
     │  1. Compile firmware.bin              │
     │  ────────────┐                        │
     │              │                        │
     │  2. Calculate SHA-256 hash            │
     │  ────────────┐                        │
     │              │                        │
     │  3. Sign hash with private key        │
     │  ────────────┐                        │
     │              │                        │
     │  4. Create firmware package:          │
     │     ┌─────────────────────────┐       │
     │     │ Header (32 bytes)       │       │
     │     │ - Magic: "SRTA"         │       │
     │     │ - Version: x.y.z        │       │
     │     │ - Size: uint32          │       │
     │     │ - Flags: uint16         │       │
     │     ├─────────────────────────┤       │
     │     │ Signature (64 bytes)    │       │
     │     │ - ECDSA P-256           │       │
     │     ├─────────────────────────┤       │
     │     │ Firmware Binary         │       │
     │     │ - Actual .bin file      │       │
     │     └─────────────────────────┘       │
     │                                       │
     │  5. Upload to server                  │
     │  ─────────────────────────────────►   │
     │                                       │
     │                    6. Download package│
     │                    ◄──────────────────│
     │                                       │
     │                    7. Verify signature│
     │                       with public key │
     │                    ────────────┐      │
     │                                │      │
     │                    8. Flash if valid  │
     │                    ────────────┐      │
     │                                │      │
```

### 4.3 Anti-Rollback Protection

```cpp
// Version stored in NVS (non-volatile storage)
typedef struct {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
    uint32_t build_number;  // Always incrementing
} FirmwareVersion;

// Check: new_version.build_number > current_version.build_number
```

---

## 5. API Specification

### 5.1 Version Check Endpoint

**Request:**

```http
GET /api/v1/firmware/check?device_id={device_id}&current_version={version}
Host: ota.suriota.com
Authorization: Bearer {device_token}
```

**Response:**

```json
{
  "update_available": true,
  "current_version": "2.3.3",
  "latest_version": "2.4.0",
  "release_date": "2025-11-25T10:00:00Z",
  "changelog": [
    "Added OTA update support",
    "Fixed memory leak in BLE manager",
    "Improved Modbus TCP performance"
  ],
  "firmware": {
    "url": "https://ota.suriota.com/firmware/srt-mgate-1210/2.4.0/firmware.bin",
    "size": 1856000,
    "sha256": "a1b2c3d4e5f6...",
    "signature": "base64_encoded_signature..."
  },
  "mandatory": false,
  "min_battery": 30
}
```

### 5.2 BLE OTA Commands

| Command    | Code | Payload                              | Description              |
| ---------- | ---- | ------------------------------------ | ------------------------ |
| OTA_START  | 0x50 | `{total_size, chunk_size, checksum}` | Initialize OTA           |
| OTA_DATA   | 0x51 | `{seq_num, data[]}`                  | Send firmware chunk      |
| OTA_VERIFY | 0x52 | `{checksum}`                         | Verify complete firmware |
| OTA_APPLY  | 0x53 | -                                    | Apply update and reboot  |
| OTA_ABORT  | 0x54 | -                                    | Cancel OTA process       |
| OTA_STATUS | 0x55 | -                                    | Get OTA progress         |

**BLE OTA Characteristic UUIDs:**

```
OTA Service:      0000FF00-0000-1000-8000-00805F9B34FB
OTA Control:      0000FF01-0000-1000-8000-00805F9B34FB  (Write)
OTA Data:         0000FF02-0000-1000-8000-00805F9B34FB  (Write No Response)
OTA Status:       0000FF03-0000-1000-8000-00805F9B34FB  (Notify)
```

### 5.3 MQTT OTA Commands

**Topic:** `suriota/{device_id}/ota/command`

```json
{
  "action": "check_update"
}
```

**Topic:** `suriota/{device_id}/ota/command`

```json
{
  "action": "start_update",
  "version": "2.4.0",
  "url": "https://ota.suriota.com/firmware/...",
  "force": false
}
```

**Topic:** `suriota/{device_id}/ota/status`

```json
{
  "state": "downloading",
  "progress": 45,
  "current_version": "2.3.3",
  "target_version": "2.4.0",
  "error": null
}
```

---

## 6. Implementation Guide

### 6.1 File Structure

```
Main/
├── OTAManager.h           # Main OTA manager class
├── OTAManager.cpp
├── OTAHttps.h             # HTTPS transport
├── OTAHttps.cpp
├── OTABle.h               # BLE transport
├── OTABle.cpp
├── OTAValidator.h         # Signature & checksum validation
├── OTAValidator.cpp
├── OTAConfig.h            # OTA configuration
└── ota_public_key.h       # Embedded public key for verification
```

### 6.2 Class Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                       OTAManager                             │
├─────────────────────────────────────────────────────────────┤
│ - state: OTAState                                           │
│ - currentVersion: String                                    │
│ - targetVersion: String                                     │
│ - progress: uint8_t                                         │
│ - transport: OTATransport*                                  │
│ - validator: OTAValidator*                                  │
│ - stateMutex: SemaphoreHandle_t                            │
├─────────────────────────────────────────────────────────────┤
│ + getInstance(): OTAManager*                                │
│ + begin(): bool                                             │
│ + checkForUpdate(): bool                                    │
│ + startUpdate(url): bool                                    │
│ + abortUpdate(): void                                       │
│ + getState(): OTAState                                      │
│ + getProgress(): uint8_t                                    │
│ + setProgressCallback(cb): void                             │
│ + setCompletionCallback(cb): void                           │
└─────────────────────────────────────────────────────────────┘
                            │
            ┌───────────────┴───────────────┐
            │                               │
            ▼                               ▼
┌─────────────────────┐         ┌─────────────────────┐
│    OTAHttps         │         │     OTABle          │
├─────────────────────┤         ├─────────────────────┤
│ - client: HTTPClient│         │ - bleManager: BLE*  │
│ - buffer: uint8_t*  │         │ - chunkBuffer: *    │
├─────────────────────┤         ├─────────────────────┤
│ + download(): bool  │         │ + receive(): bool   │
│ + getChunk(): size_t│         │ + processChunk()    │
└─────────────────────┘         └─────────────────────┘
            │                               │
            └───────────────┬───────────────┘
                            │
                            ▼
            ┌─────────────────────────────┐
            │       OTAValidator          │
            ├─────────────────────────────┤
            │ - publicKey: uint8_t[64]    │
            │ - sha256Ctx: mbedtls_sha256 │
            ├─────────────────────────────┤
            │ + verifySignature(): bool   │
            │ + verifyChecksum(): bool    │
            │ + updateHash(data): void    │
            └─────────────────────────────┘
```

### 6.3 Core Implementation

```cpp
// OTAManager.h
#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

enum class OTAState {
    IDLE,
    CHECKING,
    DOWNLOADING,
    VALIDATING,
    APPLYING,
    REBOOTING,
    ERROR
};

enum class OTAError {
    NONE,
    NETWORK_ERROR,
    SERVER_ERROR,
    INVALID_FIRMWARE,
    SIGNATURE_FAILED,
    CHECKSUM_FAILED,
    FLASH_ERROR,
    NOT_ENOUGH_SPACE,
    DOWNGRADE_REJECTED,
    TIMEOUT,
    ABORTED
};

typedef void (*OTAProgressCallback)(uint8_t progress, size_t current, size_t total);
typedef void (*OTACompletionCallback)(bool success, OTAError error);

class OTAManager {
private:
    static OTAManager* instance;

    OTAState state;
    OTAError lastError;
    String currentVersion;
    String targetVersion;
    uint8_t progress;
    size_t totalSize;
    size_t downloadedSize;

    SemaphoreHandle_t stateMutex;
    TaskHandle_t otaTaskHandle;

    OTAProgressCallback progressCallback;
    OTACompletionCallback completionCallback;

    // Configuration
    String serverUrl;
    String deviceToken;
    uint32_t checkIntervalMs;
    bool autoUpdate;

    OTAManager();
    ~OTAManager();

    static void otaTask(void* param);
    bool downloadFirmware(const String& url);
    bool validateFirmware();
    bool applyUpdate();
    void reportStatus();

public:
    static OTAManager* getInstance();

    bool begin();
    void stop();

    // Configuration
    void setServerUrl(const String& url);
    void setDeviceToken(const String& token);
    void setCheckInterval(uint32_t intervalMs);
    void setAutoUpdate(bool enabled);

    // Operations
    bool checkForUpdate();
    bool startUpdate(const String& url = "");
    void abortUpdate();

    // Status
    OTAState getState();
    OTAError getLastError();
    uint8_t getProgress();
    String getCurrentVersion();
    String getTargetVersion();

    // Callbacks
    void setProgressCallback(OTAProgressCallback cb);
    void setCompletionCallback(OTACompletionCallback cb);

    // Rollback
    bool markUpdateValid();
    bool rollbackToPrevious();
    bool rollbackToFactory();

    // Singleton
    OTAManager(const OTAManager&) = delete;
    OTAManager& operator=(const OTAManager&) = delete;
};

#endif // OTA_MANAGER_H
```

### 6.4 HTTPS Download Implementation

```cpp
bool OTAManager::downloadFirmware(const String& url) {
    HTTPClient http;
    http.begin(url);
    http.addHeader("Authorization", "Bearer " + deviceToken);

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        lastError = OTAError::SERVER_ERROR;
        return false;
    }

    totalSize = http.getSize();

    // Check available space
    const esp_partition_t* partition = esp_ota_get_next_update_partition(NULL);
    if (!partition || totalSize > partition->size) {
        lastError = OTAError::NOT_ENOUGH_SPACE;
        return false;
    }

    // Begin OTA update
    if (!Update.begin(totalSize)) {
        lastError = OTAError::FLASH_ERROR;
        return false;
    }

    // Download in chunks
    WiFiClient* stream = http.getStreamPtr();
    uint8_t* buffer = (uint8_t*)heap_caps_malloc(4096, MALLOC_CAP_SPIRAM);
    if (!buffer) {
        buffer = (uint8_t*)malloc(4096);
    }

    downloadedSize = 0;
    while (http.connected() && downloadedSize < totalSize) {
        size_t available = stream->available();
        if (available > 0) {
            size_t readBytes = stream->readBytes(buffer, min(available, (size_t)4096));

            if (Update.write(buffer, readBytes) != readBytes) {
                lastError = OTAError::FLASH_ERROR;
                free(buffer);
                return false;
            }

            downloadedSize += readBytes;
            progress = (downloadedSize * 100) / totalSize;

            if (progressCallback) {
                progressCallback(progress, downloadedSize, totalSize);
            }
        }
        vTaskDelay(1);  // Feed watchdog
    }

    free(buffer);
    http.end();

    return downloadedSize == totalSize;
}
```

### 6.5 Signature Verification

```cpp
// OTAValidator.cpp
#include "mbedtls/sha256.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/pk.h"
#include "ota_public_key.h"  // Contains SURIOTA_OTA_PUBLIC_KEY

bool OTAValidator::verifySignature(const uint8_t* firmware, size_t size,
                                    const uint8_t* signature, size_t sigLen) {
    // Calculate SHA-256 hash of firmware
    uint8_t hash[32];
    mbedtls_sha256(firmware, size, hash, 0);

    // Verify ECDSA signature
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);

    int ret = mbedtls_pk_parse_public_key(&pk,
                                          SURIOTA_OTA_PUBLIC_KEY,
                                          sizeof(SURIOTA_OTA_PUBLIC_KEY));
    if (ret != 0) {
        mbedtls_pk_free(&pk);
        return false;
    }

    ret = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256,
                            hash, sizeof(hash),
                            signature, sigLen);

    mbedtls_pk_free(&pk);
    return ret == 0;
}
```

---

## 7. Error Handling & Recovery

### 7.1 Error Codes

| Code | Name               | Description           | Recovery                  |
| ---- | ------------------ | --------------------- | ------------------------- |
| 0x00 | NONE               | No error              | -                         |
| 0x01 | NETWORK_ERROR      | Network unavailable   | Retry with backoff        |
| 0x02 | SERVER_ERROR       | Server returned error | Check server status       |
| 0x03 | INVALID_FIRMWARE   | Corrupt firmware      | Re-download               |
| 0x04 | SIGNATURE_FAILED   | Invalid signature     | Reject update             |
| 0x05 | CHECKSUM_FAILED    | Checksum mismatch     | Re-download               |
| 0x06 | FLASH_ERROR        | Flash write failed    | Check flash health        |
| 0x07 | NOT_ENOUGH_SPACE   | Insufficient space    | Cleanup or use smaller FW |
| 0x08 | DOWNGRADE_REJECTED | Version too old       | Use newer version         |
| 0x09 | TIMEOUT            | Operation timeout     | Retry                     |
| 0x0A | ABORTED            | User cancelled        | Manual restart            |

### 7.2 Automatic Rollback

```
                    ┌─────────────────┐
                    │  Boot Counter   │
                    │   NVS Storage   │
                    └────────┬────────┘
                             │
              ┌──────────────┴──────────────┐
              │                             │
              ▼                             ▼
    ┌─────────────────┐           ┌─────────────────┐
    │ boot_count < 3  │           │ boot_count >= 3 │
    │ (Normal Boot)   │           │ (Boot Loop!)    │
    └────────┬────────┘           └────────┬────────┘
             │                             │
             ▼                             ▼
    ┌─────────────────┐           ┌─────────────────┐
    │ Increment       │           │ Rollback to     │
    │ boot_count      │           │ Previous OTA    │
    └────────┬────────┘           └────────┬────────┘
             │                             │
             ▼                             │
    ┌─────────────────┐                    │
    │ Run Application │                    │
    │ (60s timeout)   │                    │
    └────────┬────────┘                    │
             │                             │
    ┌────────┴────────┐                    │
    │                 │                    │
    ▼                 ▼                    │
┌───────┐       ┌───────────┐              │
│Success│       │  Crash/   │              │
│       │       │  Reboot   │──────────────┘
└───┬───┘       └───────────┘
    │
    ▼
┌─────────────────┐
│ Reset           │
│ boot_count = 0  │
│ Mark Valid      │
└─────────────────┘
```

### 7.3 Recovery Implementation

```cpp
void OTAManager::handleBootValidation() {
    // Read boot counter from NVS
    Preferences prefs;
    prefs.begin("ota", false);
    uint8_t bootCount = prefs.getUChar("boot_count", 0);

    if (bootCount >= 3) {
        // Boot loop detected - rollback!
        LOG_OTA_ERROR("Boot loop detected! Rolling back...");
        rollbackToPrevious();
        prefs.putUChar("boot_count", 0);
        prefs.end();
        ESP.restart();
        return;
    }

    // Increment boot counter
    prefs.putUChar("boot_count", bootCount + 1);
    prefs.end();

    // Schedule validation check (60 seconds)
    xTimerCreate("OTA_Valid", pdMS_TO_TICKS(60000), pdFALSE,
                 NULL, [](TimerHandle_t timer) {
        OTAManager::getInstance()->markUpdateValid();
    });
}

bool OTAManager::markUpdateValid() {
    // Mark current partition as valid
    esp_ota_mark_app_valid_cancel_rollback();

    // Reset boot counter
    Preferences prefs;
    prefs.begin("ota", false);
    prefs.putUChar("boot_count", 0);
    prefs.end();

    LOG_OTA_INFO("Firmware marked as valid");
    return true;
}

bool OTAManager::rollbackToPrevious() {
    const esp_partition_t* partition = esp_ota_get_last_invalid_partition();
    if (partition) {
        esp_ota_set_boot_partition(partition);
        LOG_OTA_WARN("Rolling back to previous firmware");
        return true;
    }
    return rollbackToFactory();
}

bool OTAManager::rollbackToFactory() {
    const esp_partition_t* factory = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
    if (factory) {
        esp_ota_set_boot_partition(factory);
        LOG_OTA_WARN("Rolling back to factory firmware");
        return true;
    }
    return false;
}
```

---

## 8. Configuration

### 8.1 OTA Configuration File

**File:** `/ota_config.json`

```json
{
  "ota_enabled": true,
  "server": {
    "url": "https://ota.suriota.com/api/v1",
    "check_interval_hours": 24,
    "retry_count": 3,
    "retry_delay_ms": 30000,
    "timeout_ms": 60000
  },
  "update": {
    "auto_update": false,
    "allowed_hours": {
      "start": 2,
      "end": 5
    },
    "require_ethernet": false,
    "min_battery_percent": 30
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
```

### 8.2 BLE OTA Configuration

```cpp
// OTAConfig.h
#define OTA_BLE_SERVICE_UUID        "0000FF00-0000-1000-8000-00805F9B34FB"
#define OTA_BLE_CONTROL_UUID        "0000FF01-0000-1000-8000-00805F9B34FB"
#define OTA_BLE_DATA_UUID           "0000FF02-0000-1000-8000-00805F9B34FB"
#define OTA_BLE_STATUS_UUID         "0000FF03-0000-1000-8000-00805F9B34FB"

#define OTA_BLE_CHUNK_SIZE          244     // MTU - 3 bytes overhead
#define OTA_BLE_ACK_INTERVAL        10      // ACK every 10 chunks
#define OTA_BLE_TIMEOUT_MS          300000  // 5 minutes total timeout
```

---

## 9. Testing Guide

### 9.1 Test Scenarios

| #   | Scenario                  | Expected Result                          |
| --- | ------------------------- | ---------------------------------------- |
| 1   | Normal HTTPS update       | Success, device reboots with new version |
| 2   | Interrupted download      | Resume from last chunk or restart        |
| 3   | Invalid signature         | Reject update, stay on current version   |
| 4   | Checksum mismatch         | Re-download firmware                     |
| 5   | Power loss during flash   | Boot from previous valid partition       |
| 6   | Downgrade attempt         | Reject unless force flag                 |
| 7   | BLE update complete       | Success via mobile app                   |
| 8   | Network switch during OTA | Continue on new network                  |
| 9   | Boot loop (3x crash)      | Auto-rollback to previous                |
| 10  | Factory reset             | Boot from factory partition              |

### 9.2 Test Script (Python)

```python
# Testing/OTA_Testing/test_ota_https.py
import requests
import hashlib
import time

OTA_SERVER = "https://ota.suriota.com/api/v1"
DEVICE_ID = "TEST_DEVICE_001"
TOKEN = "test_token_123"

def test_version_check():
    """Test version check endpoint"""
    response = requests.get(
        f"{OTA_SERVER}/firmware/check",
        params={"device_id": DEVICE_ID, "current_version": "2.3.3"},
        headers={"Authorization": f"Bearer {TOKEN}"}
    )
    assert response.status_code == 200
    data = response.json()
    print(f"Update available: {data['update_available']}")
    print(f"Latest version: {data.get('latest_version', 'N/A')}")
    return data

def test_firmware_download(url):
    """Test firmware download and checksum"""
    response = requests.get(url, stream=True)
    assert response.status_code == 200

    sha256 = hashlib.sha256()
    total_size = 0

    with open("firmware_test.bin", "wb") as f:
        for chunk in response.iter_content(chunk_size=8192):
            f.write(chunk)
            sha256.update(chunk)
            total_size += len(chunk)

    print(f"Downloaded: {total_size} bytes")
    print(f"SHA256: {sha256.hexdigest()}")
    return sha256.hexdigest()

if __name__ == "__main__":
    print("=== OTA Test Suite ===")

    # Test 1: Version check
    print("\n[Test 1] Version Check")
    version_info = test_version_check()

    # Test 2: Download firmware
    if version_info.get("update_available"):
        print("\n[Test 2] Firmware Download")
        checksum = test_firmware_download(version_info["firmware"]["url"])
        assert checksum == version_info["firmware"]["sha256"]
        print("Checksum verified!")
```

### 9.3 BLE OTA Test (Python)

```python
# Testing/OTA_Testing/test_ota_ble.py
import asyncio
from bleak import BleakClient

OTA_SERVICE = "0000FF00-0000-1000-8000-00805F9B34FB"
OTA_CONTROL = "0000FF01-0000-1000-8000-00805F9B34FB"
OTA_DATA = "0000FF02-0000-1000-8000-00805F9B34FB"
OTA_STATUS = "0000FF03-0000-1000-8000-00805F9B34FB"

async def ota_update(device_address, firmware_path):
    async with BleakClient(device_address) as client:
        # Read firmware file
        with open(firmware_path, "rb") as f:
            firmware = f.read()

        total_size = len(firmware)
        chunk_size = 244

        # Start OTA
        start_cmd = bytes([0x50]) + total_size.to_bytes(4, 'little')
        await client.write_gatt_char(OTA_CONTROL, start_cmd)

        # Wait for ready
        await asyncio.sleep(1)

        # Send chunks
        offset = 0
        seq = 0
        while offset < total_size:
            chunk = firmware[offset:offset + chunk_size]
            data = bytes([0x51]) + seq.to_bytes(2, 'little') + chunk
            await client.write_gatt_char(OTA_DATA, data, response=False)

            offset += len(chunk)
            seq += 1

            # Progress
            progress = (offset * 100) // total_size
            print(f"\rProgress: {progress}%", end="")

            await asyncio.sleep(0.01)

        print("\nVerifying...")

        # Verify
        verify_cmd = bytes([0x52])
        await client.write_gatt_char(OTA_CONTROL, verify_cmd)
        await asyncio.sleep(2)

        # Apply
        apply_cmd = bytes([0x53])
        await client.write_gatt_char(OTA_CONTROL, apply_cmd)

        print("OTA Complete! Device will reboot.")

if __name__ == "__main__":
    asyncio.run(ota_update("AA:BB:CC:DD:EE:FF", "firmware.bin"))
```

---

## 10. Server Requirements

### 10.1 Backend API

```
/api/v1/
├── /firmware
│   ├── GET  /check        # Check for updates
│   ├── GET  /download     # Download firmware binary
│   ├── POST /report       # Report update status
│   └── GET  /changelog    # Get version changelog
├── /devices
│   ├── GET  /{id}/status  # Get device OTA status
│   └── POST /{id}/trigger # Trigger remote update
└── /admin
    ├── POST /upload       # Upload new firmware
    └── GET  /stats        # Update statistics
```

### 10.2 Database Schema

```sql
-- Firmware versions table
CREATE TABLE firmware_versions (
    id SERIAL PRIMARY KEY,
    version VARCHAR(20) NOT NULL,
    build_number INTEGER NOT NULL,
    release_date TIMESTAMP DEFAULT NOW(),
    file_path VARCHAR(255) NOT NULL,
    file_size INTEGER NOT NULL,
    sha256 VARCHAR(64) NOT NULL,
    signature TEXT NOT NULL,
    changelog TEXT,
    is_mandatory BOOLEAN DEFAULT FALSE,
    is_active BOOLEAN DEFAULT TRUE
);

-- Device update history
CREATE TABLE device_updates (
    id SERIAL PRIMARY KEY,
    device_id VARCHAR(50) NOT NULL,
    from_version VARCHAR(20),
    to_version VARCHAR(20),
    started_at TIMESTAMP DEFAULT NOW(),
    completed_at TIMESTAMP,
    status VARCHAR(20),  -- 'downloading', 'completed', 'failed', 'rolled_back'
    error_code VARCHAR(10),
    error_message TEXT
);
```

---

## 11. Troubleshooting

| Issue              | Cause                 | Solution                                 |
| ------------------ | --------------------- | ---------------------------------------- |
| "Not enough space" | Partition too small   | Use smaller firmware or larger partition |
| "Signature failed" | Wrong public key      | Verify key matches build server          |
| "Download timeout" | Slow network          | Increase timeout, use chunked download   |
| "Boot loop"        | Crash in new firmware | Wait for auto-rollback (3 boots)         |
| "BLE OTA stuck"    | MTU mismatch          | Use smaller chunks (244 bytes)           |
| "Checksum failed"  | Corrupt download      | Clear cache, re-download                 |
| "Rollback failed"  | No valid partition    | Flash via USB                            |

---

## 12. Changelog

| Version | Date       | Changes                   |
| ------- | ---------- | ------------------------- |
| 1.0.0   | 2025-11-25 | Initial OTA documentation |

---

**Made with care by SURIOTA R&D Team**
