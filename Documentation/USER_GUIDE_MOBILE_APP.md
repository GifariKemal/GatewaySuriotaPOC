# Gateway Config App - User Guide

**App Version:** 1.0.0
**Document Version:** 2.0
**Last Updated:** November 30, 2025
**Compatible with:** SRT-MGATE-1210 Industrial IoT Gateway

---

## Daftar Isi

1. [Pendahuluan](#1-pendahuluan)
2. [Persyaratan Sistem](#2-persyaratan-sistem)
3. [Instalasi Aplikasi](#3-instalasi-aplikasi)
4. [Memulai Aplikasi](#4-memulai-aplikasi)
5. [Koneksi BLE ke Gateway](#5-koneksi-ble-ke-gateway)
6. [Dashboard Utama (Detail Device)](#6-dashboard-utama-detail-device)
7. [Device Communications](#7-device-communications)
8. [Modbus Configurations](#8-modbus-configurations)
9. [Server Configurations](#9-server-configurations)
10. [Logging Configurations](#10-logging-configurations)
11. [Device Status](#11-device-status)
12. [Device Settings](#12-device-settings)
13. [Pengaturan Aplikasi](#13-pengaturan-aplikasi)
14. [Troubleshooting](#14-troubleshooting)
15. [FAQ](#15-faq)
16. [Informasi Teknis](#16-informasi-teknis)

---

## 1. Pendahuluan

### 1.1 Tentang Aplikasi

**Gateway Config App** adalah aplikasi mobile yang dibangun dengan Flutter, dirancang untuk membantu pengguna melakukan provisioning dan konfigurasi perangkat gateway—mulai dari koneksi Bluetooth Low Energy (BLE), pengaturan WiFi/Ethernet, hingga sinkronisasi data—secara cepat dan intuitif. Aplikasi ini ideal untuk teknisi lapangan atau pengguna akhir yang membutuhkan interface user-friendly untuk mengatur gateway dalam hitungan menit.

### 1.2 Fitur Utama

| Fitur | Deskripsi |
|-------|-----------|
| **Dynamic BLE Service Detection** | Otomatis menemukan dan menampilkan UUID yang tersedia untuk dipilih |
| **Network Configuration** | Mendukung input SSID, password, dan opsi DHCP/Static IP |
| **JSON File Read/Write** | Sinkronisasi parameter konfigurasi ke dan dari gateway menggunakan file JSON |
| **Responsive UI** | Dibangun dengan Flutter dan Material 3 untuk performa smooth di Android dan iOS |
| **Modbus RTU/TCP** | Dukungan penuh untuk kedua protokol Modbus |
| **Multi-Protocol Cloud** | MQTT dan HTTP untuk pengiriman data ke cloud |
| **Backup/Restore** | Export dan import konfigurasi dalam format JSON |
| **OTA Update** | Update firmware secara wireless |

### 1.3 Tech Stack

| Layer | Technology |
|-------|------------|
| **Language** | Dart |
| **Framework** | Flutter >= 3.22 |
| **State Management** | GetX |
| **Bluetooth** | flutter_blue_plus >= 1.35 |

---

## 2. Persyaratan Sistem

### 2.1 Smartphone

**Android:**
- **OS:** Android 5.0 (Lollipop) atau lebih baru
- **Bluetooth:** BLE 4.0 atau lebih tinggi
- **SDK Target:** Android SDK 34
- **RAM:** Minimal 2GB
- **Storage:** Minimal 100MB ruang kosong

**iOS:**
- **OS:** iOS 12 atau lebih baru
- **Xcode:** 15 (untuk build)
- **Bluetooth:** BLE support

### 2.2 Gateway

- **Model:** SRT-MGATE-1210
- **Firmware:** v2.0.0 atau lebih baru
- **Mode BLE:** Aktif (tekan tombol pada gateway atau aktif di mode development)

### 2.3 Izin Aplikasi

Aplikasi memerlukan izin berikut:
- **Bluetooth** - Untuk koneksi ke gateway
- **Location** - Diperlukan Android untuk scanning BLE (nearby devices)
- **Storage** - Untuk backup/restore konfigurasi

---

## 3. Instalasi Aplikasi

### 3.1 Download

**Android:**
1. Buka **Google Play Store** di smartphone Anda
2. Cari "**Gateway Config**" atau "**SURIOTA Gateway**"
3. Tap **Install** dan tunggu hingga selesai

**iOS:**
1. Buka **App Store** di iPhone/iPad Anda
2. Cari "**Gateway Config**" atau "**SURIOTA Gateway**"
3. Tap **Get** dan tunggu hingga selesai

### 3.2 Izin Pertama Kali

Saat pertama kali membuka aplikasi:
1. Izinkan akses **Bluetooth**
2. Izinkan akses **Location** (nearby devices)
3. Izinkan akses **Storage** (untuk backup/restore)

> **Catatan:** Jika izin ditolak, aplikasi akan menampilkan halaman **Permission Denied** dengan panduan untuk mengaktifkan izin di Settings.

---

## 4. Memulai Aplikasi

### 4.1 Splash Screen

Saat aplikasi dibuka, splash screen akan muncul sebentar saat aplikasi melakukan inisialisasi.

### 4.2 Home Screen

Setelah splash screen, Anda akan melihat **Home Screen** yang menampilkan:

```
┌─────────────────────────────────┐
│           Home                  │
│  Manage your connected devices  │
├─────────────────────────────────┤
│                                 │
│     Connected Devices           │
│                                 │
│     No device history.          │
│     Connect a device to see     │
│     it here.                    │
│                                 │
│           [+]                   │
├─────────────────────────────────┤
│    🏠 Home      ⚙️ Settings     │
└─────────────────────────────────┘
```

### 4.3 Navigasi Utama

- **Home** - Daftar perangkat yang terhubung/pernah terhubung
- **Tombol [+]** - Scan dan hubungkan ke gateway baru
- **Settings** - Pengaturan akun dan aplikasi

---

## 5. Koneksi BLE ke Gateway

### 5.1 Persiapan Gateway

Sebelum menghubungkan, pastikan:
1. Gateway dalam keadaan **menyala**
2. **BLE aktif** pada gateway:
   - Mode Development: BLE selalu aktif
   - Mode Production: Tekan tombol pada gateway untuk mengaktifkan BLE (timeout 5 menit)

### 5.2 Langkah Koneksi

#### Step 1: Mulai Scanning

1. Tap tombol **[+]** di Home Screen
2. Anda akan masuk ke halaman **Scan Devices**
3. Tap tombol **Scan** untuk memulai pencarian

```
┌─────────────────────────────────┐
│        Scan Devices         🔍  │
├─────────────────────────────────┤
│                                 │
│         Find device             │
│                                 │
│   Finding nearby devices with   │
│   Bluetooth connectivity...     │
│                                 │
│         [ 🔍 Scan ]             │
│                                 │
└─────────────────────────────────┘
```

#### Step 2: Proses Scanning

Aplikasi akan menampilkan animasi loading dengan pesan "**Scanning device...**"

> **Tips:** Proses scanning biasanya memakan waktu 5-10 detik

#### Step 3: Pilih Gateway

Setelah scanning selesai, daftar perangkat BLE akan muncul:

```
┌─────────────────────────────────┐
│        Scan Devices         🔍  │
├─────────────────────────────────┤
│        Scan Results             │
│  🔍 Search device by name...    │
├─────────────────────────────────┤
│ ((📶))  SURIOTA GW              │
│         ID: 94:A9:90:06:3A:C9   │
│                      [Connect]  │
├─────────────────────────────────┤
│ ((📶))  [TV] Samsung 4 Ser...   │
│         ID: A4:30:7A:E6:CA:3D   │
│                      [Connect]  │
├─────────────────────────────────┤
│ A total of 4 devices were       │
│ successfully discovered.        │
└─────────────────────────────────┘
```

4. Cari gateway Anda dengan nama **"SURIOTA GW"**
5. Gunakan search bar untuk filter berdasarkan nama
6. Tap tombol **Connect** pada gateway yang diinginkan

#### Step 4: Konfirmasi Koneksi

Setelah berhasil terhubung, dialog konfirmasi akan muncul:

```
┌─────────────────────────────────┐
│                                 │
│      Device Connected           │
│                                 │
│   Do you want to open device    │
│   (SURIOTA GW) page detail?     │
│                                 │
│      [ No ]    [ Yes ]          │
│                                 │
└─────────────────────────────────┘
```

6. Tap **Yes** untuk masuk ke halaman Detail Device

### 5.3 Status Koneksi

Setelah terhubung, status akan berubah:
- **BONDED** - Gateway terpasang dan terhubung (hijau)
- Tombol berubah menjadi **Disconnect** (merah)

---

## 6. Dashboard Utama (Detail Device)

### 6.1 Tampilan Dashboard

Setelah terhubung, Anda akan melihat dashboard utama:

```
┌─────────────────────────────────┐
│  ←       Detail Device          │
├─────────────────────────────────┤
│ ((📶))  SURIOTA GW              │
│  ✓ BONDED                       │
│  📍 94:A9:90:06:3A:C9           │
│                    [Disconnect] │
├─────────────────────────────────┤
│      CONFIGURATION MENU         │
├─────────────────────────────────┤
│ ┌───────────┐  ┌───────────┐   │
│ │    📱     │  │    🎛️     │   │
│ │  Device   │  │  Modbus   │   │
│ │  Comms    │  │  Config   │   │
│ └───────────┘  └───────────┘   │
│ ┌───────────┐  ┌───────────┐   │
│ │    🌐     │  │    📝     │   │
│ │  Server   │  │  Logging  │   │
│ │  Config   │  │  Config   │   │
│ └───────────┘  └───────────┘   │
│ ┌───────────┐  ┌───────────┐   │
│ │    📊     │  │    ⚙️     │   │
│ │  Status   │  │ Settings  │   │
│ └───────────┘  └───────────┘   │
└─────────────────────────────────┘
```

### 6.2 Menu Konfigurasi

| Menu | Fungsi |
|------|--------|
| **Device Communications** | Kelola perangkat Modbus RTU/TCP |
| **Modbus Configurations** | Kelola register Modbus |
| **Server Configurations** | Atur jaringan, MQTT, dan HTTP |
| **Logging Configurations** | Atur penyimpanan log |
| **Status** | Lihat info firmware dan update |
| **Settings** | Import/export config, factory reset |

### 6.3 Disconnect Gateway

Untuk memutuskan koneksi:
1. Tap tombol **Disconnect** (merah) di bagian atas
2. Koneksi akan terputus dan Anda kembali ke halaman scan

---

## 7. Device Communications

### 7.1 Daftar Perangkat

Menu ini menampilkan semua perangkat Modbus yang dikonfigurasi:

```
┌─────────────────────────────────┐
│  ←   Device Communications   ➕  │
├─────────────────────────────────┤
│  Data Devices                   │
│  ⟳ Updated Just now            │
│  🔍 Search by name, I...  All ≡ │
├─────────────────────────────────┤
│ ┌─────────────────────────────┐ │
│ │ ● Active              🔘 ON │ │
│ │ 📱 RTU_Device_45Regs        │ │
│ │    🎛️ RTU  # 45 Registers   │ │
│ │    ID: D7227b               │ │
│ │  [👁️ View] [✏️ Edit]  [🗑️] │ │
│ └─────────────────────────────┘ │
│                                 │
│ Showing 1 entries               │
└─────────────────────────────────┘
```

### 7.2 Informasi Perangkat

Setiap kartu perangkat menampilkan:
- **Status** - Active (hijau) / Inactive (abu-abu)
- **Toggle** - Aktifkan/nonaktifkan perangkat
- **Nama Perangkat** - Nama yang diberikan user
- **Tipe Protokol** - RTU atau TCP
- **Jumlah Register** - Total register yang dikonfigurasi
- **Device ID** - ID unik perangkat

### 7.3 Menambah Perangkat Baru

1. Tap tombol **[+]** di pojok kanan atas
2. Isi form **Form Setup Device**:

#### Untuk Modbus RTU:

```
┌─────────────────────────────────┐
│  ←      Form Setup Device       │
├─────────────────────────────────┤
│  Device Name *                  │
│  ┌─────────────────────────────┐│
│  │ Enter the device name       ││
│  └─────────────────────────────┘│
│                                 │
│  Slave ID *                     │
│  ┌─────────────────────────────┐│
│  │ Enter the slave ID          ││
│  └─────────────────────────────┘│
├─────────────────────────────────┤
│  | 📡 Protocol Selection        │
├─────────────────────────────────┤
│  ┌─────────────────────────────┐│
│  │ 🎛️ Modbus RTU          ✓   ││
│  │    Serial communication     ││
│  └─────────────────────────────┘│
│  ┌─────────────────────────────┐│
│  │ 🔌 Modbus TCP          ○   ││
│  │    Ethernet/IP              ││
│  └─────────────────────────────┘│
├─────────────────────────────────┤
│  | 🎛️ Modbus RTU Configuration  │
├─────────────────────────────────┤
│  Choose Serial Port *           │
│  Choose Baudrate *              │
│  Choose Bit Data *              │
│  Choose Parity *                │
│  Choose Stop Bit *              │
├─────────────────────────────────┤
│  | ⚙️ Advanced Settings         │
├─────────────────────────────────┤
│  Retry Count *                  │
│  Connect Timeout * (ms)         │
│  Refresh Rate * (ms)            │
├─────────────────────────────────┤
│  [💾 Save Device Configuration] │
└─────────────────────────────────┘
```

**Parameter Modbus RTU:**

| Parameter | Deskripsi | Contoh |
|-----------|-----------|--------|
| Device Name | Nama identifikasi perangkat | "Temperature_Sensor" |
| Slave ID | ID slave Modbus (1-247) | 1 |
| Serial Port | Port serial (PORT1/PORT2) | PORT1 |
| Baudrate | Kecepatan komunikasi | 9600, 19200, 115200 |
| Bit Data | Jumlah bit data | 8 |
| Parity | Paritas | None, Even, Odd |
| Stop Bit | Stop bit | 1, 2 |
| Retry Count | Jumlah percobaan ulang | 3 |
| Connect Timeout | Timeout koneksi (ms) | 3000 |
| Refresh Rate | Interval polling (ms) | 5000 |

#### Untuk Modbus TCP:

**Parameter Modbus TCP:**

| Parameter | Deskripsi | Contoh |
|-----------|-----------|--------|
| IP Address | Alamat IP perangkat Modbus | 192.168.1.100 |
| Server Port | Port TCP (default 502) | 502 |

3. Tap **Save Device Configuration**

### 7.4 Mengedit Perangkat

1. Tap tombol **Edit** (✏️) pada kartu perangkat
2. Ubah parameter yang diinginkan
3. Tap **Save Device Configuration**

### 7.5 Menghapus Perangkat

1. Tap tombol **Delete** (🗑️) merah
2. Konfirmasi penghapusan

### 7.6 Melihat Detail Perangkat

Tap tombol **View** (👁️) untuk melihat konfigurasi lengkap tanpa edit mode.

### 7.7 Enable/Disable Perangkat

Gunakan toggle switch untuk mengaktifkan atau menonaktifkan perangkat tanpa menghapusnya.

---

## 8. Modbus Configurations

### 8.1 Memilih Device

1. Pilih perangkat dari dropdown **Choose Device**
2. Daftar register akan muncul

### 8.2 Daftar Register

```
┌─────────────────────────────────┐
│  Choose Device                  │
│  RTU_Device_45Regs          ▼   │
├─────────────────────────────────┤
│ ┌─────────────────────────────┐ │
│ │ 🎛️ Temp_Zone_1             │ │
│ │    INT16                    │ │
│ │    📍 Address: 0      [✏️][🗑️]│
│ └─────────────────────────────┘ │
│ ┌─────────────────────────────┐ │
│ │ 🎛️ Temp_Zone_2             │ │
│ │    INT16                    │ │
│ │    📍 Address: 1      [✏️][🗑️]│
│ └─────────────────────────────┘ │
│           ...                   │
└─────────────────────────────────┘
```

### 8.3 Menambah Register Baru

1. Tap tombol **[+]** di pojok kanan atas
2. Isi form **Setup Modbus**

### 8.4 Parameter Register

| Parameter | Deskripsi | Contoh |
|-----------|-----------|--------|
| Data Name | Nama unik register | Temp_Zone_1 |
| Choose Device | Perangkat target | RTU_Device_45Regs |
| Description | Deskripsi opsional | Temperature Zone 1 |
| Unit | Satuan pengukuran | degC, %, bar |
| Choose Function | Function code Modbus | Input Registers (FC 04) |
| Address Modbus | Alamat register (0-based) | 0, 100, 1000 |
| Choose Data Type | Tipe data | INT16, FLOAT32_BE, etc. |
| Scale | Pengali kalibrasi | 0.1, 1, 10 |
| Offset | Offset kalibrasi | 0, -273.15 |

### 8.5 Function Codes

| Function | Kode | Deskripsi |
|----------|------|-----------|
| Coil Status | 01 | Baca status coil (bit) |
| Input Status | 02 | Baca input diskrit (bit) |
| Holding Register | 03 | Baca holding register |
| Input Registers | 04 | Baca input register |

### 8.6 Data Types Lengkap

Aplikasi mendukung **30+ tipe data Modbus**:

#### 16-bit Integer
| Tipe | Deskripsi |
|------|-----------|
| **INT16** | 16-bit Signed Integer (Single Register) |
| **UINT16** | 16-bit Unsigned Integer (Single Register) |

#### Boolean & Binary
| Tipe | Deskripsi |
|------|-----------|
| **BOOL** | Boolean Value (Single Register) |
| **BINARY** | Raw 16-bit Binary Data |

#### 32-bit Signed Integer
| Tipe | Deskripsi |
|------|-----------|
| **INT32_BE** | Big Endian |
| **INT32_LE** | Little Endian |
| **INT32_BE_BS** | Big Endian + Byte Swap |
| **INT32_LE_BS** | Little Endian + Byte Swap |

#### 32-bit Unsigned Integer
| Tipe | Deskripsi |
|------|-----------|
| **UINT32_BE** | Big Endian |
| **UINT32_LE** | Little Endian |
| **UINT32_BE_BS** | Big Endian + Byte Swap |
| **UINT32_LE_BS** | Little Endian + Byte Swap |

#### 32-bit IEEE 754 Float
| Tipe | Deskripsi |
|------|-----------|
| **FLOAT32_BE** | Big Endian |
| **FLOAT32_LE** | Little Endian |
| **FLOAT32_BE_BS** | Big Endian + Byte Swap |
| **FLOAT32_LE_BS** | Little Endian + Byte Swap |

#### 64-bit Signed Integer
| Tipe | Deskripsi |
|------|-----------|
| **INT64_BE** | Big Endian |
| **INT64_LE** | Little Endian |
| **INT64_BE_BS** | Big Endian + Byte Swap |
| **INT64_LE_BS** | Little Endian + Byte Swap |

#### 64-bit Unsigned Integer
| Tipe | Deskripsi |
|------|-----------|
| **UINT64_BE** | Big Endian |
| **UINT64_LE** | Little Endian |
| **UINT64_BE_BS** | Big Endian + Byte Swap |
| **UINT64_LE_BS** | Little Endian + Byte Swap |

#### 64-bit IEEE 754 Double
| Tipe | Deskripsi |
|------|-----------|
| **DOUBLE64_BE** | Big Endian |
| **DOUBLE64_LE** | Little Endian |
| **DOUBLE64_BE_BS** | Big Endian + Byte Swap |
| **DOUBLE64_LE_BS** | Little Endian + Byte Swap |

#### Legacy Types
| Tipe | Deskripsi |
|------|-----------|
| **INT32** | 32-bit Signed (BE) - Legacy Format |
| **FLOAT32** | 32-bit Float (BE) - Legacy Format |
| **STRING** | Text String (UTF-8 Encoded, Variable Length) |

### 8.7 Kalibrasi

Rumus kalibrasi:
```
final_value = (raw_value × scale) + offset
```

**Contoh:**
- Raw value dari sensor: 250
- Scale: 0.1
- Offset: 0
- Final value: 250 × 0.1 + 0 = **25.0°C**

---

## 9. Server Configurations

Menu ini untuk mengkonfigurasi koneksi jaringan dan protokol cloud.

### 9.1 Internet Settings

#### Mode Komunikasi

**Opsi Mode:**
- **WIFI** - Koneksi via WiFi
- **ETH** - Koneksi via Ethernet

#### Konfigurasi WiFi

| Parameter | Deskripsi |
|-----------|-----------|
| WiFi Enabled | Aktifkan/nonaktifkan WiFi |
| WiFi SSID | Nama jaringan WiFi |
| WiFi Password | Password WiFi |

#### Konfigurasi Ethernet

**Dengan DHCP (Automatic IP):**
- Ethernet Enabled: true
- Using DHCP: true

**Dengan Static IP:**

| Parameter | Deskripsi | Contoh |
|-----------|-----------|--------|
| Using DHCP | false untuk static IP | false |
| Static IP | Alamat IP statis | 192.168.1.177 |
| Gateway | Alamat gateway | 192.168.1.1 |
| Subnet | Subnet mask | 255.255.255.0 |

### 9.2 MQTT Protocol Settings

| Parameter | Deskripsi | Contoh |
|-----------|-----------|--------|
| Enabled | Aktifkan/nonaktifkan MQTT | true |
| Broker Address | Alamat MQTT broker | broker.hivemq.com |
| Broker Port | Port broker | 1883 (non-TLS), 8883 (TLS) |
| Client ID | ID unik client | SRT-MGATE-xxx |
| Username | Username autentikasi | (opsional) |
| Password | Password autentikasi | (opsional) |
| Keep Alive | Interval keep-alive (detik) | 60 |
| Clean Session | Mulai dengan session bersih | true |
| Use TLS | Enkripsi SSL/TLS | false |

### 9.3 Topic Configuration

#### Default Mode

Mode sederhana dengan satu topic untuk semua data:
- Publish Topic (contoh: `v1/devices/me/telemetry/gwsrt`)
- Subscribe Topic (opsional, untuk kontrol)
- Publish Interval dengan unit (ms, s, min)

#### Customize Mode

Mode advanced dengan multiple topic dan pemilihan register spesifik:
- Buat multiple topic dengan tombol **Add Topic**
- Setiap topic dapat memiliki:
  - Topic Name
  - Interval terpisah
  - Pilihan device dan register spesifik
- Tap device untuk expand dan pilih register yang diinginkan
- Register yang dipilih ditandai dengan ✓

### 9.4 HTTP Protocol Settings

| Parameter | Deskripsi | Contoh |
|-----------|-----------|--------|
| Enabled | Aktifkan HTTP | true |
| Endpoint URL | URL API tujuan | https://api.example.com/data |
| Method | HTTP method | POST, PUT |
| Body Format | Format body | json |
| Timeout | Timeout request (ms) | 5000 |
| Retry Count | Jumlah retry | 3 |
| Publish Interval | Interval kirim data | 5s, 60s, 5min |
| Custom Headers | Header tambahan | Authorization, Content-Type |

---

## 10. Logging Configurations

### 10.1 Logging Retention

Atur berapa lama log disimpan:
- **1 Week** - Keep logs for 1 week
- **1 Month** - Keep logs for 1 month
- **3 Months** - Keep logs for 3 months

### 10.2 Logging Interval

Atur seberapa sering log ditulis:
- **5 Minutes** - Log every 5 minutes
- **10 Minutes** - Log every 10 minutes
- **30 Minutes** - Log every 30 minutes

### 10.3 Simpan Konfigurasi

Tap **Update Logging Configuration** untuk menyimpan perubahan.

---

## 11. Device Status

### 11.1 Informasi Status

Menu ini menampilkan:
- **Firmware** - Versi firmware yang berjalan
- **SD Card Info** - Informasi storage SD card
- **Update Firmware** - Fitur OTA update

### 11.2 Update Firmware (OTA)

Untuk memperbarui firmware gateway:
1. Tap **Update Firmware**
2. Pilih file firmware (.bin)
3. Tunggu proses update selesai
4. Gateway akan restart otomatis

> **Peringatan:** Jangan matikan gateway atau tutup aplikasi selama proses update!

---

## 12. Device Settings

### 12.1 Menu Settings

- **Import Config** - Restore konfigurasi dari file backup (.json)
- **Download All Config** - Export semua konfigurasi ke file
- **Clear Configuration** - Factory reset

### 12.2 Import Config

1. Tap **Import Config**
2. Pilih file JSON dari penyimpanan
3. Konfirmasi import
4. Gateway akan memuat konfigurasi baru

**Format file yang didukung:** `.json`

### 12.3 Download All Config (Backup)

1. Tap **Download All Config**
2. File akan disimpan ke: `Documents/GatewayConfig/backup/`
3. Nama file: `config_backup_[timestamp].json`

**Data yang di-backup:**
- Konfigurasi perangkat Modbus
- Konfigurasi register
- Pengaturan jaringan
- Pengaturan MQTT/HTTP
- Pengaturan logging

### 12.4 Clear Configuration (Factory Reset)

1. Tap **Clear Configuration**
2. Konfirmasi penghapusan
3. Semua data akan dihapus
4. Gateway akan restart

> **Peringatan:** Tindakan ini tidak dapat dibatalkan! Pastikan sudah melakukan backup sebelumnya.

---

## 13. Pengaturan Aplikasi

### 13.1 Settings Screen

Menu pengaturan aplikasi terdiri dari:

**Account:**
- Profile - Kelola informasi akun

**About:**
- About Product - Informasi produk gateway
- About App - Informasi aplikasi, versi, changelog

**Other:**
- Contact Us - Hubungi tim support
- Logout - Keluar dari akun

---

## 14. Troubleshooting

### 14.1 Masalah Koneksi BLE

| Masalah | Solusi |
|---------|--------|
| Gateway tidak ditemukan saat scanning | 1. Pastikan BLE gateway aktif (tekan tombol di mode production)<br>2. Dekatkan smartphone ke gateway (max 10m)<br>3. Restart BLE di smartphone<br>4. Pastikan permission Bluetooth & Location diizinkan |
| Koneksi terputus tiba-tiba | 1. Periksa jarak (maksimal 10 meter)<br>2. Hindari gangguan WiFi 2.4GHz<br>3. Restart aplikasi |
| Pairing gagal | 1. Forget device di Bluetooth settings<br>2. Restart gateway<br>3. Coba pairing ulang |

### 14.2 Masalah Konfigurasi

| Masalah | Solusi |
|---------|--------|
| Konfigurasi tidak tersimpan | 1. Pastikan koneksi BLE stabil<br>2. Tunggu konfirmasi "Saved" muncul<br>3. Coba kembali dengan koneksi lebih dekat |
| Register tidak terbaca | 1. Periksa alamat register (0-based)<br>2. Verifikasi function code<br>3. Periksa koneksi Modbus fisik |

### 14.3 Masalah Jaringan

| Masalah | Solusi |
|---------|--------|
| WiFi tidak terhubung | 1. Verifikasi SSID dan password<br>2. Pastikan router menyala<br>3. Periksa jarak ke router |
| Ethernet tidak aktif | 1. Periksa kabel LAN<br>2. Verifikasi konfigurasi IP<br>3. Cek DHCP server |
| MQTT tidak terkoneksi | 1. Verifikasi broker address dan port<br>2. Periksa username/password<br>3. Cek firewall |

### 14.4 Reset dan Recovery

Jika gateway tidak responsif:
1. **Soft Reset:** Tap "Clear Configuration" di Device Settings
2. **Hard Reset:** Tekan dan tahan tombol reset di gateway selama 10 detik

---

## 15. FAQ

### Q: Berapa jarak maksimal koneksi BLE?
**A:** Sekitar 10 meter dalam ruangan tanpa halangan. Jarak efektif bisa berkurang jika ada banyak gangguan WiFi 2.4GHz.

### Q: Apakah bisa mengkonfigurasi gateway tanpa internet?
**A:** Ya, konfigurasi via BLE tidak memerlukan internet. Gateway hanya butuh internet untuk mengirim data ke cloud (MQTT/HTTP).

### Q: Berapa banyak perangkat Modbus yang didukung?
**A:** Gateway mendukung hingga 247 perangkat Modbus (sesuai standar Modbus) dengan total ribuan register.

### Q: Apakah data aman saat dikirim ke cloud?
**A:** Ya, jika TLS diaktifkan pada MQTT (port 8883) atau menggunakan HTTPS untuk HTTP.

### Q: Bagaimana cara update firmware?
**A:** Gunakan fitur "Update Firmware" di menu Status. Pilih file .bin dan tunggu proses selesai.

### Q: Apakah bisa backup konfigurasi?
**A:** Ya, gunakan "Download All Config" di Device Settings. File JSON akan tersimpan di folder Documents.

### Q: Apa beda Default Mode dan Customize Mode di MQTT?
**A:**
- **Default Mode:** Semua data dikirim ke satu topic dengan interval sama
- **Customize Mode:** Bisa membuat multiple topic dengan register dan interval berbeda

### Q: Gateway saya tidak muncul di scan, apa yang harus dilakukan?
**A:**
1. Pastikan BLE gateway aktif
2. Di mode Production, tekan tombol untuk mengaktifkan BLE
3. Pastikan Bluetooth dan Location permission diizinkan di smartphone
4. Coba restart aplikasi

### Q: Apakah aplikasi mendukung iOS?
**A:** Ya, aplikasi dibangun dengan Flutter dan mendukung Android 5.0+ dan iOS 12+.

### Q: Apa itu Byte Swap pada data type?
**A:** Byte Swap (BS) menukar urutan byte dalam setiap word. Beberapa perangkat Modbus menggunakan format ini. Coba tanpa BS terlebih dahulu, jika data tidak benar, gunakan versi BS.

---

## 16. Informasi Teknis

### 16.1 Format Response Gateway

Response dari gateway mengikuti format JSON:
```json
{
  "status": "ok",
  "message": "Successfully get data",
  "type": "device",
  "config": []
}
```

### 16.2 Source Code

Aplikasi ini open source dan tersedia di:
- **GitHub:** [dickykhusnaedy/suriota_mobile_app](https://github.com/dickykhusnaedy/suriota_mobile_app)

### 16.3 Dependencies Utama

| Package | Version | Fungsi |
|---------|---------|--------|
| flutter_blue_plus | >= 1.35.3 | BLE communication |
| get | >= 4.6.6 | State management |
| go_router | >= 16.2.1 | Navigation |
| file_picker | >= 8.1.6 | File selection |
| path_provider | >= 2.1.5 | Storage access |

---

## Kontak Support

**SURIOTA R&D Team**
- Email: support@suriota.com
- Website: www.suriota.com
- GitHub Gateway: [GifariKemal/GatewaySuriotaPOC](https://github.com/GifariKemal/GatewaySuriotaPOC)
- GitHub App: [dickykhusnaedy/suriota_mobile_app](https://github.com/dickykhusnaedy/suriota_mobile_app)

---

**Document Version:** 2.0
**App Version:** 1.0.0
**Last Updated:** November 30, 2025
**Author:** SURIOTA Documentation Team

---

*Built with love by the SURIOTA Team - Industrial IoT Solutions*
