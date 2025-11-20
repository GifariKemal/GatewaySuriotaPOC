# BLE Testing Tool - User Guide

## 📋 Overview

Tool interaktif untuk testing CRUD operations melalui BLE pada SRT-MGATE-1210 firmware.

**Fitur:**
- ✅ Scan dan connect ke BLE device "SURIOTA GW"
- ✅ Send JSON commands secara manual/interaktif
- ✅ Command fragmentation (18 bytes/chunk + `<END>` marker)
- ✅ Receive dan display fragmented responses
- ✅ Support semua CRUD operations (Create, Read, Update, Delete)
- ✅ Command examples untuk quick reference
- ✅ Quick test mode untuk automated testing

**BLE Configuration:**
- Service UUID: `00001830-0000-1000-8000-00805f9b34fb`
- Command Characteristic: `11111111-1111-1111-1111-111111111101`
- Response Characteristic: `11111111-1111-1111-1111-111111111102`
- Device Name: `SURIOTA GW`

---

## 🔧 Installation

### 1. Install Python Dependencies

```bash
cd C:\Users\Administrator\Pictures\SRT-MGATE-1210\Testing
pip install -r requirements.txt
```

Atau install manual:
```bash
pip install bleak
```

### 2. Verify Installation

```bash
python ble_test.py
```

---

## 🚀 Usage

### Cara 1: Interactive Mode (Recommended)

```bash
python ble_test.py
```

**Steps:**
1. Tool akan scan BLE devices
2. Pilih device number untuk connect
3. Pilih mode: `1` untuk Interactive Mode
4. Masukkan JSON command di prompt `>`
5. Lihat response dari device

**Example Session:**
```
> {"op":"read","type":"devices_summary"}
[SEND] Sending command...
[RESPONSE] Received:
{
  "status": "ok",
  "devices_summary": [...]
}

> {"op":"read","type":"device","device_id":"device_1","minimal":true}
[RESPONSE] Received:
{
  "status": "ok",
  "data": {
    "device_id": "device_1",
    "register_count": 50
  }
}
```

### Cara 2: Quick Test Mode

```bash
python ble_test.py
```

Pilih mode: `2` untuk Quick Test

Tool akan menjalankan test sequence otomatis:
- Read devices summary
- Read server config
- Read devices with registers (minimal mode)

---

## 📝 Command Examples

### READ Operations

#### 1. Read All Devices (Summary)
```json
{
  "op": "read",
  "type": "devices_summary"
}
```
**Response:** List device IDs dan names tanpa registers

---

#### 2. Read All Devices with Registers (Minimal Mode)
```json
{
  "op": "read",
  "type": "devices_with_registers",
  "minimal": true
}
```
**Response:** All devices dengan register_count (payload kecil ~1KB)

---

#### 3. Read All Devices with Registers (Full Mode)
```json
{
  "op": "read",
  "type": "devices_with_registers",
  "minimal": false
}
```
**Response:** All devices dengan full registers array (payload besar ~11KB)
**⚠️ WARNING:** Jika payload >10KB akan error!

---

#### 4. Read Specific Device (Minimal Mode)
```json
{
  "op": "read",
  "type": "device",
  "device_id": "device_1",
  "minimal": true
}
```
**Response:** Device info + register_count (tanpa registers array)

---

#### 5. Read Specific Device (Full Mode)
```json
{
  "op": "read",
  "type": "device",
  "device_id": "device_1",
  "minimal": false
}
```
**Response:** Device info + full registers array

---

#### 6. Read Registers for Device
```json
{
  "op": "read",
  "type": "registers",
  "device_id": "device_1"
}
```
**Response:** Array of all registers untuk device tersebut

---

#### 7. Read Registers Summary
```json
{
  "op": "read",
  "type": "registers_summary",
  "device_id": "device_1"
}
```
**Response:** Summary (register_id, name, address saja)

---

#### 8. Read Server Config
```json
{
  "op": "read",
  "type": "server_config"
}
```
**Response:** MQTT/HTTP server configuration

---

#### 9. Read Logging Config
```json
{
  "op": "read",
  "type": "logging_config"
}
```
**Response:** Logging configuration

---

### CREATE Operations

#### 10. Create New Device
```json
{
  "op": "create",
  "type": "device",
  "config": {
    "device_name": "Sensor Panel A",
    "slave_id": 1,
    "baud_rate": 9600,
    "protocol": "RTU",
    "enabled": true
  }
}
```
**Response:** Created device dengan device_id

---

#### 11. Create New Register
```json
{
  "op": "create",
  "type": "register",
  "device_id": "device_1",
  "config": {
    "register_name": "Temperature",
    "address": 4112,
    "function_code": 3,
    "data_type": "INT16",
    "scale": 1.0,
    "offset": 0.0,
    "unit": "°C",
    "description": "Room temperature sensor"
  }
}
```
**Response:** Created register dengan register_id

**Data Types:**
- `INT16`, `UINT16`, `INT32`, `UINT32`
- `FLOAT32`, `DOUBLE64`
- `BINARY`, `STRING`

**Function Codes:**
- `3` = Read Holding Registers
- `4` = Read Input Registers

---

### UPDATE Operations

#### 12. Update Device
```json
{
  "op": "update",
  "type": "device",
  "device_id": "device_1",
  "config": {
    "enabled": false,
    "baud_rate": 19200
  }
}
```
**Response:** Updated device data

---

#### 13. Update Register
```json
{
  "op": "update",
  "type": "register",
  "device_id": "device_1",
  "register_id": "register_1",
  "config": {
    "scale": 0.1,
    "offset": -50.0
  }
}
```
**Response:** Updated register data

---

#### 14. Update Server Config
```json
{
  "op": "update",
  "type": "server_config",
  "config": {
    "mqtt_enabled": true,
    "mqtt_broker": "broker.hivemq.com",
    "mqtt_port": 1883,
    "data_transmission_interval": 60000
  }
}
```
**Response:** Success message (device will restart in 5s)

---

### DELETE Operations

#### 15. Delete Device
```json
{
  "op": "delete",
  "type": "device",
  "device_id": "device_1"
}
```
**Response:** Deleted device data

---

#### 16. Delete Register
```json
{
  "op": "delete",
  "type": "register",
  "device_id": "device_1",
  "register_id": "register_1"
}
```
**Response:** Deleted register data

---

### BATCH Operations

#### 17. Batch Sequential
```json
{
  "op": "batch",
  "mode": "sequential",
  "commands": [
    {
      "op": "read",
      "type": "devices_summary"
    },
    {
      "op": "read",
      "type": "server_config"
    }
  ]
}
```

#### 18. Batch Atomic
```json
{
  "op": "batch",
  "mode": "atomic",
  "commands": [
    {
      "op": "create",
      "type": "device",
      "config": {...}
    },
    {
      "op": "create",
      "type": "register",
      "device_id": "device_1",
      "config": {...}
    }
  ]
}
```

---

## 🎯 Testing Scenarios

### Scenario 1: Test Minimal Mode (Payload Reduction)

**Tujuan:** Verify minimal mode mengurangi payload dari 11KB ke <1KB

```json
# Full mode (expect large payload)
{"op":"read","type":"device","device_id":"device_1","minimal":false}

# Minimal mode (expect small payload)
{"op":"read","type":"device","device_id":"device_1","minimal":true}
```

**Expected:**
- Full mode: Returns full registers array (~11KB if 50 registers)
- Minimal mode: Returns register_count only (~1KB)

---

### Scenario 2: Test BLE Payload Limit

**Tujuan:** Verify 10KB payload limit enforcement

```json
# Try to read device with 50+ registers (full mode)
{"op":"read","type":"devices_with_registers","minimal":false}
```

**Expected:**
- If payload >10KB: Error message "Payload too large"
- Device should NOT crash

---

### Scenario 3: Test Batch Tracking

**Tujuan:** Verify RTU reads all registers before MQTT publish

1. Create device dengan 50 registers
2. Monitor serial output
3. Verify batch tracking logs:
   - `[BATCH] Started tracking device_1: expecting 50 registers`
   - `[BATCH] Device device_1 COMPLETE (50/50 registers)`
   - `[MQTT] Publishing complete batch`

---

### Scenario 4: Test CRUD Operations

```json
# 1. Create device
{"op":"create","type":"device","config":{"device_name":"Test","slave_id":99,"baud_rate":9600,"protocol":"RTU","enabled":true}}

# 2. Read created device
{"op":"read","type":"device","device_id":"<device_id_from_step_1>"}

# 3. Update device
{"op":"update","type":"device","device_id":"<device_id>","config":{"enabled":false}}

# 4. Delete device
{"op":"delete","type":"device","device_id":"<device_id>"}
```

---

## 🔍 Troubleshooting

### Issue: "No BLE devices found" atau "No 'SURIOTA GW' devices found"

**Solutions:**
1. Pastikan ESP32 sudah running dan BLE enabled
2. Pastikan Bluetooth adapter di PC aktif
3. Cek jarak antara PC dan ESP32 (<10 meter)
4. Verify device name di Serial Monitor adalah "SURIOTA GW"
5. Pastikan UUIDs di firmware match dengan tool:
   - Service: `00001830-0000-1000-8000-00805f9b34fb`
   - Command: `11111111-1111-1111-1111-111111111101`
   - Response: `11111111-1111-1111-1111-111111111102`

---

### Issue: "Connection timeout"

**Solutions:**
1. Restart ESP32 device
2. Restart Bluetooth adapter di PC
3. Close aplikasi lain yang menggunakan BLE

---

### Issue: "Response timeout"

**Solutions:**
1. Cek serial monitor untuk error messages
2. Command mungkin terlalu kompleks (payload >10KB)
3. Gunakan minimal mode untuk large queries

---

### Issue: "Invalid JSON error"

**Solutions:**
1. Pastikan JSON format benar (gunakan quotes untuk strings)
2. Gunakan single-line JSON (no line breaks)
3. Type `help` untuk melihat contoh yang valid

---

## 📊 Performance Tips

### ✅ DO's

- ✅ Gunakan `minimal: true` untuk device dengan banyak registers
- ✅ Gunakan `devices_summary` untuk list devices (faster)
- ✅ Gunakan batch operations untuk multiple commands
- ✅ Add delays antara commands (1 second minimum)

### ❌ DON'Ts

- ❌ Jangan query full registers jika >50 registers (akan >10KB)
- ❌ Jangan send commands terlalu cepat (wait for response)
- ❌ Jangan create device tanpa validation
- ❌ Jangan delete device yang sedang active

---

## 🐛 Debug Mode

Untuk melihat detailed BLE communication logs, edit `ble_test.py`:

```python
# Enable debug logging
import logging
logging.basicConfig(level=logging.DEBUG)
```

---

## 📞 Support

Jika menemukan issues:
1. Cek serial monitor ESP32 untuk error logs
2. Cek Python traceback untuk BLE errors
3. Verify command JSON format
4. Test dengan Quick Test mode terlebih dahulu

---

**Version:** 1.0.0
**Last Updated:** 2025-01-16
**Author:** Claude Code
