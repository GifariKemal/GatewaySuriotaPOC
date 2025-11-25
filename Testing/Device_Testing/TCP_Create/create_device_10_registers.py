#!/usr/bin/env python3
"""
=============================================================================
SRT-MGATE-1210 Testing Program
Create 1 TCP Device + 10 Registers
=============================================================================
Device: TCP Device (192.168.1.8:502)
Slave ID: 1
Registers: 10 Input Registers (INT16)
=============================================================================
Author: Kemal - SURIOTA R&D Team
Date: 2025-11-15
Firmware: SRT-MGATE-1210 v2.2.0
=============================================================================
"""

import asyncio
import json
from bleak import BleakClient, BleakScanner

# =============================================================================
# BLE Configuration (from API.md)
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
        print("  SRT-MGATE-1210 TESTING: CREATE 1 DEVICE + 10 REGISTERS")
        print("="*70)

        # Create TCP Device
        print("\n>>> STEP 1: Creating TCP Device...")

        device_config = {
            "op": "create",
            "type": "device",
            "device_id": None,
            "config": {
                "device_name": "TCP_Device_10_Regs",
                "protocol": "TCP",
                "slave_id": 1,
                "timeout": 3000,
                "retry_count": 3,
                "refresh_rate_ms": 2000,
                "ip": "192.168.1.6",
                "port": 502
            }
        }

        await self.send_command(device_config, "Creating TCP Device: TCP_Device_10_Regs")
        await asyncio.sleep(2)

        if not self.device_id:
            print("[ERROR] Device ID not captured. Aborting...")
            return

        # Create 10 Registers
        print(f"\n>>> STEP 2: Creating 10 Registers for Device ID: {self.device_id}")

        registers = [
            {"address": 0, "name": "Temperature", "desc": "Temperature Sensor Reading", "unit": "°C"},
            {"address": 1, "name": "Humidity", "desc": "Humidity Sensor Reading", "unit": "%"},
            {"address": 2, "name": "Pressure", "desc": "Pressure Sensor Reading", "unit": "Pa"},
            {"address": 3, "name": "Voltage", "desc": "Voltage Measurement", "unit": "V"},
            {"address": 4, "name": "Current", "desc": "Current Measurement", "unit": "A"},
            {"address": 5, "name": "Power", "desc": "Power Consumption", "unit": "W"},
            {"address": 6, "name": "Energy", "desc": "Energy Consumption", "unit": "kWh"},
            {"address": 7, "name": "Frequency", "desc": "Frequency Measurement", "unit": "Hz"},
            {"address": 8, "name": "Speed", "desc": "Motor Speed", "unit": "RPM"},
            {"address": 9, "name": "Flow", "desc": "Flow Rate", "unit": "L/m"}
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
                f"Creating Register {idx}/10: {reg['name']} (Address: {reg['address']})"
            )
            await asyncio.sleep(0.5)

        # Summary
        print("\n" + "="*70)
        print("  SUMMARY")
        print("="*70)
        print(f"Device Name:      TCP_Device_10_Regs")
        print(f"Device ID:        {self.device_id}")
        print(f"Protocol:         Modbus TCP")
        print(f"IP Address:       192.168.1.8")
        print(f"Port:             502")
        print(f"Slave ID:         1")
        print(f"Timeout:          3000 ms")
        print(f"Retry Count:      3")
        print(f"Refresh Rate:     1000 ms")
        print(f"\nRegisters Created: 10")
        print(f"  1. Temperature (Address: 0) - °C")
        print(f"  2. Humidity (Address: 1) - %")
        print(f"  3. Pressure (Address: 2) - Pa")
        print(f"  4. Voltage (Address: 3) - V")
        print(f"  5. Current (Address: 4) - A")
        print(f"  6. Power (Address: 5) - W")
        print(f"  7. Energy (Address: 6) - kWh")
        print(f"  8. Frequency (Address: 7) - Hz")
        print(f"  9. Speed (Address: 8) - RPM")
        print(f" 10. Flow (Address: 9) - L/m")
        print("="*70)

async def main():
    client = DeviceCreationClient()

    try:
        print("\n" + "="*70)
        print("  SRT-MGATE-1210 Firmware Testing")
        print("  Python BLE Device Creation Client")
        print("="*70)
        print("  Version:    1.0.0")
        print("  Date:       2025-11-15")
        print("  Firmware:   SRT-MGATE-1210 v2.2.0")
        print("  Author:     Kemal - SURIOTA R&D Team")
        print("="*70)

        if not await client.connect():
            return

        await client.create_device_and_registers()

        print("\n[SUCCESS] Program completed successfully")
        print("[INFO] Device and registers created on GATEWAY")

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
