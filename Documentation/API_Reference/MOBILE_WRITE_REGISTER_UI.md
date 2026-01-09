# Mobile Apps - Write Register Feature & UI Guide

**Version:** 1.0.8 | **Date:** January 8, 2026 | **Author:** SURIOTA R&D Team

---

## Table of Contents

1. [Feature Overview](#feature-overview)
2. [BLE Command Reference](#ble-command-reference)
3. [UI Design Recommendations](#ui-design-recommendations)
4. [UI Mockups](#ui-mockups)
5. [Flutter Implementation](#flutter-implementation)
6. [UX Best Practices](#ux-best-practices)
7. [Error Handling](#error-handling)

---

## Feature Overview

### What's New in v1.0.8

Users can now **write values** to Modbus registers, not just read them. This enables:

- Setting temperature/pressure setpoints
- Controlling relay outputs (ON/OFF)
- Adjusting PID parameters
- Setting alarm thresholds
- Motor speed control

### Key Concepts

| Concept | Description |
|---------|-------------|
| **Writable Register** | Register with `writable: true` in config |
| **Reverse Calibration** | User value converted to raw: `raw = (value - offset) / scale` |
| **Min/Max Validation** | Optional bounds checking before write |
| **Function Codes** | FC5 (Coil), FC6 (Single Reg), FC16 (Multi Reg) |

### Supported Register Types

| Register Type | Can Write? | Write FC |
|---------------|------------|----------|
| Coil (FC1) | ✅ Yes | FC5 |
| Discrete Input (FC2) | ❌ No | - |
| Holding Register (FC3) | ✅ Yes | FC6/FC16 |
| Input Register (FC4) | ❌ No | - |

---

## BLE Command Reference

### 1. Check if Register is Writable

Read register details to check `writable` flag:

```json
{
  "op": "read",
  "type": "register",
  "device_id": "D7A3F2"
}
```

**Response includes:**
```json
{
  "registers": [
    {
      "register_id": "R3C8D1",
      "register_name": "setpoint_temp",
      "writable": true,
      "min_value": 0.0,
      "max_value": 100.0,
      "scale": 0.1,
      "offset": 0.0,
      "unit": "°C"
    }
  ]
}
```

### 2. Enable Writable on Register

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

### 3. Write Value to Register

```json
{
  "op": "write",
  "type": "register",
  "device_id": "D7A3F2",
  "register_id": "R3C8D1",
  "value": 25.5
}
```

**Success Response:**
```json
{
  "status": "ok",
  "device_id": "D7A3F2",
  "register_id": "R3C8D1",
  "value_written": 25.5,
  "raw_value": 255,
  "response_time_ms": 45
}
```

**Error Response:**
```json
{
  "status": "error",
  "error_code": 316,
  "error": "Value 150.0 out of range [0.0 - 100.0]"
}
```

---

## UI Design Recommendations

### Design Principles

1. **Clear Visual Distinction** - Writable registers should look different from read-only
2. **Inline Editing** - Allow editing directly in the register list
3. **Immediate Feedback** - Show loading state and result instantly
4. **Validation First** - Validate on client before sending to gateway
5. **Confirmation for Critical** - Add confirmation dialog for critical values

### Register Card States

```
┌─────────────────────────────────────────────────────────┐
│  READ-ONLY REGISTER                                     │
├─────────────────────────────────────────────────────────┤
│  📊 Temperature Sensor                                  │
│  Current Value: 24.5 °C                                 │
│  [No edit controls - display only]                      │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│  WRITABLE REGISTER (Collapsed)                          │
├─────────────────────────────────────────────────────────┤
│  ✏️ Temperature Setpoint                     [EDIT]     │
│  Current Value: 25.5 °C                                 │
│  Range: 0 - 100 °C                                      │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│  WRITABLE REGISTER (Expanded/Editing)                   │
├─────────────────────────────────────────────────────────┤
│  ✏️ Temperature Setpoint                                │
│  Current Value: 25.5 °C                                 │
│                                                         │
│  ┌─────────────────────────────┐                        │
│  │  [    28.0    ]  °C        │  ← Text Input           │
│  └─────────────────────────────┘                        │
│  Range: 0.0 - 100.0                                     │
│                                                         │
│  [ CANCEL ]                    [ SEND ▶ ]               │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│  WRITABLE REGISTER (Loading)                            │
├─────────────────────────────────────────────────────────┤
│  ✏️ Temperature Setpoint                                │
│  Writing: 28.0 °C...                                    │
│                                                         │
│  ████████████░░░░░░░░  Sending...                       │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│  COIL/BOOLEAN REGISTER                                  │
├─────────────────────────────────────────────────────────┤
│  🔘 Relay Output 1                         [  ON  🔵 ]  │
│  Status: Active                            ← Toggle     │
└─────────────────────────────────────────────────────────┘
```

---

## UI Mockups

### Mockup 1: Register List with Write Support

```
┌──────────────────────────────────────────────────────────┐
│  ← Device: Power Meter PM-01                        ⚙️   │
├──────────────────────────────────────────────────────────┤
│                                                          │
│  📖 READ-ONLY REGISTERS                                  │
│  ┌────────────────────────────────────────────────────┐  │
│  │  Voltage L1                              230.5 V   │  │
│  ├────────────────────────────────────────────────────┤  │
│  │  Current L1                               15.2 A   │  │
│  ├────────────────────────────────────────────────────┤  │
│  │  Power Factor                              0.95    │  │
│  └────────────────────────────────────────────────────┘  │
│                                                          │
│  ✏️ WRITABLE REGISTERS                                   │
│  ┌────────────────────────────────────────────────────┐  │
│  │  ⚡ Over-voltage Threshold        [EDIT]           │  │
│  │     Current: 250.0 V  │  Range: 200-300 V          │  │
│  ├────────────────────────────────────────────────────┤  │
│  │  ⚡ Under-voltage Threshold       [EDIT]           │  │
│  │     Current: 180.0 V  │  Range: 150-220 V          │  │
│  ├────────────────────────────────────────────────────┤  │
│  │  🔘 Alarm Enable                     [ ON 🔵 ]     │  │
│  │     Status: Enabled                                │  │
│  └────────────────────────────────────────────────────┘  │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

### Mockup 2: Write Value Dialog/Bottom Sheet

```
┌──────────────────────────────────────────────────────────┐
│                                                          │
│                                                          │
│  (Background: Register List - dimmed)                    │
│                                                          │
│                                                          │
├──────────────────────────────────────────────────────────┤
│  ╭────────────────────────────────────────────────────╮  │
│  │                                                    │  │
│  │   ✏️ Edit: Over-voltage Threshold                  │  │
│  │                                                    │  │
│  │   Current Value: 250.0 V                           │  │
│  │                                                    │  │
│  │   ┌──────────────────────────────────────────┐     │  │
│  │   │                                          │     │  │
│  │   │           [  255.0  ]                    │     │  │
│  │   │                                          │     │  │
│  │   └──────────────────────────────────────────┘     │  │
│  │                                                    │  │
│  │   Valid range: 200.0 - 300.0 V                     │  │
│  │                                                    │  │
│  │   ⚠️ This will change the alarm threshold          │  │
│  │                                                    │  │
│  │   ┌──────────────┐     ┌──────────────────┐        │  │
│  │   │   CANCEL     │     │   WRITE VALUE ▶  │        │  │
│  │   └──────────────┘     └──────────────────┘        │  │
│  │                                                    │  │
│  ╰────────────────────────────────────────────────────╯  │
└──────────────────────────────────────────────────────────┘
```

### Mockup 3: Slider for Bounded Values

```
┌──────────────────────────────────────────────────────────┐
│                                                          │
│   ✏️ Temperature Setpoint                                │
│                                                          │
│   ┌──────────────────────────────────────────────────┐   │
│   │                    25.5 °C                       │   │
│   │                                                  │   │
│   │  0 ├────────────●──────────────────────────┤ 100 │   │
│   │    ◄───────────────────────────────────────►     │   │
│   │                                                  │   │
│   │  ┌────────────────────┐                          │   │
│   │  │   [  25.5  ]  °C   │  ← Manual input option   │   │
│   │  └────────────────────┘                          │   │
│   └──────────────────────────────────────────────────┘   │
│                                                          │
│   Quick Set: [ 20 ] [ 25 ] [ 30 ] [ 35 ] [ 40 ]          │
│                                                          │
│   ┌────────────────────────────────────────────────┐     │
│   │              APPLY CHANGES  ▶                  │     │
│   └────────────────────────────────────────────────┘     │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

### Mockup 4: Coil/Toggle Control

```
┌──────────────────────────────────────────────────────────┐
│                                                          │
│   🔘 DIGITAL OUTPUTS                                     │
│                                                          │
│   ┌────────────────────────────────────────────────────┐ │
│   │                                                    │ │
│   │   Relay 1 - Pump Motor          ┌─────────────┐    │ │
│   │   Status: Running               │  ████  OFF  │    │ │
│   │                                 │  ON   ░░░░  │    │ │
│   │                                 └─────────────┘    │ │
│   │                                                    │ │
│   ├────────────────────────────────────────────────────┤ │
│   │                                                    │ │
│   │   Relay 2 - Heater              ┌─────────────┐    │ │
│   │   Status: Stopped               │  ░░░░  OFF  │    │ │
│   │                                 │  ON   ████  │    │ │
│   │                                 └─────────────┘    │ │
│   │                                                    │ │
│   ├────────────────────────────────────────────────────┤ │
│   │                                                    │ │
│   │   Relay 3 - Alarm               ┌─────────────┐    │ │
│   │   Status: Running               │  ████  OFF  │    │ │
│   │                                 │  ON   ░░░░  │    │ │
│   │                                 └─────────────┘    │ │
│   │                                                    │ │
│   └────────────────────────────────────────────────────┘ │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

### Mockup 5: Write History/Log

```
┌──────────────────────────────────────────────────────────┐
│  ← Write History                                    🔄   │
├──────────────────────────────────────────────────────────┤
│                                                          │
│  TODAY                                                   │
│  ┌────────────────────────────────────────────────────┐  │
│  │  ✅ 14:32:15  Temperature Setpoint                 │  │
│  │     Value: 25.5 °C → 28.0 °C                       │  │
│  │     Response: 45ms                                 │  │
│  ├────────────────────────────────────────────────────┤  │
│  │  ✅ 14:30:22  Relay 1                              │  │
│  │     Value: OFF → ON                                │  │
│  │     Response: 32ms                                 │  │
│  ├────────────────────────────────────────────────────┤  │
│  │  ❌ 14:28:05  Pressure Threshold                   │  │
│  │     Value: 150.0 bar (REJECTED)                    │  │
│  │     Error: Out of range [0-100]                    │  │
│  └────────────────────────────────────────────────────┘  │
│                                                          │
│  YESTERDAY                                               │
│  ┌────────────────────────────────────────────────────┐  │
│  │  ✅ 16:45:33  Alarm Enable                         │  │
│  │     Value: ON → OFF                                │  │
│  └────────────────────────────────────────────────────┘  │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

---

## Flutter Implementation

### 1. Register Model with Write Support

```dart
class ModbusRegister {
  final String registerId;
  final String registerName;
  final String deviceId;
  final int address;
  final int functionCode;
  final String dataType;
  final double scale;
  final double offset;
  final int decimals;
  final String unit;
  final bool writable;
  final double? minValue;
  final double? maxValue;

  double? currentValue;
  DateTime? lastUpdated;

  ModbusRegister({
    required this.registerId,
    required this.registerName,
    required this.deviceId,
    required this.address,
    required this.functionCode,
    required this.dataType,
    this.scale = 1.0,
    this.offset = 0.0,
    this.decimals = -1,
    this.unit = '',
    this.writable = false,
    this.minValue,
    this.maxValue,
    this.currentValue,
    this.lastUpdated,
  });

  factory ModbusRegister.fromJson(Map<String, dynamic> json) {
    return ModbusRegister(
      registerId: json['register_id'] ?? '',
      registerName: json['register_name'] ?? '',
      deviceId: json['device_id'] ?? '',
      address: json['address'] ?? 0,
      functionCode: json['function_code'] ?? 3,
      dataType: json['data_type'] ?? 'INT16',
      scale: (json['scale'] ?? 1.0).toDouble(),
      offset: (json['offset'] ?? 0.0).toDouble(),
      decimals: json['decimals'] ?? -1,
      unit: json['unit'] ?? '',
      writable: json['writable'] ?? false,
      minValue: json['min_value']?.toDouble(),
      maxValue: json['max_value']?.toDouble(),
    );
  }

  bool get isCoil => functionCode == 1;
  bool get isHolding => functionCode == 3;
  bool get canWrite => writable && (isCoil || isHolding);

  String get rangeText {
    if (minValue != null && maxValue != null) {
      return '${minValue!.toStringAsFixed(1)} - ${maxValue!.toStringAsFixed(1)}';
    }
    return 'No limit';
  }

  /// Validate value before writing
  String? validateValue(double value) {
    if (!writable) {
      return 'Register is read-only';
    }
    if (minValue != null && value < minValue!) {
      return 'Value must be >= $minValue';
    }
    if (maxValue != null && value > maxValue!) {
      return 'Value must be <= $maxValue';
    }
    return null; // Valid
  }
}
```

### 2. Write Register Service

```dart
class RegisterWriteService {
  final BleGatewayService _bleService;

  RegisterWriteService(this._bleService);

  /// Write a value to a register
  Future<WriteResult> writeRegister({
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

    try {
      final response = await _bleService.sendCommand(command);

      if (response['status'] == 'ok') {
        return WriteResult.success(
          writtenValue: response['value_written']?.toDouble() ?? value,
          rawValue: response['raw_value']?.toDouble(),
          responseTimeMs: response['response_time_ms'] ?? 0,
        );
      } else {
        return WriteResult.failure(
          errorCode: response['error_code'] ?? 0,
          errorMessage: response['error'] ?? 'Unknown error',
        );
      }
    } catch (e) {
      return WriteResult.failure(
        errorCode: -1,
        errorMessage: 'Connection error: $e',
      );
    }
  }

  /// Enable writable flag on a register
  Future<bool> enableWritable({
    required String deviceId,
    required String registerId,
    double? minValue,
    double? maxValue,
  }) async {
    final config = <String, dynamic>{'writable': true};
    if (minValue != null) config['min_value'] = minValue;
    if (maxValue != null) config['max_value'] = maxValue;

    final command = {
      'op': 'update',
      'type': 'register',
      'device_id': deviceId,
      'register_id': registerId,
      'config': config,
    };

    final response = await _bleService.sendCommand(command);
    return response['status'] == 'ok';
  }
}

class WriteResult {
  final bool success;
  final double? writtenValue;
  final double? rawValue;
  final int? responseTimeMs;
  final int? errorCode;
  final String? errorMessage;

  WriteResult._({
    required this.success,
    this.writtenValue,
    this.rawValue,
    this.responseTimeMs,
    this.errorCode,
    this.errorMessage,
  });

  factory WriteResult.success({
    required double writtenValue,
    double? rawValue,
    required int responseTimeMs,
  }) {
    return WriteResult._(
      success: true,
      writtenValue: writtenValue,
      rawValue: rawValue,
      responseTimeMs: responseTimeMs,
    );
  }

  factory WriteResult.failure({
    required int errorCode,
    required String errorMessage,
  }) {
    return WriteResult._(
      success: false,
      errorCode: errorCode,
      errorMessage: errorMessage,
    );
  }
}
```

### 3. Writable Register Card Widget

```dart
class WritableRegisterCard extends StatefulWidget {
  final ModbusRegister register;
  final Function(double) onWrite;
  final VoidCallback? onRefresh;

  const WritableRegisterCard({
    Key? key,
    required this.register,
    required this.onWrite,
    this.onRefresh,
  }) : super(key: key);

  @override
  State<WritableRegisterCard> createState() => _WritableRegisterCardState();
}

class _WritableRegisterCardState extends State<WritableRegisterCard> {
  bool _isEditing = false;
  bool _isLoading = false;
  final _controller = TextEditingController();
  String? _errorText;

  @override
  void initState() {
    super.initState();
    _controller.text = widget.register.currentValue?.toString() ?? '';
  }

  void _startEditing() {
    setState(() {
      _isEditing = true;
      _errorText = null;
      _controller.text = widget.register.currentValue?.toStringAsFixed(
        widget.register.decimals >= 0 ? widget.register.decimals : 2,
      ) ?? '';
    });
  }

  void _cancelEditing() {
    setState(() {
      _isEditing = false;
      _errorText = null;
    });
  }

  Future<void> _submitValue() async {
    final valueStr = _controller.text.trim();
    final value = double.tryParse(valueStr);

    if (value == null) {
      setState(() => _errorText = 'Invalid number');
      return;
    }

    // Client-side validation
    final validationError = widget.register.validateValue(value);
    if (validationError != null) {
      setState(() => _errorText = validationError);
      return;
    }

    setState(() {
      _isLoading = true;
      _errorText = null;
    });

    try {
      await widget.onWrite(value);
      setState(() {
        _isEditing = false;
        _isLoading = false;
      });
    } catch (e) {
      setState(() {
        _errorText = e.toString();
        _isLoading = false;
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    final reg = widget.register;

    return Card(
      elevation: 2,
      margin: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            // Header row
            Row(
              children: [
                Icon(
                  reg.writable ? Icons.edit : Icons.visibility,
                  color: reg.writable ? Colors.blue : Colors.grey,
                  size: 20,
                ),
                const SizedBox(width: 8),
                Expanded(
                  child: Text(
                    reg.registerName,
                    style: const TextStyle(
                      fontWeight: FontWeight.bold,
                      fontSize: 16,
                    ),
                  ),
                ),
                if (reg.writable && !_isEditing)
                  TextButton.icon(
                    onPressed: _startEditing,
                    icon: const Icon(Icons.edit, size: 16),
                    label: const Text('EDIT'),
                  ),
              ],
            ),

            const SizedBox(height: 8),

            // Current value
            Row(
              children: [
                Text(
                  'Current: ',
                  style: TextStyle(color: Colors.grey[600]),
                ),
                Text(
                  '${reg.currentValue?.toStringAsFixed(reg.decimals >= 0 ? reg.decimals : 2) ?? 'N/A'} ${reg.unit}',
                  style: const TextStyle(
                    fontSize: 18,
                    fontWeight: FontWeight.w500,
                  ),
                ),
              ],
            ),

            // Range info
            if (reg.minValue != null || reg.maxValue != null)
              Padding(
                padding: const EdgeInsets.only(top: 4),
                child: Text(
                  'Range: ${reg.rangeText} ${reg.unit}',
                  style: TextStyle(
                    color: Colors.grey[500],
                    fontSize: 12,
                  ),
                ),
              ),

            // Edit mode
            if (_isEditing) ...[
              const SizedBox(height: 16),
              TextField(
                controller: _controller,
                keyboardType: const TextInputType.numberWithOptions(
                  decimal: true,
                  signed: true,
                ),
                decoration: InputDecoration(
                  labelText: 'New Value',
                  suffixText: reg.unit,
                  errorText: _errorText,
                  border: const OutlineInputBorder(),
                  hintText: reg.rangeText,
                ),
                enabled: !_isLoading,
                autofocus: true,
                onSubmitted: (_) => _submitValue(),
              ),
              const SizedBox(height: 16),
              Row(
                mainAxisAlignment: MainAxisAlignment.end,
                children: [
                  TextButton(
                    onPressed: _isLoading ? null : _cancelEditing,
                    child: const Text('CANCEL'),
                  ),
                  const SizedBox(width: 8),
                  ElevatedButton.icon(
                    onPressed: _isLoading ? null : _submitValue,
                    icon: _isLoading
                        ? const SizedBox(
                            width: 16,
                            height: 16,
                            child: CircularProgressIndicator(strokeWidth: 2),
                          )
                        : const Icon(Icons.send, size: 16),
                    label: Text(_isLoading ? 'SENDING...' : 'SEND'),
                  ),
                ],
              ),
            ],
          ],
        ),
      ),
    );
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }
}
```

### 4. Coil Toggle Widget

```dart
class CoilToggleWidget extends StatefulWidget {
  final ModbusRegister register;
  final Function(bool) onToggle;

  const CoilToggleWidget({
    Key? key,
    required this.register,
    required this.onToggle,
  }) : super(key: key);

  @override
  State<CoilToggleWidget> createState() => _CoilToggleWidgetState();
}

class _CoilToggleWidgetState extends State<CoilToggleWidget> {
  bool _isLoading = false;

  Future<void> _handleToggle(bool newValue) async {
    if (_isLoading) return;

    setState(() => _isLoading = true);

    try {
      await widget.onToggle(newValue);
    } finally {
      setState(() => _isLoading = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    final isOn = (widget.register.currentValue ?? 0) != 0;

    return Card(
      margin: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
      child: ListTile(
        leading: Icon(
          isOn ? Icons.toggle_on : Icons.toggle_off,
          color: isOn ? Colors.green : Colors.grey,
          size: 32,
        ),
        title: Text(widget.register.registerName),
        subtitle: Text(
          isOn ? 'Status: ON' : 'Status: OFF',
          style: TextStyle(
            color: isOn ? Colors.green : Colors.grey,
          ),
        ),
        trailing: _isLoading
            ? const SizedBox(
                width: 24,
                height: 24,
                child: CircularProgressIndicator(strokeWidth: 2),
              )
            : Switch(
                value: isOn,
                onChanged: widget.register.writable ? _handleToggle : null,
                activeColor: Colors.green,
              ),
      ),
    );
  }
}
```

### 5. Write Confirmation Dialog

```dart
class WriteConfirmationDialog extends StatelessWidget {
  final String registerName;
  final double currentValue;
  final double newValue;
  final String unit;
  final VoidCallback onConfirm;
  final VoidCallback onCancel;

  const WriteConfirmationDialog({
    Key? key,
    required this.registerName,
    required this.currentValue,
    required this.newValue,
    required this.unit,
    required this.onConfirm,
    required this.onCancel,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: Row(
        children: [
          const Icon(Icons.warning_amber, color: Colors.orange),
          const SizedBox(width: 8),
          const Text('Confirm Write'),
        ],
      ),
      content: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text('Register: $registerName'),
          const SizedBox(height: 16),
          Row(
            children: [
              Expanded(
                child: Column(
                  children: [
                    const Text('Current', style: TextStyle(color: Colors.grey)),
                    Text(
                      '$currentValue $unit',
                      style: const TextStyle(fontSize: 18),
                    ),
                  ],
                ),
              ),
              const Icon(Icons.arrow_forward, color: Colors.blue),
              Expanded(
                child: Column(
                  children: [
                    const Text('New', style: TextStyle(color: Colors.grey)),
                    Text(
                      '$newValue $unit',
                      style: const TextStyle(
                        fontSize: 18,
                        fontWeight: FontWeight.bold,
                        color: Colors.blue,
                      ),
                    ),
                  ],
                ),
              ),
            ],
          ),
          const SizedBox(height: 16),
          Container(
            padding: const EdgeInsets.all(12),
            decoration: BoxDecoration(
              color: Colors.orange.withOpacity(0.1),
              borderRadius: BorderRadius.circular(8),
            ),
            child: const Row(
              children: [
                Icon(Icons.info_outline, color: Colors.orange, size: 20),
                SizedBox(width: 8),
                Expanded(
                  child: Text(
                    'This will immediately send a command to the device.',
                    style: TextStyle(fontSize: 12),
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
      actions: [
        TextButton(
          onPressed: onCancel,
          child: const Text('CANCEL'),
        ),
        ElevatedButton(
          onPressed: onConfirm,
          child: const Text('WRITE VALUE'),
        ),
      ],
    );
  }
}
```

---

## UX Best Practices

### 1. Visual Hierarchy

| Element | Priority | Style |
|---------|----------|-------|
| Register Name | High | Bold, larger font |
| Current Value | High | Prominent, with unit |
| Edit Button | Medium | Outlined/Text button |
| Range Info | Low | Small, muted color |
| Input Field | High (when editing) | Full width, clear focus |

### 2. Loading States

```dart
enum WriteState {
  idle,        // Normal display
  editing,     // Input field visible
  validating,  // Client-side check
  sending,     // BLE command in progress
  success,     // Write completed (show briefly)
  error,       // Error occurred
}
```

### 3. Feedback Timing

| Action | Feedback |
|--------|----------|
| Tap Edit | Immediately show input field |
| Invalid Input | Show error under field |
| Submit | Show spinner, disable inputs |
| Success | Green checkmark, auto-collapse after 1.5s |
| Error | Red message, keep form open |

### 4. Accessibility

- Minimum touch target: 48x48 dp
- Clear contrast between read-only and writable
- Screen reader labels for all controls
- Keyboard navigation support

---

## Error Handling

### Error Codes Reference

| Code | Description | User Message | Action |
|------|-------------|--------------|--------|
| 315 | Not writable | "This register is read-only" | Show info, no retry |
| 316 | Out of range | "Value must be between X and Y" | Keep form open |
| 317 | Unsupported type | "Cannot write to this register type" | Show info |
| 318 | Write failed | "Device did not respond" | Offer retry |
| 301 | Timeout | "Connection timed out" | Check connection, retry |
| 305 | Device not found | "Device not available" | Refresh device list |

### Error Display Widget

```dart
class WriteErrorWidget extends StatelessWidget {
  final int errorCode;
  final String errorMessage;
  final VoidCallback? onRetry;
  final VoidCallback onDismiss;

  const WriteErrorWidget({
    Key? key,
    required this.errorCode,
    required this.errorMessage,
    this.onRetry,
    required this.onDismiss,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    final canRetry = errorCode == 318 || errorCode == 301;

    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: Colors.red.shade50,
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: Colors.red.shade200),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Icon(Icons.error_outline, color: Colors.red.shade700),
              const SizedBox(width: 8),
              Expanded(
                child: Text(
                  'Write Failed (Error $errorCode)',
                  style: TextStyle(
                    fontWeight: FontWeight.bold,
                    color: Colors.red.shade700,
                  ),
                ),
              ),
              IconButton(
                icon: const Icon(Icons.close, size: 18),
                onPressed: onDismiss,
              ),
            ],
          ),
          const SizedBox(height: 8),
          Text(errorMessage),
          if (canRetry) ...[
            const SizedBox(height: 12),
            ElevatedButton.icon(
              onPressed: onRetry,
              icon: const Icon(Icons.refresh, size: 18),
              label: const Text('RETRY'),
              style: ElevatedButton.styleFrom(
                backgroundColor: Colors.red,
              ),
            ),
          ],
        ],
      ),
    );
  }
}
```

---

## Summary

### Checklist for Mobile Implementation

- [ ] Update register model with `writable`, `min_value`, `max_value`
- [ ] Add write command to BLE service
- [ ] Create WritableRegisterCard widget
- [ ] Create CoilToggleWidget for boolean registers
- [ ] Add client-side validation
- [ ] Implement loading states
- [ ] Add error handling with retry
- [ ] Add write confirmation dialog (optional)
- [ ] Add write history/log (optional)
- [ ] Test with actual gateway v1.0.8+

### Quick Reference

```dart
// Check if register is writable
if (register.writable && (register.functionCode == 1 || register.functionCode == 3)) {
  // Show write controls
}

// Validate before write
final error = register.validateValue(newValue);
if (error != null) {
  showError(error);
  return;
}

// Send write command
final result = await writeService.writeRegister(
  deviceId: register.deviceId,
  registerId: register.registerId,
  value: newValue,
);

if (result.success) {
  showSuccess('Value written: ${result.writtenValue}');
} else {
  showError('Error ${result.errorCode}: ${result.errorMessage}');
}
```

---

**Document Version:** 1.0 | **Last Updated:** January 8, 2026

**SURIOTA R&D Team** | support@suriota.com
