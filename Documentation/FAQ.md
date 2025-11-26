# Frequently Asked Questions (FAQ)

**SRT-MGATE-1210 Modbus IIoT Gateway**

[Home](../README.md) > [Documentation](README.md) > FAQ

---

## Table of Contents

1. [Getting Started](#getting-started)
2. [BLE Connection Issues](#ble-connection-issues)
3. [Network & Connectivity](#network--connectivity)
4. [Modbus Communication](#modbus-communication)
5. [MQTT & HTTP](#mqtt--http)
6. [Configuration](#configuration)
7. [Data & Registers](#data--registers)
8. [Performance](#performance)
9. [Troubleshooting](#troubleshooting)
10. [Firmware & Updates](#firmware--updates)

---

## Getting Started

### Q: What do I need to get started with the gateway?

**A:** You need:
- SRT-MGATE-1210 gateway device
- Power supply (5V/2A minimum)
- WiFi network or Ethernet connection
- Mobile device/computer with BLE support
- BLE terminal app (nRF Connect recommended)

**See:** [Quick Start Guide](QUICKSTART.md)

### Q: How do I connect to the gateway for the first time?

**A:** Follow these steps:
1. Power on the gateway
2. Open BLE app (nRF Connect)
3. Scan for "SRT-MGATE-1210" or "Modbus-Gateway-XXXX"
4. Connect to the device
5. Find UART service and enable notifications
6. Send configuration commands via JSON

**Detailed Guide:** [QUICKSTART.md - Step 2](QUICKSTART.md#step-2-connect-via-ble-1-minute)

### Q: What's the difference between WiFi and Ethernet modes?

**A:**

| Feature | WiFi | Ethernet |
|---------|------|----------|
| **Stability** | Good | Excellent |
| **Latency** | 10-50ms | 5-10ms |
| **Interference** | Possible (2.4GHz) | None |
| **Installation** | Easy (wireless) | Requires cable |
| **Best For** | Development, temporary setups | Production, critical systems |

**Recommendation:** Use both with auto failover for best reliability.

**See:** [BEST_PRACTICES.md - Network Configuration](BEST_PRACTICES.md#network-configuration)

---

## BLE Connection Issues

### Q: I can't find the gateway in BLE scan. What should I do?

**A:** Troubleshooting steps:
1. Check power LED is ON
2. Verify gateway is within 5 meters
3. Restart Bluetooth on your device
4. Restart the gateway
5. Check if another device is already connected

**BLE Range:** Works best within 5 meters, max ~10 meters in open space.

### Q: BLE connection keeps dropping. How can I fix this?

**A:** Common causes and solutions:
- **Cause:** Distance too far → **Solution:** Move closer (< 5m)
- **Cause:** Interference → **Solution:** Move away from WiFi routers, microwaves
- **Cause:** Multiple connections → **Solution:** Disconnect other devices
- **Cause:** Low power → **Solution:** Check power supply (need 5V/2A)

### Q: Can I connect multiple devices via BLE simultaneously?

**A:** No, only ONE BLE connection at a time. Disconnect the first device before connecting another.

**Alternative:** Use MQTT or HTTP API for multi-user access.

---

## Network & Connectivity

### Q: Should I use WiFi or Ethernet?

**A:** Recommended configuration:

**Production/Critical:** Use both with auto failover
```json
{
  "wifi": {"enabled": true},
  "ethernet": {"enabled": true},
  "communication": {"mode": "auto", "prefer_ethernet": true}
}
```

**Development:** WiFi only (easier setup)

**Industrial/High-EMI:** Ethernet only (more stable)

**See:** [BEST_PRACTICES.md - Network Configuration](BEST_PRACTICES.md#network-configuration)

### Q: How does network failover work?

**A:**
1. Gateway monitors active network health
2. If primary network fails, switches to backup within 5 seconds
3. Automatically switches back when primary recovers
4. No configuration loss or data loss during switch

**Configuration:**
```json
{
  "communication": {
    "mode": "auto",
    "prefer_ethernet": true,
    "failover_delay": 5000
  }
}
```

### Q: My WiFi won't connect. What should I check?

**A:** Checklist:
- [ ] SSID is correct (case-sensitive!)
- [ ] Password is correct (case-sensitive!)
- [ ] Router is powered on and accessible
- [ ] Gateway is within WiFi range (check RSSI > -70dBm)
- [ ] WiFi is 2.4GHz (gateway doesn't support 5GHz)
- [ ] No special characters in SSID/password causing issues

**Test Command:**
```json
{"cmd": "get_status"}
```

Look for `wifi_status` and `ip_address` in response.

### Q: How do I set a static IP address?

**A:** Static IP configuration:
```json
{
  "ethernet": {
    "enabled": true,
    "dhcp": false,
    "ip": "192.168.1.100",
    "gateway": "192.168.1.1",
    "subnet": "255.255.255.0",
    "dns": "8.8.8.8"
  }
}
```

**Note:** DHCP is recommended for easier deployment. Use static only if required by IT policy.

---

## Modbus Communication

### Q: My Modbus device isn't responding. What should I check?

**A:** Step-by-step diagnosis:
1. **Verify device is powered on** - Check device power LED
2. **Check wiring** - Verify A, B, GND connections
3. **Verify slave ID** - Must match device DIP switches/config
4. **Check baud rate** - Must match device setting (usually 9600)
5. **Try different function code** - Try FC3 (holding) and FC4 (input)
6. **Enable debug logs** - `{"cmd": "set_log_level", "data": {"level": "DEBUG"}}`

**See:** [TROUBLESHOOTING.md - Modbus Issues](Technical_Guides/TROUBLESHOOTING.md#modbus-communication-problems)

### Q: What's the difference between Modbus RTU and TCP?

**A:**

| Feature | Modbus RTU | Modbus TCP |
|---------|------------|------------|
| **Physical Layer** | RS485 (serial) | Ethernet (TCP/IP) |
| **Wiring** | A, B, GND | Ethernet cable |
| **Speed** | 9600-115200 baud | Up to 100 Mbps |
| **Distance** | Up to 1200m | Unlimited (network) |
| **Best For** | Field devices | Network-connected devices |

**Configuration Example - RTU:**
```json
{
  "protocol": "rtu",
  "baud_rate": 9600,
  "data_bits": 8,
  "parity": "N",
  "stop_bits": 1
}
```

**Configuration Example - TCP:**
```json
{
  "protocol": "tcp",
  "ip_address": "192.168.1.50",
  "port": 502
}
```

### Q: How do I know which data type to use for my register?

**A:** Steps to determine correct data type:
1. **Check device manual** - Usually specifies data type
2. **Try common types first:**
   - Temperature/Humidity: `FLOAT32_ABCD` or `INT16`
   - Voltage/Current: `FLOAT32_ABCD` or `UINT16` with scale
   - Status/Alarms: `UINT16` or `BOOL`
3. **Test different endianness:** If value looks wrong, try:
   - `FLOAT32_ABCD` → `FLOAT32_CDAB` → `FLOAT32_BADC` → `FLOAT32_DCBA`
4. **Use scale/offset** - If value is off by factor of 10/100

**See:** [MODBUS_DATATYPES.md](Technical_Guides/MODBUS_DATATYPES.md) for complete list

### Q: My register values look wrong. How do I fix this?

**A:** Common problems:

**Problem 1: Value off by factor of 10/100**
```json
// Raw value: 2500, Expected: 25.0
{
  "scale": 0.01,
  "offset": 0.0
}
```

**Problem 2: Wrong endianness**
```json
// Try different byte order
"data_type": "FLOAT32_CDAB"  // Instead of ABCD
```

**Problem 3: Temperature offset**
```json
// Sensor reads 2.5°C high
{
  "scale": 1.0,
  "offset": -2.5
}
```

**See:** [BEST_PRACTICES.md - Data Types](BEST_PRACTICES.md#choose-correct-data-type)

---

## MQTT & HTTP

### Q: What's the difference between Default and Customize MQTT modes?

**A:**

**Default Mode** (Simpler):
- All data in one payload
- One topic for everything
- Fixed interval for all registers
- Best for simple setups

**Customize Mode** (Advanced):
- Multiple topics with different data
- Different intervals per topic
- Registers can be in multiple topics
- Best for complex deployments

**See:** [MQTT_PUBLISH_MODES_DOCUMENTATION.md](Technical_Guides/MQTT_PUBLISH_MODES_DOCUMENTATION.md)

### Q: My MQTT messages aren't being published. What should I check?

**A:** Troubleshooting checklist:
- [ ] MQTT enabled: `"mqtt": {"enabled": true}`
- [ ] Correct broker address and port
- [ ] Valid username/password (if required)
- [ ] Publish mode enabled: `"default_mode": {"enabled": true}`
- [ ] Network connection working
- [ ] Check gateway logs for MQTT errors

**Test connectivity:**
```json
{"cmd": "get_status"}
```

Look for `mqtt_connected: true` in response.

### Q: Should I use QoS 0, 1, or 2 for MQTT?

**A:** Recommendations:

| QoS Level | Use Case | Delivery Guarantee |
|-----------|----------|-------------------|
| **0** | Non-critical data, high frequency | At most once (may lose) |
| **1** | Production (recommended) | At least once (no loss) |
| **2** | Critical data, exact once needed | Exactly once (slow) |

**Production Recommendation:** QoS 1
```json
{
  "mqtt": {
    "qos": 1
  }
}
```

### Q: How often should I publish MQTT data?

**A:** Guidelines by data type:

| Data Type | Recommended Interval | Reason |
|-----------|---------------------|---------|
| **Temperature** | 5-10 seconds | Slow changing |
| **Pressure/Flow** | 2-5 seconds | Medium changing |
| **Alarms/Status** | 1 second | Fast response needed |
| **Historical** | 60+ seconds | Reduce bandwidth |

**Balance:** More frequent = more bandwidth/processing, but faster updates

---

## Configuration

### Q: How do I backup my gateway configuration?

**A:** Get complete configuration:
```json
{"cmd": "get_config"}
```

Save the JSON response to a file. To restore:
```json
{
  "cmd": "update_config",
  "data": { /* paste saved config here */ }
}
```

**Recommendation:** Backup before major changes!

### Q: Can I configure multiple devices at once?

**A:** Yes, use batch operations:
```json
{
  "cmd": "batch",
  "operations": [
    {
      "cmd": "create_device",
      "data": { /* device 1 config */ }
    },
    {
      "cmd": "create_device",
      "data": { /* device 2 config */ }
    }
  ]
}
```

**See:** [API.md - Batch Operations](API_Reference/API.md#batch-operations)

### Q: What happens if I restart the gateway?

**A:**
- Configuration is saved to flash memory
- All settings persist across reboots
- Gateway reconnects to network automatically
- Modbus polling resumes automatically
- MQTT/HTTP reconnects automatically

**Data Loss:** No configuration or data loss on restart

### Q: How do I reset to factory defaults?

**A:**
```json
{"cmd": "factory_reset"}
```

**⚠️ WARNING:** This erases ALL configuration:
- Network settings
- Device configurations
- Register configurations
- MQTT/HTTP settings

Backup first using `get_config`!

---

## Data & Registers

### Q: What's a register_id and why is it important?

**A:** `register_id` is a unique identifier for each register:
- Format: Usually `device_id + "_" + address` or custom name
- Example: `temp_room_1`, `voltage_l1`, `pressure_inlet`
- Used in MQTT Customize Mode to select specific registers
- Stable (doesn't change if you reorder registers)

**Why important:** Makes register selection easier and more reliable.

**See:** [MQTT_PUBLISH_MODES_DOCUMENTATION.md - Register ID](Technical_Guides/MQTT_PUBLISH_MODES_DOCUMENTATION.md#what-is-register_id)

### Q: How many devices and registers can the gateway handle?

**A:** Recommended limits:

| Resource | Recommended | Maximum | Notes |
|----------|-------------|---------|-------|
| **Devices** | 50 | 100 | May impact performance |
| **Registers** | 500 | 1000 | Across all devices |
| **MQTT Topics** | 20 | 50 | Customize mode |

**Performance Impact:** More devices/registers = higher CPU/memory usage

**See:** [CAPACITY_ANALYSIS.md](Changelog/CAPACITY_ANALYSIS.md)

### Q: Can I read and write to the same register?

**A:** Yes, if the register supports both:
```json
{
  "register_name": "Setpoint Temperature",
  "address": 4001,
  "function_code": 6,  // Write Single Register
  "access": "RW"  // Read/Write
}
```

**Note:** Check device manual to confirm register supports writes.

---

## Performance

### Q: How fast can the gateway read Modbus data?

**A:** Typical performance:
- Single register read: 50-100ms
- Full device scan (10 registers): 500ms-1s
- Depends on baud rate and device response time

**Optimization:** Group consecutive registers for faster reads.

### Q: What's the maximum MQTT publish rate?

**A:**
- **Recommended:** 1-5 seconds (1-5 messages/second)
- **Maximum:** 100ms (10 messages/second)
- **Production:** 5+ seconds for most applications

**Too fast:** May overwhelm broker or cause network congestion

### Q: How can I improve BLE performance?

**A:** Tips for faster BLE:
- Stay within 5 meters of gateway
- Avoid interference (move away from WiFi/microwaves)
- Send smaller JSON payloads
- Use batch operations for multiple commands
- Disconnect when not in use

**v2.1.1 Improvement:** 28x faster transmission (21KB in 2.1s vs 58s)

---

## Troubleshooting

### Q: Where can I find error logs?

**A:** Enable DEBUG logs:
```json
{
  "cmd": "set_log_level",
  "data": {"level": "DEBUG"}
}
```

Then read logs via:
- BLE connection (if supported)
- Serial monitor (if physical access)
- MQTT/HTTP log publishing (if configured)

**See:** [LOGGING.md](Technical_Guides/LOGGING.md)

### Q: The gateway isn't responding. What should I do?

**A:** Emergency recovery:
1. Check power LED is ON
2. Wait 30 seconds for boot
3. Try BLE reconnection
4. Power cycle the gateway
5. Check network connectivity
6. Review logs if accessible

**Still not working?** May need firmware recovery or hardware check.

### Q: How do I diagnose network issues?

**A:** Diagnostic commands:
```json
// Check status
{"cmd": "get_status"}

// Expected response includes:
{
  "wifi_status": "connected",
  "ethernet_status": "connected",
  "ip_address": "192.168.1.100",
  "mqtt_connected": true
}
```

**See:** [TROUBLESHOOTING.md - Network Issues](Technical_Guides/TROUBLESHOOTING.md#network-connectivity-issues)

### Q: What do the LED indicators mean?

**A:** Common LED patterns:

| LED Pattern | Meaning |
|------------|---------|
| **Solid Blue** | Powered, no network |
| **Blinking Blue (slow)** | Connected, normal operation |
| **Blinking Blue (fast)** | Data transmission |
| **Blinking Red** | Error state |
| **Solid Red** | Critical error |

**See:** [HARDWARE.md - LED Indicators](Technical_Guides/HARDWARE.md#led-indicators)

---

## Firmware & Updates

### Q: What firmware version am I running?

**A:** Check version:
```json
{"cmd": "get_status"}
```

Response includes:
```json
{
  "firmware_version": "2.3.3",
  "build_date": "2025-11-22"
}
```

### Q: Should I upgrade to the latest firmware?

**A:** Consider upgrading if:
- New features you need
- Bug fixes for issues you're experiencing
- Security updates
- Performance improvements

**Check:** [VERSION_HISTORY.md](Changelog/VERSION_HISTORY.md) for what's new

**⚠️ Warning:** Some versions have breaking changes. Read migration guide first!

### Q: What's new in v2.3.x (v2.3.0 - v2.3.3)?

**A:** Latest v2.3.3 (November 22, 2025):
- ✅ **BUG #32 Fix** - Restore config now works with large JSON payloads (3420+ bytes)
- ✅ **Register Index Fix** - register_index increments correctly (1→2→3→4→5)
- ✅ **Device ID Preservation** - Device IDs preserved during restore operations

**v2.3.2** (November 21, 2025):
- ✅ MQTT partial publish fix - All devices complete before publish

**v2.3.1** (November 21, 2025):
- ✅ Memory leak fix - Cache properly cleared after device deletion
- ✅ Polling stop fix - Devices stop polling immediately after deletion

**v2.3.0** (November 21, 2025):
- Clean API structure
- HTTP configuration moved to `http_config`
- Breaking change: `data_interval` removed from root
- Backup & Restore System (up to 200KB)
- Factory Reset Command
- Device Control API (enable/disable)

**Migration:** v2.3.1-2.3.3 are backward compatible. v2.3.0 has breaking changes, see [VERSION_HISTORY.md](Changelog/VERSION_HISTORY.md#v220-migration-guide)

### Q: How do I update the firmware?

**A:** Contact your gateway supplier or Kemal (firmware developer) for update procedures.

**Note:** Backup configuration before updating!

---

## Still Need Help?

### Quick Resources
- **[Quick Start Guide](QUICKSTART.md)** - Get started in 5 minutes
- **[Troubleshooting Guide](Technical_Guides/TROUBLESHOOTING.md)** - Comprehensive problem solving
- **[Best Practices](BEST_PRACTICES.md)** - Production deployment guidelines
- **[API Reference](API_Reference/API.md)** - Complete API documentation

### Common Issue Guides
- **BLE Issues:** [TROUBLESHOOTING.md - BLE Section](Technical_Guides/TROUBLESHOOTING.md#ble-connection-issues)
- **Network Issues:** [TROUBLESHOOTING.md - Network Section](Technical_Guides/TROUBLESHOOTING.md#network-connectivity-issues)
- **Modbus Issues:** [TROUBLESHOOTING.md - Modbus Section](Technical_Guides/TROUBLESHOOTING.md#modbus-communication-problems)
- **MQTT Issues:** [TROUBLESHOOTING.md - MQTT Section](Technical_Guides/TROUBLESHOOTING.md#mqtt-issues)

### Contact
- **Firmware Developer:** Kemal
- **Documentation:** This documentation repository

---

**Document Version:** 1.0
**Last Updated:** November 26, 2025
**Firmware Version:** 2.3.10

[← Back to Documentation Index](README.md) | [↑ Top](#frequently-asked-questions-faq)
