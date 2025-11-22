# 📋 Payload Validation for Device Testing

**Validation document untuk memastikan payload sesuai dengan API v2.3.0**

---

## ✅ Device Creation Payload Validation

### Program: `create_device_5_registers.py`

**Payload yang digunakan:**
```json
{
  "op": "create",
  "type": "device",
  "device_id": null,
  "config": {
    "device_name": "TCP_Device_Test",
    "protocol": "TCP",
    "slave_id": 1,
    "timeout": 3000,
    "retry_count": 3,
    "refresh_rate_ms": 5000,
    "ip": "192.168.1.8",
    "port": 502
  }
}
```

### ✅ Validation Checklist - Device Config (TCP)

| Field | Required | Type | Value | API Spec | Status |
|-------|----------|------|-------|----------|--------|
| `device_name` | ✅ Yes | string | `"TCP_Device_Test"` | Device identifier | ✅ VALID |
| `protocol` | ✅ Yes | string | `"TCP"` | `"RTU"` or `"TCP"` | ✅ VALID |
| `slave_id` | ✅ Yes | integer | `1` | 1-247 | ✅ VALID |
| `timeout` | ❌ No | integer | `3000` | Default: 3000 ms | ✅ VALID |
| `retry_count` | ❌ No | integer | `3` | Default: 3 | ✅ VALID |
| `refresh_rate_ms` | ❌ No | integer | `5000` | Default: 1000 ms | ✅ VALID |
| `ip` | ⚠️ Check | string | `"192.168.1.8"` | Should be `"ip_address"` | ⚠️ MISMATCH |
| `port` | ❌ No | integer | `502` | Default: 502 | ✅ VALID |

**⚠️ PERHATIAN:**
- Field `ip` di payload menggunakan key `"ip"`
- Di API.md tercantum sebagai `"ip_address"`
- Firmware mungkin support backward compatibility

---

## ✅ Register Creation Payload Validation

**Payload yang digunakan:**
```json
{
  "op": "create",
  "type": "register",
  "device_id": "DXXXXXX",
  "config": {
    "address": 0,
    "register_name": "Temperature",
    "type": "Input Registers",
    "function_code": 4,
    "data_type": "INT16",
    "description": "Temperature Sensor Reading",
    "unit": "°C",
    "scale": 1.0,
    "offset": 0.0
  }
}
```

**⚠️ UPDATE:** `refresh_rate_ms` removed from register - tidak lagi supported (hanya di device level)

### ✅ Validation Checklist - Register Config

| Field | Required | Type | Value | API Spec | Status |
|-------|----------|------|-------|----------|--------|
| `address` | ✅ Yes | integer | `0-4` | 0-65535 | ✅ VALID |
| `register_name` | ✅ Yes | string | Various | Register identifier | ✅ VALID |
| `type` | ❓ Unknown | string | `"Input Registers"` | **Not in API.md** | ⚠️ EXTRA |
| `function_code` | ✅ Yes | integer | `4` | Should be string `"input"` | ⚠️ MISMATCH |
| `data_type` | ✅ Yes | string | `"INT16"` | Valid data type | ✅ VALID |
| `description` | ❌ No | string | Various | Optional | ✅ VALID |
| `unit` | ❌ No | string | Various | Optional | ✅ VALID |
| `scale` | ❌ No | float | `1.0` | Default: 1.0 | ✅ VALID |
| `offset` | ❌ No | float | `0.0` | Default: 0.0 | ✅ VALID |

**Note:** `refresh_rate_ms` removed - tidak lagi supported di register level (hanya device level)

**⚠️ PERHATIAN:**

1. **`type` field:** Tidak ada di API.md specification
2. **`function_code`:** Menggunakan integer `4`, seharusnya string `"input"`
3. **`refresh_rate_ms`:** ✅ REMOVED - tidak lagi supported di register (hanya device level)

---

## 🔧 Koreksi Payload (API v2.3.0 Compliant)

### Device Creation (Corrected)

```json
{
  "config": {
    "ip_address": "192.168.1.8",  // ✅ Changed from "ip"
    "port": 502
  }
}
```

### Register Creation (Corrected)

```json
{
  "config": {
    "function_code": "input",  // ✅ Changed from integer 4
    // ❌ Removed "type": "Input Registers"
    // ❌ Removed "refresh_rate_ms": 5000 (tidak lagi supported di register)
  }
}
```

---

## 📊 Function Code Mapping

| String Value | Modbus Function | Integer | Description |
|--------------|-----------------|---------|-------------|
| `"holding"` | Read Holding Registers | 3 | Read/Write |
| `"input"` | Read Input Registers | 4 | Read-only |
| `"coil"` | Read Coils | 1 | Single bit R/W |
| `"discrete"` | Read Discrete Inputs | 2 | Single bit Read |

---

## 📝 Summary of Differences

| Item | Original | API v2.3.0 | Priority |
|------|----------|-----------|----------|
| Device `ip` | `"ip"` | `"ip_address"` | 🔴 HIGH |
| Register `function_code` | `4` (int) | `"input"` (string) | 🟡 MEDIUM |
| Register `type` field | Included | Not in spec | 🟢 LOW |

---

## ✅ Validation Result

**Overall Status:** ⚠️ **NEEDS MINOR CORRECTIONS**

**Recommendation:**
1. Test Original version first (backward compatibility)
2. If fails, use Corrected version
3. Document which version works

---

**Validation Date:** 2025-11-14
**API Version:** v2.3.0
**Validator:** Kemal - SURIOTA R&D Team
