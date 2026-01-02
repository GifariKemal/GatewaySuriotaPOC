#!/usr/bin/env python3
"""
=============================================================================
SRT-MGATE-1210 Device Testing - RTU Mode
Create 1 RTU Device + 10 Registers
=============================================================================

Version: 2.0.0 | December 2025 | SURIOTA R&D Team
Firmware: SRT-MGATE-1210 v1.0.x

Device Configuration:
  - Protocol: Modbus RTU
  - Serial Port: Port 1 (ESP32 GPIO)
  - Baud Rate: 9600
  - Format: 8N1
  - Slave ID: 1

Registers: 10 Input Registers (INT16, Function Code 4)

Dependencies:
  pip install bleak colorama

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
NUM_REGISTERS = 10
DEVICE_NAME = "RTU_Device_10_Regs"
DEVICE_CONFIG = {
    "device_name": DEVICE_NAME,
    "protocol": "RTU",
    "slave_id": 1,
    "timeout": 5000,
    "retry_count": 3,
    "refresh_rate_ms": 2000,
    "serial_port": 1,
    "baud_rate": 9600,
    "data_bits": 8,
    "parity": "None",
    "stop_bits": 1,
}

# Register definitions
REGISTERS = [
    {"address": 0, "name": "Temperature", "desc": "Data Point 1", "unit": "°C"},
    {"address": 1, "name": "Humidity", "desc": "Data Point 2", "unit": "%"},
    {"address": 2, "name": "Pressure", "desc": "Data Point 3", "unit": "Pa"},
    {"address": 3, "name": "Voltage", "desc": "Data Point 4", "unit": "V"},
    {"address": 4, "name": "Current", "desc": "Data Point 5", "unit": "A"},
    {"address": 5, "name": "Register_6", "desc": "Data Point 6", "unit": "unit"},
    {"address": 6, "name": "Register_7", "desc": "Data Point 7", "unit": "unit"},
    {"address": 7, "name": "Register_8", "desc": "Data Point 8", "unit": "unit"},
    {"address": 8, "name": "Register_9", "desc": "Data Point 9", "unit": "unit"},
    {"address": 9, "name": "Register_10", "desc": "Data Point 10", "unit": "unit"},
]


# =============================================================================
# Main Program
# =============================================================================
async def main():
    """Main entry point"""
    if not check_dependencies():
        return

    print_header("RTU Device Creation", f"{NUM_REGISTERS} Input Registers", "2.0.0")

    client = BLEDeviceClient()
    success_count = 0
    failed_count = 0

    try:
        # =====================================================================
        # Step 1: Connect to Gateway
        # =====================================================================
        print_section("Step 1: BLE Connection", "🔗")

        if not await client.connect():
            print_error("Could not connect to MGate Gateway")
            return

        await asyncio.sleep(1)

        # =====================================================================
        # Step 2: Create RTU Device
        # =====================================================================
        print_section("Step 2: Create RTU Device", "📟")

        print_box(
            "Device Configuration",
            {
                "Name": DEVICE_NAME,
                "Protocol": "Modbus RTU",
                "Serial Port": "Port 1",
                "Baud Rate": "9600",
                "Format": "8N1",
                "Slave ID": "1",
                "Timeout": "5000 ms",
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
        print_section(f"Step 3: Create {NUM_REGISTERS} Registers", "📝")

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
        print_section("Summary", "📊")

        all_success = failed_count == 0

        # Register table
        headers = ["#", "Address", "Name", "Unit", "Status"]
        rows = []
        for idx, reg in enumerate(REGISTERS):
            status = (
                f"{Fore.GREEN}OK{Style.RESET_ALL}"
                if idx < success_count
                else f"{Fore.RED}FAIL{Style.RESET_ALL}"
            )
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
                "Protocol": "Modbus RTU",
                "Registers Created": f"{success_count}/{NUM_REGISTERS}",
                "Success Rate": f"{(success_count/NUM_REGISTERS)*100:.1f}%",
            },
            all_success,
        )

        if all_success:
            print_info("All registers created successfully!")
            print_info("Gateway will start polling immediately")
        else:
            print_warning(f"{failed_count} registers failed - you may retry manually")

        print()
        print_info("Next Steps:")
        print_info("  1. Start Modbus RTU Slave Simulator on COM port")
        print_info(f"  2. Configure slave with ID 1 and {NUM_REGISTERS} registers")
        print_info("  3. Monitor Gateway serial output for RTU polling")

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
