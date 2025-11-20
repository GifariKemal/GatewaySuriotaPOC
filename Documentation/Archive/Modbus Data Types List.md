# Dokumentasi Tipe Data Modbus Gateway SURIOTA

---
**Versi:** 2.0.0
**Tanggal & Waktu:** Jumat, 24 Oktober 2025 10:00:00 WIB
**Penulis:** Gifari Kemal Suryo (diperbarui oleh Gemini AI)
**Tujuan:** Team R&D SURIOTA
**Hak Cipta:** © 2025 PT Surya Inovasi Prioritas (SURIOTA). Hak cipta dilindungi undang-undang.
---

Dokumen ini menguraikan tipe data Modbus yang didukung oleh firmware SURIOTA Gateway, termasuk berbagai varian endianness dan word swap untuk nilai multi-register. String `dataType` ini digunakan dalam konfigurasi register melalui BLE CRUD API.

## Format Tipe Data (Versi 2.0)

Firmware telah diperbarui untuk menggunakan format `dataType` yang lebih jelas dan standar. Format baru ini menggunakan huruf kapital dan garis bawah (`_`) sebagai pemisah.

**Struktur Format:** `TIPE_DATA_[VARIAN_ENDIANNESS]`

- **`TIPE_DATA`**: Tipe dasar seperti `INT32`, `FLOAT32`, `UINT64`, dll.
- **`VARIAN_ENDIANNESS`**: Opsional, menjelaskan urutan byte/word.

## Tipe Data Dasar yang Didukung

*   `INT16`: Integer Bertanda 16-bit.
*   `UINT16`: Integer Tak Bertanda 16-bit.
*   `INT32`: Integer Bertanda 32-bit.
*   `UINT32`: Integer Tak Bertanda 32-bit.
*   `FLOAT32`: Floating Point IEEE 754 32-bit.
*   `INT64`: Integer Bertanda 64-bit.
*   `UINT64`: Integer Tak Bertanda 64-bit.
*   `DOUBLE64`: Floating Point IEEE 754 64-bit (Presisi Ganda).
    - **Catatan**: Nama `float64` telah diganti menjadi `DOUBLE64` untuk merefleksikan penggunaan tipe data `double` presisi ganda di dalam firmware, yang memberikan akurasi lebih tinggi.

## Varian Endianness dan Word Swap

Untuk tipe data 32-bit dan 64-bit, firmware mendukung 4 permutasi urutan byte/word yang paling umum digunakan di industri. Mari kita asumsikan sebuah nilai memiliki urutan byte dari yang paling signifikan (MSB) ke yang paling tidak signifikan (LSB) sebagai `A, B, C, D` untuk 32-bit, dan `A, B, C, D, E, F, G, H` untuk 64-bit.

| Akhiran (`_SUFFIX`) | Nama Umum                    | Urutan Byte (32-bit) | Urutan Byte (64-bit) | Deskripsi                                                  |
| :------------------ | :--------------------------- | :------------------- | :------------------- | :--------------------------------------------------------- |
| `_BE`               | Big-Endian                   | `ABCD`               | `ABCDEFGH`           | Urutan standar, MSB pertama. Tidak ada swap.               |
| `_LE`               | Little-Endian                | `DCBA`               | `HGFEDCBA`           | Urutan byte dibalik sepenuhnya.                            |
| `_BE_BS`            | Big-Endian with Byte Swap    | `BADC`               | `BADCFEHG`           | Urutan word tetap, tapi byte di dalam setiap word ditukar. |
| `_LE_BS`            | Little-Endian with Word Swap | `CDAB`               | `CDABGHEF`           | Urutan word dibalik, byte di dalam word tetap.             |

**Catatan Penting tentang `INT64_BE_BS`**: Implementasi format ini pada beberapa perangkat Modbus slave bisa jadi tidak standar. Firmware SURIOTA mengimplementasikan pola `BADC`-extended yang paling logis. Jika Anda menemukan hasil yang tidak sesuai, kemungkinan besar perangkat slave Anda menggunakan permutasi yang berbeda dan tidak terdokumentasi.

### Daftar Lengkap String `dataType` yang Didukung

Berikut adalah daftar lengkap string `dataType` yang dapat Anda gunakan dalam konfigurasi register Anda:

*   `INT16`
*   `UINT16`
*   `INT32_BE`
*   `INT32_LE`
*   `INT32_BE_BS`
*   `INT32_LE_BS`
*   `UINT32_BE`
*   `UINT32_LE`
*   `UINT32_BE_BS`
*   `UINT32_LE_BS`
*   `FLOAT32_BE`
*   `FLOAT32_LE`
*   `FLOAT32_BE_BS`
*   `FLOAT32_LE_BS`
*   `INT64_BE`
*   `INT64_LE`
*   `INT64_BE_BS`
*   `INT64_LE_BS`
*   `UINT64_BE`
*   `UINT64_LE`
*   `UINT64_BE_BS`
*   `UINT64_LE_BS`
*   `DOUBLE64_BE`
*   `DOUBLE64_LE`
*   `DOUBLE64_BE_BS`
*   `DOUBLE64_LE_BS`

## Contoh Penggunaan dalam Konfigurasi Register

Saat membuat atau memperbarui register melalui BLE CRUD API, gunakan format `dataType` baru seperti yang dijelaskan di atas.

### Contoh: Membuat `FLOAT32_LE_BS`

```json
{
  "op": "create",
  "type": "register",
  "device_id": "D123ABC",
  "config": {
    "address": 40001,
    "register_name": "MOTOR_RPM",
    "type": "Holding Register",
    "function_code": 3,
    "data_type": "FLOAT32_LE_BS",
    "description": "Motor RPM (Little-endian with Word Swap)",
    "refresh_rate_ms": 1000
  }
}
```

### Contoh: Membuat `DOUBLE64_BE`

```json
{
  "op": "create",
  "type": "register",
  "device_id": "D123ABC",
  "config": {
    "address": 40003,
    "register_name": "TOTAL_ENERGY_KWH",
    "type": "Holding Register",
    "function_code": 3,
    "data_type": "DOUBLE64_BE",
    "description": "Total Energy Consumption (Big-endian)",
    "refresh_rate_ms": 5000
  }
}
```
