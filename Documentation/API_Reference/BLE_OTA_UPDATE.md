# BLE OTA Update API Reference

**Version:** 2.4.0
**Last Updated:** November 26, 2025

---

## Overview

The OTA (Over-The-Air) Update API allows firmware updates via BLE CRUD commands. The system supports two update transports:

1. **HTTPS OTA** - Download firmware from GitHub via WiFi/Ethernet
2. **BLE OTA** - Transfer firmware directly via BLE from smartphone app

All OTA commands use the operation type `"op": "ota"`.

---

## Quick Reference

| Command          | Type               | Description                           |
| ---------------- | ------------------ | ------------------------------------- |
| Check Update     | `check_update`     | Query GitHub for new firmware version |
| Start Update     | `start_update`     | Begin HTTPS firmware download         |
| Get Status       | `ota_status`       | Get current OTA state and progress    |
| Abort Update     | `abort_update`     | Cancel ongoing update                 |
| Apply Update     | `apply_update`     | Install firmware and reboot           |
| Enable BLE OTA   | `enable_ble_ota`   | Activate BLE transfer mode            |
| Disable BLE OTA  | `disable_ble_ota`  | Deactivate BLE transfer mode          |
| Rollback         | `rollback`         | Revert to previous/factory firmware   |
| Get Config       | `get_config`       | View OTA configuration                |
| Set GitHub Repo  | `set_github_repo`  | Configure firmware source             |
| Set GitHub Token | `set_github_token` | Set token for private repos           |

---

## 1. Check for Updates

Check if a new firmware version is available on GitHub.

### Request

```json
{
  "op": "ota",
  "type": "check_update"
}
```

### Response (Update Available)

```json
{
  "status": "ok",
  "command": "check_update",
  "update_available": true,
  "current_version": "2.4.0",
  "target_version": "2.5.0",
  "mandatory": false,
  "manifest": {
    "version": "2.5.0",
    "size": 1572864,
    "release_notes": "Bug fixes and improvements"
  }
}
```

### Response (No Update)

```json
{
  "status": "ok",
  "command": "check_update",
  "update_available": false,
  "current_version": "2.4.0"
}
```

### Notes

- Requires active WiFi or Ethernet connection
- May take 2-5 seconds for HTTPS request to GitHub
- Compares semantic versions (e.g., 2.4.0 < 2.5.0)

---

## 2. Start HTTPS Update

Begin downloading firmware from GitHub or custom URL.

### Request (GitHub)

```json
{
  "op": "ota",
  "type": "start_update"
}
```

### Request (Custom URL)

```json
{
  "op": "ota",
  "type": "start_update",
  "url": "https://example.com/firmware/v2.5.0/firmware.bin"
}
```

### Response (Success)

```json
{
  "status": "ok",
  "command": "start_update",
  "message": "OTA update started. Monitor progress with ota_status command.",
  "update_mode": "https"
}
```

### Response (Error)

```json
{
  "status": "error",
  "command": "start_update",
  "error_code": 3,
  "error_message": "Network not available"
}
```

### Notes

- First call `check_update` to verify update exists
- Download runs in background FreeRTOS task
- Use `ota_status` to monitor progress
- Download can be cancelled with `abort_update`

---

## 3. Get OTA Status

Get current OTA state, progress, and version information.

### Request

```json
{
  "op": "ota",
  "type": "ota_status"
}
```

### Response

```json
{
  "status": "ok",
  "command": "ota_status",
  "state": "downloading",
  "progress": 45,
  "bytes_downloaded": 707788,
  "total_bytes": 1572864,
  "current_version": "2.4.0",
  "target_version": "2.5.0",
  "update_available": true
}
```

### State Values

| State         | Description                            |
| ------------- | -------------------------------------- |
| `idle`        | No OTA in progress, ready for commands |
| `checking`    | Checking GitHub for updates            |
| `downloading` | Downloading firmware via HTTPS         |
| `validating`  | Verifying signature and checksum       |
| `applying`    | Writing firmware to flash              |
| `rebooting`   | About to reboot with new firmware      |
| `error`       | Error occurred (check error_code)      |

### Error Response (when state = error)

```json
{
  "status": "ok",
  "command": "ota_status",
  "state": "error",
  "progress": 0,
  "bytes_downloaded": 0,
  "total_bytes": 0,
  "current_version": "2.4.0",
  "target_version": "",
  "update_available": false,
  "error_code": 5,
  "error_message": "Signature verification failed"
}
```

---

## 4. Abort Update

Cancel an ongoing OTA update.

### Request

```json
{
  "op": "ota",
  "type": "abort_update"
}
```

### Response

```json
{
  "status": "ok",
  "command": "abort_update",
  "message": "OTA update aborted"
}
```

### Notes

- Safe to call at any state
- Cleans up partial download
- Returns to `idle` state

---

## 5. Apply Update

Install downloaded firmware and reboot device.

### Request

```json
{
  "op": "ota",
  "type": "apply_update"
}
```

### Response

```json
{
  "status": "ok",
  "command": "apply_update",
  "message": "Applying update and rebooting..."
}
```

### Error Response

```json
{
  "status": "error",
  "error_message": "No validated firmware ready to apply"
}
```

### Notes

- Only works after successful download and validation
- Device reboots ~500ms after response
- BLE connection will be lost during reboot
- New firmware boots automatically

---

## 6. Enable BLE OTA Mode

Activate BLE firmware transfer service for direct smartphone transfer.

### Request

```json
{
  "op": "ota",
  "type": "enable_ble_ota"
}
```

### Response

```json
{
  "status": "ok",
  "command": "enable_ble_ota",
  "message": "BLE OTA mode enabled. Use OTA BLE service to transfer firmware.",
  "ble_ota_service_uuid": "0000FF00-0000-1000-8000-00805F9B34FB"
}
```

### BLE OTA Service Characteristics

| Characteristic | UUID           | Properties        | Description              |
| -------------- | -------------- | ----------------- | ------------------------ |
| Control        | `0000FF01-...` | Write             | Send OTA commands        |
| Data           | `0000FF02-...` | Write No Response | Send firmware chunks     |
| Status         | `0000FF03-...` | Notify            | Receive progress updates |

### BLE OTA Commands

| Command    | Code | Payload                                  |
| ---------- | ---- | ---------------------------------------- |
| OTA_START  | 0x50 | totalSize(4) + chunkSize(2) + sha256(32) |
| OTA_DATA   | 0x51 | seqNum(2) + data(up to 244 bytes)        |
| OTA_VERIFY | 0x52 | None                                     |
| OTA_APPLY  | 0x53 | None                                     |
| OTA_ABORT  | 0x54 | None                                     |
| OTA_STATUS | 0x55 | None                                     |

### Notes

- Use when no WiFi/Ethernet available
- Maximum chunk size: 244 bytes (MTU-safe)
- Transfer speed: ~20-30 KB/s typical
- Timeout: 5 minutes for complete transfer

---

## 7. Disable BLE OTA Mode

Deactivate BLE firmware transfer service.

### Request

```json
{
  "op": "ota",
  "type": "disable_ble_ota"
}
```

### Response

```json
{
  "status": "ok",
  "command": "disable_ble_ota",
  "message": "BLE OTA mode disabled"
}
```

---

## 8. Rollback Firmware

Revert to previous or factory firmware version.

### Request (Previous Version)

```json
{
  "op": "ota",
  "type": "rollback"
}
```

### Request (Factory Version)

```json
{
  "op": "ota",
  "type": "rollback",
  "target": "factory"
}
```

### Response (Success)

```json
{
  "status": "ok",
  "command": "rollback",
  "target": "previous",
  "message": "Rollback successful. Device will reboot shortly."
}
```

### Response (Error)

```json
{
  "status": "error",
  "command": "rollback",
  "target": "previous",
  "error_message": "No previous firmware available"
}
```

### Notes

- Device reboots ~1 second after response
- `previous` = last working firmware in OTA partition
- `factory` = original firmware from flash programming
- Automatic rollback occurs after 3 consecutive boot failures

---

## 9. Get OTA Configuration

View current OTA settings.

### Request

```json
{
  "op": "ota",
  "type": "get_config"
}
```

### Response

```json
{
  "status": "ok",
  "command": "get_config",
  "config": {
    "enabled": true,
    "auto_update": false,
    "github_owner": "GifariKemal",
    "github_repo": "GatewaySuriotaPOC",
    "github_branch": "main",
    "use_releases": true,
    "check_interval_hours": 24,
    "verify_signature": true,
    "allow_downgrade": false,
    "ble_ota_enabled": true
  }
}
```

### Configuration Fields

| Field                  | Type   | Description                  |
| ---------------------- | ------ | ---------------------------- |
| `enabled`              | bool   | OTA system enabled           |
| `auto_update`          | bool   | Auto-install updates         |
| `github_owner`         | string | GitHub username/organization |
| `github_repo`          | string | Repository name              |
| `github_branch`        | string | Branch for raw file access   |
| `use_releases`         | bool   | Use GitHub Releases API      |
| `check_interval_hours` | int    | Auto-check interval          |
| `verify_signature`     | bool   | Require ECDSA signature      |
| `allow_downgrade`      | bool   | Allow older versions         |
| `ble_ota_enabled`      | bool   | BLE transfer allowed         |

---

## 10. Set GitHub Repository

Configure the firmware source repository.

### Request

```json
{
  "op": "ota",
  "type": "set_github_repo",
  "owner": "GifariKemal",
  "repo": "GatewaySuriotaPOC",
  "branch": "main"
}
```

### Response

```json
{
  "status": "ok",
  "command": "set_github_repo",
  "message": "GitHub repository configured",
  "owner": "GifariKemal",
  "repo": "GatewaySuriotaPOC",
  "branch": "main"
}
```

### Notes

- `branch` is optional, defaults to "main"
- Settings saved to `/ota_config.json`
- Takes effect immediately for next `check_update`

---

## 11. Set GitHub Token

Configure authentication for private repositories.

### Request

```json
{
  "op": "ota",
  "type": "set_github_token",
  "token": "ghp_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
}
```

### Response

```json
{
  "status": "ok",
  "command": "set_github_token",
  "message": "GitHub token configured",
  "token_length": 40
}
```

### Notes

- Required for private repositories
- Token stored securely in NVS (not in JSON file)
- Use GitHub Personal Access Token with `repo` scope
- Token is NOT included in `get_config` response for security

---

## Error Codes

| Code | Name              | Description                         |
| ---- | ----------------- | ----------------------------------- |
| 0    | NONE              | No error                            |
| 1    | NETWORK_ERROR     | WiFi/Ethernet not connected         |
| 2    | HTTP_ERROR        | HTTPS request failed                |
| 3    | DOWNLOAD_FAILED   | Firmware download incomplete        |
| 4    | CHECKSUM_MISMATCH | SHA-256 verification failed         |
| 5    | SIGNATURE_INVALID | ECDSA signature verification failed |
| 6    | VERSION_INVALID   | Version downgrade not allowed       |
| 7    | PARTITION_ERROR   | Flash partition error               |
| 8    | WRITE_ERROR       | Flash write failed                  |
| 9    | TIMEOUT           | Operation timed out                 |
| 10   | ABORTED           | User cancelled operation            |
| 11   | MANIFEST_ERROR    | Invalid manifest file               |
| 12   | SIZE_MISMATCH     | Firmware size doesn't match         |
| 13   | BOOT_FAILED       | New firmware failed to boot         |

---

## Typical Update Flow

### HTTPS Update (via WiFi/Ethernet)

```
App                          Gateway                      GitHub
 |                              |                            |
 |--check_update--------------->|                            |
 |                              |--GET manifest------------->|
 |                              |<--manifest.json------------|
 |<--update_available:true------|                            |
 |                              |                            |
 |--start_update--------------->|                            |
 |<--started--------------------|                            |
 |                              |--GET firmware.bin--------->|
 |                              |<--firmware data------------|
 |--ota_status----------------->|                            |
 |<--progress:45%---------------|                            |
 |                              |                            |
 |--ota_status----------------->|                            |
 |<--progress:100%,validated----|                            |
 |                              |                            |
 |--apply_update--------------->|                            |
 |<--rebooting------------------|                            |
 |                              |                            |
 |         [Device Reboots with New Firmware]                |
 |                              |                            |
 |--ota_status----------------->|                            |
 |<--idle,version:2.5.0---------|                            |
```

### BLE Update (Direct Transfer)

```
App                          Gateway
 |                              |
 |--enable_ble_ota------------->|
 |<--ble_service_uuid-----------|
 |                              |
 |--[Connect to OTA Service]--->|
 |--OTA_START(size,checksum)--->|
 |<--ACK------------------------|
 |                              |
 |--OTA_DATA(seq=0,chunk)------>|
 |--OTA_DATA(seq=1,chunk)------>|
 |--OTA_DATA(seq=2,chunk)------>|
 |          ...                 |
 |<--Progress Notification------|
 |          ...                 |
 |--OTA_DATA(seq=N,chunk)------>|
 |                              |
 |--OTA_VERIFY----------------->|
 |<--Verification Result--------|
 |                              |
 |--OTA_APPLY------------------>|
 |<--Rebooting------------------|
 |                              |
 |    [Device Reboots]          |
```

---

## Python Example (BLE CRUD)

```python
import asyncio
from bleak import BleakClient

DEVICE_ADDRESS = "AA:BB:CC:DD:EE:FF"
CRUD_CHAR_UUID = "your-crud-characteristic-uuid"

async def check_ota_update():
    async with BleakClient(DEVICE_ADDRESS) as client:
        # Check for update
        command = '{"op":"ota","type":"check_update"}'
        await client.write_gatt_char(CRUD_CHAR_UUID, command.encode())

        # Read response
        response = await client.read_gatt_char(CRUD_CHAR_UUID)
        print(f"Response: {response.decode()}")

async def start_ota_update():
    async with BleakClient(DEVICE_ADDRESS) as client:
        # Start update
        command = '{"op":"ota","type":"start_update"}'
        await client.write_gatt_char(CRUD_CHAR_UUID, command.encode())

        # Poll status
        while True:
            status_cmd = '{"op":"ota","type":"ota_status"}'
            await client.write_gatt_char(CRUD_CHAR_UUID, status_cmd.encode())
            response = await client.read_gatt_char(CRUD_CHAR_UUID)
            data = json.loads(response.decode())

            print(f"Progress: {data.get('progress', 0)}%")

            if data.get('state') == 'idle' and data.get('progress') == 100:
                print("Download complete, applying update...")
                apply_cmd = '{"op":"ota","type":"apply_update"}'
                await client.write_gatt_char(CRUD_CHAR_UUID, apply_cmd.encode())
                break
            elif data.get('state') == 'error':
                print(f"Error: {data.get('error_message')}")
                break

            await asyncio.sleep(2)

# Run
asyncio.run(check_ota_update())
```

---

## Security Considerations

1. **HTTPS Only** - All GitHub communication uses TLS 1.2+
2. **Signature Verification** - ECDSA P-256 signature required by default
3. **Checksum Validation** - SHA-256 hash verified before installation
4. **Anti-Rollback** - Version downgrade blocked unless explicitly allowed
5. **Boot Validation** - Automatic rollback after 3 failed boots
6. **Token Security** - GitHub tokens stored in NVS, not JSON files

---

## Related Documentation

- [OTA Technical Guide](../Technical_Guides/OTA_FIRMWARE_GUIDE.md) - Step-by-step firmware preparation
- [OTA_UPDATE.md](../Technical_Guides/OTA_UPDATE.md) - System architecture
- [API.md](API.md) - Main BLE CRUD API reference

---

**End of BLE OTA Update API Reference**
