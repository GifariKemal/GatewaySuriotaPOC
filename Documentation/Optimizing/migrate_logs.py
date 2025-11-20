#!/usr/bin/env python3
"""
Log Migration Helper Script
Automatically converts Serial.printf() statements to new LOG_*() macros
"""

import re
import sys
from pathlib import Path

# Migration rules: (pattern, replacement, log_level)
MIGRATION_RULES = [
    # RTU Service - ERROR level
    (r'Serial\.printf\("\[RTU\] ERROR:', r'LOG_RTU_ERROR("', 'ERROR'),
    (r'Serial\.printf\("\[RTU\] Device .* All .* failed', r'LOG_RTU_ERROR("Device %s: All %d register reads failed', 'ERROR'),

    # RTU Service - WARN level
    (r'Serial\.printf\("\[RTU\] .*%s = ERROR', r'LOG_RTU_WARN("%s: %s = ERROR', 'WARN'),
    (r'Serial\.printf\("\[RTU\] Device .* read failed\. Retry', r'LOG_RTU_WARN("Device %s read failed. Retry %d/%d', 'WARN'),

    # RTU Service - INFO level
    (r'Serial\.printf\("\[RTU\] Device .* is disabled', r'LOG_RTU_INFO("Device %s is disabled', 'INFO'),
    (r'Serial\.printf\("\[RTU\] Device .* Read successful', r'LOG_RTU_INFO("Device %s: Read successful', 'INFO'),
    (r'Serial\.printf\("\[RTU\] Device .* enabled', r'LOG_RTU_INFO("Device %s enabled', 'INFO'),
    (r'Serial\.printf\("\[RTU\] Device .* disabled', r'LOG_RTU_INFO("Device %s disabled', 'INFO'),
    (r'Serial\.println\("\[RTU\] Initializing', r'LOG_RTU_INFO("Initializing', 'INFO'),
    (r'Serial\.printf\("\[RTU Task\] Found %d RTU devices', r'LOG_RTU_INFO("Found %d RTU devices', 'INFO'),

    # RTU Service - DEBUG level
    (r'Serial\.printf\("\[RTU\] Device .* retry backoff not elapsed', r'LOG_RTU_DEBUG("Device %s retry backoff not elapsed', 'DEBUG'),
    (r'Serial\.printf\("\[RTU\] Exponential backoff:', r'LOG_RTU_DEBUG("Exponential backoff:', 'DEBUG'),
    (r'Serial\.printf\("\[RTU\] Reset failure state', r'LOG_RTU_DEBUG("Reset failure state', 'DEBUG'),
    (r'Serial\.printf\("\[RTU Task\] Refreshing device list', r'LOG_RTU_DEBUG("Refreshing device list', 'DEBUG'),

    # RTU Service - VERBOSE level
    (r'Serial\.printf\("\[RTU\] Polling device %s \(Slave:', r'LOG_RTU_VERBOSE("Polling device %s (Slave:', 'VERBOSE'),
    (r'Serial\.printf\("\[RTU\] Reconfiguring Serial', r'LOG_RTU_VERBOSE("Reconfiguring Serial', 'VERBOSE'),

    # TCP Service - ERROR level
    (r'Serial\.printf\("\[TCP\] ERROR:', r'LOG_TCP_ERROR("', 'ERROR'),
    (r'Serial\.println\("\[TCP\] ERROR:', r'LOG_TCP_ERROR("', 'ERROR'),

    # TCP Service - INFO level
    (r'Serial\.printf\("\[TCP\] Connected to', r'LOG_TCP_INFO("Connected to', 'INFO'),
    (r'Serial\.printf\("\[TCP Task\] Found %d TCP devices', r'LOG_TCP_INFO("Found %d TCP devices', 'INFO'),
    (r'Serial\.println\("\[TCP\] Initializing', r'LOG_TCP_INFO("Initializing', 'INFO'),

    # TCP Service - DEBUG level
    (r'Serial\.printf\("\[TCP Task\] Refreshing device list', r'LOG_TCP_DEBUG("Refreshing device list', 'DEBUG'),

    # TCP Service - VERBOSE level
    (r'Serial\.printf\("\[TCP\] Polling device', r'LOG_TCP_VERBOSE("Polling device', 'VERBOSE'),
    (r'Serial\.printf\("\[TCP\] Reading register', r'LOG_TCP_VERBOSE("Reading register', 'VERBOSE'),

    # MQTT Service - ERROR level
    (r'Serial\.printf\("\[MQTT\] ERROR:', r'LOG_MQTT_ERROR("', 'ERROR'),
    (r'Serial\.printf\("\[MQTT\] Connection failed:', r'LOG_MQTT_ERROR("Connection failed:', 'ERROR'),
    (r'Serial\.printf\("\[MQTT\] .*Publish failed', r'LOG_MQTT_ERROR("', 'ERROR'),

    # MQTT Service - WARN level
    (r'Serial\.printf\("\[MQTT\] Connection lost', r'LOG_MQTT_WARN("Connection lost', 'WARN'),

    # MQTT Service - INFO level
    (r'Serial\.printf\("\[MQTT\] Connected to', r'LOG_MQTT_INFO("Connected to', 'INFO'),
    (r'Serial\.printf\("\[MQTT\] Default Mode: Published', r'LOG_MQTT_INFO("Default Mode: Published', 'INFO'),
    (r'Serial\.printf\("\[MQTT\] Customize Mode: Published', r'LOG_MQTT_INFO("Customize Mode: Published', 'INFO'),
    (r'Serial\.println\("\[MQTT\] Task started', r'LOG_MQTT_INFO("Task started', 'INFO'),
    (r'Serial\.printf\("\[MQTT\] Config loaded', r'LOG_MQTT_INFO("Config loaded', 'INFO'),

    # MQTT Service - DEBUG level
    (r'Serial\.println\("\[MQTT\] Waiting for device batch', r'LOG_MQTT_DEBUG("Waiting for device batch', 'DEBUG'),
    (r'Serial\.printf\("\[MQTT\] Waiting for network', r'LOG_MQTT_DEBUG("Waiting for network', 'DEBUG'),
    (r'Serial\.printf\("\[MQTT\] Skipped .* deleted device', r'LOG_MQTT_DEBUG("Skipped %d registers', 'DEBUG'),

    # BATCH Manager - ERROR level
    (r'Serial\.println\("\[BATCH\] ERROR:', r'LOG_BATCH_ERROR("', 'ERROR'),

    # BATCH Manager - INFO level
    (r'Serial\.printf\("\[BATCH\] Started tracking', r'LOG_BATCH_INFO("Started tracking', 'INFO'),
    (r'Serial\.printf\("\[BATCH\] Device .* COMPLETE', r'LOG_BATCH_INFO("Device %s COMPLETE', 'INFO'),
    (r'Serial\.printf\("\[BATCH\] Cleared batch status', r'LOG_BATCH_INFO("Cleared batch status', 'INFO'),

    # BATCH Manager - DEBUG level
    (r'Serial\.printf\("\[BATCH\] Active batches:', r'LOG_BATCH_DEBUG("Active batches:', 'DEBUG'),

    # CONFIG Manager - INFO level
    (r'Serial\.printf\("\[CONFIG\] ', r'LOG_CONFIG_INFO("', 'INFO'),
    (r'Serial\.println\("\[CONFIG\] ', r'LOG_CONFIG_INFO("', 'INFO'),

    # BLE Service - INFO level
    (r'Serial\.printf\("\[BLE\] Connected', r'LOG_BLE_INFO("Connected', 'INFO'),
    (r'Serial\.printf\("\[BLE\] Client connected', r'LOG_BLE_INFO("Client connected', 'INFO'),
    (r'Serial\.println\("\[BLE\] Advertising started', r'LOG_BLE_INFO("Advertising started', 'INFO'),

    # BLE Service - WARN level
    (r'Serial\.printf\("\[BLE\] WARNING:', r'LOG_BLE_WARN("', 'WARN'),
    (r'Serial\.printf\("\[BLE MTU\] WARNING:', r'LOG_BLE_WARN("', 'WARN'),

    # DATA logs (keep as is - compact format)
    # These use special formatting, don't migrate
]

def migrate_file(file_path):
    """Migrate logging statements in a single file"""
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()

        original_content = content
        changes = 0

        # Apply each migration rule
        for pattern, replacement, level in MIGRATION_RULES:
            matches = re.findall(pattern, content)
            if matches:
                content, count = re.subn(pattern, replacement, content)
                changes += count
                if count > 0:
                    print(f"  [{level}] Migrated {count} occurrence(s)")

        # Write back if changes were made
        if changes > 0:
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(content)
            print(f"✓ {file_path.name}: {changes} migrations applied")
            return True
        else:
            print(f"○ {file_path.name}: No migrations needed")
            return False

    except Exception as e:
        print(f"✗ {file_path.name}: Error - {e}")
        return False

def main():
    if len(sys.argv) > 1:
        # Migrate specific files
        files = [Path(f) for f in sys.argv[1:]]
    else:
        # Migrate all .cpp and .h files in Main directory
        main_dir = Path(__file__).parent.parent / "Main"
        files = list(main_dir.glob("*.cpp")) + list(main_dir.glob("*.h"))

    print("=" * 60)
    print("Log Migration Tool")
    print("=" * 60)
    print(f"Processing {len(files)} files...\n")

    migrated_count = 0
    for file_path in files:
        if file_path.exists():
            if migrate_file(file_path):
                migrated_count += 1

    print("\n" + "=" * 60)
    print(f"Migration complete: {migrated_count}/{len(files)} files updated")
    print("=" * 60)
    print("\nNext steps:")
    print("1. Review changes with: git diff")
    print("2. Test compilation in Arduino IDE")
    print("3. Verify log levels: setLogLevel(LOG_INFO) in setup()")
    print("4. Manual review of [DATA] logs (kept as-is)")

if __name__ == "__main__":
    main()
