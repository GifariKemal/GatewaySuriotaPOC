#!/usr/bin/env python3
"""
Modbus TCP/IP Troubleshooting Script
Automatically tests different configurations via BLE CRUD
Based on: update_test.py reference pattern
"""

import asyncio
import json
from bleak import BleakClient, BleakScanner
import sys


class ModbusTCPFixer:
    def __init__(self):
        self.client = None
        self.response_buffer = ""
        self.last_response = None

    async def connect(self):
        """Connect to SURIOTA Gateway via BLE"""
        print("\n🔍 Scanning for SURIOTA CRUD Service...")
        try:
            devices = await BleakScanner.discover()
            device = next((d for d in devices if d.name and "SURIOTA" in d.name), None)

            if device:
                print(f"✓ Found device: {device.name} ({device.address})")
                self.client = BleakClient(device.address)
                await self.client.connect()

                # Start listening for responses
                await self.client.start_notify(
                    "11111111-1111-1111-1111-111111111102", self._notification_handler
                )
                print("✓ Connected successfully!")
                return True
            else:
                print("✗ SURIOTA device not found")
                print("  Ensure SURIOTA Gateway is powered on")
                print("  Ensure BLE is enabled")
                return False
        except Exception as e:
            print(f"✗ Connection error: {e}")
            return False

    def _notification_handler(self, sender, data):
        """Handle BLE notifications"""
        fragment = data.decode("utf-8")
        if fragment == "<END>":
            try:
                self.last_response = json.loads(self.response_buffer)
                print(f"  📥 Response: {json.dumps(self.last_response, indent=4)}")
            except json.JSONDecodeError:
                print(f"  ❌ Invalid JSON response: {self.response_buffer}")
            self.response_buffer = ""
        else:
            self.response_buffer += fragment

    async def send_command(self, command, label="", wait_time=2.0):
        """Send BLE CRUD command"""
        json_str = json.dumps(command, separators=(",", ":"))

        if label:
            print(f"\n  {label}")

        print(f"  📤 Sending: {json_str[:100]}{'...' if len(json_str) > 100 else ''}")

        try:
            # Send with fragmentation (18 bytes per chunk as per update_test.py)
            chunk_size = 18
            for i in range(0, len(json_str), chunk_size):
                chunk = json_str[i : i + chunk_size]
                await self.client.write_gatt_char(
                    "11111111-1111-1111-1111-111111111101", chunk.encode()
                )
                await asyncio.sleep(0.05)

            # Send end marker
            await self.client.write_gatt_char(
                "11111111-1111-1111-1111-111111111101", "<END>".encode()
            )

            # Wait for response
            await asyncio.sleep(wait_time)
            return True
        except Exception as e:
            print(f"  ❌ Error sending command: {e}")
            return False

    async def wait_for_values(self, duration=30):
        """Wait and monitor for register values"""
        print(f"\n  ⏳ Monitoring for {duration} seconds...")
        print("  Check ESP32 serial monitor for:")
        print("    ✓ D65f89: Flowrate = <value>   (SUCCESS)")
        print("    ✗ D65f89: Flowrate = ERROR     (STILL FAILING)")

        for i in range(duration):
            print(".", end="", flush=True)
            await asyncio.sleep(1)
        print()


async def main():
    print(
        """
╔════════════════════════════════════════════════════════════╗
║     Modbus TCP/IP Troubleshooting & Fix Script             ║
║     Device: DN-150 Flowmeter (D65f89)                      ║
║     Issue: Flowrate = ERROR                                ║
╚════════════════════════════════════════════════════════════╝
    """
    )

    fixer = ModbusTCPFixer()

    # Connect to ESP32
    if not await fixer.connect():
        print("\n❌ Failed to connect. Exiting.")
        return

    print(
        """
╔════════════════════════════════════════════════════════════╗
║              Testing Sequence (Auto)                       ║
║  Total estimated time: ~3 minutes                          ║
╚════════════════════════════════════════════════════════════╝
    """
    )

    # ============================================================
    # STEP 1: Change slave_id from 5 to 1
    # ============================================================
    print("\n" + "=" * 60)
    print("STEP 1/4: Testing slave_id = 1 (changed from 5)")
    print("=" * 60)

    success = await fixer.send_command(
        {
            "op": "update",
            "type": "device",
            "device_id": "D65f89",
            "config": {
                "device_name": "DN-150 Flowmeter",
                "protocol": "TCP",
                "ip": "192.168.1.5",
                "port": 502,
                "slave_id": 1,  # ← CHANGED from 5
                "timeout": 3000,
                "retry_count": 3,
                "refresh_rate_ms": 1000,
            },
        },
        "Sending update command...",
    )

    if success:
        await fixer.wait_for_values(30)
        if fixer.last_response and fixer.last_response.get("success"):
            print("\n  ✓ Device update successful")
        else:
            print("\n  ⚠ Device update returned error")
    else:
        print("\n  ❌ Failed to send command")

    # ============================================================
    # STEP 2: Change function_code from 4 to 3
    # ============================================================
    print("\n" + "=" * 60)
    print("STEP 2/4: Testing function_code = 3 (changed from 4)")
    print("=" * 60)

    success = await fixer.send_command(
        {
            "op": "update",
            "type": "register",
            "device_id": "D65f89",
            "register_id": "Rcf946",
            "config": {
                "address": 4112,
                "register_name": "Flowrate",
                "function_code": 3,  # ← CHANGED from 4
                "data_type": "FLOAT32_LE_BS",
                "refresh_rate_ms": 500,
            },
        },
        "Sending update command...",
    )

    if success:
        await fixer.wait_for_values(30)
    else:
        print("\n  ❌ Failed to send command")

    # ============================================================
    # STEP 3: Test different data types
    # ============================================================
    print("\n" + "=" * 60)
    print("STEP 3/4: Testing data type variants")
    print("=" * 60)

    data_types = ["FLOAT32_BE", "FLOAT32_LE", "FLOAT32_BE_BS"]

    for idx, dt in enumerate(data_types, 1):
        print(f"\n  3.{idx}) Testing data_type = {dt}")

        success = await fixer.send_command(
            {
                "op": "update",
                "type": "register",
                "device_id": "D65f89",
                "register_id": "Rcf946",
                "config": {
                    "address": 4112,
                    "register_name": "Flowrate",
                    "function_code": 3,
                    "data_type": dt,  # ← DIFFERENT
                    "refresh_rate_ms": 500,
                },
            },
            f"Testing {dt}...",
        )

        if success:
            await fixer.wait_for_values(20)
        else:
            print(f"\n    ❌ Failed to send command")

    # ============================================================
    # STEP 4: Summary and next steps
    # ============================================================
    print("\n" + "=" * 60)
    print("STEP 4/4: Test Summary & Recommendations")
    print("=" * 60)

    print(
        """
If you saw values (not ERROR):
  ✓ Configuration is fixed!
  ✓ Note which combination worked:
    - slave_id: 1
    - function_code: 3
    - data_type: [BE/LE/BE_BS/LE_BS]

If you still see "Flowrate = ERROR":
  ✗ Device might be offline
  ✗ Check with: ping 192.168.1.5
  ✗ Check device manual for correct:
    - Slave ID
    - Function Code
    - Register Address
    - Data Type Endianness

Next steps if still failing:
  1. Verify device is powered on and online
  2. Check device IP is correct
  3. Test with different addresses
  4. Enable debug mode in ModbusTcpService.cpp
  5. Use Modbus TCP analyzer tool (Wireshark/etc)
    """
    )

    print("\n" + "=" * 60)
    print("✓ Testing Sequence Complete")
    print("=" * 60 + "\n")

    if fixer.client:
        await fixer.client.disconnect()
        print("✓ Disconnected from SURIOTA Gateway")


async def run_with_error_handling():
    """Run main with error handling"""
    try:
        await main()
    except KeyboardInterrupt:
        print("\n\n⚠ Interrupted by user")
        sys.exit(0)
    except Exception as e:
        print(f"\n❌ Unexpected error: {e}")
        import traceback

        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    print("\n⚠ Requirements:")
    print("  - pip install bleak")
    print("  - SURIOTA Gateway powered on with BLE enabled")
    print("  - Serial monitor open to watch output")

    input("\nPress ENTER to start testing...")

    asyncio.run(run_with_error_handling())
