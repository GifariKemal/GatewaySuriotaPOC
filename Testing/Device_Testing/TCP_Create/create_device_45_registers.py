#!/usr/bin/env python3
"""
=============================================================================
SRT-MGATE-1210 Testing Program
Create 1 TCP Device + 45 Registers
=============================================================================
Device: TCP Device (192.168.1.8:502)
Slave ID: 1
Registers: 45 Input Registers (INT16)
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
        print("  SRT-MGATE-1210 TESTING: CREATE 1 DEVICE + 45 REGISTERS")
        print("="*70)

        print("\n>>> STEP 1: Creating TCP Device...")

        device_config = {
            "op": "create",
            "type": "device",
            "device_id": None,
            "config": {
                "device_name": "TCP_Device_45_Regs",
                "protocol": "TCP",
                "slave_id": 1,
                "timeout": 5000,
                "retry_count": 3,
                "refresh_rate_ms": 2000,
                "ip": "192.168.1.6",
                "port": 502
            }
        }

        await self.send_command(device_config, "Creating TCP Device: TCP_Device_45_Regs")
        await asyncio.sleep(2)

        if not self.device_id:
            print("[ERROR] Device ID not captured. Aborting...")
            return

        print(f"\n>>> STEP 2: Creating 45 Registers for Device ID: {self.device_id}")

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
            {"address": 9, "name": "Flow", "desc": "Flow Rate", "unit": "L/m"},
            {"address": 10, "name": "Temperature_2", "desc": "Temperature Sensor 2 Reading", "unit": "°C"},
            {"address": 11, "name": "Humidity_2", "desc": "Humidity Sensor 2 Reading", "unit": "%"},
            {"address": 12, "name": "Pressure_2", "desc": "Pressure Sensor 2 Reading", "unit": "Pa"},
            {"address": 13, "name": "Voltage_2", "desc": "Voltage Measurement 2", "unit": "V"},
            {"address": 14, "name": "Current_2", "desc": "Current Measurement 2", "unit": "A"},
            {"address": 15, "name": "Power_2", "desc": "Power Consumption 2", "unit": "W"},
            {"address": 16, "name": "Energy_2", "desc": "Energy Consumption 2", "unit": "kWh"},
            {"address": 17, "name": "Frequency_2", "desc": "Frequency Measurement 2", "unit": "Hz"},
            {"address": 18, "name": "Speed_2", "desc": "Motor Speed 2", "unit": "RPM"},
            {"address": 19, "name": "Flow_2", "desc": "Flow Rate 2", "unit": "L/m"},
            {"address": 20, "name": "Temperature_3", "desc": "Temperature Sensor 3 Reading", "unit": "°C"},
            {"address": 21, "name": "Humidity_3", "desc": "Humidity Sensor 3 Reading", "unit": "%"},
            {"address": 22, "name": "Pressure_3", "desc": "Pressure Sensor 3 Reading", "unit": "Pa"},
            {"address": 23, "name": "Voltage_3", "desc": "Voltage Measurement 3", "unit": "V"},
            {"address": 24, "name": "Current_3", "desc": "Current Measurement 3", "unit": "A"},
            {"address": 25, "name": "Power_3", "desc": "Power Consumption 3", "unit": "W"},
            {"address": 26, "name": "Energy_3", "desc": "Energy Consumption 3", "unit": "kWh"},
            {"address": 27, "name": "Frequency_3", "desc": "Frequency Measurement 3", "unit": "Hz"},
            {"address": 28, "name": "Speed_3", "desc": "Motor Speed 3", "unit": "RPM"},
            {"address": 29, "name": "Flow_3", "desc": "Flow Rate 3", "unit": "L/m"},
            {"address": 30, "name": "Temperature_4", "desc": "Temperature Sensor 4 Reading", "unit": "°C"},
            {"address": 31, "name": "Humidity_4", "desc": "Humidity Sensor 4 Reading", "unit": "%"},
            {"address": 32, "name": "Pressure_4", "desc": "Pressure Sensor 4 Reading", "unit": "Pa"},
            {"address": 33, "name": "Voltage_4", "desc": "Voltage Measurement 4", "unit": "V"},
            {"address": 34, "name": "Current_4", "desc": "Current Measurement 4", "unit": "A"},
            {"address": 35, "name": "Power_4", "desc": "Power Consumption 4", "unit": "W"},
            {"address": 36, "name": "Energy_4", "desc": "Energy Consumption 4", "unit": "kWh"},
            {"address": 37, "name": "Frequency_4", "desc": "Frequency Measurement 4", "unit": "Hz"},
            {"address": 38, "name": "Speed_4", "desc": "Motor Speed 4", "unit": "RPM"},
            {"address": 39, "name": "Flow_4", "desc": "Flow Rate 4", "unit": "L/m"},
            {"address": 40, "name": "Temperature_5", "desc": "Temperature Sensor 5 Reading", "unit": "°C"},
            {"address": 41, "name": "Humidity_5", "desc": "Humidity Sensor 5 Reading", "unit": "%"},
            {"address": 42, "name": "Pressure_5", "desc": "Pressure Sensor 5 Reading", "unit": "Pa"},
            {"address": 43, "name": "Voltage_5", "desc": "Voltage Measurement 5", "unit": "V"},
            {"address": 44, "name": "Current_5", "desc": "Current Measurement 5", "unit": "A"}
        ]

        # v2.5.2 FIX: Batch processing to prevent DRAM exhaustion
        BATCH_SIZE = 10
        DELAY_BETWEEN_COMMANDS = 0.15  # 150ms between commands in same batch
        DELAY_BETWEEN_BATCHES = 3.0    # 3 seconds between batches for DRAM recovery

        total_registers = len(registers)

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
                f"Creating Register {idx}/{total_registers}: {reg['name']} (Address: {reg['address']})"
            )

            # v2.5.2 FIX: Batch processing with recovery pauses
            await asyncio.sleep(DELAY_BETWEEN_COMMANDS)

            # Every BATCH_SIZE commands, pause for DRAM recovery
            if idx % BATCH_SIZE == 0 and idx < total_registers:
                batch_num = idx // BATCH_SIZE
                total_batches = (total_registers + BATCH_SIZE - 1) // BATCH_SIZE
                print(f"\n[BATCH {batch_num}/{total_batches}] Pausing {DELAY_BETWEEN_BATCHES}s for DRAM recovery...")
                await asyncio.sleep(DELAY_BETWEEN_BATCHES)

        print("\n" + "="*70)
        print("  SUMMARY")
        print("="*70)
        print(f"Device Name:      TCP_Device_45_Regs")
        print(f"Device ID:        {self.device_id}")
        print(f"Protocol:         Modbus TCP")
        print(f"IP Address:       192.168.1.8")
        print(f"Port:             502")
        print(f"Slave ID:         1")
        print(f"Refresh Rate:     1000 ms")
        print(f"\nRegisters Created: 45")
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
        print("[INFO] Device and 45 registers created on GATEWAY")

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
