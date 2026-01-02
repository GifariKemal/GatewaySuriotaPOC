"""
MQTT Publish Modes Test Client
Tests MQTT publish modes (Default & Customize) configuration via BLE
Tests interval units (ms/s/m) and multi-topic publishing
"""

import asyncio
import json
from bleak import BleakClient, BleakScanner

# BLE Configuration
SERVICE_UUID = "00001830-0000-1000-8000-00805f9b34fb"
COMMAND_CHAR_UUID = "11111111-1111-1111-1111-111111111101"
RESPONSE_CHAR_UUID = "11111111-1111-1111-1111-111111111102"
SERVICE_NAME = "SURIOTA GW"


class MqttModesTestClient:
    def __init__(self):
        self.client = None
        self.response_buffer = ""
        self.connected = False

    async def connect(self):
        """Connect to BLE service"""
        try:
            print("Scanning for BLE devices...")
            devices = await BleakScanner.discover()
            device = next((d for d in devices if d.name == SERVICE_NAME), None)

            if not device:
                print(f"❌ Service {SERVICE_NAME} not found")
                return False

            self.client = BleakClient(device.address)
            await self.client.connect()
            await self.client.start_notify(
                RESPONSE_CHAR_UUID, self._notification_handler
            )

            self.connected = True
            print(f"✅ Connected to {device.name} ({device.address})")
            return True

        except Exception as e:
            print(f"❌ Connection failed: {e}")
            return False

    async def disconnect(self):
        """Disconnect from BLE service"""
        if self.client and self.connected:
            await self.client.disconnect()
            self.connected = False
            print("✅ Disconnected")

    def _notification_handler(self, sender, data):
        """Handle incoming response fragments"""
        fragment = data.decode("utf-8")
        print(f"📥 Fragment: '{fragment}'")

        if fragment == "<END>":
            print(f"\n📦 Complete response received")
            try:
                response = json.loads(self.response_buffer)
                print(f"📋 Parsed response:")
                print(json.dumps(response, indent=2))
            except json.JSONDecodeError as e:
                print(f"❌ Failed to parse response: {e}")
                print(f"Raw buffer: {self.response_buffer}")
            finally:
                self.response_buffer = ""
        else:
            self.response_buffer += fragment

    async def send_command(self, command):
        """Send command with automatic fragmentation"""
        if not self.connected:
            raise RuntimeError("Not connected to BLE service")

        json_str = json.dumps(command, separators=(",", ":"))
        print(f"\n📤 Sending command ({len(json_str)} bytes):")
        print(json.dumps(command, indent=2))

        # Send with fragmentation
        chunk_size = 18
        for i in range(0, len(json_str), chunk_size):
            chunk = json_str[i : i + chunk_size]
            await self.client.write_gatt_char(COMMAND_CHAR_UUID, chunk.encode())
            await asyncio.sleep(0.1)

        # Send end marker
        await self.client.write_gatt_char(COMMAND_CHAR_UUID, "<END>".encode())
        await asyncio.sleep(2.0)

    async def read_mqtt_config(self):
        """Read current MQTT configuration"""
        print("\n" + "=" * 60)
        print("📖 Reading Current MQTT Configuration")
        print("=" * 60)
        await self.send_command({"op": "read", "type": "server_config"})

    async def test_default_mode_seconds(self):
        """Test DEFAULT MODE with interval in seconds"""
        print("\n" + "=" * 60)
        print("🧪 TEST 1: Default Mode - 5 seconds interval")
        print("=" * 60)
        await self.send_command(
            {
                "op": "update",
                "type": "server_config",
                "config": {
                    "mqtt_config": {
                        "enabled": True,
                        "broker_address": "demo.thingsboard.io",
                        "broker_port": 1883,
                        "client_id": "esp32_test_default",
                        "username": "device_token",
                        "password": "device_password",
                        "keep_alive": 60,
                        "clean_session": True,
                        "use_tls": False,
                        "publish_mode": "default",
                        "default_mode": {
                            "enabled": True,
                            "topic_publish": "v1/devices/me/telemetry",
                            "topic_subscribe": "device/control",
                            "interval": 5,
                            "interval_unit": "s",
                        },
                        "customize_mode": {"enabled": False},
                    }
                },
            }
        )

    async def test_default_mode_milliseconds(self):
        """Test DEFAULT MODE with interval in milliseconds"""
        print("\n" + "=" * 60)
        print("🧪 TEST 2: Default Mode - 3000 milliseconds interval")
        print("=" * 60)
        await self.send_command(
            {
                "op": "update",
                "type": "server_config",
                "config": {
                    "mqtt_config": {
                        "enabled": True,
                        "broker_address": "demo.thingsboard.io",
                        "broker_port": 1883,
                        "client_id": "esp32_test_default_ms",
                        "username": "device_token",
                        "password": "device_password",
                        "keep_alive": 60,
                        "clean_session": True,
                        "use_tls": False,
                        "publish_mode": "default",
                        "default_mode": {
                            "enabled": True,
                            "topic_publish": "v1/devices/me/telemetry",
                            "topic_subscribe": "device/control",
                            "interval": 3000,
                            "interval_unit": "ms",
                        },
                        "customize_mode": {"enabled": False},
                    }
                },
            }
        )

    async def test_default_mode_minutes(self):
        """Test DEFAULT MODE with interval in minutes"""
        print("\n" + "=" * 60)
        print("🧪 TEST 3: Default Mode - 1 minute interval")
        print("=" * 60)
        await self.send_command(
            {
                "op": "update",
                "type": "server_config",
                "config": {
                    "mqtt_config": {
                        "enabled": True,
                        "broker_address": "demo.thingsboard.io",
                        "broker_port": 1883,
                        "client_id": "esp32_test_default_min",
                        "username": "device_token",
                        "password": "device_password",
                        "keep_alive": 60,
                        "clean_session": True,
                        "use_tls": False,
                        "publish_mode": "default",
                        "default_mode": {
                            "enabled": True,
                            "topic_publish": "v1/devices/me/telemetry",
                            "topic_subscribe": "device/control",
                            "interval": 1,
                            "interval_unit": "m",
                        },
                        "customize_mode": {"enabled": False},
                    }
                },
            }
        )

    async def test_customize_mode_basic(self):
        """Test CUSTOMIZE MODE with 2 topics"""
        print("\n" + "=" * 60)
        print("🧪 TEST 4: Customize Mode - 2 Topics (Temperature & Pressure)")
        print("=" * 60)
        await self.send_command(
            {
                "op": "update",
                "type": "server_config",
                "config": {
                    "mqtt_config": {
                        "enabled": True,
                        "broker_address": "demo.thingsboard.io",
                        "broker_port": 1883,
                        "client_id": "esp32_test_customize",
                        "username": "device_token",
                        "password": "device_password",
                        "keep_alive": 60,
                        "clean_session": True,
                        "use_tls": False,
                        "publish_mode": "customize",
                        "default_mode": {"enabled": False},
                        "customize_mode": {
                            "enabled": True,
                            "custom_topics": [
                                {
                                    "topic": "sensor/temperature",
                                    "registers": [1, 2, 3],
                                    "interval": 5,
                                    "interval_unit": "s",
                                },
                                {
                                    "topic": "sensor/pressure",
                                    "registers": [4, 5],
                                    "interval": 10,
                                    "interval_unit": "s",
                                },
                            ],
                        },
                    }
                },
            }
        )

    async def test_customize_mode_mixed_intervals(self):
        """Test CUSTOMIZE MODE with mixed interval units"""
        print("\n" + "=" * 60)
        print("🧪 TEST 5: Customize Mode - Mixed Intervals (ms/s/m)")
        print("=" * 60)
        await self.send_command(
            {
                "op": "update",
                "type": "server_config",
                "config": {
                    "mqtt_config": {
                        "enabled": True,
                        "broker_address": "demo.thingsboard.io",
                        "broker_port": 1883,
                        "client_id": "esp32_test_mixed",
                        "username": "device_token",
                        "password": "device_password",
                        "keep_alive": 60,
                        "clean_session": True,
                        "use_tls": False,
                        "publish_mode": "customize",
                        "default_mode": {"enabled": False},
                        "customize_mode": {
                            "enabled": True,
                            "custom_topics": [
                                {
                                    "topic": "alerts/critical",
                                    "registers": [1, 5],
                                    "interval": 500,
                                    "interval_unit": "ms",
                                },
                                {
                                    "topic": "dashboard/realtime",
                                    "registers": [1, 2, 3, 4, 5, 6],
                                    "interval": 2,
                                    "interval_unit": "s",
                                },
                                {
                                    "topic": "database/historical",
                                    "registers": [1, 2, 3, 4, 5, 6],
                                    "interval": 1,
                                    "interval_unit": "m",
                                },
                            ],
                        },
                    }
                },
            }
        )

    async def test_customize_mode_register_overlap(self):
        """Test CUSTOMIZE MODE with register overlap"""
        print("\n" + "=" * 60)
        print("🧪 TEST 6: Customize Mode - Register Overlap")
        print("=" * 60)
        print(
            "Register 1 → 3 topics | Register 2-3 → 2 topics | Register 4-5 → 1 topic"
        )
        await self.send_command(
            {
                "op": "update",
                "type": "server_config",
                "config": {
                    "mqtt_config": {
                        "enabled": True,
                        "broker_address": "demo.thingsboard.io",
                        "broker_port": 1883,
                        "client_id": "esp32_test_overlap",
                        "username": "device_token",
                        "password": "device_password",
                        "keep_alive": 60,
                        "clean_session": True,
                        "use_tls": False,
                        "publish_mode": "customize",
                        "default_mode": {"enabled": False},
                        "customize_mode": {
                            "enabled": True,
                            "custom_topics": [
                                {
                                    "topic": "sensor/temperature",
                                    "registers": [1, 2, 3],
                                    "interval": 5,
                                    "interval_unit": "s",
                                },
                                {
                                    "topic": "sensor/all_sensors",
                                    "registers": [1, 2, 3, 4, 5],
                                    "interval": 10,
                                    "interval_unit": "s",
                                },
                                {
                                    "topic": "sensor/critical",
                                    "registers": [1],
                                    "interval": 2,
                                    "interval_unit": "s",
                                },
                            ],
                        },
                    }
                },
            }
        )

    async def test_warehouse_scenario(self):
        """Test WAREHOUSE scenario with categorized sensors"""
        print("\n" + "=" * 60)
        print("🧪 TEST 7: Warehouse Monitoring Scenario")
        print("=" * 60)
        print("Environment: Temp (Reg 1-4), Humidity (Reg 5-8)")
        print("Safety: Smoke (Reg 9-10), CO2 (Reg 11-12)")
        await self.send_command(
            {
                "op": "update",
                "type": "server_config",
                "config": {
                    "mqtt_config": {
                        "enabled": True,
                        "broker_address": "demo.thingsboard.io",
                        "broker_port": 1883,
                        "client_id": "warehouse_gateway",
                        "username": "device_token",
                        "password": "device_password",
                        "keep_alive": 60,
                        "clean_session": True,
                        "use_tls": False,
                        "publish_mode": "customize",
                        "default_mode": {"enabled": False},
                        "customize_mode": {
                            "enabled": True,
                            "custom_topics": [
                                {
                                    "topic": "warehouse/environment/temperature",
                                    "registers": [1, 2, 3, 4],
                                    "interval": 5,
                                    "interval_unit": "s",
                                },
                                {
                                    "topic": "warehouse/environment/humidity",
                                    "registers": [5, 6, 7, 8],
                                    "interval": 5,
                                    "interval_unit": "s",
                                },
                                {
                                    "topic": "warehouse/safety/smoke",
                                    "registers": [9, 10],
                                    "interval": 1,
                                    "interval_unit": "s",
                                },
                                {
                                    "topic": "warehouse/safety/co2",
                                    "registers": [11, 12],
                                    "interval": 2,
                                    "interval_unit": "s",
                                },
                            ],
                        },
                    }
                },
            }
        )

    async def test_disable_both_modes(self):
        """Test disabling both modes (MQTT stays connected)"""
        print("\n" + "=" * 60)
        print("🧪 TEST 8: Disable Both Modes (MQTT Connected, No Publish)")
        print("=" * 60)
        await self.send_command(
            {
                "op": "update",
                "type": "server_config",
                "config": {
                    "mqtt_config": {
                        "enabled": True,
                        "broker_address": "demo.thingsboard.io",
                        "broker_port": 1883,
                        "client_id": "esp32_test_disabled",
                        "username": "device_token",
                        "password": "device_password",
                        "keep_alive": 60,
                        "clean_session": True,
                        "use_tls": False,
                        "publish_mode": "default",
                        "default_mode": {"enabled": False},
                        "customize_mode": {"enabled": False},
                    }
                },
            }
        )


async def run_all_tests(client):
    """Run all test scenarios sequentially"""
    print("\n" + "=" * 60)
    print("🚀 RUNNING ALL MQTT PUBLISH MODES TESTS")
    print("=" * 60)

    tests = [
        ("Read Current Config", client.read_mqtt_config),
        ("Default Mode - Seconds", client.test_default_mode_seconds),
        ("Default Mode - Milliseconds", client.test_default_mode_milliseconds),
        ("Default Mode - Minutes", client.test_default_mode_minutes),
        ("Customize Mode - Basic", client.test_customize_mode_basic),
        (
            "Customize Mode - Mixed Intervals",
            client.test_customize_mode_mixed_intervals,
        ),
        (
            "Customize Mode - Register Overlap",
            client.test_customize_mode_register_overlap,
        ),
        ("Warehouse Scenario", client.test_warehouse_scenario),
        ("Disable Both Modes", client.test_disable_both_modes),
    ]

    for i, (name, test_func) in enumerate(tests, 1):
        print(f"\n{'='*60}")
        print(f"Running Test {i}/{len(tests)}: {name}")
        print(f"{'='*60}")
        await test_func()
        await asyncio.sleep(3)  # Wait between tests

    print("\n" + "=" * 60)
    print("✅ ALL TESTS COMPLETED")
    print("=" * 60)


async def interactive_menu():
    """Interactive menu for MQTT modes testing"""
    client = MqttModesTestClient()

    try:
        # Connect to BLE service
        if not await client.connect():
            return

        while True:
            print("\n" + "=" * 60)
            print("🧪 MQTT PUBLISH MODES TEST MENU")
            print("=" * 60)
            print("DEFAULT MODE TESTS:")
            print("  1. Default Mode - 5 seconds")
            print("  2. Default Mode - 3000 milliseconds")
            print("  3. Default Mode - 1 minute")
            print("\nCUSTOMIZE MODE TESTS:")
            print("  4. Customize Mode - Basic (2 topics)")
            print("  5. Customize Mode - Mixed Intervals (ms/s/m)")
            print("  6. Customize Mode - Register Overlap")
            print("  7. Warehouse Monitoring Scenario")
            print("\nOTHER:")
            print("  8. Disable Both Modes")
            print("  9. Read Current Configuration")
            print("  10. Run All Tests Sequentially")
            print("  0. Exit")
            print("=" * 60)

            choice = input("\n👉 Select option (0-10): ").strip()

            if choice == "1":
                await client.test_default_mode_seconds()
            elif choice == "2":
                await client.test_default_mode_milliseconds()
            elif choice == "3":
                await client.test_default_mode_minutes()
            elif choice == "4":
                await client.test_customize_mode_basic()
            elif choice == "5":
                await client.test_customize_mode_mixed_intervals()
            elif choice == "6":
                await client.test_customize_mode_register_overlap()
            elif choice == "7":
                await client.test_warehouse_scenario()
            elif choice == "8":
                await client.test_disable_both_modes()
            elif choice == "9":
                await client.read_mqtt_config()
            elif choice == "10":
                await run_all_tests(client)
            elif choice == "0":
                print("\n👋 Exiting...")
                break
            else:
                print("\n❌ Invalid choice. Please select 0-10.")

            await asyncio.sleep(2)

    except KeyboardInterrupt:
        print("\n\n⚠️  Interrupted by user")
    except Exception as e:
        print(f"\n❌ Error: {e}")
        import traceback

        traceback.print_exc()
    finally:
        await client.disconnect()


if __name__ == "__main__":
    print(
        """
╔══════════════════════════════════════════════════════════════╗
║     MQTT PUBLISH MODES TEST CLIENT                           ║
║     SRT-MGATE-1210 Firmware v2.0                            ║
║                                                              ║
║     Tests:                                                   ║
║     • Default Mode (Batch Publishing)                       ║
║     • Customize Mode (Multi-Topic Publishing)               ║
║     • Interval Units (ms/s/m)                               ║
║     • Register Overlap                                      ║
╚══════════════════════════════════════════════════════════════╝
    """
    )

    asyncio.run(interactive_menu())
