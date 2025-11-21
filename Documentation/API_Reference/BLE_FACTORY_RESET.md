# BLE Factory Reset API Reference

**Version:** 1.0.0
**Last Updated:** November 21, 2025
**Component:** BLE CRUD Handler - System Operations

---

## 📋 Overview

The **Factory Reset** command provides a simple way to reset the gateway to factory defaults via BLE. This command:

- ✅ **Clears all device configurations** (devices.json)
- ✅ **Resets server config to defaults** (WiFi, Ethernet, MQTT, HTTP)
- ✅ **Resets logging config to defaults**
- ✅ **Automatically restarts the device**

⚠️ **WARNING:** This operation is **IRREVERSIBLE**. All configured devices, registers, and network settings will be lost!

---

## 🚀 Command Reference

### `factory_reset`

**Description:** Reset gateway to factory defaults and restart device.

**Operation:** `system`
**Type:** `factory_reset`

---

## 📤 Request Format

```json
{
  "op": "system",
  "type": "factory_reset",
  "reason": "Optional reason for reset"
}
{"op":"system","type":"factory_reset","reason":"Optional reason for reset"}
```

### Request Fields

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `op` | string | ✅ Yes | - | Must be `"system"` |
| `type` | string | ✅ Yes | - | Must be `"factory_reset"` |
| `reason` | string | ❌ No | `"No reason provided"` | Optional reason for audit trail (logged to Serial) |

---

## 📥 Response Format

### Success Response

```json
{
  "status": "ok",
  "message": "Factory reset initiated. Device will restart in 3 seconds.",
  "configs_cleared": [
    "devices.json",
    "server_config.json",
    "logging_config.json"
  ],
  "restart_in_ms": 3000
}
```

### Response Fields

| Field | Type | Description |
|-------|------|-------------|
| `status` | string | Always `"ok"` for successful reset initiation |
| `message` | string | Human-readable confirmation message |
| `configs_cleared` | array | List of configuration files that will be cleared |
| `restart_in_ms` | number | Milliseconds until device restart (3000ms = 3 seconds) |

---

## 🔄 Reset Sequence

When the command is received, the gateway performs the following steps:

1. **Stop Modbus Services** (RTU + TCP)
2. **Disconnect Network Services** (MQTT + HTTP)
3. **Clear Device Configurations** → Empty `devices.json`
4. **Reset Server Config** → Default WiFi/Ethernet/MQTT/HTTP settings
5. **Reset Logging Config** → Default retention/interval
6. **Restart Device** → ESP.restart() after 3 seconds

---

## 📋 Default Configurations After Reset

### Server Config (`server_config.json`)

```json
{
  "communication": {
    "mode": "ETH"
  },
  "protocol": "mqtt",
  "wifi": {
    "enabled": true,
    "ssid": "",
    "password": ""
  },
  "ethernet": {
    "enabled": true,
    "use_dhcp": true,
    "static_ip": "",
    "gateway": "",
    "subnet": ""
  },
  "mqtt_config": {
    "enabled": true,
    "broker_address": "broker.hivemq.com",
    "broker_port": 1883,
    "client_id": "",
    "username": "",
    "password": "",
    "topic_publish": "v1/devices/me/telemetry",
    "topic_subscribe": "",
    "keep_alive": 60,
    "clean_session": true,
    "use_tls": false,
    "publish_mode": "default",
    "default_mode": {
      "enabled": true,
      "topic_publish": "v1/devices/me/telemetry",
      "topic_subscribe": "",
      "interval": 5,
      "interval_unit": "s"
    },
    "customize_mode": {
      "enabled": false,
      "custom_topics": []
    }
  },
  "http_config": {
    "enabled": false,
    "endpoint_url": "https://api.example.com/data",
    "method": "POST",
    "body_format": "json",
    "timeout": 5000,
    "retry": 3,
    "interval": 5,
    "interval_unit": "s",
    "headers": {
      "Authorization": "Bearer token",
      "Content-Type": "application/json"
    }
  }
}
```

**Key Features:**
- **Communication Mode:** Ethernet primary (`"ETH"`)
- **WiFi:** Enabled but unconfigured (empty SSID/password)
- **Ethernet:** DHCP enabled, static IP fields empty
- **MQTT Publish Modes:**
  - **Default Mode** (enabled): Single topic, 5-second interval
  - **Customize Mode** (disabled): Per-device custom topics
- **HTTP:** Disabled by default, example endpoint configured

### Logging Config (`logging_config.json`)

```json
{
  "logging_ret": "1w",
  "logging_interval": "5m"
}
```

### Device Config (`devices.json`)

```json
{}
```

*(Empty - all devices deleted)*

---

## 🧪 Usage Examples

### Example 1: Basic Factory Reset

**Request:**
```json
{
  "op": "system",
  "type": "factory_reset"
}
```

**Response:**
```json
{
  "status": "ok",
  "message": "Factory reset initiated. Device will restart in 3 seconds.",
  "configs_cleared": [
    "devices.json",
    "server_config.json",
    "logging_config.json"
  ],
  "restart_in_ms": 3000
}
```

**Serial Log:**
```
========================================
[FACTORY RESET] ⚠️  INITIATED by BLE client
[FACTORY RESET] Reason: No reason provided
[FACTORY RESET] ⚠️  This will ERASE all device, server, and network configurations!
========================================

[FACTORY RESET] ========================================
[FACTORY RESET] Starting graceful shutdown sequence...
[FACTORY RESET] ========================================
[FACTORY RESET] [1/6] Stopping Modbus services...
[FACTORY RESET] RTU service stopped
[FACTORY RESET] TCP service stopped
[FACTORY RESET] [2/6] Disconnecting network services...
[FACTORY RESET] MQTT disconnected
[FACTORY RESET] HTTP stopped
[FACTORY RESET] [3/6] Clearing device configurations...
[FACTORY RESET] devices.json cleared
[FACTORY RESET] [4/6] Resetting server config to defaults...
[FACTORY RESET] server_config.json reset to defaults
[FACTORY RESET] [5/6] Resetting logging config to defaults...
[FACTORY RESET] logging_config.json reset to defaults
[FACTORY RESET] [6/6] All configurations cleared successfully
[FACTORY RESET] ========================================
[FACTORY RESET] Device will restart in 3 seconds...
[FACTORY RESET] ========================================
[FACTORY RESET] ✅ RESTARTING NOW...
```

---

### Example 2: Factory Reset with Reason

**Request:**
```json
{
  "op": "system",
  "type": "factory_reset",
  "reason": "Deployment to new customer site"
}
```

**Response:**
```json
{
  "status": "ok",
  "message": "Factory reset initiated. Device will restart in 3 seconds.",
  "configs_cleared": [
    "devices.json",
    "server_config.json",
    "logging_config.json"
  ],
  "restart_in_ms": 3000
}
```

**Serial Log:**
```
========================================
[FACTORY RESET] ⚠️  INITIATED by BLE client
[FACTORY RESET] Reason: Deployment to new customer site
[FACTORY RESET] ⚠️  This will ERASE all device, server, and network configurations!
========================================
```

---

## 📱 Mobile App Integration

### JavaScript Example

```javascript
async function factoryReset(reason = '') {
  // Show confirmation dialog
  const confirmed = confirm(
    '⚠️ FACTORY RESET WARNING\n\n' +
    'This will ERASE all configurations:\n' +
    '• All Modbus devices and registers\n' +
    '• WiFi/Ethernet settings\n' +
    '• MQTT/HTTP server settings\n\n' +
    'The device will restart automatically.\n\n' +
    'Are you sure you want to continue?'
  );

  if (!confirmed) {
    return;
  }

  // Send factory reset command
  const command = {
    op: 'system',
    type: 'factory_reset',
    reason: reason || 'Reset via mobile app'
  };

  try {
    const response = await bleManager.sendCommand(command);

    if (response.status === 'ok') {
      alert(
        '✅ Factory Reset Initiated\n\n' +
        `Device will restart in ${response.restart_in_ms / 1000} seconds.\n\n` +
        'Please reconnect after restart to configure the device.'
      );

      // Auto-disconnect after 2 seconds
      setTimeout(() => {
        bleManager.disconnect();
      }, 2000);
    }
  } catch (error) {
    alert('❌ Factory reset failed: ' + error.message);
  }
}

// Usage
factoryReset('Redeployment to factory floor');
```

---

## ⚠️ Important Notes

### 1. **No Confirmation Required**
This is a **simple single-command** reset without two-step confirmation. Make sure your mobile app shows a clear warning dialog before sending the command.

### 2. **Automatic Restart**
The device will **automatically restart** 3 seconds after the command is received. You don't need to manually power cycle the device.

### 3. **BLE Reconnection**
After restart, the device will:
- ✅ Keep the same BLE device name
- ✅ Start advertising again
- ✅ Have **empty device list** (no configured devices)
- ✅ Have **default server settings** (broker.hivemq.com)

### 4. **Audit Trail**
All factory reset operations are logged to Serial Monitor with:
- Timestamp
- Reason (if provided)
- Step-by-step progress
- Success confirmation

### 5. **What is NOT Reset**
The following are **preserved** after factory reset:
- ✅ Firmware version
- ✅ ESP32 hardware settings
- ✅ BLE device name
- ✅ System time (RTC)
- ✅ MAC address

---

## 🔒 Best Practices

### For Mobile App Developers

1. **Always show confirmation dialog** before sending reset command
2. **Include clear warning** about data loss
3. **Auto-disconnect** after receiving response
4. **Show restart countdown** in UI (3 seconds)
5. **Provide "reconnect" button** after expected restart time

### For System Administrators

1. **Document reset reason** in command payload for audit trail
2. **Backup current config** before reset (use `read devices` command)
3. **Monitor Serial log** during reset process
4. **Wait 10 seconds** after restart before reconnecting
5. **Reconfigure network settings** immediately after reset

---

## 📊 Comparison: Manual vs BLE Reset

| Feature | Manual (`clearAllConfigurations()`) | BLE Factory Reset |
|---------|-------------------------------------|-------------------|
| **Trigger** | Code uncomment + upload | BLE command |
| **User Access** | Requires firmware modification | Mobile app button |
| **Graceful Shutdown** | ❌ No | ✅ Yes (stops services first) |
| **Server Config Reset** | ❌ No (only devices) | ✅ Yes (all configs) |
| **Logging Config Reset** | ❌ No | ✅ Yes |
| **Auto Restart** | ❌ No | ✅ Yes (3 seconds) |
| **Audit Trail** | ❌ No | ✅ Yes (Serial log) |
| **Production Ready** | ❌ No (debug only) | ✅ Yes |

---

## 🐛 Troubleshooting

### Issue: Device doesn't restart after reset

**Solution:**
- Check Serial Monitor for errors
- Manually power cycle if needed
- Verify ESP.restart() works on your board

### Issue: Configs not cleared after restart

**Solution:**
- Check LittleFS write errors in Serial log
- Verify SD card is working
- Re-send factory reset command

### Issue: BLE connection drops before receiving response

**Solution:**
- Normal behavior - device restarts after 3 seconds
- Wait 10 seconds and reconnect
- Check Serial log to confirm reset completed

---

## 📚 Related Documentation

- **BLE CRUD API:** `/Documentation/API_Reference/API.md`
- **Device Control API:** `/Documentation/API_Reference/BLE_DEVICE_CONTROL.md`
- **Server Configuration:** `/Documentation/Technical_Guides/SERVER_CONFIG.md`
- **Troubleshooting:** `/Documentation/Technical_Guides/TROUBLESHOOTING.md`

---

**Made with ❤️ by SURIOTA R&D Team**
*Empowering Industrial IoT Solutions*
