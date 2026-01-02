# 📂 Modbus TCP Slave Simulator

**Dedicated simulator for testing SRT-MGATE-1210 Gateway**

---

## 📌 Overview

Folder khusus untuk Modbus TCP slave simulator yang mensimulasikan perangkat
Modbus nyata. Simulator ini **match** dengan konfigurasi yang dibuat di
`Device_Testing/create_device_5_registers.py`.

### Purpose

- ✅ Test gateway WITHOUT physical Modbus devices
- ✅ Simulate 5 Input Registers dengan data realistis
- ✅ Auto-update nilai register setiap 5 detik
- ✅ Allow gateway untuk connect dan read data

---

## 📂 Folder Structure

```
Modbus_Slave_Simulator/
├── modbus_slave_5_registers.py    # Main simulator program
├── requirements.txt               # Python dependencies
├── 00_START_HERE.txt              # Quick overview
├── QUICK_START.txt                # Quick reference
└── DOCUMENTATION.md               # This file
```

---

## 🚀 Quick Start

### 1. Install Dependencies

```bash
cd "C:\Users\Administrator\Pictures\SRT-MGATE-1210-Firmware\Modbus_Slave_Simulator"
pip install -r requirements.txt
```

Or manually:

```bash
pip install pymodbus
```

### 2. Run Simulator

```bash
python modbus_slave_5_registers.py
```

Press Enter when prompted to start the server.

### 3. Test with Gateway

In another terminal:

```bash
cd ../Device_Testing
python create_device_5_registers.py
```

Gateway will create device and registers, then connect to simulator!

---

## 📊 Configuration

### Slave Configuration

```
IP Address:      192.168.1.8
Port:            502
Slave ID:        1
Protocol:        Modbus TCP
Function Code:   4 (Read Input Registers)
Data Type:       INT16 (0-65535)
```

### Register Mapping (5 Registers)

| Address | Name        | Unit | Range    | Initial | Description         |
| ------- | ----------- | ---- | -------- | ------- | ------------------- |
| 0       | Temperature | °C   | 20-35    | 25      | Temperature sensor  |
| 1       | Humidity    | %    | 40-80    | 60      | Humidity sensor     |
| 2       | Pressure    | Pa   | 900-1100 | 1000    | Pressure sensor     |
| 3       | Voltage     | V    | 220-240  | 230     | Voltage measurement |
| 4       | Current     | A    | 1-10     | 5       | Current measurement |

### Auto-Update Behavior

- **Enabled:** Yes
- **Interval:** 5 seconds
- **Temperature:** Changes ±1°C slowly
- **Humidity:** Changes ±2% moderately
- **Pressure:** Changes ±5 Pa
- **Voltage:** Very stable, changes ±1V
- **Current:** Changes ±1A

---

## 🔧 Gateway Configuration

Use these settings in `Device_Testing/create_device_5_registers.py`:

### Device Config

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
    "refresh_rate_ms": 1000,
    "ip": "192.168.1.8",
    "port": 502
  }
}
```

### Register Config (for each register)

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

---

## 📖 Usage Guide

### Complete Testing Workflow

```
┌─────────────────────────────────────────────────────────────────┐
│                                                                 │
│  1. START SIMULATOR (This folder)                              │
│     └─ python modbus_slave_5_registers.py                      │
│                                                                 │
│  2. CREATE GATEWAY DEVICE (Device_Testing folder)              │
│     └─ python create_device_5_registers.py                     │
│                                                                 │
│  3. VERIFY CONNECTION                                           │
│     ├─ Simulator logs show connection                          │
│     └─ Gateway successfully reads registers                    │
│                                                                 │
│  4. MONITOR DATA FLOW                                           │
│     ├─ Simulator updates every 5s                              │
│     └─ Gateway polls every 1s                                  │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Expected Simulator Output

```
======================================================================
  MODBUS TCP SLAVE SIMULATOR
  SRT-MGATE-1210 Testing - 5 Input Registers
======================================================================
  Configuration:
  - IP Address:     192.168.1.8
  - Port:           502
  - Slave ID:       1
  - Registers:      5 Input Registers (Function Code 4)
  - Auto Update:    Enabled
  - Update Interval: 5.0s
======================================================================

[INFO] Server listening on 192.168.1.8:502
[INFO] Ready for connections from SRT-MGATE-1210 Gateway
[INFO] Auto-update enabled - register values will change every 5.0s

──────────────────────────────────────────────────────────────────
  Update #0001 - Register Values:
──────────────────────────────────────────────────────────────────
  [0] Temperature :    25 °C
  [1] Humidity    :    60 %
  [2] Pressure    :  1000 Pa
  [3] Voltage     :   230 V
  [4] Current     :     5 A
──────────────────────────────────────────────────────────────────
```

---

## 🐛 Troubleshooting

### Issue: `Address already in use`

**Cause:** Port 502 is already being used by another program

**Solutions:**

1. Check if another Modbus simulator is running
2. Close other programs using port 502
3. Change `SERVER_PORT` in the script to 5020
4. Update gateway device config to match new port

### Issue: `pymodbus not found`

**Solution:**

```bash
pip install pymodbus
```

Or:

```bash
pip3 install pymodbus
```

### Issue: Gateway cannot connect to simulator

**Checklist:**

- ✅ Simulator is running (check console output)
- ✅ IP address is 192.168.1.8 (or configured correctly)
- ✅ Port 502 is not blocked by firewall
- ✅ Computer and gateway are on same network
- ✅ Try ping from gateway: `ping 192.168.1.8`

**Windows Firewall:**

```
1. Windows Defender Firewall
2. Allow an app through firewall
3. Allow Python
```

### Issue: Register values don't change

**Check:**

- Auto-update is enabled (default: True)
- Check console for update logs
- UPDATE_INTERVAL is set (default: 5 seconds)

### Issue: "Permission denied" on port 502

**Cause:** Port 502 requires administrator privileges

**Solutions:**

1. Run as administrator (Windows)
2. Use sudo (Linux/Mac): `sudo python modbus_slave_5_registers.py`
3. Change port to >1024 (e.g., 5020)

---

## 🔍 Network Configuration

### Setting IP Address 192.168.1.8

**Windows:**

```
1. Control Panel → Network and Sharing Center
2. Change adapter settings
3. Right-click adapter → Properties
4. Select IPv4 → Properties
5. Use the following IP address:
   IP: 192.168.1.8
   Subnet: 255.255.255.0
   Gateway: 192.168.1.1
6. Click OK
```

**Linux:**

```bash
sudo ifconfig eth0 192.168.1.8 netmask 255.255.255.0
```

**Mac:**

```
System Preferences → Network
→ Select adapter → Configure IPv4 → Manually
→ IP: 192.168.1.8
```

### Verify IP Configuration

**Windows:**

```bash
ipconfig
```

**Linux/Mac:**

```bash
ifconfig
```

Look for `192.168.1.8` in the output.

---

## 📊 Technical Details

### Pymodbus Version

Compatible with **pymodbus 3.x** (tested with 3.6.0+)

### Register Data Model

- **Function Code:** 4 (Read Input Registers)
- **Register Type:** Input Registers (read-only)
- **Data Type:** INT16 (16-bit signed integer)
- **Value Range:** 0-65535
- **Addressing:** 0-based internal, maps to Modbus addresses 0-4

### Thread Safety

- Main server thread handles Modbus protocol
- Auto-update thread safely updates register values
- Thread-safe data access using pymodbus context API

---

## 📝 Testing Scenarios

### Scenario 1: Basic Connectivity Test

1. Start simulator
2. Create gateway device with `create_device_5_registers.py`
3. Verify device creation successful
4. Check simulator logs for connection

### Scenario 2: Data Polling Test

1. Start simulator
2. Gateway polls registers every 1 second
3. Simulator updates every 5 seconds
4. Verify gateway receives updated values

### Scenario 3: Multiple Register Read

1. Start simulator
2. Gateway reads all 5 registers in sequence
3. Verify all register values are correct
4. Check data types match (INT16)

### Scenario 4: Long-Running Test

1. Start simulator
2. Let run for 1 hour
3. Monitor for:
   - Memory leaks
   - Connection drops
   - Update consistency

---

## 🔗 Related Documentation

- [Device_Testing/DOCUMENTATION.md](../Device_Testing/DOCUMENTATION.md) -
  Gateway testing
- [../Docs/API.md](../Docs/API.md) - BLE CRUD API
- [../Docs/MODBUS_DATATYPES.md](../Docs/MODBUS_DATATYPES.md) - Modbus data types

---

## 📞 Support

**PT Surya Inovasi Prioritas (SURIOTA)** R&D Team 📧 Email: support@suriota.com
🌐 Website: https://suriota.com

---

**© 2025 PT Surya Inovasi Prioritas (SURIOTA) - All Rights Reserved**
