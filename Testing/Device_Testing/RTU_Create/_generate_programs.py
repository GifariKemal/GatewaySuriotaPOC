#!/usr/bin/env python3
"""
Script to generate RTU device creation programs for different register counts
Author: Kemal - SURIOTA R&D Team
Date: 2025-11-17
"""

import os

TEMPLATE = '''#!/usr/bin/env python3
"""
=============================================================================
SRT-MGATE-1210 Testing Program
Create 1 RTU Device + {num_regs} Registers
=============================================================================
Device: RTU Device (COM8, 9600 baud, 8N1)
Slave ID: 1
Registers: {num_regs} Input Registers (INT16)
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
            print(f"[SCAN] Scanning for '{{SERVICE_NAME}}'...")
            devices = await BleakScanner.discover(timeout=10.0)
            device = next((d for d in devices if d.name == SERVICE_NAME), None)

            if not device:
                print(f"[ERROR] Service '{{SERVICE_NAME}}' not found")
                return False

            print(f"[FOUND] {{device.name}} ({{device.address}})")
            self.client = BleakClient(device.address)
            await self.client.connect()
            await self.client.start_notify(RESPONSE_CHAR_UUID, self._notification_handler)

            self.connected = True
            print(f"[SUCCESS] Connected to {{device.name}}")
            return True

        except Exception as e:
            print(f"[ERROR] Connection failed: {{e}}")
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
                    print(f"[RESPONSE] {{json.dumps(response, indent=2)}}")

                    if response.get('status') == 'ok' and 'device_id' in response:
                        self.device_id = response['device_id']
                        print(f"[CAPTURE] Device ID: {{self.device_id}}")

                except json.JSONDecodeError as e:
                    print(f"[ERROR] JSON Parse: {{e}}")
                finally:
                    self.response_buffer = ""
        else:
            self.response_buffer += fragment

    async def send_command(self, command, description=""):
        if not self.connected:
            raise RuntimeError("Not connected to BLE device")

        json_str = json.dumps(command, separators=(',', ':'))

        if description:
            print(f"\\n[COMMAND] {{description}}")
        print(f"[DEBUG] Payload ({{len(json_str)}} bytes): {{json_str}}")

        # Send entire command at once (MTU is 517 bytes - no chunking needed)
        await self.client.write_gatt_char(COMMAND_CHAR_UUID, json_str.encode())
        await self.client.write_gatt_char(COMMAND_CHAR_UUID, "<END>".encode())
        await asyncio.sleep(2.0)

    async def create_device_and_registers(self):
        print("\\n" + "="*70)
        print("  SRT-MGATE-1210 TESTING: CREATE 1 RTU DEVICE + {num_regs} REGISTERS")
        print("="*70)

        print("\\n>>> STEP 1: Creating RTU Device...")

        device_config = {{
            "op": "create",
            "type": "device",
            "device_id": None,
            "config": {{
                "device_name": "RTU_Device_{num_regs}_Regs",
                "protocol": "RTU",
                "slave_id": 1,
                "timeout": 5000,
                "retry_count": 3,
                "refresh_rate_ms": 2000,
                "serial_port": 1,
                "baud_rate": 9600,
                "data_bits": 8,
                "parity": "None",
                "stop_bits": 1
            }}
        }}

        await self.send_command(device_config, "Creating RTU Device: RTU_Device_{num_regs}_Regs")
        await asyncio.sleep(2)

        if not self.device_id:
            print("[ERROR] Device ID not captured. Aborting...")
            return

        print(f"\\n>>> STEP 2: Creating {num_regs} Registers for Device ID: {{self.device_id}}")

        registers = [
{registers_list}
        ]

        for idx, reg in enumerate(registers, 1):
            register_config = {{
                "op": "create",
                "type": "register",
                "device_id": self.device_id,
                "config": {{
                    "address": reg["address"],
                    "register_name": reg["name"],
                    "type": "Input Registers",
                    "function_code": 4,
                    "data_type": "INT16",
                    "description": reg["desc"],
                    "unit": reg["unit"],
                    "scale": 1.0,
                    "offset": 0.0
                }}
            }}

            await self.send_command(
                register_config,
                f"Creating Register {{idx}}/{num_regs}: {{reg['name']}} (Address: {{reg['address']}})"
            )
            await asyncio.sleep(0.5)

        print("\\n" + "="*70)
        print("  SUMMARY")
        print("="*70)
        print(f"Device Name:      RTU_Device_{num_regs}_Regs")
        print(f"Device ID:        {{self.device_id}}")
        print(f"Protocol:         Modbus RTU")
        print(f"Serial Port:      COM8 (Port 1)")
        print(f"Baud Rate:        9600")
        print(f"Slave ID:         1")
        print(f"Timeout:          5000 ms")
        print(f"Refresh Rate:     2000 ms")
        print(f"\\nRegisters Created: {num_regs}")
        print("="*70)

async def main():
    client = DeviceCreationClient()

    try:
        print("\\n" + "="*70)
        print("  SRT-MGATE-1210 Firmware Testing - RTU Mode ({num_regs} Registers)")
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

        print("\\n[SUCCESS] Program completed successfully")
        print("[INFO] Device and {num_regs} registers created on GATEWAY")

    except KeyboardInterrupt:
        print("\\n[INTERRUPT] Program interrupted by user")
    except Exception as e:
        print(f"[ERROR] Unexpected error: {{e}}")
        import traceback
        traceback.print_exc()
    finally:
        await client.disconnect()

if __name__ == "__main__":
    asyncio.run(main())
'''

def generate_programs():
    """Generate RTU programs for different register counts"""

    # Generate for 15, 20, 25, 30, 35, 40, 45 (we already have 5, 10, 50)
    for num_regs in [15, 20, 25, 30, 35, 40, 45]:
        print(f"Generating RTU program for {num_regs} registers...")

        # Generate registers list
        registers_lines = []
        for i in range(num_regs):
            line = f'            {{"address": {i}, "name": "Register_{i+1}", "desc": "Data Point {i+1}", "unit": "unit"}}'
            if i < num_regs - 1:
                line += ','
            registers_lines.append(line)

        registers_list = '\\n'.join(registers_lines)

        # Create file content
        content = TEMPLATE.format(num_regs=num_regs, registers_list=registers_list)

        # Write file
        filename = f'create_device_{num_regs}_registers.py'
        with open(filename, 'w', encoding='utf-8') as f:
            f.write(content)

        # Make executable (Unix-like)
        try:
            os.chmod(filename, 0o755)
        except:
            pass

        print(f"  [OK] Created: {filename}")

    print("\\n[OK] All RTU programs created successfully!")
    print("\\nCreated files:")
    for num_regs in [15, 20, 25, 30, 35, 40, 45]:
        print(f"  - create_device_{num_regs}_registers.py")

if __name__ == "__main__":
    print("="*70)
    print("RTU Device Creation Programs Generator")
    print("="*70)
    print("Author: Kemal - SURIOTA R&D Team")
    print("Date: 2025-11-17")
    print("="*70)
    print()

    generate_programs()
