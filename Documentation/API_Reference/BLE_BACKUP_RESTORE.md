# BLE Backup & Restore API Reference

**Version:** 1.3.0 (Pagination Support!)
**Last Updated:** December 10, 2025
**Component:** BLE CRUD Handler - Configuration Backup & Restore
**Firmware Required:** v2.5.12+

---

## 📋 Table of Contents

1. [Overview](#overview)
2. [Backup Command](#backup-command-full_config)
3. [Restore Command](#restore-command-restore_config)
4. [Complete Workflow](#complete-workflow)
5. [Mobile App Integration](#mobile-app-integration)
6. [Error Handling](#error-handling)
7. [Best Practices](#best-practices)

---

## Overview

The **Backup & Restore** feature provides a complete configuration management system via BLE. This allows mobile apps to:

- ✅ **Backup**: Download complete gateway configuration as JSON
- ✅ **Save**: Store backup as file on mobile device
- ✅ **Restore**: Upload and apply configuration from backup file
- ✅ **Clone**: Transfer configuration between multiple gateways
- ✅ **Version Control**: Maintain multiple backup versions

### Key Features

✅ **Single Command Backup**: Get all configs (devices, server, logging) in one call
✅ **Atomic Snapshot**: Consistent configuration at single point in time
✅ **Complete Metadata**: Timestamp, firmware version, statistics
✅ **PSRAM Optimized**: Handles large configurations (100KB+)
✅ **BLE Fragmentation**: Automatic chunking for large responses
✅ **Restore Validation**: Validates backup structure before applying
✅ **Service Notification**: Automatically notifies Modbus/MQTT/HTTP services after restore
✅ **Device ID Preservation**: Device IDs and register IDs preserved during restore (BUG #32 fixed!)
✅ **Section-Based Pagination**: (v2.5.12+) Fetch config sections separately for better BLE performance
✅ **Device Pagination**: (v2.5.12+) Paginate device list for large configurations

---

## Backup Command: `full_config`

### Description

Export complete gateway configuration including all devices, registers, server settings, and logging config.

**Operation:** `read`
**Type:** `full_config`

---

### Request Format

**Basic (All Data):**
```json
{
  "op": "read",
  "type": "full_config"
}
```

**Section-Based (v2.5.12+):**
```json
{
  "op": "read",
  "type": "full_config",
  "section": "devices"
}
```

**With Device Pagination (v2.5.12+):**
```json
{
  "op": "read",
  "type": "full_config",
  "section": "devices",
  "device_offset": 0,
  "device_limit": 2
}
```

**Metadata Only (v2.5.12+):**
```json
{
  "op": "read",
  "type": "full_config",
  "section": "metadata"
}
```

### Request Fields

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `op` | string | ✅ Yes | - | Must be `"read"` |
| `type` | string | ✅ Yes | - | Must be `"full_config"` |
| `section` | string | ❌ No | `"all"` | Section to fetch: `"all"`, `"devices"`, `"server_config"`, `"logging_config"`, `"metadata"` |
| `device_offset` | integer | ❌ No | `0` | Device offset for pagination (only with section `"all"` or `"devices"`) |
| `device_limit` | integer | ❌ No | `-1` | Max devices to return (-1 = all) |

### Available Sections

| Section | Description | Data Included |
|---------|-------------|---------------|
| `all` | Complete backup (default) | devices, server_config, logging_config |
| `devices` | Devices only | devices array with all registers |
| `server_config` | Server config only | MQTT, HTTP, WiFi, Ethernet settings |
| `logging_config` | Logging config only | Retention and interval settings |
| `metadata` | Stats only (no data) | Totals + pagination recommendations |

---

### Response Format

```json
{
  "status": "ok",
  "backup_info": {
    "timestamp": 1732123456,
    "firmware_version": "2.3.1",
    "device_name": "SURIOTA_GW",
    "total_devices": 5,
    "total_registers": 50,
    "processing_time_ms": 350,
    "backup_size_bytes": 102400
  },
  "config": {
    "devices": [
      {
        "device_id": "D7A3F2",
        "device_name": "Temperature Sensor",
        "protocol": "RTU",
        "slave_id": 1,
        "serial_port": 1,
        "baud_rate": 9600,
        "registers": [
          {
            "register_id": "R001",
            "register_name": "Temperature",
            "address": 0,
            "function_code": 3,
            "data_type": "FLOAT32_ABCD",
            "quantity": 2
          }
        ]
      }
    ],
    "server_config": {
      "communication": {"mode": "ETH"},
      "protocol": "mqtt",
      "wifi": {...},
      "ethernet": {...},
      "mqtt_config": {
        "broker_address": "broker.hivemq.com",
        "publish_mode": "default",
        "default_mode": {...},
        "customize_mode": {...}
      },
      "http_config": {...}
    },
    "logging_config": {
      "logging_ret": "1w",
      "logging_interval": "5m"
    }
  }
}
```

### Response Fields

#### Backup Info Object

| Field | Type | Description |
|-------|------|-------------|
| `timestamp` | number | Milliseconds since device boot |
| `firmware_version` | string | Current firmware version |
| `device_name` | string | Gateway device name |
| `total_devices` | number | Total number of configured devices |
| `total_registers` | number | Total number of registers across all devices |
| `processing_time_ms` | number | Time taken to generate backup (ms) |
| `backup_size_bytes` | number | Total JSON size in bytes |
| `device_pagination` | boolean | (v2.5.12+) Whether pagination was used |
| `device_offset` | number | (v2.5.12+) Current device offset |
| `device_limit` | number | (v2.5.12+) Requested device limit |
| `devices_returned` | number | (v2.5.12+) Devices in current response |
| `has_more_devices` | boolean | (v2.5.12+) More devices available |
| `available_sections` | array | (v2.5.12+) List of available sections |

#### Config Object

| Field | Type | Description |
|-------|------|-------------|
| `devices` | array | All Modbus devices with complete register definitions |
| `server_config` | object | Complete server configuration (MQTT, HTTP, WiFi, Ethernet) |
| `logging_config` | object | Logging retention and interval settings |

---

### 📄 Pagination Examples (v2.5.12+)

#### Example 1: Get Metadata First (Recommended Workflow)

Check data size before downloading full backup:

```json
// Request
{"op": "read", "type": "full_config", "section": "metadata"}

// Response
{
  "status": "ok",
  "section": "metadata",
  "backup_info": {
    "total_devices": 10,
    "total_registers": 250,
    "available_sections": ["all", "devices", "server_config", "logging_config", "metadata"]
  },
  "recommendations": {
    "use_pagination": true,
    "suggested_device_limit": 2,
    "estimated_pages": 5
  }
}
```

#### Example 2: Paginated Device Backup

Backup devices 2 at a time:

```json
// Request - Page 1
{"op": "read", "type": "full_config", "section": "devices", "device_offset": 0, "device_limit": 2}

// Response - Page 1
{
  "status": "ok",
  "section": "devices",
  "backup_info": {
    "total_devices": 10,
    "total_registers": 250,
    "device_pagination": true,
    "device_offset": 0,
    "device_limit": 2,
    "devices_returned": 2,
    "has_more_devices": true
  },
  "config": {
    "devices": [/* 2 devices with all registers */]
  }
}

// Request - Page 2
{"op": "read", "type": "full_config", "section": "devices", "device_offset": 2, "device_limit": 2}
// ... continue until has_more_devices = false
```

#### Example 3: Section-Based Backup (Separate Calls)

Get config sections separately for better BLE reliability:

```javascript
// Step 1: Get server_config (small, ~2KB)
await ble.send({op: "read", type: "full_config", section: "server_config"});

// Step 2: Get logging_config (tiny, ~500 bytes)
await ble.send({op: "read", type: "full_config", section: "logging_config"});

// Step 3: Get devices with pagination (largest section)
let offset = 0;
let allDevices = [];
while (true) {
  const resp = await ble.send({
    op: "read", type: "full_config",
    section: "devices", device_offset: offset, device_limit: 2
  });
  allDevices.push(...resp.config.devices);
  if (!resp.backup_info.has_more_devices) break;
  offset += 2;
}
```

---

## Restore Command: `restore_config`

### Description

Import and apply complete gateway configuration from backup file. **This will replace all existing configurations.**

**Operation:** `system`
**Type:** `restore_config`

---

### Request Format

```json
{
  "op": "system",
  "type": "restore_config",
  "config": {
    "devices": [...],
    "server_config": {...},
    "logging_config": {...}
  }
}
```

### Request Fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `op` | string | ✅ Yes | Must be `"system"` |
| `type` | string | ✅ Yes | Must be `"restore_config"` |
| `config` | object | ✅ Yes | Configuration object from backup (exact format from `full_config` response) |

**Important:** The `config` object should be the exact `config` field from a `full_config` backup response.

---

### Response Format

#### Success Response

```json
{
  "status": "ok",
  "restored_configs": [
    "devices.json",
    "server_config.json",
    "logging_config.json"
  ],
  "success_count": 3,
  "fail_count": 0,
  "message": "Configuration restore completed. Device restart recommended.",
  "requires_restart": true
}
```

#### Partial Success Response

```json
{
  "status": "ok",
  "restored_configs": [
    "devices.json",
    "server_config.json"
  ],
  "success_count": 2,
  "fail_count": 1,
  "message": "Configuration restore completed. Device restart recommended.",
  "requires_restart": true
}
```

#### Error Response

```json
{
  "status": "error",
  "error": "Missing 'config' object in restore payload"
}
```

### Response Fields

| Field | Type | Description |
|-------|------|-------------|
| `status` | string | `"ok"` or `"error"` |
| `restored_configs` | array | List of successfully restored configuration files |
| `success_count` | number | Number of configurations successfully restored (max 3) |
| `fail_count` | number | Number of configurations that failed to restore |
| `message` | string | Human-readable summary message |
| `requires_restart` | boolean | `true` - device restart recommended to apply all changes |

---

## Complete Workflow

### Backup → Save → Restore Workflow

```
┌─────────────┐         ┌─────────────┐         ┌─────────────┐
│   Gateway   │   BLE   │  Mobile App │  File   │   Storage   │
└──────┬──────┘ ◄──────►└──────┬──────┘ ◄──────►└──────┬──────┘
       │                       │                        │
       │  1. full_config       │                        │
       │◄──────────────────────┤                        │
       │                       │                        │
       │  2. Complete config   │                        │
       ├──────────────────────►│                        │
       │                       │                        │
       │                       │  3. Save to file       │
       │                       ├───────────────────────►│
       │                       │                        │
       │                       │  4. Load from file     │
       │                       │◄───────────────────────┤
       │                       │                        │
       │  5. restore_config    │                        │
       │◄──────────────────────┤                        │
       │                       │                        │
       │  6. Restore complete  │                        │
       ├──────────────────────►│                        │
       │                       │                        │
       │  7. Restart device    │                        │
       │  (optional)           │                        │
       └───────────────────────┴────────────────────────┘
```

---

## Mobile App Integration

### JavaScript Complete Example

```javascript
class GatewayBackupManager {
  constructor(bleManager) {
    this.bleManager = bleManager;
  }

  // ============================================
  // BACKUP (Download Configuration)
  // ============================================

  async createBackup() {
    console.log('📦 Creating full configuration backup...');

    try {
      // Send backup command
      const command = {
        op: 'read',
        type: 'full_config'
      };

      const response = await this.bleManager.sendCommand(command);

      if (response.status === 'ok') {
        const backup = {
          created_at: new Date().toISOString(),
          backup_info: response.backup_info,
          config: response.config
        };

        // Display backup info
        console.log('✅ Backup created successfully');
        console.log(`   Devices: ${backup.backup_info.total_devices}`);
        console.log(`   Registers: ${backup.backup_info.total_registers}`);
        console.log(`   Size: ${(backup.backup_info.backup_size_bytes / 1024).toFixed(2)} KB`);
        console.log(`   Processing time: ${backup.backup_info.processing_time_ms} ms`);

        return backup;
      } else {
        throw new Error('Backup failed: ' + response.error);
      }
    } catch (error) {
      console.error('❌ Backup error:', error);
      throw error;
    }
  }

  // Save backup to file
  async saveBackupToFile(backup) {
    const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
    const filename = `gateway_backup_${timestamp}.json`;

    // Create downloadable file
    const blob = new Blob([JSON.stringify(backup, null, 2)],
                          {type: 'application/json'});
    const url = URL.createObjectURL(blob);

    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    a.click();

    URL.revokeObjectURL(url);

    console.log(`💾 Backup saved: ${filename}`);
    return filename;
  }

  // ============================================
  // RESTORE (Upload Configuration)
  // ============================================

  async restoreFromFile(file) {
    console.log(`📂 Loading backup from ${file.name}...`);

    try {
      // Read file content
      const fileContent = await file.text();
      const backup = JSON.parse(fileContent);

      // Validate backup structure
      if (!backup.config || !backup.config.devices) {
        throw new Error('Invalid backup file format');
      }

      // Display backup info
      console.log('📋 Backup Information:');
      console.log(`   Created: ${backup.created_at}`);
      console.log(`   Firmware: ${backup.backup_info.firmware_version}`);
      console.log(`   Devices: ${backup.backup_info.total_devices}`);
      console.log(`   Registers: ${backup.backup_info.total_registers}`);

      // Confirm with user
      const confirmed = confirm(
        '⚠️  CONFIGURATION RESTORE WARNING\n\n' +
        'This will REPLACE all current configurations:\n' +
        `• ${backup.backup_info.total_devices} devices\n` +
        `• ${backup.backup_info.total_registers} registers\n` +
        '• Server settings (MQTT, HTTP, WiFi, Ethernet)\n' +
        '• Logging settings\n\n' +
        'Current configuration will be lost!\n\n' +
        'Continue with restore?'
      );

      if (!confirmed) {
        console.log('❌ Restore cancelled by user');
        return false;
      }

      // Send restore command
      const command = {
        op: 'system',
        type: 'restore_config',
        config: backup.config
      };

      console.log('📤 Uploading configuration to gateway...');
      const response = await this.bleManager.sendCommand(command);

      if (response.status === 'ok') {
        console.log('✅ Configuration restored successfully');
        console.log(`   Restored: ${response.restored_configs.join(', ')}`);
        console.log(`   Success: ${response.success_count}, Failed: ${response.fail_count}`);

        if (response.requires_restart) {
          const restart = confirm(
            '✅ Restore Complete!\n\n' +
            'Device restart is recommended to apply all changes.\n\n' +
            'Restart gateway now?'
          );

          if (restart) {
            await this.restartDevice();
          }
        }

        return true;
      } else {
        throw new Error('Restore failed: ' + response.error);
      }
    } catch (error) {
      console.error('❌ Restore error:', error);
      alert('Restore failed: ' + error.message);
      return false;
    }
  }

  async restartDevice() {
    // Optional: Send restart command
    console.log('🔄 Restarting gateway...');
    // Implementation depends on your restart command
  }

  // ============================================
  // BACKUP MANAGEMENT
  // ============================================

  async compareBackups(backup1, backup2) {
    const diff = {
      devices_added: [],
      devices_removed: [],
      devices_modified: [],
      server_config_changed: false,
      logging_config_changed: false
    };

    // Compare devices
    const devices1 = new Set(backup1.config.devices.map(d => d.device_id));
    const devices2 = new Set(backup2.config.devices.map(d => d.device_id));

    for (const id of devices2) {
      if (!devices1.has(id)) diff.devices_added.push(id);
    }

    for (const id of devices1) {
      if (!devices2.has(id)) diff.devices_removed.push(id);
    }

    // Compare server configs
    diff.server_config_changed =
      JSON.stringify(backup1.config.server_config) !==
      JSON.stringify(backup2.config.server_config);

    // Compare logging configs
    diff.logging_config_changed =
      JSON.stringify(backup1.config.logging_config) !==
      JSON.stringify(backup2.config.logging_config);

    return diff;
  }
}

// ============================================
// USAGE EXAMPLES
// ============================================

const backupManager = new GatewayBackupManager(bleManager);

// Example 1: Create and download backup
async function backupGateway() {
  const backup = await backupManager.createBackup();
  await backupManager.saveBackupToFile(backup);
}

// Example 2: Restore from uploaded file
async function restoreGateway(fileInput) {
  const file = fileInput.files[0];
  if (file) {
    await backupManager.restoreFromFile(file);
  }
}

// Example 3: Clone configuration to another gateway
async function cloneConfiguration(fromGateway, toGateway) {
  // Connect to source gateway
  await fromGateway.connect();
  const backup = await backupManager.createBackup();
  await fromGateway.disconnect();

  // Connect to destination gateway
  await toGateway.connect();
  const command = {
    op: 'system',
    type: 'restore_config',
    config: backup.config
  };
  await toGateway.sendCommand(command);
}
```

---

## Error Handling

### Common Errors

#### 1. Missing `config` Object

**Error:**
```json
{
  "status": "error",
  "error": "Missing 'config' object in restore payload"
}
```

**Cause:** Restore command sent without `config` field

**Solution:** Ensure backup file contains `config` object

---

#### 2. Invalid Backup Format

**Error:** JSON parse error when loading file

**Cause:** Corrupted or manually edited backup file

**Solution:** Use only backup files created by `full_config` command

---

#### 3. Partial Restore Failure

**Response:**
```json
{
  "status": "ok",
  "success_count": 2,
  "fail_count": 1,
  "restored_configs": ["devices.json", "server_config.json"]
}
```

**Cause:** One configuration failed validation or write

**Solution:** Check serial logs for detailed error, may need manual fix

---

## Best Practices

### 1. Regular Backup Schedule

```javascript
// Backup every week
setInterval(async () => {
  const backup = await backupManager.createBackup();
  await cloudStorage.upload(`backup_${Date.now()}.json`, backup);
}, 7 * 24 * 60 * 60 * 1000);
```

### 2. Backup Before Critical Operations

```javascript
// ALWAYS backup before factory reset
async function safeFactoryReset() {
  const backup = await backupManager.createBackup();
  await backupManager.saveBackupToFile(backup);

  // Now safe to reset
  await bleManager.factoryReset();
}
```

### 3. Validate Backup After Creation

```javascript
async function validatedBackup() {
  const backup = await backupManager.createBackup();

  // Validate structure
  if (!backup.config.devices || !backup.config.server_config) {
    throw new Error('Incomplete backup!');
  }

  // Validate data
  if (backup.backup_info.total_devices === 0) {
    console.warn('⚠️  Backup contains zero devices!');
  }

  return backup;
}
```

### 4. Version Control

```javascript
class BackupVersionControl {
  async saveVersioned(backup) {
    const version = {
      timestamp: Date.now(),
      firmware: backup.backup_info.firmware_version,
      devices: backup.backup_info.total_devices,
      backup: backup
    };

    // Store in localStorage or cloud
    const versions = this.getVersions();
    versions.push(version);
    localStorage.setItem('gateway_backups', JSON.stringify(versions));
  }

  getVersions() {
    const stored = localStorage.getItem('gateway_backups');
    return stored ? JSON.parse(stored) : [];
  }

  async restoreVersion(timestamp) {
    const versions = this.getVersions();
    const version = versions.find(v => v.timestamp === timestamp);
    if (version) {
      await backupManager.restoreFromBackup(version.backup);
    }
  }
}
```

### 5. Backup Comparison

```javascript
async function showBackupDiff(file1, file2) {
  const backup1 = JSON.parse(await file1.text());
  const backup2 = JSON.parse(await file2.text());

  const diff = await backupManager.compareBackups(backup1, backup2);

  console.log('📊 Backup Comparison:');
  console.log(`   Devices added: ${diff.devices_added.length}`);
  console.log(`   Devices removed: ${diff.devices_removed.length}`);
  console.log(`   Devices modified: ${diff.devices_modified.length}`);
  console.log(`   Server config changed: ${diff.server_config_changed}`);
  console.log(`   Logging config changed: ${diff.logging_config_changed}`);
}
```

---

## Serial Log Examples

### Backup Operation

```
[CRUD] Full config backup requested
[CRUD] Full config backup complete: 5 devices, 50 registers, 102400 bytes, 350 ms
```

### Restore Operation

```
========================================
[CONFIG RESTORE] ⚠️  INITIATED by BLE client
========================================
[CONFIG RESTORE] [1/3] Restoring devices configuration...
[CONFIG RESTORE] Existing devices cleared
[CONFIG RESTORE] Restored 5 devices
[CONFIG RESTORE] [2/3] Restoring server configuration...
[CONFIG RESTORE] Server config restored successfully
[CONFIG RESTORE] [3/3] Restoring logging configuration...
[CONFIG RESTORE] Logging config restored successfully
[CONFIG RESTORE] ========================================
[CONFIG RESTORE] Restore complete: 3 succeeded, 0 failed
[CONFIG RESTORE] ⚠️  Device restart recommended to apply all changes
[CONFIG RESTORE] ========================================
```

---

## Performance Considerations

### Backup Size Estimates

| Configuration | Size | BLE Transfer Time (512 MTU) |
|---------------|------|------------------------------|
| 10 devices, 10 registers each | ~20 KB | ~2-3 seconds |
| 25 devices, 20 registers each | ~50 KB | ~5-7 seconds |
| 50 devices, 50 registers each | ~150 KB | ~15-20 seconds |

### Memory Usage

- **PSRAM Allocation:** ~150KB for large configs
- **BLE Fragmentation:** Handled automatically
- **Processing Time:** ~300-500ms for typical configs

---

## Related Documentation

- **Factory Reset:** `/Documentation/API_Reference/BLE_FACTORY_RESET.md`
- **Device Control:** `/Documentation/API_Reference/BLE_DEVICE_CONTROL.md`
- **CRUD Operations:** `/Documentation/API_Reference/API.md`

---

**Made with ❤️ by SURIOTA R&D Team**
*Empowering Industrial IoT Solutions*
