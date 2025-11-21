#!/usr/bin/env python3
"""
=============================================================================
Modbus TCP Slave Simulator - 5 Input Registers
=============================================================================
Description:
    Simulates a Modbus TCP slave matching the Device_Testing configuration

    Device Configuration:
    - IP Address: 192.168.1.8
    - Port: 502
    - Slave ID: 1
    - Protocol: Modbus TCP


    Registers (5 Input Registers) - ALIGNED WITH create_device_5_registers.py:
    - Address 0-4: Temp_Zone_1 to Temp_Zone_5 (°C) - Range: 20-35°C

Usage:
    1. Install: pip install pymodbus
    2. Run: python modbus_slave_5_registers.py
    3. Gateway can connect to 192.168.1.8:502

Requirements:
    pip install pymodbus

Compatible with: pymodbus 3.x

Author: Kemal - SURIOTA R&D Team
Date: 2025-11-14
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
    print("  → pip install pymodbus")
    print("\n  Or:")
    print("  → pip3 install pymodbus")
    print("\n" + "="*70 + "\n")
    sys.exit(1)

# =============================================================================
# Configuration (Match with Device_Testing/create_device_5_registers.py)
# =============================================================================
SERVER_IP = '192.168.1.8'  # MUST match device config IP
SERVER_PORT = 502          # MUST match device config port
SLAVE_ID = 1               # MUST match device config slave_id
NUM_REGISTERS = 5          # 5 Input Registers

# Register definitions - ALIGNED with create_device_5_registers.py
# Temperature Zones 1-5 (addresses 0-4)
REGISTER_INFO = {}
for i in range(5):
    REGISTER_INFO[i] = {
        "name": f"Temp_Zone_{i+1}",
        "desc": f"Temperature Zone {i+1}",
        "unit": "°C",
        "min": 20,
        "max": 35,
        "initial": 25
    }

# Auto-update configuration
AUTO_UPDATE = True
UPDATE_INTERVAL = 2.0  # Update every 10 seconds (matches gateway refresh_rate_ms: 1000ms)

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
    print("  SRT-MGATE-1210 Testing - 5 Input Registers")
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

                    # Simulate realistic sensor variations
                    if address == 0:  # Temperature: slow changes ±1°C
                        new_value = current_value + random.choice([-1, 0, 0, 1])
                        new_value = max(info["min"], min(info["max"], new_value))

                    elif address == 1:  # Humidity: moderate changes ±2%
                        new_value = current_value + random.choice([-2, -1, 0, 1, 2])
                        new_value = max(info["min"], min(info["max"], new_value))

                    elif address == 2:  # Pressure: small changes ±5 Pa
                        new_value = current_value + random.randint(-5, 5)
                        new_value = max(info["min"], min(info["max"], new_value))

                    elif address == 3:  # Voltage: very stable ±1V
                        new_value = current_value + random.choice([-1, 0, 0, 0, 1])
                        new_value = max(info["min"], min(info["max"], new_value))

                    elif address == 4:  # Current: changes ±1A
                        new_value = current_value + random.choice([-1, 0, 1])
                        new_value = max(info["min"], min(info["max"], new_value))

                    else:
                        new_value = current_value

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
                    print(f"  [{addr}] {info['name']:12s}: {value:5d} {info['unit']:4s}")
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
    for addr in range(NUM_REGISTERS):
        info = REGISTER_INFO[addr]
        print(f"  [{addr}] {info['name']:12s} - {info['unit']:4s} ({info['min']}-{info['max']})")

    print(f"\n  Gateway Configuration (use in Device_Testing):")
    print(f"  ├─ IP Address:   {SERVER_IP}")
    print(f"  ├─ Port:         {SERVER_PORT}")
    print(f"  ├─ Slave ID:     {SLAVE_ID}")
    print(f"  ├─ Protocol:     TCP")
    print(f"  └─ Function:     Read Input Registers (FC 4)")

    print("\n" + "="*70)
    print("\n  This simulator matches the configuration from:")
    print("  Device_Testing/create_device_5_registers.py")
    print("\n" + "="*70 + "\n")

    input("Press Enter to start server...")

    run_server()
