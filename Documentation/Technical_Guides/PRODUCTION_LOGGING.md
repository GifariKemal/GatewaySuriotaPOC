# Production Logging System - SRT-MGATE-1210

**Version:** 1.1.0 | **Updated:** December 10, 2025 | **Author:** Suriota R&D Team

---

## 1. Overview

Production logging adalah sistem logging minimal yang dirancang untuk deployment jangka panjang (1-5+ tahun). Sistem ini menggantikan pendekatan lama (`Serial.end()`) dengan output yang terstruktur dan informatif.

### Perbandingan Mode

| Aspek | Development (`PRODUCTION_MODE=0`) | Production (`PRODUCTION_MODE=1`) |
|-------|-----------------------------------|----------------------------------|
| Serial Output | Full verbose | Minimal JSON |
| Log Macros | All levels (ERROR→VERBOSE) | ERROR & WARN only |
| BLE Startup | Always ON | Button-controlled |
| Heartbeat | None | Every 60s (configurable) |
| Format | Human-readable | JSON (parseable) |
| Binary Size | ~2.1 MB | ~1.8 MB (15% smaller) |

---

## 2. Production Log Format

### 2.1 JSON Output Structure

Semua log production menggunakan format JSON satu baris untuk kemudahan parsing:

```json
{"t":"TYPE","up":UPTIME,...}
```

### 2.2 Log Types

| Type | Code | Description | Frequency |
|------|------|-------------|-----------|
| Boot | `SYS` | System startup | Once |
| Heartbeat | `HB` | Periodic status | Every 60s |
| Error | `ERR` | Critical errors | On occurrence |
| Warning | `WARN` | Warnings | On occurrence |
| Network | `NET` | Network changes | On state change |
| System | `SYS` | System events | On occurrence |

---

## 3. Log Message Examples

### 3.1 Boot Message

Dikirim satu kali saat device startup:

```json
{"t":"SYS","e":"BOOT","v":"2.3.3","id":"SRT-MGATE-1210","mem":{"d":150000,"p":7500000}}
```

**Fields:**
- `t`: Type = "SYS" (System)
- `e`: Event = "BOOT"
- `v`: Firmware version
- `id`: Device ID
- `mem.d`: Free DRAM (bytes)
- `mem.p`: Free PSRAM (bytes)

### 3.2 Heartbeat Message

Dikirim setiap 60 detik (configurable):

```json
{"t":"HB","up":3600,"mem":{"d":145000,"p":7400000},"net":"ETH","proto":"mqtt","st":"OK","err":0,"mb":{"ok":1250,"er":3}}
```

**Fields:**
- `t`: Type = "HB" (Heartbeat)
- `up`: Uptime in seconds (3600 = 1 hour)
- `mem.d`: Free DRAM (bytes)
- `mem.p`: Free PSRAM (bytes)
- `net`: Network type (ETH/WIFI/NONE)
- `proto`: Active protocol (mqtt/http)
- `st`: Protocol status (OK/ERR/CONN/OFF)
- `err`: Total error count
- `mb.ok`: Modbus successful reads
- `mb.er`: Modbus error count

### 3.3 Error Message

Dikirim saat terjadi error kritis:

```json
{"t":"ERR","up":7200,"m":"MQTT","msg":"Connection lost","cnt":5}
```

**Fields:**
- `t`: Type = "ERR" (Error)
- `up`: Uptime when error occurred
- `m`: Module name
- `msg`: Error message
- `cnt`: Total error count

### 3.4 Warning Message

Dikirim saat ada kondisi warning:

```json
{"t":"WARN","up":7200,"m":"MEM","msg":"PSRAM below 1MB"}
```

### 3.5 Network State Change

Dikirim saat network berubah:

```json
{"t":"NET","up":7200,"net":"WIFI","rc":2}
```

**Fields:**
- `net`: New network type
- `rc`: Reconnect count

### 3.6 System Event

Dikirim untuk event penting:

```json
{"t":"SYS","up":7200,"e":"OTA_START","d":"v2.4.0"}
```

```json
{"t":"SYS","up":7500,"e":"OTA_COMPLETE","d":"Rebooting..."}
```

```json
{"t":"SYS","up":0,"e":"FACTORY_RESET"}
```

---

## 4. Field Reference

### 4.1 All JSON Keys

| Key | Type | Description | Example |
|-----|------|-------------|---------|
| `t` | string | Log type | "HB", "ERR", "SYS" |
| `up` | number | Uptime (seconds) | 3600 |
| `e` | string | Event name | "BOOT", "OTA_START" |
| `v` | string | Firmware version | "2.3.3" |
| `id` | string | Device ID | "SRT-MGATE-1210" |
| `mem.d` | number | Free DRAM (bytes) | 150000 |
| `mem.p` | number | Free PSRAM (bytes) | 7500000 |
| `net` | string | Network type | "ETH", "WIFI", "NONE" |
| `proto` | string | Protocol | "mqtt", "http" |
| `st` | string | Status | "OK", "ERR", "CONN", "OFF" |
| `err` | number | Error count | 5 |
| `mb.ok` | number | Modbus success | 1250 |
| `mb.er` | number | Modbus errors | 3 |
| `m` | string | Module name | "MQTT", "NET", "MEM" |
| `msg` | string | Message | "Connection lost" |
| `cnt` | number | Count | 5 |
| `rc` | number | Reconnect count | 2 |
| `d` | string | Detail/description | "v2.4.0" |

### 4.2 Status Values

| Status | Code | Description |
|--------|------|-------------|
| OK | `OK` | Operating normally |
| Error | `ERR` | Error state |
| Connecting | `CONN` | Connecting/reconnecting |
| Off | `OFF` | Disabled/not started |

### 4.3 Network Types

| Network | Code | Description |
|---------|------|-------------|
| Ethernet | `ETH` | Wired Ethernet connection |
| WiFi | `WIFI` | Wireless connection |
| None | `NONE` | No network |

---

## 5. Configuration

### 5.1 Compile-Time Configuration

Edit `Main.ino`:

```cpp
#define PRODUCTION_MODE 1  // 0 = Development, 1 = Production
```

### 5.2 Runtime Configuration

Di `setup()` setelah ProductionLogger initialized:

```cpp
// Change heartbeat interval (default: 60000ms = 1 minute)
productionLogger->setHeartbeatInterval(300000);  // 5 minutes

// Use human-readable format instead of JSON
productionLogger->setJsonFormat(false);

// Disable production logging entirely
productionLogger->setEnabled(false);
```

### 5.3 Recommended Intervals

| Deployment | Heartbeat Interval | Rationale |
|------------|-------------------|-----------|
| Debug/Test | 10-30 seconds | Quick feedback |
| Normal Production | 60 seconds (default) | Balance visibility/noise |
| Low-bandwidth | 5-15 minutes | Reduce data volume |
| Long-term (years) | 5-60 minutes | Minimal wear |

---

## 6. Integration Examples

### 6.1 Updating Protocol Status

Dalam MqttManager saat connect/disconnect:

```cpp
#if PRODUCTION_MODE == 1
ProductionLogger* logger = ProductionLogger::getInstance();
if (logger) {
    logger->setMqttStatus(ProtoStatus::OK);  // Connected
    // atau
    logger->setMqttStatus(ProtoStatus::ERROR);  // Failed
}
#endif
```

### 6.2 Logging Modbus Statistics

Dalam ModbusRtuService setelah polling cycle:

```cpp
#if PRODUCTION_MODE == 1
ProductionLogger* logger = ProductionLogger::getInstance();
if (logger) {
    logger->logModbusStats(successCount, errorCount);
}
#endif
```

### 6.3 Logging System Events

```cpp
PROD_LOG_SYSTEM("CONFIG_CHANGE", "devices.json updated");
PROD_LOG_SYSTEM("BLE_CONNECT", clientAddress.c_str());
PROD_LOG_SYSTEM("OTA_COMPLETE", "v2.4.0");
```

### 6.4 Logging Errors

```cpp
PROD_LOG_ERROR("MQTT", "Broker unreachable");
PROD_LOG_ERROR("MODBUS", "Slave 5 timeout");
PROD_LOG_ERROR("NET", "DHCP failed");
```

---

## 7. Log Collection

### 7.1 Serial Monitor Parsing

Karena output JSON satu baris, mudah di-parse dengan script:

```python
# parse_production_log.py
import json
import serial

ser = serial.Serial('/dev/ttyUSB0', 115200)

while True:
    line = ser.readline().decode('utf-8').strip()
    if line.startswith('{'):
        try:
            log = json.loads(line)
            if log['t'] == 'ERR':
                print(f"ERROR at {log['up']}s: [{log['m']}] {log['msg']}")
            elif log['t'] == 'HB':
                print(f"Heartbeat: up={log['up']}s, mem={log['mem']['d']//1024}KB, net={log['net']}")
        except json.JSONDecodeError:
            pass
```

### 7.2 Log Aggregation

Production logs dapat di-collect via:
1. **Serial-to-MQTT bridge** - Forward logs to cloud
2. **USB serial logger** - Long-term file storage
3. **RS232/RS485 adapter** - Industrial log collectors

### 7.3 Sample Log Analysis

```bash
# Count errors per module
cat production.log | jq -r 'select(.t=="ERR") | .m' | sort | uniq -c

# Get uptime history
cat production.log | jq -r 'select(.t=="HB") | .up'

# Memory trend
cat production.log | jq -r 'select(.t=="HB") | "\(.up),\(.mem.d),\(.mem.p)"'
```

---

## 8. Human-Readable Format

Jika tidak perlu JSON parsing, set `setJsonFormat(false)`:

```
========================================
[SYS] BOOT - SRT-MGATE-1210 v2.3.3
[SYS] Device ID: SRT-MGATE-1210
[SYS] Memory - DRAM: 150000, PSRAM: 7500000
========================================

[3600][HB] NET:ETH PROTO:mqtt/OK MEM:D145/P7400 ERR:0 MB:1250/3
[3660][HB] NET:ETH PROTO:mqtt/OK MEM:D144/P7399 ERR:0 MB:1270/3
[7200][ERR][MQTT] Connection lost (#1)
[7205][NET] Status: WIFI (reconnects: 1)
[7210][HB] NET:WIFI PROTO:mqtt/CONN MEM:D143/P7398 ERR:1 MB:1290/4
```

---

## 9. Best Practices

### 9.1 Do's

- Keep heartbeat interval reasonable (1-5 minutes for production)
- Log only critical errors, not every Modbus timeout
- Use JSON format for automated log collection
- Include firmware version in boot message
- Track error counts, not just occurrences

### 9.2 Don'ts

- Never log sensitive data (passwords, tokens, API keys)
- Don't flood serial with every register read
- Don't log full JSON payloads in production
- Avoid string operations in error paths (memory fragmentation)

### 9.3 Memory Considerations

ProductionLogger uses:
- ~500 bytes DRAM for instance
- ~200 bytes per log call (temporary)
- Mutex for thread safety

---

## 10. Troubleshooting

| Issue | Cause | Solution |
|-------|-------|----------|
| No serial output | `enabled = false` | Check `setEnabled(true)` |
| Too much output | Interval too short | Increase heartbeat interval |
| Missing heartbeats | `loop()` not running | Check FreeRTOS task health |
| JSON parse errors | Corrupted output | Check baud rate (115200) |
| Memory dropping | Leak elsewhere | Check heartbeat mem trend |

---

## 11. Migration from Serial.end()

### Old Approach (v2.3.2 and earlier):

```cpp
#if PRODUCTION_MODE == 1
  Serial.end();  // Completely disables serial
#endif
```

**Problems:**
- No visibility into device status
- Cannot diagnose field issues
- No error logging

### New Approach (v2.3.3+):

```cpp
#if PRODUCTION_MODE == 1
  productionLogger = ProductionLogger::getInstance();
  productionLogger->begin(FIRMWARE_VERSION, DEVICE_ID);
  productionLogger->setHeartbeatInterval(60000);
  productionLogger->logBoot();
#endif
```

**Benefits:**
- Minimal but informative output
- Error tracking and counting
- Memory/network health monitoring
- Parseable JSON format
- 1-5 year deployment ready

---

## 12. Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2025-11-25 | Initial production logging system |

---

**Made with care by SURIOTA R&D Team**
