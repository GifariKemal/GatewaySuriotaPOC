# Panduan Pengguna Gateway Config App

## Informasi Dokumen

| Item | Detail |
|------|--------|
| **Versi Dokumen** | 1.0 |
| **Versi Aplikasi** | 1.0.0 |
| **Tanggal Terbit** | 30 November 2025 |
| **Kompatibel Dengan** | SRT-MGATE-1210 Industrial IoT Gateway |
| **Penulis** | Tim Dokumentasi SURIOTA |

---

## Daftar Isi

1. Pendahuluan
2. Persyaratan Sistem
3. Instalasi Aplikasi
4. Memulai Aplikasi
5. Menghubungkan Gateway via Bluetooth
6. Menu Utama Gateway
7. Mengatur Perangkat Komunikasi
8. Mengatur Data Register
9. Mengatur Koneksi Server
10. Status Perangkat
11. Pengaturan & Backup
12. Pengaturan Aplikasi
13. Pemecahan Masalah
14. Tanya Jawab (FAQ)
15. Lampiran
16. Kontak & Dukungan

---

## 1. Pendahuluan

### 1.1 Tentang Panduan Ini

Panduan ini ditujukan untuk membantu pengguna dalam mengoperasikan aplikasi **Gateway Config App** untuk mengkonfigurasi perangkat gateway **SRT-MGATE-1210**. Panduan ini ditulis untuk pengguna umum tanpa memerlukan pengetahuan teknis mendalam.

### 1.2 Tentang Aplikasi

**Gateway Config App** adalah aplikasi smartphone yang memungkinkan Anda untuk:

- Menghubungkan smartphone ke gateway melalui Bluetooth
- Mengatur koneksi jaringan (WiFi atau Kabel LAN)
- Menambahkan dan mengatur perangkat sensor/meter
- Mengkonfigurasi pengiriman data ke server cloud
- Melakukan backup dan restore pengaturan
- Memperbarui firmware gateway

### 1.3 Tentang Gateway SRT-MGATE-1210

Gateway **SRT-MGATE-1210** adalah perangkat penghubung yang mengumpulkan data dari berbagai sensor dan meter industri, kemudian mengirimkannya ke server cloud untuk dipantau dan dianalisis.

**Kemampuan Gateway:**
- Membaca data dari perangkat industri (sensor suhu, meter listrik, dll)
- Mengirim data melalui WiFi atau kabel Ethernet
- Mendukung protokol MQTT dan HTTP untuk pengiriman data
- Dapat dikonfigurasi tanpa kabel melalui Bluetooth

---

## 2. Persyaratan Sistem

### 2.1 Persyaratan Smartphone

**Untuk Pengguna Android:**
- Sistem operasi Android versi 5.0 atau lebih baru
- Bluetooth versi 4.0 atau lebih tinggi
- Ruang penyimpanan minimal 100 MB
- Koneksi internet (untuk download aplikasi)

**Untuk Pengguna iPhone/iPad:**
- Sistem operasi iOS versi 12 atau lebih baru
- Bluetooth aktif
- Ruang penyimpanan minimal 100 MB

### 2.2 Persyaratan Gateway

- Model: SRT-MGATE-1210
- Firmware versi 2.0.0 atau lebih baru
- Dalam keadaan menyala dan siap digunakan

### 2.3 Izin yang Diperlukan

Saat menggunakan aplikasi, Anda perlu mengizinkan akses berikut:

| Izin | Kegunaan |
|------|----------|
| **Bluetooth** | Untuk menghubungkan ke gateway |
| **Lokasi** | Diperlukan untuk pencarian perangkat Bluetooth |
| **Penyimpanan** | Untuk menyimpan file backup |

---

## 3. Instalasi Aplikasi

### 3.1 Untuk Pengguna Android

1. Buka aplikasi **Play Store** di smartphone Anda
2. Ketik **"Gateway Config"** atau **"SURIOTA Gateway"** di kolom pencarian
3. Pilih aplikasi yang sesuai dari hasil pencarian
4. Tekan tombol **Install**
5. Tunggu hingga proses instalasi selesai
6. Tekan **Open** untuk membuka aplikasi

### 3.2 Untuk Pengguna iPhone/iPad

1. Buka aplikasi **App Store** di perangkat Anda
2. Ketik **"Gateway Config"** atau **"SURIOTA Gateway"** di kolom pencarian
3. Pilih aplikasi yang sesuai dari hasil pencarian
4. Tekan tombol **Get** (atau ikon download)
5. Verifikasi dengan Face ID, Touch ID, atau password Apple ID
6. Tunggu hingga proses instalasi selesai

### 3.3 Pemberian Izin Pertama Kali

Saat pertama kali membuka aplikasi:

1. Akan muncul permintaan izin **Bluetooth** - Tekan **Izinkan**
2. Akan muncul permintaan izin **Lokasi** - Tekan **Izinkan**
3. Akan muncul permintaan izin **Penyimpanan** - Tekan **Izinkan**

**Catatan:** Jika Anda tidak sengaja menolak izin, Anda dapat mengaktifkannya kembali melalui menu Pengaturan smartphone.

---

## 4. Memulai Aplikasi

### 4.1 Halaman Utama (Home)

Setelah membuka aplikasi, Anda akan melihat halaman utama yang menampilkan:

- **Daftar Perangkat** - Gateway yang pernah dihubungkan
- **Tombol Tambah (+)** - Untuk mencari dan menghubungkan gateway baru
- **Menu Settings** - Pengaturan aplikasi

### 4.2 Navigasi Aplikasi

Aplikasi memiliki dua tab utama di bagian bawah layar:

| Tab | Fungsi |
|-----|--------|
| **Home** | Melihat dan mengelola gateway yang terhubung |
| **Settings** | Mengatur profil dan preferensi aplikasi |

---

## 5. Menghubungkan Gateway via Bluetooth

### 5.1 Persiapan Gateway

Sebelum menghubungkan, pastikan:

1. **Gateway dalam keadaan menyala** - Periksa lampu indikator
2. **Bluetooth gateway aktif:**
   - Pada mode standar: Tekan tombol pada gateway untuk mengaktifkan Bluetooth
   - Bluetooth akan aktif selama 5 menit setelah tombol ditekan

### 5.2 Langkah-langkah Menghubungkan

**Langkah 1: Mulai Pencarian**

1. Di halaman utama aplikasi, tekan tombol **[+]**
2. Anda akan masuk ke halaman **Scan Devices**
3. Tekan tombol **Scan** untuk memulai pencarian

**Langkah 2: Proses Pencarian**

- Aplikasi akan menampilkan animasi loading
- Proses pencarian memakan waktu sekitar 5-10 detik
- Pastikan Anda berada dalam jarak 10 meter dari gateway

**Langkah 3: Pilih Gateway**

Setelah pencarian selesai:

1. Daftar perangkat Bluetooth akan muncul
2. Cari gateway dengan nama **"SURIOTA GW"**
3. Anda dapat menggunakan kolom pencarian untuk memfilter
4. Tekan tombol **Connect** pada gateway yang diinginkan

**Langkah 4: Konfirmasi Koneksi**

Setelah berhasil terhubung:

1. Akan muncul dialog konfirmasi
2. Tekan **Yes** untuk masuk ke halaman pengaturan gateway
3. Status akan berubah menjadi **BONDED** (terhubung)

### 5.3 Tips Koneksi

- Jarak maksimal koneksi Bluetooth adalah sekitar 10 meter
- Hindari penghalang seperti tembok tebal antara smartphone dan gateway
- Jika gateway tidak ditemukan, coba tekan tombol pada gateway dan scan ulang

---

## 6. Menu Utama Gateway

### 6.1 Dashboard Gateway

Setelah terhubung, Anda akan melihat dashboard dengan informasi:

- **Nama Gateway** - Contoh: SURIOTA GW
- **Status Koneksi** - BONDED (terhubung)
- **ID Perangkat** - Alamat unik gateway
- **Tombol Disconnect** - Untuk memutuskan koneksi

### 6.2 Menu Konfigurasi

Dashboard menampilkan 6 menu utama:

| Menu | Ikon | Fungsi |
|------|------|--------|
| **Device Communications** | Perangkat | Mengatur sensor/meter yang terhubung |
| **Modbus Configurations** | Pengaturan | Mengatur data yang dibaca dari sensor |
| **Server Configurations** | Globe | Mengatur koneksi jaringan dan server |
| **Logging Configurations** | Catatan | Mengatur penyimpanan log *(Segera Hadir)* |
| **Status** | Grafik | Melihat status dan update firmware |
| **Settings** | Roda Gigi | Backup, restore, dan reset |

---

## 7. Mengatur Perangkat Komunikasi

Menu **Device Communications** digunakan untuk menambah dan mengatur perangkat sensor atau meter yang terhubung ke gateway.

### 7.1 Melihat Daftar Perangkat

Halaman ini menampilkan:
- Daftar semua perangkat yang sudah dikonfigurasi
- Status aktif/nonaktif setiap perangkat
- Jumlah data (register) yang dikonfigurasi
- Tombol untuk melihat, mengedit, dan menghapus

### 7.2 Menambah Perangkat Baru

1. Tekan tombol **[+]** di pojok kanan atas
2. Isi informasi perangkat:

**Informasi Dasar:**

| Field | Keterangan | Contoh |
|-------|------------|--------|
| Device Name | Nama untuk identifikasi | Sensor_Suhu_Ruang1 |
| Slave ID | Alamat perangkat (1-247) | 1 |

**Pilih Jenis Koneksi:**

- **Modbus RTU** - Untuk perangkat yang terhubung via kabel serial (RS485)
- **Modbus TCP** - Untuk perangkat yang terhubung via kabel jaringan (Ethernet)

### 7.3 Pengaturan Modbus RTU

Jika memilih Modbus RTU, atur parameter berikut:

| Parameter | Keterangan | Nilai Umum |
|-----------|------------|------------|
| Serial Port | Port yang digunakan | PORT1 atau PORT2 |
| Baudrate | Kecepatan komunikasi | 9600, 19200, 115200 |
| Bit Data | Jumlah bit data | 8 |
| Parity | Paritas | None, Even, Odd |
| Stop Bit | Stop bit | 1 atau 2 |

**Pengaturan Lanjutan:**

| Parameter | Keterangan | Rekomendasi |
|-----------|------------|-------------|
| Retry Count | Jumlah percobaan ulang | 3 |
| Timeout | Batas waktu tunggu (milidetik) | 3000 |
| Refresh Rate | Interval pembacaan (milidetik) | 5000 |

### 7.4 Pengaturan Modbus TCP

Jika memilih Modbus TCP, atur:

| Parameter | Keterangan | Contoh |
|-----------|------------|--------|
| IP Address | Alamat IP perangkat | 192.168.1.100 |
| Server Port | Port komunikasi | 502 |

### 7.5 Menyimpan Perangkat

Setelah mengisi semua data, tekan **Save Device Configuration** untuk menyimpan.

### 7.6 Mengaktifkan/Menonaktifkan Perangkat

Gunakan tombol toggle (saklar) pada kartu perangkat untuk mengaktifkan atau menonaktifkan pembacaan data tanpa menghapus konfigurasi.

---

## 8. Mengatur Data Register

Menu **Modbus Configurations** digunakan untuk mengatur data apa saja yang akan dibaca dari setiap perangkat.

### 8.1 Memilih Perangkat

1. Buka menu **Modbus Configurations**
2. Pilih perangkat dari dropdown **Choose Device**
3. Daftar register (data) yang sudah dikonfigurasi akan muncul

### 8.2 Menambah Register Baru

1. Tekan tombol **[+]** di pojok kanan atas
2. Isi informasi register:

**Informasi Register:**

| Field | Keterangan | Contoh |
|-------|------------|--------|
| Data Name | Nama data | Suhu_Ruangan |
| Choose Device | Perangkat sumber | Sensor_Suhu_Ruang1 |
| Description | Deskripsi (opsional) | Suhu ruang produksi |
| Unit | Satuan | °C, %, kWh |

**Pengaturan Pembacaan:**

| Field | Keterangan | Contoh |
|-------|------------|--------|
| Choose Function | Jenis pembacaan | Input Registers |
| Address Modbus | Alamat data | 0 |
| Choose Data Type | Format data | INT16, FLOAT32 |

### 8.3 Jenis Pembacaan (Function)

| Jenis | Kode | Kegunaan |
|-------|------|----------|
| Coil Status | 01 | Membaca status ON/OFF |
| Input Status | 02 | Membaca input digital |
| Holding Register | 03 | Membaca/menulis data |
| Input Registers | 04 | Membaca data sensor |

### 8.4 Format Data (Data Type)

**Format Umum:**

| Format | Keterangan |
|--------|------------|
| INT16 | Bilangan bulat (-32768 s/d 32767) |
| UINT16 | Bilangan bulat positif (0 s/d 65535) |
| FLOAT32 | Bilangan desimal |
| INT32 | Bilangan bulat besar |

**Catatan tentang Format:**
- **BE (Big Endian)** - Format standar, coba ini terlebih dahulu
- **LE (Little Endian)** - Jika data terbaca salah, coba format ini
- **BS (Byte Swap)** - Untuk perangkat dengan urutan byte berbeda

### 8.5 Kalibrasi Data

Jika data perlu dikonversi, gunakan pengaturan kalibrasi:

| Parameter | Keterangan | Contoh |
|-----------|------------|--------|
| Scale | Pengali | 0.1 |
| Offset | Penambah | 0 |

**Rumus:** `Nilai Akhir = (Nilai Mentah × Scale) + Offset`

**Contoh Penggunaan:**
- Sensor memberikan nilai 250
- Scale = 0.1, Offset = 0
- Hasil = 250 × 0.1 + 0 = **25.0°C**

---

## 9. Mengatur Koneksi Server

Menu **Server Configurations** digunakan untuk mengatur bagaimana gateway mengirim data ke server cloud.

### 9.1 Pengaturan Jaringan

**Pilih Mode Koneksi:**

| Mode | Keterangan |
|------|------------|
| **WIFI** | Menggunakan jaringan WiFi |
| **ETH** | Menggunakan kabel Ethernet |

**Pengaturan WiFi:**

| Field | Keterangan |
|-------|------------|
| WiFi Enabled | Aktifkan WiFi |
| WiFi SSID | Nama jaringan WiFi |
| WiFi Password | Kata sandi WiFi |

**Pengaturan Ethernet:**

| Field | Keterangan |
|-------|------------|
| Ethernet Enabled | Aktifkan Ethernet |
| Using DHCP | Otomatis mendapat IP |
| Static IP | Alamat IP manual (jika DHCP nonaktif) |
| Gateway | Alamat gateway jaringan |
| Subnet | Subnet mask |

### 9.2 Pengaturan MQTT

MQTT adalah protokol untuk mengirim data ke server cloud.

**Parameter Dasar:**

| Field | Keterangan | Contoh |
|-------|------------|--------|
| Enabled | Aktifkan MQTT | true |
| Broker Address | Alamat server MQTT | broker.hivemq.com |
| Broker Port | Port server | 1883 |
| Client ID | ID unik gateway | SRT-MGATE-001 |
| Username | Nama pengguna | (opsional) |
| Password | Kata sandi | (opsional) |

**Parameter Lanjutan:**

| Field | Keterangan | Rekomendasi |
|-------|------------|-------------|
| Keep Alive | Interval ping (detik) | 60 |
| Clean Session | Mulai sesi baru | true |
| Use TLS | Enkripsi data | false (untuk testing) |

### 9.3 Pengaturan Topic

**Mode Default (Sederhana):**
- Semua data dikirim ke satu alamat topic
- Cocok untuk penggunaan umum

**Mode Customize (Lanjutan):**
- Bisa membuat beberapa topic berbeda
- Setiap topic bisa memiliki interval berbeda
- Bisa memilih data spesifik untuk setiap topic

### 9.4 Pengaturan Interval Pengiriman

Interval menentukan seberapa sering data dikirim ke server.

**Satuan Waktu:**

| Satuan | Singkatan | Konversi |
|--------|-----------|----------|
| Milidetik | ms | 1000 ms = 1 detik |
| Detik | s | 60 s = 1 menit |
| Menit | m | 1 m = 60 detik |

**Tabel Konversi:**

| Nilai | Satuan | Sama Dengan |
|-------|--------|-------------|
| 5 | s | 5 detik |
| 60 | s | 1 menit |
| 5 | m | 5 menit |
| 1000 | ms | 1 detik |

**Rekomendasi Interval:**
- **Monitoring real-time:** 1-5 detik
- **Logging standar:** 30 detik - 1 menit
- **Hemat bandwidth:** 5-15 menit

### 9.5 Pengaturan HTTP

HTTP adalah alternatif protokol untuk mengirim data.

| Field | Keterangan | Contoh |
|-------|------------|--------|
| Enabled | Aktifkan HTTP | true |
| Endpoint URL | Alamat server | https://api.server.com/data |
| Method | Metode pengiriman | POST |
| Timeout | Batas waktu (ms) | 5000 |
| Custom Headers | Header tambahan | Authorization, dll |

---

## 10. Status Perangkat

Menu **Status** menampilkan informasi tentang gateway.

### 10.1 Informasi yang Ditampilkan

| Item | Keterangan |
|------|------------|
| **Firmware** | Versi perangkat lunak gateway |
| **SD Card Info** | Informasi kartu memori *(Segera Hadir)* |
| **Update Firmware** | Memperbarui perangkat lunak gateway |

### 10.2 Memperbarui Firmware

1. Tekan menu **Update Firmware**
2. Pilih file firmware (.bin) dari penyimpanan
3. Tunggu proses update selesai (jangan matikan gateway!)
4. Gateway akan restart otomatis

**Peringatan:** Jangan matikan gateway atau menutup aplikasi selama proses update berlangsung.

---

## 11. Pengaturan & Backup

Menu **Settings** pada dashboard gateway menyediakan fitur untuk mengelola konfigurasi.

### 11.1 Import Config (Memuat Konfigurasi)

Untuk memuat konfigurasi dari file backup:

1. Tekan **Import Config**
2. Pilih file .json dari penyimpanan smartphone
3. Konfirmasi untuk memuat konfigurasi
4. Gateway akan menerapkan pengaturan dari file

### 11.2 Download All Config (Backup)

Untuk menyimpan semua konfigurasi ke file:

1. Tekan **Download All Config**
2. File akan tersimpan di folder: `Documents/GatewayConfig/backup/`
3. Nama file: `config_backup_[tanggal].json`

**Data yang di-backup:**
- Pengaturan perangkat
- Pengaturan register
- Pengaturan jaringan
- Pengaturan MQTT/HTTP

### 11.3 Clear Configuration (Reset)

Untuk menghapus semua konfigurasi dan mengembalikan ke pengaturan awal:

1. Tekan **Clear Configuration**
2. Konfirmasi penghapusan
3. Semua data akan dihapus
4. Gateway akan restart

**Peringatan:** Pastikan sudah melakukan backup sebelum reset! Data yang dihapus tidak dapat dikembalikan.

---

## 12. Pengaturan Aplikasi

Tab **Settings** di bagian bawah aplikasi menyediakan pengaturan umum.

### 12.1 Menu yang Tersedia

**Akun:**
- **Profile** - Mengelola informasi akun pengguna

**Tentang:**
- **About Product** - Informasi tentang gateway
- **About App** - Informasi tentang aplikasi

**Lainnya:**
- **Contact Us** - Menghubungi tim dukungan
- **Logout** - Keluar dari akun

---

## 13. Pemecahan Masalah

### 13.1 Masalah Koneksi Bluetooth

| Masalah | Solusi |
|---------|--------|
| Gateway tidak ditemukan | 1. Pastikan Bluetooth gateway aktif (tekan tombol)<br>2. Dekatkan smartphone (maks 10 meter)<br>3. Restart Bluetooth smartphone<br>4. Cek izin aplikasi |
| Koneksi terputus | 1. Periksa jarak ke gateway<br>2. Hindari gangguan WiFi<br>3. Restart aplikasi |
| Gagal pairing | 1. Hapus gateway dari daftar Bluetooth<br>2. Restart gateway<br>3. Coba hubungkan ulang |

### 13.2 Masalah Konfigurasi

| Masalah | Solusi |
|---------|--------|
| Pengaturan tidak tersimpan | 1. Pastikan koneksi stabil<br>2. Tunggu konfirmasi "Saved"<br>3. Dekatkan smartphone ke gateway |
| Data sensor tidak terbaca | 1. Periksa alamat register<br>2. Verifikasi jenis pembacaan<br>3. Cek koneksi fisik sensor |

### 13.3 Masalah Jaringan

| Masalah | Solusi |
|---------|--------|
| WiFi tidak terhubung | 1. Periksa nama dan password WiFi<br>2. Pastikan router menyala<br>3. Cek jarak ke router |
| Ethernet tidak aktif | 1. Periksa kabel LAN<br>2. Verifikasi pengaturan IP<br>3. Cek DHCP server |
| Data tidak terkirim ke server | 1. Periksa alamat broker/server<br>2. Verifikasi username/password<br>3. Cek koneksi internet |

---

## 14. Tanya Jawab (FAQ)

### Koneksi & Bluetooth

**T: Berapa jarak maksimal koneksi Bluetooth?**
J: Sekitar 10 meter dalam ruangan tanpa penghalang.

**T: Gateway tidak muncul saat scan, apa yang harus dilakukan?**
J: Tekan tombol pada gateway untuk mengaktifkan Bluetooth, lalu scan ulang.

**T: Apakah bisa mengatur gateway tanpa internet?**
J: Ya, konfigurasi via Bluetooth tidak memerlukan internet.

### Data & Pengiriman

**T: Apa itu interval pengiriman?**
J: Waktu jeda antar pengiriman data ke server. Misalnya 5 detik artinya data dikirim setiap 5 detik.

**T: Apa arti ms, s, dan m pada interval?**
J:
- ms = milidetik (1000 ms = 1 detik)
- s = detik (60 s = 1 menit)
- m = menit (1 m = 60 detik)

**T: Berapa interval yang direkomendasikan?**
J: Untuk monitoring: 5 detik. Untuk logging: 1-5 menit. Untuk hemat bandwidth: 10-15 menit.

**T: Apa yang terjadi jika internet putus?**
J: Data akan disimpan sementara dan dikirim ulang setelah internet pulih.

### Format Data

**T: Apa perbedaan BE dan LE?**
J: BE (Big Endian) dan LE (Little Endian) adalah urutan penyimpanan data. Coba BE terlebih dahulu, jika data salah gunakan LE.

**T: Kapan menggunakan format dengan BS (Byte Swap)?**
J: Jika data masih salah setelah mencoba BE dan LE, coba format dengan BS.

### Backup & Reset

**T: Dimana file backup tersimpan?**
J: Di folder `Documents/GatewayConfig/backup/` pada smartphone.

**T: Apakah bisa memindahkan konfigurasi ke gateway lain?**
J: Ya, gunakan fitur Download Config dari gateway lama, lalu Import Config ke gateway baru.

---

## 15. Lampiran

### 15.1 Contoh Data yang Dikirim ke Server (MQTT)

Berikut contoh format data yang dikirim gateway ke server:

```
{
  "device_id": "SRT-MGATE-001",
  "timestamp": "2025-11-30T10:30:45",
  "data": {
    "Suhu_Ruangan": {
      "value": 25.5,
      "unit": "°C"
    },
    "Kelembaban": {
      "value": 65,
      "unit": "%"
    },
    "Tekanan": {
      "value": 1013.25,
      "unit": "hPa"
    }
  }
}
```

### 15.2 Daftar Port Umum

| Layanan | Port |
|---------|------|
| MQTT (tanpa enkripsi) | 1883 |
| MQTT (dengan enkripsi) | 8883 |
| HTTP | 80 |
| HTTPS | 443 |
| Modbus TCP | 502 |

### 15.3 Daftar Baudrate Umum

| Baudrate | Penggunaan |
|----------|------------|
| 9600 | Standar, paling umum |
| 19200 | Kecepatan menengah |
| 38400 | Kecepatan tinggi |
| 115200 | Kecepatan sangat tinggi |

---

## 16. Kontak & Dukungan

### Tim Dukungan SURIOTA

**Email:** support@suriota.com

**Website:** www.suriota.com

**Jam Operasional:** Senin - Jumat, 08:00 - 17:00 WIB

### Informasi Tambahan

Untuk dokumentasi teknis lebih lanjut, kunjungi:
- GitHub Gateway: github.com/GifariKemal/GatewaySuriotaPOC
- GitHub Aplikasi: github.com/dickykhusnaedy/suriota_mobile_app

---

## Informasi Dokumen

| Item | Detail |
|------|--------|
| **Judul** | Panduan Pengguna Gateway Config App |
| **Versi Dokumen** | 1.0 |
| **Versi Aplikasi** | 1.0.0 |
| **Tanggal Pembuatan** | 30 November 2025 |
| **Penulis** | Tim Dokumentasi SURIOTA |
| **Penerbit** | PT SURIOTA Technology Indonesia |

---

**© 2025 SURIOTA. Hak Cipta Dilindungi.**

*Dokumen ini dibuat untuk membantu pengguna dalam mengoperasikan Gateway Config App dan SRT-MGATE-1210 Industrial IoT Gateway.*
