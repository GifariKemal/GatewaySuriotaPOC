# 📂 SRT-MGATE-1210 Device Testing

**Dedicated Testing Folder for Device & Register Creation via BLE**

---

## 📌 Overview

Folder khusus untuk testing **Create Device + Register** pada firmware **SRT-MGATE-1210**. Program menggunakan **BLE** untuk komunikasi dengan ESP32-S3 dan melakukan CRUD operations.

### Firmware Details
- **Product:** SRT-MGATE-1210 Modbus IIoT Gateway
- **MCU:** ESP32-S3 Dev Module
- **Firmware Version:** v2.3.0
- **Communication:** BLE (Bluetooth Low Energy)
- **Library:** Bleak (Python BLE library)

---

## 📂 Folder Structure

```
Device_Testing/
├── create_device_5_registers.py            # Original version
├── create_device_5_registers_corrected.py  # API v2.3.0 compliant
├── DOCUMENTATION.md                        # This file
├── QUICK_START.txt                         # Quick reference
├── PAYLOAD_VALIDATION.md                   # Payload validation
├── VERSION_COMPARISON.md                   # Version comparison
├── requirements.txt                        # Dependencies
└── 00_START_HERE.txt                       # Overview
```

---

## 🚀 Quick Start

### 1. Install Dependencies

```bash
cd "C:\Users\Administrator\Pictures\SRT-MGATE-1210-Firmware\Device_Testing"
pip install -r requirements.txt
```

### 2. Activate BLE on ESP32-S3

1. **Press button >8 seconds**
2. **LED will turn ON (steady)**
3. Device advertises as **"SURIOTA GW"**

### 3. Run Testing Program

**Option A - Original (RECOMMENDED):**
```bash
python create_device_5_registers.py
```

**Option B - API Compliant:**
```bash
python create_device_5_registers_corrected.py
```

---

## 📊 Device Configuration

### Device Details
```
Device Name:      TCP_Device_Test
Protocol:         Modbus TCP
IP Address:       192.168.1.8
Port:             502
Slave ID:         1
Timeout:          3000 ms
Retry Count:      3
Refresh Rate:     5000 ms
```

### Registers Created (5 Total)

| # | Name | Address | Type | Function Code | Unit | Description |
|---|------|---------|------|---------------|------|-------------|
| 1 | Temperature | 0 | INT16 | 4 (Input Reg) | °C | Temperature Sensor Reading |
| 2 | Humidity | 1 | INT16 | 4 (Input Reg) | % | Humidity Sensor Reading |
| 3 | Pressure | 2 | INT16 | 4 (Input Reg) | Pa | Pressure Sensor Reading |
| 4 | Voltage | 3 | INT16 | 4 (Input Reg) | V | Voltage Measurement |
| 5 | Current | 4 | INT16 | 4 (Input Reg) | A | Current Measurement |

---

## 📝 Program Versions

### Version 1: `create_device_5_registers.py` (Original)

**Payload Format:**
- Device: `"ip": "192.168.1.8"`
- Register: `"function_code": 4` (integer)
- Register: `"type": "Input Registers"` (included)

**Status:** ✅ Based on proven working code

**Use Case:**
- First testing
- Backward compatibility
- Quick verification

---

### Version 2: `create_device_5_registers_corrected.py` (API Compliant)

**Payload Format:**
- Device: `"ip_address": "192.168.1.8"` (corrected)
- Register: `"function_code": "input"` (string)
- Register: No `"type"` field (removed)

**Status:** ✅ 100% sesuai API.md v2.3.0

**Use Case:**
- Production deployment
- Future-proof
- API compliance

---

## 🔧 BLE Configuration

### Service & Characteristics

| Item | UUID |
|------|------|
| **Service UUID** | `00001830-0000-1000-8000-00805f9b34fb` |
| **Command Characteristic** | `11111111-1111-1111-1111-111111111101` |
| **Response Characteristic** | `11111111-1111-1111-1111-111111111102` |
| **Device Name** | `SURIOTA GW` |

### Fragmentation

- **Chunk Size:** 18 bytes
- **End Marker:** `<END>`
- **Delay:** 100ms per chunk
- **Response Wait:** 2 seconds

---

## 📖 API Reference

### Create Device (Modbus TCP)

**Request:**
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

**Response:**
```json
{
  "status": "ok",
  "device_id": "DXXXXXX",
  "data": { ... }
}
```

### Create Register

**Request:**
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
    "refresh_rate_ms": 5000,
    "scale": 1.0,
    "offset": 0.0
  }
}
```

**Response:**
```json
{
  "status": "ok",
  "device_id": "DXXXXXX",
  "register_id": "RXXXXXX",
  "data": { ... }
}
```

---

## ✅ Expected Output

```
======================================================================
  SRT-MGATE-1210 Firmware Testing
  Python BLE Device Creation Client
======================================================================
[SCAN] Scanning for 'SURIOTA GW'...
[FOUND] SURIOTA GW (XX:XX:XX:XX:XX:XX)
[SUCCESS] Connected to SURIOTA GW

======================================================================
  SRT-MGATE-1210 TESTING: CREATE 1 DEVICE + 5 REGISTERS
======================================================================

>>> STEP 1: Creating TCP Device...
[COMMAND] Creating TCP Device: TCP_Device_Test
[DEBUG] Payload: {"op":"create","type":"device",...}
[RESPONSE] {
  "status": "ok",
  "device_id": "DXXXXXX",
  "data": { ... }
}
[CAPTURE] Device ID: DXXXXXX

>>> STEP 2: Creating 5 Registers for Device ID: DXXXXXX
[COMMAND] Creating Register 1/5: Temperature (Address: 0)
[RESPONSE] {
  "status": "ok",
  "register_id": "RXXXXXX"
}
...

======================================================================
  SUMMARY
======================================================================
Device Name:      TCP_Device_Test
Device ID:        DXXXXXX
Protocol:         Modbus TCP
IP Address:       192.168.1.8
Port:             502
Slave ID:         1

Registers Created: 5
  1. Temperature (Address: 0) - °C
  2. Humidity (Address: 1) - %
  3. Pressure (Address: 2) - Pa
  4. Voltage (Address: 3) - V
  5. Current (Address: 4) - A
======================================================================

[SUCCESS] Program completed successfully
[DISCONNECT] Connection closed
```

**Duration:** ~30-40 seconds

---

## 🐛 Troubleshooting

### Issue: `Service 'SURIOTA GW' not found`

**Solution:**
- Press button >8 seconds on ESP32-S3
- Check LED is ON (steady)
- Enable Bluetooth on PC

### Issue: `Connection failed`

**Solution:**
- Restart ESP32-S3
- Wait 5 seconds
- Run program again

### Issue: `bleak not found`

**Solution:**
```bash
pip install bleak
```

### Issue: Script hangs

**Solution:**
- Press Ctrl+C to stop
- Restart ESP32-S3
- Try again

---

## 📚 Related Documentation

- [API.md](../Docs/API.md) - Complete BLE CRUD API
- [MQTT_PUBLISH_MODES_DOCUMENTATION.md](../Docs/MQTT_PUBLISH_MODES_DOCUMENTATION.md) - MQTT modes
- [NETWORK_CONFIGURATION.md](../Docs/NETWORK_CONFIGURATION.md) - Network setup
- [TROUBLESHOOTING.md](../Docs/TROUBLESHOOTING.md) - Troubleshooting guide

---

## 📝 Testing Strategy

### Recommended Approach

```
1. TEST ORIGINAL VERSION FIRST
   ├─ python create_device_5_registers.py
   ├─ If ✅ SUCCESS → Firmware supports backward compatibility
   └─ If ❌ FAIL → Try corrected version

2. TEST CORRECTED VERSION
   ├─ python create_device_5_registers_corrected.py
   ├─ If ✅ SUCCESS → Firmware supports API v2.3.0
   └─ If ❌ FAIL → Check troubleshooting

3. DOCUMENT RESULTS
   └─ Note which version works
```

---

## 📞 Support

**PT Surya Inovasi Prioritas (SURIOTA)**
R&D Team
📧 Email: support@suriota.com
🌐 Website: https://suriota.com

---

**© 2025 PT Surya Inovasi Prioritas (SURIOTA) - All Rights Reserved**
