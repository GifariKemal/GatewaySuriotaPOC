#!/usr/bin/env python3
"""
=============================================================================
SRT-MGATE-1210 Device Testing - RTU Mode
Create THM-30MD Temperature & Humidity Sensor
=============================================================================

Version: 2.0.0 | December 2025 | SURIOTA R&D Team
Firmware: SRT-MGATE-1210 v1.0.x

Device: Axial Labs THM-30MD (SHT30 Sensor)
Datasheet: QS_THM30-MD_v1.1

Device Configuration:
  - Protocol: Modbus RTU
  - Serial Port: Port 1 (ESP32 GPIO)
  - Baud Rate: 9600 (default)
  - Format: 8N1
  - Slave ID: 1 (default)

Registers from Datasheet:
  Data Registers (FC03/FC04):
    - Address 0: Temperature (INT16, /10) -> scale 0.1
    - Address 1: Humidity (INT16, /10) -> scale 0.1
    - Address 2: Temperature (FLOAT32, 2 registers)
    - Address 4: Humidity (FLOAT32, 2 registers)

  Configuration Registers (FC03/FC06):
    - Address 10: Slave ID (UINT16)
    - Address 11: Baudrate (UINT16)

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
# Configuration - THM-30MD Defaults from Datasheet
# =============================================================================
DEVICE_NAME = "THM30MD_TempHumidity"
DEVICE_CONFIG = {
    "device_name": DEVICE_NAME,
    "protocol": "RTU",
    "slave_id": 1,  # Default from datasheet
    "timeout": 5000,
    "retry_count": 3,
    "refresh_rate_ms": 2000,
    "serial_port": 1,  # Serial Port 1
    "baud_rate": 9600,  # Default from datasheet
    "data_bits": 8,
    "parity": "None",
    "stop_bits": 1,
}

# Register definitions from THM-30MD Datasheet
# Data Registers [FC 03 & FC04]
REGISTERS = [
    # Data Registers - Sensor Readings
    {
        "address": 0,
        "name": "Temperature_INT16",
        "desc": "Temperature Data (16-bit Signed/10)",
        "unit": "C",
        "function_code": 3,
        "type": "Holding Registers",
        "data_type": "INT16",
        "scale": 0.1,  # Datasheet: value/10
        "offset": 0.0,
    },
    {
        "address": 1,
        "name": "Humidity_INT16",
        "desc": "Humidity Data (16-bit Signed/10)",
        "unit": "%RH",
        "function_code": 3,
        "type": "Holding Registers",
        "data_type": "INT16",
        "scale": 0.1,  # Datasheet: value/10
        "offset": 0.0,
    },
    {
        "address": 2,
        "name": "Temperature_FLOAT",
        "desc": "Temperature Data (32-bit Float)",
        "unit": "C",
        "function_code": 3,
        "type": "Holding Registers",
        "data_type": "FLOAT32_BE",  # Big-Endian (ABCD) - try FLOAT32_LE_BS if wrong
        "scale": 1.0,
        "offset": 0.0,
    },
    {
        "address": 4,
        "name": "Humidity_FLOAT",
        "desc": "Humidity Data (32-bit Float)",
        "unit": "%RH",
        "function_code": 3,
        "type": "Holding Registers",
        "data_type": "FLOAT32_BE",  # Big-Endian (ABCD) - try FLOAT32_LE_BS if wrong
        "scale": 1.0,
        "offset": 0.0,
    },
    # Configuration Registers [FC 03 & FC06]
    {
        "address": 10,
        "name": "Config_SlaveID",
        "desc": "Slave ID Configuration (1-255)",
        "unit": "",
        "function_code": 3,
        "type": "Holding Registers",
        "data_type": "UINT16",
        "scale": 1.0,
        "offset": 0.0,
    },
    {
        "address": 11,
        "name": "Config_Baudrate",
        "desc": "Baudrate Config (0=4800,1=9600,2=14400,3=19200,4=38400,5=56000,6=57600,7=115200)",
        "unit": "",
        "function_code": 3,
        "type": "Holding Registers",
        "data_type": "UINT16",
        "scale": 1.0,
        "offset": 0.0,
    },
]

NUM_REGISTERS = len(REGISTERS)


# =============================================================================
# Main Program
# =============================================================================
async def main():
    """Main entry point"""
    if not check_dependencies():
        return

    print_header(
        "THM-30MD Device Creation",
        f"Temperature & Humidity Sensor ({NUM_REGISTERS} Registers)",
        "2.0.0",
    )

    client = BLEDeviceClient()
    success_count = 0
    failed_count = 0

    try:
        # =====================================================================
        # Step 1: Connect to Gateway
        # =====================================================================
        print_section("Step 1: BLE Connection", "")

        if not await client.connect():
            print_error("Could not connect to MGate Gateway")
            return

        await asyncio.sleep(1)

        # =====================================================================
        # Step 2: Create RTU Device
        # =====================================================================
        print_section("Step 2: Create THM-30MD Device", "")

        print_box(
            "THM-30MD Configuration (from Datasheet)",
            {
                "Device": "Axial Labs THM-30MD",
                "Sensor": "Sensirion SHT30",
                "Protocol": "Modbus RTU",
                "Serial Port": "Port 1",
                "Baud Rate": "9600 (default)",
                "Format": "8N1",
                "Slave ID": "1 (default)",
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
        print_section(f"Step 3: Create {NUM_REGISTERS} Registers", "")

        print_info(f"Creating {NUM_REGISTERS} registers for THM-30MD")
        print_info("Registers based on QS_THM30-MD_v1.1 datasheet")
        print_info("Using 0.5s delay between registers")
        print()

        # Show register info
        print_info("Data Registers:")
        print_info("  [0] Temperature INT16 (scale 0.1)")
        print_info("  [1] Humidity INT16 (scale 0.1)")
        print_info("  [2] Temperature FLOAT32")
        print_info("  [4] Humidity FLOAT32")
        print_info("Configuration Registers:")
        print_info("  [10] Slave ID")
        print_info("  [11] Baudrate")
        print()

        for idx, reg in enumerate(REGISTERS, 1):
            register_config = {
                "address": reg["address"],
                "register_name": reg["name"],
                "type": reg["type"],
                "function_code": reg["function_code"],
                "data_type": reg["data_type"],
                "description": reg["desc"],
                "unit": reg["unit"],
                "scale": reg["scale"],
                "offset": reg["offset"],
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
        print_section("Summary", "")

        all_success = failed_count == 0

        # Register table
        headers = ["#", "Address", "Name", "Data Type", "Scale", "Status"]
        rows = []
        for idx, reg in enumerate(REGISTERS):
            status = "OK" if idx < success_count else "FAIL"
            rows.append(
                [
                    idx + 1,
                    reg["address"],
                    reg["name"][:20],
                    reg["data_type"],
                    reg["scale"],
                    status,
                ]
            )

        print_table(headers, rows, "THM-30MD Register Status")

        print_summary(
            "THM-30MD Creation Complete",
            {
                "Device ID": device_id,
                "Device Name": DEVICE_NAME,
                "Sensor": "Sensirion SHT30",
                "Protocol": "Modbus RTU @ 9600 8N1",
                "Slave ID": "1",
                "Serial Port": "Port 1",
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
        print_info("THM-30MD Specifications:")
        print_info("  - Temperature Accuracy: +/- 0.2C")
        print_info("  - Humidity Accuracy: +/- 2%")
        print_info("  - Temperature Range: -40 to 125C")
        print_info("  - Humidity Range: 0 to 100%")
        print()
        print_info("Next Steps:")
        print_info("  1. Connect THM-30MD sensor to RS485 Port 1")
        print_info("  2. Ensure power supply 5-36VDC")
        print_info("  3. Monitor Gateway serial output for RTU polling")
        print_info("  4. Check Temperature_INT16/Humidity_INT16 values (divide by 10)")
        print_info("  5. Or use Temperature_FLOAT/Humidity_FLOAT for direct readings")

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
