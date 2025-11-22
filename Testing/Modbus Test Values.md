# Modbus Test Values untuk SURIOTA Gateway

Dokumen ini berisi daftar nilai yang direkomendasikan untuk pengujian berbagai tipe data Modbus pada firmware SURIOTA Gateway. Untuk setiap tipe data dan nilai yang diuji, disediakan:
*   **Set Value:** Nilai numerik atau floating-point yang ingin Anda tulis ke perangkat Modbus.
*   **Modbus Registers (R1, R2, ...):** Representasi heksadesimal dari register 16-bit yang harus Anda tulis ke perangkat Modbus. Ini adalah nilai yang akan dibaca oleh gateway Anda. Urutan register ini mengikuti urutan pembacaan Modbus (R1 adalah register pertama, R2 kedua, dst.).
*   **Expected Read Value:** Nilai yang diharapkan dibaca oleh gateway Anda setelah pemrosesan.

**Catatan Penting:**

*   **Endianness Register 16-bit:** Setiap register 16-bit (R1, R2, dll.) diasumsikan dalam format Big-Endian (MSB pertama).
*   **Presisi Floating-Point:** Untuk `DOUBLE64` dan `INT64`/`UINT64` yang sangat besar, nilai yang dibaca mungkin sedikit berbeda dari `Set Value` karena konversi presisi floating-point. Ini adalah batasan yang telah didiskusikan.

---

## Test Cases

### `INT16`

| Set Value | Modbus Registers | Expected Read Value |
| --------- | ---------------- | ------------------- |
| 0         | 0x0000           | 0                   |
| 1         | 0x0001           | 1                   |
| -1        | 0xFFFF           | -1                  |
| 32767     | 0x7FFF           | 32767               |
| -32768    | 0x8000           | -32768              |

### `UINT16`

| Set Value | Modbus Registers | Expected Read Value |
| --------- | ---------------- | ------------------- |
| 0         | 0x0000           | 0                   |
| 1         | 0x0001           | 1                   |
| 65535     | 0xFFFF           | 65535               |

### `INT32_BE` (Big-Endian)

| Set Value   | Modbus Registers | Expected Read Value |
| ----------- | ---------------- | ------------------- |
| 0           | 0x0000, 0x0000   | 0                   |
| 1           | 0x0000, 0x0001   | 1                   |
| -1          | 0xFFFF, 0xFFFF   | -1                  |
| 123456789   | 0x075B, 0xCDE5   | 123456789           |
| -123456789  | 0xF8A4, 0x321B   | -123456789          |
| 2147483647  | 0x7FFF, 0xFFFF   | 2147483647          |
| -2147483648 | 0x8000, 0x0000   | -2147483648         |

### `INT32_LE` (Little-Endian - Full Byte Swap)

| Set Value   | Modbus Registers | Expected Read Value |
| ----------- | ---------------- | ------------------- |
| 0           | 0x0000, 0x0000   | 0                   |
| 1           | 0x0001, 0x0000   | 1                   |
| -1          | 0xFFFF, 0xFFFF   | -1                  |
| 123456789   | 0xCDE5, 0x075B   | 123456789           |
| -123456789  | 0x321B, 0xF8A4   | -123456789          |
| 2147483647  | 0xFFFF, 0x7FFF   | 2147483647          |
| -2147483648 | 0x0000, 0x8000   | -2147483648         |

### `INT32_BE_BS` (Byte Swap - BE with Byte Swap)

| Set Value   | Modbus Registers | Expected Read Value |
| ----------- | ---------------- | ------------------- |
| 0           | 0x0000, 0x0000   | 0                   |
| 1           | 0x0100, 0x0000   | 1                   |
| -1          | 0xFFFF, 0xFFFF   | -1                  |
| 123456789   | 0x5B07, 0xE5CD   | 123456789           |
| -123456789  | 0xA4F8, 0x1B32   | -123456789          |
| 2147483647  | 0xFF7F, 0xFFFF   | 2147483647          |
| -2147483648 | 0x0080, 0x0000   | -2147483648         |

### `INT32_LE_BS` (Word Swap - LE with Word Swap)

| Set Value   | Modbus Registers | Expected Read Value |
| ----------- | ---------------- | ------------------- |
| 0           | 0x0000, 0x0000   | 0                   |
| 1           | 0x0001, 0x0000   | 1                   |
| -1          | 0xFFFF, 0xFFFF   | -1                  |
| 123456789   | 0xCDE5, 0x075B   | 123456789           |
| -123456789  | 0x321B, 0xF8A4   | -123456789          |
| 2147483647  | 0xFFFF, 0x7FFF   | 2147483647          |
| -2147483648 | 0x0000, 0x8000   | -2147483648         |

### `UINT32_BE` (Big-Endian)

| Set Value  | Modbus Registers | Expected Read Value |
| ---------- | ---------------- | ------------------- |
| 0          | 0x0000, 0x0000   | 0                   |
| 1          | 0x0000, 0x0001   | 1                   |
| 123456789  | 0x075B, 0xCDE5   | 123456789           |
| 4294967295 | 0xFFFF, 0xFFFF   | 4294967295          |

### `UINT32_LE` (Little-Endian - Full Byte Swap)

| Set Value  | Modbus Registers | Expected Read Value |
| ---------- | ---------------- | ------------------- |
| 0          | 0x0000, 0x0000   | 0                   |
| 1          | 0x0001, 0x0000   | 1                   |
| 123456789  | 0xCDE5, 0x075B   | 123456789           |
| 4294967295 | 0xFFFF, 0xFFFF   | 4294967295          |

### `UINT32_BE_BS` (Byte Swap - BE with Byte Swap)

| Set Value  | Modbus Registers | Expected Read Value |
| ---------- | ---------------- | ------------------- |
| 0          | 0x0000, 0x0000   | 0                   |
| 1          | 0x0100, 0x0000   | 1                   |
| 123456789  | 0x5B07, 0xE5CD   | 123456789           |
| 4294967295 | 0xFFFF, 0xFFFF   | 4294967295          |

### `UINT32_LE_BS` (Word Swap - LE with Word Swap)

| Set Value  | Modbus Registers | Expected Read Value |
| ---------- | ---------------- | ------------------- |
| 0          | 0x0000, 0x0000   | 0                   |
| 1          | 0x0001, 0x0000   | 1                   |
| 123456789  | 0xCDE5, 0x075B   | 123456789           |
| 4294967295 | 0xFFFF, 0xFFFF   | 4294967295          |

### `FLOAT32_BE` (Big-Endian)

| Set Value   | Modbus Registers | Expected Read Value |
| ----------- | ---------------- | ------------------- |
| 0.0         | 0x0000, 0x0000   | 0.0                 |
| 1.0         | 0x3F80, 0x0000   | 1.0                 |
| -1.0        | 0xBF80, 0x0000   | -1.0                |
| 123.45      | 0x42F6, 0xE666   | 123.45              |
| -123.45     | 0xC2F6, 0xE666   | -123.45             |
| 0.000123    | 0x3800, 0x0000   | 0.000123            |
| -98765.4321 | 0xC7C1, 0x871C   | -98765.4321         |

### `FLOAT32_LE` (Little-Endian - Full Byte Swap)

| Set Value   | Modbus Registers | Expected Read Value |
| ----------- | ---------------- | ------------------- |
| 0.0         | 0x0000, 0x0000   | 0.0                 |
| 1.0         | 0x0000, 0x3F80   | 1.0                 |
| -1.0        | 0x0000, 0xBF80   | -1.0                |
| 123.45      | 0xE666, 0x42F6   | 123.45              |
| -123.45     | 0xE666, 0xC2F6   | -123.45             |
| 0.000123    | 0x0000, 0x3800   | 0.000123            |
| -98765.4321 | 0x871C, 0xC7C1   | -98765.4321         |

### `FLOAT32_BE_BS` (Byte Swap - BE with Byte Swap)

| Set Value   | Modbus Registers | Expected Read Value |
| ----------- | ---------------- | ------------------- |
| 0.0         | 0x0000, 0x0000   | 0.0                 |
| 1.0         | 0x803F, 0x0000   | 1.0                 |
| -1.0        | 0x80BF, 0x0000   | -1.0                |
| 123.45      | 0xF642, 0x66E6   | 123.45              |
| -123.45     | 0xF6C2, 0x66E6   | -123.45             |
| 0.000123    | 0x0038, 0x0000   | 0.000123            |
| -98765.4321 | 0xC1C7, 0x1C87   | -98765.4321         |

### `FLOAT32_LE_BS` (Word Swap - LE with Word Swap)

| Set Value   | Modbus Registers | Expected Read Value |
| ----------- | ---------------- | ------------------- |
| 0.0         | 0x0000, 0x0000   | 0.0                 |
| 1.0         | 0x0000, 0x3F80   | 1.0                 |
| -1.0        | 0x0000, 0xBF80   | -1.0                |
| 123.45      | 0xE666, 0x42F6   | 123.45              |
| -123.45     | 0xE666, 0xC2F6   | -123.45             |
| 0.000123    | 0x0000, 0x3800   | 0.000123            |
| -98765.4321 | 0x871C, 0xC7C1   | -98765.4321         |

### `INT64_BE` (Big-Endian)

| Set Value            | Modbus Registers               | Expected Read Value  |
| -------------------- | ------------------------------ | -------------------- |
| 0                    | 0x0000, 0x0000, 0x0000, 0x0000 | 0                    |
| 1                    | 0x0000, 0x0000, 0x0000, 0x0001 | 1                    |
| -1                   | 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF | -1                   |
| 123456789012345      | 0x0070, 0x488C, 0xB149, 0x4D29 | 123456789012345      |
| -123456789012345     | 0xFF8F, 0xB773, 0x4EB6, 0xB2D7 | -123456789012345     |
| 9223372036854775807  | 0x7FFF, 0xFFFF, 0xFFFF, 0xFFFF | 9223372036854775807  |
| -9223372036854775808 | 0x8000, 0x0000, 0x0000, 0x0000 | -9223372036854775808 |

### `INT64_LE` (Little-Endian - Full Byte Swap)

| Set Value            | Modbus Registers               | Expected Read Value  |
| -------------------- | ------------------------------ | -------------------- |
| 0                    | 0x0000, 0x0000, 0x0000, 0x0000 | 0                    |
| 1                    | 0x0001, 0x0000, 0x0000, 0x0000 | 1                    |
| -1                   | 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF | -1                   |
| 123456789012345      | 0x294D, 0x49B1, 0x8C48, 0x7000 | 123456789012345      |
| -123456789012345     | 0xB2D7, 0x4EB6, 0xB773, 0xFF8F | -123456789012345     |
| 9223372036854775807  | 0xFFFF, 0xFFFF, 0xFFFF, 0x7FFF | 9223372036854775807  |
| -9223372036854775808 | 0x0000, 0x0000, 0x0000, 0x8000 | -9223372036854775808 |

### `INT64_BE_BS` (Byte Swap - BE with Byte Swap)

| Set Value            | Modbus Registers               | Expected Read Value  |
| -------------------- | ------------------------------ | -------------------- |
| 0                    | 0x0000, 0x0000, 0x0000, 0x0000 | 0                    |
| 1                    | 0x0000, 0x0000, 0x0000, 0x0100 | 1                    |
| -1                   | 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF | -1                   |
| 123456789012345      | 0x7000, 0x8C48, 0x49B1, 0x294D | 123456789012345      |
| -123456789012345     | 0x8F77, 0xB7B8, 0xB64E, 0xD7B2 | -123456789012345     |
| 9223372036854775807  | 0xFFFF, 0x7FFF, 0xFFFF, 0xFFFF | 9223372036854775807  |
| -9223372036854775808 | 0x0000, 0x8000, 0x0000, 0x0000 | -9223372036854775808 |

### `INT64_LE_BS` (Word Swap - LE with Word Swap)

| Set Value            | Modbus Registers               | Expected Read Value  |
| -------------------- | ------------------------------ | -------------------- |
| 0                    | 0x0000, 0x0000, 0x0000, 0x0000 | 0                    |
| 1                    | 0x0001, 0x0000, 0x0000, 0x0000 | 1                    |
| -1                   | 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF | -1                   |
| 123456789012345      | 0x4D29, 0xB149, 0x488C, 0x0070 | 123456789012345      |
| -123456789012345     | 0xB2D7, 0x4EB6, 0xB773, 0xFF8F | -123456789012345     |
| 9223372036854775807  | 0xFFFF, 0xFFFF, 0xFFFF, 0x7FFF | 9223372036854775807  |
| -9223372036854775808 | 0x0000, 0x0000, 0x0000, 0x8000 | -9223372036854775808 |

### `UINT64_BE` (Big-Endian)

| Set Value            | Modbus Registers               | Expected Read Value  |
| -------------------- | ------------------------------ | -------------------- |
| 0                    | 0x0000, 0x0000, 0x0000, 0x0000 | 0                    |
| 1                    | 0x0000, 0x0000, 0x0000, 0x0001 | 1                    |
| 123456789012345      | 0x0070, 0x488C, 0xB149, 0x4D29 | 123456789012345      |
| 18446744073709551615 | 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF | 18446744073709551615 |

### `UINT64_LE` (Little-Endian - Full Byte Swap)

| Set Value            | Modbus Registers               | Expected Read Value  |
| -------------------- | ------------------------------ | -------------------- |
| 0                    | 0x0000, 0x0000, 0x0000, 0x0000 | 0                    |
| 1                    | 0x0001, 0x0000, 0x0000, 0x0000 | 1                    |
| 123456789012345      | 0x294D, 0x49B1, 0x8C48, 0x7000 | 123456789012345      |
| 18446744073709551615 | 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF | 18446744073709551615 |

### `UINT64_BE_BS` (Byte Swap - BE with Byte Swap)

| Set Value            | Modbus Registers               | Expected Read Value  |
| -------------------- | ------------------------------ | -------------------- |
| 0                    | 0x0000, 0x0000, 0x0000, 0x0000 | 0                    |
| 1                    | 0x0000, 0x0000, 0x0000, 0x0100 | 1                    |
| 123456789012345      | 0x7000, 0x8C48, 0x49B1, 0x294D | 123456789012345      |
| 18446744073709551615 | 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF | 18446744073709551615 |

### `UINT64_LE_BS` (Word Swap - LE with Word Swap)

| Set Value            | Modbus Registers               | Expected Read Value  |
| -------------------- | ------------------------------ | -------------------- |
| 0                    | 0x0000, 0x0000, 0x0000, 0x0000 | 0                    |
| 1                    | 0x0001, 0x0000, 0x0000, 0x0000 | 1                    |
| 123456789012345      | 0x4D29, 0xB149, 0x488C, 0x0070 | 123456789012345      |
| 18446744073709551615 | 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF | 18446744073709551615 |

### `DOUBLE64_BE` (Big-Endian)

| Set Value   | Modbus Registers               | Expected Read Value |
| ----------- | ------------------------------ | ------------------- |
| 0.0         | 0x0000, 0x0000, 0x0000, 0x0000 | 0.0                 |
| 1.0         | 0x3FF0, 0x0000, 0x0000, 0x0000 | 1.0                 |
| -1.0        | 0xBFF0, 0x0000, 0x0000, 0x0000 | -1.0                |
| 123456.789  | 0x40FE, 0xDD2F, 0x1A9F, 0xBE77 | 123456.789          |
| -123456.789 | 0xC0FE, 0xDD2F, 0x1A9F, 0xBE77 | -123456.789         |
| 1.23e+30    | 0x4841, 0x0000, 0x0000, 0x0000 | 1.23e+30            |
| -4.56e-20   | 0xBE30, 0x0000, 0x0000, 0x0000 | -4.56e-20           |

### `DOUBLE64_LE` (Little-Endian - Full Byte Swap)

| Set Value   | Modbus Registers               | Expected Read Value |
| ----------- | ------------------------------ | ------------------- |
| 0.0         | 0x0000, 0x0000, 0x0000, 0x0000 | 0.0                 |
| 1.0         | 0x0000, 0x0000, 0x0000, 0x3FF0 | 1.0                 |
| -1.0        | 0x0000, 0x0000, 0x0000, 0xBFF0 | -1.0                |
| 123456.789  | 0x77BE, 0x9F1A, 0x2FDD, 0xFEC0 | 123456.789          |
| -123456.789 | 0x77BE, 0x9F1A, 0x2FDD, 0xC0FE | -123456.789         |
| 1.23e+30    | 0x0000, 0x0000, 0x0000, 0x4841 | 1.23e+30            |
| -4.56e-20   | 0x0000, 0x0000, 0x0000, 0xBE30 | -4.56e-20           |

### `DOUBLE64_BE_BS` (Byte Swap - BE with Byte Swap)

| Set Value   | Modbus Registers               | Expected Read Value |
| ----------- | ------------------------------ | ------------------- |
| 0.0         | 0x0000, 0x0000, 0x0000, 0x0000 | 0.0                 |
| 1.0         | 0xF03F, 0x0000, 0x0000, 0x0000 | 1.0                 |
| -1.0        | 0xF0BF, 0x0000, 0x0000, 0x0000 | -1.0                |
| 123456.789  | 0xFEC0, 0x2FDD, 0x9F1A, 0x77BE | 123456.789          |
| -123456.789 | 0xFEC0, 0x2FDD, 0x9F1A, 0x77BE | -123456.789         |
| 1.23e+30    | 0x4108, 0x0000, 0x0000, 0x0000 | 1.23e+30            |
| -4.56e-20   | 0x30BE, 0x0000, 0x0000, 0x0000 | -4.56e-20           |

### `DOUBLE64_LE_BS` (Word Swap - LE with Word Swap)

| Set Value   | Modbus Registers               | Expected Read Value |
| ----------- | ------------------------------ | ------------------- |
| 0.0         | 0x0000, 0x0000, 0x0000, 0x0000 | 0.0                 |
| 1.0         | 0x0000, 0x0000, 0x3FF0, 0x0000 | 1.0                 |
| -1.0        | 0x0000, 0x0000, 0xBFF0, 0x0000 | -1.0                |
| 123456.789  | 0xBE77, 0x1A9F, 0xDD2F, 0x40FE | 123456.789          |
| -123456.789 | 0xBE77, 0x1A9F, 0xDD2F, 0xC0FE | -123456.789         |
| 1.23e+30    | 0x0000, 0x0000, 0x4841, 0x0000 | 1.23e+30            |
| -4.56e-20   | 0x0000, 0x0000, 0xBE30, 0x0000 | -4.56e-20           |

---

## Catatan Implementasi

### Data Type Mapping

Setiap data type memiliki 4 varian endianness:

- **BE (Big-Endian):** Register dalam urutan standar, MSB pertama
- **LE (Little-Endian):** Semua byte di-reverse
- **BS (Byte Swap):** Byte dalam setiap register di-swap, urutan register tetap BE

### Presisi dan Batasan

1. **INT16/UINT16:** Presisi penuh (16-bit integer)
2. **INT32/UINT32:** Presisi penuh (32-bit integer)
3. **FLOAT32:** IEEE 754 single-precision (32-bit)
4. **INT64/UINT64:** Konversi ke double (presisi terbatas untuk nilai > 2^53)
5. **FLOAT64:** IEEE 754 double-precision (64-bit)

### Testing Protocol

1. Gunakan Modbus Slave simulator (https://www.modbustools.com/)
2. Set register values sesuai dengan "Modbus Registers" di atas
3. Baca nilai dari gateway menggunakan Modbus RTU atau TCP
4. Verifikasi bahwa "Expected Read Value" sesuai dengan yang dibaca
5. Catat hasil testing untuk setiap data type variant
