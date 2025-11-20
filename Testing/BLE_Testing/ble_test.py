#!/usr/bin/env python3
"""
BLE CRUD Testing Tool for SRT-MGATE-1210
=========================================

Interactive BLE testing tool to send JSON commands and receive responses.
Supports all CRUD operations: Create, Read, Update, Delete

Dependencies:
    pip install bleak

Usage:
    python ble_test.py

Author: Claude Code
Version: 1.0.3
"""

import asyncio
import json
import sys
from datetime import datetime
from bleak import BleakClient, BleakScanner

# ============================================================================
# BLE UUIDs Configuration (must match firmware BLEManager.cpp)
# ============================================================================
SERVICE_UUID = "00001830-0000-1000-8000-00805f9b34fb"
COMMAND_CHAR_UUID = "11111111-1111-1111-1111-111111111101"
RESPONSE_CHAR_UUID = "11111111-1111-1111-1111-111111111102"
SERVICE_NAME = "SURIOTA GW"

# ============================================================================
# Global Variables
# ============================================================================
response_buffer = []
response_complete = False


# ============================================================================
# BLE Response Handler
# ============================================================================
def notification_handler(sender, data):
    """Handle incoming BLE notifications (fragmented responses)"""
    global response_buffer, response_complete

    try:
        chunk = data.decode('utf-8')

        # Check for end marker
        if chunk == "<END>":
            response_complete = True
            print("\n[BLE] ✓ Response complete")
        else:
            response_buffer.append(chunk)
            print(f"[BLE] Received chunk ({len(chunk)} bytes)")
    except Exception as e:
        print(f"[BLE] ⚠️  Error decoding notification: {e}")


# ============================================================================
# BLE Connection Functions
# ============================================================================
async def scan_devices():
    """Scan for BLE devices and auto-select first SURIOTA GW"""
    print(f"\n[SCAN] Scanning for '{SERVICE_NAME}'...")
    print("=" * 70)

    devices = await BleakScanner.discover(timeout=10.0)

    if not devices:
        print("No BLE devices found!")
        return None

    # Filter for SURIOTA GW devices
    suriota_devices = [d for d in devices if d.name == SERVICE_NAME]

    if suriota_devices:
        device = suriota_devices[0]  # Auto-select first device
        print(f"✓ Found SURIOTA Gateway")
        print(f"  Name: {device.name}")
        print(f"  Address: {device.address}\n")
        return device
    else:
        print(f"✗ No '{SERVICE_NAME}' device found!")
        return None


async def connect_to_device(address):
    """Connect to BLE device by address"""
    print(f"\n[CONNECT] Connecting to {address}...")

    try:
        client = BleakClient(address, timeout=10.0)
        await client.connect()

        if client.is_connected:
            print("[CONNECT] ✓ Connected successfully!")

            # Enable notifications
            await client.start_notify(RESPONSE_CHAR_UUID, notification_handler)
            print("[CONNECT] ✓ Notifications enabled")

            return client
        else:
            print("[CONNECT] ✗ Failed to connect")
            return None
    except Exception as e:
        print(f"[CONNECT] ✗ Connection error: {e}")
        return None


# ============================================================================
# Command Sending Functions
# ============================================================================
async def send_command(client, command_json):
    """Send JSON command to BLE device with fragmentation"""
    global response_buffer, response_complete

    # Reset response buffer
    response_buffer = []
    response_complete = False

    try:
        # Convert JSON to string
        command_str = json.dumps(command_json, separators=(',', ':'))

        print(f"\n[SEND] Sending command ({len(command_str)} bytes):")
        print(f"       {command_str}")

        # Fragmentation: 18 bytes per chunk (BLE MTU limitation)
        chunk_size = 18
        total_chunks = (len(command_str) + chunk_size - 1) // chunk_size

        print(f"[SEND] Fragmenting into {total_chunks} chunk(s) ({chunk_size} bytes/chunk)...")

        for i in range(0, len(command_str), chunk_size):
            chunk = command_str[i:i+chunk_size]
            chunk_num = (i // chunk_size) + 1

            await client.write_gatt_char(COMMAND_CHAR_UUID, chunk.encode('utf-8'))
            print(f"[SEND] Chunk {chunk_num}/{total_chunks} sent ({len(chunk)} bytes)")
            await asyncio.sleep(0.1)  # Delay for stable transmission

        # Send end marker
        await client.write_gatt_char(COMMAND_CHAR_UUID, "<END>".encode('utf-8'))
        print("[SEND] ✓ End marker <END> sent")

        # Wait for response with timeout
        timeout = 30  # 30 seconds timeout
        elapsed = 0

        while not response_complete and elapsed < timeout:
            await asyncio.sleep(0.1)
            elapsed += 0.1

        if response_complete:
            # Reconstruct full response
            full_response = ''.join(response_buffer)

            print(f"\n[RESPONSE] Received ({len(full_response)} bytes):")
            print("=" * 70)

            try:
                # Try to parse as JSON for pretty printing
                response_json = json.loads(full_response)
                print(json.dumps(response_json, indent=2))
            except:
                # If not valid JSON, print as-is
                print(full_response)

            print("=" * 70)

            return full_response
        else:
            print(f"\n[RESPONSE] ⚠️  Timeout waiting for response ({timeout}s)")
            return None

    except Exception as e:
        print(f"[SEND] ✗ Error sending command: {e}")
        return None


# ============================================================================
# Command Templates
# ============================================================================
def show_command_examples():
    """Show example commands for quick reference"""
    examples = {
        "1. Read all devices (summary)": {
            "op": "read",
            "type": "devices_summary"
        },
        "2. Read all devices with registers (minimal mode)": {
            "op": "read",
            "type": "devices_with_registers",
            "minimal": True
        },
        "3. Read specific device": {
            "op": "read",
            "type": "device",
            "device_id": "device_1",
            "minimal": False
        },
        "4. Read device (minimal mode)": {
            "op": "read",
            "type": "device",
            "device_id": "device_1",
            "minimal": True
        },
        "5. Read registers for device": {
            "op": "read",
            "type": "registers",
            "device_id": "device_1"
        },
        "6. Read server config": {
            "op": "read",
            "type": "server_config"
        },
        "7. Create new device": {
            "op": "create",
            "type": "device",
            "config": {
                "device_name": "Test Device",
                "slave_id": 1,
                "baud_rate": 9600,
                "protocol": "RTU",
                "enabled": True
            }
        },
        "8. Create new register": {
            "op": "create",
            "type": "register",
            "device_id": "device_1",
            "config": {
                "register_name": "Temperature",
                "address": 4112,
                "function_code": 3,
                "data_type": "INT16",
                "scale": 1.0,
                "offset": 0.0,
                "unit": "°C"
            }
        },
        "9. Update device": {
            "op": "update",
            "type": "device",
            "device_id": "device_1",
            "config": {
                "enabled": False
            }
        },
        "10. Delete device": {
            "op": "delete",
            "type": "device",
            "device_id": "device_1"
        },
        "11. Delete register": {
            "op": "delete",
            "type": "register",
            "device_id": "device_1",
            "register_id": "register_1"
        },
        "12. Batch operation (sequential)": {
            "op": "batch",
            "mode": "sequential",
            "commands": [
                {
                    "op": "read",
                    "type": "devices_summary"
                },
                {
                    "op": "read",
                    "type": "server_config"
                }
            ]
        }
    }

    print("\n" + "=" * 70)
    print("COMMAND EXAMPLES")
    print("=" * 70)

    for title, cmd in examples.items():
        print(f"\n{title}:")
        print(json.dumps(cmd, indent=2))

    print("\n" + "=" * 70)


# ============================================================================
# Interactive Mode
# ============================================================================
async def interactive_mode(client):
    """Interactive command input mode"""
    print("\n" + "=" * 70)
    print("INTERACTIVE MODE")
    print("=" * 70)
    print("Enter JSON commands to send to the device.")
    print("Type 'help' to see command examples.")
    print("Type 'quit' or 'exit' to disconnect and quit.")
    print("=" * 70)

    while True:
        try:
            print("\n[INPUT] Enter JSON command:")
            user_input = input("> ").strip()

            if not user_input:
                continue

            # Handle special commands
            if user_input.lower() in ['quit', 'exit', 'q']:
                print("\n[EXIT] Disconnecting...")
                break

            if user_input.lower() in ['help', 'h', '?']:
                show_command_examples()
                continue

            # Try to parse JSON
            try:
                command_json = json.loads(user_input)

                # Send command and get response
                await send_command(client, command_json)

            except json.JSONDecodeError as e:
                print(f"[ERROR] Invalid JSON: {e}")
                print("        Please enter valid JSON or type 'help' for examples")

        except KeyboardInterrupt:
            print("\n\n[EXIT] Interrupted by user")
            break
        except Exception as e:
            print(f"[ERROR] Unexpected error: {e}")


# ============================================================================
# Quick Test Functions
# ============================================================================
async def quick_test(client):
    """Run a quick test sequence"""
    print("\n" + "=" * 70)
    print("QUICK TEST SEQUENCE")
    print("=" * 70)

    tests = [
        ("Read devices summary", {
            "op": "read",
            "type": "devices_summary"
        }),
        ("Read server config", {
            "op": "read",
            "type": "server_config"
        }),
        ("Read devices with registers (minimal)", {
            "op": "read",
            "type": "devices_with_registers",
            "minimal": True
        })
    ]

    for test_name, command in tests:
        print(f"\n{'─' * 70}")
        print(f"Test: {test_name}")
        print(f"{'─' * 70}")

        await send_command(client, command)
        await asyncio.sleep(1)  # Brief delay between tests

    print(f"\n{'=' * 70}")
    print("QUICK TEST COMPLETE")
    print(f"{'=' * 70}")


# ============================================================================
# Main Function
# ============================================================================
async def main():
    """Main function"""
    print("\n" + "=" * 70)
    print("BLE CRUD Testing Tool for SRT-MGATE-1210")
    print("=" * 70)
    print(f"Started: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")

    # Scan and auto-select device
    device = await scan_devices()

    if not device:
        print("\n[EXIT] No device found. Exiting...")
        return

    # Connect to device
    client = await connect_to_device(device.address)

    if not client:
        print("\n[EXIT] Failed to connect. Exiting...")
        return

    try:
        # Ask for mode
        print("\n[MODE] Select mode:")
        print("1. Interactive mode (manual command input)")
        print("2. Quick test (automated test sequence)")

        mode_choice = input("> ").strip()

        if mode_choice == "2":
            await quick_test(client)
        else:
            await interactive_mode(client)

    finally:
        # Disconnect
        if client.is_connected:
            await client.stop_notify(RESPONSE_CHAR_UUID)
            await client.disconnect()
            print("\n[DISCONNECT] ✓ Disconnected from device")

    print(f"\n[EXIT] Session ended: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print("=" * 70)


# ============================================================================
# Entry Point
# ============================================================================
if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n\n[EXIT] Interrupted by user")
        sys.exit(0)
    except Exception as e:
        print(f"\n[FATAL] Unexpected error: {e}")
        sys.exit(1)
