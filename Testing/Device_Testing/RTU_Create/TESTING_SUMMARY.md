# RTU Testing Scripts - Summary

## Overview

Testing suite untuk Modbus RTU device creation dengan berbagai jumlah registers (5, 10, 50).

**Author:** Kemal - SURIOTA R&D Team
**Date:** 2025-11-17
**Firmware:** SRT-MGATE-1210 v2.3.0

---

## Scripts Available

| Script | Registers | Device Name | Use Case |
|--------|-----------|-------------|----------|
| `create_device_5_registers.py` | 5 | RTU_Device_Test | Quick testing, basic validation |
| `create_device_10_registers.py` | 10 | RTU_Device_10Regs | Light load testing |
| `create_device_15_registers.py` | 15 | RTU_Device_15_Regs | Light-medium load |
| `create_device_20_registers.py` | 20 | RTU_Device_20_Regs | Medium load |
| `create_device_25_registers.py` | 25 | RTU_Device_25_Regs | Medium-high load |
| `create_device_30_registers.py` | 30 | RTU_Device_30_Regs | High load |
| `create_device_35_registers.py` | 35 | RTU_Device_35_Regs | Very high load |
| `create_device_40_registers.py` | 40 | RTU_Device_40_Regs | Near maximum |
| `create_device_45_registers.py` | 45 | RTU_Device_45_Regs | Close to maximum |
| `create_device_50_registers.py` | 50 | RTU_Device_50Regs | Maximum load, production testing |

---

## Register Distribution

### 5 Registers Script
```
0: Temperature (°C)
1: Humidity (%)
2: Pressure (Pa)
3: Voltage (V)
4: Current (A)
```

### 10 Registers Script
```
0: Temperature (°C)
1: Humidity (%)
2: Pressure (Pa)
3: Voltage (V)
4: Current (A)
5: Power (W)
6: Energy (kWh)
7: Frequency (Hz)
8: Power_Factor (PF)
9: Flow_Rate (L/min)
```

### 50 Registers Script
```
Temperature Zones:  0-9   (10 sensors - Temp_Zone_1 to Temp_Zone_10)
Humidity Zones:     10-19 (10 sensors - Humid_Zone_1 to Humid_Zone_10)
Pressure Sensors:   20-24 (5 sensors - Press_Sensor_1 to Press_Sensor_5)
Voltage Lines:      25-29 (5 sensors - Voltage_L1 to Voltage_L5)
Current Lines:      30-34 (5 sensors - Current_L1 to Current_L5)
Power Meters:       35-39 (5 sensors - Power_1 to Power_5)
Energy Counters:    40-44 (5 sensors - Energy_1 to Energy_5)
Flow Meters:        45-49 (5 sensors - Flow_1 to Flow_5)
```

---

## Device Configuration (All Scripts)

```json
{
  "protocol": "RTU",
  "slave_id": 1,
  "serial_port": 1,
  "baud_rate": 9600,
  "data_bits": 8,
  "parity": "None",
  "stop_bits": 1,
  "timeout": 5000,
  "refresh_rate_ms": 2000,
  "retry_count": 3
}
```

---

## Performance Expectations

| Registers | RTU Poll Time | MQTT Payload | Batch Time | Publish Interval |
|-----------|---------------|--------------|------------|------------------|
| 5         | ~1-2s         | ~1.5 KB      | ~1.5s      | 20-22s           |
| 10        | ~2-3s         | ~1.8 KB      | ~2.5s      | 20-22s           |
| 15        | ~3-4s         | ~2.0 KB      | ~3.5s      | 20-23s           |
| 20        | ~4-5s         | ~2.2 KB      | ~4.5s      | 20-24s           |
| 25        | ~5-6s         | ~2.4 KB      | ~5.5s      | 20-24s           |
| 30        | ~6-7s         | ~2.5 KB      | ~6.5s      | 20-24s           |
| 35        | ~7-8s         | ~2.7 KB      | ~7.5s      | 20-25s           |
| 40        | ~8-9s         | ~2.8 KB      | ~8.5s      | 20-25s           |
| 45        | ~9-10s        | ~2.9 KB      | ~9.5s      | 20-25s           |
| 50        | ~10-11s       | ~3.0 KB      | ~10.5s     | 20-25s           |

**Notes:**
- RTU polling lebih lambat dari TCP karena serial communication overhead
- Batch tracking menunggu semua registers attempted (success + failed)
- MQTT interval variance normal karena asynchronous polling

---

## Modbus Slave Simulator Setup

### For 5 Registers
```
Slave ID: 1
Input Registers: 0-4 (5 registers)
```

### For 10 Registers
```
Slave ID: 1
Input Registers: 0-9 (10 registers)
```

### For 50 Registers
```
Slave ID: 1
Input Registers: 0-49 (50 registers)
```

### Serial Configuration (All)
```
COM Port:     COM8
Baud Rate:    9600
Data Bits:    8
Parity:       None
Stop Bits:    1
Mode:         RTU
Flow Control: RTS Toggle (1ms delay)
```

---

## Running Tests

### Option 1: Individual Scripts
```bash
python create_device_5_registers.py
python create_device_10_registers.py
python create_device_50_registers.py
```

### Option 2: Progressive Testing
```bash
# 1. Start with 5 registers (basic validation)
python create_device_5_registers.py

# 2. Monitor Gateway logs, verify batch tracking works

# 3. Increase to 10 registers (medium load)
python create_device_10_registers.py

# 4. Monitor polling time, MQTT payload size

# 5. Full test with 50 registers (production load)
python create_device_50_registers.py

# 6. Verify batch completion, memory stable, MQTT success
```

---

## Expected Gateway Logs

### 5 Registers
```
[RTU] Polling device RTU_Device_Test (Slave:1 Port:1 Baud:9600)
[DATA] device_XXXXX:
  L1: Temperature:25.0°C | Humidity:65.0% | Pressure:1013.0Pa | Voltage:220.0V | Current:5.0A
[BATCH] Device device_XXXXX COMPLETE (5 success, 0 failed, 5/5 total, took 1500 ms)
[MQTT] Publishing device device_XXXXX (payload: 1.5KB)
[MQTT] Publish successful
```

### 10 Registers
```
[RTU] Polling device RTU_Device_10Regs (Slave:1 Port:1 Baud:9600)
[DATA] device_XXXXX:
  L1: Temperature:25.0°C | Humidity:65.0% | Pressure:1013.0Pa | Voltage:220.0V | Current:5.0A | Power:1100.0W
  L2: Energy:50.0kWh | Frequency:50.0Hz | Power_Factor:0.9PF | Flow_Rate:10.0L/min
[BATCH] Device device_XXXXX COMPLETE (10 success, 0 failed, 10/10 total, took 3000 ms)
[MQTT] Publishing device device_XXXXX (payload: 2.0KB)
[MQTT] Publish successful
```

### 50 Registers
```
[RTU] Polling device RTU_Device_50Regs (Slave:1 Port:1 Baud:9600)
[DATA] device_XXXXX:
  L1: Temp_Zone_1:25.0°C | Temp_Zone_2:26.0°C | Temp_Zone_3:24.0°C | ...
  L2: Humid_Zone_1:65.0% | Humid_Zone_2:60.0% | Humid_Zone_3:70.0% | ...
  ... (8-10 lines of compact data)
[BATCH] Device device_XXXXX COMPLETE (48 success, 2 failed, 50/50 total, took 9000 ms)
[MQTT] Publishing device device_XXXXX (payload: 2.8KB)
[MQTT] Publish successful
```

---

## Troubleshooting

### Issue: "Timeout reading registers"
**Cause:** Serial communication too slow or simulator not responding
**Fix:**
- Check COM8 connection
- Verify simulator is running and slave ID = 1
- Increase timeout to 5000ms (already default)

### Issue: "Batch incomplete"
**Cause:** Some registers failed to read
**Fix:**
- Normal behavior - batch tracks attempts (success + failed)
- Check simulator has all required registers configured
- Failed registers will be logged with ERROR

### Issue: "MQTT payload too large"
**Cause:** 50 registers in full mode exceeds 8KB buffer
**Fix:**
- Should not happen with 50 registers (~2.8KB)
- If happens, use minimal mode in future

### Issue: "Slow RTU polling"
**Expected:** RTU is slower than TCP due to serial overhead
**Normal rates:**
- 5 regs: ~1-2s
- 10 regs: ~2-4s
- 50 regs: ~8-10s

---

## BLE Communication Details

### MTU Configuration
- **MTU Size:** 517 bytes (512 effective)
- **Chunking:** Not used (commands <512 bytes)
- **Transmission:** Single packet per command + END marker

### Command Flow
```
Python Script
    ↓ (BLE)
Gateway BLE Manager
    ↓ (Parse JSON)
CRUD Handler
    ↓ (Create device/register)
Config Manager
    ↓ (Save to LittleFS)
RTU Service
    ↓ (Poll via Serial)
Modbus Slave Simulator (COM8)
    ↓ (Response)
Queue Manager
    ↓ (Batch tracking)
MQTT Manager
    ↓ (Publish when complete)
MQTT Broker
```

---

## Success Criteria

### All Scripts Should Achieve:
✅ Device creation successful (status: "ok")
✅ All registers created successfully
✅ RTU polling starts automatically
✅ Batch tracking shows correct attempt count
✅ MQTT publishes complete payload
✅ No memory leaks (DRAM stable)
✅ No crashes or reboots

### Specific Metrics:
- **5 regs:** Batch complete in ~1.5s, MQTT payload ~1.5KB
- **10 regs:** Batch complete in ~3s, MQTT payload ~2.0KB
- **50 regs:** Batch complete in ~9s, MQTT payload ~2.8KB

---

## Files Structure

```
RTU/
├── _generate_programs.py             ← Generator script for creating test programs
├── create_device_5_registers.py      ← 5 registers
├── create_device_10_registers.py     ← 10 registers
├── create_device_15_registers.py     ← 15 registers
├── create_device_20_registers.py     ← 20 registers
├── create_device_25_registers.py     ← 25 registers
├── create_device_30_registers.py     ← 30 registers
├── create_device_35_registers.py     ← 35 registers
├── create_device_40_registers.py     ← 40 registers
├── create_device_45_registers.py     ← 45 registers
├── create_device_50_registers.py     ← 50 registers (maximum)
├── requirements.txt                  ← Python dependencies
├── README.md                         ← Main documentation
├── QUICK_START.txt                   ← Quick reference
├── TESTING_SUMMARY.md                ← This file
└── run_test.bat                      ← Windows launcher
```

---

**Last Updated:** 2025-11-17
**Tested On:** SRT-MGATE-1210 v2.3.0
**Author:** Kemal - SURIOTA R&D Team
