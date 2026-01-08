#!/usr/bin/env python3
"""
=============================================================================
SRT-MGATE-1210 Device Testing - Write Register Feature (v1.0.8)
=============================================================================

Version: 1.0.0 | January 2026 | SURIOTA R&D Team
Firmware: SRT-MGATE-1210 v1.0.8+

This script tests the Write Register functionality:
  1. Connect to gateway via BLE
  2. Create a TCP device with writable registers
  3. Enable writable flag on registers
  4. Test writing values to registers
  5. Verify error handling (non-writable, out of range)

Dependencies:
  pip install bleak rich

IMPORTANT: Start the Modbus TCP Slave Simulator BEFORE running this script!
  Location: Testing/Modbus_Simulators/TCP_Slave/modbus_slave_50_registers.py

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
DEVICE_NAME = "TCP_Write_Test"
DEVICE_CONFIG = {
    "device_name": DEVICE_NAME,
    "protocol": "TCP",
    "slave_id": 1,
    "timeout": 3000,
    "retry_count": 3,
    "refresh_rate_ms": 5000,
    "ip": "10.106.226.9",  # Update to your simulator IP
    "port": 502,
}

# Register definitions for write testing
REGISTERS = [
    {
        "address": 0,
        "name": "Setpoint_Temp",
        "desc": "Temperature Setpoint (Writable)",
        "unit": "degC",
        "data_type": "INT16",
        "function_code": 3,  # Holding register (writable)
        "scale": 0.1,
        "offset": 0.0,
        "writable": True,
        "min_value": 0.0,
        "max_value": 100.0,
    },
    {
        "address": 1,
        "name": "Setpoint_Pressure",
        "desc": "Pressure Setpoint (Writable)",
        "unit": "bar",
        "data_type": "UINT16",
        "function_code": 3,  # Holding register (writable)
        "scale": 0.01,
        "offset": 0.0,
        "writable": True,
        "min_value": 0.0,
        "max_value": 10.0,
    },
    {
        "address": 2,
        "name": "Read_Only_Value",
        "desc": "Read-only value (NOT writable)",
        "unit": "units",
        "data_type": "INT16",
        "function_code": 3,  # Holding register but NOT writable
        "scale": 1.0,
        "offset": 0.0,
        "writable": False,  # This should fail write attempts
    },
    {
        "address": 10,
        "name": "Coil_Output",
        "desc": "Digital Output (Coil - Writable)",
        "unit": "",
        "data_type": "BOOL",
        "function_code": 1,  # Coil (writable)
        "scale": 1.0,
        "offset": 0.0,
        "writable": True,
    },
]

# Test cases for write operations
WRITE_TESTS = [
    {
        "name": "Write Temperature Setpoint",
        "register_name": "Setpoint_Temp",
        "value": 25.5,
        "expected_success": True,
        "description": "Write 25.5°C (raw: 255)",
    },
    {
        "name": "Write Pressure Setpoint",
        "register_name": "Setpoint_Pressure",
        "value": 5.55,
        "expected_success": True,
        "description": "Write 5.55 bar (raw: 555)",
    },
    {
        "name": "Write to Read-Only Register",
        "register_name": "Read_Only_Value",
        "value": 100,
        "expected_success": False,
        "description": "Should fail - register not writable",
    },
    {
        "name": "Write Out of Range (High)",
        "register_name": "Setpoint_Temp",
        "value": 150.0,
        "expected_success": False,
        "description": "Should fail - value > max_value (100)",
    },
    {
        "name": "Write Out of Range (Low)",
        "register_name": "Setpoint_Pressure",
        "value": -5.0,
        "expected_success": False,
        "description": "Should fail - value < min_value (0)",
    },
    {
        "name": "Write Coil ON",
        "register_name": "Coil_Output",
        "value": 1,
        "expected_success": True,
        "description": "Turn coil ON",
    },
    {
        "name": "Write Coil OFF",
        "register_name": "Coil_Output",
        "value": 0,
        "expected_success": True,
        "description": "Turn coil OFF",
    },
]


# =============================================================================
# Main Program
# =============================================================================
async def main():
    """Main entry point"""
    if not check_dependencies():
        return

    print_header(
        "Write Register Feature Test", "v1.0.8 - Modbus Write Operations", "1.0.0"
    )

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
            "   Location: Testing/Modbus_Simulators/TCP_Slave/",
            "   Command:  python modbus_slave_50_registers.py",
            "",
            "2. Simulator is configured with:",
            f"   - IP: {DEVICE_CONFIG['ip']}",
            f"   - Port: {DEVICE_CONFIG['port']}",
            "   - Slave ID: 1",
            "",
            "3. Gateway firmware is v1.0.8 or later",
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
            print_info("Run: python modbus_slave_50_registers.py")
            return
    except KeyboardInterrupt:
        print()
        return

    client = BLEDeviceClient()
    device_id = None
    register_ids = {}  # Map register_name -> register_id

    test_results = []

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
            },
        )

        device_id = await client.create_device(DEVICE_CONFIG, DEVICE_NAME)

        if not device_id:
            print_error("Device creation failed. Aborting...")
            return

        print_success(f"Device created: {device_id}")
        await asyncio.sleep(2)

        # =====================================================================
        # Step 3: Create Registers with Writable Configuration
        # =====================================================================
        print_section("Step 3: Create Registers", "[REG]")

        print_info(f"Creating {len(REGISTERS)} registers (some writable)...")
        print()

        for idx, reg in enumerate(REGISTERS, 1):
            register_config = {
                "address": reg["address"],
                "register_name": reg["name"],
                "function_code": reg["function_code"],
                "data_type": reg["data_type"],
                "description": reg["desc"],
                "unit": reg["unit"],
                "scale": reg["scale"],
                "offset": reg["offset"],
                "writable": reg.get("writable", False),
            }

            # Add min/max if present
            if "min_value" in reg:
                register_config["min_value"] = reg["min_value"]
            if "max_value" in reg:
                register_config["max_value"] = reg["max_value"]

            print_progress_bar(
                int((idx / len(REGISTERS)) * 100),
                prefix=f"Register {idx}/{len(REGISTERS)}",
            )

            result = await client.create_register(device_id, register_config, reg["name"])

            if result:
                # Get the register_id from the created register
                # We need to read registers to get the IDs
                print_success(f"Created: {reg['name']} (writable={reg.get('writable', False)})")
            else:
                print_error(f"Failed: {reg['name']}")

            await asyncio.sleep(0.5)

        print()

        # Read registers to get their IDs
        print_info("Reading register IDs...")
        registers = await client.read_registers(device_id)

        for reg in registers:
            reg_name = reg.get("register_name", "")
            reg_id = reg.get("register_id", "")
            if reg_name and reg_id:
                register_ids[reg_name] = reg_id
                print_data(reg_name, reg_id)

        await asyncio.sleep(2)

        # =====================================================================
        # Step 4: Run Write Tests
        # =====================================================================
        print_section("Step 4: Write Register Tests", "[WRITE]")

        for idx, test in enumerate(WRITE_TESTS, 1):
            print()
            print_box(
                f"Test {idx}/{len(WRITE_TESTS)}: {test['name']}",
                {
                    "Register": test["register_name"],
                    "Value": str(test["value"]),
                    "Expected": "SUCCESS" if test["expected_success"] else "FAIL",
                    "Description": test["description"],
                },
            )

            register_id = register_ids.get(test["register_name"])
            if not register_id:
                print_error(f"Register ID not found for: {test['register_name']}")
                test_results.append(
                    {
                        "name": test["name"],
                        "expected": test["expected_success"],
                        "actual": False,
                        "passed": False,
                        "error": "Register ID not found",
                    }
                )
                continue

            success, response = await client.write_register(
                device_id, register_id, test["value"]
            )

            # Check if result matches expectation
            passed = success == test["expected_success"]

            if passed:
                print_success(f"Test PASSED - Result as expected")
            else:
                print_error(
                    f"Test FAILED - Expected {'success' if test['expected_success'] else 'failure'}, "
                    f"got {'success' if success else 'failure'}"
                )

            test_results.append(
                {
                    "name": test["name"],
                    "expected": test["expected_success"],
                    "actual": success,
                    "passed": passed,
                    "response": response,
                }
            )

            await asyncio.sleep(1)

        # =====================================================================
        # Step 5: Summary
        # =====================================================================
        print_section("Test Summary", "[RESULT]")

        # Results table
        headers = ["#", "Test Name", "Expected", "Actual", "Status"]
        rows = []
        passed_count = 0

        for idx, result in enumerate(test_results, 1):
            expected_str = "PASS" if result["expected"] else "FAIL"
            actual_str = "PASS" if result["actual"] else "FAIL"
            status = "OK" if result["passed"] else "FAIL"

            if result["passed"]:
                passed_count += 1

            rows.append([idx, result["name"][:30], expected_str, actual_str, status])

        print_table(headers, rows, "Write Test Results")

        all_passed = passed_count == len(test_results)

        print_summary(
            "Write Register Tests Complete",
            {
                "Device ID": device_id,
                "Total Tests": str(len(test_results)),
                "Passed": str(passed_count),
                "Failed": str(len(test_results) - passed_count),
                "Success Rate": f"{(passed_count/len(test_results))*100:.1f}%",
            },
            all_passed,
        )

        if all_passed:
            print_success("All write register tests passed!")
        else:
            print_warning("Some tests failed - check the results above")

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
