# ⚡ QUICK START - BLE Backup & Restore Testing

**5 Langkah Simple untuk Testing Cepat**

---

## 1️⃣ Install Dependencies

```bash
cd /home/user/GatewaySuriotaPOC/Testing/BLE_Testing/Backup_Restore_Test
pip3 install -r requirements.txt
```

---

## 2️⃣ Nyalakan Gateway & BLE

**Development Mode:**
- BLE otomatis ON saat boot

**Production Mode:**
- Tekan dan tahan button 3 detik

**Verifikasi:** Buka Serial Monitor, cari:
```
[BLE] BLE advertising started: SURIOTA GW
```

---

## 3️⃣ Jalankan Test Script

```bash
python3 test_backup_restore.py
```

Script akan otomatis:
- ✅ Scan dan connect ke gateway
- ✅ Tampilkan menu testing

---

## 4️⃣ Pilih Test

### Test Cepat (5 menit):

```
Pilih option: 1    # Backup
Pilih option: 3    # Error handling
```

### Test Lengkap (10 menit):

```
Pilih option: 7    # Run ALL tests (otomatis)
```

### Test Manual (step-by-step):

```
Pilih option: 1    # Backup dulu
Pilih option: 2    # Restore
Ketik: yes         # Konfirmasi restore
```

---

## 5️⃣ Validasi Results

### ✅ Test PASSED jika:

**Backup:**
- Status = "ok"
- Ada backup_info dengan semua fields
- Ada config dengan devices, server_config, logging_config
- Processing time < 2000ms
- Backup size reasonable (tidak 0, tidak > 200KB)

**Restore:**
- Status = "ok"
- success_count = 3
- fail_count = 0
- requires_restart = true

**Error Handling:**
- Status = "error"
- Error message: "Missing 'config' object"

---

## 🎯 Test Priority

**WAJIB test ini:**
1. ✅ Test 1: Backup (validate struktur response)
2. ✅ Test 4: Backup-Restore-Compare Cycle (validate data integrity)

**Opsional:**
- Test 2: Restore (jika ingin test manual)
- Test 3: Error handling (validate firmware error handling)

---

## 🚨 Troubleshooting Cepat

**Gateway tidak ditemukan:**
```bash
# Check BLE status
# Restart gateway
# Coba scan lagi
```

**Timeout:**
```bash
# Check config size (terlalu besar?)
# Check memory di serial monitor
# Restart gateway
```

**JSON Error:**
```bash
# Restart gateway
# Coba lagi
```

---

## 📊 Expected Performance

| Config Size | Devices | Processing | Transfer Time |
|------------|---------|------------|---------------|
| Small      | 5       | 200-300 ms | 2-3 sec       |
| Medium     | 20      | 400-600 ms | 5-8 sec       |
| Large      | 50      | 800-1200 ms| 15-20 sec     |

---

## 📞 Need Help?

**Baca dokumentasi lengkap:**
```bash
cat README_TESTING.md
```

**Lihat contoh payloads:**
```bash
cat example_payloads.json
```

**Report bug:**
- GitHub Issues
- Email: support@suriota.com

---

**Made with ❤️ by SURIOTA R&D Team**
