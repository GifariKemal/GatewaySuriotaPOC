# SD Card Manager - Dokumentasi Testing

**Versi:** 1.0.0
**Tanggal:** Januari 2025
**Penulis:** SURIOTA R&D Team

---

## Daftar Isi / Table of Contents

1. [Lingkungan Test](#lingkungan-test)
2. [Checklist Sebelum Test](#checklist-sebelum-test)
3. [Test Cases](#test-cases)
4. [Prosedur Testing](#prosedur-testing)
5. [Hasil yang Diharapkan](#hasil-yang-diharapkan)
6. [Troubleshooting](#troubleshooting)
7. [Template Laporan Test](#template-laporan-test)

---

## Lingkungan Test

### Kebutuhan Hardware

| Komponen         | Spesifikasi                            |
| ---------------- | -------------------------------------- |
| Board            | ESP32-S3-WROOM-1-N16R8                 |
| Model Gateway    | SRT-MGATE-1210                         |
| SD Card          | SDHC, format FAT32, < 32GB             |
| Kabel USB        | USB-C atau Micro-USB (untuk Serial)    |

### Kebutuhan Software

| Software           | Versi         |
| ------------------ | ------------- |
| Arduino IDE        | 2.x atau lebih baru |
| ESP32 Arduino Core | 3.3.3         |
| Serial Monitor     | 115200 baud   |

### Pengaturan Board di Arduino IDE

| Setting          | Value                                   |
| ---------------- | --------------------------------------- |
| Board            | ESP32S3 Dev Module                      |
| Flash Size       | 16MB (128Mb)                            |
| Flash Mode       | QIO 80MHz                               |
| PSRAM            | OPI PSRAM                               |
| Partition Scheme | 8M with spiffs (3MB APP/1.5MB SPIFFS)   |
| Upload Speed     | 921600                                  |
| USB Mode         | Hardware CDC and JTAG                   |

---

## Checklist Sebelum Test

### Persiapan Hardware

- [ ] Slot SD Card bersih dan tidak ada kotoran
- [ ] SD Card sudah diformat FAT32
- [ ] SD Card terpasang dengan benar (terdengar bunyi klik)
- [ ] Kabel USB terhubung untuk monitoring Serial
- [ ] Gateway dalam kondisi menyala

### Persiapan Software

- [ ] Arduino IDE dikonfigurasi dengan pengaturan board yang benar
- [ ] Firmware berhasil dikompilasi tanpa error
- [ ] Serial Monitor terbuka pada 115200 baud
- [ ] SD_MODE sudah diatur di `SDCardManager.h`:
  ```cpp
  #define SD_MODE SD_MODE_SPI    // Untuk mode SPI (default)
  // atau
  #define SD_MODE SD_MODE_SDMMC  // Untuk mode SDMMC
  ```

---

## Test Cases

### TC-001: Inisialisasi Dasar / Basic Initialization

| ID       | TC-001                                         |
| -------- | ---------------------------------------------- |
| Judul    | Inisialisasi SD Card Dasar                     |
| Prioritas | Kritikal                                      |
| Prasyarat | SD Card terpasang, firmware sudah di-upload   |

**Langkah-langkah:**
1. Nyalakan gateway
2. Amati output Serial

**Hasil yang Diharapkan:**
```
========================================================
     SD Card Manager Test - SRT-MGATE-1210
========================================================
 Platform:  ESP32-S3-WROOM-1-N16R8
 Flash:     16MB (QIO 80MHz)
 PSRAM:     8MB OPI
 SD Mode:   SPI (HSPI/SPI3 - Separate from Ethernet)
 SD Pins:   CS=11, MOSI=10, MISO=13, SCK=12
========================================================

[SD] Initializing SD Card Manager...
[SD] Mode: SPI (HSPI/SPI3 - Separate from Ethernet)
[SD] Pins: CS=11, MOSI=10, MISO=13, SCK=12
========================================
  SD CARD: INITIALIZING -> READY
========================================
[SD] Card ready for operations
[SD] Card mounted successfully!
[SD] Task started on Core 0 (priority 1)
```

---

### TC-002: Tampilan Informasi Kartu / Card Information Display

| ID       | TC-002                                         |
| -------- | ---------------------------------------------- |
| Judul    | Pengambilan Informasi SD Card                  |
| Prioritas | Tinggi                                        |
| Prasyarat | TC-001 berhasil                               |

**Langkah-langkah:**
1. Ketik `status` di Serial Monitor
2. Tekan Enter

**Hasil yang Diharapkan:**
```
> Command: status

==================================================
              SD CARD STATUS
==================================================
 State:           READY
 Card Type:       SDHC
 Card Size:       XXXXX      MB
 Total Space:     XXXXX      MB
 Used Space:      XXX        MB
 Free Space:      XXXXX      MB
--------------------------------------------------
              STATISTICS
--------------------------------------------------
 Mount Count:     1
 Unmount Count:   0
 Error Count:     0
 Writes OK:       0
 Writes Failed:   0
 Log Entries:     0
==================================================
```

---

### TC-003: Operasi Tulis / Write Operation

| ID       | TC-003                                         |
| -------- | ---------------------------------------------- |
| Judul    | Test Penulisan Manual                          |
| Prioritas | Kritikal                                      |
| Prasyarat | TC-001 berhasil                               |

**Langkah-langkah:**
1. Ketik `write` di Serial Monitor
2. Tekan Enter
3. Ulangi 3 kali

**Hasil yang Diharapkan:**
```
> Command: write
Written: [XXXXX] Manual test entry - Heap: XXXXXX bytes

> Command: write
Written: [XXXXX] Manual test entry - Heap: XXXXXX bytes

> Command: write
Written: [XXXXX] Manual test entry - Heap: XXXXXX bytes
```

**Verifikasi:**
- Ketik `status` - `Writes OK` harus menunjukkan 3
- Ketik `list` - `/logs/log_000001.txt` harus ada

---

### TC-004: Operasi Baca / Read Operation

| ID       | TC-004                                         |
| -------- | ---------------------------------------------- |
| Judul    | Test Pembacaan File                            |
| Prioritas | Tinggi                                        |
| Prasyarat | TC-003 berhasil                               |

**Langkah-langkah:**
1. Ketik `read` di Serial Monitor
2. Tekan Enter

**Hasil yang Diharapkan:**
```
> Command: read
--- Log Contents ---
[XXXXX] Manual test entry - Heap: XXXXXX bytes
[XXXXX] Manual test entry - Heap: XXXXXX bytes
[XXXXX] Manual test entry - Heap: XXXXXX bytes
--- End of Log ---
```

---

### TC-005: Daftar Direktori / Directory Listing

| ID       | TC-005                                         |
| -------- | ---------------------------------------------- |
| Judul    | Test Daftar Direktori                          |
| Prioritas | Sedang                                        |
| Prasyarat | TC-003 berhasil                               |

**Langkah-langkah:**
1. Ketik `list` di Serial Monitor
2. Tekan Enter

**Hasil yang Diharapkan:**
```
> Command: list

Directory listing: /
----------------------------------------
  [DIR]  logs
  [DIR]  System Volume Information
  [FILE] log_000001.txt          XXX bytes
----------------------------------------
```

---

### TC-006: Test Performa / Performance Test

| ID       | TC-006                                         |
| -------- | ---------------------------------------------- |
| Judul    | Performa Tulis/Baca 100KB                      |
| Prioritas | Sedang                                        |
| Prasyarat | TC-001 berhasil                               |

**Langkah-langkah:**
1. Ketik `perf` di Serial Monitor
2. Tekan Enter
3. Tunggu hingga selesai

**Hasil yang Diharapkan:**
```
> Command: perf

=== Performance Test (100KB) ===
Writing 102400 bytes in 512-byte chunks...
Write: 102400 bytes in XXX ms (XXX.X KB/s)
Reading back...
Read:  102400 bytes in XXX ms (XXX.X KB/s)
=== Performance Test Complete ===
```

**Kriteria Kelulusan:**
| Mode   | Kecepatan Tulis | Kecepatan Baca |
| ------ | --------------- | -------------- |
| SPI    | > 200 KB/s      | > 300 KB/s     |
| SDMMC  | > 400 KB/s      | > 600 KB/s     |

---

### TC-007: Hot-Plug - Pencabutan Kartu / Card Removal

| ID       | TC-007                                         |
| -------- | ---------------------------------------------- |
| Judul    | Deteksi Pencabutan Kartu Hot-Plug              |
| Prioritas | Kritikal                                      |
| Prasyarat | TC-001 berhasil, state adalah READY           |

**Langkah-langkah:**
1. Saat sistem berjalan, cabut SD Card secara fisik
2. Amati output Serial dalam 2-5 detik

**Hasil yang Diharapkan:**
```
[SD] WARN: Card removal detected!
========================================
  SD CARD: READY -> REMOVED
========================================
[SD] WARN: Card removed - operations suspended
[SD] Insert card to auto-reconnect
```

**Verifikasi:**
- Ketik `status` - State harus `REMOVED`
- Ketik `write` - Harus gagal dengan "Write failed (card not ready?)"

---

### TC-008: Hot-Plug - Pemasangan Kembali / Card Re-insertion

| ID       | TC-008                                         |
| -------- | ---------------------------------------------- |
| Judul    | Auto-Reconnect Hot-Plug                        |
| Prioritas | Kritikal                                      |
| Prasyarat | TC-007 berhasil, state adalah REMOVED         |

**Langkah-langkah:**
1. Masukkan kembali SD Card
2. Tunggu deteksi otomatis (2-5 detik)

**Hasil yang Diharapkan:**
```
[SD] Reconnect attempt 1/3
========================================
  SD CARD: RECONNECTING -> READY
========================================
[SD] Card ready for operations
[SD] Card re-inserted and mounted!
```

**Verifikasi:**
- Ketik `status` - State harus `READY`
- Ketik `write` - Harus berhasil

---

### TC-009: Hot-Plug - Maksimal Percobaan / Max Retries

| ID       | TC-009                                         |
| -------- | ---------------------------------------------- |
| Judul    | Penanganan Maksimal Percobaan Hot-Plug         |
| Prioritas | Tinggi                                        |
| Prasyarat | Kartu dicabut                                 |

**Langkah-langkah:**
1. Cabut SD Card
2. Tunggu 3 percobaan reconnect (6-10 detik)
3. JANGAN masukkan kartu

**Hasil yang Diharapkan:**
```
[SD] Reconnect attempt 1/3
[SD] Reconnect attempt 2/3
[SD] Reconnect attempt 3/3
========================================
  SD CARD: RECONNECTING -> ERROR
========================================
[SD] ERROR: Max reconnect attempts reached
```

**Verifikasi:**
- Ketik `status` - State harus `ERROR`
- Error Count harus bertambah

---

### TC-010: Tanpa Kartu Saat Startup / No Card at Startup

| ID       | TC-010                                         |
| -------- | ---------------------------------------------- |
| Judul    | Startup Tanpa SD Card                          |
| Prioritas | Tinggi                                        |
| Prasyarat | SD Card TIDAK terpasang                       |

**Langkah-langkah:**
1. Cabut SD Card
2. Reset/Power cycle gateway
3. Amati output Serial

**Hasil yang Diharapkan:**
```
[SD] Initializing SD Card Manager...
[SD] Mode: SPI (HSPI/SPI3 - Separate from Ethernet)
[SD] WARN: No card detected at startup (will auto-detect)
========================================
  SD CARD: INITIALIZING -> REMOVED
========================================
[SD] WARN: Card removed - operations suspended
[SD] Insert card to auto-reconnect
[SD] Task started on Core 0 (priority 1)
```

**Verifikasi:**
- Sistem TIDAK boleh crash
- Masukkan kartu nanti - harus auto-mount

---

### TC-011: Output Status JSON / JSON Status Output

| ID       | TC-011                                         |
| -------- | ---------------------------------------------- |
| Judul    | Format Status JSON                             |
| Prioritas | Sedang                                        |
| Prasyarat | TC-001 berhasil                               |

**Langkah-langkah:**
1. Ketik `json` di Serial Monitor
2. Tekan Enter

**Hasil yang Diharapkan:**
```json
{"state":"READY","card_inserted":true,"card_type":"SDHC","size_mb":XXXXX,"total_mb":XXXXX,"used_mb":XXX,"free_mb":XXXXX,"mount_count":1,"unmount_count":0,"error_count":0,"writes_ok":0,"writes_fail":0,"log_entries":0}
```

**Verifikasi:**
- JSON harus valid (bisa di-parse)
- Semua field harus ada

---

### TC-012: Diagnostik Memori / Memory Diagnostics

| ID       | TC-012                                         |
| -------- | ---------------------------------------------- |
| Judul    | Laporan Penggunaan Memori                      |
| Prioritas | Sedang                                        |
| Prasyarat | Firmware berjalan                             |

**Langkah-langkah:**
1. Ketik `mem` di Serial Monitor
2. Tekan Enter

**Hasil yang Diharapkan:**
```
==================================================
           MEMORY DIAGNOSTICS
==================================================
 Free Heap:       XXXXXX bytes
 Min Free Heap:   XXXXXX bytes
 Max Alloc Heap:  XXXXXX bytes
 Free PSRAM:      XXXXXXX bytes
 PSRAM Size:      8388608 bytes
 SD Task Stack:   XXXX bytes remaining
==================================================
```

**Kriteria Kelulusan:**
- Free Heap > 50.000 bytes
- PSRAM Size = 8388608 (8MB)
- SD Task Stack > 2000 bytes tersisa

---

### TC-013: Kontrol Enable/Disable

| ID       | TC-013                                         |
| -------- | ---------------------------------------------- |
| Judul    | Enable/Disable Manual                          |
| Prioritas | Sedang                                        |
| Prasyarat | TC-001 berhasil                               |

**Langkah-langkah:**
1. Ketik `disable` - harus menonaktifkan operasi SD
2. Ketik `status` - verifikasi state adalah DISABLED
3. Ketik `write` - harus gagal
4. Ketik `enable` - harus mengaktifkan kembali
5. Tunggu reconnection
6. Ketik `write` - harus berhasil

**Hasil yang Diharapkan:**
```
> Command: disable
========================================
  SD CARD: READY -> DISABLED
========================================
[SD] SD card operations disabled

> Command: enable
========================================
  SD CARD: DISABLED -> RECONNECTING
========================================
[SD] Reconnect attempt 1/3
========================================
  SD CARD: RECONNECTING -> READY
========================================
```

---

### TC-014: Auto-Logging

| ID       | TC-014                                         |
| -------- | ---------------------------------------------- |
| Judul    | Logging Otomatis Periodik                      |
| Prioritas | Sedang                                        |
| Prasyarat | TC-001 berhasil                               |

**Langkah-langkah:**
1. Tunggu 30 detik setelah inisialisasi
2. Amati entri auto-log (setiap 5 detik)

**Hasil yang Diharapkan:**
```
[SD] DBG: Auto-logged: [15000] Heap: 245000, PSRAM: 7500000, State: READY
[SD] DBG: Auto-logged: [20000] Heap: 244800, PSRAM: 7500000, State: READY
[SD] DBG: Auto-logged: [25000] Heap: 244600, PSRAM: 7500000, State: READY
```

**Verifikasi:**
- Ketik `status` - Log Entries harus bertambah setiap 5 detik

---

### TC-015: Stress Test - Hot-Plug Cepat / Rapid Hot-Plug

| ID       | TC-015                                         |
| -------- | ---------------------------------------------- |
| Judul    | Siklus Cabut/Pasang Kartu Cepat                |
| Prioritas | Tinggi                                        |
| Prasyarat | TC-001 berhasil                               |

**Langkah-langkah:**
1. Cabut SD Card
2. Tunggu 3 detik
3. Masukkan kembali SD Card
4. Tunggu state READY
5. Ulangi langkah 1-4 sebanyak 10 siklus

**Hasil yang Diharapkan:**
- Sistem TIDAK boleh crash
- Setiap siklus harus bertransisi: READY → REMOVED → RECONNECTING → READY
- Mount/Unmount counts harus bertambah dengan benar

**Kriteria Kelulusan:**
- 10/10 siklus recovery berhasil
- Tidak ada memory leak (Free Heap stabil)
- Error count = 0

---

### TC-016: Test Durasi Panjang / Long Duration Test

| ID       | TC-016                                         |
| -------- | ---------------------------------------------- |
| Judul    | Test Stabilitas 24 Jam                         |
| Prioritas | Tinggi                                        |
| Prasyarat | TC-001 berhasil                               |

**Langkah-langkah:**
1. Mulai sistem dengan SD Card terpasang
2. Biarkan berjalan selama 24 jam
3. Periksa status secara berkala

**Titik Monitoring:**
- [ ] Jam ke-1: Periksa status
- [ ] Jam ke-4: Periksa status
- [ ] Jam ke-8: Periksa status
- [ ] Jam ke-12: Periksa status
- [ ] Jam ke-24: Pemeriksaan akhir

**Kriteria Kelulusan:**
| Metrik           | Kriteria                    |
| ---------------- | --------------------------- |
| State            | READY                       |
| Error Count      | 0                           |
| Failed Writes    | 0                           |
| Free Heap        | > 40.000 bytes              |
| Task Stack       | > 1.500 bytes tersisa       |

---

## Prosedur Testing

### Prosedur 1: Full Regression Test

Jalankan semua test case secara berurutan:

```
1. TC-001: Inisialisasi Dasar
2. TC-002: Tampilan Informasi Kartu
3. TC-003: Operasi Tulis
4. TC-004: Operasi Baca
5. TC-005: Daftar Direktori
6. TC-006: Test Performa
7. TC-011: Output Status JSON
8. TC-012: Diagnostik Memori
9. TC-007: Hot-Plug Pencabutan Kartu
10. TC-008: Hot-Plug Pemasangan Kembali
11. TC-013: Kontrol Enable/Disable
12. TC-014: Auto-Logging
```

### Prosedur 2: Quick Smoke Test

Test minimal untuk verifikasi cepat:

```
1. TC-001: Inisialisasi Dasar
2. TC-003: Operasi Tulis
3. TC-007: Hot-Plug Pencabutan Kartu
4. TC-008: Hot-Plug Pemasangan Kembali
```

### Prosedur 3: Pre-Production Test

Sebelum deploy ke produksi:

```
1. Full Regression Test (Prosedur 1)
2. TC-009: Maksimal Percobaan
3. TC-010: Tanpa Kartu Saat Startup
4. TC-015: Stress Test
5. TC-016: Test Durasi Panjang (minimal 8 jam)
```

---

## Hasil yang Diharapkan

### Transisi State

```
Alur Normal:
NOT_INITIALIZED → INITIALIZING → READY

Pencabutan Kartu:
READY → REMOVED → RECONNECTING → READY (berhasil)
READY → REMOVED → RECONNECTING → ERROR (max retries)

Kontrol Manual:
READY → DISABLED → RECONNECTING → READY
```

### Benchmark Performa

| Test           | Mode SPI      | Mode SDMMC    |
| -------------- | ------------- | ------------- |
| 100KB Write    | 250-400 KB/s  | 400-600 KB/s  |
| 100KB Read     | 350-500 KB/s  | 600-800 KB/s  |
| Small Log      | 3-8 ms        | 2-5 ms        |

### Penggunaan Memori

| Metrik              | Nilai yang Diharapkan |
| ------------------- | --------------------- |
| Task Stack Usage    | < 6.000 bytes         |
| DRAM Impact         | < 5.000 bytes         |
| PSRAM Impact        | ~ 0 (minimal)         |

---

## Troubleshooting

### Masalah: Kartu Tidak Terdeteksi

**Gejala:**
- State tetap REMOVED
- Pesan "No card detected"

**Solusi:**
1. Periksa kartu sudah masuk sepenuhnya (bunyi klik)
2. Coba SD Card berbeda
3. Format kartu sebagai FAT32
4. Periksa pin yang bengkok di slot

### Masalah: Mount Gagal

**Gejala:**
- "Mount failed at 4MHz, trying 1MHz..."
- "Mount failed even at 1MHz"

**Solusi:**
1. Bersihkan kontak SD Card
2. Gunakan SD Card bermerek asli
3. Periksa power supply 3.3V
4. Coba mode SDMMC jika SPI gagal

### Masalah: Gagal Menulis

**Gejala:**
- Counter `writes_fail` bertambah
- "Failed to open log file"

**Solusi:**
1. Periksa SD Card tidak write-protected
2. Verifikasi ruang kosong cukup
3. Periksa kerusakan file system
4. Format ulang kartu

### Masalah: Memory Leak

**Gejala:**
- Free Heap menurun seiring waktu
- Akhirnya crash

**Solusi:**
1. Periksa kebocoran file handle
2. Pastikan file ditutup setelah digunakan
3. Monitor dengan perintah `mem`
4. Laporkan bug dengan trace memori

---

## Template Laporan Test

```
===========================================
LAPORAN TEST SD CARD MANAGER
===========================================

Tanggal: _______________
Penguji: _______________
Versi Firmware: _______________
Versi Arduino Core: _______________

HARDWARE:
- Board: ESP32-S3-WROOM-1-N16R8
- Merek SD Card: _______________
- Ukuran SD Card: _______________GB
- Mode SD: [ ] SPI  [ ] SDMMC

HASIL TEST:
┌─────────┬──────────────────────────────┬────────┬─────────┐
│ ID Test │ Nama Test                    │ Hasil  │ Catatan │
├─────────┼──────────────────────────────┼────────┼─────────┤
│ TC-001  │ Inisialisasi Dasar           │ [ ]    │         │
│ TC-002  │ Tampilan Informasi Kartu     │ [ ]    │         │
│ TC-003  │ Operasi Tulis                │ [ ]    │         │
│ TC-004  │ Operasi Baca                 │ [ ]    │         │
│ TC-005  │ Daftar Direktori             │ [ ]    │         │
│ TC-006  │ Test Performa                │ [ ]    │         │
│ TC-007  │ Hot-Plug Pencabutan          │ [ ]    │         │
│ TC-008  │ Hot-Plug Pemasangan          │ [ ]    │         │
│ TC-009  │ Hot-Plug Max Retries         │ [ ]    │         │
│ TC-010  │ Tanpa Kartu Saat Startup     │ [ ]    │         │
│ TC-011  │ Output Status JSON           │ [ ]    │         │
│ TC-012  │ Diagnostik Memori            │ [ ]    │         │
│ TC-013  │ Kontrol Enable/Disable       │ [ ]    │         │
│ TC-014  │ Auto-Logging                 │ [ ]    │         │
│ TC-015  │ Stress Test                  │ [ ]    │         │
│ TC-016  │ Test Durasi Panjang          │ [ ]    │         │
└─────────┴──────────────────────────────┴────────┴─────────┘

Keterangan: [P] Pass/Lulus  [F] Fail/Gagal  [S] Skip  [B] Blocked

HASIL PERFORMA:
- Kecepatan Tulis: _______ KB/s
- Kecepatan Baca: _______ KB/s

MEMORI SETELAH TEST:
- Free Heap: _______ bytes
- Free PSRAM: _______ bytes
- Task Stack Tersisa: _______ bytes

MASALAH DITEMUKAN:
1. _______________________________________________
2. _______________________________________________
3. _______________________________________________

HASIL KESELURUHAN: [ ] LULUS  [ ] GAGAL

TANDA TANGAN:
Penguji: _______________ Tanggal: _______________
Reviewer: _______________ Tanggal: _______________
===========================================
```

---

## Ringkasan Test Cases

| ID      | Nama Test                      | Prioritas | Estimasi Waktu |
| ------- | ------------------------------ | --------- | -------------- |
| TC-001  | Inisialisasi Dasar             | Kritikal  | 2 menit        |
| TC-002  | Tampilan Informasi Kartu       | Tinggi    | 1 menit        |
| TC-003  | Operasi Tulis                  | Kritikal  | 2 menit        |
| TC-004  | Operasi Baca                   | Tinggi    | 1 menit        |
| TC-005  | Daftar Direktori               | Sedang    | 1 menit        |
| TC-006  | Test Performa (100KB)          | Sedang    | 3 menit        |
| TC-007  | Hot-Plug Pencabutan Kartu      | Kritikal  | 2 menit        |
| TC-008  | Hot-Plug Auto-Reconnect        | Kritikal  | 2 menit        |
| TC-009  | Penanganan Max Retries         | Tinggi    | 3 menit        |
| TC-010  | Tanpa Kartu Saat Startup       | Tinggi    | 3 menit        |
| TC-011  | Output Status JSON             | Sedang    | 1 menit        |
| TC-012  | Diagnostik Memori              | Sedang    | 1 menit        |
| TC-013  | Kontrol Enable/Disable         | Sedang    | 3 menit        |
| TC-014  | Auto-Logging                   | Sedang    | 2 menit        |
| TC-015  | Stress Test (Rapid Hot-Plug)   | Tinggi    | 15 menit       |
| TC-016  | Test Stabilitas 24 Jam         | Tinggi    | 24 jam         |

**Total Waktu (tanpa TC-016):** ~40 menit
**Total Waktu (dengan TC-016):** ~24 jam 40 menit

---

**Kontrol Dokumen:**

| Versi | Tanggal    | Penulis         | Perubahan         |
| ----- | ---------- | --------------- | ----------------- |
| 1.0.0 | Jan 2025   | SURIOTA R&D     | Rilis awal        |

---

**Akhir Dokumentasi Testing**
