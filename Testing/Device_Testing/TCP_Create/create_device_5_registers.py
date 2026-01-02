#!/usr/bin/env python3
"""
=============================================================================
SRT-MGATE-1210 Device Testing - TCP Mode
Create 1 TCP Device + 5 Registers
=============================================================================

Version: 2.0.0 | December 2025 | SURIOTA R&D Team
Firmware: SRT-MGATE-1210 v1.0.x

Device Configuration:
  - Protocol: Modbus TCP
  - IP Address: 192.168.1.100
  - Port: 502
  - Slave ID: 1

Registers: 5 Input Registers (INT16, Function Code 4)

Dependencies:
  pip install bleak colorama

IMPORTANT: Start the Modbus TCP Slave Simulator BEFORE running this script!
  Location: Testing/Modbus_Simulators/TCP_Slave/modbus_slave_5_registers.py

=============================================================================
"""

import asyncio
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
    print_progress_bar,
    print_table,
    print_box,
    print_summary,
    countdown,
    Fore,
    Style,
)

# =============================================================================
# Configuration
# =============================================================================
NUM_REGISTERS = 5
DEVICE_NAME = "TCP_Device_5_Regs"
DEVICE_CONFIG = {
    "device_name": DEVICE_NAME,
    "protocol": "TCP",
    "slave_id": 1,
    "timeout": 3000,
    "retry_count": 3,
    "refresh_rate_ms": 2000,
    "ip": "192.168.1.101",
    "port": 502,
}

# Register definitions (Temperature Zones)
REGISTERS = [
    {"address": 0, "name": "Temp_Zone_1", "desc": "Temperature Zone 1", "unit": "degC"},
    {"address": 1, "name": "Temp_Zone_2", "desc": "Temperature Zone 2", "unit": "degC"},
    {"address": 2, "name": "Temp_Zone_3", "desc": "Temperature Zone 3", "unit": "degC"},
    {"address": 3, "name": "Temp_Zone_4", "desc": "Temperature Zone 4", "unit": "degC"},
    {"address": 4, "name": "Temp_Zone_5", "desc": "Temperature Zone 5", "unit": "degC"},
]


# =============================================================================
# Main Program
# =============================================================================
async def main():
    """Main entry point"""
    if not check_dependencies():
        return

    print_header("TCP Device Creation", f"{NUM_REGISTERS} Input Registers", "2.0.0")

    # =========================================================================
    # Pre-flight Check
    # =========================================================================
    print_section("Pre-flight Check", "!")

    print_box(
        "IMPORTANT",
        [
            "Before continuing, make sure:",
            "",
            "1. Modbus TCP Slave Simulator is RUNNING",
            f"   Location: Testing/Modbus_Simulators/TCP_Slave/",
            f"   Command:  python modbus_slave_{NUM_REGISTERS}_registers.py",
            "",
            "2. Simulator is configured with:",
            f"   - IP: {DEVICE_CONFIG['ip']}",
            f"   - Port: {DEVICE_CONFIG['port']}",
            "   - Slave ID: 1",
            "",
            "3. Gateway can reach the simulator network",
        ],
        Fore.YELLOW,
    )

    print()
    try:
        response = (
            input(
                f"  {Fore.WHITE}Have you started the simulator? (yes/no): {Style.RESET_ALL}"
            )
            .strip()
            .lower()
        )
        if response != "yes":
            print()
            print_warning("Please start the Modbus TCP Slave Simulator first!")
            print_info(f"Run: python modbus_slave_{NUM_REGISTERS}_registers.py")
            return
    except KeyboardInterrupt:
        print()
        return

    client = BLEDeviceClient()
    success_count = 0
    failed_count = 0

    try:
        # =====================================================================
        # Step 1: Connect to Gateway
        # =====================================================================
        print_section("Step 1: BLE Connection", "[BLE]")

        if not await client.connect():
            print_error("Could not connect to MGate Gateway")
            return

        await asyncio.sleep(1)

        # =====================================================================
        # Step 2: Create TCP Device
        # =====================================================================
        print_section("Step 2: Create TCP Device", "[TCP]")

        print_box(
            "Device Configuration",
            {
                "Name": DEVICE_NAME,
                "Protocol": "Modbus TCP",
                "IP Address": DEVICE_CONFIG["ip"],
                "Port": str(DEVICE_CONFIG["port"]),
                "Slave ID": "1",
                "Timeout": "3000 ms",
                "Refresh Rate": "2000 ms",
            },
        )

        device_id = await client.create_device(DEVICE_CONFIG, DEVICE_NAME)

        if not device_id:
            print_error("Device creation failed. Aborting...")
            return

        print_success(f"Device created: {device_id}")
        await asyncio.sleep(2)

        # =====================================================================
        # Step 3: Create Registers
        # =====================================================================
        print_section(f"Step 3: Create {NUM_REGISTERS} Registers", "[REG]")

        print_info(f"Creating {NUM_REGISTERS} Input Registers for device {device_id}")
        print_info("Using 0.5s delay between registers")
        print()

        for idx, reg in enumerate(REGISTERS, 1):
            register_config = {
                "address": reg["address"],
                "register_name": reg["name"],
                "type": "Input Registers",
                "function_code": 4,
                "data_type": "INT16",
                "description": reg["desc"],
                "unit": reg["unit"],
                "scale": 1.0,
                "offset": 0.0,
            }

            # Progress bar
            progress = int((idx / NUM_REGISTERS) * 100)
            print_progress_bar(progress, prefix=f"Register {idx}/{NUM_REGISTERS}")

            result = await client.create_register(
                device_id, register_config, reg["name"]
            )

            if result:
                success_count += 1
            else:
                failed_count += 1
                print()
                print_warning(f"Failed: {reg['name']} (Address: {reg['address']})")

            await asyncio.sleep(0.5)

        print()  # New line after progress bar

        # =====================================================================
        # Summary
        # =====================================================================
        print_section("Summary", "[OK]")

        all_success = failed_count == 0

        # Register table
        headers = ["#", "Address", "Name", "Unit", "Status"]
        rows = []
        for idx, reg in enumerate(REGISTERS):
            rows.append(
                [
                    idx + 1,
                    reg["address"],
                    reg["name"][:20],
                    reg["unit"],
                    "OK" if idx < success_count else "FAIL",
                ]
            )

        # Only show first 10 and last 5 if too many
        if len(rows) > 20:
            display_rows = rows[:10] + [["...", "...", "...", "...", "..."]] + rows[-5:]
        else:
            display_rows = rows

        print_table(headers, display_rows, "Register Status")

        print_summary(
            "Creation Complete",
            {
                "Device ID": device_id,
                "Device Name": DEVICE_NAME,
                "Protocol": "Modbus TCP",
                "Target": f"{DEVICE_CONFIG['ip']}:{DEVICE_CONFIG['port']}",
                "Registers Created": f"{success_count}/{NUM_REGISTERS}",
                "Success Rate": f"{(success_count/NUM_REGISTERS)*100:.1f}%",
            },
            all_success,
        )

        if all_success:
            print_info("All registers created successfully!")
            print_info("Gateway will start TCP polling immediately")
        else:
            print_warning(f"{failed_count} registers failed - you may retry manually")

        print()
        print_info("Expected Gateway Behavior:")
        print_info(f"  - TCP polling time: ~1-2 seconds for {NUM_REGISTERS} registers")
        print_info("  - Connection pool: Reuses TCP connections")
        print_info("  - MQTT publish: Every refresh interval")

    except KeyboardInterrupt:
        print()
        print_warning("Interrupted by user")
    except Exception as e:
        print_error(f"Unexpected error: {e}")
        import traceback

        traceback.print_exc()
    finally:
        await client.disconnect()


if __name__ == "__main__":
    asyncio.run(main())
