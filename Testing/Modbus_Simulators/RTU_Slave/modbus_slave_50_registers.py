#!/usr/bin/env python3
"""
=============================================================================
Modbus RTU Slave Simulator - 50 Input Registers
=============================================================================
Description:
    Simulates a Modbus RTU slave with 50 Input Registers
    ALIGNED with create_device_50_registers.py for perfect register matching

    Device Configuration:
    - Serial Port: COM8 (Windows) or /dev/ttyUSB0 (Linux)
    - Baud Rate: 9600
    - Data Bits: 8
    - Parity: None
    - Stop Bits: 1
    - Slave ID: 1
    - Protocol: Modbus RTU
    - Flow Control: RTS Toggle (RTS disable delay: 1ms)

    Registers (50 Input Registers - Function Code 4):
    - Addresses: 0 to 49
    - Various sensor types for comprehensive testing

Usage:
    1. Install: pip install pymodbus pyserial
    2. Configure COM port in script (default: COM8)
    3. Run: python modbus_slave_50_registers.py
    4. Gateway will poll via RTU at configured refresh rate

Requirements:
    pip install pymodbus pyserial

Compatible with: pymodbus 2.x and 3.x

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
# Configuration
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
NUM_REGISTERS = 50      # 50 Input Registers

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
        "max": 5000,
        "initial": 1000
    }

# Energy Counters (40-44) - kWh
for i in range(5):
    REGISTER_INFO[40 + i] = {
        "name": f"Energy_{i+1}",
        "desc": f"Energy Counter {i+1}",
        "unit": "kWh",
        "min": 0,
        "max": 9999,
        "initial": 100
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
UPDATE_INTERVAL = 1.0  # Update every 1 second (gateway polls every 2s)

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
    print(f"  SRT-MGATE-1210 Testing - 50 Input Registers")
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

                    # Simulate realistic sensor variations based on register type
                    if 0 <= address <= 9:  # Temperature Zones (0-9)
                        new_value = current_value + random.choice([-1, 0, 0, 1])
                        new_value = max(info["min"], min(info["max"], new_value))

                    elif 10 <= address <= 19:  # Humidity Zones (10-19)
                        new_value = current_value + random.choice([-2, -1, 0, 1, 2])
                        new_value = max(info["min"], min(info["max"], new_value))

                    elif 20 <= address <= 24:  # Pressure Sensors (20-24)
                        new_value = current_value + random.randint(-5, 5)
                        new_value = max(info["min"], min(info["max"], new_value))

                    elif 25 <= address <= 29:  # Voltage Lines (25-29)
                        new_value = current_value + random.choice([-1, 0, 0, 0, 1])
                        new_value = max(info["min"], min(info["max"], new_value))

                    elif 30 <= address <= 34:  # Current Lines (30-34)
                        new_value = current_value + random.choice([-1, 0, 1])
                        new_value = max(info["min"], min(info["max"], new_value))

                    elif 35 <= address <= 39:  # Power Meters (35-39)
                        new_value = current_value + random.randint(-50, 50)
                        new_value = max(info["min"], min(info["max"], new_value))

                    elif 40 <= address <= 44:  # Energy Counters (40-44)
                        new_value = current_value + random.randint(0, 5)  # Energy only increases
                        new_value = max(info["min"], min(info["max"], new_value))

                    elif 45 <= address <= 49:  # Flow Meters (45-49)
                        new_value = current_value + random.randint(-3, 3)
                        new_value = max(info["min"], min(info["max"], new_value))

                    else:
                        new_value = current_value

                    # Set new value
                    slave_context.setValues(4, address, [new_value])

                self.update_count += 1

                # Log updated values (first 10 and last register for brevity)
                values = slave_context.getValues(4, 0, count=NUM_REGISTERS)
                if NUM_REGISTERS <= 10:
                    log.info(f"Update #{self.update_count:04d} - Values: {values}")
                else:
                    log.info(f"Update #{self.update_count:04d} - First 5: {values[:5]} ... Last: {values[-1]}")

                # Print detailed values every update (limited to first 10 for readability)
                print(f"\n{'─'*70}")
                print(f"  Update #{self.update_count:04d} - Register Values (Slave ID: {SLAVE_ID}):")
                print(f"{'─'*70}")

                # Show first 10 registers
                display_count = min(10, NUM_REGISTERS)
                for addr in range(display_count):
                    info = REGISTER_INFO[addr]
                    value = values[addr]
                    print(f"  [{addr}] {info['name']:15s}: {value:5d} {info['unit']:6s}")

                if NUM_REGISTERS > 10:
                    print(f"  ... ({NUM_REGISTERS - display_count} more registers)")

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
        hr=ModbusSequentialDataBlock(0, [0]*200),  # Holding Registers (not used)
        ir=input_registers,                         # Input Registers (USED)
        zero_mode=True  # Use 0-based addressing
    )

    # Create server context with single slave
    server_context = ModbusServerContext(
        slaves={SLAVE_ID: slave_context},
        single=False
    )

    # Display initial register values (organized by category)
    print("\n" + "="*70)
    print(f"  INITIAL REGISTER VALUES ({NUM_REGISTERS} Input Registers)")
    print("="*70)
    print(f"  {'Addr':<6} {'Name':<20} {'Value':<8} {'Unit':<8} {'Range':<15}")
    print("─"*70)

    # Show sample from each category
    categories = [
        (0, 2, 10, "Temperature Zones"),
        (10, 12, 10, "Humidity Zones"),
        (20, 22, 5, "Pressure Sensors"),
        (25, 27, 5, "Voltage Lines"),
        (30, 32, 5, "Current Lines"),
        (35, 37, 5, "Power Meters"),
        (40, 42, 5, "Energy Counters"),
        (45, 47, 5, "Flow Meters")
    ]

    for start, end, total, category in categories:
        print(f"  {category}:")
        for addr in range(start, end):
            info = REGISTER_INFO[addr]
            value = info["initial"]
            range_str = f"{info['min']}-{info['max']}"
            print(f"    {addr:<4} {info['name']:<20} {value:<8} {info['unit']:<8} {range_str:<15}")
        remaining = total - (end - start)
        if remaining > 0:
            print(f"    ... ({remaining} more in this category)")
        print()

    print("="*70)

    print(f"\n[INFO] Server listening on {SERIAL_PORT}")
    print(f"[INFO] Baud Rate: {BAUD_RATE}, Format: {DATA_BITS}{PARITY}{STOP_BITS}, Framer: RTU")
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

    print(f"\n  Register Mapping (by category):")
    print(f"  ├─ Temperature Zones (0-9):   {REGISTER_INFO[0]['name']} ... {REGISTER_INFO[9]['name']}")
    print(f"  ├─ Humidity Zones (10-19):    {REGISTER_INFO[10]['name']} ... {REGISTER_INFO[19]['name']}")
    print(f"  ├─ Pressure Sensors (20-24):  {REGISTER_INFO[20]['name']} ... {REGISTER_INFO[24]['name']}")
    print(f"  ├─ Voltage Lines (25-29):     {REGISTER_INFO[25]['name']} ... {REGISTER_INFO[29]['name']}")
    print(f"  ├─ Current Lines (30-34):     {REGISTER_INFO[30]['name']} ... {REGISTER_INFO[34]['name']}")
    print(f"  ├─ Power Meters (35-39):      {REGISTER_INFO[35]['name']} ... {REGISTER_INFO[39]['name']}")
    print(f"  ├─ Energy Counters (40-44):   {REGISTER_INFO[40]['name']} ... {REGISTER_INFO[44]['name']}")
    print(f"  └─ Flow Meters (45-49):       {REGISTER_INFO[45]['name']} ... {REGISTER_INFO[49]['name']}")

    print("\n" + "="*70)
    print(f"\n  This simulator provides {NUM_REGISTERS} Input Registers for comprehensive testing")
    print(f"  Compatible with Device_Testing/RTU/create_device_{NUM_REGISTERS}_registers.py")
    print("\n  IMPORTANT:")
    print("  - Make sure USB-to-RS485 adapter is connected to COM8")
    print(f"  - Configure gateway to poll {NUM_REGISTERS} registers (addresses 0-{NUM_REGISTERS-1})")
    print("  - RTS Toggle with 1ms disable delay")
    print("  - No other program should use COM8")
    print("\n" + "="*70 + "\n")

    input("Press Enter to start server...")

    run_server()
