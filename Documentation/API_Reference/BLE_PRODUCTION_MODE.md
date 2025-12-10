# BLE API: Production Mode Control

**Version:** 2.5.34
**Last Updated:** December 10, 2025
**Feature:** Runtime Production Mode Switching via BLE

---

## 📋 Overview

The **Production Mode Control** feature allows applications to switch the gateway between **Development Mode (0)** and **Production Mode (1)** via BLE command, and read the current mode status without requiring firmware re-upload.

### Key Benefits
- ✅ **No Firmware Re-upload** - Switch modes remotely via BLE
- ✅ **Persistent Storage** - Mode saved to `/logging_config.json` and survives reboots
- ✅ **Automatic Restart** - Device restarts automatically after mode change
- ✅ **Full Response Data** - Get previous/current mode and confirmation
- ✅ **Status Read** - Check current mode, log level, and sync status anytime

---

## 🎯 Mode Comparison

| Feature | Development Mode (0) | Production Mode (1) |
|---------|---------------------|---------------------|
| **BLE Startup** | Always ON at boot | Button-controlled (starts OFF) |
| **Debug Output** | Full verbose logging | Minimal JSON logs only |
| **Modbus Polling Data** | JSON output per device | Silent (no output) |
| **MQTT Publish Data** | Full JSON payload shown | Silent (no output) |
| **Log Level** | INFO (ERROR + WARN + INFO) | ERROR (critical only) |
| **Serial Output** | Verbose development logs | Clean production JSON |
| **Use Case** | Development, debugging, testing | Production deployment (1-5+ years) |

---

## 📡 BLE Command Format

### Command Type
**Operation:** `control`
**Type:** `set_production_mode`

### Request Structure
```json
{
  "op": "control",
  "type": "set_production_mode",
  "mode": 0
}
```

### Parameters

| Parameter | Type | Required | Valid Values | Description |
|-----------|------|----------|--------------|-------------|
| `op` | String | ✅ Yes | `"control"` | Operation type |
| `type` | String | ✅ Yes | `"set_production_mode"` | Command identifier |
| `mode` | Integer | ✅ Yes | `0` or `1` | Target mode: 0 = Development, 1 = Production |

---

## 📥 Response Format

### Success Response
```json
{
  "status": "ok",
  "previous_mode": 1,
  "current_mode": 0,
  "mode_name": "Development",
  "message": "Production mode updated. Device will restart in 2 seconds...",
  "persistent": true,
  "restarting": true
}
```

### Response Fields

| Field | Type | Description |
|-------|------|-------------|
| `status` | String | Always `"ok"` on success |
| `previous_mode` | Integer | Mode before change (0 or 1) |
| `current_mode` | Integer | New mode after change (0 or 1) |
| `mode_name` | String | Human-readable mode name ("Development" or "Production") |
| `message` | String | Confirmation message with restart notice |
| `persistent` | Boolean | `true` if mode was saved to config file |
| `restarting` | Boolean | Always `true` - indicates device will restart |

### Error Response
```json
{
  "status": "error",
  "error": "mode parameter required (0 = Development, 1 = Production)"
}
```

**Common Errors:**
- Missing `mode` parameter
- Invalid mode value (not 0 or 1)
- Config save failure (rare)

---

## 📊 Read Production Mode Status

### Command Type
**Operation:** `read`
**Type:** `production_mode`

### Request Structure
```json
{
  "op": "read",
  "type": "production_mode"
}
```

### Parameters

| Parameter | Type | Required | Valid Values | Description |
|-----------|------|----------|--------------|-------------|
| `op` | String | ✅ Yes | `"read"` | Operation type |
| `type` | String | ✅ Yes | `"production_mode"` | Command identifier |

### Success Response
```json
{
  "status": "ok",
  "current_mode": 0,
  "mode_name": "Development",
  "saved_mode": 0,
  "saved_mode_name": "Development",
  "is_synced": true,
  "compile_time_default": 0,
  "firmware_version": "2.3.5",
  "log_level": 3,
  "log_level_name": "INFO",
  "uptime_ms": 45230
}
```

### Response Fields

| Field | Type | Description |
|-------|------|-------------|
| `status` | String | Always `"ok"` on success |
| `current_mode` | Integer | Current runtime mode (0 = Dev, 1 = Production) |
| `mode_name` | String | Human-readable current mode name |
| `saved_mode` | Integer | Mode saved in `/logging_config.json` |
| `saved_mode_name` | String | Human-readable saved mode name |
| `is_synced` | Boolean | `true` if current_mode == saved_mode |
| `compile_time_default` | Integer | Compile-time default from firmware |
| `firmware_version` | String | Current firmware version |
| `log_level` | Integer | Current log level (0-5) |
| `log_level_name` | String | Log level name (NONE/ERROR/WARN/INFO/DEBUG/VERBOSE) |
| `uptime_ms` | Integer | Device uptime in milliseconds |

### Use Cases

**1. Check Current Mode Before Switching:**
```javascript
// Application pseudo-code
const status = await bleDevice.read("production_mode");
if (status.current_mode === 0) {
  console.log("Device in Development mode, switching to Production...");
  await bleDevice.control("set_production_mode", {mode: 1});
}
```

**2. Verify Mode After Restart:**
```javascript
// After sending set_production_mode command
await sleep(8000); // Wait for device restart
const status = await bleDevice.read("production_mode");
if (status.is_synced && status.current_mode === 1) {
  console.log("✅ Production mode applied successfully");
}
```

**3. Monitor Log Level:**
```javascript
const status = await bleDevice.read("production_mode");
console.log(`Log Level: ${status.log_level_name} (${status.log_level})`);
// Production mode should show: "ERROR (1)"
// Development mode should show: "INFO (3)"
```

---

## 🔄 Complete Flow (Set Mode)

### 1. Application Sends Command
```json
{
  "op": "control",
  "type": "set_production_mode",
  "mode": 0
}
```

### 2. Gateway Processing
```
[✓] Validate mode parameter
[✓] Update runtime variable (g_productionMode)
[✓] Save to /logging_config.json
[✓] Send response to BLE client
[✓] Wait 2 seconds (ensure response delivered)
[✓] Execute ESP.restart()
```

### 3. Device Restart
```
[~5-8 seconds boot time]
```

### 4. Gateway Boots with New Mode
```
[SYSTEM] Boot sequence starting...
[SYSTEM] Loading logging config...
[SYSTEM] Production mode loaded from config: 0 (Development)
[SYSTEM] Mode remembered from previous session ✅
```

---

## 💡 Usage Examples

### Example 1: Switch to Development Mode
**Use Case:** Enable verbose logging for debugging

**Request:**
```json
{
  "op": "control",
  "type": "set_production_mode",
  "mode": 0
}
```

**Response:**
```json
{
  "status": "ok",
  "previous_mode": 1,
  "current_mode": 0,
  "mode_name": "Development",
  "message": "Production mode updated. Device will restart in 2 seconds...",
  "persistent": true,
  "restarting": true
}
```

**After Restart:**
- Full debug logs appear on serial
- Modbus polling data shown as JSON
- MQTT publish payloads displayed
- All development features active

---

### Example 2: Switch to Production Mode
**Use Case:** Deploy device for long-term operation

**Request:**
```json
{
  "op": "control",
  "type": "set_production_mode",
  "mode": 1
}
```

**Response:**
```json
{
  "status": "ok",
  "previous_mode": 0,
  "current_mode": 1,
  "mode_name": "Production",
  "message": "Production mode updated. Device will restart in 2 seconds...",
  "persistent": true,
  "restarting": true
}
```

**After Restart:**
- Minimal JSON logs only (heartbeat every 60s)
- Clean production serial output
- No debug/verbose output
- Optimized for 1-5+ year deployment

---

## ⏱️ Timeline & Expectations

| Time | Event |
|------|-------|
| t=0s | BLE command received |
| t=0.1s | Mode saved to config file |
| t=0.2s | Response sent to application |
| t=2s | ESP.restart() executed |
| t=2-7s | Device rebooting (BLE disconnected) |
| t=7s | Device boot complete |
| t=7.1s | Mode loaded from config ✅ |
| t=8s | BLE ready for reconnection |
| t=10s | All services running with new mode |

**Expected Behavior:**
1. ⚠️ **BLE connection will drop** during restart (~2-10 seconds)
2. ✅ **Mode persists** across reboots and power cycles
3. ✅ **Application can reconnect** after ~10 seconds
4. ✅ **Device boots in new mode** automatically

---

## 🛡️ Application Best Practices

### 1. Handle Connection Loss
```javascript
// Pseudo-code example
async function setProductionMode(mode) {
  // Send command
  const response = await ble.sendCommand({
    op: "control",
    type: "set_production_mode",
    mode: mode
  });

  if (response.status === "ok" && response.restarting === true) {
    // Show user notification
    showNotification(`Switching to ${response.mode_name} mode. Device will restart...`);

    // Disconnect BLE (device will restart anyway)
    await ble.disconnect();

    // Wait for device to restart (10 seconds recommended)
    await sleep(10000);

    // Reconnect to device
    await ble.reconnect();

    // Verify mode changed
    const status = await getDeviceStatus();
    if (status.production_mode === mode) {
      showNotification("Mode changed successfully!");
    }
  }
}
```

### 2. UI/UX Recommendations
- ✅ Show loading indicator during restart (10 seconds)
- ✅ Display countdown timer ("Restarting... 8s remaining")
- ✅ Confirm mode change with user before sending command
- ✅ Auto-reconnect after device restarts
- ✅ Show current mode in UI (Development/Production badge)

### 3. Error Handling
```javascript
try {
  const response = await setProductionMode(0);

  if (response.status === "error") {
    showError(response.error);
    return;
  }

  // Handle successful mode change...

} catch (error) {
  if (error.code === "BLE_DISCONNECTED") {
    // Expected during restart
    showInfo("Device is restarting. Please wait...");
  } else {
    showError("Failed to change mode: " + error.message);
  }
}
```

### 4. Validation Before Sending
```javascript
function validateMode(mode) {
  if (mode !== 0 && mode !== 1) {
    throw new Error("Invalid mode. Use 0 (Development) or 1 (Production)");
  }
  return true;
}
```

---

## 🔍 Verification & Testing

### Test Scenario 1: Mode Persistence
1. Set mode to Development (0) via BLE
2. Wait for device restart
3. **Power off** the device completely
4. **Power on** the device
5. ✅ Verify device boots in Development mode
6. ✅ Check serial output shows verbose logs

### Test Scenario 2: Toggle Mode
1. Current mode: Production (1)
2. Send command: `mode = 0`
3. ✅ Verify response shows `previous_mode: 1, current_mode: 0`
4. Wait for restart
5. Send command: `mode = 1`
6. ✅ Verify response shows `previous_mode: 0, current_mode: 1`
7. ✅ Verify clean production logs after restart

### Test Scenario 3: Invalid Input
1. Send command: `mode = 2` (invalid)
2. ✅ Verify error response
3. ✅ Device does NOT restart
4. ✅ Mode unchanged

---

## 📊 Production Output Format (Mode = 1)

When in **Production Mode (1)**, the serial output is minimal JSON format:

### Boot Message
```json
{"ts":"2025-11-26T09:30:47","t":"SYS","e":"BOOT","v":"2.3.4","id":"SRT-MGATE-1210","mem":{"d":112352,"p":8248068}}
```

### Heartbeat (every 60 seconds)
```json
{"ts":"2025-11-26T09:31:47","t":"HB","up":60,"mem":{"d":112368,"p":8263688},"net":"ETH","proto":"mqtt","st":"OK","err":0,"mb":{"ok":125,"er":2}}
```

### Network Change
```json
{"ts":"2025-11-26T09:32:15","t":"NET","up":88,"net":"WIFI","rc":1}
```

### Error
```json
{"ts":"2025-11-26T09:33:20","t":"ERR","up":153,"m":"MQTT","msg":"Connection timeout","cnt":1}
```

---

## 📝 Development Output Format (Mode = 0)

When in **Development Mode (0)**, full verbose logging is enabled:

### Modbus RTU Polling
```
[RTU] POLLED DATA: {"device_id":"D001","device_name":"TempSensor","protocol":"RTU","slave_id":1,"timestamp":123456,"registers":[{"name":"Temperature","address":0,"function_code":3,"value":25.5,"unit":"°C"}],"success_count":1,"failed_count":0}
```

### MQTT Publish
```
[MQTT] PUBLISH REQUEST - Default Mode
  Topic: suriota/devices/D001/data
  Size: 345 bytes
  Payload: {"device_id":"D001","registers":[{"name":"Temperature","value":25.5}]}
  Broker: mqtt.example.com:1883
  Packet size: 378/16384 bytes
```

---

## 🔐 Security Considerations

1. **Authentication:** Ensure BLE connection is authenticated before allowing mode changes
2. **Authorization:** Only authorized users/apps should send `set_production_mode` commands
3. **Audit Trail:** Device logs all mode changes to serial output with timestamp
4. **Persistent Storage:** Mode saved to `/logging_config.json` (encrypted filesystem recommended)

---

## 🐛 Troubleshooting

### Issue: Device doesn't restart after command
**Cause:** Command not received or processed
**Solution:** Check BLE connection, verify command format, check serial logs

### Issue: Mode doesn't persist after power cycle
**Cause:** Config file not saved or filesystem error
**Solution:** Check `persistent: true` in response, verify LittleFS mounted correctly

### Issue: BLE reconnection fails after restart
**Cause:** Reconnecting too quickly
**Solution:** Wait at least 10 seconds before reconnecting, increase timeout

### Issue: Invalid mode error
**Cause:** Incorrect parameter value
**Solution:** Ensure `mode` is exactly `0` or `1` (integer, not string)

---

## 📚 Related Documentation

- [BLE_API.md](./BLE_API.md) - Complete BLE CRUD API reference
- [LOGGING.md](../Technical_Guides/LOGGING.md) - Logging system details
- [PRODUCTION_LOGGING.md](../Technical_Guides/PRODUCTION_LOGGING.md) - Production mode logging guide

---

## 📞 Support

For issues or questions:
- **Email:** support@suriota.com
- **GitHub:** https://github.com/suriota/SRT-MGATE-1210-Firmware/issues

---

**Made with ❤️ by SURIOTA R&D Team** | Industrial IoT Solutions
