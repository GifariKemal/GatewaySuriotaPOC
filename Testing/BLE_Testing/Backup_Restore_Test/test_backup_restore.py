#!/usr/bin/env python3
"""
BLE BACKUP & RESTORE TESTING TOOL
==================================

Comprehensive testing tool for BLE Backup & Restore feature
Supports automated testing with validation and manual testing mode
Author: SURIOTA R&D Team
Version: 1.1.0
Date: November 22, 2025
"""

import asyncio
import json
import sys
import os
from pathlib import Path
from datetime import datetime
from bleak import BleakClient, BleakScanner
import time

# ============================================================================
# Directory Configuration
# ============================================================================
# Get the directory where this script is located
SCRIPT_DIR = Path(__file__).parent
BACKUP_DIR = SCRIPT_DIR  # Save backups in same directory as script

# Create backup directory if it doesn't exist
BACKUP_DIR.mkdir(parents=True, exist_ok=True)

print(f"[INIT] Script directory: {SCRIPT_DIR}")
print(f"[INIT] Backup directory: {BACKUP_DIR}")

# ============================================================================
# BLE Configuration (must match firmware)
# ============================================================================
SERVICE_UUID = "00001830-0000-1000-8000-00805f9b34fb"
COMMAND_CHAR_UUID = "11111111-1111-1111-1111-111111111101"
RESPONSE_CHAR_UUID = "11111111-1111-1111-1111-111111111102"
SERVICE_NAME = "SURIOTA GW"

# ============================================================================
# Global Variables for Response Handling
# ============================================================================
response_buffer = []
response_complete = False


# ============================================================================
# BLE Response Handler
# ============================================================================
def notification_handler(sender, data):
    """Handle incoming BLE notifications (fragmented responses)"""
    global response_buffer, response_complete

    try:
        chunk = data.decode('utf-8')

        if chunk == "<END>":
            response_complete = True
            print(f"[BLE] ✓ Response complete ({len(response_buffer)} fragments)")
        else:
            response_buffer.append(chunk)
            print(f"[BLE] Fragment {len(response_buffer)}: {len(chunk)} bytes")
    except Exception as e:
        print(f"[BLE] ⚠️  Error decoding: {e}")


# ============================================================================
# BLE Connection Functions
# ============================================================================
async def scan_and_connect():
    """Scan for and connect to SURIOTA Gateway"""
    print("\n" + "="*70)
    print(f"🔍 SCANNING FOR '{SERVICE_NAME}'")
    print("="*70)

    devices = await BleakScanner.discover(timeout=10.0)

    if not devices:
        print("❌ No BLE devices found!")
        return None

    # Find SURIOTA Gateway
    gateway = None
    for d in devices:
        if d.name == SERVICE_NAME:
            gateway = d
            break

    if not gateway:
        print(f"❌ '{SERVICE_NAME}' not found!")
        print("\nAvailable devices:")
        for d in devices:
            print(f"  - {d.name} ({d.address})")
        return None

    print(f"✓ Found: {gateway.name}")
    print(f"✓ Address: {gateway.address}")

    # Connect
    print(f"\n🔗 CONNECTING...")
    client = BleakClient(gateway.address, timeout=15.0)
    await client.connect()

    if client.is_connected:
        print("✓ Connected successfully!")

        # Enable notifications
        await client.start_notify(RESPONSE_CHAR_UUID, notification_handler)
        print("✓ Notifications enabled")

        # Wait for MTU negotiation
        await asyncio.sleep(2)

        return client
    else:
        print("❌ Connection failed!")
        return None


async def send_command(client, command):
    """Send JSON command and wait for complete response"""
    global response_buffer, response_complete

    # Reset buffers
    response_buffer = []
    response_complete = False

    # Convert to JSON string
    command_json = json.dumps(command, separators=(',', ':'))
    print(f"\n📤 SENDING COMMAND:")
    print(f"   {command_json}")

    # Fragmentation: 18 bytes per chunk (matching ble_test.py)
    chunk_size = 18
    total_chunks = (len(command_json) + chunk_size - 1) // chunk_size

    print(f"   Fragmenting into {total_chunks} chunk(s) ({chunk_size} bytes/chunk)...")

    # Send command in chunks
    for i in range(0, len(command_json), chunk_size):
        chunk = command_json[i:i+chunk_size]
        chunk_num = (i // chunk_size) + 1

        await client.write_gatt_char(COMMAND_CHAR_UUID, chunk.encode('utf-8'))
        print(f"   Chunk {chunk_num}/{total_chunks} sent ({len(chunk)} bytes)")
        await asyncio.sleep(0.1)  # Delay for stable transmission

    # Send end marker (CRITICAL!)
    await client.write_gatt_char(COMMAND_CHAR_UUID, "<END>".encode('utf-8'))
    print(f"   ✓ End marker <END> sent")

    # Wait for response with timeout
    timeout = 60  # 60 seconds for large responses
    start_time = time.time()

    print(f"\n📥 WAITING FOR RESPONSE (timeout: {timeout}s)...")

    while not response_complete:
        await asyncio.sleep(0.1)

        if time.time() - start_time > timeout:
            print(f"⏱️  TIMEOUT after {timeout} seconds!")
            return None

    # Assemble response
    full_response = ''.join(response_buffer)

    try:
        response = json.loads(full_response)
        return response
    except json.JSONDecodeError as e:
        print(f"❌ JSON Parse Error: {e}")
        print(f"Raw response (first 500 chars): {full_response[:500]}")
        return None


# ============================================================================
# TEST PAYLOADS
# ============================================================================

def get_backup_payload():
    """Get backup (full_config) command payload"""
    return {
        "op": "read",
        "type": "full_config"
    }


def get_restore_payload(config):
    """Get restore command payload with config data"""
    return {
        "op": "system",
        "type": "restore_config",
        "config": config
    }


def get_restore_invalid_payload():
    """Get invalid restore payload (missing config field)"""
    return {
        "op": "system",
        "type": "restore_config"
        # Missing "config" field
    }


# ============================================================================
# TEST FUNCTIONS
# ============================================================================

async def test_backup(client):
    """
    TEST 1: Backup Configuration

    Purpose: Export complete gateway configuration
    Expected: Success response with config data
    """
    print("\n" + "="*70)
    print("TEST 1: BACKUP CONFIGURATION (full_config)")
    print("="*70)

    # Send backup command
    command = get_backup_payload()
    response = await send_command(client, command)

    if not response:
        print("❌ TEST FAILED: No response received")
        return None

    # Validate response structure
    print("\n✅ RESPONSE RECEIVED")
    print(f"   Status: {response.get('status', 'UNKNOWN')}")

    if response.get('status') != 'ok':
        print(f"❌ TEST FAILED: Status is not 'ok'")
        return None

    # Check backup_info
    backup_info = response.get('backup_info', {})
    print(f"\n📊 BACKUP INFO:")
    print(f"   Firmware Version: {backup_info.get('firmware_version', 'N/A')}")
    print(f"   Device Name: {backup_info.get('device_name', 'N/A')}")
    print(f"   Total Devices: {backup_info.get('total_devices', 0)}")
    print(f"   Total Registers: {backup_info.get('total_registers', 0)}")
    print(f"   Processing Time: {backup_info.get('processing_time_ms', 0)} ms")
    print(f"   Backup Size: {backup_info.get('backup_size_bytes', 0)} bytes ({backup_info.get('backup_size_bytes', 0)/1024:.2f} KB)")

    # Check config structure
    config = response.get('config', {})

    devices = config.get('devices', [])
    server_config = config.get('server_config', {})
    logging_config = config.get('logging_config', {})

    print(f"\n📦 CONFIG STRUCTURE:")
    print(f"   ✓ Devices: {len(devices)} found")
    print(f"   ✓ Server Config: {'Present' if server_config else 'Missing'}")
    print(f"   ✓ Logging Config: {'Present' if logging_config else 'Missing'}")

    # Validate devices have registers
    total_registers = 0
    for device in devices:
        device_id = device.get('device_id', 'UNKNOWN')
        registers = device.get('registers', [])
        total_registers += len(registers)
        print(f"      - Device {device_id}: {len(registers)} registers")

    print(f"\n✅ TEST PASSED: Backup successful")
    print(f"   Total: {len(devices)} devices, {total_registers} registers")

    return response


async def test_restore(client, config):
    """
    TEST 2: Restore Configuration

    Purpose: Import and apply configuration from backup
    Expected: Success response with restore summary
    """
    print("\n" + "="*70)
    print("TEST 2: RESTORE CONFIGURATION (restore_config)")
    print("="*70)

    if not config:
        print("❌ TEST SKIPPED: No config data provided")
        return False

    # Confirm restore
    print("\n⚠️  WARNING: This will REPLACE all current configurations!")
    print(f"   Devices to restore: {len(config.get('devices', []))}")
    print(f"   Server config: {'Yes' if config.get('server_config') else 'No'}")
    print(f"   Logging config: {'Yes' if config.get('logging_config') else 'No'}")

    confirm = input("\n⚠️  Continue with restore? (yes/no): ")
    if confirm.lower() != 'yes':
        print("❌ TEST CANCELLED by user")
        return False

    # Send restore command
    command = get_restore_payload(config)
    response = await send_command(client, command)

    if not response:
        print("❌ TEST FAILED: No response received")
        return False

    # Validate response
    print("\n✅ RESPONSE RECEIVED")
    print(f"   Status: {response.get('status', 'UNKNOWN')}")

    if response.get('status') != 'ok':
        print(f"❌ TEST FAILED: Status is not 'ok'")
        print(f"   Error: {response.get('error', 'Unknown error')}")
        return False

    # Check restore results
    restored_configs = response.get('restored_configs', [])
    success_count = response.get('success_count', 0)
    fail_count = response.get('fail_count', 0)
    message = response.get('message', '')
    requires_restart = response.get('requires_restart', False)

    print(f"\n📊 RESTORE RESULTS:")
    print(f"   Success Count: {success_count}")
    print(f"   Fail Count: {fail_count}")
    print(f"   Restored Configs: {', '.join(restored_configs)}")
    print(f"   Message: {message}")
    print(f"   Requires Restart: {'Yes' if requires_restart else 'No'}")

    if fail_count > 0:
        print(f"\n⚠️  WARNING: {fail_count} configuration(s) failed to restore")
        print("   Check serial monitor for details")

    if success_count > 0:
        print(f"\n✅ TEST PASSED: Restore successful ({success_count}/{success_count + fail_count})")
        if requires_restart:
            print("\n⚠️  DEVICE RESTART RECOMMENDED")
    else:
        print(f"❌ TEST FAILED: No configurations restored")
        return False

    return True


async def test_restore_invalid(client):
    """
    TEST 3: Restore with Invalid Payload

    Purpose: Test error handling for missing config field
    Expected: Error response
    """
    print("\n" + "="*70)
    print("TEST 3: RESTORE WITH INVALID PAYLOAD (Error Handling)")
    print("="*70)

    # Send invalid restore command
    command = get_restore_invalid_payload()
    print("\n⚠️  Sending restore command WITHOUT 'config' field...")

    response = await send_command(client, command)

    if not response:
        print("❌ TEST FAILED: No response received")
        return False

    # Validate error response
    print("\n✅ RESPONSE RECEIVED")
    print(f"   Status: {response.get('status', 'UNKNOWN')}")

    if response.get('status') == 'error':
        error_msg = response.get('error', '')
        print(f"   Error: {error_msg}")

        if "Missing 'config' object" in error_msg:
            print(f"\n✅ TEST PASSED: Error handling works correctly")
            return True
        else:
            print(f"\n⚠️  Unexpected error message: {error_msg}")
            return False
    else:
        print(f"❌ TEST FAILED: Expected error status, got '{response.get('status')}'")
        return False


async def test_backup_restore_cycle(client):
    """
    TEST 4: Complete Backup-Restore-Compare Cycle

    Purpose: Verify data integrity through full cycle
    Expected: Restored config matches original backup
    """
    print("\n" + "="*70)
    print("TEST 4: BACKUP-RESTORE-COMPARE CYCLE (Data Integrity)")
    print("="*70)

    # Step 1: First backup
    print("\n[STEP 1/4] Creating initial backup...")
    backup1 = await test_backup(client)
    if not backup1:
        print("❌ TEST FAILED: Initial backup failed")
        return False

    config1 = backup1.get('config', {})
    devices1_count = len(config1.get('devices', []))

    # Save backup to file
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    backup_filename = f"backup_before_restore_{timestamp}.json"
    backup_file = BACKUP_DIR / backup_filename
    with open(backup_file, 'w', encoding='utf-8') as f:
        json.dump(backup1, f, indent=2, ensure_ascii=False)
    print(f"   ✓ Backup saved: {backup_file}")

    # Step 2: Restore the backup
    print("\n[STEP 2/4] Restoring configuration...")
    print("⚠️  This will restore the same config (should succeed)")

    if not await test_restore(client, config1):
        print("❌ TEST FAILED: Restore failed")
        return False

    # Wait for restore to complete
    print("\n⏳ Waiting 3 seconds for restore to complete...")
    await asyncio.sleep(3)

    # Step 3: Second backup (after restore)
    print("\n[STEP 3/4] Creating backup after restore...")
    backup2 = await test_backup(client)
    if not backup2:
        print("❌ TEST FAILED: Post-restore backup failed")
        return False

    config2 = backup2.get('config', {})
    devices2_count = len(config2.get('devices', []))

    # Save second backup
    backup_filename2 = f"backup_after_restore_{timestamp}.json"
    backup_file2 = BACKUP_DIR / backup_filename2
    with open(backup_file2, 'w', encoding='utf-8') as f:
        json.dump(backup2, f, indent=2, ensure_ascii=False)
    print(f"   ✓ Backup saved: {backup_file2}")

    # Step 4: Compare backups
    print("\n[STEP 4/4] Comparing backups...")

    if devices1_count != devices2_count:
        print(f"❌ TEST FAILED: Device count mismatch!")
        print(f"   Before: {devices1_count} devices")
        print(f"   After: {devices2_count} devices")
        return False

    print(f"   ✓ Device count matches: {devices1_count}")

    # Compare device IDs
    devices1 = config1.get('devices', [])
    devices2 = config2.get('devices', [])

    ids1 = set(d.get('device_id') for d in devices1)
    ids2 = set(d.get('device_id') for d in devices2)

    if ids1 != ids2:
        print(f"❌ TEST FAILED: Device IDs mismatch!")
        print(f"   Missing: {ids1 - ids2}")
        print(f"   Extra: {ids2 - ids1}")
        return False

    print(f"   ✓ Device IDs match")

    # Compare register counts
    total_regs1 = sum(len(d.get('registers', [])) for d in devices1)
    total_regs2 = sum(len(d.get('registers', [])) for d in devices2)

    if total_regs1 != total_regs2:
        print(f"❌ TEST FAILED: Register count mismatch!")
        print(f"   Before: {total_regs1} registers")
        print(f"   After: {total_regs2} registers")
        return False

    print(f"   ✓ Register count matches: {total_regs1}")

    print(f"\n✅ TEST PASSED: Data integrity verified")
    print(f"   Backup files saved for manual inspection:")
    print(f"   - {backup_file}")
    print(f"   - {backup_file2}")

    return True


async def save_backup_to_file(backup, filename=None):
    """Save backup to JSON file in backup directory"""
    if not filename:
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"gateway_backup_{timestamp}.json"

    # Ensure filename is just the name, not a path
    filename = Path(filename).name
    filepath = BACKUP_DIR / filename

    with open(filepath, 'w', encoding='utf-8') as f:
        json.dump(backup, f, indent=2, ensure_ascii=False)

    print(f"\n💾 Backup saved to: {filepath}")
    print(f"   Size: {filepath.stat().st_size} bytes ({filepath.stat().st_size/1024:.2f} KB)")

    return str(filepath)


async def load_backup_from_file(filename):
    """Load backup from JSON file in backup directory"""
    try:
        # If filename is just a name (no path), look in backup directory
        filepath = Path(filename)
        if not filepath.is_absolute() and not filepath.parent.exists():
            filepath = BACKUP_DIR / filename

        with open(filepath, 'r', encoding='utf-8') as f:
            backup = json.load(f)

        print(f"\n📂 Backup loaded from: {filepath}")
        print(f"   Size: {filepath.stat().st_size} bytes ({filepath.stat().st_size/1024:.2f} KB)")

        backup_info = backup.get('backup_info', {})
        print(f"   Firmware: {backup_info.get('firmware_version', 'N/A')}")
        print(f"   Devices: {backup_info.get('total_devices', 0)}")
        print(f"   Registers: {backup_info.get('total_registers', 0)}")

        return backup
    except FileNotFoundError:
        print(f"❌ File not found: {filepath if 'filepath' in locals() else filename}")
        print(f"   Searched in: {BACKUP_DIR}")
        return None
    except json.JSONDecodeError as e:
        print(f"❌ Invalid JSON: {e}")
        return None


def list_backup_files():
    """List all backup JSON files in backup directory"""
    backup_files = sorted(BACKUP_DIR.glob("*.json"), key=lambda p: p.stat().st_mtime, reverse=True)

    if not backup_files:
        print(f"\n📂 No backup files found in: {BACKUP_DIR}")
        return []

    print(f"\n📂 AVAILABLE BACKUP FILES ({len(backup_files)} found):")
    print(f"   Location: {BACKUP_DIR}\n")

    for i, filepath in enumerate(backup_files, 1):
        size_kb = filepath.stat().st_size / 1024
        mtime = datetime.fromtimestamp(filepath.stat().st_mtime)
        print(f"   {i}. {filepath.name}")
        print(f"      Size: {size_kb:.2f} KB | Modified: {mtime.strftime('%Y-%m-%d %H:%M:%S')}")

    return [f.name for f in backup_files]


# ============================================================================
# MENU SYSTEM
# ============================================================================

def print_menu():
    """Display main menu"""
    print("\n" + "="*70)
    print("BLE BACKUP & RESTORE TEST MENU")
    print("="*70)
    print("\n📋 AVAILABLE TESTS:")
    print("   1. Test Backup (full_config)")
    print("   2. Test Restore (from previous backup)")
    print("   3. Test Restore Error Handling (invalid payload)")
    print("   4. Test Complete Backup-Restore Cycle")
    print("\n💾 FILE OPERATIONS:")
    print("   5. Save Backup to File")
    print("   6. List Available Backup Files")
    print("   7. Load Backup from File and Restore")
    print("\n🚀 AUTOMATED:")
    print("   8. Run ALL Tests (Automated Suite)")
    print("\n   0. Exit")
    print("="*70)
    print(f"\n📂 Backup Directory: {BACKUP_DIR}")


async def interactive_mode(client):
    """Interactive testing menu"""
    last_backup = None

    while True:
        print_menu()

        choice = input("\n👉 Select test (0-8): ").strip()

        if choice == '0':
            print("\n👋 Exiting...")
            break

        elif choice == '1':
            last_backup = await test_backup(client)
            if last_backup:
                print("\n💡 TIP: Use option 2 to restore this backup, or option 5 to save to file")

        elif choice == '2':
            if not last_backup:
                print("\n⚠️  No backup available. Run Test 1 first or load from file (option 7)")
            else:
                config = last_backup.get('config', {})
                await test_restore(client, config)

        elif choice == '3':
            await test_restore_invalid(client)

        elif choice == '4':
            await test_backup_restore_cycle(client)

        elif choice == '5':
            if not last_backup:
                print("\n⚠️  No backup available. Run Test 1 first")
            else:
                filename = input("Enter filename (or press Enter for default): ").strip()
                if not filename:
                    filename = None
                await save_backup_to_file(last_backup, filename)

        elif choice == '6':
            list_backup_files()

        elif choice == '7':
            # List available files first
            available_files = list_backup_files()

            if not available_files:
                print("\n💡 TIP: Create a backup first (option 1) or save one (option 5)")
            else:
                print("\n💡 Enter filename from the list above, or type a custom path")
                filename = input("Enter backup filename: ").strip()

                if not filename:
                    print("❌ No filename provided")
                else:
                    backup = await load_backup_from_file(filename)
                    if backup:
                        config = backup.get('config', {})
                        await test_restore(client, config)

        elif choice == '8':
            # Run all tests
            print("\n" + "="*70)
            print("🚀 RUNNING AUTOMATED TEST SUITE")
            print("="*70)

            results = {
                'test1': False,
                'test2': False,
                'test3': False,
                'test4': False
            }

            # Test 1: Backup
            backup = await test_backup(client)
            results['test1'] = backup is not None

            if backup:
                # Test 3: Error handling (can run independently)
                results['test3'] = await test_restore_invalid(client)

                # Test 4: Full cycle (includes restore)
                results['test4'] = await test_backup_restore_cycle(client)

            # Summary
            print("\n" + "="*70)
            print("📊 TEST SUITE SUMMARY")
            print("="*70)

            total_tests = len(results)
            passed_tests = sum(1 for r in results.values() if r)

            for test_name, result in results.items():
                status = "✅ PASSED" if result else "❌ FAILED"
                print(f"   {test_name}: {status}")

            print(f"\n   Total: {passed_tests}/{total_tests} tests passed")

            if passed_tests == total_tests:
                print("\n   🎉 ALL TESTS PASSED!")
            else:
                print(f"\n   ⚠️  {total_tests - passed_tests} test(s) failed")

        else:
            print("❌ Invalid choice. Please select 0-8")

        input("\n⏸️  Press Enter to continue...")


# ============================================================================
# MAIN FUNCTION
# ============================================================================

async def main():
    """Main entry point"""
    print("\n" + "="*70)
    print("🔧 BLE BACKUP & RESTORE TESTING TOOL")
    print("="*70)
    print("\nVersion: 1.1.0")
    print("Author: SURIOTA R&D Team")
    print("Date: November 22, 2025")
    print(f"Backup Directory: {BACKUP_DIR}")

    # Connect to gateway
    client = await scan_and_connect()

    if not client:
        print("\n❌ Failed to connect to gateway")
        return

    try:
        # Interactive mode
        await interactive_mode(client)

    except KeyboardInterrupt:
        print("\n\n⚠️  Interrupted by user")

    finally:
        # Disconnect
        if client.is_connected:
            print("\n🔌 Disconnecting...")
            await client.disconnect()
            print("✓ Disconnected")


if __name__ == "__main__":
    asyncio.run(main())
