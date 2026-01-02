# 📝 Logging Reference

**SRT-MGATE-1210 Modbus IIoT Gateway** Debug and Log Message Documentation

[Home](../../README.md) > [Documentation](../README.md) >
[Technical Guides](README.md) > Logging Reference

---

## 📋 Table of Contents

- [Overview](#overview)
- [Log Format](#log-format)
- [Log Categories](#log-categories)
- [BLE Manager Logs](#ble-manager-logs)
- [CRUD Handler Logs](#crud-handler-logs)
- [Modbus RTU Logs](#modbus-rtu-logs)
- [Modbus TCP Logs](#modbus-tcp-logs)
- [Network Manager Logs](#network-manager-logs)
- [MQTT Manager Logs](#mqtt-manager-logs)
- [HTTP Manager Logs](#http-manager-logs)
- [Error Handler Logs](#error-handler-logs)
- [System Logs](#system-logs)
- [Production Mode](#production-mode)
- [Log Analysis](#log-analysis)

---

## 🔍 Overview

The SRT-MGATE-1210 firmware provides comprehensive logging for debugging and
monitoring system behavior.

### Log Levels

| Level       | Usage               | Example                                         |
| ----------- | ------------------- | ----------------------------------------------- |
| **INFO**    | Normal operation    | `[BLE] Connected to client`                     |
| **DEBUG**   | Development details | `[CRUD] Command - op: 'create', type: 'device'` |
| **WARNING** | Recoverable issues  | `[RTU] Device timeout, retrying...`             |
| **ERROR**   | Operation failures  | `[MQTT] Connection failed`                      |

### Configuration

Logging can be configured via BLE:

```json
{
  "op": "update",
  "type": "logging_config",
  "config": {
    "log_level": "INFO",
    "enable_ble_logs": true,
    "enable_modbus_logs": true,
    "enable_network_logs": true,
    "enable_system_logs": true
  }
}
```

---

## 📄 Log Format

### Standard Format

```
[TAG] Message content
```

**Examples:**

```
[BLE] Manager initialized
[CRUD] Device created: D7A3F2
[RTU] Polling device D7A3F2 (Slave:1 Port:1 Baud:9600)
```

### Detailed Format (with context)

```
[TAG] Message (Detail1:value Detail2:value)
```

**Examples:**

```
[LED] State changed to: FAST_BLINK (MQTT:ON HTTP:OFF)
[RTU] Reconfiguring Serial1 from 9600 to 19200 baud
[NETWORK] Connection check (WiFi:OK Ethernet:FAILED Attempts:3/5)
```

### Error Format

```
[TAG] ERROR: Description (Code:value)
```

**Examples:**

```
[BLE] ERROR: Fragmentation failed (Size:1024 Max:512)
[RTU] ERROR: Invalid baudrate 7200
[MQTT] ERROR: Connection timeout (Attempt:3/3)
```

---

## 📱 Log Categories

### By Module

| Category          | Tag               | Description             |
| ----------------- | ----------------- | ----------------------- |
| **BLE**           | `[BLE]`           | Bluetooth communication |
| **CRUD**          | `[CRUD]`          | Command processing      |
| **RTU**           | `[RTU]`           | Modbus RTU operations   |
| **TCP**           | `[TCP]`           | Modbus TCP operations   |
| **MQTT**          | `[MQTT]`          | MQTT client             |
| **HTTP**          | `[HTTP]`          | HTTP client             |
| **NETWORK**       | `[NETWORK]`       | WiFi/Ethernet           |
| **LED**           | `[LED]`           | LED indicators          |
| **BUTTON**        | `[BUTTON]`        | Button control          |
| **QUEUE**         | `[QUEUE]`         | Data queue              |
| **CONFIG**        | `[CONFIG]`        | Configuration           |
| **SYSTEM**        | `[SYSTEM]`        | System info             |
| **ERROR_HANDLER** | `[ERROR_HANDLER]` | Unified errors          |

---

## 📱 BLE Manager Logs

### Initialization

```
[BLE] Manager initialized
[BLE] Service UUID: 12345678-1234-1234-1234-123456789abc
[BLE] Advertising started
```

**Meaning**: BLE service successfully started and advertising.

---

### Connection

```
[BLE] Connected to client
[BLE] Device connected, MAC: XX:XX:XX:XX:XX:XX
```

**Meaning**: Mobile app connected to gateway.

---

### Disconnection

```
[BLE] Disconnected from client
[BLE] Connection lost, restarting advertising
```

**Meaning**: Client disconnected, gateway ready for new connection.

---

### Command Reception

```
[BLE CMD] Raw JSON: {"op":"create","type":"device",...}
[BLE CMD] Received fragment 0/3 (18 bytes)
[BLE CMD] Received fragment 1/3 (18 bytes)
[BLE CMD] Received fragment 2/3 (12 bytes)
[BLE CMD] Complete command assembled (48 bytes)
```

**Meaning**: Command received and reassembled from fragments.

---

### Response Transmission

```
[BLE] Sending response (234 bytes)
[BLE] Fragmented into 11 fragments
[BLE] Sent fragment 0/11
[BLE] Sent fragment 1/11
...
[BLE] Response transmission complete
```

**Meaning**: Response fragmented and sent to client.

---

### Streaming

```
[BLE] Streaming started for device: D7A3F2
[BLE] Streaming data (device: D7A3F2, registers: 3)
[BLE] Streaming stopped during mutex wait, discarding data
[BLE] Streaming stopped cleanly
```

**Meaning**: Real-time data streaming active/stopped.

---

### Errors

| Log                                              | Meaning             | Action                  |
| ------------------------------------------------ | ------------------- | ----------------------- |
| `[BLE] ERROR: Fragmentation failed`              | Payload too large   | Reduce data size        |
| `[BLE] ERROR: Cannot send, not connected`        | Client disconnected | Wait for reconnection   |
| `[BLE] ERROR: Mutex timeout (5000ms)`            | Thread deadlock     | Check for blocking code |
| `[BLE] Already running, skipping initialization` | Duplicate init call | Normal (idempotent)     |

---

## 🔧 CRUD Handler Logs

### Command Processing

```
[CRUD] Command - op: 'create', type: 'device'
[CRUD] Enqueued command (priority: NORMAL, queue size: 1)
[CRUD] Processing command from queue
[CRUD] Command executed successfully
```

**Meaning**: Command received, queued, and executed.

---

### Priority Queue

```
[CRUD] Enqueued command (priority: HIGH, queue size: 3)
[CRUD] Processing HIGH priority command
[CRUD] Queue status: HIGH=1, NORMAL=2, LOW=0
```

**Meaning**: High-priority command processed first.

---

### Batch Operations

```
[CRUD] Batch operation started (mode: sequential, commands: 5)
[CRUD] Processing batch command 1/5
[CRUD] Processing batch command 2/5
...
[CRUD] Batch operation completed (completed: 5, failed: 0)
```

**Meaning**: Batch of 5 commands executed successfully.

---

```
[CRUD] Atomic batch operation started (commands: 3)
[CRUD] Processing atomic command 1/3
[CRUD] ATOMIC batch failed: Command 2 failed
[CRUD] Rolling back atomic batch
```

**Meaning**: Atomic batch failed, changes rolled back.

---

### Streaming Control

```
[CRUD] Stop streaming command received
[CRUD] Set streaming flag to false
[CRUD] Waiting for transmissions to complete...
[CRUD] All transmissions complete, sending stop response
```

**Meaning**: Streaming stopped cleanly with race condition prevention.

---

### Errors

| Log                                                         | Meaning            | Action                  |
| ----------------------------------------------------------- | ------------------ | ----------------------- |
| `[CRUD] ERROR: Invalid operation`                           | Unsupported op     | Check command structure |
| `[CRUD] ERROR: Missing device_id`                           | Required field     | Add device_id field     |
| `[CRUD] ERROR: Device creation failed`                      | Config save error  | Check SPIFFS space      |
| `[CRUD] ERROR: Cannot stop streaming, transmission timeout` | Stuck transmission | Reconnect BLE           |

---

## 🔌 Modbus RTU Logs

### Initialization

```
Initializing Modbus RTU service with ModbusMaster library...
[RTU] Serial1 initialized (RX:15 TX:16 Baud:9600)
[RTU] Serial2 initialized (RX:17 TX:18 Baud:9600)
Modbus RTU service initialized successfully
Modbus RTU service started successfully
```

**Meaning**: Both RTU serial ports configured and ready.

---

### Device Polling

```
[RTU] Polling device D7A3F2 (Slave:1 Port:1 Baud:9600)
[RTU] Read holding registers: addr=0 count=2
[RTU] Register 'temperature' = 25.6 (raw: 0x41CCCCCD)
[RTU] Register 'humidity' = 60.2 (raw: 0x0258)
[RTU] Data enqueued for transmission
```

**Meaning**: Successfully read 2 registers from device.

---

### Baudrate Switching

```
[RTU] Reconfiguring Serial1 from 9600 to 19200 baud
[RTU] Baudrate switch complete (stabilization: 50ms)
[RTU] Polling device D8B4C1 (Slave:2 Port:1 Baud:19200)
```

**Meaning**: Dynamic baudrate change for different device.

---

### Timeout Handling

```
[RTU] Device D7A3F2 timeout (3000ms exceeded)
[RTU] Retry attempt 1/3 for device D7A3F2
[RTU] Device D7A3F2 timeout again
[RTU] Retry attempt 2/3 for device D7A3F2
[RTU] Device D7A3F2 recovered (consecutive failures: 0)
```

**Meaning**: Device temporarily unresponsive, recovered after retry.

---

### Exponential Backoff

```
[RTU] Device D7A3F2 failed (consecutive: 1, backoff: 1000ms)
[RTU] Device D7A3F2 failed (consecutive: 2, backoff: 2000ms)
[RTU] Device D7A3F2 failed (consecutive: 3, backoff: 4000ms)
[RTU] Device D7A3F2 failed (consecutive: 4, backoff: 8000ms)
[RTU] Device D7A3F2 disabled (max failures exceeded)
```

**Meaning**: Device persistently failing, disabled to prevent resource waste.

---

### Errors

| Log                                     | Meaning          | Action                                |
| --------------------------------------- | ---------------- | ------------------------------------- |
| `[RTU] ERROR: Invalid baudrate 7200`    | Unsupported baud | Use standard rate (9600, 19200, etc.) |
| `[RTU] Exception: Illegal data address` | Wrong register   | Check device manual                   |
| `[RTU] Exception: Illegal function`     | Unsupported FC   | Check function code                   |
| `[RTU] Exception: Slave device failure` | Device error     | Check device power/wiring             |
| `[RTU] Device not enabled, skipping`    | Device disabled  | Check failure state                   |

---

## 🌐 Modbus TCP Logs

### Initialization

```
[TCP] Modbus TCP service initialized
[TCP] Client pool created (max: 4 connections)
[TCP] Task started on Core 1
```

**Meaning**: TCP service ready with 4 client slots.

---

### Connection Management

```
[TCP] Connecting to 192.168.1.10:502 (slave: 1)
[TCP] TCP connection established
[TCP] Assigned client slot 0 to slave 1
[TCP] Reading holding registers (addr: 0, count: 2)
[TCP] Response received (MBAP ID: 0x0001, length: 9)
```

**Meaning**: Successfully connected and read data.

---

### Client Pool

```
[TCP] Client pool status: 0=Active, 1=Active, 2=Idle, 3=Idle
[TCP] Reusing client slot 2 for slave 3
[TCP] Evicting LRU client slot 1 (idle: 120s)
```

**Meaning**: Managing connection pool, reusing/evicting clients.

---

### Errors

| Log                               | Meaning        | Action                               |
| --------------------------------- | -------------- | ------------------------------------ |
| `[TCP] Connection timeout`        | Network issue  | Check IP/firewall                    |
| `[TCP] No available client slots` | Pool exhausted | Increase pool size or reduce polling |
| `[TCP] MBAP transaction mismatch` | Protocol error | Check slave implementation           |
| `[TCP] Response timeout (30s)`    | Slow device    | Increase timeout                     |

---

## 🌐 Network Manager Logs

### WiFi Connection

```
[NETWORK] Connecting to WiFi: MyNetwork
[NETWORK] WiFi connected (IP: 192.168.1.100, RSSI: -65 dBm)
[NETWORK] DHCP lease obtained
[NETWORK] DNS server: 8.8.8.8
```

**Meaning**: WiFi successfully connected with DHCP.

---

### Ethernet Connection

```
[NETWORK] Initializing Ethernet (W5500)
[NETWORK] Ethernet connected (IP: 192.168.1.200)
[NETWORK] Link speed: 100 Mbps Full-Duplex
```

**Meaning**: Ethernet primary connection established.

---

### Network Hysteresis

```
[NETWORK] Connection check (WiFi:OK Ethernet:FAILED Attempts:1/5)
[NETWORK] Connection check (WiFi:OK Ethernet:FAILED Attempts:2/5)
[NETWORK] Connection check (WiFi:OK Ethernet:FAILED Attempts:3/5)
[NETWORK] Connection check (WiFi:OK Ethernet:FAILED Attempts:4/5)
[NETWORK] Connection check (WiFi:OK Ethernet:FAILED Attempts:5/5)
[NETWORK] Ethernet declared DOWN (hysteresis threshold met)
```

**Meaning**: Prevents network flapping with 5-check threshold.

---

### Network Failover

```
[NETWORK] Ethernet connection lost
[NETWORK] Failover: Activating WiFi fallback
[NETWORK] WiFi connected (fallback mode)
[NETWORK] Ethernet connection restored
[NETWORK] Failover: Switching back to Ethernet (primary)
```

**Meaning**: Automatic failover between Ethernet and WiFi.

---

### Errors

| Log                                      | Meaning             | Action                     |
| ---------------------------------------- | ------------------- | -------------------------- |
| `[NETWORK] WiFi connection timeout`      | Wrong SSID/password | Check credentials          |
| `[NETWORK] DHCP failed, using static IP` | DHCP unavailable    | Configure static IP        |
| `[NETWORK] Ethernet hardware not found`  | W5500 issue         | Check SPI wiring           |
| `[NETWORK] Both WiFi and Ethernet down`  | Total network loss  | Check physical connections |

---

## 📤 MQTT Manager Logs

### Connection

```
[MQTT] Connecting to mqtt.suriota.com:1883
[MQTT] Client ID: GW-ESP32-ABCD
[MQTT] Connected successfully
[MQTT] Subscribed to: suriota/config/GW-ESP32-ABCD
```

**Meaning**: MQTT broker connected, subscribed to config topic.

---

### Data Transmission

```
[MQTT] Publishing to: suriota/devices/D7A3F2/temperature
[MQTT] Payload: {"value":25.6,"timestamp":1704067200}
[MQTT] Published successfully (QoS: 1)
[MQTT] LED NET notified (data transmission)
```

**Meaning**: Data published to MQTT with QoS 1.

---

### Persistent Queue

```
[MQTT] Connection lost, queuing message
[MQTT] Queue size: 15/1000 messages
[MQTT] Reconnecting (attempt: 1/∞)
[MQTT] Reconnected, flushing queue
[MQTT] Published queued message 1/15
...
[MQTT] Queue flushed successfully
```

**Meaning**: Messages queued during disconnection, replayed on reconnect.

---

### Configuration Update

```
[MQTT] Received config update on: suriota/config/GW-ESP32-ABCD
[MQTT] Parsing config JSON...
[MQTT] Data interval updated: 5000ms → 10000ms
[MQTT] Configuration applied
```

**Meaning**: Remote configuration update received via MQTT.

---

### Errors

| Log                                   | Meaning            | Action                             |
| ------------------------------------- | ------------------ | ---------------------------------- |
| `[MQTT] Connection failed (code: -2)` | Network issue      | Check broker reachability          |
| `[MQTT] Authentication failed`        | Wrong credentials  | Verify username/password           |
| `[MQTT] Publish failed (QoS: 1)`      | Broker unavailable | Check connection                   |
| `[MQTT] Queue full (1000/1000)`       | Long disconnection | Increase queue size or reduce rate |

---

## 🌐 HTTP Manager Logs

### Request

```
[HTTP] POST https://api.suriota.com/api/v1/data
[HTTP] Headers: Authorization: Bearer eyJ...
[HTTP] Payload size: 487 bytes
[HTTP] Request sent
```

**Meaning**: HTTP request prepared and sent.

---

### Response

```
[HTTP] Response code: 200 OK
[HTTP] Response body: {"status":"success","received":3}
[HTTP] Data transmission successful
[HTTP] LED NET notified
```

**Meaning**: Server accepted data.

---

### Retry Logic

```
[HTTP] Request failed (code: 503 Service Unavailable)
[HTTP] Retry attempt 1/3 (backoff: 1000ms)
[HTTP] Request failed (code: 503)
[HTTP] Retry attempt 2/3 (backoff: 2000ms)
[HTTP] Request successful on retry
```

**Meaning**: Server temporarily unavailable, retry successful.

---

### Errors

| Log                                               | Meaning        | Action           |
| ------------------------------------------------- | -------------- | ---------------- |
| `[HTTP] Connection timeout`                       | Network slow   | Increase timeout |
| `[HTTP] Response code: 401 Unauthorized`          | Invalid token  | Update API token |
| `[HTTP] Response code: 404 Not Found`             | Wrong endpoint | Check URL        |
| `[HTTP] Response code: 500 Internal Server Error` | Server issue   | Contact support  |
| `[HTTP] SSL certificate verification failed`      | Cert issue     | Update root CA   |

---

## ⚠️ Error Handler Logs

### Error Reporting

```
[12:34:56] [ERROR] [MODBUS_RTU] Device communication failure (Device: D7A3F2) [Detail: 3]
  └─ Modbus device not responding to read requests
  └─ Recovery: Check device power, wiring, and slave ID configuration
```

**Meaning**: Structured error with timestamp, severity, domain, and recovery
suggestion.

---

### Error Statistics

```
=== ERROR STATISTICS ===
Total Errors: 127
Critical: 2 | Error: 15 | Warning: 85 | Info: 25
Errors This Hour: 12 | This Day: 127
Most Frequent Domain: MODBUS_RTU
Error Rate: 12.00/hour, 127.00/day
```

**Meaning**: Summary of error trends.

---

### Error History

```
=== ERROR HISTORY (Last 5) ===
[12:34:56] [ERROR] [MODBUS_RTU] Device timeout (D7A3F2)
[12:35:12] [WARNING] [NETWORK] WiFi signal weak (-82 dBm)
[12:35:45] [ERROR] [MQTT] Connection lost
[12:36:01] [INFO] [MQTT] Reconnected successfully
[12:36:30] [ERROR] [MODBUS_RTU] Exception: Illegal data address
```

**Meaning**: Recent error log with timestamps.

---

### Detailed Report

```
╔════════════════════════════════════════════════╗
║     UNIFIED ERROR HANDLER - DETAILED REPORT    ║
╚════════════════════════════════════════════════╝

=== ERROR TRENDS ===
Errors this hour: 12 (rate: 12.00/hour)
Errors this day: 127 (rate: 5.29/hour average)
Most frequent domain (1h): MODBUS_RTU

Repeating errors (last hour):
  - Device communication failure: 8 times
  - Device timeout: 4 times
```

**Meaning**: Comprehensive error analysis.

---

## 🖥️ System Logs

### Startup

```
Starting BLE CRUD Manager...
Random seed initialized with: 3472819456
PSRAM available: 8388608 bytes
PSRAM free: 8192000 bytes
[MAIN] Configuration initialization completed.
[MAIN] All services started successfully
BLE CRUD Manager started successfully
```

**Meaning**: Firmware boot sequence complete.

---

### Memory

```
[SYSTEM] Free heap: 102400 bytes
[SYSTEM] Free PSRAM: 6291456 bytes
[SYSTEM] Largest free block: 98304 bytes
[SYSTEM] Memory fragmentation: 4%
```

**Meaning**: Memory health check.

---

### Task Monitoring

```
[SYSTEM] Task status:
  - MODBUS_RTU_TASK: Running (Core 1, Priority 2)
  - MODBUS_TCP_TASK: Running (Core 1, Priority 2)
  - MQTT_TASK: Running (Core 0, Priority 2)
  - BLE_STREAM_TASK: Running (Core 1, Priority 2)
```

**Meaning**: FreeRTOS task health.

---

### Watchdog

```
[SYSTEM] Watchdog timeout on task: MODBUS_RTU_TASK
[SYSTEM] Task backtrace: 0x400d1234 → 0x400d5678
[SYSTEM] System reset triggered
```

**Meaning**: Task watchdog timeout, automatic recovery.

---

## 🏭 Production Mode

### Mode Configuration

```cpp
#define PRODUCTION_MODE 0  // 0 = Development, 1 = Production
```

**Development Mode (0):**

- Serial output: **ENABLED**
- BLE: **Always ON**
- Button: **Disabled**
- LED STATUS: **Slow blink (2000ms)**

```
Serial.begin(115200);
Serial.println("Development mode - full logging enabled");
```

**Production Mode (1):**

- Serial output: **DISABLED** (via `Serial.end()`)
- BLE: **Controlled by button**
- Button: **Active** (long press/double click)
- LED STATUS: **Medium blink (500ms) or very slow (3000ms)**

```
#if PRODUCTION_MODE == 1
  Serial.end();  // Disable serial output
#endif
```

---

## 📊 Log Analysis

### Performance Monitoring

**Command Processing Time:**

```
[CRUD] Command received at: 1234567890
[CRUD] Command queued at: 1234567891 (1ms)
[CRUD] Command processed at: 1234567895 (5ms total)
```

**Analysis**: Command took 5ms total (1ms queue + 4ms process).

---

**BLE Transmission:**

```
[BLE] Sending response (234 bytes)
[BLE] Fragmentation: 11 fragments × 23 bytes
[BLE] Transmission time: 550ms (50ms × 11)
```

**Analysis**: 11 fragments with 50ms inter-fragment delay.

---

### Troubleshooting Patterns

**Pattern 1: Device Timeout Loop**

```
[RTU] Device D7A3F2 timeout
[RTU] Retry attempt 1/3
[RTU] Device D7A3F2 timeout
[RTU] Retry attempt 2/3
[RTU] Device D7A3F2 timeout
[RTU] Retry attempt 3/3
[RTU] Device D7A3F2 disabled
```

**Diagnosis**: Device permanently offline. **Action**: Check physical
connection.

---

**Pattern 2: Network Flapping**

```
[NETWORK] WiFi connected
[NETWORK] WiFi disconnected
[NETWORK] WiFi connected
[NETWORK] WiFi disconnected
```

**Diagnosis**: Weak signal or interference. **Action**: Move gateway closer to
AP.

---

**Pattern 3: Memory Leak**

```
[SYSTEM] Free heap: 102400 bytes
[SYSTEM] Free heap: 98304 bytes (30s later)
[SYSTEM] Free heap: 94208 bytes (60s later)
[SYSTEM] Free heap: 90112 bytes (90s later)
```

**Diagnosis**: Gradual memory decrease. **Action**: Check for unfreed
allocations.

---

### Log Filtering

**Grep Examples:**

```bash
# Filter BLE logs only
grep "\[BLE\]" serial_output.log

# Filter errors only
grep "ERROR" serial_output.log

# Filter specific device
grep "D7A3F2" serial_output.log

# Count errors by type
grep "\[ERROR\]" serial_output.log | cut -d']' -f3 | sort | uniq -c

# Show last 100 lines
tail -n 100 serial_output.log

# Follow logs in real-time
tail -f /dev/ttyUSB0
```

---

## Related Documentation

- **[Documentation Index](../README.md)** - Main documentation hub
- **[API Reference](../API_Reference/API.md)** - Complete BLE API reference
  (including log level commands)
- **[PROTOCOL.md](PROTOCOL.md)** - Communication protocols
- **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** - Issue diagnosis using logs
- **[HARDWARE.md](HARDWARE.md)** - Hardware specifications
- **[Best Practices](../BEST_PRACTICES.md)** - Production logging
  recommendations
- **[Optimization Documentation](../Optimizing/README.md)** - Log system
  implementation history

---

**Document Version:** 1.1 **Last Updated:** December 10, 2025 **Firmware
Version:** 2.5.34

[← Back to Technical Guides](README.md) | [↑ Top](#-logging-reference)

---

**© 2025 PT Surya Inovasi Prioritas (SURIOTA) - R&D Team** _For technical
support: support@suriota.com_
