#!/usr/bin/env python3
"""
Quick fix script to update serial_port from 1 to 2 in all create_device files
"""
import os
import re

def fix_serial_port(filepath):
    """Fix serial_port from 1 to 2 in a file"""
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    # Replace "serial_port": 1 with "serial_port": 2
    original = content
    content = content.replace('"serial_port": 1,', '"serial_port": 2,')

    # Also fix the summary section
    content = content.replace('Serial Port:      COM8 (Port 1)', 'Serial Port:      COM8 (Port 2)')

    if content != original:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        return True
    return False

# Get all create_device files
path = '.'
files = [f for f in os.listdir(path) if f.startswith('create_device_') and f.endswith('.py')]

print("="*60)
print("  FIXING SERIAL_PORT IN ALL CREATE_DEVICE FILES")
print("="*60)

fixed_count = 0
for filename in sorted(files):
    filepath = os.path.join(path, filename)
    if fix_serial_port(filepath):
        print(f"[FIXED] {filename}")
        fixed_count += 1
    else:
        print(f"[OK]    {filename} (already correct)")

print("="*60)
print(f"Total fixed: {fixed_count} files")
print("="*60)
