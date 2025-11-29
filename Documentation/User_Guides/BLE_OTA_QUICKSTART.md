# BLE OTA Quick Start Guide

**5-Minute Guide to Firmware Updates via Bluetooth**

---

## Prerequisites

- SRT-MGATE-1210 device (powered on)
- Signed firmware binary (.bin file)
- Python 3.8+ with `bleak` library

---

## Step 1: Install Tools

```bash
pip install bleak tqdm
```

---

## Step 2: Prepare Firmware

Make sure you have:
1. Compiled firmware binary: `SRT-MGATE-1210_vX.X.X.bin`
2. File should be < 3MB

---

## Step 3: Upload Firmware

### Option A: Interactive Mode

```bash
cd Testing/BLE_Testing/OTA_Test
python ble_ota_upload.py
```

The script will:
1. Scan for devices
2. Ask you to select device
3. Ask for firmware file path
4. Upload and verify automatically

### Option B: Direct Upload

```bash
python ble_ota_upload.py -f /path/to/firmware.bin
```

### Option C: Specify Device

```bash
python ble_ota_upload.py -f firmware.bin -a AA:BB:CC:DD:EE:FF
```

---

## Step 4: Wait for Update

```
Firmware: SRT-MGATE-1210_v2.5.16.bin
Size: 2,005,360 bytes (1958.4 KB)
Calculating SHA256... d5fca63041457f74...

[1/4] Sending OTA_START...
Device ready to receive firmware

[2/4] Uploading firmware...
Upload: 100%|████████████████████| 1.96M/1.96M [01:25<00:00, 23.5KB/s]
Sent 8356 chunks (2,005,360 bytes)

[3/4] Verifying firmware...
Firmware verified successfully!

[4/4] Applying update...
Update applied! Device will reboot...

==================================================
OTA UPDATE SUCCESSFUL!
==================================================
```

---

## Troubleshooting

### Device Not Found

```bash
# Scan for devices
python ble_ota_upload.py --scan
```

Make sure:
- Device is powered on
- BLE is enabled (default)
- You're within 10 meters

### Transfer Failed

```bash
# Abort and retry
python ble_ota_upload.py --abort
python ble_ota_upload.py -f firmware.bin
```

### Check Status

```bash
python ble_ota_upload.py --status
```

---

## BLE UUIDs (for Mobile Developers)

```
Service:  0000FF00-0000-1000-8000-00805F9B34FB
Control:  0000FF01-0000-1000-8000-00805F9B34FB  (Write)
Data:     0000FF02-0000-1000-8000-00805F9B34FB  (Write No Response)
Status:   0000FF03-0000-1000-8000-00805F9B34FB  (Notify)
```

---

## Commands Quick Reference

| Command | Code | Description |
|---------|------|-------------|
| START | 0x50 | Begin OTA with size + SHA256 |
| DATA | 0x51 | Send firmware chunk |
| VERIFY | 0x52 | Verify checksum |
| APPLY | 0x53 | Apply and reboot |
| ABORT | 0x54 | Cancel transfer |
| STATUS | 0x55 | Get progress |

---

## Next Steps

- [Full API Reference](../API_Reference/BLE_OTA_API.md)
- [Android Implementation Guide](../API_Reference/BLE_OTA_API.md#8-android-implementation-guide)
- [iOS Implementation Guide](../API_Reference/BLE_OTA_API.md#9-ios-implementation-guide)

---

**Need Help?** support@suriota.com
