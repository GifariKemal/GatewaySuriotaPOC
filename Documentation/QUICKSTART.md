# Quick Start Guide

**SRT-MGATE-1210 Modbus IIoT Gateway**
Get your gateway configured and running in 5 minutes

[Home](../README.md) > [Documentation](README.md) > Quick Start Guide

---

## What You'll Need

- SRT-MGATE-1210 Gateway device
- Power supply (5V/2A minimum)
- WiFi network or Ethernet connection
- Mobile device or computer with BLE capability
- BLE terminal app (nRF Connect, Serial Bluetooth Terminal, or custom app)

---

## Step 1: Power On (30 seconds)

1. Connect power supply to the gateway
2. Observe the LED indicators:
   - **Power LED:** Should be solid ON
   - **Status LED:** Should start blinking (device initializing)
3. Wait for device to boot (~10-15 seconds)

**LED Reference:**
- **Solid Blue:** Device ready, no network
- **Blinking Blue (slow):** Connected and operating normally
- **Blinking Red:** Error state (see [Troubleshooting](Technical_Guides/TROUBLESHOOTING.md))

---

## Step 2: Connect via BLE (1 minute)

### Using nRF Connect (Recommended)

1. **Open nRF Connect** on your mobile device
2. **Scan for devices** - Look for device named:
   ```
   SRT-MGATE-1210
   ```
   or
   ```
   Modbus-Gateway-XXXX
   ```

3. **Connect** to the device
4. **Find the UART service:**
   - Service UUID: `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
   - TX Characteristic: `6E400003-...` (receive data from device)
   - RX Characteristic: `6E400002-...` (send data to device)

5. **Enable notifications** on TX characteristic

### Using Serial Bluetooth Terminal

1. Open Serial Bluetooth Terminal app
2. Scan and connect to gateway
3. Select "Text mode" for communication

**Troubleshooting:**
- Can't find device? → Check power, verify LED is blinking
- Connection fails? → Move closer, restart device
- See [BLE Troubleshooting](Technical_Guides/TROUBLESHOOTING.md#ble-connection-issues)

---

## Step 3: Configure Network (2 minutes)

Choose **WiFi** (easier) or **Ethernet** (more stable):

### Option A: WiFi Configuration

Send this JSON command via BLE:

```json
{
  "cmd": "update_config",
  "data": {
    "wifi": {
      "enabled": true,
      "ssid": "YOUR_WIFI_NAME",
      "password": "YOUR_WIFI_PASSWORD"
    },
    "ethernet": {
      "enabled": false
    },
    "communication": {
      "mode": "wifi_only"
    }
  }
}
```

**Expected response:**
```json
{
  "status": "success",
  "message": "Configuration updated",
  "wifi_status": "connected",
  "ip_address": "192.168.1.100"
}
```

### Option B: Ethernet Configuration

1. **Connect Ethernet cable** to gateway and router
2. Send this JSON command via BLE:

```json
{
  "cmd": "update_config",
  "data": {
    "ethernet": {
      "enabled": true,
      "dhcp": true
    },
    "wifi": {
      "enabled": false
    },
    "communication": {
      "mode": "ethernet_only"
    }
  }
}
```

**Expected response:**
```json
{
  "status": "success",
  "message": "Configuration updated",
  "ethernet_status": "connected",
  "ip_address": "192.168.1.101"
}
```

### Option C: Dual Network (Recommended for Production)

Both WiFi and Ethernet enabled with automatic failover:

```json
{
  "cmd": "update_config",
  "data": {
    "wifi": {
      "enabled": true,
      "ssid": "YOUR_WIFI_NAME",
      "password": "YOUR_WIFI_PASSWORD"
    },
    "ethernet": {
      "enabled": true,
      "dhcp": true
    },
    "communication": {
      "mode": "auto",
      "prefer_ethernet": true
    }
  }
}
```

---

## Step 4: Add Your First Modbus Device (1 minute)

Send this command to add a Modbus device:

```json
{
  "cmd": "create_device",
  "data": {
    "name": "Temperature Sensor",
    "slave_id": 1,
    "protocol": "rtu",
    "baud_rate": 9600,
    "data_bits": 8,
    "parity": "N",
    "stop_bits": 1,
    "enabled": true
  }
}
```

**Expected response:**
```json
{
  "status": "success",
  "message": "Device created",
  "device_id": 1,
  "device": {
    "id": 1,
    "name": "Temperature Sensor",
    "slave_id": 1,
    "enabled": true
  }
}
```

**Save the device_id!** You'll need it for the next step.

---

## Step 5: Add Registers to Read (1 minute)

Add a holding register to read temperature data:

```json
{
  "cmd": "create_register",
  "data": {
    "device_id": 1,
    "name": "Temperature",
    "address": 0,
    "function_code": 3,
    "data_type": "FLOAT32_ABCD",
    "access": "R",
    "scale": 1.0,
    "offset": 0.0,
    "unit": "°C",
    "enabled": true
  }
}
```

**Common data types:**
- `INT16` - 16-bit signed integer
- `UINT16` - 16-bit unsigned integer
- `INT32_ABCD` - 32-bit integer (big-endian)
- `FLOAT32_ABCD` - 32-bit float (big-endian)
- `FLOAT32_CDAB` - 32-bit float (little-endian)

See [Modbus Data Types](Technical_Guides/MODBUS_DATATYPES.md) for complete list.

**Expected response:**
```json
{
  "status": "success",
  "message": "Register created",
  "register_id": 1
}
```

---

## Step 6: Start Data Streaming (30 seconds)

Enable real-time data streaming via BLE:

```json
{
  "cmd": "start_stream",
  "data": {
    "device_id": 1
  }
}
```

You should now see live data notifications:

```json
{
  "type": "stream_data",
  "device_id": 1,
  "device_name": "Temperature Sensor",
  "timestamp": 1700000000,
  "registers": [
    {
      "register_id": 1,
      "name": "Temperature",
      "value": 23.5,
      "unit": "°C",
      "status": "success"
    }
  ]
}
```

**To stop streaming:**
```json
{
  "cmd": "stop_stream",
  "data": {
    "device_id": 1
  }
}
```

---

## Step 7: Configure Cloud Publishing (Optional)

### MQTT Configuration

```json
{
  "cmd": "update_config",
  "data": {
    "mqtt": {
      "enabled": true,
      "broker": "mqtt.example.com",
      "port": 1883,
      "username": "your_username",
      "password": "your_password",
      "client_id": "gateway-001",
      "topic_prefix": "factory/line1",
      "publish_interval": 5000
    }
  }
}
```

### HTTP Configuration

```json
{
  "cmd": "update_config",
  "data": {
    "http_config": {
      "enabled": true,
      "url": "https://api.example.com/data",
      "method": "POST",
      "interval": 5000,
      "headers": {
        "Authorization": "Bearer YOUR_TOKEN",
        "Content-Type": "application/json"
      }
    }
  }
}
```

---

## Quick Verification Checklist

- [ ] Device powered on and LED indicators working
- [ ] Connected to gateway via BLE
- [ ] Network configured (WiFi or Ethernet)
- [ ] Device has IP address
- [ ] Modbus device added successfully
- [ ] Registers configured
- [ ] Data streaming works via BLE
- [ ] (Optional) MQTT/HTTP publishing configured

---

## Common Quick Fixes

### "Can't connect via BLE"
1. Check device is powered on (LED should blink)
2. Move closer to device (< 5 meters)
3. Restart BLE on your phone
4. Restart gateway device
5. See [BLE Troubleshooting](Technical_Guides/TROUBLESHOOTING.md#ble-connection-issues)

### "Network won't connect"
1. Verify WiFi credentials (case-sensitive!)
2. Check router is accessible
3. For Ethernet: verify cable is connected and LED on RJ45 is lit
4. Send `{"cmd": "get_status"}` to check network status
5. See [Network Troubleshooting](Technical_Guides/TROUBLESHOOTING.md#network-connectivity-issues)

### "No data from Modbus device"
1. Verify Modbus device is powered on
2. Check wiring: A, B, GND connections
3. Verify baud rate matches your device
4. Confirm slave ID is correct
5. Try reading a different register address
6. Enable logging: `{"cmd": "set_log_level", "data": {"level": "DEBUG"}}`
7. See [Modbus Troubleshooting](Technical_Guides/TROUBLESHOOTING.md#modbus-communication-problems)

### "Data looks wrong"
1. Check `data_type` - try different endianness (ABCD vs CDAB)
2. Verify register `address` is correct
3. Check `function_code` (3 = holding register, 4 = input register)
4. See [Modbus Data Types Guide](Technical_Guides/MODBUS_DATATYPES.md)

---

## Next Steps

Now that your gateway is running, explore these topics:

### Essential Reading
1. **[API Reference](API_Reference/API.md)** - Complete command reference
2. **[Best Practices](BEST_PRACTICES.md)** - Recommended configurations
3. **[Network Configuration](Technical_Guides/NETWORK_CONFIGURATION.md)** - Advanced network setup

### Advanced Topics
4. **[MQTT Publish Modes](Technical_Guides/MQTT_PUBLISH_MODES_DOCUMENTATION.md)** - Optimize MQTT payload
5. **[Register Calibration](Technical_Guides/REGISTER_CALIBRATION_DOCUMENTATION.md)** - Scale and calibrate sensors
6. **[Batch Operations](API_Reference/API.md#batch-operations)** - Configure multiple devices at once
7. **[Troubleshooting Guide](Technical_Guides/TROUBLESHOOTING.md)** - Comprehensive problem solving

### Code Examples
- **JavaScript:** [API.md - JavaScript Example](API_Reference/API.md#javascript-example)
- **Python:** [API.md - Python Example](API_Reference/API.md#python-example)
- **Flutter/Dart:** [API.md - Flutter Example](API_Reference/API.md#flutter-example)

---

## Configuration Templates

### Factory Default Configuration

```json
{
  "cmd": "update_config",
  "data": {
    "wifi": {
      "enabled": true,
      "ssid": "FACTORY_WIFI",
      "password": "password123"
    },
    "ethernet": {
      "enabled": false
    },
    "communication": {
      "mode": "wifi_only"
    },
    "mqtt": {
      "enabled": false
    },
    "http_config": {
      "enabled": false
    },
    "modbus_rtu": {
      "enabled": true,
      "timeout": 1000,
      "retry_count": 3
    }
  }
}
```

### Production Configuration (High Availability)

```json
{
  "cmd": "update_config",
  "data": {
    "wifi": {
      "enabled": true,
      "ssid": "PRODUCTION_WIFI",
      "password": "secure_password"
    },
    "ethernet": {
      "enabled": true,
      "dhcp": true
    },
    "communication": {
      "mode": "auto",
      "prefer_ethernet": true,
      "failover_delay": 5000
    },
    "mqtt": {
      "enabled": true,
      "broker": "mqtt.example.com",
      "port": 8883,
      "use_ssl": true,
      "keepalive": 60,
      "qos": 1,
      "publish_interval": 1000
    },
    "modbus_rtu": {
      "enabled": true,
      "timeout": 500,
      "retry_count": 3,
      "inter_frame_delay": 100
    }
  }
}
```

---

## Useful Commands Reference

### Status & Info
```json
{"cmd": "get_status"}                    // Get system status
{"cmd": "get_config"}                    // Get current configuration
{"cmd": "get_devices"}                   // List all devices
{"cmd": "get_device", "data": {"id": 1}} // Get specific device
```

### Device Management
```json
{"cmd": "create_device", "data": {...}}   // Add new device
{"cmd": "update_device", "data": {...}}   // Update device
{"cmd": "delete_device", "data": {"id": 1}} // Remove device
```

### Register Management
```json
{"cmd": "create_register", "data": {...}} // Add register
{"cmd": "update_register", "data": {...}} // Update register
{"cmd": "delete_register", "data": {"id": 1}} // Remove register
```

### Data Access
```json
{"cmd": "read_device", "data": {"device_id": 1}}  // Read all registers
{"cmd": "start_stream", "data": {"device_id": 1}} // Start streaming
{"cmd": "stop_stream", "data": {"device_id": 1}}  // Stop streaming
```

### System Operations
```json
{"cmd": "restart"}                       // Restart gateway
{"cmd": "factory_reset"}                 // Reset to defaults (WARNING!)
{"cmd": "set_log_level", "data": {"level": "DEBUG"}} // Enable debug logs
```

---

## Getting Help

**Have questions?** Check these resources:

- **[FAQ](FAQ.md)** - Frequently asked questions
- **[Troubleshooting Guide](Technical_Guides/TROUBLESHOOTING.md)** - Comprehensive problem solving
- **[API Reference](API_Reference/API.md)** - Complete command documentation
- **[Hardware Specs](Technical_Guides/HARDWARE.md)** - LED indicators, pinout, specs

**Still stuck?** See the [Troubleshooting Guide](Technical_Guides/TROUBLESHOOTING.md) for detailed diagnosis steps.

---

**Document Version:** 1.0
**Last Updated:** November 22, 2025
**Firmware Version:** 2.3.3
**Maintainer:** Kemal

[← Back to Documentation Index](README.md)
