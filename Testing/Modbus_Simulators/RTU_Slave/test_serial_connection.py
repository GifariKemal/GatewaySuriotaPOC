#!/usr/bin/env python3
"""
=============================================================================
Modbus RTU Serial Connection Diagnostic Tool
=============================================================================
Tests serial port availability and basic communication

Author: Kemal - SURIOTA R&D Team
Date: 2025-11-17
=============================================================================
"""

import sys
import time

print("\n" + "=" * 70)
print("  MODBUS RTU SERIAL CONNECTION DIAGNOSTIC")
print("=" * 70)

# Step 1: Check pyserial
print("\n[STEP 1] Checking pyserial installation...")
try:
    import serial
    import serial.tools.list_ports

    print(f"✓ pyserial {serial.VERSION} installed")
except ImportError as e:
    print(f"✗ pyserial not installed: {e}")
    print("  Run: pip install pyserial")
    sys.exit(1)

# Step 2: List all available ports
print("\n[STEP 2] Scanning available serial ports...")
ports = serial.tools.list_ports.comports()
if not ports:
    print("✗ No serial ports detected!")
    print("  - Check USB-to-RS485 adapter is connected")
    print("  - Check driver is installed")
    sys.exit(1)

print(f"✓ Found {len(ports)} serial port(s):")
for i, port in enumerate(ports, 1):
    print(f"  {i}. {port.device}")
    print(f"     Description: {port.description}")
    print(f"     Hardware ID: {port.hwid}")
    print()

# Step 3: Ask user which port to test
print("[STEP 3] Which port is connected to ESP32 Gateway?")
print("  (Usually COM8 or the USB-to-RS485 adapter)")
port_list = [p.device for p in ports]

default_port = "COM8" if "COM8" in port_list else port_list[0]
selected_port = input(f"\nEnter port name (default: {default_port}): ").strip()
if not selected_port:
    selected_port = default_port

if selected_port not in port_list:
    print(f"✗ Port {selected_port} not in available ports!")
    sys.exit(1)

# Step 4: Test opening the port
print(f"\n[STEP 4] Testing port {selected_port}...")
try:
    ser = serial.Serial(
        port=selected_port, baudrate=9600, bytesize=8, parity="N", stopbits=1, timeout=1
    )
    print(f"✓ Port {selected_port} opened successfully")
    print(f"  Baudrate: {ser.baudrate}")
    print(f"  Format: {ser.bytesize}{ser.parity}{ser.stopbits}")

    # Step 5: Check if port is readable
    print("\n[STEP 5] Testing port I/O...")
    print("  Listening for 3 seconds...")
    print("  (If gateway is polling, you should see data)")

    start_time = time.time()
    data_received = False

    while time.time() - start_time < 3:
        if ser.in_waiting > 0:
            data = ser.read(ser.in_waiting)
            print(f"  ✓ Received {len(data)} bytes: {data.hex()}")
            data_received = True
        time.sleep(0.1)

    if not data_received:
        print("  ✗ No data received in 3 seconds")
        print("  Possible causes:")
        print("    1. Gateway not connected to this port")
        print("    2. Wiring issue (A+/B- not connected)")
        print("    3. Gateway not polling (device not created)")
        print("    4. Wrong port selected")

    ser.close()
    print(f"✓ Port {selected_port} closed")

except serial.SerialException as e:
    print(f"✗ Failed to open port {selected_port}: {e}")
    print("  Possible causes:")
    print("    1. Port already in use by another program")
    print("    2. Insufficient permissions (try as Administrator)")
    print("    3. Driver issue")
    sys.exit(1)

# Step 6: Check pymodbus
print("\n[STEP 6] Checking pymodbus installation...")
try:
    import pymodbus

    print(f"✓ pymodbus {pymodbus.__version__} installed")
except ImportError:
    print("✗ pymodbus not installed")
    print("  Run: pip install pymodbus")

# Summary
print("\n" + "=" * 70)
print("  DIAGNOSTIC SUMMARY")
print("=" * 70)
print(f"✓ Python serial libraries: OK")
print(f"✓ Available ports: {len(ports)}")
print(f"✓ Selected port: {selected_port}")
print(
    f"{'✓' if data_received else '✗'} Data received: {'YES' if data_received else 'NO'}"
)
print("=" * 70)

print("\n[NEXT STEPS]")
if data_received:
    print("✓ Port is receiving data - connection looks good!")
    print("  1. Run modbus_slave_5_registers.py on this port")
    print("  2. Gateway should be able to communicate")
else:
    print("✗ No data detected - troubleshoot hardware:")
    print("  1. Check RS485 wiring:")
    print("     - USB-to-RS485 A+ → Gateway Serial2 A+")
    print("     - USB-to-RS485 B- → Gateway Serial2 B-")
    print("     - GND → GND")
    print("  2. Verify gateway is polling:")
    print("     - Check gateway serial monitor logs")
    print("     - Should see '[RTU] Polling device...'")
    print("  3. Try running simulator anyway:")
    print("     python modbus_slave_5_registers.py")
    print("  4. Check with multimeter/oscilloscope on A+/B- pins")

print("\n" + "=" * 70 + "\n")
