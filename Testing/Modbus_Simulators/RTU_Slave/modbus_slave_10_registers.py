#!/usr/bin/env python3
"""
=============================================================================
Modbus RTU Slave Simulator - 10 Input Registers
=============================================================================

Version: 2.0.0 | December 2025 | SURIOTA R&D Team
Compatible with: pymodbus 2.x and 3.x

Serial Configuration:
  - Port: COM8 (Windows) or /dev/ttyUSB0 (Linux)
  - Baud Rate: 9600
  - Format: 8N1
  - Slave ID: 1
  - Function Code: 4 (Read Input Registers)

Dependencies:
  pip install pymodbus pyserial colorama

Usage:
  1. Connect USB-to-RS485 adapter
  2. Run this simulator
  3. Gateway will poll via RTU

=============================================================================
"""

import logging
import threading
import time
import random
import sys
import os
import platform

# Add parent directory to path for shared module
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..'))

try:
    from simulator_common import (
        print_header, print_section, print_success, print_error, print_warning,
        print_info, print_table, print_box, print_register_values,
        print_connection_info, print_startup_banner, print_ready_message,
        print_dependencies, Fore, Style
    )
    COMMON_AVAILABLE = True
except ImportError:
    COMMON_AVAILABLE = False
    def print_header(t, s="", v=""): print(f"\n=== {t} ===")
    def print_section(t, i=""): print(f"\n--- {t} ---")
    def print_success(m): print(f"[OK] {m}")
    def print_error(m): print(f"[ERROR] {m}")
    def print_warning(m): print(f"[WARN] {m}")
    def print_info(m): print(f"[INFO] {m}")
    class Fore:
        RED = GREEN = YELLOW = CYAN = MAGENTA = WHITE = BLUE = RESET = ""
    class Style:
        BRIGHT = DIM = RESET_ALL = ""

# =============================================================================
# pymodbus Import (2.x and 3.x compatible)
# =============================================================================
try:
    try:
        from pymodbus.server import StartSerialServer
        PYMODBUS_VERSION = 3
    except ImportError:
        from pymodbus.server.sync import StartSerialServer
        PYMODBUS_VERSION = 2

    from pymodbus.datastore import ModbusSequentialDataBlock, ModbusSlaveContext, ModbusServerContext

    try:
        from pymodbus.transaction import ModbusRtuFramer
    except ImportError:
        from pymodbus.framer.rtu_framer import ModbusRtuFramer

    PYMODBUS_AVAILABLE = True
except ImportError as e:
    PYMODBUS_AVAILABLE = False
    print_error(f"pymodbus not installed: {e}")
    print_info("Install with: pip install pymodbus pyserial")
    sys.exit(1)

# =============================================================================
# Configuration
# =============================================================================
if platform.system() == 'Windows':
    SERIAL_PORT = 'COM8'
else:
    SERIAL_PORT = '/dev/ttyUSB0'

BAUD_RATE = 9600
DATA_BITS = 8
PARITY = 'N'
STOP_BITS = 1
SLAVE_ID = 1
NUM_REGISTERS = 10

# Register definitions
REGISTER_INFO = {
    0: {"name": "Temperature", "unit": "degC", "min": 20, "max": 35, "initial": 25},
    1: {"name": "Humidity", "unit": "%", "min": 40, "max": 80, "initial": 60},
    2: {"name": "Pressure", "unit": "Pa", "min": 900, "max": 1100, "initial": 1000},
    3: {"name": "Voltage", "unit": "V", "min": 220, "max": 240, "initial": 230},
    4: {"name": "Current", "unit": "A", "min": 1, "max": 10, "initial": 5},
    5: {"name": "Power", "unit": "W", "min": 0, "max": 5000, "initial": 1000},
    6: {"name": "Frequency", "unit": "Hz", "min": 48, "max": 52, "initial": 50},
    7: {"name": "PowerFactor", "unit": "%", "min": 0, "max": 100, "initial": 95},
    8: {"name": "Energy", "unit": "kWh", "min": 0, "max": 9999, "initial": 100},
    9: {"name": "Flow", "unit": "L/min", "min": 0, "max": 100, "initial": 50}
}

# Auto-update configuration
AUTO_UPDATE = True
UPDATE_INTERVAL = 2.0

# =============================================================================
# Logging
# =============================================================================
logging.basicConfig(
    format='%(asctime)s [%(levelname)s] %(message)s',
    level=logging.INFO,
    datefmt='%Y-%m-%d %H:%M:%S'
)
log = logging.getLogger(__name__)

# =============================================================================
# Serial Port Check
# =============================================================================
def verify_serial_port():
    """Check if serial port is available"""
    try:
        import serial.tools.list_ports
        ports = serial.tools.list_ports.comports()
        available = [p.device for p in ports]

        print_section("Available Serial Ports", "[COM]") if COMMON_AVAILABLE else print("\n--- Serial Ports ---")

        if not available:
            print_warning("No serial ports detected!") if COMMON_AVAILABLE else print("[WARN] No ports found")
            return False

        for port in ports:
            if port.device == SERIAL_PORT:
                print_success(f"{port.device}: {port.description}") if COMMON_AVAILABLE else print(f"[OK] {port.device}")
            else:
                print_info(f"{port.device}: {port.description}") if COMMON_AVAILABLE else print(f"[INFO] {port.device}")

        if SERIAL_PORT not in available:
            print_warning(f"Configured port '{SERIAL_PORT}' not found!") if COMMON_AVAILABLE else print(f"[WARN] {SERIAL_PORT} not found")
            return False

        return True
    except Exception as e:
        log.warning(f"Could not verify serial port: {e}")
        return True

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

                for address in range(NUM_REGISTERS):
                    if address in REGISTER_INFO:
                        info = REGISTER_INFO[address]
                        current_value = slave_context.getValues(4, address, count=1)[0]

                        change = random.choice([-2, -1, 0, 0, 1, 2])
                        new_value = current_value + change
                        new_value = max(info["min"], min(info["max"], new_value))

                        slave_context.setValues(4, address, [new_value])

                self.update_count += 1

                values = slave_context.getValues(4, 0, count=NUM_REGISTERS)
                print(f"\n{Fore.CYAN}{'='*70}{Style.RESET_ALL}")
                print(f"  {Fore.WHITE}{Style.BRIGHT}Update #{self.update_count:04d} - Slave ID: {SLAVE_ID}{Style.RESET_ALL}")
                print(f"{Fore.CYAN}{'='*70}{Style.RESET_ALL}")

                for addr in range(min(10, NUM_REGISTERS)):
                    if addr in REGISTER_INFO:
                        info = REGISTER_INFO[addr]
                        value = values[addr]
                        print(f"  [{addr:2d}] {info['name'][:15]:<15s}: {value:5d} {info['unit']}")

                if NUM_REGISTERS > 10:
                    print(f"  ... and {NUM_REGISTERS - 10} more registers")

                print(f"{Fore.CYAN}{'='*70}{Style.RESET_ALL}")
                print(f"  {Fore.GREEN}Waiting for Modbus RTU requests on {SERIAL_PORT}...{Style.RESET_ALL}\n")

            except Exception as e:
                log.error(f"Update error: {e}")

    def stop(self):
        self.running = False

# =============================================================================
# Main Server
# =============================================================================
def run_server():
    """Setup and run the Modbus RTU server"""

    print_startup_banner("RTU", NUM_REGISTERS) if COMMON_AVAILABLE else print(f"\n=== RTU Simulator - {NUM_REGISTERS} Registers ===")

    if COMMON_AVAILABLE:
        print_dependencies()

    if not verify_serial_port():
        print_warning("Serial port verification failed") if COMMON_AVAILABLE else print("[WARN] Port check failed")
        response = input("\nContinue anyway? (y/n): ")
        if response.lower() != 'y':
            return

    print_section("Configuration", "[CFG]") if COMMON_AVAILABLE else print("\n--- Configuration ---")

    config = {
        "port": SERIAL_PORT,
        "baud_rate": BAUD_RATE,
        "data_bits": DATA_BITS,
        "parity": PARITY,
        "stop_bits": STOP_BITS,
        "slave_id": SLAVE_ID,
        "num_registers": NUM_REGISTERS
    }

    if COMMON_AVAILABLE:
        print_connection_info("RTU", config)
    else:
        print(f"  Port: {SERIAL_PORT}")
        print(f"  Baud: {BAUD_RATE}")
        print(f"  Slave ID: {SLAVE_ID}")

    initial_values = [REGISTER_INFO[i]["initial"] for i in range(NUM_REGISTERS)]

    input_registers = ModbusSequentialDataBlock(0, initial_values)

    slave_context = ModbusSlaveContext(
        di=ModbusSequentialDataBlock(0, [0]*100),
        co=ModbusSequentialDataBlock(0, [0]*100),
        hr=ModbusSequentialDataBlock(0, [0]*100),
        ir=input_registers,
        zero_mode=True
    )

    server_context = ModbusServerContext(
        slaves={SLAVE_ID: slave_context},
        single=False
    )

    print_section("Initial Values", "[REG]") if COMMON_AVAILABLE else print("\n--- Initial Values ---")

    headers = ["Addr", "Name", "Value", "Unit", "Range"]
    rows = []
    for addr in range(min(15, NUM_REGISTERS)):
        info = REGISTER_INFO[addr]
        rows.append([addr, info["name"][:15], info["initial"], info["unit"], f"{info['min']}-{info['max']}"])

    if COMMON_AVAILABLE:
        from simulator_common import print_table
        print_table(headers, rows)
    else:
        for row in rows:
            print(f"  [{row[0]:2d}] {row[1]:<15s}: {row[2]:5d} {row[3]}")

    if NUM_REGISTERS > 15:
        print(f"  ... and {NUM_REGISTERS - 15} more registers")

    updater = None
    if AUTO_UPDATE:
        updater = RegisterUpdater(server_context, SLAVE_ID)
        updater.start()
        print_info(f"Auto-update enabled ({UPDATE_INTERVAL}s interval)") if COMMON_AVAILABLE else print(f"[INFO] Auto-update: {UPDATE_INTERVAL}s")

    if COMMON_AVAILABLE:
        print_ready_message("RTU", config)
    else:
        print(f"\n[INFO] Server ready on {SERIAL_PORT}")
        print("[INFO] Press Ctrl+C to stop")

    try:
        log.info(f"Starting RTU server on {SERIAL_PORT} (pymodbus {PYMODBUS_VERSION}.x)")
        StartSerialServer(
            context=server_context,
            framer=ModbusRtuFramer,
            port=SERIAL_PORT,
            baudrate=BAUD_RATE,
            bytesize=DATA_BITS,
            parity=PARITY,
            stopbits=STOP_BITS,
            timeout=1
        )
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
        if "could not open port" in str(e).lower():
            print_error(f"Could not open {SERIAL_PORT}") if COMMON_AVAILABLE else print(f"[ERROR] Port error")
            print_info("Check: USB adapter connected, port not in use, correct port number")
        raise

# =============================================================================
# Main
# =============================================================================
if __name__ == "__main__":
    print()
    print("=" * 60)
    print(f"  Modbus RTU Slave Simulator - {NUM_REGISTERS} Registers")
    print("=" * 60)
    print(f"  Port: {SERIAL_PORT}")
    print(f"  Baud: {BAUD_RATE}, Format: {DATA_BITS}{PARITY}{STOP_BITS}")
    print(f"  Slave ID: {SLAVE_ID}")
    print(f"  pymodbus: v{PYMODBUS_VERSION}.x API")
    print("=" * 60)
    print()

    input("Press Enter to start server...")

    run_server()
