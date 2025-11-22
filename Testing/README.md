# 🧪 Testing Documentation

**SRT-MGATE-1210 Gateway Firmware Testing Suite**

**Last Updated:** November 22, 2025 (v2.3.3)

---

## 📋 Overview

This directory contains comprehensive testing infrastructure for the SRT-MGATE-1210 firmware, including:
- **BLE Testing** - Backup/restore, device control, CRUD operations
- **Device Testing** - RTU/TCP device creation and validation
- **Modbus Simulators** - Software RTU/TCP slaves for testing
- **Server Config** - MQTT/HTTP configuration testing
- **Test Values** - Reference data for Modbus data types

---

## 🚀 Quick Start

### Prerequisites

**Python Environment:**
```bash
# Install Python 3.8+
python3 --version

# Install common dependencies
pip install bleak pyserial pymodbus
```

**Hardware Requirements:**
- ESP32-S3 with firmware v2.3.3+
- BLE-capable computer/smartphone
- (Optional) RS485-to-USB adapter for RTU testing
- (Optional) Network connection for TCP testing

---

## 📂 Directory Structure

```
Testing/
├── README.md (this file)                   # Testing documentation index
├── Modbus Test Values.md                   # Reference test values for data types
│
├── BLE_Testing/                            # BLE communication tests
│   ├── BLE_TESTING_README.md               # BLE testing overview
│   └── Backup_Restore_Test/                # Backup/restore functionality
│       ├── README_TESTING.md               # Test documentation
│       ├── QUICK_START.md                  # Quick start guide
│       ├── TESTING_CHECKLIST.md            # Test checklist
│       ├── FIX_RESTORE_BUG32.md            # BUG #32 fix documentation
│       ├── CHANGELOG_v1.1.0.md             # Version changelog
│       └── test_backup_restore.py          # Python test script
│
├── Device_Testing/                         # Device creation tests
│   ├── RTU_Create/                         # RTU device creation
│   │   ├── README.md                       # RTU testing guide
│   │   ├── TESTING_SUMMARY.md              # Test results summary
│   │   ├── create_device_*.py              # RTU creation scripts
│   │   └── requirements.txt                # Python dependencies
│   │
│   └── TCP_Create/                         # TCP device creation
│       ├── DOCUMENTATION.md                # TCP testing guide
│       ├── PAYLOAD_VALIDATION.md           # Payload validation details
│       ├── VERSION_COMPARISON.md           # Version comparison
│       ├── create_device_*.py              # TCP creation scripts
│       └── requirements.txt                # Python dependencies
│
├── Modbus_Simulators/                      # Software Modbus slaves
│   ├── RTU_Slave/                          # RTU slave simulator
│   │   ├── README.md                       # RTU simulator guide
│   │   ├── rtu_slave_simulator.py          # Python RTU slave
│   │   └── requirements.txt                # Dependencies
│   │
│   └── TCP_Slave/                          # TCP slave simulator
│       ├── DOCUMENTATION.md                # TCP simulator guide
│       ├── tcp_slave_simulator.py          # Python TCP slave
│       └── requirements.txt                # Dependencies
│
├── Server_Config/                          # Server configuration tests
│   ├── CONFIG_REFERENCE.md                 # Config reference
│   ├── README_59_REGISTERS.md              # Large config testing
│   └── test_*.py                           # Config test scripts
│
└── Old_Testing/                            # Archived test files
    └── MQTT_MODES_TEST_GUIDE.md            # Legacy MQTT testing
```

---

## 🔬 Testing Categories

### 1. BLE Testing

**Purpose:** Validate BLE communication, backup/restore, device control

**Key Tests:**
- ✅ BLE connection and MTU negotiation
- ✅ CRUD operations (Create, Read, Update, Delete)
- ✅ Backup configuration (up to 200KB payloads)
- ✅ Restore configuration with data integrity verification
- ✅ Factory reset functionality
- ✅ Device enable/disable control
- ✅ Large JSON payload handling (3420+ bytes)

**Quick Start:**
```bash
cd BLE_Testing/Backup_Restore_Test
python test_backup_restore.py

# Select test:
# 1. Create device with 5 registers
# 2. Create device with 50 registers
# 3. Backup configuration
# 4. Restore configuration
# 5. Backup-Restore-Compare cycle (recommended)
```

**Documentation:**
- [BLE Testing Overview](BLE_Testing/BLE_TESTING_README.md)
- [Backup/Restore Guide](BLE_Testing/Backup_Restore_Test/README_TESTING.md)
- [Quick Start](BLE_Testing/Backup_Restore_Test/QUICK_START.md)
- [BUG #32 Fix](BLE_Testing/Backup_Restore_Test/FIX_RESTORE_BUG32.md)

---

### 2. Device Testing

**Purpose:** Validate device creation (RTU/TCP) with various configurations

#### RTU Device Testing

**Scenarios:**
- Single device with 1-50 registers
- Multiple devices with different baud rates
- Different data types (FLOAT32, INT16, UINT32, etc.)
- Serial port switching (Port 1/2)

**Quick Start:**
```bash
cd Device_Testing/RTU_Create
pip install -r requirements.txt
python create_device_5_registers.py   # Simple test
python create_device_50_registers.py  # Stress test
```

**Documentation:**
- [RTU Testing Guide](Device_Testing/RTU_Create/README.md)
- [Testing Summary](Device_Testing/RTU_Create/TESTING_SUMMARY.md)

#### TCP Device Testing

**Scenarios:**
- Single TCP device with 1-50 registers
- Multiple TCP devices with different IP addresses
- Connection timeout handling
- Payload size validation

**Quick Start:**
```bash
cd Device_Testing/TCP_Create
pip install -r requirements.txt
python create_device_5_registers.py
```

**Documentation:**
- [TCP Testing Guide](Device_Testing/TCP_Create/DOCUMENTATION.md)
- [Payload Validation](Device_Testing/TCP_Create/PAYLOAD_VALIDATION.md)

---

### 3. Modbus Simulators

**Purpose:** Software Modbus slaves for testing without physical devices

#### RTU Slave Simulator

**Features:**
- Simulates Modbus RTU slave on serial port
- Configurable slave ID, baud rate, parity
- Supports function codes 1, 2, 3, 4, 15, 16
- Configurable register values

**Quick Start:**
```bash
cd Modbus_Simulators/RTU_Slave
pip install -r requirements.txt
python rtu_slave_simulator.py --port COM3 --slave-id 1 --baudrate 9600
```

**Documentation:**
- [RTU Simulator Guide](Modbus_Simulators/RTU_Slave/README.md)

#### TCP Slave Simulator

**Features:**
- Simulates Modbus TCP slave on network
- Configurable IP address and port
- Supports multiple client connections
- Register mapping configuration

**Quick Start:**
```bash
cd Modbus_Simulators/TCP_Slave
pip install -r requirements.txt
python tcp_slave_simulator.py --port 502
```

**Documentation:**
- [TCP Simulator Guide](Modbus_Simulators/TCP_Slave/DOCUMENTATION.md)

---

### 4. Server Configuration Testing

**Purpose:** Validate MQTT/HTTP server configurations

**Scenarios:**
- MQTT broker connection (local/cloud)
- HTTP endpoint testing (POST/PUT)
- Large payload publishing (59+ registers)
- Network failover testing

**Quick Start:**
```bash
cd Server_Config
python test_mqtt_config.py
```

**Documentation:**
- [Config Reference](Server_Config/CONFIG_REFERENCE.md)
- [Large Config Testing](Server_Config/README_59_REGISTERS.md)

---

## 📊 Test Coverage Matrix

### Feature Testing Status

| Feature | BLE Test | Device Test | Simulator | Status |
|---------|----------|-------------|-----------|--------|
| **Device CRUD** | ✅ | ✅ | ✅ | Complete |
| **Register CRUD** | ✅ | ✅ | ✅ | Complete |
| **Backup Config** | ✅ | - | - | Complete |
| **Restore Config** | ✅ | - | - | Complete |
| **Factory Reset** | ✅ | - | - | Complete |
| **Device Control** | ✅ | - | - | Complete |
| **RTU Polling** | - | ✅ | ✅ | Complete |
| **TCP Polling** | - | ✅ | ✅ | Complete |
| **MQTT Publish** | - | ✅ | - | Complete |
| **HTTP Publish** | - | ✅ | - | Complete |
| **Large Payloads** | ✅ | ✅ | - | Complete (v2.3.3) |
| **Data Types** | - | ✅ | ✅ | 40+ types |

### Bug Fix Validation

| Bug # | Description | Test Script | Status |
|-------|-------------|-------------|--------|
| **#32** | Restore config failure (large JSON) | `test_backup_restore.py` | ✅ FIXED (v2.3.3) |
| **MQTT** | Partial publish (incomplete data) | Device testing scripts | ✅ FIXED (v2.3.2) |
| **Cache** | Memory leak after deletion | Device creation/deletion | ✅ FIXED (v2.3.1) |

---

## 🎯 Recommended Test Sequence

### For New Firmware Build:

1. **BLE Connection Test** (2 min)
   ```bash
   cd BLE_Testing/Backup_Restore_Test
   python test_backup_restore.py
   # Select: Create device with 5 registers
   ```

2. **Backup/Restore Cycle** (5 min)
   ```bash
   # Same script
   # Select: Backup-Restore-Compare cycle
   ```

3. **Device Polling Test** (10 min)
   ```bash
   cd Device_Testing/RTU_Create
   python create_device_50_registers.py
   # Monitor serial output for polling success
   ```

4. **MQTT Publish Test** (5 min)
   ```bash
   # Use MQTT subscriber (mosquitto_sub or MQTT Explorer)
   mosquitto_sub -h localhost -t "suriota/gateway/#" -v
   # Verify all 50 registers published
   ```

5. **Stress Test** (30 min)
   ```bash
   # Create 5 devices with 10 registers each
   # Run for 30 minutes, monitor memory usage
   ```

### Total Testing Time: ~50 minutes

---

## 🐛 Troubleshooting

### Common Issues

#### BLE Connection Fails

**Symptom:** Python script can't find gateway
**Solution:**
1. Check firmware PRODUCTION_MODE (should be 0 for dev)
2. Enable BLE via button (if PRODUCTION_MODE=1)
3. Check BLE name in script matches gateway
4. Restart gateway and retry

#### Modbus Simulator Not Responding

**Symptom:** Gateway shows "Device timeout"
**Solution:**
1. Check serial port/IP address correct
2. Verify simulator running (`ps aux | grep python`)
3. Check baud rate matches (RTU)
4. Check firewall allows port 502 (TCP)

#### Large Payload Test Fails

**Symptom:** Restore fails with "Missing 'config' object"
**Solution:**
1. Upgrade firmware to v2.3.3+ (BUG #32 fix)
2. Check PSRAM available (>1MB free)
3. Use `test_backup_restore.py` option 4 (Backup-Restore-Compare)

---

## 📝 Test Data Reference

### Modbus Test Values

See [Modbus Test Values.md](Modbus Test Values.md) for comprehensive test data including:
- INT16, UINT16, INT32, UINT32 test values
- FLOAT32, FLOAT64 precision testing
- Endianness variants (BE, LE, ABCD, CDAB, etc.)
- Edge cases (0, MAX, MIN, -1, overflow)

**Example:**
```
INT16:
  Set Value: 32767
  Modbus Registers: 0x7FFF
  Expected Read Value: 32767

FLOAT32_ABCD:
  Set Value: 123.456
  Modbus Registers: 0x42F6, 0xE979
  Expected Read Value: 123.456
```

---

## 🔧 Development Testing

### Adding New Tests

1. **Create test directory:**
   ```bash
   mkdir -p Testing/NewFeature_Testing
   ```

2. **Add Python script:**
   ```python
   # test_new_feature.py
   import bleak
   # ... test code ...
   ```

3. **Create documentation:**
   ```bash
   touch Testing/NewFeature_Testing/README.md
   ```

4. **Update this index** (Testing/README.md)

### Test Script Template

```python
#!/usr/bin/env python3
"""
Test: [Feature Name]
Date: [YYYY-MM-DD]
Firmware: v2.3.3+
"""

import asyncio
from bleak import BleakClient, BleakScanner

# BLE UUIDs
SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
TX_CHAR_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  # Write
RX_CHAR_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"  # Notify

async def test_feature():
    """Test specific feature"""
    # Connect to gateway
    device = await BleakScanner.find_device_by_name("SURIOTA GW")

    async with BleakClient(device) as client:
        # Send test command
        command = '{"op":"read","type":"devices"}'
        await client.write_gatt_char(TX_CHAR_UUID, command.encode())

        # Read response
        response = await client.read_gatt_char(RX_CHAR_UUID)
        print(f"Response: {response.decode()}")

if __name__ == "__main__":
    asyncio.run(test_feature())
```

---

## 📚 Related Documentation

### Firmware Documentation
- [Quick Start Guide](../Documentation/QUICKSTART.md)
- [API Reference](../Documentation/API_Reference/API.md)
- [Troubleshooting Guide](../Documentation/Technical_Guides/TROUBLESHOOTING.md)
- [Version History](../Documentation/Changelog/VERSION_HISTORY.md)

### Specific API Docs
- [BLE Backup/Restore API](../Documentation/API_Reference/BLE_BACKUP_RESTORE.md)
- [BLE Factory Reset API](../Documentation/API_Reference/BLE_FACTORY_RESET.md)
- [BLE Device Control API](../Documentation/API_Reference/BLE_DEVICE_CONTROL.md)
- [Modbus Data Types](../Documentation/Technical_Guides/MODBUS_DATATYPES.md)

---

## 🤝 Contributing Tests

When contributing new tests:

1. **Follow naming convention:**
   - Test scripts: `test_feature_name.py`
   - Documentation: `README.md` or `FEATURE_NAME_TESTING.md`
   - Requirements: `requirements.txt` with pinned versions

2. **Include documentation:**
   - Purpose and scope
   - Prerequisites
   - Step-by-step instructions
   - Expected results
   - Troubleshooting tips

3. **Test on clean environment:**
   - Fresh Python virtual environment
   - Factory reset gateway before testing
   - Document all dependencies

4. **Update this index** with new test category

---

## 📞 Support

### Issues & Questions

1. **Check existing documentation** in this directory
2. **Review troubleshooting** section above
3. **Check firmware logs** (Serial Monitor @ 115200 baud)
4. **Contact:** Kemal (Firmware Developer)

### Reporting Test Failures

When reporting test failures, include:
- Firmware version (`[INFO] Current version: X.X.X` in serial log)
- Python version (`python3 --version`)
- Test script name and version
- Complete error output
- Serial monitor logs (if applicable)
- Hardware setup (RTU/TCP, network config)

---

**Testing Documentation Version:** 1.0
**Last Updated:** November 22, 2025
**Firmware Version:** 2.3.3
**Maintainer:** Kemal

[← Back to Main README](../README.md) | [↑ Top](#-testing-documentation)
