#!/usr/bin/env python3
"""
=============================================================================
RTU Device Creation Programs Generator (Enhanced v2.0)
=============================================================================
Generates RTU device creation scripts with:
  - New BLE UUIDs and device name detection (MGate-1210*)
  - Enhanced terminal UI with colors and progress
  - Shared ble_common module integration

Author: SURIOTA R&D Team
Updated: December 2025
=============================================================================
"""

import os
import sys

TEMPLATE = '''#!/usr/bin/env python3
"""
=============================================================================
SRT-MGATE-1210 Device Testing - RTU Mode
Create 1 RTU Device + {num_regs} Registers
=============================================================================

Version: 2.0.0 | December 2025 | SURIOTA R&D Team
Firmware: SRT-MGATE-1210 v1.0.x

Device Configuration:
  - Protocol: Modbus RTU
  - Serial Port: Port 1 (ESP32 GPIO)
  - Baud Rate: 9600
  - Format: 8N1
  - Slave ID: 1

Registers: {num_regs} Input Registers (INT16, Function Code 4)

Dependencies:
  pip install bleak colorama

=============================================================================
"""

import asyncio
import sys
import os

# Add parent directory to path for shared module
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..'))

from ble_common import (
    BLEDeviceClient, check_dependencies,
    print_header, print_section, print_step, print_success, print_error,
    print_warning, print_info, print_data, print_progress_bar, print_table,
    print_box, print_summary, countdown, Fore, Style
)

# =============================================================================
# Configuration
# =============================================================================
NUM_REGISTERS = {num_regs}
DEVICE_NAME = "RTU_Device_{num_regs}_Regs"
DEVICE_CONFIG = {{
    "device_name": DEVICE_NAME,
    "protocol": "RTU",
    "slave_id": 1,
    "timeout": 5000,
    "retry_count": 3,
    "refresh_rate_ms": 2000,
    "serial_port": 2,
    "baud_rate": 9600,
    "data_bits": 8,
    "parity": "None",
    "stop_bits": 1
}}

# Register definitions
REGISTERS = [
{registers_list}
]

# =============================================================================
# Main Program
# =============================================================================
async def main():
    """Main entry point"""
    if not check_dependencies():
        return

    print_header(
        "RTU Device Creation",
        f"{{NUM_REGISTERS}} Input Registers",
        "2.0.0"
    )

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

        print_box("Device Configuration", {{
            "Name": DEVICE_NAME,
            "Protocol": "Modbus RTU",
            "Serial Port": "Port 1",
            "Baud Rate": "9600",
            "Format": "8N1",
            "Slave ID": "1",
            "Timeout": "5000 ms",
            "Refresh Rate": "2000 ms"
        }})

        device_id = await client.create_device(DEVICE_CONFIG, DEVICE_NAME)

        if not device_id:
            print_error("Device creation failed. Aborting...")
            return

        print_success(f"Device created: {{device_id}}")
        await asyncio.sleep(2)

        # =====================================================================
        # Step 3: Create Registers
        # =====================================================================
        print_section(f"Step 3: Create {{NUM_REGISTERS}} Registers", "📝")

        print_info(f"Creating {{NUM_REGISTERS}} Input Registers for device {{device_id}}")
        print_info("Using 0.5s delay between registers")
        print()

        for idx, reg in enumerate(REGISTERS, 1):
            register_config = {{
                "address": reg["address"],
                "register_name": reg["name"],
                "type": "Input Registers",
                "function_code": 4,
                "data_type": "INT16",
                "description": reg["desc"],
                "unit": reg["unit"],
                "scale": 1.0,
                "offset": 0.0
            }}

            # Progress bar
            progress = int((idx / NUM_REGISTERS) * 100)
            print_progress_bar(progress, prefix=f"Register {{idx}}/{{NUM_REGISTERS}}")

            result = await client.create_register(device_id, register_config, reg["name"])

            if result:
                success_count += 1
            else:
                failed_count += 1
                print()
                print_warning(f"Failed: {{reg['name']}} (Address: {{reg['address']}})")

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
            status = f"{{Fore.GREEN}}OK{{Style.RESET_ALL}}" if idx < success_count else f"{{Fore.RED}}FAIL{{Style.RESET_ALL}}"
            rows.append([idx+1, reg["address"], reg["name"][:20], reg["unit"], "OK" if idx < success_count else "FAIL"])

        # Only show first 10 and last 5 if too many
        if len(rows) > 20:
            display_rows = rows[:10] + [["...", "...", "...", "...", "..."]] + rows[-5:]
        else:
            display_rows = rows

        print_table(headers, display_rows, "Register Status")

        print_summary("Creation Complete", {{
            "Device ID": device_id,
            "Device Name": DEVICE_NAME,
            "Protocol": "Modbus RTU",
            "Registers Created": f"{{success_count}}/{{NUM_REGISTERS}}",
            "Success Rate": f"{{(success_count/NUM_REGISTERS)*100:.1f}}%"
        }}, all_success)

        if all_success:
            print_info("All registers created successfully!")
            print_info("Gateway will start polling immediately")
        else:
            print_warning(f"{{failed_count}} registers failed - you may retry manually")

        print()
        print_info("Next Steps:")
        print_info("  1. Start Modbus RTU Slave Simulator on COM port")
        print_info(f"  2. Configure slave with ID 1 and {{NUM_REGISTERS}} registers")
        print_info("  3. Monitor Gateway serial output for RTU polling")

    except KeyboardInterrupt:
        print()
        print_warning("Interrupted by user")
    except Exception as e:
        print_error(f"Unexpected error: {{e}}")
        import traceback
        traceback.print_exc()
    finally:
        await client.disconnect()

if __name__ == "__main__":
    asyncio.run(main())
'''


def generate_programs():
    """Generate RTU programs for different register counts"""

    register_counts = [5, 10, 15, 20, 25, 30, 35, 40, 45, 50]

    print()
    for num_regs in register_counts:
        print(f"  Generating create_device_{num_regs}_registers.py...")

        # Generate registers list
        registers_lines = []
        for i in range(num_regs):
            if i < 5:
                names = ["Temperature", "Humidity", "Pressure", "Voltage", "Current"]
                units = ["°C", "%", "Pa", "V", "A"]
                name = names[i]
                unit = units[i]
            else:
                name = f"Register_{i+1}"
                unit = "unit"

            line = f'    {{"address": {i}, "name": "{name}", "desc": "Data Point {i+1}", "unit": "{unit}"}}'
            if i < num_regs - 1:
                line += ","
            registers_lines.append(line)

        registers_list = "\n".join(registers_lines)

        # Create file content
        content = TEMPLATE.format(num_regs=num_regs, registers_list=registers_list)

        # Write file
        filename = f"create_device_{num_regs}_registers.py"
        with open(filename, "w", encoding="utf-8") as f:
            f.write(content)

        # Make executable (Unix-like)
        try:
            os.chmod(filename, 0o755)
        except:
            pass

    print()
    print("  [OK] All RTU programs generated!")
    print()
    print("  Generated files:")
    for num_regs in register_counts:
        print(f"    - create_device_{num_regs}_registers.py")
    print()


if __name__ == "__main__":
    print()
    print("=" * 60)
    print("  RTU Device Creation Programs Generator v2.0")
    print("=" * 60)
    print("  Enhanced with:")
    print("    - New BLE device name detection (MGate-1210*)")
    print("    - Colored terminal output")
    print("    - Progress bars and tables")
    print("    - Shared ble_common module")
    print("=" * 60)

    generate_programs()
