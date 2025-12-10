# 🔧 Troubleshooting Guide

**SRT-MGATE-1210 Modbus IIoT Gateway**
Common Issues and Solutions

[Home](../../README.md) > [Documentation](../README.md) > [Technical Guides](README.md) > Troubleshooting Guide

**Current Version:** v2.5.34
**Developer:** Kemal
**Last Updated:** December 10, 2025

---

## 📋 Table of Contents

- [Quick Diagnostics](#quick-diagnostics)
- [BLE Issues](#ble-issues)
- [Modbus RTU Issues](#modbus-rtu-issues)
- [Modbus TCP Issues](#modbus-tcp-issues)
- [Network Issues](#network-issues)
- [MQTT/HTTP Issues](#mqtthttp-issues)
- [Power and Hardware Issues](#power-and-hardware-issues)
- [Performance Issues](#performance-issues)
- [Configuration Issues](#configuration-issues)
- [Advanced Diagnostics](#advanced-diagnostics)

---

## 🚀 Quick Diagnostics

### System Health Check

Run these commands to quickly assess system status:

**1. Get System Status**
```json
{
  "op": "read",
  "type": "status"
}
```

**2. Get BLE Metrics**
```json
{
  "op": "read",
  "type": "metrics"
}
```

**3. Check Error Statistics**

View serial output:
```
[ERROR_HANDLER] Total Errors: 127
[ERROR_HANDLER] Most Frequent Domain: MODBUS_RTU
```

### LED Indicators

| LED            | State          | Meaning                  | Action              |
| -------------- | -------------- | ------------------------ | ------------------- |
| **LED NET**    | OFF            | No network connection    | Check WiFi/Ethernet |
| **LED NET**    | Slow blink     | Connected, no data       | Normal idle state   |
| **LED NET**    | Fast blink     | Active data transmission | Normal operation    |
| **LED STATUS** | Slow (2s)      | Development mode         | Normal (dev mode)   |
| **LED STATUS** | Medium (500ms) | Production running       | Normal (BLE off)    |
| **LED STATUS** | Very slow (3s) | Production config        | Normal (BLE on)     |

---

## 📱 BLE Issues

### Issue: Gateway not discoverable

**Symptoms:**
- BLE device "SURIOTA GW" not appearing in scan
- Mobile app can't find gateway

**Diagnosis:**
```
Check serial output:
[BLE] Already running, skipping initialization
[BLE] Advertising started
```

**Solutions:**

1. **Check production mode:**
   ```cpp
   #define PRODUCTION_MODE 1  // BLE controlled by button
   ```
   - In production mode, long-press button to enable BLE

2. **Verify BLE is advertising:**
   ```
   [BLE] Advertising started  // Should appear in logs
   ```

3. **Check Bluetooth on mobile device:**
   - Enable Bluetooth
   - Enable location (required on Android)
   - Grant app permissions

4. **Reset BLE:**
   - Reboot gateway
   - Wait for `[BLE] Advertising started` log

5. **Check antenna:**
   - Ensure antenna properly connected (if external)
   - Verify no metal obstructions nearby

---

### Issue: BLE connection drops frequently

**Symptoms:**
- Frequent disconnections
- Commands timeout
- Data streaming interrupted

**Diagnosis:**
```
[BLE] Disconnected from client
[BLE] Device connected, MAC: XX:XX:XX:XX:XX:XX
[BLE] Disconnected from client (repeating pattern)
```

**Solutions:**

1. **Check signal strength:**
   - Move mobile device closer (< 10m)
   - Remove obstacles between devices
   - Check for interference (2.4GHz WiFi, microwaves)

2. **Check mobile device:**
   - Ensure app is running in foreground
   - Disable battery optimization for app
   - Close other Bluetooth apps

3. **Check gateway load:**
   ```
   [SYSTEM] Free heap: 102400 bytes (check if critically low)
   ```
   - If heap < 50KB, reduce device/register count

4. **Update connection parameters:**
   - Increase connection interval
   - Increase supervision timeout

---

### Issue: BLE transmission timeout / slow response

**Symptoms:**
- Mobile app shows timeout error (30+ seconds)
- Large API responses take very long time
- `devices_with_registers` with 100+ registers timeout
- UI freezes during data loading

**Diagnosis:**
```
// Check Serial Monitor for processing time:
[CRUD] devices_with_registers returned 100 devices, 2600 total registers in 87 ms
[BLE] Sending response... (takes 58+ seconds on v2.0)

// Check firmware version:
Firmware v2.0: ❌ SLOW (58 seconds for 21KB payload)
Firmware v2.1.1: ✅ FAST (2.1 seconds for 21KB payload)
```

**Solutions:**

1. **✅ Upgrade to v2.1.1+ (RECOMMENDED)**
   - BLE transmission optimized (28x faster)
   - 21KB payload: 58s → 2.1s
   - Completely eliminates timeout issues

   **What changed in v2.1.1:**
   ```cpp
   // BLEManager.h
   #define CHUNK_SIZE 244          // Was 18 (1356% improvement)
   #define FRAGMENT_DELAY_MS 10    // Was 50 (80% reduction)
   ```

2. **Use minimal mode** (if on v2.1.0+):
   ```json
   {
     "op": "read",
     "type": "devices_with_registers",
     "minimal": true  // 71% smaller payload
   }
   ```
   - Full mode: 21KB → 2.1s
   - Minimal mode: 6KB → 0.6s

3. **Reduce dataset size** (temporary workaround for v2.0):
   - Delete unused devices/registers
   - Query specific devices instead of all
   - Use pagination if available

4. **Increase mobile app timeout** (not recommended):
   - Only works if payload < 100KB
   - Better to upgrade firmware

**Performance Comparison:**

| Version | Payload | Transmission Time | Status |
|---------|---------|-------------------|--------|
| v2.0 | 21 KB | 58.4 sec | ❌ Timeout |
| v2.1.0 | 21 KB | 58.4 sec | ❌ Timeout |
| v2.1.1 | 21 KB (full) | 2.1 sec | ✅ Fast |
| v2.1.1 | 6 KB (minimal) | 0.6 sec | ✅ Very fast |

**Root Cause Explained:**

Old firmware used very small chunk size (18 bytes) with slow delay (50ms):
```
21,000 bytes ÷ 18 bytes/chunk = 1,167 chunks
1,167 chunks × 50ms delay = 58,350ms ≈ 58 seconds
```

New firmware uses optimal chunk size (244 bytes) with fast delay (10ms):
```
21,000 bytes ÷ 244 bytes/chunk = 86 chunks
86 chunks × 10ms delay = 860ms ≈ 0.86 seconds
(+ ~1.2s processing overhead = 2.1 seconds total)
```

**See:** [VERSION_HISTORY.md](VERSION_HISTORY.md) for v2.1.1 details

---

### Issue: Commands return errors

**Symptoms:**
- `ERR_INVALID_JSON` errors
- `ERR_MISSING_FIELD` errors
- Commands not executed

**Diagnosis:**
```
[BLE CMD] Raw JSON: {"op":"create","type":"device"...
[CRUD] ERROR: Invalid operation
```

**Solutions:**

1. **Validate JSON syntax:**
   - Check for missing quotes
   - Verify closing braces
   - Use JSON validator online

2. **Check required fields:**
   ```json
   {
     "op": "create",        // Required
     "type": "device",      // Required
     "config": { }          // Required for create/update
   }
   ```

3. **Check device_id/register_id:**
   - List devices first to get valid IDs
   - Use exact ID (case-sensitive)

4. **Check data types:**
   - Ensure numbers are not in quotes
   - Boolean should be `true`/`false`, not `"true"`/`"false"`

---

### Issue: Response fragmentation errors

**Symptoms:**
- Incomplete responses
- "Malformed JSON structure" errors
- Response timeout

**Diagnosis:**
```
[BLE] Sent fragment 0/11
[BLE] Sent fragment 1/11
[BLE] ERROR: Transmission timeout
```

**Solutions:**

1. **Check MTU size:**
   - Ensure MTU negotiation successful
   - Default MTU: 23 bytes

2. **Reduce response size:**
   - Use `devices_summary` instead of `devices`
   - Use `registers_summary` instead of `registers`
   - Read single device instead of all

3. **Wait for transmission complete:**
   - Don't send new commands immediately
   - Wait for response before next command

---

## 🔌 Modbus RTU Issues

### Issue: Device not responding

**Symptoms:**
- `[RTU] Device timeout` errors
- No data from device
- Device disabled after retries

**Diagnosis:**
```
[RTU] Device D7A3F2 timeout (3000ms exceeded)
[RTU] Exception: Slave device failure
[RTU] Device D7A3F2 disabled (max failures exceeded)
```

**Solutions:**

1. **Check physical connection:**
   - Verify RS485 A/B wiring
   - Check termination resistors (120Ω at both ends)
   - Ensure common ground (if required)

2. **Check slave ID:**
   ```json
   {
     "slave_id": 1  // Must match device DIP switch/config
   }
   ```

3. **Check baudrate:**
   ```json
   {
     "baud_rate": 9600  // Must match device setting
   }
   ```
   - Supported: 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200

4. **Check timeout:**
   ```json
   {
     "timeout": 5000  // Increase for slow devices
   }
   ```

5. **Check serial port:**
   ```json
   {
     "serial_port": 1  // Port 1 (GPIO 15/16) or 2 (GPIO 17/18)
   }
   ```

6. **Enable device manually:**
   ```json
   {
     "op": "update",
     "type": "device",
     "device_id": "D7A3F2",
     "config": {
       "retry_count": 5
     }
   }
   ```

---

### Issue: Wrong data values

**Symptoms:**
- Data values incorrect
- NaN or Inf values
- Values don't match device display

**Diagnosis:**
```
[RTU] Register 'temperature' = NaN (raw: 0xFFFFFFFF)
[RTU] Register 'pressure' = 65535 (expected: 100.5)
```

**Solutions:**

1. **Check data type:**
   ```json
   {
     "data_type": "FLOAT32_ABCD"  // Try other endianness
   }
   ```
   - Try: `FLOAT32_CDAB`, `FLOAT32_BADC`, `FLOAT32_DCBA`
   - Check device manual for endianness

2. **Check register address:**
   ```json
   {
     "address": 0  // 0-based or 1-based? Check manual
   }
   ```
   - Some devices use 1-based addressing (subtract 1)

3. **Check function code:**
   ```json
   {
     "function_code": "holding"  // Try "input" if not working
   }
   ```

4. **Check scale and offset:**
   ```json
   {
     "scale": 0.1,    // Multiply raw value
     "offset": -40.0  // Add after scaling
   }
   ```
   - Formula: `final = (raw * scale) + offset`

5. **Read raw value:**
   - Use data streaming to see raw register value
   - Compare with device display
   - Calculate correct scale/offset

---

### Issue: Baudrate switching fails

**Symptoms:**
- Device works at 9600 but not at higher baudrates
- "Invalid baudrate" errors

**Diagnosis:**
```
[RTU] ERROR: Invalid baudrate 7200
[RTU] Reconfiguring Serial1 from 9600 to 19200 baud
```

**Solutions:**

1. **Use standard baudrates only:**
   - Valid: 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200
   - Invalid: 7200, 14400, 31250

2. **Check device support:**
   - Verify device supports requested baudrate
   - Some devices only support 9600/19200

3. **One device per port if mixing baudrates:**
   - Port 1: All devices at same baud
   - Port 2: Different baudrate devices

4. **Increase stabilization delay:**
   - Default: 50ms after baudrate change
   - Modify firmware if needed for slow devices

---

## 🌐 Modbus TCP Issues

### Issue: Cannot connect to TCP device

**Symptoms:**
- `[TCP] Connection timeout` errors
- No data from TCP devices

**Diagnosis:**
```
[TCP] Connecting to 192.168.1.10:502 (slave: 1)
[TCP] Connection timeout (30s)
```

**Solutions:**

1. **Check IP address:**
   - Verify device IP with ping: `ping 192.168.1.10`
   - Check device IP configuration

2. **Check port:**
   ```json
   {
     "port": 502  // Standard Modbus TCP port
   }
   ```
   - Some devices use non-standard ports

3. **Check network:**
   - Ensure gateway and device on same subnet
   - Check firewall rules
   - Verify switch/router configuration

4. **Check device TCP support:**
   - Some devices limit simultaneous connections
   - Try disconnecting other clients

5. **Check slave ID:**
   ```json
   {
     "slave_id": 1  // Modbus unit ID (usually 1 or 255)
   }
   ```

---

### Issue: Client pool exhausted

**Symptoms:**
- `[TCP] No available client slots` error
- Some devices not polled

**Diagnosis:**
```
[TCP] Client pool status: 0=Active, 1=Active, 2=Active, 3=Active
[TCP] No available client slots
```

**Solutions:**

1. **Reduce device count:**
   - Max 4 simultaneous TCP connections
   - Use RTU for additional devices

2. **Increase polling interval:**
   ```json
   {
     "refresh_rate_ms": 5000  // Slower polling = fewer connections
   }
   ```

3. **Group devices by IP:**
   - Multiple slaves on same IP reuse connection

---

## 🌐 Network Issues

### Issue: WiFi won't connect

**Symptoms:**
- `[NETWORK] WiFi connection timeout` error
- No IP address assigned

**Diagnosis:**
```
[NETWORK] Connecting to WiFi: MyNetwork
[NETWORK] WiFi connection timeout (30s)
```

**Solutions:**

1. **Check credentials:**
   ```json
   {
     "wifi_ssid": "MyNetwork",
     "wifi_password": "correct_password"
   }
   ```

2. **Check WiFi band:**
   - ESP32-S3 only supports 2.4GHz
   - 5GHz networks won't work

3. **Check signal strength:**
   - Move gateway closer to AP
   - Check for interference

4. **Check SSID visibility:**
   - Hidden SSIDs may have issues
   - Make SSID visible temporarily

5. **Reset network settings:**
   - Delete `/server_config.json` via BLE
   - Reconfigure WiFi

---

### Issue: Ethernet not working

**Symptoms:**
- `[NETWORK] Ethernet hardware not found` error
- No Ethernet connection

**Diagnosis:**
```
[NETWORK] Initializing Ethernet (W5500)
[NETWORK] Ethernet hardware not found
```

**Solutions:**

1. **Check SPI wiring:**
   - Verify W5500 connections
   - Check SPI MISO/MOSI/CLK/CS pins

2. **Check W5500 power:**
   - Ensure 3.3V supply
   - Check current capacity (> 150mA)

3. **Check Ethernet cable:**
   - Verify cable connected
   - Try different cable
   - Check link LED on W5500

4. **Check MAC address:**
   - Should auto-generate from ESP32 MAC
   - Check for duplicate MAC on network

---

### Issue: Network flapping

**Symptoms:**
- Frequent connect/disconnect cycles
- LED NET rapid blinking

**Diagnosis:**
```
[NETWORK] WiFi connected
[NETWORK] WiFi disconnected (10s later)
[NETWORK] WiFi connected
[NETWORK] WiFi disconnected (10s later)
```

**Solutions:**

1. **Check signal strength:**
   ```
   [NETWORK] WiFi RSSI: -82 dBm  // Too weak (< -80 dBm)
   ```
   - Move gateway closer to AP
   - Use external antenna
   - Reduce obstacles

2. **Network hysteresis working:**
   - Requires 5 consecutive failures to declare down
   - Requires 3 consecutive successes to declare up
   - Prevents rapid state changes

3. **Check router:**
   - Update router firmware
   - Disable aggressive power saving
   - Increase DHCP lease time

---

## 📤 MQTT/HTTP Issues

### Issue: MQTT connection fails

**Symptoms:**
- `[MQTT] Connection failed` error
- No data reaching cloud

**Diagnosis:**
```
[MQTT] Connecting to mqtt.suriota.com:1883
[MQTT] Connection failed (code: -2)
```

**Error Codes:**
- `-2`: Network error
- `-4`: Connection timeout
- `5`: Authentication failed

**Solutions:**

1. **Check broker address:**
   ```json
   {
     "broker": "mqtt.suriota.com",
     "port": 1883
   }
   ```

2. **Check authentication:**
   ```json
   {
     "username": "gateway",
     "password": "your_password"
   }
   ```

3. **Check network connectivity:**
   - Verify internet connection
   - Ping broker: `ping mqtt.suriota.com`
   - Check firewall (port 1883)

4. **Try alternative port:**
   ```json
   {
     "port": 8883  // TLS port (if supported)
   }
   ```

5. **Check client ID:**
   - Ensure unique client ID
   - Don't connect same ID from multiple gateways

---

### Issue: MQTT messages not received by server

**Symptoms:**
- Gateway shows publish success
- Server doesn't receive data

**Diagnosis:**
```
[MQTT] Published successfully (QoS: 1)
[MQTT] No acknowledgment received
```

**Solutions:**

1. **Check topic format:**
   ```json
   {
     "topic_prefix": "suriota/devices"
   }
   ```
   - Verify server expects this format
   - Check for typos

2. **Check QoS:**
   ```json
   {
     "qos": 1  // At least once delivery
   }
   ```
   - QoS 0: No guarantee
   - QoS 1: Acknowledged delivery

3. **Check payload format:**
   - Verify server expects JSON
   - Check timestamp format

4. **Check subscription:**
   - Verify server subscribed to correct topic
   - Use MQTT client (mosquitto_sub) to test

---

### Issue: HTTP requests timeout

**Symptoms:**
- `[HTTP] Connection timeout` error
- Data not reaching server

**Diagnosis:**
```
[HTTP] POST https://api.suriota.com/api/v1/data
[HTTP] Connection timeout (10s)
```

**Solutions:**

1. **Check endpoint:**
   ```json
   {
     "endpoint": "https://api.suriota.com"
   }
   ```
   - Verify URL correct
   - Test with curl/Postman

2. **Increase timeout:**
   ```json
   {
     "timeout": 30000  // 30 seconds
   }
   ```

3. **Check SSL certificate:**
   - Server certificate must be valid
   - Update root CA if needed

4. **Check API token:**
   ```json
   {
     "api_token": "your_valid_token"
   }
   ```
   - Verify token not expired
   - Check Authorization header

---

## ⚡ Power and Hardware Issues

### Issue: Gateway reboots randomly

**Symptoms:**
- Unexpected restarts
- Watchdog resets

**Diagnosis:**
```
[SYSTEM] Watchdog timeout on task: MODBUS_RTU_TASK
[SYSTEM] System reset triggered
```

**Solutions:**

1. **Check power supply:**
   - Use 12V DC, 2A minimum
   - Check for voltage drops
   - Use regulated power supply

2. **Check brownout:**
   - Add 1000µF capacitor near power input
   - Improve power supply filtering

3. **Check task watchdog:**
   - Tasks must yield regularly
   - Check for blocking code

4. **Check memory:**
   ```
   [SYSTEM] Free heap: 5120 bytes  // Critical low
   ```
   - Reduce device/register count
   - Increase PSRAM usage

---

### Issue: RS485 communication errors

**Symptoms:**
- Intermittent Modbus failures
- Corrupted data

**Diagnosis:**
```
[RTU] CRC error
[RTU] Exception: Illegal data value
```

**Solutions:**

1. **Check termination:**
   - 120Ω resistor at both ends of bus
   - Remove termination from middle devices

2. **Check cable quality:**
   - Use twisted pair (Cat5e or better)
   - Max length: 1200m @ 9600 baud
   - Shield cable and ground one end

3. **Check biasing:**
   - Some networks need pull-up/pull-down resistors
   - A: 680Ω to 5V
   - B: 680Ω to GND

4. **Check grounding:**
   - RS485 is differential, but ground may help
   - Connect GND at one point only

---

## 🚀 Performance Issues

### Issue: Slow data polling

**Symptoms:**
- Data updates delayed
- Long intervals between readings

**Diagnosis:**
```
[RTU] Polling device D7A3F2 (refresh: 10000ms)
```

**Solutions:**

1. **Reduce refresh rate:**
   ```json
   {
     "refresh_rate_ms": 1000  // Poll every 1 second
   }
   ```

2. **Check device timeout:**
   ```json
   {
     "timeout": 3000  // Reduce if device responds fast
   }
   ```

3. **Optimize register reads:**
   - Group contiguous registers
   - Use multi-register reads

4. **Check task priority:**
   - Modbus tasks run at priority 2
   - Ensure not blocked by higher priority

---

### Issue: High memory usage

**Symptoms:**
- Out of memory errors
- System instability

**Diagnosis:**
```
[SYSTEM] Free heap: 20480 bytes  // Low
[SYSTEM] Free PSRAM: 1048576 bytes  // Available
```

**Solutions:**

1. **Use PSRAM for large allocations:**
   - ConfigManager already uses PSRAM
   - QueueManager uses PSRAM
   - BLEManager uses PSRAM

2. **Reduce device count:**
   - Max recommended: 20 devices
   - Reduce register count per device

3. **Reduce queue size:**
   - Default: 1000 data points
   - Reduce if memory constrained

4. **Clear persistent queue:**
   ```json
   {
     "op": "update",
     "type": "server_config",
     "config": {
       "clear_queue": true
     }
   }
   ```

---

## ⚙️ Configuration Issues

### Issue: Configuration not saved

**Symptoms:**
- Changes revert after reboot
- `ERR_CONFIG_SAVE_FAILED` error

**Diagnosis:**
```
[CONFIG] Failed to save configuration
[CONFIG] SPIFFS write error
```

**Solutions:**

1. **Check SPIFFS space:**
   - Total: 192 KB
   - Check available space

2. **Check file integrity:**
   - Delete corrupted config files via BLE
   - Reconfigure from scratch

3. **Use atomic operations:**
   - Configuration writes use atomic file ops
   - Should prevent corruption

4. **Check SD card (if using):**
   - Verify SD card mounted
   - Check filesystem health

---

### Issue: Device ID conflicts

**Symptoms:**
- Duplicate device IDs
- Configuration overwritten

**Diagnosis:**
```
[CONFIG] Device D7A3F2 already exists
```

**Solutions:**

1. **List devices first:**
   ```json
   {
     "op": "read",
     "type": "devices_summary"
   }
   ```

2. **Use update instead of create:**
   ```json
   {
     "op": "update",  // Not "create"
     "type": "device",
     "device_id": "D7A3F2",
     "config": { }
   }
   ```

3. **Delete duplicate:**
   ```json
   {
     "op": "delete",
     "type": "device",
     "device_id": "D7A3F2"
   }
   ```

---

## 🔬 Advanced Diagnostics

### Serial Monitor Commands

**1. Enable diagnostic mode:**
```cpp
// Via firmware modification
errorHandler->setDiagnosticMode(true);
```

**2. View error history:**
```cpp
errorHandler->printErrorHistory(10);  // Last 10 errors
```

**3. View BLE metrics:**
```cpp
bleMetrics->printDetailedReport();
```

**4. View memory:**
```cpp
Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
Serial.printf("Free PSRAM: %d\n", ESP.getFreePsram());
```

---

### Log Analysis

**1. Capture logs:**
```bash
# Linux/Mac
screen /dev/ttyUSB0 115200 | tee gateway.log

# Windows (PuTTY)
# Enable session logging in settings
```

**2. Analyze patterns:**
```bash
# Count errors by type
grep "ERROR" gateway.log | cut -d']' -f3 | sort | uniq -c

# Show timing of events
grep "\[RTU\] Device D7A3F2" gateway.log | head -20

# Find memory leaks
grep "Free heap" gateway.log
```

**3. Share logs:**
- Copy relevant section (50-100 lines around issue)
- Include timestamp
- Include system status output

---

### Factory Reset

**Via BLE:**
```json
{
  "op": "delete",
  "type": "device",
  "device_id": "D7A3F2"
}
```
Repeat for all devices.

**Via Serial:**
```cpp
// Uncomment in Main.ino setup():
configManager->clearAllConfigurations();
```

**Via SPIFFS:**
- Delete `/devices.json`
- Delete `/server_config.json`
- Delete `/logging_config.json`
- Reboot

---

### Firmware Update

1. **Backup configuration:**
   - Export all devices via BLE
   - Save to file

2. **Upload new firmware:**
   - Use Arduino IDE
   - Select correct board: ESP32-S3
   - Partition: 16MB (8MB APP/8MB OTA)

3. **Restore configuration:**
   - Import devices via BLE batch operation

---

## 📞 Getting Help

### Information to Provide

When requesting support, include:

1. **Hardware version:**
   - Board model
   - ESP32-S3 variant
   - Firmware version

2. **Configuration:**
   - Device configuration JSON
   - Server configuration
   - Number of devices/registers

3. **Logs:**
   - 50-100 lines around issue
   - Include `[SYSTEM]` status
   - Include error messages

4. **Steps to reproduce:**
   - What you did
   - What you expected
   - What actually happened

---

### Support Channels

- **Email**: support@suriota.com
- **GitHub Issues**: [Repository Link]
- **Documentation**: [Docs Website]

---

## Related Documentation

- **[Documentation Index](../README.md)** - Main documentation hub
- **[Quick Start Guide](../QUICKSTART.md)** - Get started in 5 minutes
- **[FAQ](../FAQ.md)** - Frequently asked questions
- **[API Reference](../API_Reference/API.md)** - Complete API reference
- **[PROTOCOL.md](PROTOCOL.md)** - Protocol specifications
- **[HARDWARE.md](HARDWARE.md)** - Hardware specifications
- **[LOGGING.md](LOGGING.md)** - Log system reference
- **[NETWORK_CONFIGURATION.md](NETWORK_CONFIGURATION.md)** - Network configuration guide
- **[Best Practices](../BEST_PRACTICES.md)** - Production deployment guidelines

---

**Document Version:** 1.0
**Last Updated:** December 10, 2025
**Firmware Version:** 2.5.34

[← Back to Technical Guides](README.md) | [↑ Top](#-troubleshooting-guide)

---

**© 2025 PT Surya Inovasi Prioritas (SURIOTA) - R&D Team**
*For technical support: support@suriota.com*
