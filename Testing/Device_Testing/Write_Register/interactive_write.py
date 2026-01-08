#!/usr/bin/env python3
"""
=============================================================================
SRT-MGATE-1210 - Interactive Write Register Tool (v1.0.8)
=============================================================================

Version: 1.0.0 | January 2026 | SURIOTA R&D Team
Firmware: SRT-MGATE-1210 v1.0.8+

Interactive tool for testing write register functionality:
  - Select device from list
  - Select register from device
  - Enter value to write
  - See response and verify write

Dependencies:
  pip install bleak rich

=============================================================================
"""

import asyncio
import json
import sys
import os

# Add parent directory to path for shared module
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))

from ble_common import (
    BLEDeviceClient,
    check_dependencies,
    print_header,
    print_section,
    print_step,
    print_success,
    print_error,
    print_warning,
    print_info,
    print_data,
    print_table,
    print_box,
    Fore,
    Style,
    console,
    RICH_AVAILABLE,
)


class InteractiveWriteTool:
    """Interactive tool for write register testing"""

    def __init__(self):
        self.client = BLEDeviceClient()
        self.devices = []
        self.current_device_id = None
        self.current_device_name = None
        self.registers = []

    async def connect(self):
        """Connect to gateway"""
        print_section("BLE Connection", "[BLE]")
        return await self.client.connect()

    async def load_devices(self):
        """Load all devices from gateway"""
        print_info("Loading devices...")

        cmd = json.dumps({"op": "read", "type": "device"}, separators=(",", ":"))
        response = await self.client.send_command(cmd, timeout=15)

        if response and response.get("status") == "ok":
            self.devices = response.get("devices", [])
            print_success(f"Loaded {len(self.devices)} device(s)")
            return True
        else:
            print_error("Failed to load devices")
            return False

    async def select_device(self):
        """Display device selection menu"""
        if not self.devices:
            print_warning("No devices found")
            return False

        print_section("Device Selection", "[DEV]")

        headers = ["#", "Device ID", "Device Name", "Protocol", "IP/Port"]
        rows = []

        for idx, device in enumerate(self.devices, 1):
            protocol = device.get("protocol", "RTU")
            if protocol.upper() == "TCP":
                target = f"{device.get('ip', 'N/A')}:{device.get('port', 502)}"
            else:
                target = f"Bus {device.get('serial_port', 1)}"

            rows.append(
                [
                    idx,
                    device.get("device_id", "N/A"),
                    device.get("device_name", "N/A")[:20],
                    protocol,
                    target,
                ]
            )

        print_table(headers, rows, "Available Devices")

        print()
        try:
            choice = input(f"  Select device (1-{len(self.devices)}): ").strip()
            idx = int(choice) - 1

            if 0 <= idx < len(self.devices):
                self.current_device_id = self.devices[idx].get("device_id")
                self.current_device_name = self.devices[idx].get("device_name")
                print_success(
                    f"Selected: {self.current_device_name} ({self.current_device_id})"
                )
                return True
            else:
                print_error("Invalid selection")
                return False
        except (ValueError, KeyboardInterrupt):
            return False

    async def load_registers(self):
        """Load registers for selected device"""
        if not self.current_device_id:
            print_error("No device selected")
            return False

        print_info(f"Loading registers for {self.current_device_name}...")

        self.registers = await self.client.read_registers(self.current_device_id)

        if self.registers:
            print_success(f"Loaded {len(self.registers)} register(s)")
            return True
        else:
            print_warning("No registers found for this device")
            return False

    def display_registers(self):
        """Display register list with writable status"""
        if not self.registers:
            print_warning("No registers loaded")
            return

        print_section("Registers", "[REG]")

        headers = ["#", "Register ID", "Name", "Address", "FC", "Writable", "Min/Max"]
        rows = []

        for idx, reg in enumerate(self.registers, 1):
            writable = "YES" if reg.get("writable", False) else "NO"
            min_val = reg.get("min_value", "-")
            max_val = reg.get("max_value", "-")
            min_max = (
                f"{min_val}/{max_val}" if min_val != "-" or max_val != "-" else "-"
            )

            rows.append(
                [
                    idx,
                    reg.get("register_id", "N/A")[:10],
                    reg.get("register_name", "N/A")[:15],
                    reg.get("address", 0),
                    reg.get("function_code", 3),
                    writable,
                    min_max,
                ]
            )

        print_table(headers, rows, f"Registers for {self.current_device_name}")

    async def select_and_write(self):
        """Select a register and write a value"""
        if not self.registers:
            print_error("No registers loaded")
            return

        self.display_registers()

        print()
        try:
            choice = input(f"  Select register to write (1-{len(self.registers)}): ").strip()
            idx = int(choice) - 1

            if not (0 <= idx < len(self.registers)):
                print_error("Invalid selection")
                return

            reg = self.registers[idx]
            reg_id = reg.get("register_id")
            reg_name = reg.get("register_name")
            writable = reg.get("writable", False)
            min_val = reg.get("min_value")
            max_val = reg.get("max_value")
            scale = reg.get("scale", 1.0)
            offset = reg.get("offset", 0.0)

            print()
            print_box(
                "Register Info",
                {
                    "Name": reg_name,
                    "ID": reg_id,
                    "Writable": "YES" if writable else "NO",
                    "Scale": str(scale),
                    "Offset": str(offset),
                    "Min": str(min_val) if min_val is not None else "None",
                    "Max": str(max_val) if max_val is not None else "None",
                },
            )

            if not writable:
                print_warning("This register is NOT writable!")
                confirm = input("  Continue anyway? (yes/no): ").strip().lower()
                if confirm != "yes":
                    return

            print()
            value_str = input("  Enter value to write: ").strip()
            value = float(value_str)

            print()
            print_info(f"Writing {value} to {reg_name}...")

            # Calculate expected raw value
            raw_value = (value - offset) / scale
            print_data("Expected raw value", f"{raw_value:.2f}")

            success, response = await self.client.write_register(
                self.current_device_id, reg_id, value
            )

            print()
            if success:
                print_box(
                    "Write Response",
                    {
                        "Status": "SUCCESS",
                        "Written Value": str(response.get("value_written", value)),
                        "Raw Value": str(response.get("raw_value", "N/A")),
                        "Response Time": f"{response.get('response_time_ms', 'N/A')} ms",
                    },
                    "info",
                )
            else:
                error_code = response.get("error_code", "N/A") if response else "N/A"
                error_msg = response.get("error", "Unknown") if response else "No response"
                print_box(
                    "Write Response",
                    {
                        "Status": "FAILED",
                        "Error Code": str(error_code),
                        "Error": error_msg,
                    },
                    "warning",
                )

        except ValueError:
            print_error("Invalid number format")
        except KeyboardInterrupt:
            print()

    async def enable_writable(self):
        """Enable writable flag on a register"""
        if not self.registers:
            print_error("No registers loaded")
            return

        self.display_registers()

        print()
        try:
            choice = input(
                f"  Select register to enable writable (1-{len(self.registers)}): "
            ).strip()
            idx = int(choice) - 1

            if not (0 <= idx < len(self.registers)):
                print_error("Invalid selection")
                return

            reg = self.registers[idx]
            reg_id = reg.get("register_id")
            reg_name = reg.get("register_name")

            print()
            print_info(f"Enabling writable on {reg_name}...")

            # Optional min/max
            min_val = input("  Enter min_value (or press Enter to skip): ").strip()
            max_val = input("  Enter max_value (or press Enter to skip): ").strip()

            config = {"writable": True}
            if min_val:
                config["min_value"] = float(min_val)
            if max_val:
                config["max_value"] = float(max_val)

            success, response = await self.client.update_register(
                self.current_device_id, reg_id, config
            )

            if success:
                print_success(f"Register {reg_name} is now writable")
                # Reload registers
                await self.load_registers()
            else:
                print_error("Failed to update register")

        except (ValueError, KeyboardInterrupt):
            print()

    async def run(self):
        """Main interactive loop"""
        print_header(
            "Interactive Write Register Tool",
            "Test write operations interactively",
            "1.0.0",
        )

        if not await self.connect():
            return

        await asyncio.sleep(1)

        if not await self.load_devices():
            return

        while True:
            print_section("Main Menu", "[MENU]")
            print()
            print("  1. Select device")
            print("  2. Show registers")
            print("  3. Write to register")
            print("  4. Enable writable on register")
            print("  5. Reload devices")
            print("  6. Reload registers")
            print("  q. Quit")
            print()

            try:
                choice = input("  Enter choice: ").strip().lower()

                if choice == "1":
                    await self.select_device()
                    if self.current_device_id:
                        await self.load_registers()

                elif choice == "2":
                    self.display_registers()

                elif choice == "3":
                    await self.select_and_write()

                elif choice == "4":
                    await self.enable_writable()

                elif choice == "5":
                    await self.load_devices()

                elif choice == "6":
                    if self.current_device_id:
                        await self.load_registers()
                    else:
                        print_warning("Select a device first")

                elif choice == "q":
                    break

                else:
                    print_warning("Invalid choice")

            except KeyboardInterrupt:
                print()
                break

        await self.client.disconnect()
        print_info("Goodbye!")


async def main():
    """Main entry point"""
    if not check_dependencies():
        return

    tool = InteractiveWriteTool()
    await tool.run()


if __name__ == "__main__":
    asyncio.run(main())
