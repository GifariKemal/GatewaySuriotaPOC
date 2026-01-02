# OTA Update Testing Tools

**Version:** 2.0.0 **Last Updated:** December 19, 2025

Tools for testing OTA (Over-The-Air) firmware updates via BLE with real-time
progress monitoring.

## Files

| File            | Description                                            |
| --------------- | ------------------------------------------------------ |
| `ota_update.py` | Full automated OTA update process with v2.0.0 features |

## Requirements

```bash
pip install bleak colorama
```

## What's New in v2.0.0

- **Real-time Progress Notifications** - Receive push notifications from device
  during download
- **Download Speed & ETA** - Live display of transfer speed and estimated time
  remaining
- **Network Mode Indicator** - Shows WiFi or Ethernet connection
- **Retry Count Monitoring** - Track retry attempts during download
- **Interactive Progress Bar** - Color-coded progress visualization
- **New CLI Options** - `--status` and `--monitor` commands

## Usage

### Interactive Menu (Default)

```bash
python ota_update.py
```

**Menu Options:**

```
  1. Check for updates
  2. Set GitHub token (for private repos)
  3. Get OTA configuration
  4. Get OTA status (enhanced v2.0)      <- NEW
  5. Monitor OTA progress (real-time)    <- NEW
  6. Full OTA update
  7. Exit
```

### Command Line Options

```bash
python ota_update.py                    # Interactive menu
python ota_update.py --check            # Check for updates only
python ota_update.py --status           # Get enhanced OTA status
python ota_update.py --monitor          # Monitor progress real-time
python ota_update.py --update           # Full update flow
python ota_update.py --set-token TOKEN  # Set GitHub token
python ota_update.py --auto             # Auto update (no prompts)
```

## Process Flow

```
┌─────────────────┐
│   check_update  │ → Check for new firmware version
└────────┬────────┘
         │ (3s delay)
         ▼
┌─────────────────┐
│  start_update   │ → Download & verify firmware (~60-90s)
│                 │   Real-time progress via push notifications
└────────┬────────┘
         │ (3s delay)
         ▼
┌─────────────────┐
│   User Confirm  │ → Confirm before applying
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  apply_update   │ → Apply firmware & reboot
└─────────────────┘
```

## Real-Time Progress Display

During download, you'll see a live progress bar with detailed stats:

```
  📥 Download Progress:

  [██████████████░░░░░░░░░░░░░░░░]  45% | 707.8 KB/1.5 MB | ⚡28.5 KB/s | ⏱️ 35s | 📡WiFi
```

**Progress Bar Elements:** | Element | Description | |---------|-------------| |
`[████░░░░]` | Visual progress bar | | `45%` | Download percentage | |
`707.8 KB/1.5 MB` | Bytes downloaded / Total size | | `⚡28.5 KB/s` | Current
download speed | | `⏱️ 35s` | Estimated time remaining | | `📡WiFi` | Network
mode (WiFi/Ethernet) | | `[Retry 1/3]` | Retry count (if retrying) |

## Enhanced OTA Status Response

The `--status` command shows comprehensive OTA information:

```
  ┌────────────────────────────────────────────────────────────────┐
  │ 📊 OTA Status - DOWNLOADING                                    │
  ├────────────────────────────────────────────────────────────────┤
  │  State: DOWNLOADING (2)                                        │
  │  Progress: 45%                                                 │
  │  Downloaded: 707.8 KB / 1.50 MB                               │
  │  Speed: 28.5 KB/s                                             │
  │  ETA: 35s                                                     │
  │  Network: WiFi                                                │
  │  Retries: 0 / 3                                               │
  │  Current Version: 2.5.36                                      │
  │  Target Version: 2.5.37                                       │
  │  Update Available: Yes                                        │
  └────────────────────────────────────────────────────────────────┘
```

## Push Notifications

The tool automatically handles OTA progress push notifications from the device:

```json
{
  "type": "ota_progress",
  "state": "DOWNLOADING",
  "progress": 45,
  "bytes_downloaded": 707788,
  "total_bytes": 1572864,
  "bytes_per_second": 28500,
  "eta_seconds": 35,
  "network_mode": "WiFi"
}
```

Notifications are sent every 5% progress (0%, 5%, 10%, ..., 100%).

## Example Output

```
══════════════════════════════════════════════════════════════════════
║                                                                    ║
║  🚀  OTA FIRMWARE UPDATE TOOL  -  SRT-MGATE-1210                   ║
║                                                                    ║
══════════════════════════════════════════════════════════════════════
  Started: 2025-12-19 10:30:45

──────────────────────────────────────────────────────────────────────
⏳ [Step 1/4] Checking for firmware updates
──────────────────────────────────────────────────────────────────────

  📤 Sending: {"op":"ota","type":"check_update"}
  📥 Receiving: .............

  ✅ Update available: 2.5.36 → 2.5.37

  ┌────────────────────────────────────────────────────────────────┐
  │ 📦 Update Information                                          │
  ├────────────────────────────────────────────────────────────────┤
  │  Current Version: 2.5.36                                       │
  │  Target Version: 2.5.37                                        │
  │  Firmware Size: 1,572,864 bytes                               │
  │  Mandatory: No                                                 │
  └────────────────────────────────────────────────────────────────┘

──────────────────────────────────────────────────────────────────────
⏳ [Step 2/4] Downloading firmware from GitHub
──────────────────────────────────────────────────────────────────────

  ℹ️  Real-time progress will be displayed via push notifications...

  📥 Download Progress:

  [██████████████████████████████]  100% | 1.5 MB/1.5 MB | ⚡32.1 KB/s | ⏱️ 0s | 📡WiFi

  ✅ Firmware downloaded and verified in 62.3 seconds!

  ┌────────────────────────────────────────────────────────────────┐
  │ ✅ Download Complete                                           │
  ├────────────────────────────────────────────────────────────────┤
  │  Download Time: 62.3 seconds                                   │
  │  Total Size: 1,572,864 bytes (1.50 MB)                        │
  │  Average Speed: 24.6 KB/s                                     │
  │  Network Mode: WiFi                                           │
  │  Status: Success                                              │
  └────────────────────────────────────────────────────────────────┘

...
```

## State Reference

| Value | State Name    | Description                            |
| ----- | ------------- | -------------------------------------- |
| 0     | `IDLE`        | No OTA in progress, ready for commands |
| 1     | `CHECKING`    | Checking GitHub for updates            |
| 2     | `DOWNLOADING` | Downloading firmware via HTTPS         |
| 3     | `VALIDATING`  | Verifying signature and checksum       |
| 4     | `APPLYING`    | Writing firmware to flash              |
| 5     | `REBOOTING`   | About to reboot with new firmware      |
| 6     | `ERROR`       | Error occurred (check error_code)      |

## Notes

- Device will **reboot** after applying update
- Process takes approximately 1-3 minutes depending on network speed
- WiFi/Ethernet must be connected on device for download
- BLE connection will be lost when device reboots
- Push notifications require firmware v2.5.37+
- The tool supports both new (MGate-1210) and legacy (SURIOTA) device names

## Troubleshooting

| Issue                 | Solution                                        |
| --------------------- | ----------------------------------------------- |
| No device found       | Ensure device is powered on and BLE is enabled  |
| Connection timeout    | Move closer to device, check for interference   |
| Download slow         | Check WiFi/Ethernet signal strength on device   |
| Retries failing       | Check network connectivity, GitHub availability |
| No push notifications | Verify firmware is v2.5.37 or later             |

## Related Documentation

- [BLE OTA Update API](../../../Documentation/API_Reference/BLE_OTA_UPDATE.md)
- [OTA Technical Guide](../../../Documentation/Technical_Guides/OTA_FIRMWARE_GUIDE.md)
