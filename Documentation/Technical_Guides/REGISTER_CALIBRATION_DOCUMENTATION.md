# Register Calibration - Documentation

**SRT-MGATE-1210 Modbus IIoT Gateway**

[Home](../../README.md) > [Documentation](../README.md) > [Technical Guides](README.md) > Register Calibration

**Current Version:** v2.3.0
**Developer:** Kemal
**Last Updated:** November 21, 2025

## Overview

SRT-MGATE-1210 Firmware v2.3.0 supports **automatic calibration** for Modbus register values using the **scale & offset** formula. This feature enables conversion of raw Modbus values to appropriate measurement units without requiring post-processing on the subscriber side.

**Key Features:**
- ✅ Automatic calibration with formula: `final_value = (raw_value × scale) + offset`
- ✅ Support for custom measurement units (°C, V, A, PSI, bar, etc.)
- ✅ Negative values allowed for scale & offset
- ✅ Default values for backward compatibility
- ✅ Auto-migration for existing registers
- ✅ Device-level polling interval (not per-register)

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

| Field    | Type   | Required | Default | Description                                  |
| -------- | ------ | -------- | ------- | -------------------------------------------- |
| `scale`  | float  | No       | 1.0     | Multiplier for calibration (can be negative) |
| `offset` | float  | No       | 0.0     | Offset for calibration (can be negative)     |
| `unit`   | string | No       | ""      | Measurement unit (°C, V, A, PSI, bar, etc.)  |

### Default Values

If fields are not provided when creating a register, the firmware will use default values:

```json
{
  "scale": 1.0,
  "offset": 0.0,
  "unit": ""
}
```

With these default values:
- `final_value = (raw_value × 1.0) + 0.0 = raw_value`
- No value change (backward compatible)

### Value Constraints

- **scale**: Can be positive, negative, or zero
- **offset**: Can be positive, negative, or zero
- **unit**: Free-form string (maximum 50 characters)

---

## Calibration Formula

### Formula

```
final_value = (raw_value × scale) + offset
```

### Execution Order

1. **Read raw value from Modbus** (16-bit or 32-bit register)
2. **Convert data type** (int16, uint16, float, int32, uint32)
3. **Apply calibration** using scale & offset
4. **Publish to MQTT** with calibrated value

### Flow Diagram

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

Register without calibration (using default values):

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
    "description": "Room humidity"
  }
}
```

**Result:**
- Raw Modbus: 65 → Final value: **65** (no unit)
- MQTT payload:
```json
{
  "time": 1699123456,
  "name": "Humidity Sensor",
  "device_id": "DEVICE_001",
  "value": 65,
  "description": "Room humidity",
  "unit": ""
}
```

### Example 2: Voltage Divider (Scale Only)

Voltage sensor with 1:100 voltage divider (raw value in centivolts):

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
    "description": "Main battery voltage"
  }
}
```

**Result:**
- Raw Modbus: 2456 → Calibrated: (2456 × 0.01) + 0 = **24.56 V**
- MQTT payload:
```json
{
  "time": 1699123456,
  "name": "Battery Voltage",
  "device_id": "DEVICE_001",
  "value": 24.56,
  "description": "Main battery voltage",
  "unit": "V"
}
```

### Example 3: Current Sensor with Offset

Current sensor requiring offset correction:

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
    "description": "Total load current"
  }
}
```

**Result:**
- Raw Modbus: 5.35 A → Calibrated: (5.35 × 1.0) - 0.15 = **5.20 A**

### Example 4: Temperature Sensor (Offset Correction)

Temperature sensor needing calibration due to +2.5°C bias:

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
    "description": "Server room temperature"
  }
}
```

**Result:**
- Raw Modbus: 27.5°C → Calibrated: (27.5 × 1.0) - 2.5 = **25.0°C**

### Example 5: Fahrenheit to Celsius Conversion

Temperature sensor in Fahrenheit to be converted to Celsius:

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
    "description": "Outdoor temperature (converted from F)"
  }
}
```

**Result:**
- Raw Modbus: 77°F → Calibrated: (77 × 0.5556) - 17.778 ≈ **25.0°C**

### Example 6: Pressure Sensor (PSI to Bar)

Pressure sensor in PSI to be converted to bar:

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
    "description": "System hydraulic pressure"
  }
}
```

**Result:**
- Raw Modbus: 100 PSI → Calibrated: (100 × 0.06895) + 0 = **6.895 bar**

### Example 7: Percentage with Divider

Sensor providing 0-10000 values representing 0-100%:

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
    "description": "Water tank level"
  }
}
```

**Result:**
- Raw Modbus: 8550 → Calibrated: (8550 × 0.01) + 0 = **85.50%**

### Example 8: Negative Scale (Inverting)

Sensor with values that need to be inverted:

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
    "description": "Flow velocity (inverted)"
  }
}
```

**Result:**
- Raw Modbus: 50 → Calibrated: (50 × -1.0) + 0 = **-50 m/s**

---

## Use Cases

### Use Case 1: Industrial Monitoring - Mixed Units

**Scenario:** Factory with sensors in various units (V, A, PSI, °F) that need to be standardized to SI units.

**Solution:** Use scale & offset for automatic conversion:
- Voltage: Raw centivolt → Volt (scale: 0.01)
- Current: Raw milliampere → Ampere (scale: 0.001)
- Pressure: Raw PSI → bar (scale: 0.06895)
- Temperature: Raw Fahrenheit → Celsius (scale: 0.5556, offset: -17.778)

**Benefits:**
- Subscribers don't need manual conversion
- Data is immediately ready for analysis
- Unit consistency across the entire system

### Use Case 2: Sensor Calibration - Offset Correction

**Scenario:** Aging temperature sensor with +3°C bias.

**Solution:** Use offset for correction:
```json
{
  "scale": 1.0,
  "offset": -3.0,
  "unit": "°C"
}
```

**Benefits:**
- No need to replace sensor
- Calibration can be updated from mobile app
- Historical data remains valid

### Use Case 3: Multi-Range Sensors

**Scenario:** Sensor that can be read in 2 different ranges (0-5V for 0-100°C).

**Solution:** Configure 2 registers with different calibrations:

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

**Scenario:** Integration with old PLC sending values in proprietary format.

**Solution:** Reverse-engineer PLC formula and apply at gateway:
```json
{
  "scale": 0.0625,
  "offset": -1024.0,
  "unit": "kW"
}
```

**Benefits:**
- No need to update PLC programming
- Gateway acts as translator
- Easy calibration updates without downtime

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
- `device_id`: Previously created device ID
- `config.register_name`: Register name
- `config.address`: Modbus register address
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
- Only fields to be updated need to be sent
- `register_id` format: `{device_id}_{address}`
- Update will be applied immediately on next polling cycle

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
      "description": "Battery voltage"
    }
  ]
}
```

---

## Migration Notes

### Automatic Migration

Firmware automatically performs migration on boot for existing registers:

#### Migration 1: Add Default Calibration Values

If a register does not have `scale`, `offset`, or `unit` fields, firmware will add them:

```
[MIGRATION] Added scale=1.0 to register DEVICE_001_40001
[MIGRATION] Added offset=0.0 to register DEVICE_001_40001
[MIGRATION] Added unit="" to register DEVICE_001_40001
```

#### Migration 2: Remove Old `refresh_rate_ms`

The `refresh_rate_ms` per-register field has been deprecated and is automatically removed:

```
[MIGRATION] Removed refresh_rate_ms from register DEVICE_001_40001 (now using device-level polling)
```

#### Migration 3: Auto-Save

After migration, `devices.json` is automatically saved:

```
[MIGRATION] Saving devices.json with calibration fields and removed refresh_rate_ms...
```

### Backward Compatibility

- ✅ Old registers without calibration will continue to work (scale=1.0, offset=0.0)
- ✅ Published values remain unchanged for registers without calibration
- ✅ MQTT payload format remains consistent (6 fields)

### Manual Migration (Optional)

To update calibration for existing registers:

1. **List existing registers:**
```json
{"op": "list", "type": "register", "device_id": "DEVICE_001"}
```

2. **Update each register with appropriate calibration:**
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

**IMPORTANT:** Polling interval now uses **device-level** `refresh_rate_ms`, not per-register.

### Configuration

Set polling interval in device config:

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

**Effect:**
- All registers in `DEVICE_001` will be polled every 1000ms (1 second)
- No individual polling interval per-register

### Rationale

**Why device-level polling?**

1. **Modbus Efficiency:**
   - Modbus is a serial/sequential protocol
   - Reading multiple registers in one cycle is more efficient
   - Reduces communication overhead

2. **Consistency:**
   - All device data is read at the same time
   - Consistent timestamps for all registers
   - Easier for data correlation

3. **Simplified Configuration:**
   - User only needs to set 1 interval per device
   - Easier to understand and configure

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

If `refresh_rate_ms` is not set, firmware uses defaults:
- **TCP:** 1000ms (1 second)
- **RTU:** 1000ms (1 second)

---

## MQTT Payload Format

### Payload Structure

Each data point published to MQTT has the following format:

```json
{
  "time": 1699123456,
  "name": "Battery Voltage",
  "device_id": "DEVICE_001",
  "value": 24.56,
  "description": "Main battery voltage",
  "unit": "V"
}
```

### Field Details

| Field         | Type      | Source           | Description                             |
| ------------- | --------- | ---------------- | --------------------------------------- |
| `time`        | integer   | RTC              | Unix timestamp when data was read       |
| `name`        | string    | `register_name`  | Register name from configuration        |
| `device_id`   | string    | Device config    | Modbus device ID                        |
| `value`       | float/int | Calibrated value | Value AFTER scale & offset calibration  |
| `description` | string    | Register config  | Optional description from BLE config    |
| `unit`        | string    | Register config  | Measurement unit (°C, V, A, PSI, bar, etc.) |

### Internal Fields (Not Published)

These fields are used internally for deduplication and routing, NOT published to MQTT:

| Field            | Type    | Purpose                                    |
| ---------------- | ------- | ------------------------------------------ |
| `register_id`    | string  | Unique identifier for deduplication        |
| `register_index` | integer | Index for customize mode topic filtering   |

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

When a register is read, firmware will log:

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

### Issue 1: Value Not Changing After Setting Calibration

**Symptoms:** After updating scale/offset, MQTT values remain the same as raw values.

**Causes:**
- Config not saved to `devices.json`
- Cache not refreshed

**Solution:**
1. Restart gateway to force config reload
2. Check `devices.json` to verify scale/offset are saved
3. Ensure no errors in Serial Monitor during update

### Issue 2: Calibration Values Not Accurate

**Symptoms:** Calibration results differ from expected values.

**Causes:**
- Incorrect formula
- Data type conversion not correct
- Offset sign reversed (positive/negative)

**Solution:**
1. Check formula using manual calculator
2. Ensure data type matches sensor
3. Try with scale=1.0, offset=0.0 to view raw value
4. Debug using Serial Monitor

### Issue 3: Unit Not Appearing in MQTT

**Symptoms:** `unit` field is empty or missing.

**Causes:**
- Register created before firmware update
- `unit` field not set during creation

**Solution:**
1. Update register with `unit` field:
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

### Issue 4: Migration Not Running

**Symptoms:** Old registers not receiving default calibration values.

**Causes:**
- Corrupted `devices.json` file
- SPIFFS/LittleFS full

**Solution:**
1. Check Serial Monitor for migration logs
2. Check SPIFFS usage: `/info` endpoint
3. Backup and reset `devices.json` if necessary

---

## Best Practices

### 1. Set Units Consistently

Always set units for every register:
```json
{
  "unit": "V"  // GOOD
}
```

Don't leave empty:
```json
{
  "unit": ""  // BAD - difficult to distinguish in dashboard
}
```

### 2. Document Formulas

Use `description` field to document formulas:
```json
{
  "register_name": "Voltage L1",
  "scale": 0.01,
  "offset": 0.0,
  "unit": "V",
  "description": "Raw cV to V conversion (scale 0.01)"
}
```

### 3. Test in Development First

- Test calibration with known/reference values
- Validate formula using calculator
- Check serial monitor for debug output

### 4. Backup Config Before Updates

Before updating calibration values:
1. Export `devices.json` via BLE
2. Save backup
3. Then perform update

### 5. Use High Precision Values

For small scales, use high precision:
```json
{
  "scale": 0.06895,  // GOOD - 5 decimal places
  "scale": 0.07      // BAD - insufficient precision
}
```

---

## Appendix A: Complete Example

### Scenario: Power Meter with 8 Registers

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
Test script for register calibration
Tests various calibration scenarios using BLE
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

        # Test 1: Create register with voltage divider
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

        # Test 2: Create register with temperature offset
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

        # List registers to verify
        print("\n=== Verifying Configuration ===")
        cmd = {
            "op": "list",
            "type": "register",
            "device_id": "TEST_DEV"
        }
        await send_command(client, cmd)

        print("\nCalibration test completed!")
        print("Check Serial Monitor to view calibration results during polling.")

if __name__ == "__main__":
    asyncio.run(test_calibration())
```

---

## Support

For questions or issues related to register calibration:

1. **Check Serial Monitor** for debug output
2. **Verify Config** using BLE list command
3. **Test Formula** using manual calculator
4. **Contact Support** if issues persist

---

## Related Documentation

- **[API Reference](../API_Reference/API.md)** - Complete BLE API reference
- **[PROTOCOL.md](PROTOCOL.md)** - Communication protocols
- **[MODBUS_DATATYPES.md](MODBUS_DATATYPES.md)** - Modbus data type reference
- **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** - Troubleshooting guide
- **[MQTT_PUBLISH_MODES_DOCUMENTATION.md](MQTT_PUBLISH_MODES_DOCUMENTATION.md)** - MQTT publishing modes
- **[Best Practices](../BEST_PRACTICES.md)** - Production deployment guidelines
- **[FAQ](../FAQ.md)** - Frequently asked questions

---

**Document Version:** 1.2 (Updated)
**Firmware Version:** v2.3.0
**Developer:** Kemal
**Last Updated:** November 21, 2025

[← Back to Technical Guides](README.md) | [↑ Top](#register-calibration---documentation)

---

**© 2025 PT Surya Inovasi Prioritas (SURIOTA) - R&D Team**
*For technical support: support@suriota.com*
