"""
Create Modbus RTU Device and Registers Test
Creates 1 RTU device with 10 data points via BLE
"""

import asyncio
import json
from bleak import BleakClient, BleakScanner

# BLE Configuration
SERVICE_UUID = "00001830-0000-1000-8000-00805f9b34fb"
COMMAND_CHAR_UUID = "11111111-1111-1111-1111-111111111101"
RESPONSE_CHAR_UUID = "11111111-1111-1111-1111-111111111102"
SERVICE_NAME = "SURIOTA GW"


class CreateDeviceRegisterClient:
    def __init__(self):
        self.client = None
        self.response_buffer = ""
        self.connected = False
        self.device_id = None

    async def connect(self):
        """Connect to BLE service"""
        try:
            devices = await BleakScanner.discover()
            device = next((d for d in devices if d.name == SERVICE_NAME), None)

            if not device:
                print(f"Service {SERVICE_NAME} not found")
                return False

            self.client = BleakClient(device.address)
            await self.client.connect()
            await self.client.start_notify(
                RESPONSE_CHAR_UUID, self._notification_handler
            )

            self.connected = True
            print(f"Connected to {device.name}")
            return True

        except Exception as e:
            print(f"Connection failed: {e}")
            return False

    async def disconnect(self):
        """Disconnect from BLE service"""
        if self.client and self.connected:
            await self.client.disconnect()
            self.connected = False
            print("Disconnected")

    def _notification_handler(self, sender, data):
        """Handle incoming response fragments"""
        fragment = data.decode("utf-8")

        if fragment == "<END>":
            try:
                response = json.loads(self.response_buffer)
                status = response.get("status")
                if status == "ok":
                    print(f"✅ Response OK: {json.dumps(response, indent=2)}")
                    # Store device ID for register creation
                    if "device_id" in response:
                        self.device_id = response["device_id"]
                        print(f"   Stored device ID: {self.device_id}")
                else:
                    print(f"❌ Response ERROR: {json.dumps(response, indent=2)}")

            except json.JSONDecodeError:
                print(f"❌ Invalid JSON response: {self.response_buffer}")
            finally:
                self.response_buffer = ""
        else:
            self.response_buffer += fragment

    async def send_command(self, command):
        """Send command with automatic fragmentation"""
        if not self.connected:
            raise RuntimeError("Not connected to BLE service")

        json_str = json.dumps(command, separators=(",", ":"))

        # Send with fragmentation
        chunk_size = 18
        for i in range(0, len(json_str), chunk_size):
            chunk = json_str[i : i + chunk_size]
            await self.client.write_gatt_char(COMMAND_CHAR_UUID, chunk.encode())
            await asyncio.sleep(0.1)

        # Send end marker
        await self.client.write_gatt_char(COMMAND_CHAR_UUID, "<END>".encode())
        await asyncio.sleep(2.0)

    async def create_rtu_device(self):
        """Create Modbus RTU device"""
        print("\n=== Creating Modbus RTU Device ===")
        await self.send_command(
            {
                "op": "create",
                "type": "device",
                "config": {
                    "device_name": "RTUDE",
                    "protocol": "RTU",
                    "serial_port": 1,
                    "baud_rate": 9600,
                    "parity": "None",
                    "data_bits": 8,
                    "stop_bits": 1,
                    "slave_id": 1,
                    "timeout": 1000,
                    "retry_count": 3,
                    "refresh_rate_ms": 20000,
                },
            }
        )

    async def create_test_registers(self):
        """Create registers for all data type variants"""
        if not self.device_id:
            print("No device ID available")
            return

        test_data_types = [
            # 16-bit types
            {"base_type": "int16", "endianness": ""},
            {"base_type": "uint16", "endianness": ""},
            {"base_type": "bool", "endianness": ""},
            # 32-bit types
            {"base_type": "int32", "endianness": "be"},
            {"base_type": "int32", "endianness": "le"},
            {"base_type": "int32", "endianness": "ws1"},
            {"base_type": "int32", "endianness": "ws2"},
            {"base_type": "uint32", "endianness": "be"},
            {"base_type": "uint32", "endianness": "le"},
            {"base_type": "uint32", "endianness": "ws1"},
            {"base_type": "uint32", "endianness": "ws2"},
            {"base_type": "float32", "endianness": "be"},
            {"base_type": "float32", "endianness": "le"},
            {"base_type": "float32", "endianness": "ws1"},
            {"base_type": "float32", "endianness": "ws2"},
            # 64-bit types
            {"base_type": "int64", "endianness": "be"},
            {"base_type": "int64", "endianness": "le"},
            {"base_type": "int64", "endianness": "ws1"},
            {"base_type": "int64", "endianness": "ws2"},
            {"base_type": "uint64", "endianness": "be"},
            {"base_type": "uint64", "endianness": "le"},
            {"base_type": "uint64", "endianness": "ws1"},
            {"base_type": "uint64", "endianness": "ws2"},
            {"base_type": "float64", "endianness": "be"},
            {"base_type": "float64", "endianness": "le"},
            {"base_type": "float64", "endianness": "ws1"},
            {"base_type": "float64", "endianness": "ws2"},
        ]

        address_counter = 1
        for i, reg_spec in enumerate(test_data_types):
            base_type = reg_spec["base_type"]
            endianness = reg_spec["endianness"]

            data_type_str = base_type
            if endianness:
                data_type_str += f"-{endianness}"

            register_name = (
                f"{data_type_str.upper().replace('-', '_')}_TEST_{address_counter}"
            )

            print(
                f"\n=== Creating Register {i+1}/{len(test_data_types)}: {register_name} ({data_type_str}) ==="
            )

            # Determine function code and register type
            function_code = 3  # Default to Holding Register
            register_type = "Holding Register"
            if base_type == "bool":
                function_code = 1
                register_type = "Coil"
            elif base_type == "discrete_input":  # Example for other types if needed
                function_code = 2
                register_type = "Discrete Input"
            elif base_type == "input_register":  # Example for other types if needed
                function_code = 4
                register_type = "Input Register"

            await self.send_command(
                {
                    "op": "create",
                    "type": "register",
                    "device_id": self.device_id,
                    "config": {
                        "address": address_counter,
                        "register_name": register_name,
                        "type": register_type,
                        "function_code": function_code,
                        "data_type": data_type_str,
                        "description": f"Test register for {data_type_str}",
                        "refresh_rate_ms": 5000,
                    },
                }
            )
            print(f"   Sending command to create register: {register_name}")
            address_counter += 1
            await asyncio.sleep(1)  # Increased delay to 1 second

    async def read_device_registers(self):
        """Read all registers for the created device"""
        if not self.device_id:
            print("No device ID available to read registers from.")
            return

        print(f"\n=== Reading all registers for device {self.device_id} ===")
        await self.send_command(
            {"op": "read", "type": "registers", "device_id": self.device_id}
        )
        await asyncio.sleep(2)  # Wait for response


async def main():
    """Main function"""
    client = CreateDeviceRegisterClient()

    try:
        # Connect to BLE service
        if not await client.connect():
            return

        print("=== Modbus RTU Device & Register Creation ===")

        # Step 1: Create RTU device
        await client.create_rtu_device()
        await asyncio.sleep(3)

        # Step 2: Create test registers for all data types
        await client.create_test_registers()
        await asyncio.sleep(3)

        # Step 3: Read all registers to verify creation
        await client.read_device_registers()

        print("\n✅ === Creation Completed ===")
        print("Created Modbus RTU device with various data type registers")
        print("The Modbus RTU service should now be reading data from these registers")

    except Exception as e:
        print(f"Test failed: {e}")
    finally:
        await client.disconnect()


if __name__ == "__main__":
    asyncio.run(main())
