# Mobile Apps - Write Register Feature Guide

**Version:** 1.0.8 | **Date:** January 8, 2026

---

## Overview

The Write Register feature allows mobile apps to write values to Modbus registers,
enabling control scenarios like:

- Setting temperature setpoints
- Toggling relay outputs (coils)
- Adjusting PID parameters
- Controlling motor speeds
- Setting alarm thresholds

---

## Quick Start

### 1. Enable Writing on a Register

First, update the register configuration to allow writing:

```json
{
  "op": "update",
  "type": "register",
  "device_id": "D7A3F2",
  "register_id": "R3C8D1",
  "config": {
    "writable": true,
    "min_value": 0.0,
    "max_value": 100.0
  }
}
```

### 2. Write a Value

Send the write command:

```json
{
  "op": "write",
  "type": "register",
  "device_id": "D7A3F2",
  "register_id": "R3C8D1",
  "value": 25.5
}
```

### 3. Handle Response

```json
{
  "status": "ok",
  "device_id": "D7A3F2",
  "register_id": "R3C8D1",
  "message": "Write successful",
  "data": {
    "register_name": "setpoint_temperature",
    "written_value": 25.5,
    "raw_value": 255,
    "function_code": 6,
    "address": 100
  }
}
```

---

## Register Configuration Fields

| Field       | Type    | Required | Default | Description                              |
| ----------- | ------- | -------- | ------- | ---------------------------------------- |
| `writable`  | boolean | No       | false   | Set to `true` to enable write operations |
| `min_value` | float   | No       | -       | Minimum allowed value for validation     |
| `max_value` | float   | No       | -       | Maximum allowed value for validation     |

### Example: Writable Holding Register

```json
{
  "register_id": "R3C8D1",
  "register_name": "setpoint_temperature",
  "address": 100,
  "function_code": 3,
  "data_type": "UINT16",
  "scale": 0.1,
  "offset": 0.0,
  "decimals": 1,
  "unit": "°C",
  "writable": true,
  "min_value": 0.0,
  "max_value": 50.0
}
```

### Example: Writable Coil (Boolean)

```json
{
  "register_id": "R4D9E2",
  "register_name": "relay_output_1",
  "address": 0,
  "function_code": 1,
  "data_type": "BOOL",
  "scale": 1.0,
  "offset": 0.0,
  "writable": true
}
```

---

## Write Command Structure

```json
{
  "op": "write",
  "type": "register",
  "device_id": "<device_id>",
  "register_id": "<register_id>",
  "value": <number>
}
```

| Field         | Type   | Required | Description                            |
| ------------- | ------ | -------- | -------------------------------------- |
| `op`          | string | Yes      | Must be `"write"`                      |
| `type`        | string | Yes      | Must be `"register"`                   |
| `device_id`   | string | Yes      | Target device ID                       |
| `register_id` | string | Yes      | Target register ID                     |
| `value`       | number | Yes      | Value to write (in user/display units) |

---

## Calibration: How Values Are Converted

The gateway applies **reverse calibration** when writing:

```
raw_value = (user_value - offset) / scale
```

### Example 1: Temperature Setpoint

**Register Config:**
- `scale`: 0.1
- `offset`: 0

**User writes:** `25.5`
**Raw value sent:** `(25.5 - 0) / 0.1 = 255`

### Example 2: With Offset

**Register Config:**
- `scale`: 0.01
- `offset`: -10

**User writes:** `100.0`
**Raw value sent:** `(100.0 - (-10)) / 0.01 = 11000`

### Example 3: Boolean Coil

**User writes:** `1` (or `true`)
**Coil state:** ON

---

## Error Handling

### Error Codes

| Code | Domain  | Description                              |
| ---- | ------- | ---------------------------------------- |
| 315  | MODBUS  | Register not writable                    |
| 316  | MODBUS  | Value out of range                       |
| 317  | MODBUS  | Unsupported data type for write          |
| 318  | MODBUS  | Write operation failed (device error)    |
| 301  | MODBUS  | Device timeout                           |
| 305  | MODBUS  | Device not found                         |

### Error Response Examples

**Register Not Writable:**

```json
{
  "status": "error",
  "error_code": 315,
  "domain": "MODBUS",
  "severity": "ERROR",
  "message": "Register not writable",
  "suggestion": "Enable writable flag in register config"
}
```

**Value Out of Range:**

```json
{
  "status": "error",
  "error_code": 316,
  "domain": "MODBUS",
  "severity": "ERROR",
  "message": "Value 150.0 out of range [0.0 - 100.0]",
  "suggestion": "Enter value between 0.0 and 100.0"
}
```

**Device Timeout:**

```json
{
  "status": "error",
  "error_code": 301,
  "domain": "MODBUS",
  "severity": "ERROR",
  "message": "Device timeout - no response",
  "suggestion": "Check device connection"
}
```

---

## Flutter Implementation Example

### BLE Command Helper

```dart
class SuriotaGateway {
  // ... existing connection code ...

  /// Write a value to a register
  Future<Map<String, dynamic>> writeRegister({
    required String deviceId,
    required String registerId,
    required double value,
  }) async {
    final command = {
      'op': 'write',
      'type': 'register',
      'device_id': deviceId,
      'register_id': registerId,
      'value': value,
    };

    return await sendCommandAndWaitResponse(command);
  }
}
```

### UI Widget: Value Input with Send Button

```dart
class RegisterWriteWidget extends StatefulWidget {
  final String deviceId;
  final String registerId;
  final String registerName;
  final double? minValue;
  final double? maxValue;
  final String unit;

  const RegisterWriteWidget({
    required this.deviceId,
    required this.registerId,
    required this.registerName,
    this.minValue,
    this.maxValue,
    this.unit = '',
  });

  @override
  State<RegisterWriteWidget> createState() => _RegisterWriteWidgetState();
}

class _RegisterWriteWidgetState extends State<RegisterWriteWidget> {
  final TextEditingController _controller = TextEditingController();
  bool _isLoading = false;
  String? _errorMessage;

  Future<void> _writeValue() async {
    final valueStr = _controller.text.trim();
    if (valueStr.isEmpty) {
      setState(() => _errorMessage = 'Please enter a value');
      return;
    }

    final value = double.tryParse(valueStr);
    if (value == null) {
      setState(() => _errorMessage = 'Invalid number format');
      return;
    }

    // Client-side validation (optional, server also validates)
    if (widget.minValue != null && value < widget.minValue!) {
      setState(() => _errorMessage = 'Value must be >= ${widget.minValue}');
      return;
    }
    if (widget.maxValue != null && value > widget.maxValue!) {
      setState(() => _errorMessage = 'Value must be <= ${widget.maxValue}');
      return;
    }

    setState(() {
      _isLoading = true;
      _errorMessage = null;
    });

    try {
      final response = await SuriotaGateway.instance.writeRegister(
        deviceId: widget.deviceId,
        registerId: widget.registerId,
        value: value,
      );

      if (response['status'] == 'ok') {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text('Value written successfully'),
            backgroundColor: Colors.green,
          ),
        );
        _controller.clear();
      } else {
        setState(() => _errorMessage = response['message'] ?? 'Write failed');
      }
    } catch (e) {
      setState(() => _errorMessage = 'Connection error: $e');
    } finally {
      setState(() => _isLoading = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              widget.registerName,
              style: Theme.of(context).textTheme.titleMedium,
            ),
            const SizedBox(height: 8),
            Row(
              children: [
                Expanded(
                  child: TextField(
                    controller: _controller,
                    keyboardType: TextInputType.numberWithOptions(
                      decimal: true,
                      signed: true,
                    ),
                    decoration: InputDecoration(
                      hintText: _buildHintText(),
                      suffixText: widget.unit,
                      errorText: _errorMessage,
                      border: OutlineInputBorder(),
                    ),
                    enabled: !_isLoading,
                  ),
                ),
                const SizedBox(width: 8),
                IconButton(
                  onPressed: _isLoading ? null : _writeValue,
                  icon: _isLoading
                      ? const SizedBox(
                          width: 24,
                          height: 24,
                          child: CircularProgressIndicator(strokeWidth: 2),
                        )
                      : const Icon(Icons.send),
                  style: IconButton.styleFrom(
                    backgroundColor: Theme.of(context).primaryColor,
                    foregroundColor: Colors.white,
                  ),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  String _buildHintText() {
    if (widget.minValue != null && widget.maxValue != null) {
      return '${widget.minValue} - ${widget.maxValue}';
    } else if (widget.minValue != null) {
      return '>= ${widget.minValue}';
    } else if (widget.maxValue != null) {
      return '<= ${widget.maxValue}';
    }
    return 'Enter value';
  }
}
```

### Toggle Widget for Coils (Boolean)

```dart
class CoilToggleWidget extends StatefulWidget {
  final String deviceId;
  final String registerId;
  final String registerName;
  final bool currentValue;

  const CoilToggleWidget({
    required this.deviceId,
    required this.registerId,
    required this.registerName,
    required this.currentValue,
  });

  @override
  State<CoilToggleWidget> createState() => _CoilToggleWidgetState();
}

class _CoilToggleWidgetState extends State<CoilToggleWidget> {
  late bool _value;
  bool _isLoading = false;

  @override
  void initState() {
    super.initState();
    _value = widget.currentValue;
  }

  Future<void> _toggleCoil(bool newValue) async {
    setState(() => _isLoading = true);

    try {
      final response = await SuriotaGateway.instance.writeRegister(
        deviceId: widget.deviceId,
        registerId: widget.registerId,
        value: newValue ? 1.0 : 0.0,
      );

      if (response['status'] == 'ok') {
        setState(() => _value = newValue);
      } else {
        // Revert on failure
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(response['message'] ?? 'Toggle failed'),
            backgroundColor: Colors.red,
          ),
        );
      }
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text('Connection error: $e'),
          backgroundColor: Colors.red,
        ),
      );
    } finally {
      setState(() => _isLoading = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    return ListTile(
      title: Text(widget.registerName),
      trailing: _isLoading
          ? const SizedBox(
              width: 24,
              height: 24,
              child: CircularProgressIndicator(strokeWidth: 2),
            )
          : Switch(
              value: _value,
              onChanged: _toggleCoil,
            ),
    );
  }
}
```

---

## Writable Register Types

| Read FC | Data Type                | Writable | Write FC Used |
| ------- | ------------------------ | -------- | ------------- |
| FC1     | BOOL (Coil)              | Yes      | FC5           |
| FC2     | BOOL (Discrete Input)    | No       | -             |
| FC3     | INT16, UINT16            | Yes      | FC6           |
| FC3     | INT32_*, UINT32_*, FLOAT32_* | Yes  | FC16          |
| FC3     | INT64_*, UINT64_*, DOUBLE64_* | Yes  | FC16          |
| FC4     | Any (Input Register)     | No       | -             |

---

## UI Design Recommendations

### 1. Identify Writable Registers

When listing registers, check the `writable` field:

```dart
Widget buildRegisterList(List<Map<String, dynamic>> registers) {
  return ListView.builder(
    itemCount: registers.length,
    itemBuilder: (context, index) {
      final reg = registers[index];
      final isWritable = reg['writable'] == true;

      if (isWritable) {
        // Show input field with send button
        return RegisterWriteWidget(
          deviceId: reg['device_id'],
          registerId: reg['register_id'],
          registerName: reg['register_name'],
          minValue: reg['min_value']?.toDouble(),
          maxValue: reg['max_value']?.toDouble(),
          unit: reg['unit'] ?? '',
        );
      } else {
        // Read-only display
        return RegisterReadWidget(
          registerName: reg['register_name'],
          value: reg['current_value'],
          unit: reg['unit'] ?? '',
        );
      }
    },
  );
}
```

### 2. Visual Indicators

- Show a pencil/edit icon for writable registers
- Display min/max range as hint text
- Use different colors for read-only vs writable registers
- Show loading spinner during write operation

### 3. Streaming Mode Integration

In streaming mode, display both current value and write input:

```
+----------------------------------+
| Temperature Setpoint      [25.5] |
| Current: 24.8°C                  |
| [___________] [SEND]  (0-50)     |
+----------------------------------+
```

---

## Best Practices

1. **Validate on Client Side** - Check min/max before sending to reduce round trips
2. **Show Loading State** - Disable input during write operation
3. **Handle Errors Gracefully** - Display meaningful error messages
4. **Confirm Destructive Actions** - Add confirmation for critical writes
5. **Refresh After Write** - Update displayed value after successful write

---

## Related Documentation

- [API.md](API.md) - Full API reference
- [MOBILE_DECIMALS_FEATURE.md](MOBILE_DECIMALS_FEATURE.md) - Decimal precision feature
- [BLE_DEVICE_CONTROL.md](BLE_DEVICE_CONTROL.md) - Device enable/disable

---

**Document Version:** 1.0 | **Last Updated:** January 8, 2026 | **Developer:** Kemal

**SURIOTA R&D Team** | support@suriota.com
