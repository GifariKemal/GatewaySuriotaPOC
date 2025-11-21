#!/usr/bin/env python3
"""
=============================================================================
SRT-MGATE-1210 Testing Program
Create 1 TCP Device + 50 Registers
=============================================================================
Device: TCP Device (192.168.1.8:502)
Slave ID: 1
Registers: 50 Input Registers (INT16)
=============================================================================
Author: Kemal - SURIOTA R&D Team
Date: 2025-11-15
Firmware: SRT-MGATE-1210 v2.2.0
=============================================================================
"""

import asyncio
import json
from bleak import BleakClient, BleakScanner

SERVICE_UUID = "00001830-0000-1000-8000-00805f9b34fb"
COMMAND_CHAR_UUID = "11111111-1111-1111-1111-111111111101"
RESPONSE_CHAR_UUID = "11111111-1111-1111-1111-111111111102"
SERVICE_NAME = "SURIOTA GW"

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
                print("[INFO] Make sure:")
                print("       1. GATEWAY is powered on")
                print("       2. Mode Development BLE activated")
                print("       3. LED should be ON (steady)")
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
                    print(f"[DEBUG] Buffer content: {self.response_buffer}")
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
        print(f"[DEBUG] Payload: {json_str}")

        chunk_size = 18
        for i in range(0, len(json_str), chunk_size):
            chunk = json_str[i:i+chunk_size]
            await self.client.write_gatt_char(COMMAND_CHAR_UUID, chunk.encode())
            await asyncio.sleep(0.1)

        await self.client.write_gatt_char(COMMAND_CHAR_UUID, "<END>".encode())
        await asyncio.sleep(2.0)

    async def create_device_and_registers(self):
        print("\n" + "="*70)
        print("  SRT-MGATE-1210 TESTING: CREATE 1 DEVICE + 50 REGISTERS")
        print("="*70)

        print("\n>>> STEP 1: Creating TCP Device...")

        device_config = {
            "op": "create",
            "type": "device",
            "device_id": None,
            "config": {
                "device_name": "TCP_Device_50_Regs",
                "protocol": "TCP",
                "slave_id": 1,
                "timeout": 5000,
                "retry_count": 3,
                "refresh_rate_ms": 500,
                "ip": "10.21.239.9",
                "port": 502
            }
        }

        await self.send_command(device_config, "Creating TCP Device: TCP_Device_50_Regs")
        await asyncio.sleep(2)

        if not self.device_id:
            print("[ERROR] Device ID not captured. Aborting...")
            return

        # =============================================================================
        # STEP 2: Create 50 Registers (ALIGNED with RTU layout)
        # =============================================================================
        print(f"\n>>> STEP 2: Creating 50 Registers for Device ID: {self.device_id}")

        # Generate 50 registers with consistent layout (same as RTU)
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

        print("\n" + "="*70)
        print("  SUMMARY")
        print("="*70)
        print(f"Device Name:      TCP_Device_50_Regs")
        print(f"Device ID:        {self.device_id}")
        print(f"Protocol:         Modbus TCP")
        print(f"IP Address:       192.168.1.8")
        print(f"Port:             502")
        print(f"Slave ID:         1")
        print(f"Refresh Rate:     1000 ms")
        print(f"\nRegisters Created: 50")
        print("="*70)

async def main():
    client = DeviceCreationClient()

    try:
        print("\n" + "="*70)
        print("  SRT-MGATE-1210 Firmware Testing")
        print("  Python BLE Device Creation Client")
        print("="*70)
        print("  Version:    1.0.0")
        print("  Date:       2025-11-14")
        print("  Author:     Kemal - SURIOTA R&D Team")
        print("="*70)

        if not await client.connect():
            return

        await client.create_device_and_registers()

        print("\n[SUCCESS] Program completed successfully")
        print("[INFO] Device and 50 registers created on GATEWAY")

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
