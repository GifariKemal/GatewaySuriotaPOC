#!/usr/bin/env python3
"""
=============================================================================
Modbus RTU Slave Simulator Generator
=============================================================================
Automatically generates Modbus RTU slave simulators with different
register counts (5, 10, 15, 20, 25, 30, 35, 40, 45, 50 registers)

Author: Kemal - SURIOTA R&D Team
Date: 2025-11-17
Firmware Target: SRT-MGATE-1210 v2.2.0
=============================================================================
"""

import os
from pathlib import Path

# Expanded sensor types for larger register counts
SENSOR_TYPES = [
    {"name": "Temperature", "unit": "°C", "min": 20, "max": 35, "initial": 25, "variation": [-1, 0, 0, 1]},
    {"name": "Humidity", "unit": "%", "min": 40, "max": 80, "initial": 60, "variation": [-2, -1, 0, 1, 2]},
    {"name": "Pressure", "unit": "Pa", "min": 900, "max": 1100, "initial": 1000, "variation_range": (-5, 5)},
    {"name": "Voltage", "unit": "V", "min": 220, "max": 240, "initial": 230, "variation": [-1, 0, 0, 0, 1]},
    {"name": "Current", "unit": "A", "min": 1, "max": 10, "initial": 5, "variation": [-1, 0, 1]},
    {"name": "Power", "unit": "W", "min": 0, "max": 5000, "initial": 1000, "variation_range": (-50, 50)},
    {"name": "Frequency", "unit": "Hz", "min": 48, "max": 52, "initial": 50, "variation": [-1, 0, 0, 0, 1]},
    {"name": "PowerFactor", "unit": "", "min": 0, "max": 100, "initial": 95, "variation": [-2, -1, 0, 1, 2]},
    {"name": "Energy", "unit": "kWh", "min": 0, "max": 9999, "initial": 100, "variation_range": (0, 5)},
    {"name": "Flow", "unit": "L/min", "min": 0, "max": 100, "initial": 50, "variation_range": (-3, 3)},
    {"name": "Level", "unit": "cm", "min": 0, "max": 200, "initial": 100, "variation_range": (-2, 2)},
    {"name": "pH", "unit": "", "min": 0, "max": 14, "initial": 7, "variation": [-1, 0, 0, 1]},
    {"name": "Conductivity", "unit": "mS/cm", "min": 0, "max": 200, "initial": 100, "variation_range": (-5, 5)},
    {"name": "TDS", "unit": "ppm", "min": 0, "max": 1000, "initial": 500, "variation_range": (-10, 10)},
    {"name": "Turbidity", "unit": "NTU", "min": 0, "max": 100, "initial": 10, "variation_range": (-2, 2)},
    {"name": "Speed", "unit": "RPM", "min": 0, "max": 3000, "initial": 1500, "variation_range": (-50, 50)},
    {"name": "Torque", "unit": "Nm", "min": 0, "max": 500, "initial": 100, "variation_range": (-10, 10)},
    {"name": "Vibration", "unit": "mm/s", "min": 0, "max": 20, "initial": 5, "variation": [-1, 0, 0, 1]},
    {"name": "CO2", "unit": "ppm", "min": 300, "max": 2000, "initial": 400, "variation_range": (-20, 20)},
    {"name": "VOC", "unit": "ppb", "min": 0, "max": 1000, "initial": 100, "variation_range": (-10, 10)},
    {"name": "Light", "unit": "lux", "min": 0, "max": 10000, "initial": 500, "variation_range": (-50, 50)},
    {"name": "Noise", "unit": "dB", "min": 30, "max": 120, "initial": 60, "variation_range": (-5, 5)},
    {"name": "Distance", "unit": "cm", "min": 0, "max": 400, "initial": 100, "variation_range": (-5, 5)},
    {"name": "Angle", "unit": "deg", "min": 0, "max": 360, "initial": 180, "variation_range": (-10, 10)},
    {"name": "Weight", "unit": "kg", "min": 0, "max": 1000, "initial": 100, "variation_range": (-5, 5)},
    {"name": "Force", "unit": "N", "min": 0, "max": 1000, "initial": 100, "variation_range": (-10, 10)},
    {"name": "Position", "unit": "mm", "min": 0, "max": 500, "initial": 250, "variation_range": (-5, 5)},
    {"name": "Velocity", "unit": "m/s", "min": 0, "max": 50, "initial": 10, "variation_range": (-2, 2)},
    {"name": "Acceleration", "unit": "m/s²", "min": 0, "max": 20, "initial": 10, "variation": [-1, 0, 1]},
    {"name": "Tank1Level", "unit": "%", "min": 0, "max": 100, "initial": 50, "variation_range": (-2, 2)},
    {"name": "Tank2Level", "unit": "%", "min": 0, "max": 100, "initial": 60, "variation_range": (-2, 2)},
    {"name": "Pump1Speed", "unit": "RPM", "min": 0, "max": 1500, "initial": 750, "variation_range": (-20, 20)},
    {"name": "Pump2Speed", "unit": "RPM", "min": 0, "max": 1500, "initial": 800, "variation_range": (-20, 20)},
    {"name": "Valve1Position", "unit": "%", "min": 0, "max": 100, "initial": 50, "variation_range": (-5, 5)},
    {"name": "Valve2Position", "unit": "%", "min": 0, "max": 100, "initial": 60, "variation_range": (-5, 5)},
    {"name": "MotorCurrent", "unit": "A", "min": 0, "max": 50, "initial": 10, "variation_range": (-2, 2)},
    {"name": "MotorVoltage", "unit": "V", "min": 0, "max": 480, "initial": 380, "variation": [-2, -1, 0, 1, 2]},
    {"name": "MotorTemp", "unit": "°C", "min": 20, "max": 80, "initial": 40, "variation": [-1, 0, 0, 1]},
    {"name": "AmbientTemp", "unit": "°C", "min": 15, "max": 40, "initial": 25, "variation": [-1, 0, 0, 1]},
    {"name": "AmbientHumidity", "unit": "%", "min": 20, "max": 90, "initial": 60, "variation_range": (-3, 3)},
    {"name": "Dewpoint", "unit": "°C", "min": -10, "max": 30, "initial": 15, "variation": [-1, 0, 1]},
    {"name": "WindSpeed", "unit": "m/s", "min": 0, "max": 30, "initial": 5, "variation_range": (-2, 2)},
    {"name": "WindDirection", "unit": "deg", "min": 0, "max": 360, "initial": 180, "variation_range": (-15, 15)},
    {"name": "Rain", "unit": "mm", "min": 0, "max": 100, "initial": 0, "variation": [0, 0, 0, 1, 2]},
    {"name": "SolarRadiation", "unit": "W/m²", "min": 0, "max": 1200, "initial": 500, "variation_range": (-50, 50)},
    {"name": "BarometricPressure", "unit": "hPa", "min": 950, "max": 1050, "initial": 1013, "variation": [-2, -1, 0, 1, 2]},
    {"name": "UVIndex", "unit": "", "min": 0, "max": 15, "initial": 5, "variation": [0, 0, 1]},
    {"name": "AirQualityIndex", "unit": "", "min": 0, "max": 500, "initial": 50, "variation_range": (-5, 5)},
    {"name": "PM25", "unit": "µg/m³", "min": 0, "max": 500, "initial": 35, "variation_range": (-5, 5)},
    {"name": "PM10", "unit": "µg/m³", "min": 0, "max": 600, "initial": 50, "variation_range": (-5, 5)},
]


def generate_register_info(num_registers):
    """Generate register information for specified number of registers"""
    register_info = {}

    for i in range(num_registers):
        # Cycle through sensor types, reusing if needed
        sensor = SENSOR_TYPES[i % len(SENSOR_TYPES)]

        # If we're reusing sensors, add a suffix
        if i >= len(SENSOR_TYPES):
            suffix = (i // len(SENSOR_TYPES)) + 1
            name = f"{sensor['name']}{suffix}"
        else:
            name = sensor['name']

        register_info[i] = {
            "name": name,
            "unit": sensor["unit"],
            "min": sensor["min"],
            "max": sensor["max"],
            "initial": sensor["initial"]
        }

        # Add variation info for simulation
        if "variation" in sensor:
            register_info[i]["variation"] = sensor["variation"]
        elif "variation_range" in sensor:
            register_info[i]["variation_range"] = sensor["variation_range"]
        else:
            register_info[i]["variation"] = [-1, 0, 1]

    return register_info


def generate_update_logic(num_registers):
    """Generate update logic code for register simulation"""
    lines = []

    for addr in range(num_registers):
        sensor = SENSOR_TYPES[addr % len(SENSOR_TYPES)]

        lines.append(f"                    {'if' if addr == 0 else 'elif'} address == {addr}:  # {sensor['name']}")

        if "variation" in sensor:
            variation_str = str(sensor['variation'])
            lines.append(f"                        new_value = current_value + random.choice({variation_str})")
        elif "variation_range" in sensor:
            vmin, vmax = sensor['variation_range']
            lines.append(f"                        new_value = current_value + random.randint({vmin}, {vmax})")
        else:
            lines.append(f"                        new_value = current_value + random.choice([-1, 0, 1])")

        lines.append(f"                        new_value = max(info[\"min\"], min(info[\"max\"], new_value))")
        lines.append("")

    lines.append("                    else:")
    lines.append("                        new_value = current_value")

    return "\n".join(lines)


def generate_simulator_file(num_registers, output_dir):
    """Generate a complete Modbus RTU slave simulator file"""

    register_info = generate_register_info(num_registers)
    update_logic = generate_update_logic(num_registers)

    # Build register info dict as string
    register_dict_lines = []
    for addr, info in register_info.items():
        register_dict_lines.append(
            f'    {addr}: {{"name": "{info["name"]}", "unit": "{info["unit"]}", '
            f'"min": {info["min"]}, "max": {info["max"]}, "initial": {info["initial"]}}}'
        )
    register_dict_str = ",\n".join(register_dict_lines)

    # Generate filename
    filename = f"modbus_slave_{num_registers}_registers.py"
    filepath = os.path.join(output_dir, filename)

    # Generate file content
    content = f'''#!/usr/bin/env python3
"""
=============================================================================
Modbus RTU Slave Simulator - {num_registers} Input Registers
=============================================================================
Description:
    Simulates a Modbus RTU slave with {num_registers} Input Registers
    matching the Device_Testing configuration

    Device Configuration:
    - Serial Port: COM8 (Windows) or /dev/ttyUSB0 (Linux)
    - Baud Rate: 9600
    - Data Bits: 8
    - Parity: None
    - Stop Bits: 1
    - Slave ID: 1
    - Protocol: Modbus RTU
    - Flow Control: RTS Toggle (RTS disable delay: 1ms)

    Registers ({num_registers} Input Registers - Function Code 4):
    - Addresses: 0 to {num_registers-1}
    - Various sensor types for comprehensive testing

Usage:
    1. Install: pip install pymodbus pyserial
    2. Configure COM port in script (default: COM8)
    3. Run: python {filename}
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
    print("\\n" + "="*70)
    print("  ERROR: Required libraries not installed!")
    print("="*70)
    print(f"\\n  {{e}}\\n")
    print("  Please install required packages:")
    print("  → pip install pymodbus pyserial")
    print("\\n  Or:")
    print("  → pip3 install pymodbus pyserial")
    print("\\n" + "="*70 + "\\n")
    sys.exit(1)

try:
    import serial
    PYSERIAL_IMPORTED = True
except ImportError as e:
    print("\\n" + "="*70)
    print("  ERROR: pyserial not installed!")
    print("="*70)
    print(f"\\n  {{e}}\\n")
    print("  Please install pyserial:")
    print("  → pip install pyserial")
    print("\\n" + "="*70 + "\\n")
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
NUM_REGISTERS = {num_registers}      # {num_registers} Input Registers

# Register definitions
REGISTER_INFO = {{
{register_dict_str}
}}

# Auto-update configuration
AUTO_UPDATE = True
UPDATE_INTERVAL = 5.0  # Update every 5 seconds (gateway polls every 2s)

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
        pymodbus_version = f"{{pymodbus.__version__}} (API v{{PYMODBUS_VERSION}}.x)"
    except:
        pymodbus_version = f"Unknown (API v{{PYMODBUS_VERSION}}.x)"

    try:
        import serial
        pyserial_version = serial.VERSION
    except:
        pyserial_version = "Unknown"

    print("\\n" + "="*70)
    print("  MODBUS RTU SLAVE SIMULATOR")
    print(f"  SRT-MGATE-1210 Testing - {num_registers} Input Registers")
    print("="*70)
    print(f"  Serial Configuration:")
    print(f"  - Port:           {{SERIAL_PORT}}")
    print(f"  - Baud Rate:      {{BAUD_RATE}}")
    print(f"  - Data Bits:      {{DATA_BITS}}")
    print(f"  - Parity:         {{PARITY}} (None)")
    print(f"  - Stop Bits:      {{STOP_BITS}}")
    print(f"  - Frame Format:   RTU")
    print(f"  - Flow Control:   RTS Toggle")
    print(f"")
    print(f"  Modbus Configuration:")
    print(f"  - Slave ID:       {{SLAVE_ID}}")
    print(f"  - Registers:      {{NUM_REGISTERS}} Input Registers (Function Code 4)")
    print(f"  - Data Type:      INT16 (0-65535)")
    print(f"  - Address Range:  0-{{NUM_REGISTERS-1}}")
    print(f"")
    print(f"  Auto Update:")
    print(f"  - Enabled:        {{'Yes' if AUTO_UPDATE else 'No'}}")
    print(f"  - Interval:       {{UPDATE_INTERVAL}}s")
    print(f"")
    print(f"  Libraries:")
    print(f"  - PyModbus:       v{{pymodbus_version}}")
    print(f"  - PySerial:       v{{pyserial_version}}")
    print(f"  - Platform:       {{platform.system()}}")
    print("="*70)
    print("\\n[INFO] Initializing Modbus RTU slave server...")

# =============================================================================
# Verify Serial Port Availability
# =============================================================================
def verify_serial_port():
    """Check if the specified serial port is available"""
    try:
        import serial.tools.list_ports
        ports = serial.tools.list_ports.comports()
        available_ports = [port.device for port in ports]

        print(f"\\n[INFO] Available serial ports on this system:")
        if not available_ports:
            print("  → No serial ports detected!")
            return False

        for port in ports:
            print(f"  → {{port.device}}: {{port.description}}")

        if SERIAL_PORT not in available_ports and platform.system() == 'Windows':
            print(f"\\n[WARNING] Configured port '{{SERIAL_PORT}}' not found in available ports!")
            print(f"[WARNING] Please check:")
            print(f"  1. USB-to-RS485 adapter is connected")
            print(f"  2. Driver is installed correctly")
            print(f"  3. Port number matches your configuration")
            print(f"  4. No other program is using {{SERIAL_PORT}}")
            return False

        return True

    except Exception as e:
        log.warning(f"Could not verify serial port: {{e}}")
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
{update_logic}

                    # Set new value
                    slave_context.setValues(4, address, [new_value])

                self.update_count += 1

                # Log updated values (first 10 and last register for brevity)
                values = slave_context.getValues(4, 0, count=NUM_REGISTERS)
                if NUM_REGISTERS <= 10:
                    log.info(f"Update #{{self.update_count:04d}} - Values: {{values}}")
                else:
                    log.info(f"Update #{{self.update_count:04d}} - First 5: {{values[:5]}} ... Last: {{values[-1]}}")

                # Print detailed values every update (limited to first 10 for readability)
                print(f"\\n{{'─'*70}}")
                print(f"  Update #{{self.update_count:04d}} - Register Values (Slave ID: {{SLAVE_ID}}):")
                print(f"{{'─'*70}}")

                # Show first 10 registers
                display_count = min(10, NUM_REGISTERS)
                for addr in range(display_count):
                    info = REGISTER_INFO[addr]
                    value = values[addr]
                    print(f"  [{{addr}}] {{info['name']:15s}}: {{value:5d}} {{info['unit']:6s}}")

                if NUM_REGISTERS > 10:
                    print(f"  ... ({{NUM_REGISTERS - display_count}} more registers)")

                print(f"{{'─'*70}}")
                print(f"[INFO] Waiting for Modbus RTU requests on {{SERIAL_PORT}}...\\n")

            except Exception as e:
                log.error(f"Error in auto-update thread: {{e}}")
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
        print("\\n[ERROR] Serial port verification failed!")
        print("[INFO] You can still try to run the server, but connection may fail.")
        response = input("\\nContinue anyway? (y/n): ")
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
        slaves={{SLAVE_ID: slave_context}},
        single=False
    )

    # Display initial register values (first 10 for brevity)
    print("\\n" + "="*70)
    print(f"  INITIAL REGISTER VALUES ({{NUM_REGISTERS}} Input Registers)")
    print("="*70)
    print(f"  {{'Addr':<6}} {{'Name':<18}} {{'Value':<10}} {{'Unit':<10}} {{'Range':<20}}")
    print("─"*70)

    display_count = min(10, NUM_REGISTERS)
    for addr in range(display_count):
        info = REGISTER_INFO[addr]
        value = info["initial"]
        range_str = f"{{info['min']}}-{{info['max']}}"
        print(f"  {{addr:<6}} {{info['name']:<18}} {{value:<10}} {{info['unit']:<10}} {{range_str:<20}}")

    if NUM_REGISTERS > 10:
        print(f"  ... ({{NUM_REGISTERS - display_count}} more registers)")

    print("="*70)

    print(f"\\n[INFO] Server listening on {{SERIAL_PORT}}")
    print(f"[INFO] Baud Rate: {{BAUD_RATE}}, Format: {{DATA_BITS}}{{PARITY}}{{STOP_BITS}}, Framer: RTU")
    print(f"[INFO] Slave ID: {{SLAVE_ID}}")
    print(f"[INFO] Function Code: 4 (Read Input Registers)")
    print(f"[INFO] Register Addresses: 0-{{NUM_REGISTERS-1}}")
    print(f"\\n[INFO] Ready for connections from SRT-MGATE-1210 Gateway")
    print(f"[INFO] Press Ctrl+C to stop server\\n")

    # Start auto-update thread if enabled
    updater = None
    if AUTO_UPDATE:
        updater = RegisterUpdater(server_context, SLAVE_ID)
        updater.start()
        log.info("Auto-update enabled - register values will change every %.1fs", UPDATE_INTERVAL)

    try:
        # Start RTU Serial server (blocks until stopped)
        # Compatibility for pymodbus 2.x and 3.x
        log.info(f"Starting Modbus RTU server on {{SERIAL_PORT}}...")
        log.info(f"Using pymodbus {{PYMODBUS_VERSION}}.x API")

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
        print("\\n\\n[INFO] Shutting down server...")
        if updater:
            updater.stop()
            updater.join(timeout=2)
        print("[INFO] Server stopped gracefully")

    except Exception as e:
        log.error(f"Server error: {{e}}")
        import traceback
        traceback.print_exc()
        if updater:
            updater.stop()

        # Check for common errors
        if "could not open port" in str(e).lower():
            print("\\n" + "="*70)
            print("  SERIAL PORT ERROR")
            print("="*70)
            print(f"\\n  Could not open {{SERIAL_PORT}}")
            print(f"\\n  Common causes:")
            print(f"  1. Port is already in use by another program")
            print(f"  2. USB-to-RS485 adapter not connected")
            print(f"  3. Incorrect COM port number")
            print(f"  4. Insufficient permissions (try running as Administrator)")
            print(f"  5. Driver not installed correctly")
            print("\\n" + "="*70 + "\\n")
        raise

# =============================================================================
# Main Entry Point
# =============================================================================
if __name__ == "__main__":
    print("\\n" + "="*70)
    print("  MODBUS RTU SLAVE SIMULATOR")
    print("  SRT-MGATE-1210 Firmware Testing Tool")
    print("="*70)
    print(f"\\n  Configuration:")
    print(f"  ├─ Port:         {{SERIAL_PORT}}")
    print(f"  ├─ Baud Rate:    {{BAUD_RATE}}")
    print(f"  ├─ Format:       {{DATA_BITS}}{{PARITY}}{{STOP_BITS}} (RTU)")
    print(f"  ├─ Slave ID:     {{SLAVE_ID}}")
    print(f"  ├─ Registers:    {{NUM_REGISTERS}} Input Registers (INT16)")
    print(f"  └─ Addresses:    0-{{NUM_REGISTERS-1}}")

    print(f"\\n  First {{min(10, NUM_REGISTERS)}} Register Mapping:")
    for addr in range(min(10, NUM_REGISTERS)):
        info = REGISTER_INFO[addr]
        print(f"  [{{addr}}] {{info['name']:15s}} - {{info['unit']:6s}} ({{info['min']}}-{{info['max']}})")

    if NUM_REGISTERS > 10:
        print(f"  ... ({{NUM_REGISTERS - 10}} more registers)")

    print("\\n" + "="*70)
    print(f"\\n  This simulator provides {{NUM_REGISTERS}} Input Registers for comprehensive testing")
    print(f"  Compatible with Device_Testing/RTU/create_device_{{NUM_REGISTERS}}_registers.py")
    print("\\n  IMPORTANT:")
    print("  - Make sure USB-to-RS485 adapter is connected to COM8")
    print(f"  - Configure gateway to poll {{NUM_REGISTERS}} registers (addresses 0-{{NUM_REGISTERS-1}})")
    print("  - RTS Toggle with 1ms disable delay")
    print("  - No other program should use COM8")
    print("\\n" + "="*70 + "\\n")

    input("Press Enter to start server...")

    run_server()
'''

    # Write file
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(content)

    print(f"[OK] Generated: {filename}")
    return filepath


def main():
    """Main generator function"""
    print("\n" + "="*70)
    print("  MODBUS RTU SLAVE SIMULATOR GENERATOR")
    print("  SRT-MGATE-1210 Testing Tool")
    print("="*70)

    # Get output directory
    output_dir = Path(__file__).parent
    print(f"\nOutput directory: {output_dir}")

    # Register counts to generate
    register_counts = [10, 15, 20, 25, 30, 35, 40, 45, 50]

    print(f"\nGenerating simulators for: {register_counts}")
    print("-"*70)

    generated_files = []

    for count in register_counts:
        print(f"\n[{count} registers]")
        try:
            filepath = generate_simulator_file(count, output_dir)
            generated_files.append(filepath)
        except Exception as e:
            print(f"[ERROR] Error generating {count} registers: {e}")
            import traceback
            traceback.print_exc()

    print("\n" + "="*70)
    print("  GENERATION COMPLETE")
    print("="*70)
    print(f"\n[OK] Generated {len(generated_files)} simulator files:")
    for filepath in generated_files:
        print(f"  - {Path(filepath).name}")

    print("\n" + "="*70)
    print("\nAll simulators are ready to use!")
    print("Run any simulator with: python modbus_slave_XX_registers.py")
    print("\n" + "="*70 + "\n")


if __name__ == "__main__":
    main()
