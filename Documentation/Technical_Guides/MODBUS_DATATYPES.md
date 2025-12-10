# 📊 Modbus Data Types Reference

**SRT-MGATE-1210 Modbus IIoT Gateway**

Comprehensive reference for all supported Modbus data types, endianness variants, and byte order configurations.

[Home](../../README.md) > [Documentation](../README.md) > [Technical Guides](README.md) > Modbus Data Types

---

## 📋 Table of Contents

- [Overview](#overview)
- [Data Type Format](#data-type-format)
- [16-bit Data Types](#16-bit-data-types)
- [32-bit Data Types](#32-bit-data-types)
- [64-bit Data Types](#64-bit-data-types)
- [Endianness Explained](#endianness-explained)
- [Usage Examples](#usage-examples)
- [Common Device Configurations](#common-device-configurations)
- [Troubleshooting](#troubleshooting)

---

## Overview

The SRT-MGATE-1210 firmware supports **40+ Modbus data types** with multiple endianness variants to ensure compatibility with virtually any Modbus device in industrial environments. This comprehensive support eliminates the need for external data conversion and provides seamless integration with PLCs, sensors, meters, and other industrial equipment.

### Key Features

- ✅ **16-bit Types**: INT16, UINT16
- ✅ **32-bit Types**: INT32, UINT32, FLOAT32 (IEEE 754)
- ✅ **64-bit Types**: INT64, UINT64, DOUBLE64 (IEEE 754 double precision)
- ✅ **4 Endianness Variants**: BE, LE, BE_BS, LE_BS for 32/64-bit types
- ✅ **Automatic Byte Ordering**: Hardware-level endianness conversion
- ✅ **IEEE 754 Compliance**: Full floating-point support

---

## Data Type Format

### Format Structure

**Version 2.0.0** uses standardized naming convention:

```
TYPE_SIZE[_ENDIANNESS_VARIANT]
```

| Component              | Description                                     | Example         |
| ---------------------- | ----------------------------------------------- | --------------- |
| **TYPE**               | Data type base (INT, UINT, FLOAT, DOUBLE)       | `INT`, `FLOAT`  |
| **SIZE**               | Bit size (16, 32, 64)                           | `32`, `64`      |
| **ENDIANNESS_VARIANT** | Optional: BE, LE, BE_BS, LE_BS (32/64-bit only) | `_BE`, `_LE_BS` |

### Examples

```
INT16           → 16-bit signed integer (no endianness variant needed)
FLOAT32_BE      → 32-bit float, Big-Endian (ABCD)
INT64_LE_BS     → 64-bit integer, Little-Endian with Word Swap (CDABGHEF)
DOUBLE64_BE     → 64-bit double precision float, Big-Endian (ABCDEFGH)
```

---

## 16-bit Data Types

**Single Register (1 Modbus register = 16 bits)**

| Data Type  | Size   | Range             | Description             |
| ---------- | ------ | ----------------- | ----------------------- |
| **INT16**  | 16-bit | -32,768 to 32,767 | Signed 16-bit integer   |
| **UINT16** | 16-bit | 0 to 65,535       | Unsigned 16-bit integer |

### 16-bit Usage Notes

- 16-bit types occupy **1 Modbus register**
- No endianness variants (single register, no byte ordering issues)
- Most common in legacy industrial devices
- Direct memory mapping without conversion

### 16-bit Example

```json
{
  "address": 40001,
  "register_name": "TEMPERATURE_C",
  "data_type": "INT16",
  "scale": 0.1,
  "offset": 0.0,
  "unit": "°C",
  "description": "Temperature sensor (scaled by 0.1)"
}
```

**Example value**: Register value `235` → Real value: `235 × 0.1 = 23.5°C`

---

## 32-bit Data Types

**Dual Register (2 Modbus registers = 32 bits)**

### 32-bit Integers

| Data Type        | Endianness                   | Byte Order | Range                           |
| ---------------- | ---------------------------- | ---------- | ------------------------------- |
| **INT32_BE**     | Big-Endian                   | ABCD       | -2,147,483,648 to 2,147,483,647 |
| **INT32_LE**     | Little-Endian                | DCBA       | -2,147,483,648 to 2,147,483,647 |
| **INT32_BE_BS**  | Big-Endian with Byte Swap    | BADC       | -2,147,483,648 to 2,147,483,647 |
| **INT32_LE_BS**  | Little-Endian with Word Swap | CDAB       | -2,147,483,648 to 2,147,483,647 |
| **UINT32_BE**    | Big-Endian                   | ABCD       | 0 to 4,294,967,295              |
| **UINT32_LE**    | Little-Endian                | DCBA       | 0 to 4,294,967,295              |
| **UINT32_BE_BS** | Big-Endian with Byte Swap    | BADC       | 0 to 4,294,967,295              |
| **UINT32_LE_BS** | Little-Endian with Word Swap | CDAB       | 0 to 4,294,967,295              |

### 32-bit Floating Point (IEEE 754)

| Data Type         | Endianness                   | Byte Order | Precision | Range                   |
| ----------------- | ---------------------------- | ---------- | --------- | ----------------------- |
| **FLOAT32_BE**    | Big-Endian                   | ABCD       | ~7 digits | ±3.4 × 10<sup>±38</sup> |
| **FLOAT32_LE**    | Little-Endian                | DCBA       | ~7 digits | ±3.4 × 10<sup>±38</sup> |
| **FLOAT32_BE_BS** | Big-Endian with Byte Swap    | BADC       | ~7 digits | ±3.4 × 10<sup>±38</sup> |
| **FLOAT32_LE_BS** | Little-Endian with Word Swap | CDAB       | ~7 digits | ±3.4 × 10<sup>±38</sup> |

### 32-bit Usage Notes

- 32-bit types occupy **2 consecutive Modbus registers**
- Address must point to the first register of the pair
- Most common in modern industrial PLCs and smart sensors
- IEEE 754 floating-point for high-precision measurements

### 32-bit Example

```json
{
  "address": 40010,
  "register_name": "POWER_KW",
  "data_type": "FLOAT32_BE",
  "scale": 1.0,
  "offset": 0.0,
  "unit": "kW",
  "description": "Active power in kilowatts"
}
```

---

## 64-bit Data Types

**Quad Register (4 Modbus registers = 64 bits)**

### 64-bit Integers

| Data Type        | Endianness                   | Byte Order | Range                                                   |
| ---------------- | ---------------------------- | ---------- | ------------------------------------------------------- |
| **INT64_BE**     | Big-Endian                   | ABCDEFGH   | -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807 |
| **INT64_LE**     | Little-Endian                | HGFEDCBA   | -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807 |
| **INT64_BE_BS**  | Big-Endian with Byte Swap    | BADCFEHG   | -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807 |
| **INT64_LE_BS**  | Little-Endian with Word Swap | CDABGHEF   | -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807 |
| **UINT64_BE**    | Big-Endian                   | ABCDEFGH   | 0 to 18,446,744,073,709,551,615                         |
| **UINT64_LE**    | Little-Endian                | HGFEDCBA   | 0 to 18,446,744,073,709,551,615                         |
| **UINT64_BE_BS** | Big-Endian with Byte Swap    | BADCFEHG   | 0 to 18,446,744,073,709,551,615                         |
| **UINT64_LE_BS** | Little-Endian with Word Swap | CDABGHEF   | 0 to 18,446,744,073,709,551,615                         |

### 64-bit Floating Point (IEEE 754 Double Precision)

| Data Type          | Endianness                   | Byte Order | Precision     | Range                    |
| ------------------ | ---------------------------- | ---------- | ------------- | ------------------------ |
| **DOUBLE64_BE**    | Big-Endian                   | ABCDEFGH   | ~15-16 digits | ±1.7 × 10<sup>±308</sup> |
| **DOUBLE64_LE**    | Little-Endian                | HGFEDCBA   | ~15-16 digits | ±1.7 × 10<sup>±308</sup> |
| **DOUBLE64_BE_BS** | Big-Endian with Byte Swap    | BADCFEHG   | ~15-16 digits | ±1.7 × 10<sup>±308</sup> |
| **DOUBLE64_LE_BS** | Little-Endian with Word Swap | CDABGHEF   | ~15-16 digits | ±1.7 × 10<sup>±308</sup> |

### 64-bit Usage Notes

- 64-bit types occupy **4 consecutive Modbus registers**
- Address must point to the first register of the quad
- Common in energy meters, high-precision sensors, and financial systems
- Double precision IEEE 754 for scientific/laboratory measurements
- Higher memory and bandwidth requirements

### 64-bit Example

```json
{
  "address": 40020,
  "register_name": "TOTAL_ENERGY_KWH",
  "data_type": "DOUBLE64_BE",
  "scale": 1.0,
  "offset": 0.0,
  "unit": "kWh",
  "description": "Cumulative energy consumption (high precision)"
}
```

---

## Endianness Explained

### What is Endianness?

**Endianness** refers to the order in which bytes are arranged in multi-byte data types. Different manufacturers use different byte ordering conventions, requiring the gateway to convert between formats.

### Byte Order Notation

For a 32-bit value with bytes **A B C D** (most significant to least significant):

```
Original Value: [A] [B] [C] [D]
                MSB ←——————→ LSB
```

| Suffix     | Name                         | Byte Order | Description                                 |
| ---------- | ---------------------------- | ---------- | ------------------------------------------- |
| **_BE**    | Big-Endian                   | ABCD       | Standard, MSB first (network byte order)    |
| **_LE**    | Little-Endian                | DCBA       | Completely reversed byte order              |
| **_BE_BS** | Big-Endian with Byte Swap    | BADC       | Word order unchanged, bytes swapped within  |
| **_LE_BS** | Little-Endian with Word Swap | CDAB       | Word order reversed, bytes within unchanged |

### Visual Example: 32-bit Value `0x12345678`

```
Register Layout:

_BE (ABCD):     [Reg1: 0x1234] [Reg2: 0x5678]  ← Standard Big-Endian
_LE (DCBA):     [Reg1: 0x7856] [Reg2: 0x3412]  ← Full reversal
_BE_BS (BADC):  [Reg1: 0x3412] [Reg2: 0x7856]  ← Bytes swapped in each word
_LE_BS (CDAB):  [Reg1: 0x5678] [Reg2: 0x1234]  ← Words swapped
```

### 64-bit Byte Order

For a 64-bit value with bytes **A B C D E F G H**:

```
Original Value: [A] [B] [C] [D] [E] [F] [G] [H]
                MSB ←———————————————————————→ LSB
```

| Suffix     | Byte Order | Register Layout                     |
| ---------- | ---------- | ----------------------------------- |
| **_BE**    | ABCDEFGH   | [R1: AB] [R2: CD] [R3: EF] [R4: GH] |
| **_LE**    | HGFEDCBA   | [R1: HG] [R2: FE] [R3: DC] [R4: BA] |
| **_BE_BS** | BADCFEHG   | [R1: BA] [R2: DC] [R3: FE] [R4: HG] |
| **_LE_BS** | CDABGHEF   | [R1: CD] [R2: AB] [R3: GH] [R4: EF] |

### How to Determine Correct Endianness

1. **Check Device Documentation**: Most manufacturers specify byte order in Modbus register maps
2. **Test Known Values**: Write a known value (e.g., `1234.56`) and read it back with different endianness
3. **Common Patterns**:
   - **Siemens PLCs**: Often use `_BE` (ABCD)
   - **Schneider Electric**: Often use `_LE_BS` (CDAB)
   - **ABB Devices**: Often use `_BE_BS` (BADC)
   - **Chinese Meters**: Often use `_LE_BS` (CDAB)
4. **Trial and Error**: If undocumented, try all 4 variants until correct value appears

---

## Usage Examples

### Example 1: Temperature Sensor (16-bit)

**Device**: DHT22 Temperature/Humidity Sensor
**Register**: 40001 (Temperature in 0.1°C units)
**Value Range**: -400 to 800 (-40.0°C to 80.0°C)

```json
{
  "op": "create",
  "type": "register",
  "device_id": "DHT22_001",
  "config": {
    "address": 40001,
    "register_name": "TEMPERATURE",
    "type": "Holding Register",
    "function_code": 3,
    "data_type": "INT16",
    "scale": 0.1,
    "offset": 0.0,
    "unit": "°C",
    "description": "Ambient temperature",
    "refresh_rate_ms": 2000
  }
}
```

**Reading**: Register = `235` → Value = `235 × 0.1 = 23.5°C`

---

### Example 2: Power Meter (32-bit Float)

**Device**: Schneider PM5560 Power Meter
**Register**: 3000 (Active Power)
**Endianness**: Little-Endian with Word Swap (CDAB)

```json
{
  "op": "create",
  "type": "register",
  "device_id": "PM5560_001",
  "config": {
    "address": 3000,
    "register_name": "ACTIVE_POWER_KW",
    "type": "Holding Register",
    "function_code": 3,
    "data_type": "FLOAT32_LE_BS",
    "scale": 1.0,
    "offset": 0.0,
    "unit": "kW",
    "description": "Three-phase active power",
    "refresh_rate_ms": 1000
  }
}
```

**Reading**: Registers [3000-3001] → Float value: `12.345 kW`

---

### Example 3: Energy Meter (64-bit Double)

**Device**: Carlo Gavazzi EM340 Energy Meter
**Register**: 40100 (Total Energy)
**Endianness**: Big-Endian (ABCDEFGH)

```json
{
  "op": "create",
  "type": "register",
  "device_id": "EM340_001",
  "config": {
    "address": 40100,
    "register_name": "TOTAL_ENERGY_KWH",
    "type": "Holding Register",
    "function_code": 3,
    "data_type": "DOUBLE64_BE",
    "scale": 1.0,
    "offset": 0.0,
    "unit": "kWh",
    "description": "Cumulative energy consumption",
    "refresh_rate_ms": 5000
  }
}
```

**Reading**: Registers [40100-40103] → Double value: `123456789.12345 kWh`

---

### Example 4: Solar Inverter (Mixed Types)

**Device**: SMA Sunny Tripower Inverter
**Registers**: Multiple types

```json
{
  "op": "batch_create",
  "type": "register",
  "device_id": "SMA_INVERTER_001",
  "registers": [
    {
      "address": 30775,
      "register_name": "DC_VOLTAGE_V",
      "data_type": "UINT32_BE",
      "scale": 0.01,
      "unit": "V",
      "description": "DC input voltage"
    },
    {
      "address": 30777,
      "register_name": "DC_CURRENT_A",
      "data_type": "INT32_BE",
      "scale": 0.001,
      "unit": "A",
      "description": "DC input current"
    },
    {
      "address": 30783,
      "register_name": "AC_POWER_W",
      "data_type": "UINT32_BE",
      "scale": 1.0,
      "unit": "W",
      "description": "AC output power"
    },
    {
      "address": 30529,
      "register_name": "TOTAL_YIELD_KWH",
      "data_type": "UINT64_BE",
      "scale": 1.0,
      "unit": "kWh",
      "description": "Lifetime energy production"
    }
  ]
}
```

---

## Common Device Configurations

### Manufacturer-Specific Endianness

| Manufacturer           | Common Model      | Typical Endianness | Notes                  |
| ---------------------- | ----------------- | ------------------ | ---------------------- |
| **Siemens**            | S7-1200/1500 PLCs | `_BE` (ABCD)       | Standard Modbus TCP    |
| **Schneider Electric** | PM5560, EM6400    | `_LE_BS` (CDAB)    | PowerLogic series      |
| **ABB**                | M2M, ACS880       | `_BE_BS` (BADC)    | Drives and meters      |
| **Allen-Bradley**      | CompactLogix      | `_LE_BS` (CDAB)    | Via ProSoft gateways   |
| **Modicon**            | M340, M580        | `_BE` (ABCD)       | Native Modbus TCP      |
| **Mitsubishi**         | FX5U, iQ-R        | `_LE_BS` (CDAB)    | Via Modbus gateway     |
| **Omron**              | NJ/NX series      | `_LE_BS` (CDAB)    | EtherNet/IP to Modbus  |
| **Carlo Gavazzi**      | EM340, EM530      | `_BE` (ABCD)       | Energy meters          |
| **SMA**                | Sunny Tripower    | `_BE` (ABCD)       | Solar inverters        |
| **Huawei**             | SUN2000           | `_LE_BS` (CDAB)    | Solar inverters        |
| **Eastron**            | SDM630, SDM120    | `_LE_BS` (CDAB)    | Power meters (Chinese) |
| **Phoenix Contact**    | ILC, RFC          | `_BE` (ABCD)       | Industrial controllers |
| **Wago**               | 750 series        | `_BE` (ABCD)       | I/O modules            |

---

## Troubleshooting

### Incorrect Data Values

**Symptom**: Values are completely wrong, garbage, or zero

**Solutions**:

1. **Wrong Endianness**: Try all 4 variants (`_BE`, `_LE`, `_BE_BS`, `_LE_BS`)
   ```bash
   # Test script example
   FLOAT32_BE      → 123.45
   FLOAT32_LE      → 1.234e+38 (wrong)
   FLOAT32_BE_BS   → 3.456e-12 (wrong)
   FLOAT32_LE_BS   → 123.45 ✓
   ```

2. **Wrong Data Type**:
   - INT32 vs UINT32 (negative values appear as large positive)
   - FLOAT32 vs INT32 (completely different interpretation)

3. **Wrong Scale/Offset**:
   - Check device documentation for scaling factors
   - Common scales: 0.1, 0.01, 0.001, 10, 100

4. **Wrong Register Address**:
   - Verify address from device manual
   - Check if device uses 0-based or 1-based addressing

---

### NaN or Infinity Values

**Symptom**: Readings show `NaN` or `Inf`

**Causes**:

1. **Invalid IEEE 754 Representation**: Device wrote invalid float value
2. **Communication Error**: Corrupted Modbus response
3. **Wrong Endianness**: Bytes interpreted incorrectly

**Solutions**:

```json
{
  "data_type": "FLOAT32_BE",
  "validation": {
    "min": -1000.0,
    "max": 1000.0,
    "reject_nan": true,
    "reject_inf": true
  }
}
```

---

### Byte Swap Detection Script

Use this BLE command to test all 4 endianness variants:

```json
{
  "op": "test_endianness",
  "device_id": "TEST_DEVICE",
  "address": 40001,
  "base_type": "FLOAT32",
  "expected_range": [0.0, 1000.0]
}
```

**Response**:
```json
{
  "results": [
    {"data_type": "FLOAT32_BE", "value": 123.45, "valid": true},
    {"data_type": "FLOAT32_LE", "value": 1.234e+38, "valid": false},
    {"data_type": "FLOAT32_BE_BS", "value": 3.456e-12, "valid": false},
    {"data_type": "FLOAT32_LE_BS", "value": 123.45, "valid": true}
  ],
  "recommendation": "FLOAT32_BE or FLOAT32_LE_BS"
}
```

---

## Complete Data Type Summary

### All Supported Types (40 Total)

```
16-bit (2 types):
├── INT16
└── UINT16

32-bit Integers (8 types):
├── INT32_BE
├── INT32_LE
├── INT32_BE_BS
├── INT32_LE_BS
├── UINT32_BE
├── UINT32_LE
├── UINT32_BE_BS
└── UINT32_LE_BS

32-bit Floats (4 types):
├── FLOAT32_BE
├── FLOAT32_LE
├── FLOAT32_BE_BS
└── FLOAT32_LE_BS

64-bit Integers (8 types):
├── INT64_BE
├── INT64_LE
├── INT64_BE_BS
├── INT64_LE_BS
├── UINT64_BE
├── UINT64_LE
├── UINT64_BE_BS
└── UINT64_LE_BS

64-bit Doubles (4 types):
├── DOUBLE64_BE
├── DOUBLE64_LE
├── DOUBLE64_BE_BS
└── DOUBLE64_LE_BS
```

---

## Related Documentation

- **[API Reference](../API_Reference/API.md)** - Complete BLE API reference
- **[PROTOCOL.md](PROTOCOL.md)** - Protocol specifications
- **[HARDWARE.md](HARDWARE.md)** - Hardware specifications and GPIO pinout
- **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** - Data type troubleshooting
- **[REGISTER_CALIBRATION_DOCUMENTATION.md](REGISTER_CALIBRATION_DOCUMENTATION.md)** - Register calibration guide
- **[MQTT_PUBLISH_MODES_DOCUMENTATION.md](MQTT_PUBLISH_MODES_DOCUMENTATION.md)** - MQTT publishing modes

---

**Document Version:** 1.1
**Last Updated:** December 10, 2025
**Firmware Version:** 2.5.34

[← Back to Technical Guides](README.md) | [↑ Top](#-modbus-data-types-reference)

---

**© 2025 PT Surya Inovasi Prioritas (SURIOTA) - R&D Team**
*For technical support: support@suriota.com*
