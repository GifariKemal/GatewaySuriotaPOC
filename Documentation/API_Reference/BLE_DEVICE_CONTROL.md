# BLE Device Control API Reference

**Version:** 1.0.0
**Last Updated:** November 26, 2025
**Component:** BLE CRUD Handler - Device Control Operations

---

## 📋 Table of Contents

1. [Overview](#overview)
2. [Command Structure](#command-structure)
3. [Commands Reference](#commands-reference)
   - [enable_device](#1-enable_device)
   - [disable_device](#2-disable_device)
   - [get_device_status](#3-get_device_status)
   - [get_all_device_status](#4-get_all_device_status)
4. [Response Formats](#response-formats)
5. [Error Handling](#error-handling)
6. [Usage Examples](#usage-examples)
7. [Best Practices](#best-practices)

---

## Overview

The **Device Control API** provides BLE commands for flexible management of Modbus devices (both RTU and TCP). This enhancement allows mobile apps to:

- **Enable/disable devices remotely** via BLE commands
- **Track device health metrics** (success rate, response time)
- **Monitor device status** (enabled, disabled reason, metrics)
- **Distinguish manual vs auto-disables** (manual disables are preserved, auto-disables are recovered)

### Key Features

✅ **Flexible Control**: Manual enable/disable from mobile app
✅ **Auto-Recovery**: System auto-recovers auto-disabled devices every 5 minutes
✅ **Health Metrics**: Real-time success rate, avg response time, min/max tracking
✅ **Disable Reason Tracking**: NONE, MANUAL, AUTO_RETRY, AUTO_TIMEOUT
✅ **Protocol-Agnostic**: Works for both RTU and TCP devices

---

## Command Structure

All device control commands use the following base structure:

```json
{
  "op": "control",
  "type": "<command_type>",
  "priority": "normal",
  "<additional_fields>": "..."
}
```

### Fields

- **`op`** (string, required): Must be `"control"` for device control operations
- **`type`** (string, required): Command type (`enable_device`, `disable_device`, `get_device_status`, `get_all_device_status`)
- **`priority`** (string, optional): Command priority - `"high"`, `"normal"` (default), `"low"`
- **Additional fields**: Command-specific parameters (see each command below)

---

## Commands Reference

### 1. `enable_device`

**Description:** Enable a previously disabled device and optionally clear its health metrics.

**Use Cases:**
- Re-enable a manually disabled device
- Clear metrics after maintenance/troubleshooting
- Override auto-disable (e.g., after fixing network issues)

#### Request

```json
{
  "op": "control",
  "type": "enable_device",
  "device_id": "D7A3F2",
  "clear_metrics": false
}
```

#### Request Fields

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `op` | string | ✅ Yes | - | Must be `"control"` |
| `type` | string | ✅ Yes | - | Must be `"enable_device"` |
| `device_id` | string | ✅ Yes | - | Device ID to enable (e.g., `"D7A3F2"`) |
| `clear_metrics` | boolean | ❌ No | `false` | Clear all health metrics (success counters, response times) |

#### Response (Success)

```json
{
  "status": "ok",
  "device_id": "D7A3F2",
  "message": "Device enabled",
  "metrics_cleared": false
}
```

#### Serial Log Output

```
[RTU] BLE Command: Enable device D7A3F2 (clearMetrics: false)
[RTU] Device D7A3F2 enabled (reason cleared)
```

---

### 2. `disable_device`

**Description:** Manually disable a device with a custom reason. Device will NOT be auto-recovered (preserved until manual enable).

**Use Cases:**
- Disable device during maintenance
- Temporarily disable faulty devices
- Disable devices for testing purposes

#### Request

```json
{
  "op": "control",
  "type": "disable_device",
  "device_id": "D7A3F2",
  "reason": "Under maintenance - replacing sensor"
}
```

#### Request Fields

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `op` | string | ✅ Yes | - | Must be `"control"` |
| `type` | string | ✅ Yes | - | Must be `"disable_device"` |
| `device_id` | string | ✅ Yes | - | Device ID to disable (e.g., `"D7A3F2"`) |
| `reason` | string | ❌ No | `"Manual disable via BLE"` | Reason for disabling (user-provided text) |

#### Response (Success)

```json
{
  "status": "ok",
  "device_id": "D7A3F2",
  "message": "Device disabled",
  "reason": "Under maintenance - replacing sensor"
}
```

#### Serial Log Output

```
[RTU] BLE Command: Disable device D7A3F2 (reason: Under maintenance - replacing sensor)
[RTU] Device D7A3F2 disabled (reason: MANUAL, detail: Under maintenance - replacing sensor)
```

---

### 3. `get_device_status`

**Description:** Get detailed status and health metrics for a specific device.

**Use Cases:**
- Check if device is enabled or disabled
- View device health metrics (success rate, response time)
- Troubleshoot device issues
- Monitor device performance

#### Request

```json
{
  "op": "control",
  "type": "get_device_status",
  "device_id": "D7A3F2"
}
```

#### Request Fields

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `op` | string | ✅ Yes | - | Must be `"control"` |
| `type` | string | ✅ Yes | - | Must be `"get_device_status"` |
| `device_id` | string | ✅ Yes | - | Device ID to query (e.g., `"D7A3F2"`) |

#### Response (Success)

```json
{
  "status": "ok",
  "device_status": {
    "device_id": "D7A3F2",
    "enabled": true,
    "consecutive_failures": 0,
    "retry_count": 0,
    "disable_reason": "NONE",
    "disable_reason_detail": "",
    "timeout_ms": 5000,
    "consecutive_timeouts": 0,
    "max_consecutive_timeouts": 3,
    "metrics": {
      "total_reads": 1250,
      "successful_reads": 1238,
      "failed_reads": 12,
      "success_rate": 99.04,
      "avg_response_time_ms": 245,
      "min_response_time_ms": 180,
      "max_response_time_ms": 520,
      "last_response_time_ms": 230
    }
  }
}
```

#### Response (Disabled Device)

```json
{
  "status": "ok",
  "device_status": {
    "device_id": "A1B2C3",
    "enabled": false,
    "consecutive_failures": 5,
    "retry_count": 5,
    "disable_reason": "AUTO_RETRY",
    "disable_reason_detail": "Max retries exceeded",
    "disabled_duration_ms": 120000,
    "timeout_ms": 5000,
    "consecutive_timeouts": 0,
    "max_consecutive_timeouts": 3,
    "metrics": {
      "total_reads": 87,
      "successful_reads": 82,
      "failed_reads": 5,
      "success_rate": 94.25,
      "avg_response_time_ms": 310,
      "min_response_time_ms": 250,
      "max_response_time_ms": 1500,
      "last_response_time_ms": 1500
    }
  }
}
```

#### Response Fields Explanation

| Field | Type | Description |
|-------|------|-------------|
| `device_id` | string | Device ID |
| `enabled` | boolean | `true` if device is enabled, `false` if disabled |
| `consecutive_failures` | number | Count of consecutive read failures |
| `retry_count` | number | Current retry attempt (0 if no failures) |
| `disable_reason` | string | `"NONE"`, `"MANUAL"`, `"AUTO_RETRY"`, `"AUTO_TIMEOUT"` |
| `disable_reason_detail` | string | User-provided reason (for MANUAL) or system message |
| `disabled_duration_ms` | number | *(Only if disabled)* Milliseconds since disabled |
| `timeout_ms` | number | Device timeout in milliseconds |
| `consecutive_timeouts` | number | Count of consecutive timeouts |
| `max_consecutive_timeouts` | number | Max timeouts before auto-disable |
| `metrics.total_reads` | number | Total read attempts |
| `metrics.successful_reads` | number | Successful reads |
| `metrics.failed_reads` | number | Failed reads |
| `metrics.success_rate` | number | Success rate percentage (0-100) |
| `metrics.avg_response_time_ms` | number | Average response time in milliseconds |
| `metrics.min_response_time_ms` | number | Minimum response time |
| `metrics.max_response_time_ms` | number | Maximum response time |
| `metrics.last_response_time_ms` | number | Last response time |

---

### 4. `get_all_device_status`

**Description:** Get status and health metrics for **all** devices (both RTU and TCP).

**Use Cases:**
- Dashboard overview of all devices
- Bulk health monitoring
- Identify devices with issues

#### Request

```json
{
  "op": "control",
  "type": "get_all_device_status"
}
```

#### Request Fields

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `op` | string | ✅ Yes | - | Must be `"control"` |
| `type` | string | ✅ Yes | - | Must be `"get_all_device_status"` |

*(No additional fields required)*

#### Response (Success)

```json
{
  "status": "ok",
  "rtu_devices": {
    "devices": [
      {
        "device_id": "D7A3F2",
        "enabled": true,
        "consecutive_failures": 0,
        "retry_count": 0,
        "disable_reason": "NONE",
        "disable_reason_detail": "",
        "metrics": {
          "total_reads": 1250,
          "successful_reads": 1238,
          "failed_reads": 12,
          "success_rate": 99.04,
          "avg_response_time_ms": 245
        }
      },
      {
        "device_id": "A1B2C3",
        "enabled": false,
        "consecutive_failures": 5,
        "retry_count": 5,
        "disable_reason": "AUTO_RETRY",
        "disable_reason_detail": "Max retries exceeded",
        "disabled_duration_ms": 120000,
        "metrics": {
          "total_reads": 87,
          "successful_reads": 82,
          "failed_reads": 5,
          "success_rate": 94.25,
          "avg_response_time_ms": 310
        }
      }
    ],
    "total_devices": 2
  },
  "tcp_devices": {
    "devices": [
      {
        "device_id": "E8F4G5",
        "enabled": true,
        "consecutive_failures": 0,
        "retry_count": 0,
        "disable_reason": "NONE",
        "disable_reason_detail": "",
        "metrics": {
          "total_reads": 980,
          "successful_reads": 980,
          "failed_reads": 0,
          "success_rate": 100.0,
          "avg_response_time_ms": 120
        }
      }
    ],
    "total_devices": 1
  }
}
```

#### Response Structure

- **`rtu_devices`**: Object containing RTU devices
  - **`devices`**: Array of device status objects (same format as `get_device_status`)
  - **`total_devices`**: Total count of RTU devices
- **`tcp_devices`**: Object containing TCP devices
  - **`devices`**: Array of device status objects (same format as `get_device_status`)
  - **`total_devices`**: Total count of TCP devices

---

## Response Formats

### Success Response

```json
{
  "status": "ok",
  "<command_specific_fields>": "..."
}
```

### Error Response

```json
{
  "status": "error",
  "message": "<error_description>"
}
```

### Common Error Messages

| Error Message | Cause | Solution |
|---------------|-------|----------|
| `"device_id is required"` | Missing `device_id` field | Add `device_id` to request |
| `"Device not found"` | Invalid device ID | Check device ID in devices list |
| `"Invalid protocol or service not available"` | Protocol mismatch or service not initialized | Check device protocol and service status |
| `"Failed to enable device"` | Internal error during enable | Check serial logs for details |
| `"Failed to disable device"` | Internal error during disable | Check serial logs for details |
| `"Failed to get device status"` | Internal error during status query | Check serial logs for details |

---

## Error Handling

### Request Validation

All requests are validated for:
1. ✅ Required fields present (`op`, `type`, `device_id` if applicable)
2. ✅ Device exists in configuration
3. ✅ Device protocol is valid (RTU or TCP)
4. ✅ Corresponding service is available

### Serial Log Debugging

Enable verbose logging to debug issues:

```cpp
// In Main.ino setup()
setLogLevel(LOG_DEBUG);  // or LOG_VERBOSE for more detail
```

Look for these log prefixes:
- `[RTU]` - Modbus RTU service logs
- `[TCP]` - Modbus TCP service logs
- `[CRUD]` - CRUD handler logs
- `[RTU AutoRecovery]` - Auto-recovery task logs
- `[TCP AutoRecovery]` - Auto-recovery task logs

---

## Usage Examples

### Example 1: Disable Device for Maintenance

**Scenario:** You need to replace a sensor, so you disable the device via BLE.

**Request:**
```json
{
  "op": "control",
  "type": "disable_device",
  "device_id": "D7A3F2",
  "reason": "Replacing temperature sensor"
}
```

**Response:**
```json
{
  "status": "ok",
  "device_id": "D7A3F2",
  "message": "Device disabled",
  "reason": "Replacing temperature sensor"
}
```

**What Happens:**
1. Device is immediately disabled (polling stops)
2. Disable reason is set to `MANUAL`
3. Device will NOT be auto-recovered (manual disable is preserved)
4. Serial log shows: `[RTU] Device D7A3F2 disabled (reason: MANUAL, detail: Replacing temperature sensor)`

---

### Example 2: Re-enable Device After Maintenance

**Scenario:** Sensor replacement is complete, re-enable the device and clear old metrics.

**Request:**
```json
{
  "op": "control",
  "type": "enable_device",
  "device_id": "D7A3F2",
  "clear_metrics": true
}
```

**Response:**
```json
{
  "status": "ok",
  "device_id": "D7A3F2",
  "message": "Device enabled",
  "metrics_cleared": true
}
```

**What Happens:**
1. Device is re-enabled (polling resumes)
2. Disable reason is cleared to `NONE`
3. All metrics are reset to zero (fresh start)
4. Serial log shows: `[RTU] Device D7A3F2 metrics cleared` and `[RTU] Device D7A3F2 enabled (reason cleared)`

---

### Example 3: Check Device Health

**Scenario:** You notice slow response times, check device metrics.

**Request:**
```json
{
  "op": "control",
  "type": "get_device_status",
  "device_id": "D7A3F2"
}
```

**Response:**
```json
{
  "status": "ok",
  "device_status": {
    "device_id": "D7A3F2",
    "enabled": true,
    "metrics": {
      "total_reads": 1250,
      "successful_reads": 1200,
      "failed_reads": 50,
      "success_rate": 96.0,
      "avg_response_time_ms": 450,
      "min_response_time_ms": 200,
      "max_response_time_ms": 1200
    }
  }
}
```

**Analysis:**
- ✅ Device is enabled
- ⚠️ Success rate is 96% (4% failure rate - investigate)
- ⚠️ Average response time is 450ms (higher than typical 200-300ms)
- 🔴 Max response time is 1200ms (very slow - check cable/network)

---

### Example 4: Auto-Recovery Scenario

**Scenario:** Device auto-disabled due to network issues, then network is restored.

**Initial State (Network Issue):**
```
[RTU] Device A1B2C3 read failed. Retry 5/5
[RTU] Device A1B2C3 exceeded max retries (5), disabling...
[RTU] Device A1B2C3 disabled (reason: AUTO_RETRY, detail: Max retries exceeded)
```

**5 Minutes Later (Auto-Recovery):**
```
[RTU AutoRecovery] Checking for auto-disabled devices...
[RTU AutoRecovery] Device A1B2C3 auto-disabled for 300000 ms, attempting recovery...
[RTU] Device A1B2C3 enabled (reason cleared)
[RTU AutoRecovery] Device A1B2C3 re-enabled
```

**Get Status:**
```json
{
  "op": "control",
  "type": "get_device_status",
  "device_id": "A1B2C3"
}
```

**Response (After Recovery):**
```json
{
  "status": "ok",
  "device_status": {
    "device_id": "A1B2C3",
    "enabled": true,
    "consecutive_failures": 0,
    "retry_count": 0,
    "disable_reason": "NONE",
    "disable_reason_detail": "",
    "metrics": {
      "total_reads": 92,
      "successful_reads": 87,
      "failed_reads": 5,
      "success_rate": 94.57
    }
  }
}
```

---

### Example 5: Dashboard Overview

**Scenario:** Mobile app dashboard needs overview of all devices.

**Request:**
```json
{
  "op": "control",
  "type": "get_all_device_status"
}
```

**Response:**
```json
{
  "status": "ok",
  "rtu_devices": {
    "devices": [
      {
        "device_id": "D7A3F2",
        "enabled": true,
        "metrics": {
          "success_rate": 99.04,
          "avg_response_time_ms": 245
        }
      },
      {
        "device_id": "A1B2C3",
        "enabled": false,
        "disable_reason": "MANUAL",
        "disable_reason_detail": "Under maintenance"
      }
    ],
    "total_devices": 2
  },
  "tcp_devices": {
    "devices": [
      {
        "device_id": "E8F4G5",
        "enabled": true,
        "metrics": {
          "success_rate": 100.0,
          "avg_response_time_ms": 120
        }
      }
    ],
    "total_devices": 1
  }
}
```

**Mobile App Display:**
```
┌─────────────────────────────────────┐
│ Device Status Dashboard             │
├─────────────────────────────────────┤
│ RTU Devices (2)                     │
│   ✅ D7A3F2    99.04%  245ms       │
│   🔴 A1B2C3    Manual disable       │
│                                     │
│ TCP Devices (1)                     │
│   ✅ E8F4G5    100.0%  120ms       │
└─────────────────────────────────────┘
```

---

## Best Practices

### 1. When to Use `clear_metrics`

✅ **Do clear metrics:**
- After device maintenance/repair
- After hardware replacement
- When starting fresh monitoring

❌ **Don't clear metrics:**
- For routine enable/disable
- When you want to preserve historical performance data
- During troubleshooting (metrics help diagnose issues)

---

### 2. Disable Reason Guidelines

**Manual Disable Reasons (user-provided):**
- `"Under maintenance - replacing sensor"`
- `"Testing - temporary disable"`
- `"Device offline for calibration"`
- `"Troubleshooting network issue"`

**Auto Disable Reasons (system-generated):**
- `"Max retries exceeded"` (AUTO_RETRY)
- `"Max consecutive timeouts exceeded"` (AUTO_TIMEOUT)

---

### 3. Monitoring Best Practices

**Success Rate Thresholds:**
- ✅ **95-100%**: Excellent - device is healthy
- ⚠️ **90-95%**: Good - minor issues, monitor
- 🔴 **<90%**: Poor - investigate immediately

**Response Time Thresholds (RTU):**
- ✅ **<300ms**: Normal
- ⚠️ **300-500ms**: Slow - check cable length/quality
- 🔴 **>500ms**: Very slow - investigate

**Response Time Thresholds (TCP):**
- ✅ **<200ms**: Normal
- ⚠️ **200-400ms**: Slow - check network
- 🔴 **>400ms**: Very slow - investigate

---

### 4. Auto-Recovery Behavior

**What Gets Auto-Recovered:**
- ✅ Devices disabled with `AUTO_RETRY` reason
- ✅ Devices disabled with `AUTO_TIMEOUT` reason

**What Does NOT Get Auto-Recovered:**
- ❌ Devices disabled with `MANUAL` reason
- ❌ Devices with `disable_reason = NONE` (already enabled)

**Recovery Interval:**
- 🕐 Every **5 minutes** (300,000 milliseconds)
- Configurable in `ModbusRtuService.cpp` and `ModbusTcpService.cpp`:
  ```cpp
  const unsigned long RECOVERY_INTERVAL_MS = 300000; // 5 minutes
  ```

---

### 5. Error Handling in Mobile App

**Recommended Flow:**

```javascript
async function enableDevice(deviceId, clearMetrics = false) {
  try {
    const response = await sendBleCommand({
      op: "control",
      type: "enable_device",
      device_id: deviceId,
      clear_metrics: clearMetrics
    });

    if (response.status === "ok") {
      showToast(`Device ${deviceId} enabled`);
      refreshDeviceList();
    } else {
      showError(`Failed to enable device: ${response.message}`);
    }
  } catch (error) {
    showError(`BLE communication error: ${error.message}`);
  }
}
```

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2025-11-21 | Initial documentation for device control API |

---

**Made with ❤️ by SURIOTA R&D Team**
*Empowering Industrial IoT Solutions*
