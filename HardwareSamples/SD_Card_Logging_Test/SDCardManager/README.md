# SD Card Manager Test - SRT-MGATE-1210

SD Card logging solution for gateway MGATE-1210 with **hot-plug support**,
**FreeRTOS integration**, and **thread-safe operations**.

## File Structure

```
SD_Card_Logging_Test/
├── SDCardManager.ino    # Entry point (setup, loop, serial commands)
├── SDCardManager.h      # Header (class definition, enums, config)
├── SDCardManager.cpp    # Implementation (full logic)
└── README.md            # This documentation
```

## Features

| Feature             | Description                                       |
| ------------------- | ------------------------------------------------- |
| Hot-Plug Detection  | Auto-detect card removal/insertion                |
| Auto-Reconnect      | Automatic reconnection with configurable retries  |
| FreeRTOS Task       | Background monitoring (non-blocking)              |
| Thread-Safe         | Mutex-protected file operations                   |
| Switchable Modes    | SPI (HSPI) or SDMMC (native peripheral)           |
| Memory Diagnostics  | PSRAM/DRAM usage reporting                        |
| JSON Status         | Machine-readable status output                    |

## Hardware Configuration

### SPI Bus Separation (CRITICAL)

| Device       | SPI Bus          | CS   | MOSI | MISO | SCK  |
| ------------ | ---------------- | ---- | ---- | ---- | ---- |
| **Ethernet** | Default (FSPI)   | 48   | 14   | 21   | 47   |
| **SD Card**  | **HSPI (SPI3)**  | 11   | 10   | 13   | 12   |

> **IMPORTANT:** Ethernet uses the DEFAULT SPI bus. SD Card MUST use a separate
> bus (HSPI/SPI3) to avoid conflicts.

### Board Settings

| Parameter     | Value                                    |
| ------------- | ---------------------------------------- |
| Board         | ESP32-S3-WROOM-1-N16R8                   |
| Flash         | 16MB (QIO 80MHz)                         |
| PSRAM         | 8MB OPI                                  |
| Partition     | 8M with spiffs (3MB APP / 1.5MB SPIFFS)  |
| Arduino Core  | Core 1                                   |

## Mode Selection

Edit `SDCardManager.h` to switch modes:

```cpp
// SPI Mode (DEFAULT - recommended for compatibility)
#define SD_MODE SD_MODE_SPI

// SDMMC Mode (faster, but may not work with all SD modules)
#define SD_MODE SD_MODE_SDMMC
```

### Mode Comparison

| Feature            | SPI (HSPI)           | SDMMC (Native)       |
| ------------------ | -------------------- | -------------------- |
| Speed              | ~25 MHz max          | ~40 MHz max          |
| CPU Overhead       | Higher (software)    | Lower (hardware)     |
| Pin Flexibility    | More flexible        | Fixed mapping        |
| Compatibility      | Most SD modules      | May have issues      |
| Ethernet Conflict  | None (separate bus)  | None (no SPI used)   |

## State Machine

```
NOT_INITIALIZED
       │
       ▼
  INITIALIZING
       │
       ├──────────────┐
       ▼              ▼
    READY ◄──────► REMOVED
       │              │
       ▼              ▼
    ERROR ◄───── RECONNECTING
       │
       ▼
   DISABLED
```

## FreeRTOS Configuration

| Parameter        | Value                 |
| ---------------- | --------------------- |
| Task Name        | `SD_CARD_TASK`        |
| Stack Size       | 8192 bytes            |
| Priority         | 1 (same as services)  |
| Core             | 0 (background)        |
| Check Interval   | 2000 ms               |
| Log Interval     | 5000 ms               |

## Serial Commands

| Command     | Description                              |
| ----------- | ---------------------------------------- |
| `status`    | Print detailed SD card status            |
| `json`      | Get status as JSON                       |
| `write`     | Write manual test log entry              |
| `read`      | Read current log file                    |
| `list`      | List root directory contents             |
| `perf`      | Run 100KB performance test               |
| `mem`       | Print memory diagnostics                 |
| `enable`    | Enable SD card operations                |
| `disable`   | Disable SD card operations               |
| `reconnect` | Force reconnection attempt               |
| `help`      | Show available commands                  |

## Status Output Example

```
==================================================
              SD CARD STATUS
==================================================
 State:           READY
 Card Type:       SDHC
 Card Size:       14832      MB
 Total Space:     14832      MB
 Used Space:      128        MB
 Free Space:      14704      MB
--------------------------------------------------
              STATISTICS
--------------------------------------------------
 Mount Count:     3
 Unmount Count:   2
 Error Count:     0
 Writes OK:       156
 Writes Failed:   0
 Log Entries:     156
==================================================
```

## JSON Output Example

```json
{
  "state": "READY",
  "card_inserted": true,
  "card_type": "SDHC",
  "size_mb": 14832,
  "total_mb": 14832,
  "used_mb": 128,
  "free_mb": 14704,
  "mount_count": 3,
  "unmount_count": 2,
  "error_count": 0,
  "writes_ok": 156,
  "writes_fail": 0,
  "log_entries": 156
}
```

## Field Scenarios Handled

### Scenario 1: Normal Operation

```
[SD] Card mounted successfully!
[SD] Task started on Core 0 (priority 1)
[SD] Auto-logged: [15000] Heap: 245000, PSRAM: 7500000, State: READY
```

### Scenario 2: Card Removal While Running

```
[SD] WARN: Card removal detected!
========================================
  SD CARD: READY -> REMOVED
========================================
[SD] WARN: Card removed - operations suspended
[SD] Insert card to auto-reconnect
```

### Scenario 3: Card Re-insertion

```
[SD] Reconnect attempt 1/3
========================================
  SD CARD: RECONNECTING -> READY
========================================
[SD] Card re-inserted and mounted!
```

### Scenario 4: Max Retries Reached

```
[SD] Reconnect attempt 3/3
[SD] ERROR: Mount failed even at 1 MHz
========================================
  SD CARD: RECONNECTING -> ERROR
========================================
[SD] ERROR: Max reconnect attempts reached
```

### Scenario 5: No Card at Startup

```
[SD] Initializing SD Card Manager...
[SD] Mode: SPI (HSPI/SPI3 - Separate from Ethernet)
[SD] WARN: No card detected at startup (will auto-detect)
[SD] Task started on Core 0 (priority 1)
```

## Technologies Adopted from Main Firmware

| Technology        | Usage                                      |
| ----------------- | ------------------------------------------ |
| Singleton Pattern | `SDCardManager::getInstance()`             |
| PSRAM Allocation  | `heap_caps_malloc(MALLOC_CAP_SPIRAM)`      |
| FreeRTOS Tasks    | `xTaskCreatePinnedToCore()`                |
| Mutex Protection  | `xSemaphoreCreateMutex()` for thread-safe  |
| LOG_* Macros      | `LOG_SD_INFO`, `LOG_SD_ERROR`, etc.        |
| LogThrottle       | Prevent log spam (configurable interval)   |

## Configuration Constants

```cpp
#define SD_SPI_FREQUENCY         4000000   // 4 MHz (conservative)
#define SD_CHECK_INTERVAL_MS     2000      // Check every 2 seconds
#define SD_RECONNECT_DELAY_MS    1000      // Delay before reconnect
#define SD_MAX_RECONNECT_ATTEMPTS 3        // Max attempts before ERROR
#define SD_LOG_INTERVAL_MS       5000      // Auto-log every 5 seconds
#define SD_MUTEX_TIMEOUT_MS      500       // Mutex wait timeout

#define SD_TASK_STACK_SIZE       8192      // 8KB stack
#define SD_TASK_PRIORITY         1         // Priority level
#define SD_TASK_CORE             0         // Core 0 (background)
```

## Integration with Main Firmware

After testing is complete, integrate into main firmware:

### 1. Copy Files to Main/

```
Main/
├── SDCardManager.h
├── SDCardManager.cpp
└── SDCardConfig.h     (optional, for BLE config)
```

### 2. Include in Main.ino

```cpp
#include "SDCardManager.h"

SDCardManager* sdCardManager = nullptr;

void setup() {
  // ... existing init ...

  // Initialize SD Card Manager
  sdCardManager = SDCardManager::getInstance();
  if (sdCardManager && sdCardManager->init()) {
    sdCardManager->start();
  }
}
```

### 3. Add BLE CRUD Commands (Future)

```json
// Get SD status
{"target":"sd","op":"read"}

// Configure SD logging
{"target":"sd","op":"update","data":{"enabled":true,"interval_ms":5000}}
```

### 4. Log Modbus Data to SD

```cpp
// In ModbusRtuService or ModbusTcpService
if (sdCardManager && sdCardManager->isReady()) {
    sdCardManager->writeLog(modbusDataJson);
}
```

## Directory Structure on SD Card

```
/sd
├── logs/
│   ├── log_000001.txt
│   ├── log_000002.txt
│   └── ...
├── config/
│   └── sd_config.json
└── backup/
    ├── devices.json
    └── server_config.json
```

## Troubleshooting

### Card Not Detected

1. Ensure SD Card is properly inserted
2. Format card as **FAT32**
3. Use SDHC card (< 32GB recommended)
4. Check wiring connections

### Mount Failed

- Try a different SD Card
- Reduce SPI frequency to 1MHz (automatic fallback)
- Check 3.3V power supply stability

### Write Failures After Hot-Plug

- Wait for state to become READY
- Check `failedWrites` counter via `status` command
- Verify file system integrity

### Performance Issues

- Use SPI mode for better compatibility
- Increase buffer size for larger writes
- Avoid frequent small writes (batch data)

## Performance Benchmarks

Typical performance with 16GB SDHC card:

| Operation        | SPI Mode    | SDMMC Mode  |
| ---------------- | ----------- | ----------- |
| Write 100KB      | ~300 KB/s   | ~500 KB/s   |
| Read 100KB       | ~400 KB/s   | ~700 KB/s   |
| Small Log Write  | ~5 ms       | ~3 ms       |

## References

- [ESP32-S3 SDMMC Host Driver](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/sdmmc_host.html)
- [ESP32 SD Library](https://github.com/espressif/arduino-esp32/tree/master/libraries/SD)
- [FreeRTOS Task Management](https://www.freertos.org/a00125.html)

---

**Author:** SURIOTA R&D Team
**Date:** January 2025
**Firmware Compatibility:** v1.0.x
