# BLE Testing Tool - Changelog

## Version 1.0.3 (2025-01-16)

### 🎯 Simplified - Auto-Connect Workflow

**User Feedback:** "kenapa ada device number, bisa dibuat sederhana saja gk"

**Changes:**

- ✅ Removed device numbering and selection menu
- ✅ Auto-connects to first SURIOTA GW device found
- ✅ Simplified scan output (just show name and address)
- ✅ Streamlined user flow - scan → connect → test

**Before (Manual Selection):**

```
Found 2 SURIOTA Gateway(s):

1. Name: SURIOTA GW
   Address: AA:BB:CC:DD:EE:FF

2. Name: SURIOTA GW
   Address: 11:22:33:44:55:66

[SELECT] Enter device number to connect (or 0 to quit):
>
```

**After (Auto-Connect):**

```
✓ Found SURIOTA Gateway
  Name: SURIOTA GW
  Address: AA:BB:CC:DD:EE:FF

[CONNECT] Connecting to AA:BB:CC:DD:EE:FF...
```

**Impact:** Faster workflow - tool automatically connects to first device,
perfect for single-device testing scenarios

---

## Version 1.0.2 (2025-01-16)

### 🔧 Fixed - RSSI Attribute Error

**Issue:** `'BLEDevice' object has no attribute 'rssi'` error when scanning
devices

**Root Cause:** bleak library's `BLEDevice` object doesn't provide RSSI
attribute in scan results

**User Requirement:** Only need command and response checking, not signal
strength or network details

**Changes:**

- ✅ Removed RSSI display from device scan output (lines 83, 93)
- ✅ Simplified device listing to show only Name and Address
- ✅ Cleaner, more focused output for device selection

**Before:**

```python
for i, device in enumerate(suriota_devices, 1):
    print(f"{i}. Name: {device.name}")
    print(f"   Address: {device.address}")
    print(f"   RSSI: {device.rssi} dBm")  # ← ERROR
    print()
```

**After:**

```python
for i, device in enumerate(suriota_devices, 1):
    print(f"{i}. Name: {device.name}")
    print(f"   Address: {device.address}")
    print()
```

**Impact:** Tool now runs without errors and displays only essential device
information for connection

---

## Version 1.0.1 (2025-01-16)

### 🔧 Fixed - UUID Configuration

**Issue:** UUIDs in `ble_test.py` didn't match firmware configuration

**Changes:**

- ✅ Updated `SERVICE_UUID` to `00001830-0000-1000-8000-00805f9b34fb`
- ✅ Updated `COMMAND_CHAR_UUID` to `11111111-1111-1111-1111-111111111101`
- ✅ Updated `RESPONSE_CHAR_UUID` to `11111111-1111-1111-1111-111111111102`
- ✅ Added `SERVICE_NAME = "SURIOTA GW"`

**Reference:** Based on working implementation in
`Device_Testing/TCP/create_device_5_registers.py`

---

### 🔧 Fixed - Command Fragmentation

**Issue:** Commands were sent as single packet instead of fragmented chunks

**Changes:**

- ✅ Implemented 18-byte chunk fragmentation
- ✅ Added `<END>` marker after command transmission
- ✅ Added chunk progress logging
- ✅ Added 0.1s delay between chunks for stability

**Before:**

```python
# Single packet transmission
await client.write_gatt_char(COMMAND_CHAR_UUID, command_bytes, response=True)
```

**After:**

```python
# Fragmented transmission (18 bytes/chunk)
chunk_size = 18
for i in range(0, len(command_str), chunk_size):
    chunk = command_str[i:i+chunk_size]
    await client.write_gatt_char(COMMAND_CHAR_UUID, chunk.encode('utf-8'))
    await asyncio.sleep(0.1)

# Send end marker
await client.write_gatt_char(COMMAND_CHAR_UUID, "<END>".encode('utf-8'))
```

---

### 🔧 Enhanced - Device Scanning

**Issue:** Tool scanned all BLE devices without filtering

**Changes:**

- ✅ Filter for `SERVICE_NAME = "SURIOTA GW"` devices first
- ✅ Show all devices if no SURIOTA GW found (fallback)
- ✅ Improved scan timeout from 5s to 10s
- ✅ Better user feedback during scan

**Before:**

```python
devices = await BleakScanner.discover(timeout=5.0)
```

**After:**

```python
devices = await BleakScanner.discover(timeout=10.0)
suriota_devices = [d for d in devices if d.name == SERVICE_NAME]
if suriota_devices:
    return suriota_devices
else:
    return devices  # Fallback to all devices
```

---

### 📝 Updated - Documentation

**Files Updated:**

1. `BLE_TESTING_README.md`
   - Added BLE configuration details (UUIDs, Service Name)
   - Updated troubleshooting section with UUID verification
   - Added note about command fragmentation

2. `BLE_COMMANDS_QUICK_REF.txt`
   - No changes needed (commands remain the same)

---

## Version 1.0.0 (2025-01-16)

### ✨ Initial Release

**Features:**

- Interactive BLE testing tool
- CRUD operations support
- Command examples
- Quick test mode
- Fragmented response handling

**Files Created:**

- `ble_test.py` - Main testing tool
- `requirements.txt` - Python dependencies
- `BLE_TESTING_README.md` - User guide
- `BLE_COMMANDS_QUICK_REF.txt` - Command reference
- `run_ble_test.bat` - Windows launcher

---

## Comparison with Device Testing Tools

### Similarities

✅ Both use same UUIDs ✅ Both use 18-byte fragmentation ✅ Both use `<END>`
marker ✅ Both handle fragmented responses

### Differences

| Feature          | ble_test.py              | create_device_5_registers.py   |
| ---------------- | ------------------------ | ------------------------------ |
| Purpose          | Interactive testing      | Automated device creation      |
| Commands         | All CRUD ops             | Create device + registers only |
| Mode             | Interactive + Quick Test | Automated only                 |
| User Input       | Manual JSON input        | Hardcoded config               |
| Response Display | Pretty print JSON        | Simple JSON dump               |

---

## Migration Notes

### For Users of Previous Version (v1.0.0)

**No action required** - Update is backward compatible.

### For Developers

If you have custom scripts using the old UUIDs, update to:

```python
SERVICE_UUID = "00001830-0000-1000-8000-00805f9b34fb"
COMMAND_CHAR_UUID = "11111111-1111-1111-1111-111111111101"
RESPONSE_CHAR_UUID = "11111111-1111-1111-1111-111111111102"
SERVICE_NAME = "SURIOTA GW"
```

---

## Known Issues

### Issue #1: Response Buffer Overflow for Large Payloads

**Status:** MITIGATED **Description:** Devices with >50 registers in full mode
may exceed 10KB payload limit **Workaround:** Use `minimal: true` parameter to
reduce payload size **Example:**

```json
{ "op": "read", "type": "device", "device_id": "device_1", "minimal": true }
```

### Issue #2: Bluetooth Adapter Compatibility

**Status:** KNOWN **Description:** Some Bluetooth 4.0 adapters may have
connection stability issues **Workaround:** Use Bluetooth 4.2+ adapter or
built-in Bluetooth on modern laptops

---

## Future Enhancements

### Planned for v1.1.0

- [ ] Support for batch operations in quick test mode
- [ ] JSON command history (up arrow to recall)
- [ ] Export test results to JSON/CSV
- [ ] Auto-reconnect on connection loss
- [ ] Configurable chunk size (18/32/64 bytes)

### Planned for v1.2.0

- [ ] GUI interface using tkinter
- [ ] Real-time data streaming visualization
- [ ] Performance benchmarking mode
- [ ] Automated regression testing suite

---

**Last Updated:** 2025-01-16 (v1.0.3) **Author:** Claude Code
