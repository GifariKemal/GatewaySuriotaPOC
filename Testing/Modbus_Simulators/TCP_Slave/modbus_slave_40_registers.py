#!/usr/bin/env python3
"""
=============================================================================
Modbus TCP Slave Simulator - 40 Input Registers
=============================================================================

Version: 2.0.0 | December 2025 | SURIOTA R&D Team
Compatible with: pymodbus 2.x and 3.x

Device Configuration:
  - IP Address: 192.168.1.100
  - Port: 502
  - Slave ID: 1
  - Function Code: 4 (Read Input Registers)

Dependencies:
  pip install pymodbus colorama

Usage:
  1. Run this simulator
  2. Run the corresponding Device_Testing script
  3. Gateway will poll this simulator

=============================================================================
"""

import logging
import threading
import time
import random
import sys
import os

# Add parent directory to path for shared module
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))

try:
    from simulator_common import (
        print_header,
        print_section,
        print_success,
        print_error,
        print_warning,
        print_info,
        print_table,
        print_box,
        print_register_values,
        print_connection_info,
        print_startup_banner,
        print_ready_message,
        print_dependencies,
        Fore,
        Style,
    )

    COMMON_AVAILABLE = True
except ImportError:
    COMMON_AVAILABLE = False

    # Fallback to basic printing
    def print_header(t, s="", v=""):
        print(f"\n=== {t} ===")

    def print_section(t, i=""):
        print(f"\n--- {t} ---")

    def print_success(m):
        print(f"[OK] {m}")

    def print_error(m):
        print(f"[ERROR] {m}")

    def print_warning(m):
        print(f"[WARN] {m}")

    def print_info(m):
        print(f"[INFO] {m}")

    class Fore:
        RED = GREEN = YELLOW = CYAN = MAGENTA = WHITE = BLUE = RESET = ""

    class Style:
        BRIGHT = DIM = RESET_ALL = ""


# =============================================================================
# pymodbus Import (2.x and 3.x compatible)
# =============================================================================
try:
    # Try pymodbus 3.x first
    try:
        from pymodbus.server import StartTcpServer

        PYMODBUS_VERSION = 3
    except ImportError:
        from pymodbus.server.sync import StartTcpServer

        PYMODBUS_VERSION = 2

    from pymodbus.datastore import (
        ModbusSequentialDataBlock,
        ModbusSlaveContext,
        ModbusServerContext,
    )

    PYMODBUS_AVAILABLE = True
except ImportError as e:
    PYMODBUS_AVAILABLE = False
    print_error(f"pymodbus not installed: {e}")
    print_info("Install with: pip install pymodbus")
    sys.exit(1)


# =============================================================================
# Configuration
# =============================================================================
def get_local_ip():
    """Auto-detect local IP address"""
    import socket

    try:
        # Connect to external address to determine local IP
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        local_ip = s.getsockname()[0]
        s.close()
        return local_ip
    except:
        return "192.168.100.101"  # Fallback


SERVER_IP = get_local_ip()
SERVER_PORT = 502
SLAVE_ID = 1
NUM_REGISTERS = 40

# Register definitions
REGISTER_INFO = {
    0: {"name": "Temp_Zone_1", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    1: {"name": "Temp_Zone_2", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    2: {"name": "Temp_Zone_3", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    3: {"name": "Temp_Zone_4", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    4: {"name": "Temp_Zone_5", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    5: {"name": "Temp_Zone_6", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    6: {"name": "Temp_Zone_7", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    7: {"name": "Temp_Zone_8", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    8: {"name": "Temp_Zone_9", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    9: {"name": "Temp_Zone_10", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    10: {"name": "Temp_Zone_11", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    11: {"name": "Temp_Zone_12", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    12: {"name": "Temp_Zone_13", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    13: {"name": "Temp_Zone_14", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    14: {"name": "Temp_Zone_15", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    15: {"name": "Temp_Zone_16", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    16: {"name": "Temp_Zone_17", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    17: {"name": "Temp_Zone_18", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    18: {"name": "Temp_Zone_19", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    19: {"name": "Temp_Zone_20", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    20: {"name": "Temp_Zone_21", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    21: {"name": "Temp_Zone_22", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    22: {"name": "Temp_Zone_23", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    23: {"name": "Temp_Zone_24", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    24: {"name": "Temp_Zone_25", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    25: {"name": "Temp_Zone_26", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    26: {"name": "Temp_Zone_27", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    27: {"name": "Temp_Zone_28", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    28: {"name": "Temp_Zone_29", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    29: {"name": "Temp_Zone_30", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    30: {"name": "Temp_Zone_31", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    31: {"name": "Temp_Zone_32", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    32: {"name": "Temp_Zone_33", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    33: {"name": "Temp_Zone_34", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    34: {"name": "Temp_Zone_35", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    35: {"name": "Temp_Zone_36", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    36: {"name": "Temp_Zone_37", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    37: {"name": "Temp_Zone_38", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    38: {"name": "Temp_Zone_39", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    39: {"name": "Temp_Zone_40", "unit": "degC", "min": 20, "max": 35, "initial": 25},
}

# Auto-update configuration
AUTO_UPDATE = True
UPDATE_INTERVAL = 2.0

# =============================================================================
# Logging
# =============================================================================
logging.basicConfig(
    format="%(asctime)s [%(levelname)s] %(message)s",
    level=logging.INFO,
    datefmt="%Y-%m-%d %H:%M:%S",
)
log = logging.getLogger(__name__)


# =============================================================================
# Register Updater Thread
# =============================================================================
class RegisterUpdater(threading.Thread):
    """Thread to automatically update register values"""

    def __init__(self, context, slave_id):
        super().__init__()
        self.context = context
        self.slave_id = slave_id
        self.running = True
        self.daemon = True
        self.update_count = 0

    def run(self):
        log.info("Auto-update thread started")

        while self.running:
            try:
                time.sleep(UPDATE_INTERVAL)

                slave_context = self.context[self.slave_id]

                # Update each register with realistic variations
                for address in range(NUM_REGISTERS):
                    if address in REGISTER_INFO:
                        info = REGISTER_INFO[address]
                        current_value = slave_context.getValues(4, address, count=1)[0]

                        # Random walk within bounds
                        change = random.choice([-2, -1, 0, 0, 1, 2])
                        new_value = current_value + change
                        new_value = max(info["min"], min(info["max"], new_value))

                        slave_context.setValues(4, address, [new_value])

                self.update_count += 1

                # Display update
                values = slave_context.getValues(4, 0, count=NUM_REGISTERS)
                print(f"\n{Fore.CYAN}{'='*70}{Style.RESET_ALL}")
                print(
                    f"  {Fore.WHITE}{Style.BRIGHT}Update #{self.update_count:04d} - Slave ID: {SLAVE_ID}{Style.RESET_ALL}"
                )
                print(f"{Fore.CYAN}{'='*70}{Style.RESET_ALL}")

                for addr in range(min(10, NUM_REGISTERS)):  # Show first 10
                    if addr in REGISTER_INFO:
                        info = REGISTER_INFO[addr]
                        value = values[addr]
                        print(
                            f"  [{addr:2d}] {info['name'][:15]:<15s}: {value:5d} {info['unit']}"
                        )

                if NUM_REGISTERS > 10:
                    print(f"  ... and {NUM_REGISTERS - 10} more registers")

                print(f"{Fore.CYAN}{'='*70}{Style.RESET_ALL}")
                print(
                    f"  {Fore.GREEN}Waiting for Modbus TCP requests on {SERVER_IP}:{SERVER_PORT}...{Style.RESET_ALL}\n"
                )

            except Exception as e:
                log.error(f"Update error: {e}")

    def stop(self):
        self.running = False


# =============================================================================
# Main Server
# =============================================================================
def run_server():
    """Setup and run the Modbus TCP server"""

    (
        print_startup_banner("TCP", NUM_REGISTERS)
        if COMMON_AVAILABLE
        else print(f"\n=== TCP Simulator - {NUM_REGISTERS} Registers ===")
    )

    if COMMON_AVAILABLE:
        print_dependencies()

    # Print configuration
    (
        print_section("Configuration", "[CFG]")
        if COMMON_AVAILABLE
        else print("\n--- Configuration ---")
    )

    config = {
        "ip": SERVER_IP,
        "port": SERVER_PORT,
        "slave_id": SLAVE_ID,
        "num_registers": NUM_REGISTERS,
    }

    if COMMON_AVAILABLE:
        print_connection_info("TCP", config)
    else:
        print(f"  IP: {SERVER_IP}:{SERVER_PORT}")
        print(f"  Slave ID: {SLAVE_ID}")
        print(f"  Registers: {NUM_REGISTERS}")

    # Create initial values
    initial_values = [REGISTER_INFO[i]["initial"] for i in range(NUM_REGISTERS)]

    # Create data blocks
    input_registers = ModbusSequentialDataBlock(0, initial_values)

    slave_context = ModbusSlaveContext(
        di=ModbusSequentialDataBlock(0, [0] * 100),
        co=ModbusSequentialDataBlock(0, [0] * 100),
        hr=ModbusSequentialDataBlock(0, [0] * 100),
        ir=input_registers,
        zero_mode=True,
    )

    server_context = ModbusServerContext(slaves={SLAVE_ID: slave_context}, single=False)

    # Display initial values
    (
        print_section("Initial Values", "[REG]")
        if COMMON_AVAILABLE
        else print("\n--- Initial Values ---")
    )

    headers = ["Addr", "Name", "Value", "Unit", "Range"]
    rows = []
    for addr in range(min(15, NUM_REGISTERS)):
        info = REGISTER_INFO[addr]
        rows.append(
            [
                addr,
                info["name"][:15],
                info["initial"],
                info["unit"],
                f"{info['min']}-{info['max']}",
            ]
        )

    if COMMON_AVAILABLE:
        from simulator_common import print_table

        print_table(headers, rows)
    else:
        for row in rows:
            print(f"  [{row[0]:2d}] {row[1]:<15s}: {row[2]:5d} {row[3]}")

    if NUM_REGISTERS > 15:
        print(f"  ... and {NUM_REGISTERS - 15} more registers")

    # Start auto-update thread
    updater = None
    if AUTO_UPDATE:
        updater = RegisterUpdater(server_context, SLAVE_ID)
        updater.start()
        (
            print_info(f"Auto-update enabled ({UPDATE_INTERVAL}s interval)")
            if COMMON_AVAILABLE
            else print(f"[INFO] Auto-update: {UPDATE_INTERVAL}s")
        )

    # Ready message
    if COMMON_AVAILABLE:
        print_ready_message("TCP", config)
    else:
        print(f"\n[INFO] Server ready on {SERVER_IP}:{SERVER_PORT}")
        print("[INFO] Press Ctrl+C to stop")

    try:
        log.info(
            f"Starting TCP server on {SERVER_IP}:{SERVER_PORT} (pymodbus {PYMODBUS_VERSION}.x)"
        )
        StartTcpServer(context=server_context, address=(SERVER_IP, SERVER_PORT))
    except KeyboardInterrupt:
        print("\n\n[INFO] Shutting down...")
        if updater:
            updater.stop()
            updater.join(timeout=2)
        print("[INFO] Server stopped")
    except Exception as e:
        log.error(f"Server error: {e}")
        if updater:
            updater.stop()
        raise


# =============================================================================
# Main
# =============================================================================
if __name__ == "__main__":
    print()
    print("=" * 60)
    print(f"  Modbus TCP Slave Simulator - {NUM_REGISTERS} Registers")
    print("=" * 60)
    print(f"  Target: {SERVER_IP}:{SERVER_PORT}")
    print(f"  Slave ID: {SLAVE_ID}")
    print(f"  pymodbus: v{PYMODBUS_VERSION}.x API")
    print("=" * 60)
    print()

    input("Press Enter to start server...")

    run_server()
