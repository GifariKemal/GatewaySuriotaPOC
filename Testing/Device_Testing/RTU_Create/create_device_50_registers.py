#!/usr/bin/env python3
"""
=============================================================================
SRT-MGATE-1210 Testing Program
Create 1 RTU Device + 50 Registers
=============================================================================
Device: RTU Device (COM8, 9600 baud, 8N1)
Slave ID: 1
Registers: 50 Input Registers (INT16)
=============================================================================
Author: Kemal - SURIOTA R&D Team
Date: 2025-11-17
Firmware: SRT-MGATE-1210 v2.2.0
=============================================================================
"""

import asyncio
import json
from bleak import BleakClient, BleakScanner

# =============================================================================
# BLE Configuration
# =============================================================================
SERVICE_UUID = "00001830-0000-1000-8000-00805f9b34fb"
COMMAND_CHAR_UUID = "11111111-1111-1111-1111-111111111101"
RESPONSE_CHAR_UUID = "11111111-1111-1111-1111-111111111102"
SERVICE_NAME = "SURIOTA GW"

# =============================================================================
# Device Creation Class
# =============================================================================
class DeviceCreationClient:
    def __init__(self):
        self.client = None
        self.response_buffer = ""
        self.connected = False
        self.device_id = None

    async def connect(self):
        try:
            print(f"[SCAN] Scanning for '{SERVICE_NAME}'...")
            devices = await BleakScanner.discover(timeout=10.0)
            device = next((d for d in devices if d.name == SERVICE_NAME), None)

            if not device:
                print(f"[ERROR] Service '{SERVICE_NAME}' not found")
                return False

            print(f"[FOUND] {device.name} ({device.address})")
            self.client = BleakClient(device.address)
            await self.client.connect()
            await self.client.start_notify(RESPONSE_CHAR_UUID, self._notification_handler)

            self.connected = True
            print(f"[SUCCESS] Connected to {device.name}")
            return True

        except Exception as e:
            print(f"[ERROR] Connection failed: {e}")
            return False

    async def disconnect(self):
        if self.client and self.connected:
            await self.client.disconnect()
            self.connected = False
            print("[DISCONNECT] Connection closed")

    def _notification_handler(self, sender, data):
        fragment = data.decode('utf-8')

        if fragment == "<END>":
            if self.response_buffer:
                try:
                    response = json.loads(self.response_buffer)
                    print(f"[RESPONSE] {json.dumps(response, indent=2)}")

                    if response.get('status') == 'ok' and 'device_id' in response:
                        self.device_id = response['device_id']
                        print(f"[CAPTURE] Device ID: {self.device_id}")

                except json.JSONDecodeError as e:
                    print(f"[ERROR] JSON Parse: {e}")
                finally:
                    self.response_buffer = ""
        else:
            self.response_buffer += fragment

    async def send_command(self, command, description=""):
        if not self.connected:
            raise RuntimeError("Not connected to BLE device")

        json_str = json.dumps(command, separators=(',', ':'))

        if description:
            print(f"\n[COMMAND] {description}")
        print(f"[DEBUG] Payload ({len(json_str)} bytes): {json_str}")

        # Send entire command at once (MTU is 517 bytes)
        await self.client.write_gatt_char(COMMAND_CHAR_UUID, json_str.encode())
        await self.client.write_gatt_char(COMMAND_CHAR_UUID, "<END>".encode())
        await asyncio.sleep(2.0)

    async def create_device_and_registers(self):
        print("\n" + "="*70)
        print("  SRT-MGATE-1210 TESTING: CREATE 1 RTU DEVICE + 50 REGISTERS")
        print("="*70)

        # =============================================================================
        # STEP 1: Create RTU Device
        # =============================================================================
        print("\n>>> STEP 1: Creating RTU Device...")

        device_config = {
            "op": "create",
            "type": "device",
            "device_id": None,
            "config": {
                "device_name": "RTU_Device_50Regs",
                "protocol": "RTU",
                "slave_id": 1,
                "timeout": 5000,
                "retry_count": 3,
                "refresh_rate_ms": 10000,
                "serial_port": 2,
                "baud_rate": 9600,
                "data_bits": 8,
                "parity": "None",
                "stop_bits": 1
            }
        }

        await self.send_command(device_config, "Creating RTU Device: RTU_Device_50Regs")
        await asyncio.sleep(2)

        if not self.device_id:
            print("[ERROR] Device ID not captured. Aborting...")
            return

        # =============================================================================
        # STEP 2: Create 50 Registers
        # =============================================================================
        print(f"\n>>> STEP 2: Creating 50 Registers for Device ID: {self.device_id}")

        # Generate 50 registers with diverse sensor types
        registers = []

        # Temperature sensors (0-9)
        for i in range(10):
            registers.append({
                "address": i,
                "name": f"Temp_Zone_{i+1}",
                "desc": f"Temperature Zone {i+1}",
                "unit": "°C"
            })

        # Humidity sensors (10-19)
        for i in range(10):
            registers.append({
                "address": 10 + i,
                "name": f"Humid_Zone_{i+1}",
                "desc": f"Humidity Zone {i+1}",
                "unit": "%"
            })

        # Pressure sensors (20-24)
        for i in range(5):
            registers.append({
                "address": 20 + i,
                "name": f"Press_Sensor_{i+1}",
                "desc": f"Pressure Sensor {i+1}",
                "unit": "Pa"
            })

        # Voltage sensors (25-29)
        for i in range(5):
            registers.append({
                "address": 25 + i,
                "name": f"Voltage_L{i+1}",
                "desc": f"Voltage Line {i+1}",
                "unit": "V"
            })

        # Current sensors (30-34)
        for i in range(5):
            registers.append({
                "address": 30 + i,
                "name": f"Current_L{i+1}",
                "desc": f"Current Line {i+1}",
                "unit": "A"
            })

        # Power sensors (35-39)
        for i in range(5):
            registers.append({
                "address": 35 + i,
                "name": f"Power_{i+1}",
                "desc": f"Power Meter {i+1}",
                "unit": "W"
            })

        # Energy counters (40-44)
        for i in range(5):
            registers.append({
                "address": 40 + i,
                "name": f"Energy_{i+1}",
                "desc": f"Energy Counter {i+1}",
                "unit": "kWh"
            })

        # Flow rate sensors (45-49)
        for i in range(5):
            registers.append({
                "address": 45 + i,
                "name": f"Flow_{i+1}",
                "desc": f"Flow Meter {i+1}",
                "unit": "L/min"
            })

        # Create all 50 registers
        for idx, reg in enumerate(registers, 1):
            register_config = {
                "op": "create",
                "type": "register",
                "device_id": self.device_id,
                "config": {
                    "address": reg["address"],
                    "register_name": reg["name"],
                    "type": "Input Registers",
                    "function_code": 4,
                    "data_type": "INT16",
                    "description": reg["desc"],
                    "unit": reg["unit"],
                    "scale": 1.0,
                    "offset": 0.0
                }
            }

            await self.send_command(
                register_config,
                f"Creating Register {idx}/50: {reg['name']} (Address: {reg['address']})"
            )
            await asyncio.sleep(0.5)

        # =============================================================================
        # STEP 3: Summary
        # =============================================================================
        print("\n" + "="*70)
        print("  SUMMARY")
        print("="*70)
        print(f"Device Name:      RTU_Device_50Regs")
        print(f"Device ID:        {self.device_id}")
        print(f"Protocol:         Modbus RTU")
        print(f"Serial Port:      COM8 (Port 2)")
        print(f"Baud Rate:        9600")
        print(f"Slave ID:         1")
        print(f"Timeout:          5000 ms")
        print(f"Refresh Rate:     2000 ms")
        print(f"\nRegisters Created: 50")
        print(f"  Temperature Zones:  0-9   (10 sensors)")
        print(f"  Humidity Zones:     10-19 (10 sensors)")
        print(f"  Pressure Sensors:   20-24 (5 sensors)")
        print(f"  Voltage Lines:      25-29 (5 sensors)")
        print(f"  Current Lines:      30-34 (5 sensors)")
        print(f"  Power Meters:       35-39 (5 sensors)")
        print(f"  Energy Counters:    40-44 (5 sensors)")
        print(f"  Flow Meters:        45-49 (5 sensors)")
        print("="*70)
        print("\n[INFO] Expected Gateway behavior:")
        print("  - RTU polling time: ~8-10 seconds for 50 registers")
        print("  - Batch completion: All 50 registers must be attempted")
        print("  - MQTT payload: ~2.5-3.0 KB")
        print("  - Publish interval: Every 20-25 seconds (with batch wait)")
        print("="*70)

# =============================================================================
# Main Execution
# =============================================================================
async def main():
    client = DeviceCreationClient()

    try:
        print("\n" + "="*70)
        print("  SRT-MGATE-1210 Firmware Testing - RTU Mode (50 Registers)")
        print("  Python BLE Device Creation Client")
        print("="*70)
        print("  Version:    1.0.0")
        print("  Date:       2025-11-17")
        print("  Firmware:   SRT-MGATE-1210 v2.2.0")
        print("  Author:     Kemal - SURIOTA R&D Team")
        print("="*70)

        if not await client.connect():
            return

        await client.create_device_and_registers()

        print("\n[SUCCESS] Program completed successfully")
        print("[INFO] Device and 50 registers created on GATEWAY")
        print("\n[NEXT STEPS]")
        print("  1. Configure Modbus Slave Simulator with 50 Input Registers (addr 0-49)")
        print("  2. Monitor Gateway serial output for RTU polling logs")
        print("  3. Verify batch completion: (X success, Y failed, 50/50 total)")
        print("  4. Check MQTT broker for published payload (~2.5-3.0 KB)")

    except KeyboardInterrupt:
        print("\n[INTERRUPT] Program interrupted by user")
    except Exception as e:
        print(f"[ERROR] Unexpected error: {e}")
        import traceback
        traceback.print_exc()
    finally:
        await client.disconnect()

if __name__ == "__main__":
    asyncio.run(main())
