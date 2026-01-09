#!/usr/bin/env python3
"""
BLE Writable Registers Test
============================
Version: 1.1.0

Test script for the 'read writable_registers' BLE command.
This command returns all registers with writable=true, grouped by device.

Usage:
    python test_ble_writable_registers.py

Requirements:
    pip install bleak asyncio

Author: SURIOTA R&D Team
"""

import asyncio
import json
import sys
import struct

try:
    from bleak import BleakClient, BleakScanner
except ImportError:
    print("ERROR: bleak not installed. Run: pip install bleak")
    sys.exit(1)

# BLE UUIDs for SRT-MGATE-1210
SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
TX_CHAR_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8"  # Write to gateway
RX_CHAR_UUID = "beb5483f-36e1-4688-b7f5-ea07361b26a8"  # Read from gateway

# Fragmentation protocol
FRAGMENT_START = 0x01
FRAGMENT_CONTINUE = 0x02
FRAGMENT_END = 0x03
FRAGMENT_SINGLE = 0x04


class BLEGatewayClient:
    """BLE client for communicating with SRT-MGATE-1210 gateway."""

    def __init__(self):
        self.client = None
        self.response_buffer = bytearray()
        self.response_complete = asyncio.Event()
        self.response_data = None

    async def scan_for_gateway(self, timeout: float = 10.0) -> str:
        """Scan for gateway device."""
        print(f"\nScanning for MGate-1210 devices ({timeout}s)...")

        devices = await BleakScanner.discover(timeout=timeout)

        gateways = []
        for device in devices:
            name = device.name or ""
            if "MGate" in name or "MGATE" in name:
                gateways.append(device)
                print(f"  Found: {name} ({device.address})")

        if not gateways:
            print("  No MGate devices found")
            return None

        # Return first gateway found
        return gateways[0].address

    async def connect(self, address: str) -> bool:
        """Connect to gateway."""
        try:
            print(f"\nConnecting to {address}...")
            self.client = BleakClient(address)
            await self.client.connect()

            if self.client.is_connected:
                print("[OK] Connected")

                # Subscribe to notifications
                await self.client.start_notify(RX_CHAR_UUID, self._on_notification)
                print("[OK] Subscribed to notifications")
                return True
            return False

        except Exception as e:
            print(f"[ERROR] Connection failed: {e}")
            return False

    async def disconnect(self):
        """Disconnect from gateway."""
        if self.client and self.client.is_connected:
            await self.client.disconnect()
            print("[OK] Disconnected")

    def _on_notification(self, sender, data: bytearray):
        """Handle incoming BLE notifications (fragmented responses)."""
        if len(data) < 1:
            return

        fragment_type = data[0]
        payload = data[1:]

        if fragment_type == FRAGMENT_SINGLE:
            # Single packet response
            self.response_data = payload.decode('utf-8')
            self.response_complete.set()

        elif fragment_type == FRAGMENT_START:
            # Start of multi-packet response
            self.response_buffer = bytearray(payload)

        elif fragment_type == FRAGMENT_CONTINUE:
            # Continue multi-packet response
            self.response_buffer.extend(payload)

        elif fragment_type == FRAGMENT_END:
            # End of multi-packet response
            self.response_buffer.extend(payload)
            self.response_data = self.response_buffer.decode('utf-8')
            self.response_complete.set()

    async def send_command(self, command: dict, timeout: float = 10.0) -> dict:
        """Send BLE command and wait for response."""
        if not self.client or not self.client.is_connected:
            return {"status": "error", "error": "Not connected"}

        # Reset response state
        self.response_complete.clear()
        self.response_data = None
        self.response_buffer = bytearray()

        # Serialize command
        cmd_json = json.dumps(command, separators=(',', ':'))
        cmd_bytes = cmd_json.encode('utf-8')

        print(f"\n[TX] Sending command:")
        print(f"     {json.dumps(command, indent=2)}")

        # Send command (fragmented if needed)
        mtu = 512  # Default MTU
        max_payload = mtu - 3  # Account for header

        if len(cmd_bytes) <= max_payload:
            # Single packet
            packet = bytes([FRAGMENT_SINGLE]) + cmd_bytes
            await self.client.write_gatt_char(TX_CHAR_UUID, packet)
        else:
            # Multi-packet
            offset = 0
            first = True
            while offset < len(cmd_bytes):
                chunk = cmd_bytes[offset:offset + max_payload]
                offset += len(chunk)

                if first:
                    packet = bytes([FRAGMENT_START]) + chunk
                    first = False
                elif offset >= len(cmd_bytes):
                    packet = bytes([FRAGMENT_END]) + chunk
                else:
                    packet = bytes([FRAGMENT_CONTINUE]) + chunk

                await self.client.write_gatt_char(TX_CHAR_UUID, packet)
                await asyncio.sleep(0.05)  # Small delay between packets

        # Wait for response
        try:
            await asyncio.wait_for(self.response_complete.wait(), timeout=timeout)

            if self.response_data:
                response = json.loads(self.response_data)
                print(f"\n[RX] Response received:")
                print(f"     {json.dumps(response, indent=2)}")
                return response

        except asyncio.TimeoutError:
            print(f"\n[TIMEOUT] No response within {timeout}s")
            return {"status": "error", "error": "Timeout"}

        except json.JSONDecodeError as e:
            print(f"\n[ERROR] Invalid JSON response: {e}")
            return {"status": "error", "error": f"Invalid JSON: {e}"}

        return {"status": "error", "error": "Unknown error"}


async def test_writable_registers(client: BLEGatewayClient):
    """Test the 'read writable_registers' command."""
    print("\n" + "=" * 60)
    print("   TEST: read writable_registers")
    print("=" * 60)

    command = {
        "action": "read",
        "resource": "writable_registers"
    }

    response = await client.send_command(command, timeout=15.0)

    # Analyze response
    print("\n" + "-" * 60)
    print("ANALYSIS:")
    print("-" * 60)

    if response.get("status") == "ok":
        total_writable = response.get("total_writable", 0)
        total_mqtt = response.get("total_mqtt_enabled", 0)
        devices = response.get("devices", [])

        print(f"[OK] Command successful")
        print(f"  Total writable registers: {total_writable}")
        print(f"  MQTT subscribe enabled: {total_mqtt}")
        print(f"  Devices with writable registers: {len(devices)}")

        if devices:
            print("\n  Device breakdown:")
            for device in devices:
                device_id = device.get("device_id", "?")
                device_name = device.get("device_name", "?")
                registers = device.get("registers", [])
                print(f"\n  [{device_id}] {device_name}")
                print(f"    Writable registers: {len(registers)}")

                for reg in registers:
                    reg_id = reg.get("register_id", "?")
                    reg_name = reg.get("register_name", "?")
                    mqtt_sub = reg.get("mqtt_subscribe", {})
                    mqtt_enabled = mqtt_sub.get("enabled", False)
                    topic_suffix = mqtt_sub.get("topic_suffix", "")

                    mqtt_status = "MQTT enabled" if mqtt_enabled else "MQTT disabled"
                    print(f"      - {reg_name} ({reg_id}): {mqtt_status}")
                    if mqtt_enabled:
                        print(f"        Topic suffix: {topic_suffix}")
                        print(f"        QoS: {mqtt_sub.get('qos', 1)}")
        else:
            print("\n  No devices with writable registers found")
            print("  Hint: Ensure devices have registers with 'writable: true'")

    else:
        error = response.get("error", "Unknown error")
        error_code = response.get("error_code", "N/A")
        print(f"[FAILED] Command failed")
        print(f"  Error: {error}")
        print(f"  Error code: {error_code}")

    return response


async def main():
    print("\n" + "=" * 60)
    print("   BLE Writable Registers Test")
    print("   MQTT Subscribe Control Feature v1.1.0")
    print("=" * 60)

    client = BLEGatewayClient()

    # Scan for gateway
    address = await client.scan_for_gateway()

    if not address:
        print("\nNo gateway found. Make sure:")
        print("  1. Gateway is powered on")
        print("  2. BLE is enabled (button press or always-on mode)")
        print("  3. Device is within BLE range")
        sys.exit(1)

    # Connect
    if not await client.connect(address):
        sys.exit(1)

    try:
        # Run test
        await test_writable_registers(client)

    finally:
        await client.disconnect()

    print("\n" + "=" * 60)
    print("Test completed")
    print("=" * 60)


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nTest cancelled by user")
