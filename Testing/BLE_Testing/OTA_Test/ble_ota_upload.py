#!/usr/bin/env python3
"""
BLE OTA Upload Tool for SRT-MGATE-1210
======================================

Upload firmware binary via Bluetooth Low Energy.

Requirements:
    pip install bleak tqdm

Usage:
    python ble_ota_upload.py                    # Interactive mode
    python ble_ota_upload.py -f firmware.bin    # Direct upload
    python ble_ota_upload.py --scan             # Scan for devices

Author: Suriota R&D Team
Version: 1.0.0
Date: 2025-11-29
"""

import asyncio
import argparse
import hashlib
import struct
import sys
import os
from pathlib import Path

try:
    from bleak import BleakClient, BleakScanner
    from bleak.exc import BleakError
except ImportError:
    print("Error: 'bleak' library not installed.")
    print("Install with: pip install bleak")
    sys.exit(1)

try:
    from tqdm import tqdm
except ImportError:
    print("Warning: 'tqdm' not installed. Progress bar disabled.")
    print("Install with: pip install tqdm")
    tqdm = None

# ============================================
# BLE OTA UUIDs (must match OTAConfig.h)
# ============================================
OTA_SERVICE_UUID = "0000ff00-0000-1000-8000-00805f9b34fb"
OTA_CONTROL_UUID = "0000ff01-0000-1000-8000-00805f9b34fb"  # Write
OTA_DATA_UUID = "0000ff02-0000-1000-8000-00805f9b34fb"     # Write No Response
OTA_STATUS_UUID = "0000ff03-0000-1000-8000-00805f9b34fb"   # Notify

# Device name prefix
DEVICE_NAME_PREFIX = "SRT-MGATE"

# ============================================
# OTA Commands (must match OTABle.h)
# ============================================
OTA_CMD_START = 0x50
OTA_CMD_DATA = 0x51
OTA_CMD_VERIFY = 0x52
OTA_CMD_APPLY = 0x53
OTA_CMD_ABORT = 0x54
OTA_CMD_STATUS = 0x55

# Response codes
OTA_RSP_OK = 0x00
OTA_RSP_ERROR = 0x01
OTA_RSP_BUSY = 0x02
OTA_RSP_INVALID_STATE = 0x03
OTA_RSP_SIZE_ERROR = 0x04
OTA_RSP_CHECKSUM_FAIL = 0x05
OTA_RSP_FLASH_ERROR = 0x06

# States
OTA_STATE_IDLE = 0
OTA_STATE_RECEIVING = 1
OTA_STATE_VERIFYING = 2
OTA_STATE_APPLYING = 3
OTA_STATE_COMPLETE = 4
OTA_STATE_ERROR = 5

STATE_NAMES = {
    0: "IDLE",
    1: "RECEIVING",
    2: "VERIFYING",
    3: "APPLYING",
    4: "COMPLETE",
    5: "ERROR"
}

# Transfer settings
CHUNK_SIZE = 240  # Leave room for header (244 max for MTU 247)
ACK_INTERVAL = 20  # Expect ACK every N chunks


class BLEOTAUploader:
    """BLE OTA Firmware Uploader"""

    def __init__(self, device_address: str = None):
        self.device_address = device_address
        self.client: BleakClient = None
        self.current_state = OTA_STATE_IDLE
        self.current_progress = 0
        self.bytes_received = 0
        self.total_bytes = 0
        self.last_error = OTA_RSP_OK
        self.notification_event = asyncio.Event()
        self.connected = False

    def notification_handler(self, sender, data: bytearray):
        """Handle status notifications from device"""
        if len(data) >= 11:
            self.current_state = data[0]
            self.current_progress = data[1]
            self.bytes_received = struct.unpack('<I', data[2:6])[0]
            self.total_bytes = struct.unpack('<I', data[6:10])[0]
            self.last_error = data[10]
            self.notification_event.set()

    async def scan_devices(self, timeout: float = 10.0) -> list:
        """Scan for SRT-MGATE devices"""
        print(f"Scanning for BLE devices ({timeout}s)...")
        devices = await BleakScanner.discover(timeout=timeout)

        ota_devices = []
        for device in devices:
            name = device.name or "Unknown"
            if DEVICE_NAME_PREFIX in name or "OTA" in name.upper():
                ota_devices.append(device)
                print(f"  Found: {name} ({device.address})")

        if not ota_devices:
            print("  No SRT-MGATE devices found.")
            print("  Make sure the device is powered on and BLE is enabled.")

        return ota_devices

    async def connect(self, address: str = None) -> bool:
        """Connect to device"""
        if address:
            self.device_address = address

        if not self.device_address:
            # Scan and select device
            devices = await self.scan_devices()
            if not devices:
                return False

            if len(devices) == 1:
                self.device_address = devices[0].address
                print(f"Auto-selecting: {devices[0].name}")
            else:
                print("\nMultiple devices found. Select one:")
                for i, dev in enumerate(devices):
                    print(f"  [{i+1}] {dev.name} ({dev.address})")
                choice = input("Enter number: ").strip()
                try:
                    idx = int(choice) - 1
                    self.device_address = devices[idx].address
                except (ValueError, IndexError):
                    print("Invalid selection")
                    return False

        print(f"Connecting to {self.device_address}...")

        try:
            self.client = BleakClient(self.device_address)
            await self.client.connect()
            self.connected = True
            print("Connected!")

            # Subscribe to notifications
            await self.client.start_notify(OTA_STATUS_UUID, self.notification_handler)
            print("Subscribed to status notifications")

            return True

        except BleakError as e:
            print(f"Connection failed: {e}")
            return False

    async def disconnect(self):
        """Disconnect from device"""
        if self.client and self.connected:
            try:
                await self.client.stop_notify(OTA_STATUS_UUID)
            except:
                pass
            await self.client.disconnect()
            self.connected = False
            print("Disconnected")

    async def send_command(self, characteristic: str, data: bytes, response: bool = True):
        """Send command to device"""
        await self.client.write_gatt_char(characteristic, data, response=response)

    async def wait_for_notification(self, timeout: float = 5.0) -> bool:
        """Wait for status notification"""
        self.notification_event.clear()
        try:
            await asyncio.wait_for(self.notification_event.wait(), timeout=timeout)
            return True
        except asyncio.TimeoutError:
            return False

    def calculate_sha256(self, filepath: str) -> bytes:
        """Calculate SHA256 hash of file"""
        sha256 = hashlib.sha256()
        with open(filepath, 'rb') as f:
            while chunk := f.read(8192):
                sha256.update(chunk)
        return sha256.digest()

    async def upload_firmware(self, filepath: str) -> bool:
        """Upload firmware file via BLE OTA"""

        # Validate file
        if not os.path.exists(filepath):
            print(f"Error: File not found: {filepath}")
            return False

        file_size = os.path.getsize(filepath)
        if file_size == 0:
            print("Error: File is empty")
            return False

        if file_size > 3 * 1024 * 1024:  # 3MB max
            print(f"Error: File too large ({file_size} bytes, max 3MB)")
            return False

        print(f"\nFirmware: {os.path.basename(filepath)}")
        print(f"Size: {file_size:,} bytes ({file_size/1024:.1f} KB)")

        # Calculate SHA256
        print("Calculating SHA256...", end=" ", flush=True)
        sha256_hash = self.calculate_sha256(filepath)
        print(sha256_hash.hex()[:16] + "...")

        # ============================================
        # Step 1: Send OTA_START command
        # ============================================
        print("\n[1/4] Sending OTA_START...")

        # Build start command: cmd(1) + size(4) + chunkSize(2) + sha256(32)
        start_cmd = struct.pack('<BIH', OTA_CMD_START, file_size, CHUNK_SIZE)
        start_cmd += sha256_hash

        await self.send_command(OTA_CONTROL_UUID, start_cmd)

        # Wait for response
        if not await self.wait_for_notification(timeout=5.0):
            print("Error: No response from device")
            return False

        if self.last_error != OTA_RSP_OK:
            print(f"Error: Device rejected OTA start (code: {self.last_error})")
            return False

        if self.current_state != OTA_STATE_RECEIVING:
            print(f"Error: Unexpected state: {STATE_NAMES.get(self.current_state, 'UNKNOWN')}")
            return False

        print("Device ready to receive firmware")

        # ============================================
        # Step 2: Send firmware chunks
        # ============================================
        print("\n[2/4] Uploading firmware...")

        chunks_total = (file_size + CHUNK_SIZE - 1) // CHUNK_SIZE
        seq_num = 0
        bytes_sent = 0

        # Setup progress bar
        if tqdm:
            pbar = tqdm(total=file_size, unit='B', unit_scale=True, desc="Upload")
        else:
            pbar = None

        try:
            with open(filepath, 'rb') as f:
                while True:
                    chunk = f.read(CHUNK_SIZE)
                    if not chunk:
                        break

                    # Build data command: cmd(1) + seqNum(2) + data(N)
                    data_cmd = struct.pack('<BH', OTA_CMD_DATA, seq_num)
                    data_cmd += chunk

                    # Send chunk (Write No Response for speed)
                    await self.send_command(OTA_DATA_UUID, data_cmd, response=False)

                    bytes_sent += len(chunk)
                    seq_num += 1

                    # Update progress
                    if pbar:
                        pbar.update(len(chunk))
                    else:
                        progress = (bytes_sent * 100) // file_size
                        print(f"\r  Progress: {progress}% ({bytes_sent:,}/{file_size:,} bytes)", end="")

                    # Small delay to prevent buffer overflow
                    if seq_num % 10 == 0:
                        await asyncio.sleep(0.01)

                    # Wait for ACK periodically
                    if seq_num % ACK_INTERVAL == 0:
                        await asyncio.sleep(0.05)  # Let device process

        finally:
            if pbar:
                pbar.close()
            else:
                print()  # New line after progress

        print(f"Sent {seq_num} chunks ({bytes_sent:,} bytes)")

        # Wait for device to process all chunks
        await asyncio.sleep(0.5)

        # ============================================
        # Step 3: Send OTA_VERIFY command
        # ============================================
        print("\n[3/4] Verifying firmware...")

        verify_cmd = bytes([OTA_CMD_VERIFY]) + sha256_hash
        await self.send_command(OTA_CONTROL_UUID, verify_cmd)

        if not await self.wait_for_notification(timeout=10.0):
            print("Error: Verification timeout")
            return False

        if self.last_error == OTA_RSP_CHECKSUM_FAIL:
            print("Error: Checksum verification failed!")
            print("The firmware may be corrupted during transfer.")
            return False

        if self.last_error != OTA_RSP_OK:
            print(f"Error: Verification failed (code: {self.last_error})")
            return False

        print("Firmware verified successfully!")

        # ============================================
        # Step 4: Send OTA_APPLY command
        # ============================================
        print("\n[4/4] Applying update...")

        apply_cmd = bytes([OTA_CMD_APPLY])
        await self.send_command(OTA_CONTROL_UUID, apply_cmd)

        print("Update applied! Device will reboot...")
        print("\n" + "="*50)
        print("OTA UPDATE SUCCESSFUL!")
        print("="*50)

        return True

    async def abort_transfer(self):
        """Abort ongoing transfer"""
        print("Aborting transfer...")
        abort_cmd = bytes([OTA_CMD_ABORT])
        await self.send_command(OTA_CONTROL_UUID, abort_cmd)
        await self.wait_for_notification(timeout=2.0)
        print("Transfer aborted")

    async def get_status(self):
        """Get current OTA status"""
        status_cmd = bytes([OTA_CMD_STATUS])
        await self.send_command(OTA_CONTROL_UUID, status_cmd)

        if await self.wait_for_notification(timeout=2.0):
            print(f"State: {STATE_NAMES.get(self.current_state, 'UNKNOWN')}")
            print(f"Progress: {self.current_progress}%")
            print(f"Received: {self.bytes_received:,} / {self.total_bytes:,} bytes")
            if self.last_error != OTA_RSP_OK:
                print(f"Error: {self.last_error}")
        else:
            print("No response from device")


async def main():
    parser = argparse.ArgumentParser(
        description="BLE OTA Upload Tool for SRT-MGATE-1210",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                          Interactive mode
  %(prog)s -f firmware.bin          Upload specific file
  %(prog)s -f firmware.bin -a AA:BB:CC:DD:EE:FF
                                    Upload to specific device
  %(prog)s --scan                   Scan for devices only
  %(prog)s --abort                  Abort ongoing transfer
        """
    )
    parser.add_argument('-f', '--file', type=str, help='Firmware binary file (.bin)')
    parser.add_argument('-a', '--address', type=str, help='Device BLE address')
    parser.add_argument('--scan', action='store_true', help='Scan for devices only')
    parser.add_argument('--abort', action='store_true', help='Abort ongoing transfer')
    parser.add_argument('--status', action='store_true', help='Get current status')

    args = parser.parse_args()

    uploader = BLEOTAUploader(args.address)

    # Scan only mode
    if args.scan:
        await uploader.scan_devices(timeout=15.0)
        return

    # Connect to device
    if not await uploader.connect(args.address):
        return

    try:
        # Abort mode
        if args.abort:
            await uploader.abort_transfer()
            return

        # Status mode
        if args.status:
            await uploader.get_status()
            return

        # Upload mode
        firmware_path = args.file
        if not firmware_path:
            # Interactive file selection
            print("\nEnter firmware file path (or drag & drop):")
            firmware_path = input("> ").strip().strip('"').strip("'")

        if not firmware_path:
            print("No file specified")
            return

        # Confirm upload
        print(f"\nReady to upload: {os.path.basename(firmware_path)}")
        confirm = input("Proceed with OTA update? [y/N]: ").strip().lower()
        if confirm != 'y':
            print("Cancelled")
            return

        # Perform upload
        success = await uploader.upload_firmware(firmware_path)

        if not success:
            print("\nOTA update failed!")
            sys.exit(1)

    except KeyboardInterrupt:
        print("\n\nInterrupted by user")
        await uploader.abort_transfer()
    except Exception as e:
        print(f"\nError: {e}")
        import traceback
        traceback.print_exc()
    finally:
        await uploader.disconnect()


if __name__ == "__main__":
    asyncio.run(main())
