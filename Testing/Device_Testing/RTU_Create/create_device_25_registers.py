#!/usr/bin/env python3
"""
=============================================================================
SRT-MGATE-1210 Testing Program
Create 1 RTU Device + 25 Registers
=============================================================================
Device: RTU Device (COM8, 9600 baud, 8N1)
Slave ID: 1
Registers: 25 Input Registers (INT16)
=============================================================================
Author: Kemal - SURIOTA R&D Team
Date: 2025-11-17
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

        # Send entire command at once (MTU is 517 bytes - no chunking needed)
        await self.client.write_gatt_char(COMMAND_CHAR_UUID, json_str.encode())
        await self.client.write_gatt_char(COMMAND_CHAR_UUID, "<END>".encode())
        await asyncio.sleep(2.0)

    async def create_device_and_registers(self):
        print("\n" + "="*70)
        print("  SRT-MGATE-1210 TESTING: CREATE 1 RTU DEVICE + 25 REGISTERS")
        print("="*70)

        print("\n>>> STEP 1: Creating RTU Device...")

        device_config = {
            "op": "create",
            "type": "device",
            "device_id": None,
            "config": {
                "device_name": "RTU_Device_25_Regs",
                "protocol": "RTU",
                "slave_id": 1,
                "timeout": 5000,
                "retry_count": 3,
                "refresh_rate_ms": 2000,
                "serial_port": 2,
                "baud_rate": 9600,
                "data_bits": 8,
                "parity": "None",
                "stop_bits": 1
            }
        }

        await self.send_command(device_config, "Creating RTU Device: RTU_Device_25_Regs")
        await asyncio.sleep(2)

        if not self.device_id:
            print("[ERROR] Device ID not captured. Aborting...")
            return

        print(f"\n>>> STEP 2: Creating 25 Registers for Device ID: {self.device_id}")

        registers = [
            {"address": 0, "name": "Register_1", "desc": "Data Point 1", "unit": "unit"},\n            {"address": 1, "name": "Register_2", "desc": "Data Point 2", "unit": "unit"},\n            {"address": 2, "name": "Register_3", "desc": "Data Point 3", "unit": "unit"},\n            {"address": 3, "name": "Register_4", "desc": "Data Point 4", "unit": "unit"},\n            {"address": 4, "name": "Register_5", "desc": "Data Point 5", "unit": "unit"},\n            {"address": 5, "name": "Register_6", "desc": "Data Point 6", "unit": "unit"},\n            {"address": 6, "name": "Register_7", "desc": "Data Point 7", "unit": "unit"},\n            {"address": 7, "name": "Register_8", "desc": "Data Point 8", "unit": "unit"},\n            {"address": 8, "name": "Register_9", "desc": "Data Point 9", "unit": "unit"},\n            {"address": 9, "name": "Register_10", "desc": "Data Point 10", "unit": "unit"},\n            {"address": 10, "name": "Register_11", "desc": "Data Point 11", "unit": "unit"},\n            {"address": 11, "name": "Register_12", "desc": "Data Point 12", "unit": "unit"},\n            {"address": 12, "name": "Register_13", "desc": "Data Point 13", "unit": "unit"},\n            {"address": 13, "name": "Register_14", "desc": "Data Point 14", "unit": "unit"},\n            {"address": 14, "name": "Register_15", "desc": "Data Point 15", "unit": "unit"},\n            {"address": 15, "name": "Register_16", "desc": "Data Point 16", "unit": "unit"},\n            {"address": 16, "name": "Register_17", "desc": "Data Point 17", "unit": "unit"},\n            {"address": 17, "name": "Register_18", "desc": "Data Point 18", "unit": "unit"},\n            {"address": 18, "name": "Register_19", "desc": "Data Point 19", "unit": "unit"},\n            {"address": 19, "name": "Register_20", "desc": "Data Point 20", "unit": "unit"},\n            {"address": 20, "name": "Register_21", "desc": "Data Point 21", "unit": "unit"},\n            {"address": 21, "name": "Register_22", "desc": "Data Point 22", "unit": "unit"},\n            {"address": 22, "name": "Register_23", "desc": "Data Point 23", "unit": "unit"},\n            {"address": 23, "name": "Register_24", "desc": "Data Point 24", "unit": "unit"},\n            {"address": 24, "name": "Register_25", "desc": "Data Point 25", "unit": "unit"}
        ]

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
                f"Creating Register {idx}/25: {reg['name']} (Address: {reg['address']})"
            )
            await asyncio.sleep(0.5)

        print("\n" + "="*70)
        print("  SUMMARY")
        print("="*70)
        print(f"Device Name:      RTU_Device_25_Regs")
        print(f"Device ID:        {self.device_id}")
        print(f"Protocol:         Modbus RTU")
        print(f"Serial Port:      COM8 (Port 2)")
        print(f"Baud Rate:        9600")
        print(f"Slave ID:         1")
        print(f"Timeout:          5000 ms")
        print(f"Refresh Rate:     2000 ms")
        print(f"\nRegisters Created: 25")
        print("="*70)

async def main():
    client = DeviceCreationClient()

    try:
        print("\n" + "="*70)
        print("  SRT-MGATE-1210 Firmware Testing - RTU Mode (25 Registers)")
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
        print("[INFO] Device and 25 registers created on GATEWAY")

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
