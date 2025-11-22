# Modbus RTU Slave Simulator - 5 Registers

**SRT-MGATE-1210 Firmware Testing Tool**

---

## Overview

This simulator creates a **Modbus RTU Slave (Server)** that emulates a device with **5 Input Registers**, designed to test the SRT-MGATE-1210 gateway's RTU polling capabilities over serial communication (RS-485/RS-232).

The simulator **exactly matches** the configuration used in `Device_Testing/RTU/create_device_5_registers.py`, ensuring seamless integration testing.

---

## Features

- **Modbus RTU Protocol** (Function Code 4: Read Input Registers)
- **5 Input Registers** with realistic sensor simulation:
  - Temperature (°C)
  - Humidity (%)
  - Pressure (Pa)
  - Voltage (V)
  - Current (A)
- **Auto-updating values** every 5 seconds (realistic sensor behavior)
- **Serial port support** (COM8 on Windows, /dev/ttyUSB0 on Linux)
- **RTU Frame Format** with configurable baud rate, parity, and stop bits
- **Thread-safe** register updates
- **Comprehensive logging** with real-time value display

---

## Hardware Requirements

### USB-to-RS485 Adapter

You need a **USB-to-RS485** or **USB-to-RS232** adapter to connect your PC to the SRT-MGATE-1210 gateway.

**Recommended adapters:**
- FTDI USB-to-RS485 converter (stable drivers)
- CH340/CH341 USB-to-RS485 (budget-friendly)
- CP2102 USB-to-TTL with RS485 module

**Connection Diagram:**
```
PC (COM8)  <--USB-->  [USB-to-RS485 Adapter]  <--RS485-->  ESP32-S3 (Serial2: RX=17, TX=18)
                           A/B Terminals              A/B Terminals on gateway
```

**Wiring:**
- **A+ (Adapter)** → **A+ (Gateway Serial2)**
- **B- (Adapter)** → **B- (Gateway Serial2)**
- **GND** → **GND** (common ground recommended)

---

## Configuration Match

This simulator is **synchronized** with the gateway device configuration:

| Parameter        | Simulator Value | Gateway Config (create_device_5_registers.py) |
|------------------|-----------------|-----------------------------------------------|
| **Serial Port**  | COM8            | Serial Port 2 (serial_port: 2)                |
| **Baud Rate**    | 9600            | 9600                                          |
| **Data Bits**    | 8               | 8                                             |
| **Parity**       | None            | None                                          |
| **Stop Bits**    | 1               | 1                                             |
| **Frame Format** | RTU             | RTU                                           |
| **Slave ID**     | 1               | 1                                             |
| **Function Code**| 4 (Read IR)     | 4 (Read Input Registers)                      |
| **Registers**    | 5 (Address 0-4) | 5 Input Registers (INT16)                     |

---

## Installation

### Prerequisites

- **Python 3.7+** (recommended: Python 3.9 or higher)
- **USB-to-RS485 adapter** connected to PC
- **Driver installed** for the USB adapter (check Device Manager on Windows)

### Step 1: Install Dependencies

Navigate to the RTU simulator directory:

```bash
cd C:\Users\Administrator\Pictures\SRT-MGATE-1210\Testing\Modbus_Simulators\RTU
```

Install required Python packages:

```bash
pip install -r requirements.txt
```

Or install manually:

```bash
pip install pymodbus pyserial
```

**Verify installation:**

```bash
python -c "import pymodbus; import serial; print('pymodbus:', pymodbus.__version__); print('pyserial:', serial.VERSION)"
```

Expected output:
```
pymodbus: 3.x.x
pyserial: 3.5
```

---

## Usage

### Step 1: Check Serial Port

**Windows:**

Open **Device Manager** → **Ports (COM & LPT)** and verify your USB-to-RS485 adapter COM port (e.g., COM8).

If it's **not COM8**, edit `modbus_slave_5_registers.py`:

```python
SERIAL_PORT = 'COM8'  # Change to your port, e.g., 'COM3'
```

**Linux/Raspberry Pi:**

List available ports:

```bash
ls /dev/ttyUSB*
```

Edit the script if needed:

```python
SERIAL_PORT = '/dev/ttyUSB0'  # Adjust as needed
```

### Step 2: Run the Simulator

```bash
python modbus_slave_5_registers.py
```

**Expected Output:**

```
======================================================================
  MODBUS RTU SLAVE SIMULATOR
  SRT-MGATE-1210 Firmware Testing Tool
======================================================================

  Configuration:
  ├─ Port:         COM8
  ├─ Baud Rate:    9600
  ├─ Format:       8N1 (RTU)
  ├─ Slave ID:     1
  ├─ Registers:    5 Input Registers (INT16)
  └─ Addresses:    0-4

  Register Mapping:
  [0] Temperature  - °C   (20-35)
  [1] Humidity     - %    (40-80)
  [2] Pressure     - Pa   (900-1100)
  [3] Voltage      - V    (220-240)
  [4] Current      - A    (1-10)

  Gateway Configuration (use in Device_Testing/RTU):
  ├─ Serial Port:  COM8 (or Port 2 on ESP32)
  ├─ Baud Rate:    9600
  ├─ Data Bits:    8
  ├─ Parity:       None
  ├─ Stop Bits:    1
  ├─ Slave ID:     1
  ├─ Protocol:     RTU
  └─ Function:     Read Input Registers (FC 4)

======================================================================

  This simulator matches the configuration from:
  Device_Testing/RTU/create_device_5_registers.py

  IMPORTANT:
  - Make sure USB-to-RS485 adapter is connected to COM8
  - Configure Modbus Slave Simulator settings as shown in screenshot
  - RTS Toggle with 1ms disable delay
  - No other program should use COM8

======================================================================

Press Enter to start server...
```

Press **Enter** to start the server.

### Step 3: Server Running

Once started, you'll see:

```
======================================================================
  MODBUS RTU SLAVE SIMULATOR
  SRT-MGATE-1210 Testing - 5 Input Registers
======================================================================
  Serial Configuration:
  - Port:           COM8
  - Baud Rate:      9600
  - Data Bits:      8
  - Parity:         N (None)
  - Stop Bits:      1
  - Frame Format:   RTU
  - Flow Control:   RTS Toggle

  Modbus Configuration:
  - Slave ID:       1
  - Registers:      5 Input Registers (Function Code 4)
  - Data Type:      INT16 (0-65535)
  - Address Range:  0-4

  Auto Update:
  - Enabled:        Yes
  - Interval:       5.0s

  Libraries:
  - PyModbus:       v3.x.x
  - PySerial:       v3.5
  - Platform:       Windows

======================================================================

[INFO] Initializing Modbus RTU slave server...
[INFO] Available serial ports on this system:
  → COM8: USB Serial Port (COM8)
[INFO] Server listening on COM8
[INFO] Ready for connections from SRT-MGATE-1210 Gateway
[INFO] Press Ctrl+C to stop server

──────────────────────────────────────────────────────────────────────
  Update #0001 - Register Values (Slave ID: 1):
──────────────────────────────────────────────────────────────────────
  [0] Temperature :    25 °C
  [1] Humidity    :    60 %
  [2] Pressure    :  1000 Pa
  [3] Voltage     :   230 V
  [4] Current     :     5 A
──────────────────────────────────────────────────────────────────────
[INFO] Waiting for Modbus RTU requests on COM8...
```

The simulator is now **listening** for Modbus RTU requests from the gateway!

### Step 4: Test with Gateway

1. **Upload firmware** to SRT-MGATE-1210 (if not already done)
2. **Connect gateway Serial2** (RX=17, TX=18) to USB-to-RS485 adapter via RS485 A/B wires
3. **Run device creation script** from `Device_Testing/RTU/`:

```bash
python create_device_5_registers.py
```

4. **Gateway will poll** the simulator every 2 seconds (refresh_rate_ms: 2000)
5. **Monitor simulator output** for incoming read requests and updated values

---

## Troubleshooting

### Error: "Could not open port COM8"

**Causes:**
- Port is already in use by another program
- USB adapter not connected
- Incorrect COM port number
- Insufficient permissions

**Solutions:**
1. Close any program using COM8 (Arduino IDE, PuTTY, etc.)
2. Check Device Manager for correct COM port
3. Run Command Prompt/PowerShell as **Administrator**
4. Reconnect USB adapter and check port assignment

### Error: "pymodbus not installed"

```bash
pip install pymodbus pyserial
```

If using Python 3.11+, ensure you have pymodbus 3.0+:

```bash
pip install --upgrade pymodbus
```

### Error: "Port not found in available ports"

The script auto-detects available ports. If your port is different:

1. Check available ports in the script output
2. Edit `SERIAL_PORT` variable in `modbus_slave_5_registers.py`
3. Restart the script

### No Data Received by Gateway

**Check:**
1. **Wiring:** A+ to A+, B- to B-, GND to GND
2. **Baud rate match:** Both simulator and gateway set to 9600
3. **Slave ID match:** Both set to 1
4. **Serial port:** Gateway configured for Serial Port 2 (serial_port: 2)
5. **RS485 adapter:** Verify TX/RX LEDs blink during polling

**Test with Modbus Poll:**

Use **Modbus Poll** software (Windows) to manually test the simulator:
- Connection: COM8, 9600, 8N1, RTU
- Slave ID: 1
- Function: 04 Read Input Registers
- Address: 0, Length: 5

You should see live register values updating.

---

## Register Details

| Address | Name        | Unit | Data Type | Range       | Initial Value | Description                |
|---------|-------------|------|-----------|-------------|---------------|----------------------------|
| 0       | Temperature | °C   | INT16     | 20-35       | 25            | Temperature sensor reading |
| 1       | Humidity    | %    | INT16     | 40-80       | 60            | Humidity sensor reading    |
| 2       | Pressure    | Pa   | INT16     | 900-1100    | 1000          | Pressure sensor reading    |
| 3       | Voltage     | V    | INT16     | 220-240     | 230           | Voltage measurement        |
| 4       | Current     | A    | INT16     | 1-10        | 5             | Current measurement        |

**Note:** All registers are **Input Registers** (Read-Only, Function Code 4).

---

## Auto-Update Behavior

The simulator automatically updates register values every **5 seconds** to simulate realistic sensor behavior:

- **Temperature:** Slow changes (±1°C)
- **Humidity:** Moderate changes (±2%)
- **Pressure:** Small variations (±5 Pa)
- **Voltage:** Very stable (±1V)
- **Current:** Changes (±1A)

Values are **clamped** to their defined min/max ranges to prevent unrealistic data.

---

## Gateway Integration

### Gateway Configuration (via BLE)

Use `create_device_5_registers.py` to configure the gateway:

```python
device_config = {
    "op": "create",
    "type": "device",
    "device_id": None,
    "config": {
        "device_name": "RTU_Device_Test",
        "protocol": "RTU",
        "slave_id": 1,
        "timeout": 5000,
        "retry_count": 3,
        "refresh_rate_ms": 2000,
        "serial_port": 2,       # Serial2 on ESP32-S3
        "baud_rate": 9600,
        "data_bits": 8,
        "parity": "None",
        "stop_bits": 1
    }
}
```

### Expected Gateway Behavior

1. Gateway polls simulator every **2 seconds** (refresh_rate_ms: 2000)
2. Sends **Modbus RTU request** (Function Code 4, Slave ID 1, Start Address 0, Count 5)
3. Simulator **responds** with 5 register values (10 bytes total)
4. Gateway **parses** response and stores in queue for MQTT/HTTP transmission
5. **LED indicators** on USB-to-RS485 adapter blink during communication

---

## Advanced Configuration

### Changing Serial Port

Edit `modbus_slave_5_registers.py`:

```python
# For Windows
SERIAL_PORT = 'COM3'  # Change to your port

# For Linux/Raspberry Pi
SERIAL_PORT = '/dev/ttyUSB0'
```

### Changing Baud Rate

Edit both simulator **and** gateway device config:

**Simulator:**
```python
BAUD_RATE = 19200  # Example: 19200 baud
```

**Gateway (via BLE):**
```python
"baud_rate": 19200
```

### Disabling Auto-Update

To use **static** register values:

```python
AUTO_UPDATE = False
```

### Adjusting Update Interval

```python
UPDATE_INTERVAL = 10.0  # Update every 10 seconds
```

---

## Testing Workflow

### Complete Testing Procedure

1. **Start Simulator:**
   ```bash
   python modbus_slave_5_registers.py
   ```

2. **Verify simulator is listening** on COM8

3. **Power on SRT-MGATE-1210 Gateway**

4. **Configure gateway** via BLE using `create_device_5_registers.py`:
   ```bash
   python create_device_5_registers.py
   ```

5. **Monitor simulator output** for incoming Modbus requests

6. **Check gateway serial output** (if connected) for RTU polling logs:
   ```
   [RTU] Reading device: RTU_Device_Test (Slave ID: 1)
   [RTU] Read 5 registers from address 0
   [RTU] Values: [25, 60, 1000, 230, 5]
   ```

7. **Verify data transmission** to MQTT/HTTP server (if configured)

8. **Stop simulator** with `Ctrl+C`

---

## File Structure

```
RTU/
├── modbus_slave_5_registers.py  # Main simulator script
├── requirements.txt              # Python dependencies
└── README.md                     # This documentation
```

---

## Technical Details

### Modbus RTU Frame Format

**Request from Gateway (Function Code 4):**
```
[Slave ID] [Function Code] [Start Address High] [Start Address Low] [Count High] [Count Low] [CRC Low] [CRC High]
   0x01        0x04             0x00                0x00              0x00         0x05       [CRC]    [CRC]
```

**Response from Simulator:**
```
[Slave ID] [Function Code] [Byte Count] [Reg0 High] [Reg0 Low] [Reg1 High] [Reg1 Low] ... [CRC Low] [CRC High]
   0x01        0x04            0x0A         Data        Data       Data        Data            [CRC]    [CRC]
```

### Timing Characteristics

- **Response Time:** <10ms (typical)
- **CRC Calculation:** Automatic (handled by pymodbus)
- **Frame Gap:** 3.5 character times (handled by RTU framer)
- **Timeout:** 1 second (configurable in StartSerialServer)

---

## Compatibility

- **Firmware:** SRT-MGATE-1210 v2.3.0
- **Python:** 3.7, 3.8, 3.9, 3.10, 3.11, 3.12
- **PyModbus:** 3.0.0 - 3.6.x
- **PySerial:** 3.5+
- **OS:** Windows 10/11, Linux, Raspberry Pi OS

---

## Support

For issues or questions:

1. Check **Troubleshooting** section above
2. Verify hardware connections (RS485 wiring)
3. Review gateway serial logs
4. Contact: **Kemal - SURIOTA R&D Team**

---

## License

Part of SRT-MGATE-1210 Firmware Testing Suite
Copyright © 2025 SURIOTA R&D Team

---

## Changelog

### v1.0.0 (2025-11-17)
- Initial release
- Support for 5 Input Registers (INT16)
- Auto-update simulation
- Serial port auto-detection
- Comprehensive error handling
- Complete documentation
