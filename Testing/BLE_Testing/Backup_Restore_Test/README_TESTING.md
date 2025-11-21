# 🧪 PANDUAN TESTING BLE BACKUP & RESTORE

**Versi:** 1.0.0
**Tanggal:** 21 November 2025
**Author:** SURIOTA R&D Team

---

## 📋 Daftar Isi

1. [Persiapan Testing](#-persiapan-testing)
2. [Cara Testing Manual](#-cara-testing-manual)
3. [Cara Testing Otomatis](#-cara-testing-otomatis)
4. [Payload JSON Siap Pakai](#-payload-json-siap-pakai)
5. [Expected Results (Hasil yang Diharapkan)](#-expected-results)
6. [Troubleshooting](#-troubleshooting)
7. [Checklist Validasi](#-checklist-validasi)

---

## 🔧 Persiapan Testing

### 1. Kebutuhan Hardware

✅ **ESP32-S3 Gateway** dengan firmware v2.3.0 atau lebih baru
✅ **PC/Laptop** dengan Bluetooth LE
✅ **Kabel USB-C** untuk monitoring serial (opsional)

### 2. Kebutuhan Software

#### Install Python 3.8+

```bash
# Cek versi Python
python3 --version

# Jika belum ada, install Python 3.8+
```

#### Install Dependencies

```bash
# Masuk ke folder testing
cd /home/user/GatewaySuriotaPOC/Testing/BLE_Testing/Backup_Restore_Test

# Install library yang dibutuhkan
pip3 install bleak
```

### 3. Persiapan Gateway

#### A. Upload Firmware

1. Buka Arduino IDE
2. Buka file `Main/Main.ino`
3. Set board: **ESP32-S3 Dev Module**
4. Upload ke gateway
5. Tunggu sampai selesai

#### B. Setup Konfigurasi Awal

**PENTING:** Sebelum testing, pastikan gateway sudah punya **beberapa device** yang terkonfigurasi!

Gunakan BLE untuk membuat minimal **3-5 device** dengan cara:

```json
{
  "op": "create",
  "type": "device",
  "config": {
    "device_name": "Test Sensor 1",
    "protocol": "RTU",
    "slave_id": 1,
    "serial_port": 1,
    "baud_rate": 9600,
    "timeout": 3000,
    "retry_count": 3,
    "refresh_rate_ms": 5000,
    "enabled": true,
    "registers": [
      {
        "register_name": "Temperature",
        "address": 0,
        "function_code": 3,
        "data_type": "FLOAT32_ABCD",
        "quantity": 2,
        "refresh_rate_ms": 1000
      }
    ]
  }
}
```

Ulangi untuk **Test Sensor 2, 3, 4, 5** dengan slave_id berbeda.

#### C. Nyalakan BLE

**Development Mode** (BLE selalu ON):
- BLE akan otomatis ON saat boot

**Production Mode** (BLE pakai button):
- **Tekan dan tahan button** selama 3 detik
- LED akan berkedip, BLE ON

#### D. Verifikasi dengan Serial Monitor

Buka Serial Monitor (115200 baud), cari output:

```
[BLE] BLE advertising started: SURIOTA GW
[BLE] MTU negotiation completed: 512 bytes
```

---

## 📱 Cara Testing Manual

### STEP 1: Jalankan Test Script

```bash
cd /home/user/GatewaySuriotaPOC/Testing/BLE_Testing/Backup_Restore_Test

python3 test_backup_restore.py
```

### STEP 2: Koneksi ke Gateway

Script akan otomatis:
1. **Scan** BLE devices
2. **Detect** "SURIOTA GW"
3. **Connect** otomatis
4. **Nego MTU** (sampai 512 bytes)

Output yang diharapkan:

```
======================================================================
🔍 SCANNING FOR 'SURIOTA GW'
======================================================================
✓ Found: SURIOTA GW
✓ Address: XX:XX:XX:XX:XX:XX

🔗 CONNECTING...
✓ Connected successfully!
✓ Notifications enabled
```

### STEP 3: Menu Testing

Setelah connect, akan muncul menu:

```
======================================================================
BLE BACKUP & RESTORE TEST MENU
======================================================================

📋 AVAILABLE TESTS:
   1. Test Backup (full_config)
   2. Test Restore (from previous backup)
   3. Test Restore Error Handling (invalid payload)
   4. Test Complete Backup-Restore Cycle
   5. Save Backup to File
   6. Load Backup from File and Restore
   7. Run ALL Tests (Automated Suite)

   0. Exit
======================================================================
```

---

## 🧪 Cara Testing Otomatis

### Option 1: Run ALL Tests (Pilihan 7)

Pilih **option 7** dari menu untuk menjalankan semua test otomatis.

Test yang akan dijalankan:
- ✅ **Test 1**: Backup configuration
- ✅ **Test 3**: Error handling
- ✅ **Test 4**: Complete backup-restore cycle

### Option 2: Run Individual Tests

#### TEST 1: Backup Configuration

**Pilih option: 1**

**Apa yang terjadi:**
1. Script kirim command backup ke gateway
2. Gateway proses semua konfigurasi
3. Gateway kirim response via BLE (fragmented)
4. Script assembly fragments jadi complete JSON
5. Script validasi struktur response

**Output yang diharapkan:**

```
======================================================================
TEST 1: BACKUP CONFIGURATION (full_config)
======================================================================

📤 SENDING COMMAND:
   {"op": "read", "type": "full_config"}

📥 WAITING FOR RESPONSE (timeout: 60s)...
[BLE] Fragment 1: 244 bytes
[BLE] Fragment 2: 244 bytes
...
[BLE] ✓ Response complete (42 fragments)

✅ RESPONSE RECEIVED
   Status: ok

📊 BACKUP INFO:
   Firmware Version: 2.3.0
   Device Name: SURIOTA_GW
   Total Devices: 5
   Total Registers: 15
   Processing Time: 287 ms
   Backup Size: 10240 bytes (10.00 KB)

📦 CONFIG STRUCTURE:
   ✓ Devices: 5 found
   ✓ Server Config: Present
   ✓ Logging Config: Present
      - Device D7A3F2: 3 registers
      - Device A1B2C3: 3 registers
      - Device E4F5G6: 3 registers
      - Device H7I8J9: 3 registers
      - Device K0L1M2: 3 registers

✅ TEST PASSED: Backup successful
   Total: 5 devices, 15 registers
```

**Serial Monitor Output (Gateway):**

```
[CRUD] Full config backup requested
[CRUD] Full config backup complete: 5 devices, 15 registers, 10240 bytes, 287 ms
```

---

#### TEST 2: Restore Configuration

**Pilih option: 2**

**⚠️ PENTING:** Harus run TEST 1 dulu, atau load backup dari file (option 6)!

**Apa yang terjadi:**
1. Script tanya konfirmasi (karena akan replace semua config)
2. User ketik **yes** untuk lanjut
3. Script kirim command restore dengan config dari backup
4. Gateway clear semua config existing
5. Gateway restore devices satu-satu
6. Gateway restore server config
7. Gateway restore logging config
8. Gateway notify Modbus services
9. Gateway kirim response summary

**Output yang diharapkan:**

```
======================================================================
TEST 2: RESTORE CONFIGURATION (restore_config)
======================================================================

⚠️  WARNING: This will REPLACE all current configurations!
   Devices to restore: 5
   Server config: Yes
   Logging config: Yes

⚠️  Continue with restore? (yes/no): yes

📤 SENDING COMMAND:
   {"op": "system", "type": "restore_config", "config": {...}}

📥 WAITING FOR RESPONSE (timeout: 60s)...
[BLE] Fragment 1: 200 bytes
[BLE] ✓ Response complete (1 fragments)

✅ RESPONSE RECEIVED
   Status: ok

📊 RESTORE RESULTS:
   Success Count: 3
   Fail Count: 0
   Restored Configs: devices.json, server_config.json, logging_config.json
   Message: Configuration restore completed. Device restart recommended.
   Requires Restart: Yes

✅ TEST PASSED: Restore successful (3/3)

⚠️  DEVICE RESTART RECOMMENDED
```

**Serial Monitor Output (Gateway):**

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

#### TEST 3: Error Handling

**Pilih option: 3**

**Apa yang terjadi:**
1. Script kirim command restore **TANPA field 'config'** (sengaja invalid)
2. Gateway detect error
3. Gateway kirim error response

**Output yang diharapkan:**

```
======================================================================
TEST 3: RESTORE WITH INVALID PAYLOAD (Error Handling)
======================================================================

⚠️  Sending restore command WITHOUT 'config' field...

📤 SENDING COMMAND:
   {"op": "system", "type": "restore_config"}

✅ RESPONSE RECEIVED
   Status: error
   Error: Missing 'config' object in restore payload

✅ TEST PASSED: Error handling works correctly
```

**Serial Monitor Output (Gateway):**

```
[CONFIG RESTORE] ❌ ERROR: Missing 'config' object in payload
```

---

#### TEST 4: Complete Backup-Restore-Compare Cycle

**Pilih option: 4**

**Apa yang terjadi:**
1. **STEP 1:** Backup configuration pertama
2. Save backup ke file: `backup_before_restore_TIMESTAMP.json`
3. **STEP 2:** Restore configuration dari backup
4. Tunggu 3 detik untuk apply changes
5. **STEP 3:** Backup configuration kedua (setelah restore)
6. Save backup ke file: `backup_after_restore_TIMESTAMP.json`
7. **STEP 4:** Compare kedua backup:
   - Device count harus sama
   - Device IDs harus sama
   - Register count harus sama

**Output yang diharapkan:**

```
======================================================================
TEST 4: BACKUP-RESTORE-COMPARE CYCLE (Data Integrity)
======================================================================

[STEP 1/4] Creating initial backup...
   ✓ Backup saved: backup_before_restore_20251121_143052.json

[STEP 2/4] Restoring configuration...
⚠️  This will restore the same config (should succeed)

⚠️  Continue with restore? (yes/no): yes
   ✓ Restore successful

⏳ Waiting 3 seconds for restore to complete...

[STEP 3/4] Creating backup after restore...
   ✓ Backup saved: backup_after_restore_20251121_143052.json

[STEP 4/4] Comparing backups...
   ✓ Device count matches: 5
   ✓ Device IDs match
   ✓ Register count matches: 15

✅ TEST PASSED: Data integrity verified
   Backup files saved for manual inspection:
   - backup_before_restore_20251121_143052.json
   - backup_after_restore_20251121_143052.json
```

**Ini test PALING PENTING** karena membuktikan:
- ✅ Data tidak hilang
- ✅ Data tidak corrupt
- ✅ Restore bekerja dengan benar

---

## 📄 Payload JSON Siap Pakai

### 1. Backup Command

```json
{
  "op": "read",
  "type": "full_config"
}
```

**Kirim via BLE COMMAND characteristic**

---

### 2. Restore Command

```json
{
  "op": "system",
  "type": "restore_config",
  "config": {
    "devices": [
      {
        "device_id": "D7A3F2",
        "device_name": "Temperature Sensor",
        "protocol": "RTU",
        "slave_id": 1,
        "serial_port": 1,
        "baud_rate": 9600,
        "timeout": 3000,
        "retry_count": 3,
        "refresh_rate_ms": 5000,
        "enabled": true,
        "registers": [
          {
            "register_id": "R001",
            "register_name": "Temperature",
            "address": 0,
            "function_code": 3,
            "data_type": "FLOAT32_ABCD",
            "quantity": 2,
            "refresh_rate_ms": 1000
          }
        ]
      }
    ],
    "server_config": {
      "communication": {
        "mode": "ETH"
      },
      "protocol": "mqtt",
      "wifi": {
        "ssid": "MyWiFi",
        "password": "password123"
      },
      "mqtt_config": {
        "broker_address": "broker.hivemq.com",
        "broker_port": 1883,
        "topic_publish": "suriota/gateway/data",
        "client_id": "SURIOTA_GW_001",
        "qos": 1
      }
    },
    "logging_config": {
      "logging_ret": "1w",
      "logging_interval": "5m"
    }
  }
}
```

**⚠️ CATATAN:** Field `config` harus diambil dari response **full_config** backup!

---

### 3. Invalid Restore Command (untuk error testing)

```json
{
  "op": "system",
  "type": "restore_config"
}
```

**Note:** Sengaja tidak ada field `config` - untuk test error handling.

---

## ✅ Expected Results

### Backup Response

```json
{
  "status": "ok",
  "backup_info": {
    "timestamp": 1234567,
    "firmware_version": "2.3.0",
    "device_name": "SURIOTA_GW",
    "total_devices": 5,
    "total_registers": 15,
    "processing_time_ms": 287,
    "backup_size_bytes": 10240
  },
  "config": {
    "devices": [...],
    "server_config": {...},
    "logging_config": {...}
  }
}
```

**Validasi:**
- ✅ `status` = `"ok"`
- ✅ `backup_info` ada dan lengkap
- ✅ `config.devices` adalah array
- ✅ `config.server_config` ada
- ✅ `config.logging_config` ada
- ✅ Total devices match dengan jumlah element dalam array
- ✅ Setiap device punya `registers` array

---

### Restore Response (Success)

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

**Validasi:**
- ✅ `status` = `"ok"`
- ✅ `success_count` = 3 (semua config restored)
- ✅ `fail_count` = 0
- ✅ `restored_configs` berisi 3 file names
- ✅ `requires_restart` = `true`

---

### Restore Response (Error)

```json
{
  "status": "error",
  "error": "Missing 'config' object in restore payload"
}
```

**Validasi:**
- ✅ `status` = `"error"`
- ✅ `error` message jelas dan deskriptif

---

## 🔧 Troubleshooting

### Problem 1: Gateway tidak ditemukan saat scan

**Symptom:**
```
❌ 'SURIOTA GW' not found!
```

**Solution:**
1. ✅ Cek apakah BLE sudah ON (cek serial monitor)
2. ✅ Production mode: tekan button 3 detik untuk enable BLE
3. ✅ Cek jarak: gateway harus dalam radius 5-10 meter
4. ✅ Restart gateway dan coba lagi
5. ✅ Cek Bluetooth PC/laptop sudah ON

---

### Problem 2: Connection timeout

**Symptom:**
```
❌ Connection failed!
```

**Solution:**
1. ✅ Reset gateway (tekan tombol reset)
2. ✅ Tunggu 5 detik, coba connect lagi
3. ✅ Check serial monitor untuk error messages
4. ✅ Pastikan tidak ada device lain yang connect ke gateway

---

### Problem 3: Response timeout (no response)

**Symptom:**
```
⏱️  TIMEOUT after 60 seconds!
```

**Solution:**
1. ✅ Check serial monitor - lihat error messages
2. ✅ Config terlalu besar? Coba dengan config lebih kecil (hapus beberapa device)
3. ✅ Check memory: lihat free DRAM/PSRAM di serial monitor
4. ✅ Restart gateway dan coba lagi

---

### Problem 4: JSON parse error

**Symptom:**
```
❌ JSON Parse Error: ...
Raw response (first 500 chars): ...
```

**Solution:**
1. ✅ BLE response mungkin corrupt - coba lagi
2. ✅ MTU negotiation gagal - restart gateway
3. ✅ Check serial monitor untuk BLE errors
4. ✅ Update firmware ke versi terbaru

---

### Problem 5: Partial restore failure

**Symptom:**
```
⚠️  WARNING: 1 configuration(s) failed to restore
   Success Count: 2
   Fail Count: 1
```

**Solution:**
1. ✅ Check serial monitor untuk detail error
2. ✅ Biasanya `logging_config` yang fail (not critical)
3. ✅ Verify critical configs (devices, server) sudah restored
4. ✅ Manual fix via BLE jika perlu

---

### Problem 6: Memory exhaustion

**Symptom di Serial Monitor:**
```
[MEM] CRITICAL: Free DRAM below threshold
[MEM] DRAM: 15234 bytes free
```

**Solution:**
1. ✅ Gateway memory penuh - hapus beberapa device
2. ✅ Restart gateway untuk clear memory
3. ✅ Test dengan config lebih kecil (< 30 devices)
4. ✅ Check firmware sudah pakai PSRAM allocation

---

## ✔️ Checklist Validasi

### Pre-Testing Checklist

- [ ] Firmware v2.3.0+ sudah uploaded
- [ ] Gateway punya minimal 3-5 devices configured
- [ ] BLE sudah ON (cek serial monitor)
- [ ] Python 3.8+ installed
- [ ] Library `bleak` installed
- [ ] Serial monitor open (115200 baud) - opsional tapi recommended

---

### Test Execution Checklist

#### TEST 1: Backup

- [ ] Command terkirim via BLE
- [ ] Response diterima (tidak timeout)
- [ ] Status = "ok"
- [ ] backup_info lengkap (7 fields)
- [ ] config.devices adalah array
- [ ] Jumlah devices match
- [ ] Setiap device punya registers array
- [ ] config.server_config ada
- [ ] config.logging_config ada
- [ ] Processing time < 1000ms (untuk 10 devices)
- [ ] Backup size reasonable (tidak 0, tidak > 500KB)

---

#### TEST 2: Restore

- [ ] Konfirmasi user diminta
- [ ] Command terkirim via BLE
- [ ] Response diterima (tidak timeout)
- [ ] Status = "ok"
- [ ] success_count = 3
- [ ] fail_count = 0
- [ ] restored_configs berisi 3 filenames
- [ ] requires_restart = true
- [ ] Serial monitor menunjukkan restore steps
- [ ] Devices sudah muncul di gateway (verify via backup lagi)

---

#### TEST 3: Error Handling

- [ ] Command invalid terkirim
- [ ] Response diterima
- [ ] Status = "error"
- [ ] Error message: "Missing 'config' object"
- [ ] Gateway tidak crash
- [ ] Serial monitor menunjukkan error log

---

#### TEST 4: Backup-Restore-Compare

- [ ] Backup pertama success
- [ ] File saved: backup_before_restore_*.json
- [ ] Restore success
- [ ] Wait 3 seconds completed
- [ ] Backup kedua success
- [ ] File saved: backup_after_restore_*.json
- [ ] Device count match
- [ ] Device IDs match
- [ ] Register count match
- [ ] Kedua file bisa dibuka dan valid JSON

---

### Post-Testing Checklist

- [ ] Gateway masih responsive (tidak crash)
- [ ] BLE masih bisa connect
- [ ] Modbus polling masih jalan (cek serial monitor)
- [ ] Memory usage normal (check serial monitor)
- [ ] Backup files tersimpan dengan benar
- [ ] Manual inspection: compare kedua backup files dengan JSON diff tool

---

## 📊 Performance Benchmarks

### Expected Performance

| Config Size | Devices | Registers | Backup Size | Processing Time | BLE Transfer Time (512 MTU) |
|------------|---------|-----------|-------------|-----------------|----------------------------|
| Small      | 5       | 15        | ~10 KB      | 200-300 ms      | 2-3 seconds                |
| Medium     | 20      | 60        | ~40 KB      | 400-600 ms      | 5-8 seconds                |
| Large      | 50      | 150       | ~120 KB     | 800-1200 ms     | 15-20 seconds              |

**Note:** Processing time = waktu di gateway. Transfer time = waktu kirim via BLE.

---

## 📞 Support

Jika menemukan bug atau masalah:

1. **Simpan log files:**
   - Backup files (.json)
   - Serial monitor output (copy-paste ke .txt file)
   - Python script output

2. **Report ke:**
   - GitHub Issues: [Link repository]
   - Email: support@suriota.com

3. **Include information:**
   - Firmware version
   - Python version
   - OS (Windows/Linux/Mac)
   - Error messages lengkap

---

## 🎓 Tips & Best Practices

### Tip 1: Backup Before Critical Operations

**SELALU backup dulu sebelum:**
- Factory reset
- Firmware update
- Mass device configuration changes

```python
# Workflow recommended:
1. Backup configuration
2. Save to file
3. Do critical operation
4. If fail: restore from backup
```

---

### Tip 2: Regular Backups

Setup routine backup setiap minggu:

```python
# Simpan dengan nama yang jelas
gateway_backup_2025-11-21_weekly.json
gateway_backup_2025-11-28_weekly.json
```

---

### Tip 3: Version Control

Simpan backup dengan metadata:

```json
{
  "backup_date": "2025-11-21",
  "firmware_version": "2.3.0",
  "notes": "Backup sebelum upgrade ke v2.4.0",
  "backup_data": {
    // actual backup here
  }
}
```

---

### Tip 4: Test Restore in Staging

Jika punya 2 gateway:
1. Backup dari Gateway A
2. Restore ke Gateway B (test environment)
3. Validate everything works
4. Then restore to Gateway A (production)

---

**Made with ❤️ by SURIOTA R&D Team**
*Empowering Industrial IoT Solutions*
