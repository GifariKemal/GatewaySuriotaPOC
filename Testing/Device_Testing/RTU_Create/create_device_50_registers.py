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
        self.last_response = None
        self.response_received = False

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
                    self.last_response = response
                    self.response_received = True

                    # Compact response logging (just status + device_id/register_id)
                    status = response.get('status', 'unknown')
                    if status == 'ok':
                        if 'device_id' in response:
                            self.device_id = response['device_id']
                            print(f"[OK] Device created: {self.device_id}")
                        elif 'data' in response and 'register_name' in response['data']:
                            reg_name = response['data']['register_name']
                            print(f"[OK] Register created: {reg_name}")
                        else:
                            print(f"[OK] Operation successful")
                    else:
                        error_msg = response.get('error', 'Unknown error')
                        print(f"[ERROR] Operation failed: {error_msg}")

                except json.JSONDecodeError as e:
                    print(f"[ERROR] JSON Parse: {e}")
                    self.response_received = True
                    self.last_response = {"status": "error", "error": str(e)}
                finally:
                    self.response_buffer = ""
        else:
            self.response_buffer += fragment

    async def send_command(self, command, description="", max_retries=3):
        if not self.connected:
            raise RuntimeError("Not connected to BLE device")

        json_str = json.dumps(command, separators=(',', ':'))

        if description:
            print(f"\n[COMMAND] {description}")

        for attempt in range(1, max_retries + 1):
            try:
                # Reset response tracking
                self.response_received = False
                self.last_response = None

                # Send command
                await self.client.write_gatt_char(COMMAND_CHAR_UUID, json_str.encode())
                await self.client.write_gatt_char(COMMAND_CHAR_UUID, "<END>".encode())

                # Wait for response (max 5 seconds)
                for _ in range(50):  # 50 * 0.1s = 5 seconds
                    if self.response_received:
                        if self.last_response and self.last_response.get('status') == 'ok':
                            return True  # Success
                        else:
                            print(f"[RETRY] Attempt {attempt}/{max_retries} failed")
                            break
                    await asyncio.sleep(0.1)

                # Timeout - retry
                if not self.response_received:
                    print(f"[TIMEOUT] No response after 5s (attempt {attempt}/{max_retries})")

                if attempt < max_retries:
                    await asyncio.sleep(1.0)  # Wait before retry

            except Exception as e:
                print(f"[ERROR] Send failed: {e}")
                if attempt < max_retries:
                    await asyncio.sleep(1.0)

        print(f"[FAILED] Command failed after {max_retries} attempts")
        return False

    async def create_device_and_registers(self):
        print("\n" + "="*70)
        print("  SRT-MGATE-1210 TESTING: CREATE 1 RTU DEVICE + 50 REGISTERS")
        print("="*70)

        # =============================================================================
        # IMPORTANT WARNING
        # =============================================================================
        print("\n" + "!"*70)
        print("  IMPORTANT: BEFORE RUNNING THIS SCRIPT")
        print("!"*70)
        print("  1. START Modbus RTU Slave Simulator FIRST")
        print("     Location: Testing/Modbus_Simulators/RTU_Slave/")
        print("     Command:  python modbus_slave_50_registers.py")
        print("")
        print("  2. This prevents RTU polling errors during device creation")
        print("     which can cause DRAM warnings and BLE packet loss")
        print("")
        print("  3. Wait 10 seconds after starting the simulator before")
        print("     running this script")
        print("!"*70)

        user_input = input("\nHave you started the Modbus slave simulator? (yes/no): ")
        if user_input.lower() != 'yes':
            print("\n[ABORTED] Please start the Modbus slave simulator first")
            return

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

        success = await self.send_command(device_config, "Creating RTU Device: RTU_Device_50Regs")

        if not success or not self.device_id:
            print("[ERROR] Device creation failed. Aborting...")
            return

        print(f"\n[SUCCESS] Device created: {self.device_id}")
        await asyncio.sleep(2)

        # =============================================================================
        # STEP 2: Create 50 Registers
        # =============================================================================
        print(f"\n>>> STEP 2: Creating 50 Registers for Device ID: {self.device_id}")
        print("[INFO] Using batch processing with DRAM recovery pauses (v2.5.2 fix)")

        # Generate 50 registers with diverse sensor types
        registers = []

        # Temperature sensors (0-9)
        for i in range(10):
            registers.append({
                "address": i,
                "name": f"Temp_Zone_{i+1}",
                "desc": f"Temperature Zone {i+1}",
                "unit": "degC"  # FIXED: Changed from °C to avoid UTF-8 encoding issues
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

        # Create all 50 registers with retry mechanism
        # v2.5.2 FIX: Batch processing to prevent DRAM exhaustion
        # Process in batches of 10 with recovery pause between batches
        BATCH_SIZE = 10
        DELAY_BETWEEN_COMMANDS = 0.15  # 150ms between commands in same batch
        DELAY_BETWEEN_BATCHES = 3.0    # 3 seconds between batches for DRAM recovery

        success_count = 0
        failed_count = 0
        failed_registers = []

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

            print(f"\n[{idx}/{total_registers}] {reg['name']} (Addr: {reg['address']})", end=" ")
            success = await self.send_command(register_config, "")

            if success:
                success_count += 1
                print(f"✓")
            else:
                failed_count += 1
                failed_registers.append(reg)
                print(f"✗ FAILED")

            # v2.5.2 FIX: Batch processing with recovery pauses
            # Short delay between commands in same batch
            await asyncio.sleep(DELAY_BETWEEN_COMMANDS)

            # Every BATCH_SIZE commands, pause for DRAM recovery
            if idx % BATCH_SIZE == 0 and idx < total_registers:
                batch_num = idx // BATCH_SIZE
                total_batches = (total_registers + BATCH_SIZE - 1) // BATCH_SIZE
                print(f"\n[BATCH {batch_num}/{total_batches}] Pausing {DELAY_BETWEEN_BATCHES}s for DRAM recovery...")
                await asyncio.sleep(DELAY_BETWEEN_BATCHES)

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
        print(f"Refresh Rate:     10000 ms")
        print(f"\nRegister Creation Results:")
        print(f"  Total Attempted:  50")
        print(f"  Success:          {success_count} ✓")
        print(f"  Failed:           {failed_count} ✗")

        if success_count == 50:
            print(f"\n  STATUS: ALL REGISTERS CREATED SUCCESSFULLY! 🎉")
        elif success_count >= 45:
            print(f"\n  STATUS: Nearly complete ({success_count}/50)")
        else:
            print(f"\n  STATUS: Incomplete - many failures")

        if failed_registers:
            print(f"\n  Failed Registers:")
            for reg in failed_registers:
                print(f"    - {reg['name']} (Addr: {reg['address']})")
            print(f"\n  [TIP] Re-run the script to retry failed registers")
            print(f"  [TIP] Or manually create them via BLE app")

        print(f"\nRegister Layout:")
        print(f"  Temperature Zones:  0-9   (10 sensors)")
        print(f"  Humidity Zones:     10-19 (10 sensors)")
        print(f"  Pressure Sensors:   20-24 (5 sensors)")
        print(f"  Voltage Lines:      25-29 (5 sensors)")
        print(f"  Current Lines:      30-34 (5 sensors)")
        print(f"  Power Meters:       35-39 (5 sensors)")
        print(f"  Energy Counters:    40-44 (5 sensors)")
        print(f"  Flow Meters:        45-49 (5 sensors)")
        print("="*70)

        if success_count == 50:
            print("\n[INFO] Expected Gateway behavior:")
            print("  - RTU polling time: ~8-10 seconds for 50 registers")
            print("  - Batch completion: All 50 registers attempted")
            print("  - MQTT payload: ~2.5-3.0 KB")
            print("  - Publish interval: Every 20-25 seconds")
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

        print("\n[SUCCESS] Program completed")
        print("\n[NEXT STEPS]")
        print("  1. Monitor Gateway serial output for RTU polling logs")
        print("  2. Verify batch completion: (X success, Y failed, 50/50 total)")
        print("  3. Check MQTT broker for published payload (~2.5-3.0 KB)")
        print("  4. If some registers failed, re-run this script (it will retry)")
        print("\n[TROUBLESHOOTING]")
        print("  - Low DRAM warnings: Modbus slave should be running")
        print("  - BLE packet loss: Try increasing delay in script")
        print("  - Polling errors: Check slave simulator is running correctly")

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
