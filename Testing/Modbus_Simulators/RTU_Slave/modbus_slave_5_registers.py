#!/usr/bin/env python3
"""
=============================================================================
Modbus RTU Slave Simulator - 5 Input Registers
=============================================================================
Description:
    Simulates a Modbus RTU slave matching the Device_Testing configuration

    Device Configuration:
    - Serial Port: COM8 (Windows) or /dev/ttyUSB0 (Linux)
    - Baud Rate: 9600
    - Data Bits: 8
    - Parity: None
    - Stop Bits: 1
    - Slave ID: 1
    - Protocol: Modbus RTU
    - Flow Control: RTS Toggle (RTS disable delay: 1ms)

    Registers (5 Input Registers - Function Code 4):
    - Address 0: Temperature (°C)    - Range: 20-35°C
    - Address 1: Humidity (%)        - Range: 40-80%
    - Address 2: Pressure (Pa)       - Range: 900-1100 Pa
    - Address 3: Voltage (V)         - Range: 220-240 V
    - Address 4: Current (A)         - Range: 1-10 A

Usage:
    1. Install: pip install pymodbus pyserial
    2. Configure COM port in script (default: COM8)
    3. Run: python modbus_slave_5_registers.py
    4. Gateway will poll via RTU at configured refresh rate (2000ms default)

Requirements:
    pip install pymodbus pyserial

Compatible with: pymodbus 3.x

Author: Kemal - SURIOTA R&D Team
Date: 2025-11-17
Firmware Target: SRT-MGATE-1210 v2.2.0
=============================================================================
"""

import logging
import threading
import time
import random
import sys
import platform

# Check pymodbus and pyserial installation
try:
    # Try pymodbus 3.x import first
    try:
        from pymodbus.server import StartSerialServer
        PYMODBUS_VERSION = 3
    except ImportError:
        # Fall back to pymodbus 2.x import
        from pymodbus.server.sync import StartSerialServer
        PYMODBUS_VERSION = 2

    from pymodbus.datastore import ModbusSequentialDataBlock, ModbusSlaveContext, ModbusServerContext

    # ModbusRtuFramer import handling for version compatibility
    try:
        from pymodbus.transaction import ModbusRtuFramer
    except ImportError:
        # pymodbus 2.x uses different import
        from pymodbus.framer.rtu_framer import ModbusRtuFramer

    PYMODBUS_IMPORTED = True
except ImportError as e:
    print("\n" + "="*70)
    print("  ERROR: Required libraries not installed!")
    print("="*70)
    print(f"\n  {e}\n")
    print("  Please install required packages:")
    print("  → pip install pymodbus pyserial")
    print("\n  Or:")
    print("  → pip3 install pymodbus pyserial")
    print("\n" + "="*70 + "\n")
    sys.exit(1)

try:
    import serial
    PYSERIAL_IMPORTED = True
except ImportError as e:
    print("\n" + "="*70)
    print("  ERROR: pyserial not installed!")
    print("="*70)
    print(f"\n  {e}\n")
    print("  Please install pyserial:")
    print("  → pip install pyserial")
    print("\n" + "="*70 + "\n")
    sys.exit(1)

# =============================================================================
# Configuration (Match with Device_Testing/RTU/create_device_5_registers.py)
# =============================================================================
# Serial Port Configuration - ADJUST FOR YOUR SYSTEM
if platform.system() == 'Windows':
    SERIAL_PORT = 'COM8'  # MUST match device_testing configuration
else:
    SERIAL_PORT = '/dev/ttyUSB0'  # Linux/Raspberry Pi

BAUD_RATE = 9600       # MUST match device config
DATA_BITS = 8          # MUST match device config (8N1)
PARITY = 'N'           # None parity (8N1)
STOP_BITS = 1          # MUST match device config
SLAVE_ID = 1           # MUST match device config slave_id
NUM_REGISTERS = 5      # 5 Input Registers

# Register definitions (matching create_device_5_registers.py)
REGISTER_INFO = {
    0: {"name": "Temperature", "unit": "°C",  "min": 20,   "max": 35,   "initial": 25},
    1: {"name": "Humidity",    "unit": "%",   "min": 40,   "max": 80,   "initial": 60},
    2: {"name": "Pressure",    "unit": "Pa",  "min": 900,  "max": 1100, "initial": 1000},
    3: {"name": "Voltage",     "unit": "V",   "min": 220,  "max": 240,  "initial": 230},
    4: {"name": "Current",     "unit": "A",   "min": 1,    "max": 10,   "initial": 5}
}

# Auto-update configuration
AUTO_UPDATE = True
UPDATE_INTERVAL = 1.0  # Update every 5 seconds (gateway polls every 2s)

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
    """Display startup banner with configuration"""
    try:
        import pymodbus
        pymodbus_version = f"{pymodbus.__version__} (API v{PYMODBUS_VERSION}.x)"
    except:
        pymodbus_version = f"Unknown (API v{PYMODBUS_VERSION}.x)"

    try:
        import serial
        pyserial_version = serial.VERSION
    except:
        pyserial_version = "Unknown"

    print("\n" + "="*70)
    print("  MODBUS RTU SLAVE SIMULATOR")
    print("  SRT-MGATE-1210 Testing - 5 Input Registers")
    print("="*70)
    print(f"  Serial Configuration:")
    print(f"  - Port:           {SERIAL_PORT}")
    print(f"  - Baud Rate:      {BAUD_RATE}")
    print(f"  - Data Bits:      {DATA_BITS}")
    print(f"  - Parity:         {PARITY} (None)")
    print(f"  - Stop Bits:      {STOP_BITS}")
    print(f"  - Frame Format:   RTU")
    print(f"  - Flow Control:   RTS Toggle")
    print(f"")
    print(f"  Modbus Configuration:")
    print(f"  - Slave ID:       {SLAVE_ID}")
    print(f"  - Registers:      {NUM_REGISTERS} Input Registers (Function Code 4)")
    print(f"  - Data Type:      INT16 (0-65535)")
    print(f"  - Address Range:  0-{NUM_REGISTERS-1}")
    print(f"")
    print(f"  Auto Update:")
    print(f"  - Enabled:        {'Yes' if AUTO_UPDATE else 'No'}")
    print(f"  - Interval:       {UPDATE_INTERVAL}s")
    print(f"")
    print(f"  Libraries:")
    print(f"  - PyModbus:       v{pymodbus_version}")
    print(f"  - PySerial:       v{pyserial_version}")
    print(f"  - Platform:       {platform.system()}")
    print("="*70)
    print("\n[INFO] Initializing Modbus RTU slave server...")

# =============================================================================
# Verify Serial Port Availability
# =============================================================================
def verify_serial_port():
    """Check if the specified serial port is available"""
    try:
        import serial.tools.list_ports
        ports = serial.tools.list_ports.comports()
        available_ports = [port.device for port in ports]

        print(f"\n[INFO] Available serial ports on this system:")
        if not available_ports:
            print("  → No serial ports detected!")
            return False

        for port in ports:
            print(f"  → {port.device}: {port.description}")

        if SERIAL_PORT not in available_ports and platform.system() == 'Windows':
            print(f"\n[WARNING] Configured port '{SERIAL_PORT}' not found in available ports!")
            print(f"[WARNING] Please check:")
            print(f"  1. USB-to-RS485 adapter is connected")
            print(f"  2. Driver is installed correctly")
            print(f"  3. Port number matches your configuration")
            print(f"  4. No other program is using {SERIAL_PORT}")
            return False

        return True

    except Exception as e:
        log.warning(f"Could not verify serial port: {e}")
        return True  # Continue anyway on Linux

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
                print(f"  Update #{self.update_count:04d} - Register Values (Slave ID: {SLAVE_ID}):")
                print(f"{'─'*70}")
                for addr in range(NUM_REGISTERS):
                    info = REGISTER_INFO[addr]
                    value = values[addr]
                    print(f"  [{addr}] {info['name']:12s}: {value:5d} {info['unit']:4s}")
                print(f"{'─'*70}")
                print(f"[INFO] Waiting for Modbus RTU requests on {SERIAL_PORT}...\n")

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
    """Setup and run the Modbus RTU server"""

    print_banner()

    # Verify serial port availability
    if not verify_serial_port():
        print("\n[ERROR] Serial port verification failed!")
        print("[INFO] You can still try to run the server, but connection may fail.")
        response = input("\nContinue anyway? (y/n): ")
        if response.lower() != 'y':
            print("[INFO] Exiting...")
            sys.exit(1)

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

    print(f"\n[INFO] Server listening on {SERIAL_PORT}")
    print(f"[INFO] Baud Rate: {BAUD_RATE}, Format: {DATA_BITS}{PARITY}{STOP_BITS}, Framer: RTU")
    print(f"[INFO] Slave ID: {SLAVE_ID}")
    print(f"[INFO] Function Code: 4 (Read Input Registers)")
    print(f"[INFO] Register Addresses: 0-{NUM_REGISTERS-1}")
    print(f"\n[INFO] Ready for connections from SRT-MGATE-1210 Gateway")
    print(f"[INFO] Gateway should be configured with:")
    print(f"       - Serial Port: COM8 or Serial Port 2 on ESP32")
    print(f"       - Baud: 9600, 8N1, RTU mode")
    print(f"       - Slave ID: 1")
    print(f"\n[INFO] Press Ctrl+C to stop server\n")

    # Start auto-update thread if enabled
    updater = None
    if AUTO_UPDATE:
        updater = RegisterUpdater(server_context, SLAVE_ID)
        updater.start()
        log.info("Auto-update enabled - register values will change every %.1fs", UPDATE_INTERVAL)

    try:
        # Start RTU Serial server (blocks until stopped)
        # Compatibility for pymodbus 2.x and 3.x
        log.info(f"Starting Modbus RTU server on {SERIAL_PORT}...")
        log.info(f"Using pymodbus {PYMODBUS_VERSION}.x API")

        if PYMODBUS_VERSION == 3:
            # pymodbus 3.x API
            StartSerialServer(
                context=server_context,
                framer=ModbusRtuFramer,
                port=SERIAL_PORT,
                baudrate=BAUD_RATE,
                bytesize=DATA_BITS,
                parity=PARITY,
                stopbits=STOP_BITS,
                timeout=1  # 1 second timeout for serial reads
            )
        else:
            # pymodbus 2.x API
            StartSerialServer(
                context=server_context,
                framer=ModbusRtuFramer,
                port=SERIAL_PORT,
                baudrate=BAUD_RATE,
                bytesize=DATA_BITS,
                parity=PARITY,
                stopbits=STOP_BITS,
                timeout=1  # 1 second timeout for serial reads
            )

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

        # Check for common errors
        if "could not open port" in str(e).lower():
            print("\n" + "="*70)
            print("  SERIAL PORT ERROR")
            print("="*70)
            print(f"\n  Could not open {SERIAL_PORT}")
            print(f"\n  Common causes:")
            print(f"  1. Port is already in use by another program")
            print(f"  2. USB-to-RS485 adapter not connected")
            print(f"  3. Incorrect COM port number")
            print(f"  4. Insufficient permissions (try running as Administrator)")
            print(f"  5. Driver not installed correctly")
            print("\n" + "="*70 + "\n")
        raise

# =============================================================================
# Main Entry Point
# =============================================================================
if __name__ == "__main__":
    print("\n" + "="*70)
    print("  MODBUS RTU SLAVE SIMULATOR")
    print("  SRT-MGATE-1210 Firmware Testing Tool")
    print("="*70)
    print(f"\n  Configuration:")
    print(f"  ├─ Port:         {SERIAL_PORT}")
    print(f"  ├─ Baud Rate:    {BAUD_RATE}")
    print(f"  ├─ Format:       {DATA_BITS}{PARITY}{STOP_BITS} (RTU)")
    print(f"  ├─ Slave ID:     {SLAVE_ID}")
    print(f"  ├─ Registers:    {NUM_REGISTERS} Input Registers (INT16)")
    print(f"  └─ Addresses:    0-{NUM_REGISTERS-1}")

    print(f"\n  Register Mapping:")
    for addr in range(NUM_REGISTERS):
        info = REGISTER_INFO[addr]
        print(f"  [{addr}] {info['name']:12s} - {info['unit']:4s} ({info['min']}-{info['max']})")

    print(f"\n  Gateway Configuration (use in Device_Testing/RTU):")
    print(f"  ├─ Serial Port:  COM8 (or Port 2 on ESP32)")
    print(f"  ├─ Baud Rate:    {BAUD_RATE}")
    print(f"  ├─ Data Bits:    {DATA_BITS}")
    print(f"  ├─ Parity:       None")
    print(f"  ├─ Stop Bits:    {STOP_BITS}")
    print(f"  ├─ Slave ID:     {SLAVE_ID}")
    print(f"  ├─ Protocol:     RTU")
    print(f"  └─ Function:     Read Input Registers (FC 4)")

    print("\n" + "="*70)
    print("\n  This simulator matches the configuration from:")
    print("  Device_Testing/RTU/create_device_5_registers.py")
    print("\n  IMPORTANT:")
    print("  - Make sure USB-to-RS485 adapter is connected to COM8")
    print("  - Configure Modbus Slave Simulator settings as shown in screenshot")
    print("  - RTS Toggle with 1ms disable delay")
    print("  - No other program should use COM8")
    print("\n" + "="*70 + "\n")

    input("Press Enter to start server...")

    run_server()
