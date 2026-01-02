#!/usr/bin/env python3
"""
Quick OTA Test Script - Set token then check update
"""

import asyncio
import json
import sys
import os
from bleak import BleakClient, BleakScanner

# BLE Configuration
SERVICE_UUID = "00001830-0000-1000-8000-00805f9b34fb"
COMMAND_CHAR_UUID = "11111111-1111-1111-1111-111111111101"
RESPONSE_CHAR_UUID = "11111111-1111-1111-1111-111111111102"
SERVICE_NAME = "SURIOTA GW"

# GitHub Token (Fine-grained PAT for GatewaySuriotaOTA repo)
# IMPORTANT: Set your token here or via environment variable
GITHUB_TOKEN = os.environ.get("GITHUB_OTA_TOKEN", "YOUR_GITHUB_TOKEN_HERE")

# Global
response_buffer = []
response_complete = False


def notification_handler(sender, data):
    global response_buffer, response_complete
    try:
        chunk = data.decode("utf-8")
        if chunk == "<END>":
            response_complete = True
            print(" [END]")
        else:
            response_buffer.append(chunk)
            print(".", end="", flush=True)
    except Exception as e:
        print(f"\nError: {e}")


async def send_command(client, command_dict, timeout=60):
    global response_buffer, response_complete
    response_buffer = []
    response_complete = False

    command_str = json.dumps(command_dict, separators=(",", ":"))
    print(f"\n>> Sending: {command_str[:80]}{'...' if len(command_str) > 80 else ''}")
    print("   Receiving: ", end="", flush=True)

    # Fragment and send (18 bytes chunks)
    chunk_size = 18
    for i in range(0, len(command_str), chunk_size):
        chunk = command_str[i : i + chunk_size]
        await client.write_gatt_char(COMMAND_CHAR_UUID, chunk.encode("utf-8"))
        await asyncio.sleep(0.1)

    # Send end marker
    await client.write_gatt_char(COMMAND_CHAR_UUID, "<END>".encode("utf-8"))

    # Wait for response
    elapsed = 0
    while not response_complete and elapsed < timeout:
        await asyncio.sleep(0.5)
        elapsed += 0.5
        if elapsed % 10 == 0:
            print(f" ({int(elapsed)}s)", end="", flush=True)

    if response_complete:
        full_response = "".join(response_buffer)
        print(f"\n<< Response ({len(full_response)} bytes):")
        try:
            parsed = json.loads(full_response)
            print(json.dumps(parsed, indent=2))
            return parsed
        except:
            print(full_response[:500])
            return {"raw": full_response}
    else:
        print("\n!! TIMEOUT - no response received")
        return None


async def main():
    print("=" * 60)
    print("OTA Quick Test - Set Token & Check Update")
    print("=" * 60)

    # Scan
    print("\n[1/5] Scanning for device...")
    devices = await BleakScanner.discover(timeout=10.0)
    device = None
    for d in devices:
        if d.name == SERVICE_NAME:
            device = d
            print(f"      Found: {d.name} ({d.address})")
            break

    if not device:
        print("      ERROR: Device not found!")
        return

    # Connect
    print("\n[2/5] Connecting...")
    client = BleakClient(device.address, timeout=15.0)
    await client.connect()

    if not client.is_connected:
        print("      ERROR: Failed to connect!")
        return

    print("      Connected!")
    await client.start_notify(RESPONSE_CHAR_UUID, notification_handler)
    await asyncio.sleep(1)

    try:
        # Step 1: Set GitHub Token
        print("\n[3/5] Setting GitHub token...")
        result = await send_command(
            client,
            {"op": "ota", "type": "set_github_token", "token": GITHUB_TOKEN},
            timeout=30,
        )

        if not result or result.get("status") != "ok":
            print("      WARNING: Token set may have failed, continuing anyway...")
        else:
            print("      Token set successfully!")

        # Wait longer for token to be processed
        print("\n      Waiting 5 seconds for token to be applied...")
        await asyncio.sleep(5)

        # Step 2: Get OTA config to verify state
        print("\n[4/6] Getting OTA config...")
        config_result = await send_command(
            client, {"op": "ota", "type": "get_config"}, timeout=30
        )

        # Step 3: DIRECTLY start_update (skip check_update)
        # This will internally call checkForUpdate() if needed
        print("\n[5/6] Starting update directly (will check internally)...")
        print("      This will fetch manifest and download firmware...")
        update_result = await send_command(
            client, {"op": "ota", "type": "start_update"}, timeout=180
        )

        if update_result and update_result.get("status") == "ok":
            print("\n      Download complete! Apply update...")
            await asyncio.sleep(2)

            apply_result = await send_command(
                client, {"op": "ota", "type": "apply_update"}, timeout=30
            )
            print("\n      Device will reboot now!")
        else:
            print(f"\n      Update failed or no update available")
            if update_result:
                print(f"      Status: {update_result.get('status')}")
                print(f"      Error code: {update_result.get('error_code')}")
                print(f"      Error message: {update_result.get('error_message')}")

    finally:
        try:
            await client.stop_notify(RESPONSE_CHAR_UUID)
            await client.disconnect()
            print("\n[Done] Disconnected")
        except:
            pass


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n\nInterrupted by user")
