# Modbus RTU Testing Scripts

Python scripts untuk testing Modbus RTU device creation via BLE pada SRT-MGATE-1210 Gateway.

## Prerequisites

### Hardware Setup
- **SRT-MGATE-1210 Gateway** (dalam mode Development/BLE ON)
- **RS485 USB Adapter** terhubung ke PC (COM8)
- **Modbus Slave Simulator** running di PC

### Serial Configuration (dari screenshot)
```
Port:       COM8
Baud Rate:  9600
Data Bits:  8
Parity:     None
Stop Bits:  1
Mode:       RTU
Flow Ctrl:  RTS Toggle (1ms delay)
```

### Software Requirements
- Python 3.8+
- Bluetooth adapter di PC
- Modbus Slave Simulator (untuk simulasi RTU slave)

## Installation

1. Install Python dependencies:
```bash
pip install -r requirements.txt
```

2. Pastikan Bluetooth di PC sudah enabled

3. Setup Modbus Slave Simulator:
   - Open Modbus Slave Simulator
   - Connection Setup:
     - Serial Port: COM8
     - Baud: 9600
     - Data bits: 8
     - Parity: None
     - Stop bits: 1
     - Mode: RTU ✓
     - Flow Control: RTS Toggle ✓
   - Add slave dengan ID: 1
   - Add 5 Input Registers (addresses 0-4)

## Available Scripts

### `create_device_5_registers.py`
Membuat 1 RTU device dengan 5 registers via BLE.

### `create_device_10_registers.py`
Membuat 1 RTU device dengan 10 registers via BLE.

### `create_device_50_registers.py`
Membuat 1 RTU device dengan 50 registers via BLE.

**Device Configuration:**
- Device Name: `RTU_Device_Test`
- Protocol: Modbus RTU
- Serial Port: 1 (mapped to COM8)
- Slave ID: 1
- Baud Rate: 9600
- Parameters: 8N1
- Refresh Rate: 1000ms

**Registers Created:**
1. Temperature (Address: 0) - °C
2. Humidity (Address: 1) - %
3. Pressure (Address: 2) - Pa
4. Voltage (Address: 3) - V
5. Current (Address: 4) - A

**Usage:**
```bash
python create_device_5_registers.py
```

**Expected Output:**
```
[SCAN] Scanning for 'SURIOTA GW'...
[FOUND] SURIOTA GW (XX:XX:XX:XX:XX:XX)
[SUCCESS] Connected to SURIOTA GW

>>> STEP 1: Creating RTU Device...
[COMMAND] Creating RTU Device: RTU_Device_Test
[RESPONSE] {
  "status": "ok",
  "device_id": "device_XXXXX",
  "message": "Device created successfully"
}
[CAPTURE] Device ID: device_XXXXX

>>> STEP 2: Creating 5 Registers for Device ID: device_XXXXX
[COMMAND] Creating Register 1/5: Temperature (Address: 0)
...
[SUCCESS] Program completed successfully
```

## Troubleshooting

### Issue: "Service 'SURIOTA GW' not found"
**Solution:**
1. Pastikan Gateway dalam mode Development (BLE ON)
2. Tekan tombol MODE di Gateway
3. LED harus steady ON (bukan blinking)
4. Bluetooth PC sudah enabled

### Issue: "COM8 not found" di Modbus Slave Simulator
**Solution:**
1. Cek Device Manager → Ports (COM & LPT)
2. RS485 USB adapter harus terdeteksi sebagai COM port
3. Update driver jika perlu
4. Ubah COM port di simulator sesuai yang terdeteksi

### Issue: RTU polling timeout
**Solution:**
1. Pastikan serial parameters match (9600, 8N1, RTU)
2. Cek RTS Toggle enabled di simulator
3. Pastikan slave ID match (1)
4. Cek kabel RS485 terhubung dengan benar (A to A, B to B)

### Issue: No data received
**Solution:**
1. Restart Modbus Slave Simulator
2. Verify register addresses exist (0-4)
3. Check serial port tidak digunakan aplikasi lain
4. Monitor Gateway serial log untuk error messages

## Testing Flow

```
1. Setup Modbus Slave Simulator (COM8, 9600, 8N1, RTU)
   ↓
2. Add Slave ID: 1 with 5 Input Registers (addr 0-4)
   ↓
3. Run Python script: create_device_5_registers.py
   ↓
4. Script connects via BLE → Creates device + registers
   ↓
5. Gateway starts polling RTU device via serial
   ↓
6. Monitor Gateway logs:
   [RTU] Polling device RTU_Device_Test (Slave:1 Port:1 Baud:9600)
   [BATCH] Device XXX COMPLETE (5 success, 0 failed, 5/5 total)
   [MQTT] Publishing device XXX (payload size)
```

## Serial Port Mapping

Gateway menggunakan **serial_port: 1** untuk RTU communication, yang di-map ke:
- **Hardware:** ESP32-S3 Serial1 (TX/RX pins)
- **PC Side:** COM8 (via RS485 USB adapter)

**Connection:**
```
Gateway (Serial1) <--RS485--> RS485 Adapter <--USB--> PC (COM8)
```

## Notes

- **No Chunking:** BLE MTU sudah 517 bytes (512 effective), command dikirim langsung tanpa fragmentasi
- **Batch Tracking:** Gateway menunggu semua 5 registers terbaca sebelum publish MQTT
- **Timeout:** Default 5000ms per register read
- **Refresh Rate:** Default 2000ms per device polling cycle
- **Flow Control:** RTS Toggle perlu diaktifkan di simulator untuk compatibility

## References

- Gateway API Documentation: `/Main/API.md`
- BLE UUIDs Configuration: Service UUID, Command UUID, Response UUID
- Serial Configuration: Sesuai screenshot RTU Connection Setup

---

**Author:** Kemal - SURIOTA R&D Team
**Date:** 2025-11-17
**Firmware Version:** SRT-MGATE-1210 v2.3.0
