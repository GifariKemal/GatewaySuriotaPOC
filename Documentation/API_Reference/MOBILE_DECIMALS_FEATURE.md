# Mobile Apps - Decimal Precision Feature (v1.0.7)

**Version:** 1.0.7 | **Date:** January 8, 2026 | **Status:** Production Ready

---

## Overview

Field baru `decimals` untuk mengontrol jumlah angka di belakang koma pada setiap
register. Nilai yang dikirim via MQTT/HTTP akan diformat sesuai pengaturan ini.

| Field      | Type    | Default | Range    | Keterangan                          |
| ---------- | ------- | ------- | -------- | ----------------------------------- |
| `decimals` | integer | -1      | -1 s/d 6 | -1 = auto, 0-6 = fixed decimal places |

### Decimal Options

| Value | Label            | Contoh Output |
| ----- | ---------------- | ------------- |
| -1    | Auto (Default)   | `24.560001`   |
| 0     | Integer          | `25`          |
| 1     | 1 decimal place  | `24.6`        |
| 2     | 2 decimal places | `24.56`       |
| 3     | 3 decimal places | `24.560`      |
| 4     | 4 decimal places | `24.5600`     |
| 5     | 5 decimal places | `24.56000`    |
| 6     | 6 decimal places | `24.560000`   |

---

## BLE Commands

### 1. Create Register (dengan decimals)

**Request:**

```json
{
  "op": "create",
  "type": "register",
  "device_id": "D7A3F2",
  "config": {
    "register_name": "Battery Voltage",
    "address": 40001,
    "function_code": "holding",
    "data_type": "UINT16",
    "scale": 0.01,
    "offset": 0.0,
    "decimals": 2,
    "unit": "V",
    "description": "Main battery voltage"
  }
}
```

**Response (Success):**

```json
{
  "status": "ok",
  "device_id": "D7A3F2",
  "register_id": "R3C8D1",
  "data": {
    "register_id": "R3C8D1",
    "register_name": "Battery Voltage",
    "address": 40001,
    "function_code": "holding",
    "data_type": "UINT16",
    "scale": 0.01,
    "offset": 0.0,
    "decimals": 2,
    "unit": "V",
    "description": "Main battery voltage",
    "register_index": 1
  }
}
```

---

### 2. Update Register (ubah decimals saja)

**Request:**

```json
{
  "op": "update",
  "type": "register",
  "device_id": "D7A3F2",
  "register_id": "R3C8D1",
  "config": {
    "decimals": 3
  }
}
```

**Response (Success):**

```json
{
  "status": "ok",
  "device_id": "D7A3F2",
  "register_id": "R3C8D1"
}
```

---

### 3. Update Multiple Fields

**Request:**

```json
{
  "op": "update",
  "type": "register",
  "device_id": "D7A3F2",
  "register_id": "R3C8D1",
  "config": {
    "scale": 0.001,
    "offset": -10.0,
    "decimals": 4,
    "unit": "kV"
  }
}
```

---

### 4. Read Register

**Request:**

```json
{
  "op": "read",
  "type": "register",
  "device_id": "D7A3F2",
  "register_id": "R3C8D1"
}
```

**Response:**

```json
{
  "status": "ok",
  "device_id": "D7A3F2",
  "register_id": "R3C8D1",
  "data": {
    "register_id": "R3C8D1",
    "register_name": "Battery Voltage",
    "address": 40001,
    "function_code": "holding",
    "data_type": "UINT16",
    "scale": 0.01,
    "offset": 0.0,
    "decimals": 2,
    "unit": "V",
    "description": "Main battery voltage",
    "register_index": 1
  }
}
```

---

### 5. List Registers (per device)

**Request:**

```json
{
  "op": "list",
  "type": "register",
  "device_id": "D7A3F2"
}
```

**Response:**

```json
{
  "status": "ok",
  "device_id": "D7A3F2",
  "data": [
    {
      "register_id": "R3C8D1",
      "register_name": "Battery Voltage",
      "address": 40001,
      "data_type": "UINT16",
      "description": "Main battery voltage",
      "scale": 0.01,
      "offset": 0.0,
      "decimals": 2,
      "unit": "V"
    },
    {
      "register_id": "R4D9E2",
      "register_name": "Temperature",
      "address": 40002,
      "data_type": "INT16",
      "description": "Ambient temperature",
      "scale": 0.1,
      "offset": 0.0,
      "decimals": 1,
      "unit": "C"
    }
  ]
}
```

---

## Calibration Formula

```
final_value = round((raw_value x scale + offset), decimals)
```

### Contoh Perhitungan

| Raw Value | Scale | Offset | Decimals | Calculation                  | Output   |
| --------- | ----- | ------ | -------- | ---------------------------- | -------- |
| 2456      | 0.01  | 0      | 2        | round(24.56, 2)              | `24.56`  |
| 2456      | 0.01  | 0      | 1        | round(24.56, 1)              | `24.6`   |
| 2456      | 0.01  | 0      | 0        | round(24.56, 0)              | `25`     |
| 2750      | 0.1   | -40    | 1        | round(275 - 40, 1) = 235.0   | `235.0`  |
| 12345     | 0.001 | 0      | 3        | round(12.345, 3)             | `12.345` |
| 27567     | 0.001 | 0      | -1       | no rounding                  | `27.567` |

---

## UI Implementation Guide

### Recommended UI Component

```
+----------------------------------+
| Calibration Settings             |
+----------------------------------+
|                                  |
| Scale Factor                     |
| +------------------------------+ |
| | 0.01                         | |
| +------------------------------+ |
|                                  |
| Offset                           |
| +------------------------------+ |
| | 0.0                          | |
| +------------------------------+ |
|                                  |
| Decimal Places                   |
| +------------------------------+ |
| | 2 decimal places          v  | |
| +------------------------------+ |
|   Options:                       |
|   - Auto (Default)    -> -1      |
|   - Integer (0)       -> 0       |
|   - 1 decimal place   -> 1       |
|   - 2 decimal places  -> 2       |
|   - 3 decimal places  -> 3       |
|   - 4 decimal places  -> 4       |
|   - 5 decimal places  -> 5       |
|   - 6 decimal places  -> 6       |
|                                  |
| Unit                             |
| +------------------------------+ |
| | V                            | |
| +------------------------------+ |
|                                  |
+----------------------------------+
```

### Dropdown Options Array

```javascript
const decimalOptions = [
  { value: -1, label: "Auto (Default)" },
  { value: 0, label: "Integer (0)" },
  { value: 1, label: "1 decimal place" },
  { value: 2, label: "2 decimal places" },
  { value: 3, label: "3 decimal places" },
  { value: 4, label: "4 decimal places" },
  { value: 5, label: "5 decimal places" },
  { value: 6, label: "6 decimal places" },
];
```

### Flutter Example

```dart
DropdownButtonFormField<int>(
  value: decimals,
  decoration: InputDecoration(labelText: 'Decimal Places'),
  items: [
    DropdownMenuItem(value: -1, child: Text('Auto (Default)')),
    DropdownMenuItem(value: 0, child: Text('Integer (0)')),
    DropdownMenuItem(value: 1, child: Text('1 decimal place')),
    DropdownMenuItem(value: 2, child: Text('2 decimal places')),
    DropdownMenuItem(value: 3, child: Text('3 decimal places')),
    DropdownMenuItem(value: 4, child: Text('4 decimal places')),
    DropdownMenuItem(value: 5, child: Text('5 decimal places')),
    DropdownMenuItem(value: 6, child: Text('6 decimal places')),
  ],
  onChanged: (value) => setState(() => decimals = value!),
)
```

---

## Data Flow

```
+-------------+     +----------------+     +------------------+
| Modbus      | --> | Gateway        | --> | MQTT/HTTP        |
| Raw: 2456   |     | Calibration:   |     | Output: 24.56    |
|             |     | scale: 0.01    |     |                  |
|             |     | offset: 0      |     |                  |
|             |     | decimals: 2    |     |                  |
+-------------+     +----------------+     +------------------+
```

---

## Important Notes

1. **Optional Field** - `decimals` tidak wajib. Jika tidak dikirim, default `-1`
   (auto)

2. **Auto Clamping** - Nilai di luar range otomatis di-clamp:
   - `< -1` akan menjadi `-1`
   - `> 6` akan menjadi `6`

3. **Backward Compatible** - Register lama yang tidak punya field `decimals`
   akan otomatis mendapat nilai `-1` saat firmware boot

4. **Applies to Both** - Berlaku untuk Modbus RTU dan Modbus TCP

5. **Real-time Effect** - Perubahan `decimals` langsung berlaku pada polling
   berikutnya (~3 detik)

---

## Error Handling

### Invalid Device ID

```json
{
  "status": "error",
  "error": "Device not found: INVALID_ID"
}
```

### Invalid Register ID

```json
{
  "status": "error",
  "error": "Register not found: INVALID_REG"
}
```

### Missing Required Fields (Create)

```json
{
  "status": "error",
  "error": "Missing required fields: address or register_name"
}
```

---

## Testing Checklist

- [ ] Create register dengan `decimals: 2` - verify response contains decimals
- [ ] Update register mengubah `decimals` dari 2 ke 0
- [ ] Read register - verify `decimals` field exists
- [ ] List registers - verify semua register punya `decimals`
- [ ] Create register tanpa `decimals` - verify default -1
- [ ] Update dengan `decimals: 10` - verify clamped to 6
- [ ] Update dengan `decimals: -5` - verify clamped to -1
- [ ] Verify MQTT output sesuai dengan decimal setting

---

## Related Documentation

- Full API Reference: `Documentation/API_Reference/API.md`
- Data Types: `Documentation/Technical_Guides/MODBUS_DATATYPES.md`
- Version History: `Documentation/Changelog/VERSION_HISTORY.md`

---

**Questions?** Contact firmware team atau lihat `CLAUDE.md` untuk referensi
lengkap.
