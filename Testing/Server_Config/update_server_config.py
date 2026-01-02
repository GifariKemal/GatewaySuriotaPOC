#!/usr/bin/env python3
"""
=============================================================================
SRT-MGATE-1210 Testing Program
Update Server Configuration via BLE
=============================================================================
Configuration:
  - Ethernet Mode: DHCP (Automatic)
  - WiFi: Enabled (SSID: "KOWI", Password: "@Kremi201")
  - MQTT: Enabled with Default Mode
    - Broker: broker.hivemq.com:1883
    - Publish Interval: 20 seconds
  - HTTP: Disabled
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
# Server Config Update Class
# =============================================================================
class ServerConfigClient:
    """
    BLE Client for updating server configuration
    """

    def __init__(self):
        self.client = None
        self.response_buffer = ""
        self.connected = False

    async def connect(self):
        """
        Scan and connect to SURIOTA Gateway via BLE
        """
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
            await self.client.start_notify(
                RESPONSE_CHAR_UUID, self._notification_handler
            )

            self.connected = True
            print(f"[SUCCESS] Connected to {device.name}")
            return True

        except Exception as e:
            print(f"[ERROR] Connection failed: {e}")
            return False

    async def disconnect(self):
        """
        Disconnect from BLE device
        """
        if self.client and self.connected:
            await self.client.disconnect()
            self.connected = False
            print("[DISCONNECT] Connection closed")

    def _notification_handler(self, sender, data):
        """
        Handle BLE notification responses with fragmentation support
        """
        fragment = data.decode("utf-8")

        if fragment == "<END>":
            if self.response_buffer:
                try:
                    response = json.loads(self.response_buffer)
                    print(f"[RESPONSE] {json.dumps(response, indent=2)}")

                    # Check if server config update was successful
                    if response.get("status") == "ok":
                        print("\n" + "=" * 70)
                        print("✅ SERVER CONFIGURATION UPDATED SUCCESSFULLY!")
                        print("=" * 70)
                        print("\n⚠️  WARNING: Device will restart in 5 seconds...")
                        print("    Please wait for the device to reboot.")
                        print("\n📋 Configuration Summary:")
                        print("    • Communication Mode: Ethernet (DHCP)")
                        print("    • WiFi: Enabled (SSID: KOWI)")
                        print("    • MQTT: Enabled (broker.hivemq.com)")
                        print("    • MQTT Mode: Default Mode (20s interval)")
                        print("    • HTTP: Disabled")
                        print("=" * 70)
                    elif response.get("status") == "error":
                        print("\n" + "=" * 70)
                        print("❌ SERVER CONFIGURATION UPDATE FAILED!")
                        print("=" * 70)
                        error_code = response.get("code", "UNKNOWN")
                        error_msg = response.get("message", "No error message")
                        print(f"    Error Code: {error_code}")
                        print(f"    Error Message: {error_msg}")
                        print("=" * 70)

                except json.JSONDecodeError as e:
                    print(f"[ERROR] JSON Parse: {e}")
                    print(f"[DEBUG] Buffer content: {self.response_buffer}")
                finally:
                    self.response_buffer = ""
        else:
            self.response_buffer += fragment

    async def send_command(self, command, description=""):
        """
        Send JSON command to ESP32-S3 with fragmentation

        Args:
            command (dict): JSON command object
            description (str): Human-readable description for logging
        """
        if not self.connected:
            raise RuntimeError("Not connected to BLE device")

        json_str = json.dumps(command, separators=(",", ":"))

        if description:
            print(f"\n[COMMAND] {description}")
        print(f"[DEBUG] Payload size: {len(json_str)} bytes")

        # Fragmentation: 18 bytes per chunk (BLE MTU limitation)
        chunk_size = 18
        total_chunks = (len(json_str) + chunk_size - 1) // chunk_size
        print(f"[INFO] Sending {total_chunks} fragments...")

        for i in range(0, len(json_str), chunk_size):
            chunk = json_str[i : i + chunk_size]
            chunk_num = (i // chunk_size) + 1
            print(f"[FRAGMENT {chunk_num}/{total_chunks}] {len(chunk)} bytes")
            await self.client.write_gatt_char(COMMAND_CHAR_UUID, chunk.encode())
            await asyncio.sleep(0.1)  # Delay for stable transmission

        # Send end marker
        print(f"[END] Sending termination marker...")
        await self.client.write_gatt_char(COMMAND_CHAR_UUID, "<END>".encode())
        print(f"[WAIT] Waiting for response...")
        await asyncio.sleep(3.0)  # Wait for response

    async def update_server_config(self):
        """
        Main workflow: Update server configuration
        """
        print("\n" + "=" * 70)
        print("  SRT-MGATE-1210 TESTING: UPDATE SERVER CONFIGURATION")
        print("=" * 70)
        print("\n📋 Configuration to be applied:")
        print("    • Communication Mode: Ethernet (DHCP - Automatic IP)")
        print("    • WiFi: Enabled")
        print("        - SSID: KOWI")
        print("        - Password: @Kremi201")
        print("    • MQTT: Enabled")
        print("        - Broker: broker.hivemq.com:1883")
        print("        - Mode: Default Mode")
        print("        - Topic Publish: v1/devices/me/telemetry/gwsrt")
        print("        - Topic Subscribe: v1/devices/me/rpc")
        print("        - Interval: 20 seconds")
        print("        - Client ID: SuriotaGateway-001")
        print("        - Username: ")
        print("        - Keep Alive: 60s")
        print("        - Clean Session: Yes")
        print("        - TLS: No")
        print("    • HTTP: Disabled")
        print("=" * 70)

        # =============================================================================
        # Server Config Update Payload
        # =============================================================================
        server_config = {
            "op": "update",
            "type": "server_config",
            "config": {
                "communication": {"mode": "WiFi"},
                "wifi": {
                    "enabled": True,
                    "ssid": "SURIOTA 5G 25",
                    "password": "Tampan12",
                },
                "ethernet": {
                    "enabled": False,
                    "use_dhcp": True,
                    "static_ip": "",
                    "gateway": "",
                    "subnet": "",
                },
                "protocol": "mqtt",
                "mqtt_config": {
                    "enabled": True,
                    "broker_address": "broker.hivemq.com",
                    "broker_port": 1883,
                    "client_id": "SuriotaGateway-001",
                    "username": "",
                    "password": "",
                    "keep_alive": 60,
                    "clean_session": True,
                    "use_tls": False,
                    "publish_mode": "default",
                    "default_mode": {
                        "enabled": True,
                        "topic_publish": "v1/devices/me/telemetry/gwsrt",
                        "topic_subscribe": "v1/devices/me/rpc",
                        "interval": 20,
                        "interval_unit": "s",
                    },
                    "customize_mode": {"enabled": False, "custom_topics": []},
                },
                "http_config": {
                    "enabled": False,
                    "endpoint_url": "",
                    "method": "POST",
                    "body_format": "json",
                    "timeout": 5000,
                    "retry": 3,
                    "interval": 5,
                    "interval_unit": "s",
                    "headers": {},
                },
            },
        }

        print("\n>>> Sending server_config update command...")
        await self.send_command(server_config, "Update Server Configuration")

        print("\n✅ Command sent successfully!")
        print("⏳ Waiting for device to process and restart...")


# =============================================================================
# Main Execution
# =============================================================================
async def main():
    """
    Main entry point
    """
    client = ServerConfigClient()

    try:
        # Connect to gateway
        if not await client.connect():
            return

        # Update server configuration
        await client.update_server_config()

        # Wait a bit for device restart notification
        print("\n[INFO] Waiting 5 seconds before disconnecting...")
        await asyncio.sleep(5.0)

    except Exception as e:
        print(f"\n[EXCEPTION] {e}")
        import traceback

        traceback.print_exc()

    finally:
        await client.disconnect()

    print("\n" + "=" * 70)
    print("  TESTING COMPLETE")
    print("=" * 70)
    print("\n📝 Next Steps:")
    print("    1. Wait for device to finish rebooting (~10 seconds)")
    print("    2. Check device LED status:")
    print("       • Ethernet connected: LED should be steady or blinking")
    print("    3. Verify MQTT connection:")
    print("       • Subscribe to topic: v1/devices/me/telemetry/gwsrt")
    print("       • Data should arrive every 20 seconds")
    print("    4. Check WiFi connection on your router")
    print("       • SSID: KOWI should show new device")
    print("\n🔍 Troubleshooting:")
    print("    • If device doesn't restart, power cycle it manually")
    print("    • If MQTT not working, check broker.hivemq.com accessibility")
    print("    • If WiFi fails, verify password is correct: @Kremi201")
    print("=" * 70 + "\n")


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n\n[INTERRUPT] Program interrupted by user")
    except Exception as e:
        print(f"\n[FATAL ERROR] {e}")
        import traceback

        traceback.print_exc()
