#!/usr/bin/env python3
"""
=============================================================================
Modbus TCP Slave Simulator - 50 Input Registers
=============================================================================
Description:
    Simulates a Modbus TCP slave matching the Device_Testing configuration

    Device Configuration:
    - IP Address: 192.168.1.8
    - Port: 502
    - Slave ID: 1
    - Protocol: Modbus TCP

    Registers (50 Input Registers) - ALIGNED WITH create_device_50_registers.py:

    Temperature Zones (0-9):
    - Address 0-9: Temp_Zone_1 to Temp_Zone_10 (°C)  - Range: 20-35

    Humidity Zones (10-19):
    - Address 10-19: Humid_Zone_1 to Humid_Zone_10 (%)  - Range: 40-80

    Pressure Sensors (20-24):
    - Address 20-24: Press_Sensor_1 to Press_Sensor_5 (Pa)  - Range: 900-1100

    Voltage Lines (25-29):
    - Address 25-29: Voltage_L1 to Voltage_L5 (V)  - Range: 220-240

    Current Lines (30-34):
    - Address 30-34: Current_L1 to Current_L5 (A)  - Range: 1-10

    Power Meters (35-39):
    - Address 35-39: Power_1 to Power_5 (W)  - Range: 0-2400

    Energy Counters (40-44):
    - Address 40-44: Energy_1 to Energy_5 (kWh)  - Range: 0-1000

    Flow Meters (45-49):
    - Address 45-49: Flow_1 to Flow_5 (L/min)  - Range: 0-100

Usage:
    1. Install: pip install pymodbus
    2. Run: python modbus_slave_50_registers.py
    3. Gateway can connect to 192.168.1.8:502

Requirements:
    pip install pymodbus

Compatible with: pymodbus 3.x

Author: Kemal - SURIOTA R&D Team
Date: 2025-11-15
=============================================================================
"""

import logging
import threading
import time
import random
import sys

# Check pymodbus installation
try:
    from pymodbus.server.sync import StartTcpServer
    from pymodbus.datastore import ModbusSequentialDataBlock, ModbusSlaveContext, ModbusServerContext
    PYMODBUS_IMPORTED = True
except ImportError as e:
    print("\n" + "="*70)
    print("  ERROR: pymodbus not installed!")
    print("="*70)
    print(f"\n  {e}\n")
    print("  Please install pymodbus:")
    print("  -> pip install pymodbus")
    print("\n  Or:")
    print("  -> pip3 install pymodbus")
    print("\n" + "="*70 + "\n")
    sys.exit(1)

# =============================================================================
# Configuration (Match with Device_Testing/create_device_50_registers.py)
# =============================================================================
SERVER_IP = '192.168.100.101'  # MUST match device config IP
SERVER_PORT = 502          # MUST match device config port
SLAVE_ID = 1               # MUST match device config slave_id
NUM_REGISTERS = 50          # 50 Input Registers

# Register definitions - ALIGNED WITH create_device_50_registers.py
# Temperature Zones: 0-9 (10 registers)
# Humidity Zones: 10-19 (10 registers)
# Pressure Sensors: 20-24 (5 registers)
# Voltage Lines: 25-29 (5 registers)
# Current Lines: 30-34 (5 registers)
# Power Meters: 35-39 (5 registers)
# Energy Counters: 40-44 (5 registers)
# Flow Meters: 45-49 (5 registers)

REGISTER_INFO = {}

# Temperature Zones (0-9) - °C
for i in range(10):
    REGISTER_INFO[i] = {
        "name": f"Temp_Zone_{i+1}",
        "desc": f"Temperature Zone {i+1}",
        "unit": "°C",
        "min": 20,
        "max": 35,
        "initial": 25
    }

# Humidity Zones (10-19) - %
for i in range(10):
    REGISTER_INFO[10 + i] = {
        "name": f"Humid_Zone_{i+1}",
        "desc": f"Humidity Zone {i+1}",
        "unit": "%",
        "min": 40,
        "max": 80,
        "initial": 60
    }

# Pressure Sensors (20-24) - Pa
for i in range(5):
    REGISTER_INFO[20 + i] = {
        "name": f"Press_Sensor_{i+1}",
        "desc": f"Pressure Sensor {i+1}",
        "unit": "Pa",
        "min": 900,
        "max": 1100,
        "initial": 1000
    }

# Voltage Lines (25-29) - V
for i in range(5):
    REGISTER_INFO[25 + i] = {
        "name": f"Voltage_L{i+1}",
        "desc": f"Voltage Line {i+1}",
        "unit": "V",
        "min": 220,
        "max": 240,
        "initial": 230
    }

# Current Lines (30-34) - A
for i in range(5):
    REGISTER_INFO[30 + i] = {
        "name": f"Current_L{i+1}",
        "desc": f"Current Line {i+1}",
        "unit": "A",
        "min": 1,
        "max": 10,
        "initial": 5
    }

# Power Meters (35-39) - W
for i in range(5):
    REGISTER_INFO[35 + i] = {
        "name": f"Power_{i+1}",
        "desc": f"Power Meter {i+1}",
        "unit": "W",
        "min": 0,
        "max": 2400,
        "initial": 1200
    }

# Energy Counters (40-44) - kWh
for i in range(5):
    REGISTER_INFO[40 + i] = {
        "name": f"Energy_{i+1}",
        "desc": f"Energy Counter {i+1}",
        "unit": "kWh",
        "min": 0,
        "max": 1000,
        "initial": 500
    }

# Flow Meters (45-49) - L/min
for i in range(5):
    REGISTER_INFO[45 + i] = {
        "name": f"Flow_{i+1}",
        "desc": f"Flow Meter {i+1}",
        "unit": "L/min",
        "min": 0,
        "max": 100,
        "initial": 50
    }

# Auto-update configuration
AUTO_UPDATE = True
UPDATE_INTERVAL = 3.0  # Update every 3 seconds

# =============================================================================
# Logging Configuration
# =============================================================================
logging.basicConfig(
    format='%(asctime)s [%(levelname)s] %(message)s',
    level=logging.INFO,
    datefmt='%Y-%m-%d %H:%M:%S'
)
log = logging.getLogger(__name__)

# =============================================================================
# Display Banner
# =============================================================================
def print_banner():
    """Display startup banner"""
    try:
        import pymodbus
        version_str = pymodbus.__version__
    except:
        version_str = "Unknown"

    print("\n" + "="*70)
    print("  MODBUS TCP SLAVE SIMULATOR")
    print("  SRT-MGATE-1210 Testing - 50 Input Registers")
    print("="*70)
    print(f"  Configuration:")
    print(f"  - IP Address:     {SERVER_IP}")
    print(f"  - Port:           {SERVER_PORT}")
    print(f"  - Slave ID:       {SLAVE_ID}")
    print(f"  - Registers:      {NUM_REGISTERS} Input Registers (Function Code 4)")
    print(f"  - Data Type:      INT16 (0-65535)")
    print(f"  - Auto Update:    {'Enabled' if AUTO_UPDATE else 'Disabled'}")
    print(f"  - Update Interval: {UPDATE_INTERVAL}s")
    print(f"  - PyModbus:       v{version_str}")
    print("="*70)
    print("\n[INFO] Server is starting...")

# =============================================================================
# Auto-Update Thread
# =============================================================================
class RegisterUpdater(threading.Thread):
    """Thread to automatically update register values with realistic simulation"""

    def __init__(self, context, slave_id):
        super().__init__()
        self.context = context
        self.slave_id = slave_id
        self.running = True
        self.daemon = True
        self.update_count = 0

    def run(self):
        """Main update loop"""
        log.info("Auto-update thread started")

        while self.running:
            try:
                time.sleep(UPDATE_INTERVAL)

                # Get the slave context
                slave_context = self.context[self.slave_id]

                # Update each register with realistic variations
                for address in range(NUM_REGISTERS):
                    info = REGISTER_INFO[address]

                    # Get current value (function code 4 = Input Registers)
                    current_value = slave_context.getValues(4, address, count=1)[0]

                    # Simulate realistic variations based on register type
                    variation_range = max(1, (info["max"] - info["min"]) // 20)  # 5% variation

                    # Random walk with bounds
                    change = random.randint(-variation_range, variation_range)
                    new_value = current_value + change
                    new_value = max(info["min"], min(info["max"], new_value))

                    # Set new value
                    slave_context.setValues(4, address, [new_value])

                self.update_count += 1

                # Log updated values
                values = slave_context.getValues(4, 0, count=NUM_REGISTERS)
                log.info(f"Update #{self.update_count:04d} - Values: {values}")

                # Print detailed values every update
                print(f"\n{'─'*70}")
                print(f"  Update #{self.update_count:04d} - Register Values:")
                print(f"{'─'*70}")
                for addr in range(NUM_REGISTERS):
                    info = REGISTER_INFO[addr]
                    value = values[addr]
                    print(f"  [{addr:2d}] {info['name']:15s}: {value:5d} {info['unit']:4s}")
                print(f"{'─'*70}\n")

            except Exception as e:
                log.error(f"Error in auto-update thread: {e}")
                import traceback
                traceback.print_exc()

    def stop(self):
        """Stop the update thread"""
        self.running = False

# =============================================================================
# Main Server Setup
# =============================================================================
def run_server():
    """Setup and run the Modbus TCP server"""

    print_banner()

    # Create initial values for Input Registers
    initial_values = [REGISTER_INFO[i]["initial"] for i in range(NUM_REGISTERS)]

    # Create data blocks for Input Registers (function code 4)
    input_registers = ModbusSequentialDataBlock(
        0,  # Starting address
        initial_values
    )

    # Create slave context with only Input Registers
    slave_context = ModbusSlaveContext(
        di=ModbusSequentialDataBlock(0, [0]*100),  # Discrete Inputs (not used)
        co=ModbusSequentialDataBlock(0, [0]*100),  # Coils (not used)
        hr=ModbusSequentialDataBlock(0, [0]*100),  # Holding Registers (not used)
        ir=input_registers,                         # Input Registers (USED)
        zero_mode=True  # Use 0-based addressing
    )

    # Create server context with single slave
    server_context = ModbusServerContext(
        slaves={SLAVE_ID: slave_context},
        single=False
    )

    # Display initial register values
    print("\n" + "="*70)
    print("  INITIAL REGISTER VALUES (Input Registers)")
    print("="*70)
    print(f"  {'Addr':<6} {'Name':<15} {'Value':<10} {'Unit':<10} {'Range':<20}")
    print("─"*70)
    for addr in range(NUM_REGISTERS):
        info = REGISTER_INFO[addr]
        value = info["initial"]
        range_str = f"{info['min']}-{info['max']}"
        print(f"  {addr:<6} {info['name']:<15} {value:<10} {info['unit']:<10} {range_str:<20}")
    print("="*70)

    # Get computer IP addresses
    try:
        import socket
        hostname = socket.gethostname()
        local_ip = socket.gethostbyname(hostname)
        print(f"\n[INFO] Computer hostname: {hostname}")
        print(f"[INFO] Local IP address: {local_ip}")
        if local_ip != SERVER_IP:
            print(f"[WARNING] Server IP ({SERVER_IP}) != Local IP ({local_ip})")
            print(f"[WARNING] Make sure {SERVER_IP} is configured on your network adapter!")
    except Exception as e:
        log.warning(f"Could not get local IP: {e}")

    print(f"\n[INFO] Server listening on {SERVER_IP}:{SERVER_PORT}")
    print(f"[INFO] Slave ID: {SLAVE_ID}")
    print(f"[INFO] Function Code: 4 (Read Input Registers)")
    print(f"[INFO] Register Addresses: 0-{NUM_REGISTERS-1}")
    print(f"\n[INFO] Ready for connections from SRT-MGATE-1210 Gateway")
    print(f"[INFO] Press Ctrl+C to stop server\n")

    # Start auto-update thread if enabled
    updater = None
    if AUTO_UPDATE:
        updater = RegisterUpdater(server_context, SLAVE_ID)
        updater.start()
        log.info("Auto-update enabled - register values will change every %.1fs", UPDATE_INTERVAL)

    try:
        # Start TCP server (blocks until stopped)
        StartTcpServer(context=server_context, address=(SERVER_IP, SERVER_PORT))
    except KeyboardInterrupt:
        print("\n\n[INFO] Shutting down server...")
        if updater:
            updater.stop()
            updater.join(timeout=2)
        print("[INFO] Server stopped gracefully")
    except Exception as e:
        log.error(f"Server error: {e}")
        import traceback
        traceback.print_exc()
        if updater:
            updater.stop()
        raise

# =============================================================================
# Main Entry Point
# =============================================================================
if __name__ == "__main__":
    print("\n" + "="*70)
    print("  MODBUS TCP SLAVE SIMULATOR")
    print("  SRT-MGATE-1210 Firmware Testing Tool")
    print("="*70)
    print(f"\n  Configuration:")
    print(f"  ├─ IP:Port:      {SERVER_IP}:{SERVER_PORT}")
    print(f"  ├─ Slave ID:     {SLAVE_ID}")
    print(f"  ├─ Registers:    {NUM_REGISTERS} Input Registers (INT16)")
    print(f"  └─ Addresses:    0-{NUM_REGISTERS-1}")

    print(f"\n  Register Mapping:")
    for addr in range(min(10, NUM_REGISTERS)):  # Show first 10
        info = REGISTER_INFO[addr]
        print(f"  [{addr}] {info['name']:15s} - {info['unit']:4s} ({info['min']}-{info['max']})")
    if NUM_REGISTERS > 10:
        print(f"  ... and {NUM_REGISTERS - 10} more registers")

    print(f"\n  Gateway Configuration (use in Device_Testing):")
    print(f"  ├─ IP Address:   {SERVER_IP}")
    print(f"  ├─ Port:         {SERVER_PORT}")
    print(f"  ├─ Slave ID:     {SLAVE_ID}")
    print(f"  ├─ Protocol:     TCP")
    print(f"  └─ Function:     Read Input Registers (FC 4)")

    print("\n" + "="*70)
    print("\n  This simulator matches the configuration from:")
    print("  Device_Testing/create_device_50_registers.py")
    print("\n" + "="*70 + "\n")

    input("Press Enter to start server...")

    run_server()
