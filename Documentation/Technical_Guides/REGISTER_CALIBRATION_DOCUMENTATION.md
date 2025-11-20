# Register Calibration - Documentation

**SRT-MGATE-1210 Modbus IIoT Gateway**

**Current Version:** v2.1.1
**Developer:** Kemal
**Last Updated:** November 14, 2025 (Friday) - WIB (GMT+7)

## Overview

SRT-MGATE-1210 Firmware v2.1.1 mendukung **kalibrasi otomatis** pada nilai register Modbus menggunakan formula **scale & offset**. Fitur ini memungkinkan konversi nilai raw Modbus ke unit pengukuran yang sesuai tanpa perlu post-processing di sisi subscriber.

**Fitur Utama:**
- ✅ Kalibrasi otomatis dengan formula: `final_value = (raw_value × scale) + offset`
- ✅ Support unit pengukuran custom (°C, V, A, PSI, bar, dll.)
- ✅ Nilai negatif diperbolehkan untuk scale & offset
- ✅ Default values untuk backward compatibility
- ✅ Auto-migration untuk register yang sudah ada
- ✅ Device-level polling interval (bukan per-register)

---

## Table of Contents

1. [Calibration Fields](#calibration-fields)
2. [Calibration Formula](#calibration-formula)
3. [Configuration Examples](#configuration-examples)
4. [Use Cases](#use-cases)
5. [API Reference](#api-reference)
6. [Migration Notes](#migration-notes)
7. [Device-Level Polling](#device-level-polling)

---

## Calibration Fields

### Field Descriptions

| Field    | Type   | Required | Default | Description                                |
| -------- | ------ | -------- | ------- | ------------------------------------------ |
| `scale`  | float  | No       | 1.0     | Multiplier untuk kalibrasi (bisa negatif)  |
| `offset` | float  | No       | 0.0     | Offset untuk kalibrasi (bisa negatif)      |
| `unit`   | string | No       | ""      | Unit pengukuran (°C, V, A, PSI, bar, dll.) |

### Default Values

Jika field tidak diberikan saat membuat register, firmware akan menggunakan nilai default:

```json
{
  "scale": 1.0,
  "offset": 0.0,
  "unit": ""
}
```

Dengan default values ini:
- `final_value = (raw_value × 1.0) + 0.0 = raw_value`
- Tidak ada perubahan nilai (backward compatible)

### Value Constraints

- **scale**: Dapat bernilai positif, negatif, atau nol
- **offset**: Dapat bernilai positif, negatif, atau nol
- **unit**: String bebas (maksimal 50 karakter)

---

## Calibration Formula

### Formula

```
final_value = (raw_value × scale) + offset
```

### Urutan Eksekusi

1. **Baca nilai raw dari Modbus** (16-bit atau 32-bit register)
2. **Konversi data type** (int16, uint16, float, int32, uint32)
3. **Terapkan kalibrasi** menggunakan scale & offset
4. **Publish ke MQTT** dengan nilai yang sudah dikalibrasi

### Diagram Flow

```
┌─────────────┐      ┌──────────────┐      ┌─────────────┐      ┌──────────┐
│ Modbus Raw  │ ───> │ Data Type    │ ───> │ Calibration │ ───> │  MQTT    │
│ (registers) │      │ Conversion   │      │ (scale+off) │      │ Publish  │
└─────────────┘      └──────────────┘      └─────────────┘      └──────────┘
   0x1234              1234 (int)           12.34 (V)            {"value":12.34}
```

---

## Configuration Examples

### Example 1: Basic Register (No Calibration)

Register tanpa kalibrasi (menggunakan default values):

```json
{
  "op": "create",
  "type": "register",
  "device_id": "DEVICE_001",
  "config": {
    "register_name": "Humidity Sensor",
    "address": 40001,
    "function_code": 3,
    "data_type": "uint16",
    "description": "Kelembaban ruangan"
  }
}
```

**Hasil:**
- Raw Modbus: 65 → Final value: **65** (no unit)
- MQTT payload:
```json
{
  "time": 1699123456,
  "name": "Humidity Sensor",
  "device_id": "DEVICE_001",
  "value": 65,
  "description": "Kelembaban ruangan",
  "unit": ""
}
```

### Example 2: Voltage Divider (Scale Only)

Sensor tegangan dengan voltage divider 1:100 (raw value dalam centivolts):

```json
{
  "op": "create",
  "type": "register",
  "device_id": "DEVICE_001",
  "config": {
    "register_name": "Battery Voltage",
    "address": 40002,
    "function_code": 3,
    "data_type": "uint16",
    "scale": 0.01,
    "offset": 0.0,
    "unit": "V",
    "description": "Tegangan baterai utama"
  }
}
```

**Hasil:**
- Raw Modbus: 2456 → Calibrated: (2456 × 0.01) + 0 = **24.56 V**
- MQTT payload:
```json
{
  "time": 1699123456,
  "name": "Battery Voltage",
  "device_id": "DEVICE_001",
  "value": 24.56,
  "description": "Tegangan baterai utama",
  "unit": "V"
}
```

### Example 3: Current Sensor with Offset

Sensor arus yang membutuhkan offset correction:

```json
{
  "op": "create",
  "type": "register",
  "device_id": "DEVICE_001",
  "config": {
    "register_name": "Load Current",
    "address": 40003,
    "function_code": 3,
    "data_type": "float",
    "scale": 1.0,
    "offset": -0.15,
    "unit": "A",
    "description": "Arus beban total"
  }
}
```

**Hasil:**
- Raw Modbus: 5.35 A → Calibrated: (5.35 × 1.0) - 0.15 = **5.20 A**

### Example 4: Temperature Sensor (Offset Correction)

Sensor suhu yang perlu dikalibrasi karena bias +2.5°C:

```json
{
  "op": "create",
  "type": "register",
  "device_id": "DEVICE_001",
  "config": {
    "register_name": "Room Temperature",
    "address": 40004,
    "function_code": 3,
    "data_type": "float",
    "scale": 1.0,
    "offset": -2.5,
    "unit": "°C",
    "description": "Suhu ruang server"
  }
}
```

**Hasil:**
- Raw Modbus: 27.5°C → Calibrated: (27.5 × 1.0) - 2.5 = **25.0°C**

### Example 5: Fahrenheit to Celsius Conversion

Sensor suhu dalam Fahrenheit yang ingin dikonversi ke Celsius:

**Formula:** `°C = (°F - 32) × 5/9 = (°F × 0.5556) - 17.778`

```json
{
  "op": "create",
  "type": "register",
  "device_id": "DEVICE_001",
  "config": {
    "register_name": "External Temperature",
    "address": 40005,
    "function_code": 3,
    "data_type": "float",
    "scale": 0.5556,
    "offset": -17.778,
    "unit": "°C",
    "description": "Suhu outdoor (converted from F)"
  }
}
```

**Hasil:**
- Raw Modbus: 77°F → Calibrated: (77 × 0.5556) - 17.778 ≈ **25.0°C**

### Example 6: Pressure Sensor (PSI to Bar)

Sensor tekanan dalam PSI yang ingin dikonversi ke bar:

**Formula:** `1 PSI = 0.06895 bar`

```json
{
  "op": "create",
  "type": "register",
  "device_id": "DEVICE_001",
  "config": {
    "register_name": "Hydraulic Pressure",
    "address": 40006,
    "function_code": 3,
    "data_type": "float",
    "scale": 0.06895,
    "offset": 0.0,
    "unit": "bar",
    "description": "Tekanan hidrolik sistem"
  }
}
```

**Hasil:**
- Raw Modbus: 100 PSI → Calibrated: (100 × 0.06895) + 0 = **6.895 bar**

### Example 7: Percentage with Divider

Sensor yang memberikan nilai 0-10000 untuk representasi 0-100%:

```json
{
  "op": "create",
  "type": "register",
  "device_id": "DEVICE_001",
  "config": {
    "register_name": "Tank Level",
    "address": 40007,
    "function_code": 3,
    "data_type": "uint16",
    "scale": 0.01,
    "offset": 0.0,
    "unit": "%",
    "description": "Level tangki air"
  }
}
```

**Hasil:**
- Raw Modbus: 8550 → Calibrated: (8550 × 0.01) + 0 = **85.50%**

### Example 8: Negative Scale (Inverting)

Sensor dengan nilai yang perlu dibalik (inverting):

```json
{
  "op": "create",
  "type": "register",
  "device_id": "DEVICE_001",
  "config": {
    "register_name": "Flow Direction",
    "address": 40008,
    "function_code": 3,
    "data_type": "int16",
    "scale": -1.0,
    "offset": 0.0,
    "unit": "m/s",
    "description": "Kecepatan aliran (inverted)"
  }
}
```

**Hasil:**
- Raw Modbus: 50 → Calibrated: (50 × -1.0) + 0 = **-50 m/s**

---

## Use Cases

### Use Case 1: Industrial Monitoring - Mixed Units

**Scenario:** Pabrik dengan sensor dalam berbagai unit (V, A, PSI, °F) yang ingin distandarisasi ke unit SI.

**Solution:** Gunakan scale & offset untuk konversi otomatis:
- Voltage: Raw centivolt → Volt (scale: 0.01)
- Current: Raw milliampere → Ampere (scale: 0.001)
- Pressure: Raw PSI → bar (scale: 0.06895)
- Temperature: Raw Fahrenheit → Celsius (scale: 0.5556, offset: -17.778)

**Benefits:**
- Subscriber tidak perlu konversi manual
- Data langsung siap untuk analisis
- Konsistensi unit di seluruh sistem

### Use Case 2: Sensor Calibration - Offset Correction

**Scenario:** Sensor suhu yang sudah lama dan memiliki bias +3°C.

**Solution:** Gunakan offset untuk koreksi:
```json
{
  "scale": 1.0,
  "offset": -3.0,
  "unit": "°C"
}
```

**Benefits:**
- Tidak perlu ganti sensor
- Kalibrasi bisa diupdate dari mobile app
- Historical data tetap valid

### Use Case 3: Multi-Range Sensors

**Scenario:** Sensor yang bisa dibaca dalam 2 range berbeda (0-5V untuk 0-100°C).

**Solution:** Konfigurasi 2 register dengan kalibrasi berbeda:

**Register 1 - Raw Voltage:**
```json
{
  "register_name": "Sensor Voltage",
  "address": 40001,
  "scale": 0.001,
  "unit": "V"
}
```

**Register 2 - Converted Temperature:**
```json
{
  "register_name": "Temperature from Voltage",
  "address": 40001,
  "scale": 20.0,
  "offset": 0.0,
  "unit": "°C"
}
```
Formula: 5V = 100°C → scale = 100/5 = 20

### Use Case 4: Legacy System Integration

**Scenario:** Integrasi dengan PLC lama yang mengirim nilai dalam format proprietary.

**Solution:** Reverse-engineer formula PLC dan terapkan di gateway:
```json
{
  "scale": 0.0625,
  "offset": -1024.0,
  "unit": "kW"
}
```

**Benefits:**
- Tidak perlu update PLC programming
- Gateway bertindak sebagai translator
- Mudah update kalibrasi tanpa downtime

---

## API Reference

### Create Register with Calibration

**Endpoint:** BLE GATT Characteristic Write

**JSON Format:**
```json
{
  "op": "create",
  "type": "register",
  "device_id": "DEVICE_001",
  "config": {
    "register_name": "Sensor Name",
    "address": 40001,
    "function_code": 3,
    "data_type": "float",
    "scale": 1.0,
    "offset": 0.0,
    "unit": "V",
    "description": "Optional description"
  }
}
```

**Required Fields:**
- `op`: "create"
- `type`: "register"
- `device_id`: Device ID yang sudah dibuat sebelumnya
- `config.register_name`: Nama register
- `config.address`: Alamat register Modbus
- `config.function_code`: Modbus function code (3, 4, etc.)
- `config.data_type`: "int16", "uint16", "int32", "uint32", "float"

**Optional Calibration Fields:**
- `config.scale`: Default 1.0
- `config.offset`: Default 0.0
- `config.unit`: Default ""
- `config.description`: Default ""

### Update Register Calibration

**JSON Format:**
```json
{
  "op": "update",
  "type": "register",
  "device_id": "DEVICE_001",
  "register_id": "DEVICE_001_40001",
  "config": {
    "scale": 0.01,
    "offset": -0.5,
    "unit": "V"
  }
}
```

**Notes:**
- Hanya field yang ingin diupdate yang perlu dikirim
- `register_id` format: `{device_id}_{address}`
- Update akan langsung diterapkan pada polling berikutnya

### List Registers (Check Current Calibration)

**JSON Format:**
```json
{
  "op": "list",
  "type": "register",
  "device_id": "DEVICE_001"
}
```

**Response Example:**
```json
{
  "status": "success",
  "registers": [
    {
      "register_id": "DEVICE_001_40001",
      "register_index": 1,
      "register_name": "Battery Voltage",
      "address": 40001,
      "function_code": 3,
      "data_type": "uint16",
      "scale": 0.01,
      "offset": 0.0,
      "unit": "V",
      "description": "Tegangan baterai"
    }
  ]
}
```

---

## Migration Notes

### Automatic Migration

Firmware secara otomatis melakukan migration saat boot untuk register yang sudah ada:

#### Migration 1: Add Default Calibration Values

Jika register tidak memiliki field `scale`, `offset`, atau `unit`, firmware akan menambahkan:

```
[MIGRATION] Added scale=1.0 to register DEVICE_001_40001
[MIGRATION] Added offset=0.0 to register DEVICE_001_40001
[MIGRATION] Added unit="" to register DEVICE_001_40001
```

#### Migration 2: Remove Old `refresh_rate_ms`

Field `refresh_rate_ms` per-register telah deprecated dan dihapus otomatis:

```
[MIGRATION] Removed refresh_rate_ms from register DEVICE_001_40001 (now using device-level polling)
```

#### Migration 3: Auto-Save

Setelah migration, `devices.json` akan disimpan otomatis:

```
[MIGRATION] Saving devices.json with calibration fields and removed refresh_rate_ms...
```

### Backward Compatibility

- ✅ Register lama tanpa calibration akan tetap berfungsi (scale=1.0, offset=0.0)
- ✅ Nilai yang dipublish tidak berubah untuk register tanpa kalibrasi
- ✅ MQTT payload format tetap konsisten (6 fields)

### Manual Migration (Optional)

Jika ingin update kalibrasi untuk register yang sudah ada:

1. **List register yang ada:**
```json
{"op": "list", "type": "register", "device_id": "DEVICE_001"}
```

2. **Update setiap register dengan kalibrasi yang sesuai:**
```json
{
  "op": "update",
  "type": "register",
  "device_id": "DEVICE_001",
  "register_id": "DEVICE_001_40001",
  "config": {
    "scale": 0.01,
    "offset": 0.0,
    "unit": "V"
  }
}
```

---

## Device-Level Polling

### Overview

**PENTING:** Polling interval sekarang menggunakan **device-level** `refresh_rate_ms`, bukan per-register.

### Configuration

Set polling interval di device config:

```json
{
  "op": "update",
  "type": "device",
  "device_id": "DEVICE_001",
  "config": {
    "refresh_rate_ms": 1000
  }
}
```

**Efek:**
- Semua register dalam `DEVICE_001` akan di-poll setiap 1000ms (1 detik)
- Tidak ada polling interval individual per-register

### Rationale

**Mengapa device-level polling?**

1. **Efisiensi Modbus:**
   - Modbus adalah protokol serial/sequential
   - Membaca multiple register dalam satu cycle lebih efisien
   - Mengurangi overhead komunikasi

2. **Consistency:**
   - Semua data dari device di-read pada waktu yang sama
   - Timestamp yang konsisten untuk semua register
   - Lebih mudah untuk data correlation

3. **Simplified Configuration:**
   - User hanya perlu set 1 interval per device
   - Lebih mudah dipahami dan dikonfigurasi

### Example Configuration

**Full Device Config:**
```json
{
  "op": "update",
  "type": "device",
  "device_id": "DEVICE_001",
  "config": {
    "device_name": "PM-5560 Power Meter",
    "protocol": "TCP",
    "slave_id": 1,
    "timeout": 3000,
    "retry_count": 3,
    "refresh_rate_ms": 1000,
    "ip": "192.168.1.100",
    "port": 502
  }
}
```

**Register Config (No refresh_rate_ms):**
```json
{
  "op": "create",
  "type": "register",
  "device_id": "DEVICE_001",
  "config": {
    "register_name": "Voltage L1",
    "address": 40001,
    "function_code": 3,
    "data_type": "float",
    "scale": 0.1,
    "offset": 0.0,
    "unit": "V"
  }
}
```

### Default Refresh Rate

Jika `refresh_rate_ms` tidak diset, firmware menggunakan default:
- **TCP:** 1000ms (1 detik)
- **RTU:** 1000ms (1 detik)

---

## MQTT Payload Format

### Payload Structure

Setiap data point yang dipublish ke MQTT memiliki format:

```json
{
  "time": 1699123456,
  "name": "Battery Voltage",
  "device_id": "DEVICE_001",
  "value": 24.56,
  "description": "Tegangan baterai utama",
  "unit": "V"
}
```

### Field Details

| Field         | Type      | Source           | Description                                |
| ------------- | --------- | ---------------- | ------------------------------------------ |
| `time`        | integer   | RTC              | Unix timestamp saat data dibaca            |
| `name`        | string    | `register_name`  | Nama register dari konfigurasi             |
| `device_id`   | string    | Device config    | ID device Modbus                           |
| `value`       | float/int | Calibrated value | Nilai SETELAH kalibrasi scale & offset     |
| `description` | string    | Register config  | Deskripsi opsional dari BLE config         |
| `unit`        | string    | Register config  | Unit pengukuran (°C, V, A, PSI, bar, dll.) |

### Internal Fields (Not Published)

Field ini digunakan internal untuk deduplication dan routing, TIDAK dipublish ke MQTT:

| Field            | Type    | Purpose                                    |
| ---------------- | ------- | ------------------------------------------ |
| `register_id`    | string  | Unique identifier untuk deduplication      |
| `register_index` | integer | Index untuk customize mode topic filtering |

---

## Testing Calibration

### Test Scenario 1: Voltage Sensor

**Setup:**
```json
{
  "register_name": "Test Voltage",
  "address": 40001,
  "data_type": "uint16",
  "scale": 0.01,
  "offset": 0.0,
  "unit": "V"
}
```

**Test Cases:**

| Raw Modbus | Expected Output | Formula                   |
| ---------- | --------------- | ------------------------- |
| 0          | 0.00 V          | (0 × 0.01) + 0 = 0.00     |
| 1000       | 10.00 V         | (1000 × 0.01) + 0 = 10.00 |
| 2456       | 24.56 V         | (2456 × 0.01) + 0 = 24.56 |
| 5000       | 50.00 V         | (5000 × 0.01) + 0 = 50.00 |

### Test Scenario 2: Temperature with Offset

**Setup:**
```json
{
  "register_name": "Test Temperature",
  "address": 40002,
  "data_type": "float",
  "scale": 1.0,
  "offset": -2.5,
  "unit": "°C"
}
```

**Test Cases:**

| Raw Modbus | Expected Output | Formula                   |
| ---------- | --------------- | ------------------------- |
| 20.0       | 17.5 °C         | (20.0 × 1.0) - 2.5 = 17.5 |
| 25.0       | 22.5 °C         | (25.0 × 1.0) - 2.5 = 22.5 |
| 27.5       | 25.0 °C         | (27.5 × 1.0) - 2.5 = 25.0 |
| 30.0       | 27.5 °C         | (30.0 × 1.0) - 2.5 = 27.5 |

### Test Scenario 3: PSI to Bar Conversion

**Setup:**
```json
{
  "register_name": "Test Pressure",
  "address": 40003,
  "data_type": "float",
  "scale": 0.06895,
  "offset": 0.0,
  "unit": "bar"
}
```

**Test Cases:**

| Raw Modbus | Expected Output | Formula                     |
| ---------- | --------------- | --------------------------- |
| 0          | 0.000 bar       | (0 × 0.06895) + 0 = 0.000   |
| 14.5       | 1.000 bar       | (14.5 × 0.06895) + 0 ≈ 1.00 |
| 100        | 6.895 bar       | (100 × 0.06895) + 0 = 6.895 |
| 145        | 10.00 bar       | (145 × 0.06895) + 0 ≈ 10.00 |

### Debugging Calibration

**Serial Monitor Output:**

Saat register dibaca, firmware akan log:

```
[TCP] Reading register: Test Voltage (address: 40001)
[TCP] Raw value: 2456
[TCP] Data type conversion: 2456 (uint16)
[TCP] Calibration: scale=0.01, offset=0.00
[TCP] Calibrated value: 24.56
[TCP] Publishing to MQTT: {"time":1699123456,"name":"Test Voltage","device_id":"DEVICE_001","value":24.56,"description":"","unit":"V"}
```

---

## Common Formula Reference

### Temperature Conversions

**Fahrenheit to Celsius:**
```
°C = (°F - 32) × 5/9
°C = (°F × 0.5556) - 17.778

scale: 0.5556
offset: -17.778
```

**Celsius to Fahrenheit:**
```
°F = (°C × 9/5) + 32
°F = (°C × 1.8) + 32

scale: 1.8
offset: 32
```

**Kelvin to Celsius:**
```
°C = K - 273.15

scale: 1.0
offset: -273.15
```

### Pressure Conversions

**PSI to Bar:**
```
bar = PSI × 0.06895

scale: 0.06895
offset: 0.0
```

**Bar to PSI:**
```
PSI = bar × 14.5038

scale: 14.5038
offset: 0.0
```

**kPa to Bar:**
```
bar = kPa × 0.01

scale: 0.01
offset: 0.0
```

### Voltage/Current Scaling

**Centivolt to Volt:**
```
V = cV × 0.01

scale: 0.01
offset: 0.0
```

**Millivolt to Volt:**
```
V = mV × 0.001

scale: 0.001
offset: 0.0
```

**Milliampere to Ampere:**
```
A = mA × 0.001

scale: 0.001
offset: 0.0
```

### Percentage Scaling

**0-10000 to 0-100%:**
```
% = raw × 0.01

scale: 0.01
offset: 0.0
```

**0-1000 to 0-100%:**
```
% = raw × 0.1

scale: 0.1
offset: 0.0
```

---

## Troubleshooting

### Issue 1: Nilai Tidak Berubah Setelah Set Calibration

**Gejala:** Setelah update scale/offset, nilai MQTT masih sama seperti raw value.

**Penyebab:**
- Config belum tersimpan ke `devices.json`
- Cache belum refresh

**Solusi:**
1. Restart gateway untuk force reload config
2. Check `devices.json` apakah scale/offset sudah tersimpan
3. Pastikan tidak ada error di Serial Monitor saat update

### Issue 2: Nilai Kalibrasi Tidak Akurat

**Gejala:** Hasil kalibrasi berbeda dengan yang diharapkan.

**Penyebab:**
- Formula salah
- Data type konversi belum benar
- Offset terbalik (positif/negatif)

**Solusi:**
1. Cek formula menggunakan kalkulator manual
2. Pastikan data type sesuai dengan sensor
3. Coba dengan scale=1.0, offset=0.0 untuk lihat raw value
4. Debug menggunakan Serial Monitor

### Issue 3: Unit Tidak Muncul di MQTT

**Gejala:** Field `unit` kosong atau tidak ada.

**Penyebab:**
- Register dibuat sebelum firmware update
- Field `unit` tidak di-set saat create

**Solusi:**
1. Update register dengan field `unit`:
```json
{
  "op": "update",
  "type": "register",
  "device_id": "DEVICE_001",
  "register_id": "DEVICE_001_40001",
  "config": {
    "unit": "V"
  }
}
```

### Issue 4: Migration Tidak Jalan

**Gejala:** Register lama tidak mendapat default calibration values.

**Penyebab:**
- File `devices.json` corrupt
- SPIFFS/LittleFS full

**Solusi:**
1. Check Serial Monitor untuk migration logs
2. Check SPIFFS usage: `/info` endpoint
3. Backup dan reset `devices.json` jika perlu

---

## Best Practices

### 1. Set Unit Konsisten

Selalu set unit untuk setiap register:
```json
{
  "unit": "V"  // GOOD
}
```

Jangan biarkan kosong:
```json
{
  "unit": ""  // BAD - sulit dibedakan di dashboard
}
```

### 2. Dokumentasikan Formula

Gunakan field `description` untuk dokumentasi formula:
```json
{
  "register_name": "Voltage L1",
  "scale": 0.01,
  "offset": 0.0,
  "unit": "V",
  "description": "Raw cV to V conversion (scale 0.01)"
}
```

### 3. Test di Development Dulu

- Test kalibrasi dengan nilai known/reference
- Validasi formula menggunakan kalkulator
- Check serial monitor untuk debug output

### 4. Backup Config Sebelum Update

Sebelum update calibration values:
1. Export `devices.json` via BLE
2. Simpan backup
3. Baru lakukan update

### 5. Gunakan Nilai Presisi Tinggi

Untuk scale yang kecil, gunakan presisi tinggi:
```json
{
  "scale": 0.06895,  // GOOD - 5 decimal places
  "scale": 0.07      // BAD - kurang presisi
}
```

---

## Appendix A: Complete Example

### Scenario: Power Meter dengan 8 Registers

**Device Configuration:**
```json
{
  "op": "create",
  "type": "device",
  "device_id": "PM001",
  "config": {
    "device_name": "PM-5560 Power Meter",
    "protocol": "TCP",
    "slave_id": 1,
    "ip": "192.168.1.100",
    "port": 502,
    "timeout": 3000,
    "retry_count": 3,
    "refresh_rate_ms": 1000
  }
}
```

**Register Configurations:**

**1. Voltage L1 (centivolt to volt):**
```json
{
  "op": "create",
  "type": "register",
  "device_id": "PM001",
  "config": {
    "register_name": "Voltage L1",
    "address": 40001,
    "function_code": 3,
    "data_type": "uint16",
    "scale": 0.01,
    "offset": 0.0,
    "unit": "V",
    "description": "Line 1 voltage"
  }
}
```

**2. Current L1 (milliampere to ampere):**
```json
{
  "op": "create",
  "type": "register",
  "device_id": "PM001",
  "config": {
    "register_name": "Current L1",
    "address": 40003,
    "function_code": 3,
    "data_type": "uint16",
    "scale": 0.001,
    "offset": 0.0,
    "unit": "A",
    "description": "Line 1 current"
  }
}
```

**3. Active Power (watt):**
```json
{
  "op": "create",
  "type": "register",
  "device_id": "PM001",
  "config": {
    "register_name": "Active Power",
    "address": 40005,
    "function_code": 3,
    "data_type": "uint32",
    "scale": 1.0,
    "offset": 0.0,
    "unit": "W",
    "description": "Total active power"
  }
}
```

**4. Power Factor (0-1000 to 0-1.0):**
```json
{
  "op": "create",
  "type": "register",
  "device_id": "PM001",
  "config": {
    "register_name": "Power Factor",
    "address": 40007,
    "function_code": 3,
    "data_type": "uint16",
    "scale": 0.001,
    "offset": 0.0,
    "unit": "",
    "description": "Power factor (0-1)"
  }
}
```

**5. Frequency (Hz × 100):**
```json
{
  "op": "create",
  "type": "register",
  "device_id": "PM001",
  "config": {
    "register_name": "Frequency",
    "address": 40009,
    "function_code": 3,
    "data_type": "uint16",
    "scale": 0.01,
    "offset": 0.0,
    "unit": "Hz",
    "description": "Line frequency"
  }
}
```

**6. Energy (kWh):**
```json
{
  "op": "create",
  "type": "register",
  "device_id": "PM001",
  "config": {
    "register_name": "Total Energy",
    "address": 40011,
    "function_code": 3,
    "data_type": "uint32",
    "scale": 0.01,
    "offset": 0.0,
    "unit": "kWh",
    "description": "Total energy consumption"
  }
}
```

**7. Temperature (with offset correction):**
```json
{
  "op": "create",
  "type": "register",
  "device_id": "PM001",
  "config": {
    "register_name": "Internal Temperature",
    "address": 40013,
    "function_code": 3,
    "data_type": "int16",
    "scale": 0.1,
    "offset": -2.0,
    "unit": "°C",
    "description": "Device internal temperature (calibrated)"
  }
}
```

**8. Status (no calibration):**
```json
{
  "op": "create",
  "type": "register",
  "device_id": "PM001",
  "config": {
    "register_name": "Status Register",
    "address": 40015,
    "function_code": 3,
    "data_type": "uint16",
    "scale": 1.0,
    "offset": 0.0,
    "unit": "",
    "description": "Device status flags"
  }
}
```

**Expected MQTT Output:**

Topic: `v1/devices/me/telemetry` (Default Mode)

```json
{
  "timestamp": 1699123456789,
  "data": [
    {
      "time": 1699123456,
      "name": "Voltage L1",
      "device_id": "PM001",
      "value": 230.5,
      "description": "Line 1 voltage",
      "unit": "V"
    },
    {
      "time": 1699123456,
      "name": "Current L1",
      "device_id": "PM001",
      "value": 5.234,
      "description": "Line 1 current",
      "unit": "A"
    },
    {
      "time": 1699123456,
      "name": "Active Power",
      "device_id": "PM001",
      "value": 1206,
      "description": "Total active power",
      "unit": "W"
    },
    {
      "time": 1699123456,
      "name": "Power Factor",
      "device_id": "PM001",
      "value": 0.95,
      "description": "Power factor (0-1)",
      "unit": ""
    },
    {
      "time": 1699123456,
      "name": "Frequency",
      "device_id": "PM001",
      "value": 50.02,
      "description": "Line frequency",
      "unit": "Hz"
    },
    {
      "time": 1699123456,
      "name": "Total Energy",
      "device_id": "PM001",
      "value": 1234.56,
      "description": "Total energy consumption",
      "unit": "kWh"
    },
    {
      "time": 1699123456,
      "name": "Internal Temperature",
      "device_id": "PM001",
      "value": 43.5,
      "description": "Device internal temperature (calibrated)",
      "unit": "°C"
    },
    {
      "time": 1699123456,
      "name": "Status Register",
      "device_id": "PM001",
      "value": 0,
      "description": "Device status flags",
      "unit": ""
    }
  ]
}
```

---

## Appendix B: Python Test Script

File: `test_calibration.py`

```python
#!/usr/bin/env python3
"""
Test script untuk kalibrasi register
Menguji berbagai scenario kalibrasi menggunakan BLE
"""

import asyncio
from bleak import BleakClient, BleakScanner
import json

# UUID for BLE characteristics (adjust to your firmware)
SERVICE_UUID = "12345678-1234-1234-1234-123456789012"
CHAR_WRITE_UUID = "12345678-1234-1234-1234-123456789013"
CHAR_NOTIFY_UUID = "12345678-1234-1234-1234-123456789014"

async def send_command(client, command):
    """Send JSON command via BLE"""
    payload = json.dumps(command)
    await client.write_gatt_char(CHAR_WRITE_UUID, payload.encode())
    print(f"Sent: {payload}")
    await asyncio.sleep(2)

async def test_calibration():
    """Test calibration scenarios"""

    # Find gateway device
    print("Scanning for SRT-MGATE-1210...")
    devices = await BleakScanner.discover()
    gateway = None
    for d in devices:
        if "MGATE" in d.name:
            gateway = d
            break

    if not gateway:
        print("Gateway not found!")
        return

    print(f"Connecting to {gateway.name}...")

    async with BleakClient(gateway.address) as client:
        print("Connected!")

        # Test 1: Create register dengan voltage divider
        print("\n=== Test 1: Voltage Divider (cV to V) ===")
        cmd = {
            "op": "create",
            "type": "register",
            "device_id": "TEST_DEV",
            "config": {
                "register_name": "Test Voltage",
                "address": 40001,
                "function_code": 3,
                "data_type": "uint16",
                "scale": 0.01,
                "offset": 0.0,
                "unit": "V",
                "description": "Voltage sensor with divider"
            }
        }
        await send_command(client, cmd)

        # Test 2: Create register dengan temperature offset
        print("\n=== Test 2: Temperature with Offset ===")
        cmd = {
            "op": "create",
            "type": "register",
            "device_id": "TEST_DEV",
            "config": {
                "register_name": "Test Temperature",
                "address": 40002,
                "function_code": 3,
                "data_type": "float",
                "scale": 1.0,
                "offset": -2.5,
                "unit": "°C",
                "description": "Temperature with -2.5C offset"
            }
        }
        await send_command(client, cmd)

        # Test 3: PSI to Bar conversion
        print("\n=== Test 3: PSI to Bar Conversion ===")
        cmd = {
            "op": "create",
            "type": "register",
            "device_id": "TEST_DEV",
            "config": {
                "register_name": "Test Pressure",
                "address": 40003,
                "function_code": 3,
                "data_type": "float",
                "scale": 0.06895,
                "offset": 0.0,
                "unit": "bar",
                "description": "Pressure PSI to bar"
            }
        }
        await send_command(client, cmd)

        # List registers untuk verify
        print("\n=== Verifying Configuration ===")
        cmd = {
            "op": "list",
            "type": "register",
            "device_id": "TEST_DEV"
        }
        await send_command(client, cmd)

        print("\nCalibration test completed!")
        print("Check Serial Monitor untuk melihat hasil kalibrasi saat polling.")

if __name__ == "__main__":
    asyncio.run(test_calibration())
```

---

## Support

Untuk pertanyaan atau issue terkait kalibrasi register:

1. **Check Serial Monitor** untuk debug output
2. **Verify Config** menggunakan BLE list command
3. **Test Formula** menggunakan kalkulator manual
4. **Contact Support** jika masih ada masalah

---

**Document Version:** 1.1
**Firmware Version:** v2.1.1
**Developer:** Kemal
**Last Updated:** November 14, 2025 (Friday) - WIB (GMT+7)
**Author:** SRT Engineering Team
