#!/usr/bin/env python3
"""
=============================================================================
SRT-MGATE-1210 Testing Program
Create 1 TCP Device + 5 Registers
=============================================================================
Device: TCP Device (10.21.239.9:502)
Slave ID: 1
Registers: 5 Input Registers (INT16) - Temperature Zones 1-5
=============================================================================
Author: Kemal - SURIOTA R&D Team
Date: 2025-11-21
Firmware: SRT-MGATE-1210 v2.3.0
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

                    # Compact response logging
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
        print("  SRT-MGATE-1210 TESTING: CREATE 1 TCP DEVICE + 5 REGISTERS")
        print("="*70)

        # =============================================================================
        # IMPORTANT WARNING
        # =============================================================================
        print("\n" + "!"*70)
        print("  IMPORTANT: BEFORE RUNNING THIS SCRIPT")
        print("!"*70)
        print("  1. START Modbus TCP Slave Simulator FIRST")
        print("     Location: Testing/Modbus_Simulators/TCP_Slave/")
        print("     Command:  python modbus_slave_5_registers.py")
        print("")
        print("  2. This prevents TCP polling errors during device creation")
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
        # STEP 1: Create TCP Device
        # =============================================================================
        print("\n>>> STEP 1: Creating TCP Device...")

        device_config = {
            "op": "create",
            "type": "device",
            "device_id": None,
            "config": {
                "device_name": "TCP_Device_5Regs",
                "protocol": "TCP",
                "slave_id": 1,
                "timeout": 3000,
                "retry_count": 3,
                "refresh_rate_ms": 1000,
                "ip": "192.168.1.6",
                "port": 502
            }
        }

        success = await self.send_command(device_config, "Creating TCP Device: TCP_Device_5Regs")

        if not success or not self.device_id:
            print("[ERROR] Device creation failed. Aborting...")
            return

        print(f"\n[SUCCESS] Device created: {self.device_id}")
        await asyncio.sleep(2)

        # =============================================================================
        # STEP 2: Create 5 Registers (Temperature Zones 1-5)
        # =============================================================================
        print(f"\n>>> STEP 2: Creating 5 Registers for Device ID: {self.device_id}")
        print("[INFO] Using 1.5 second delay between registers to prevent BLE packet loss")

        # Temperature Zones 1-5 (addresses 0-4) - ALIGNED with 50 register layout
        registers = []
        for i in range(5):
            registers.append({
                "address": i,
                "name": f"Temp_Zone_{i+1}",
                "desc": f"Temperature Zone {i+1}",
                "unit": "°C"
            })

        # Create all 5 registers with retry mechanism
        success_count = 0
        failed_count = 0
        failed_registers = []

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

            print(f"\n[{idx}/5] {reg['name']} (Addr: {reg['address']})", end=" ")
            success = await self.send_command(register_config, "")

            if success:
                success_count += 1
                print(f"✓")
            else:
                failed_count += 1
                failed_registers.append(reg)
                print(f"✗ FAILED")

            # Increased delay to prevent BLE packet loss
            await asyncio.sleep(1.5)

        # =============================================================================
        # STEP 3: Summary
        # =============================================================================
        print("\n" + "="*70)
        print("  SUMMARY")
        print("="*70)
        print(f"Device Name:      TCP_Device_5Regs")
        print(f"Device ID:        {self.device_id}")
        print(f"Protocol:         Modbus TCP")
        print(f"IP Address:       10.21.239.9")
        print(f"Port:             502")
        print(f"Slave ID:         1")
        print(f"Timeout:          3000 ms")
        print(f"Refresh Rate:     5000 ms")
        print(f"\nRegister Creation Results:")
        print(f"  Total Attempted:  5")
        print(f"  Success:          {success_count} ✓")
        print(f"  Failed:           {failed_count} ✗")

        if success_count == 5:
            print(f"\n  STATUS: ALL REGISTERS CREATED SUCCESSFULLY! 🎉")
        elif success_count >= 4:
            print(f"\n  STATUS: Nearly complete ({success_count}/5)")
        else:
            print(f"\n  STATUS: Incomplete - many failures")

        if failed_registers:
            print(f"\n  Failed Registers:")
            for reg in failed_registers:
                print(f"    - {reg['name']} (Addr: {reg['address']})")
            print(f"\n  [TIP] Re-run the script to retry failed registers")
            print(f"  [TIP] Or manually create them via BLE app")

        print(f"\nRegister Layout (Temperature Zones 1-5):")
        print(f"  Temp_Zone_1:  Address 0  (°C)")
        print(f"  Temp_Zone_2:  Address 1  (°C)")
        print(f"  Temp_Zone_3:  Address 2  (°C)")
        print(f"  Temp_Zone_4:  Address 3  (°C)")
        print(f"  Temp_Zone_5:  Address 4  (°C)")
        print("="*70)

        if success_count == 5:
            print("\n[INFO] Expected Gateway behavior:")
            print("  - TCP polling time: ~1-2 seconds for 5 registers")
            print("  - Batch completion: All 5 registers attempted")
            print("  - MQTT payload: ~250-350 bytes")
            print("  - Publish interval: Every 5-10 seconds")
            print("="*70)

# =============================================================================
# Main Execution
# =============================================================================
async def main():
    client = DeviceCreationClient()

    try:
        print("\n" + "="*70)
        print("  SRT-MGATE-1210 Firmware Testing - TCP Mode (5 Registers)")
        print("  Python BLE Device Creation Client")
        print("="*70)
        print("  Version:    2.0.0")
        print("  Date:       2025-11-21")
        print("  Firmware:   SRT-MGATE-1210 v2.3.0")
        print("  Author:     Kemal - SURIOTA R&D Team")
        print("="*70)

        if not await client.connect():
            return

        await client.create_device_and_registers()

        print("\n[SUCCESS] Program completed")
        print("\n[NEXT STEPS]")
        print("  1. Monitor Gateway serial output for TCP polling logs")
        print("  2. Verify batch completion: (X success, Y failed, 5/5 total)")
        print("  3. Check MQTT broker for published payload (~250-350 bytes)")
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
