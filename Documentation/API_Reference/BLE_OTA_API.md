# BLE OTA API Reference

**Version:** 1.0.0 | **Updated:** November 29, 2025 | **Device:** SRT-MGATE-1210

---

## Table of Contents

1. [Overview](#1-overview)
2. [Quick Start](#2-quick-start)
3. [BLE Service & Characteristics](#3-ble-service--characteristics)
4. [Commands](#4-commands)
5. [Response Format](#5-response-format)
6. [Protocol Flow](#6-protocol-flow)
7. [Error Handling](#7-error-handling)
8. [Android Implementation Guide](#8-android-implementation-guide)
9. [iOS Implementation Guide](#9-ios-implementation-guide)
10. [Security Considerations](#10-security-considerations)
11. [Troubleshooting](#11-troubleshooting)

---

## 1. Overview

BLE OTA allows firmware updates via Bluetooth Low Energy without WiFi/Internet connectivity.

### Use Cases

| Scenario | Description |
|----------|-------------|
| **Offline Update** | Update devices in locations without internet |
| **Emergency Recovery** | Restore devices when HTTPS certificates expire |
| **Field Service** | Technicians update devices on-site |
| **Development** | Quick firmware iteration during development |

### Specifications

| Parameter | Value |
|-----------|-------|
| **BLE Version** | 4.2+ / 5.0 |
| **MTU Size** | 247-512 bytes (negotiated) |
| **Chunk Size** | 240 bytes (default) |
| **Max Firmware Size** | 3 MB |
| **Transfer Speed** | ~15-25 KB/s |
| **Estimated Time (2MB)** | 80-130 seconds |

---

## 2. Quick Start

### Prerequisites

1. Mobile device with BLE support
2. SRT-MGATE-1210 device powered on
3. Signed firmware binary (.bin file)
4. SHA256 hash of firmware

### Basic Flow

```
1. Scan & Connect to device
2. Discover OTA Service (UUID: 0000FF00-...)
3. Subscribe to Status characteristic for notifications
4. Send OTA_START with firmware size and SHA256
5. Send firmware in 240-byte chunks
6. Send OTA_VERIFY to check integrity
7. Send OTA_APPLY to reboot with new firmware
```

### Python Example

```bash
# Install dependencies
pip install bleak tqdm

# Upload firmware
python ble_ota_upload.py -f SRT-MGATE-1210_v2.5.16.bin
```

---

## 3. BLE Service & Characteristics

### OTA Service

| Property | Value |
|----------|-------|
| **Service UUID** | `0000FF00-0000-1000-8000-00805F9B34FB` |
| **Service Name** | SRT-MGATE OTA |

### Characteristics

| Characteristic | UUID | Properties | Description |
|----------------|------|------------|-------------|
| **Control** | `0000FF01-...` | Write | Send commands (START, VERIFY, APPLY, ABORT, STATUS) |
| **Data** | `0000FF02-...` | Write No Response | Send firmware chunks (high speed) |
| **Status** | `0000FF03-...` | Read, Notify | Receive progress and status updates |

### UUID Constants

```java
// Android/Java
public static final UUID OTA_SERVICE_UUID =
    UUID.fromString("0000FF00-0000-1000-8000-00805F9B34FB");
public static final UUID OTA_CONTROL_UUID =
    UUID.fromString("0000FF01-0000-1000-8000-00805F9B34FB");
public static final UUID OTA_DATA_UUID =
    UUID.fromString("0000FF02-0000-1000-8000-00805F9B34FB");
public static final UUID OTA_STATUS_UUID =
    UUID.fromString("0000FF03-0000-1000-8000-00805F9B34FB");
```

```swift
// iOS/Swift
let OTA_SERVICE_UUID = CBUUID(string: "0000FF00-0000-1000-8000-00805F9B34FB")
let OTA_CONTROL_UUID = CBUUID(string: "0000FF01-0000-1000-8000-00805F9B34FB")
let OTA_DATA_UUID = CBUUID(string: "0000FF02-0000-1000-8000-00805F9B34FB")
let OTA_STATUS_UUID = CBUUID(string: "0000FF03-0000-1000-8000-00805F9B34FB")
```

---

## 4. Commands

### 4.1 OTA_START (0x50)

Initialize OTA transfer.

**Request Format:**

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 1 | Command | `0x50` |
| 1 | 4 | Size | Total firmware size (little-endian) |
| 5 | 2 | ChunkSize | Expected chunk size (default: 240) |
| 7 | 32 | SHA256 | Firmware SHA256 hash |

**Total Length:** 39 bytes

**Example (Python):**

```python
import struct
import hashlib

firmware_size = 2005360
chunk_size = 240
sha256_hash = hashlib.sha256(firmware_data).digest()

cmd = struct.pack('<BIH', 0x50, firmware_size, chunk_size)
cmd += sha256_hash  # 32 bytes
# Total: 39 bytes
```

**Example (Java/Android):**

```java
ByteBuffer buffer = ByteBuffer.allocate(39);
buffer.order(ByteOrder.LITTLE_ENDIAN);
buffer.put((byte) 0x50);           // Command
buffer.putInt(firmwareSize);        // Size
buffer.putShort((short) 240);       // Chunk size
buffer.put(sha256Hash);             // 32 bytes
byte[] startCmd = buffer.array();
```

**Response:** Status notification with state = RECEIVING (1) if accepted.

---

### 4.2 OTA_DATA (0x51)

Send firmware chunk.

**Request Format:**

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 1 | Command | `0x51` |
| 1 | 2 | SeqNum | Chunk sequence number (0, 1, 2, ...) |
| 3 | N | Data | Firmware chunk data (max 240 bytes) |

**Total Length:** 3 + N bytes (max 243)

**Example (Python):**

```python
seq_num = 0
chunk = firmware_data[offset:offset+240]

cmd = struct.pack('<BH', 0x51, seq_num)
cmd += chunk
```

**Notes:**
- Use **Write Without Response** for speed
- Device sends status notification every 20 chunks
- Add small delay (10ms) every 10 chunks to prevent buffer overflow

---

### 4.3 OTA_VERIFY (0x52)

Verify received firmware.

**Request Format:**

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 1 | Command | `0x52` |
| 1 | 32 | SHA256 | Expected SHA256 hash (optional if sent in START) |

**Total Length:** 1 or 33 bytes

**Response:**
- Success: State = COMPLETE (4)
- Failure: State = ERROR (5), ErrorCode = CHECKSUM_FAIL (0x05)

---

### 4.4 OTA_APPLY (0x53)

Apply update and reboot.

**Request Format:**

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 1 | Command | `0x53` |

**Total Length:** 1 byte

**Behavior:**
1. Device finalizes OTA partition
2. Sets boot partition to new firmware
3. Sends success response
4. Reboots after 1 second

**Warning:** Device will disconnect after reboot!

---

### 4.5 OTA_ABORT (0x54)

Cancel ongoing transfer.

**Request Format:**

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 1 | Command | `0x54` |

**Behavior:**
- Aborts OTA write operation
- Resets transfer state to IDLE
- No reboot

---

### 4.6 OTA_STATUS (0x55)

Request current status.

**Request Format:**

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 1 | Command | `0x55` |

**Response:** Status notification with current progress.

---

## 5. Response Format

Status notifications are sent via the **Status Characteristic** (0000FF03-...).

### Status Notification Structure

| Offset | Size | Field | Type | Description |
|--------|------|-------|------|-------------|
| 0 | 1 | State | uint8 | Current OTA state |
| 1 | 1 | Progress | uint8 | Progress percentage (0-100) |
| 2 | 4 | BytesReceived | uint32 | Bytes received so far |
| 6 | 4 | TotalBytes | uint32 | Total expected bytes |
| 10 | 1 | ErrorCode | uint8 | Error code (0 = OK) |

**Total Length:** 11 bytes

### State Values

| Value | Name | Description |
|-------|------|-------------|
| 0 | IDLE | Ready for OTA |
| 1 | RECEIVING | Receiving firmware chunks |
| 2 | VERIFYING | Verifying firmware integrity |
| 3 | APPLYING | Writing to boot partition |
| 4 | COMPLETE | Ready to apply/reboot |
| 5 | ERROR | Error occurred |

### Error Codes

| Value | Name | Description |
|-------|------|-------------|
| 0x00 | OK | Success |
| 0x01 | ERROR | General error |
| 0x02 | BUSY | Transfer already in progress |
| 0x03 | INVALID_STATE | Command not valid in current state |
| 0x04 | SIZE_ERROR | Invalid firmware size |
| 0x05 | CHECKSUM_FAIL | SHA256 verification failed |
| 0x06 | FLASH_ERROR | Flash write/erase error |

### Parsing Example (Java)

```java
public void onCharacteristicChanged(BluetoothGatt gatt,
                                     BluetoothGattCharacteristic characteristic) {
    byte[] data = characteristic.getValue();
    if (data.length >= 11) {
        int state = data[0] & 0xFF;
        int progress = data[1] & 0xFF;
        long bytesReceived = ByteBuffer.wrap(data, 2, 4)
            .order(ByteOrder.LITTLE_ENDIAN).getInt() & 0xFFFFFFFFL;
        long totalBytes = ByteBuffer.wrap(data, 6, 4)
            .order(ByteOrder.LITTLE_ENDIAN).getInt() & 0xFFFFFFFFL;
        int errorCode = data[10] & 0xFF;

        updateUI(state, progress, bytesReceived, totalBytes, errorCode);
    }
}
```

---

## 6. Protocol Flow

### Successful Update Sequence

```
┌─────────────┐                           ┌─────────────┐
│  Mobile App │                           │   Device    │
└──────┬──────┘                           └──────┬──────┘
       │                                         │
       │  ──── BLE Connect ─────────────────────►│
       │                                         │
       │  ──── Discover Services ───────────────►│
       │  ◄──── OTA Service Found ───────────────│
       │                                         │
       │  ──── Subscribe to Status ─────────────►│
       │  ◄──── Subscription Confirmed ──────────│
       │                                         │
       │  ════════════════════════════════════════
       │  ║          OTA UPDATE START           ║
       │  ════════════════════════════════════════
       │                                         │
       │  ──── OTA_START (size, sha256) ────────►│
       │  ◄──── Status: RECEIVING ───────────────│
       │                                         │
       │  ──── OTA_DATA (seq=0, chunk) ─────────►│
       │  ──── OTA_DATA (seq=1, chunk) ─────────►│
       │  ──── OTA_DATA (seq=2, chunk) ─────────►│
       │       ... (repeat for all chunks) ...   │
       │  ──── OTA_DATA (seq=N, chunk) ─────────►│
       │  ◄──── Status: Progress 100% ───────────│
       │                                         │
       │  ──── OTA_VERIFY (sha256) ─────────────►│
       │  ◄──── Status: COMPLETE ────────────────│
       │                                         │
       │  ──── OTA_APPLY ───────────────────────►│
       │  ◄──── Status: APPLYING ────────────────│
       │                                         │
       │  ◄──── Disconnect (device reboots) ─────│
       │                                         │
       ▼                                         ▼
```

### Error Recovery Sequence

```
       │  ──── OTA_DATA (seq=50, chunk) ────────►│
       │  ◄──── Status: ERROR, FLASH_ERROR ──────│
       │                                         │
       │  ──── OTA_ABORT ───────────────────────►│
       │  ◄──── Status: IDLE ────────────────────│
       │                                         │
       │  (Retry from OTA_START)                 │
```

---

## 7. Error Handling

### Common Errors

| Error | Cause | Solution |
|-------|-------|----------|
| BUSY | Previous transfer not completed | Send OTA_ABORT first |
| SIZE_ERROR | Firmware > 3MB or size = 0 | Check file size |
| CHECKSUM_FAIL | Data corrupted during transfer | Retry transfer |
| FLASH_ERROR | Flash write failed | Check device storage |
| Timeout | No data received for 30s | Retry or check connection |

### Retry Strategy

```python
MAX_RETRIES = 3
retry_count = 0

while retry_count < MAX_RETRIES:
    try:
        success = await upload_firmware(firmware_path)
        if success:
            break
    except Exception as e:
        retry_count += 1
        await uploader.abort_transfer()
        await asyncio.sleep(2)  # Wait before retry
```

### Connection Lost During Transfer

1. Device automatically aborts transfer after 30s timeout
2. Reconnect to device
3. Send OTA_ABORT to reset state
4. Retry from OTA_START

---

## 8. Android Implementation Guide

### Required Permissions

```xml
<uses-permission android:name="android.permission.BLUETOOTH" />
<uses-permission android:name="android.permission.BLUETOOTH_ADMIN" />
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
<uses-permission android:name="android.permission.BLUETOOTH_SCAN" />
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />
```

### Sample Implementation

```java
public class BleOtaManager {
    private BluetoothGatt gatt;
    private BluetoothGattCharacteristic controlChar;
    private BluetoothGattCharacteristic dataChar;
    private BluetoothGattCharacteristic statusChar;

    // Connect and discover services
    public void connect(BluetoothDevice device) {
        gatt = device.connectGatt(context, false, gattCallback);
    }

    private BluetoothGattCallback gattCallback = new BluetoothGattCallback() {
        @Override
        public void onServicesDiscovered(BluetoothGatt gatt, int status) {
            BluetoothGattService otaService = gatt.getService(OTA_SERVICE_UUID);
            if (otaService != null) {
                controlChar = otaService.getCharacteristic(OTA_CONTROL_UUID);
                dataChar = otaService.getCharacteristic(OTA_DATA_UUID);
                statusChar = otaService.getCharacteristic(OTA_STATUS_UUID);

                // Enable notifications
                gatt.setCharacteristicNotification(statusChar, true);
                BluetoothGattDescriptor desc = statusChar.getDescriptor(CCCD_UUID);
                desc.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
                gatt.writeDescriptor(desc);
            }
        }

        @Override
        public void onCharacteristicChanged(BluetoothGatt gatt,
                                           BluetoothGattCharacteristic characteristic) {
            parseStatusNotification(characteristic.getValue());
        }
    };

    // Send OTA_START
    public void startOta(byte[] firmware) {
        byte[] sha256 = calculateSha256(firmware);
        ByteBuffer cmd = ByteBuffer.allocate(39).order(ByteOrder.LITTLE_ENDIAN);
        cmd.put((byte) 0x50);
        cmd.putInt(firmware.length);
        cmd.putShort((short) 240);
        cmd.put(sha256);

        controlChar.setValue(cmd.array());
        gatt.writeCharacteristic(controlChar);
    }

    // Send firmware chunks
    public void sendChunk(int seqNum, byte[] chunkData) {
        ByteBuffer cmd = ByteBuffer.allocate(3 + chunkData.length)
            .order(ByteOrder.LITTLE_ENDIAN);
        cmd.put((byte) 0x51);
        cmd.putShort((short) seqNum);
        cmd.put(chunkData);

        dataChar.setValue(cmd.array());
        dataChar.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE);
        gatt.writeCharacteristic(dataChar);
    }
}
```

### UI Considerations

```xml
<!-- activity_ota_upload.xml -->
<LinearLayout>
    <Button android:id="@+id/btnSelectFile" android:text="Select Firmware" />
    <TextView android:id="@+id/tvFileName" />
    <TextView android:id="@+id/tvFileSize" />
    <Button android:id="@+id/btnStartOta" android:text="Start Update" />
    <ProgressBar android:id="@+id/progressBar" style="@style/Widget.AppCompat.ProgressBar.Horizontal" />
    <TextView android:id="@+id/tvProgress" />
    <TextView android:id="@+id/tvStatus" />
    <Button android:id="@+id/btnAbort" android:text="Abort" android:visibility="gone" />
</LinearLayout>
```

---

## 9. iOS Implementation Guide

### Info.plist

```xml
<key>NSBluetoothAlwaysUsageDescription</key>
<string>Bluetooth is required for firmware updates</string>
<key>UIBackgroundModes</key>
<array>
    <string>bluetooth-central</string>
</array>
```

### Sample Implementation (Swift)

```swift
import CoreBluetooth

class BleOtaManager: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    var centralManager: CBCentralManager!
    var peripheral: CBPeripheral?
    var controlChar: CBCharacteristic?
    var dataChar: CBCharacteristic?
    var statusChar: CBCharacteristic?

    func startOta(firmware: Data) {
        var sha256 = firmware.sha256()

        var cmd = Data(capacity: 39)
        cmd.append(0x50)  // Command
        cmd.append(contentsOf: withUnsafeBytes(of: UInt32(firmware.count).littleEndian) { Array($0) })
        cmd.append(contentsOf: withUnsafeBytes(of: UInt16(240).littleEndian) { Array($0) })
        cmd.append(sha256)

        peripheral?.writeValue(cmd, for: controlChar!, type: .withResponse)
    }

    func sendChunk(seqNum: UInt16, data: Data) {
        var cmd = Data(capacity: 3 + data.count)
        cmd.append(0x51)
        cmd.append(contentsOf: withUnsafeBytes(of: seqNum.littleEndian) { Array($0) })
        cmd.append(data)

        peripheral?.writeValue(cmd, for: dataChar!, type: .withoutResponse)
    }

    func peripheral(_ peripheral: CBPeripheral,
                   didUpdateValueFor characteristic: CBCharacteristic,
                   error: Error?) {
        if characteristic.uuid == OTA_STATUS_UUID {
            if let data = characteristic.value, data.count >= 11 {
                let state = data[0]
                let progress = data[1]
                let bytesReceived = data.subdata(in: 2..<6).withUnsafeBytes {
                    $0.load(as: UInt32.self)
                }
                // Update UI...
            }
        }
    }
}
```

---

## 10. Security Considerations

### Firmware Verification

| Layer | Method | Purpose |
|-------|--------|---------|
| **Integrity** | SHA256 | Detect corruption during transfer |
| **Authenticity** | ECDSA Signature | Verify firmware is from legitimate source |
| **Rollback Protection** | Version check | Prevent downgrade attacks |

### BLE Security

| Feature | Status | Notes |
|---------|--------|-------|
| **BLE Pairing** | Optional | Can require PIN for OTA |
| **Encryption** | BLE 4.2+ | Link layer encryption |
| **MITM Protection** | Recommended | Use Secure Connections |

### Best Practices

1. **Always verify SHA256** before applying update
2. **Check firmware signature** (ECDSA) if available
3. **Implement version check** to prevent downgrades
4. **Timeout protection** - abort if no progress for 30s
5. **Require user confirmation** before starting OTA

---

## 11. Troubleshooting

### Device Not Found

| Symptom | Cause | Solution |
|---------|-------|----------|
| No devices in scan | BLE disabled on device | Power cycle device |
| | Device not advertising | Check if in pairing mode |
| | Wrong scan filter | Use correct device name prefix |

### Connection Issues

| Symptom | Cause | Solution |
|---------|-------|----------|
| Connect timeout | Device out of range | Move closer |
| | Too many connections | Disconnect other BLE devices |
| Disconnect during transfer | Interference | Reduce distance, remove obstacles |
| | MTU issues | Use smaller chunk size |

### Transfer Failures

| Error | Cause | Solution |
|-------|-------|----------|
| CHECKSUM_FAIL | Data corrupted | Retry transfer |
| | Wrong firmware file | Verify file integrity |
| FLASH_ERROR | Flash wear | Contact support |
| Timeout | Slow transfer | Increase timeout, reduce chunk size |

### Debug Logging

Enable device debug mode for detailed logs:
```json
// Send via main BLE service
{
  "command": "set_production_mode",
  "production_mode": 0
}
```

---

## Appendix: Test Tools

### Python Script

```bash
# Location: Testing/BLE_Testing/OTA_Test/ble_ota_upload.py

# Scan for devices
python ble_ota_upload.py --scan

# Upload firmware
python ble_ota_upload.py -f firmware.bin

# Upload to specific device
python ble_ota_upload.py -f firmware.bin -a AA:BB:CC:DD:EE:FF

# Abort ongoing transfer
python ble_ota_upload.py --abort

# Check status
python ble_ota_upload.py --status
```

### Requirements

```bash
pip install bleak tqdm
```

---

**Contact:** support@suriota.com | **GitHub:** GifariKemal/GatewaySuriotaPOC
