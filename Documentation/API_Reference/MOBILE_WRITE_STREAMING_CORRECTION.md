# Koreksi Dokumentasi: Write Register + Streaming Integration

**Version:** 1.0.0 | **Date:** January 22, 2026 | **For:** Mobile App Development Team

---

## Tujuan Dokumen

Dokumen ini berisi **koreksi** terhadap dokumentasi mobile app berikut:

- `FLUTTER_WRITE_STREAMING_QUICKSTART.md`
- `FLUTTER_WRITE_STREAMING_GUIDE.md`

Koreksi berdasarkan analisis firmware **SRT-MGATE-1210 v1.3.1** (production).

---

## 1. Error Codes - KOREKSI UTAMA

### Dokumentasi Lama (SALAH):

```dart
switch (errorCode) {
  case 315: return "Register not writable";
  case 316: return "Value out of range";
  case 317: return "Unsupported data type";
  case 318: return "Write failed - check device";
  case 301: return "Device timeout";
  case 305: return "Device not found";
}
```

### Dokumentasi Baru (BENAR):

```dart
switch (errorCode) {
  // Write-specific errors (315-324)
  case 315: return "Mutex timeout - coba lagi";
  case 316: return "Device atau register tidak ditemukan";
  case 317: return "Register read-only (FC2/FC4 tidak bisa ditulis)";
  case 318: return "Register tidak di-enable untuk write";
  case 319: return "Value di bawah batas minimum";
  case 320: return "Value di atas batas maximum";
  case 321: return "Gagal koneksi ke device";
  case 322: return "Function code tidak valid";
  case 323: return "Write timeout - device tidak merespons";
  case 324: return "Response dari device tidak valid";

  // Modbus exception codes (331-334)
  case 331: return "Modbus Exception: Illegal Function";
  case 332: return "Modbus Exception: Illegal Data Address";
  case 333: return "Modbus Exception: Illegal Data Value";
  case 334: return "Modbus Exception: Slave Device Failure";

  // General Modbus errors
  case 301: return "Device timeout - periksa koneksi";
  case 302: return "CRC error - masalah komunikasi";
  case 303: return "Response tidak valid";
  case 305: return "Serial port error";
}
```

### Tabel Referensi Error Codes:

| Code | Konstanta Firmware              | Deskripsi                         | User Action                        |
| ---- | ------------------------------- | --------------------------------- | ---------------------------------- |
| 315  | ERR_MODBUS_WRITE_MUTEX_TIMEOUT  | Mutex acquisition timeout         | Coba lagi dalam beberapa detik     |
| 316  | ERR_MODBUS_WRITE_DEVICE_NOT_FOUND | Device/register tidak ditemukan | Periksa device_id dan register_id  |
| 317  | ERR_MODBUS_WRITE_READONLY       | Register FC2/FC4 (read-only)      | Gunakan register FC1 atau FC3      |
| 318  | ERR_MODBUS_WRITE_NOT_WRITABLE   | Flag writable = false             | Enable writable di konfigurasi     |
| 319  | ERR_MODBUS_WRITE_VALUE_BELOW_MIN | Value < min_value                | Masukkan value >= min_value        |
| 320  | ERR_MODBUS_WRITE_VALUE_ABOVE_MAX | Value > max_value                | Masukkan value <= max_value        |
| 321  | ERR_MODBUS_WRITE_CONNECTION_FAILED | Koneksi TCP/Serial gagal       | Periksa kabel/jaringan             |
| 322  | ERR_MODBUS_WRITE_INVALID_FC     | Function code tidak didukung      | Periksa konfigurasi register       |
| 323  | ERR_MODBUS_WRITE_TIMEOUT        | Device tidak merespons write      | Periksa device hidup dan terhubung |
| 324  | ERR_MODBUS_WRITE_INVALID_RESPONSE | Response CRC/format salah       | Periksa komunikasi serial/TCP      |
| 331  | ERR_MODBUS_EXCEPTION_ILLEGAL_FUNC | Modbus exception 0x01           | Device tidak support operasi ini   |
| 332  | ERR_MODBUS_EXCEPTION_ILLEGAL_ADDR | Modbus exception 0x02           | Alamat register tidak valid        |
| 333  | ERR_MODBUS_EXCEPTION_ILLEGAL_VALUE | Modbus exception 0x03          | Value tidak diterima device        |
| 334  | ERR_MODBUS_EXCEPTION_DEVICE_FAIL | Modbus exception 0x04           | Device internal error              |
| 301  | ERR_MODBUS_DEVICE_TIMEOUT       | Timeout saat polling              | Periksa koneksi device             |

---

## 2. Success Response Format - KOREKSI

### Dokumentasi Lama (SALAH):

```json
{
  "status": "ok",
  "device_id": "D7A3F2",
  "register_id": "R3C8D1",
  "message": "Write successful",
  "data": {
    "register_name": "setpoint_temperature",
    "written_value": 25.5,
    "raw_value": 255,
    "function_code": 6,
    "address": 100
  }
}
```

### Dokumentasi Baru (BENAR):

```json
{
  "status": "ok",
  "device_id": "D7A3F2",
  "register_id": "R3C8D1",
  "value_written": 25.5,
  "raw_value": 255.0,
  "response_time_ms": 45
}
```

### Perubahan yang Diperlukan di Flutter:

```dart
// LAMA (SALAH)
final writtenValue = response['data']['written_value'];

// BARU (BENAR)
final writtenValue = response['value_written'];
final rawValue = response['raw_value'];
final responseTimeMs = response['response_time_ms'];
```

### Model Class yang Benar:

```dart
class WriteSuccessResponse {
  final bool isSuccess;
  final String deviceId;
  final String registerId;
  final double valueWritten;  // PERHATIKAN: "value_written" bukan "written_value"
  final double rawValue;
  final int responseTimeMs;

  WriteSuccessResponse({
    required this.isSuccess,
    required this.deviceId,
    required this.registerId,
    required this.valueWritten,
    required this.rawValue,
    required this.responseTimeMs,
  });

  factory WriteSuccessResponse.fromJson(Map<String, dynamic> json) {
    return WriteSuccessResponse(
      isSuccess: json['status'] == 'ok',
      deviceId: json['device_id'] ?? '',
      registerId: json['register_id'] ?? '',
      valueWritten: (json['value_written'] ?? 0).toDouble(),  // KEY BERBEDA!
      rawValue: (json['raw_value'] ?? 0).toDouble(),
      responseTimeMs: json['response_time_ms'] ?? 0,
    );
  }
}
```

---

## 3. Error Response Format - KOREKSI

### Dokumentasi Lama (SALAH):

```json
{
  "status": "error",
  "error_code": 316,
  "domain": "MODBUS",
  "severity": "ERROR",
  "message": "Value out of range",
  "suggestion": "Enter value between 0.0 and 100.0"
}
```

### Dokumentasi Baru (BENAR):

```json
{
  "status": "error",
  "error": "Value below minimum",
  "error_code": 319,
  "min_value": 0.0,
  "provided_value": -5.0
}
```

### Catatan Penting:

1. Field `"error"` digunakan, **BUKAN** `"message"`
2. Field `"domain"`, `"severity"`, `"suggestion"` **TIDAK ADA**
3. Field tambahan kontekstual mungkin ada (seperti `min_value`, `max_value`, `provided_value`)

### Model Class yang Benar:

```dart
class WriteErrorResponse {
  final bool isError;
  final String errorMessage;  // dari field "error"
  final int errorCode;
  final double? minValue;
  final double? maxValue;
  final double? providedValue;

  WriteErrorResponse({
    required this.isError,
    required this.errorMessage,
    required this.errorCode,
    this.minValue,
    this.maxValue,
    this.providedValue,
  });

  factory WriteErrorResponse.fromJson(Map<String, dynamic> json) {
    return WriteErrorResponse(
      isError: json['status'] == 'error',
      errorMessage: json['error'] ?? 'Unknown error',  // FIELD "error" BUKAN "message"!
      errorCode: json['error_code'] ?? 999,
      minValue: json['min_value']?.toDouble(),
      maxValue: json['max_value']?.toDouble(),
      providedValue: json['provided_value']?.toDouble(),
    );
  }

  /// Get user-friendly message based on error code
  String getUserFriendlyMessage() {
    switch (errorCode) {
      case 315:
        return 'Sistem sibuk, coba lagi.';
      case 316:
        return 'Device atau register tidak ditemukan.';
      case 317:
        return 'Register ini tidak bisa ditulis (read-only).';
      case 318:
        return 'Register belum di-enable untuk write. Aktifkan di pengaturan.';
      case 319:
        return 'Value terlalu kecil. Minimum: ${minValue ?? "N/A"}';
      case 320:
        return 'Value terlalu besar. Maximum: ${maxValue ?? "N/A"}';
      case 321:
        return 'Gagal terhubung ke device. Periksa koneksi.';
      case 322:
        return 'Konfigurasi register tidak valid.';
      case 323:
        return 'Device tidak merespons. Periksa apakah device menyala.';
      case 324:
        return 'Komunikasi error. Periksa koneksi.';
      case 331:
        return 'Device tidak mendukung operasi ini.';
      case 332:
        return 'Alamat register tidak valid.';
      case 333:
        return 'Value tidak diterima oleh device.';
      case 334:
        return 'Device mengalami error internal.';
      case 301:
        return 'Device timeout. Periksa koneksi.';
      default:
        return errorMessage;
    }
  }
}
```

---

## 4. Default Value `writable` - KOREKSI

### Dokumentasi Lama (SALAH):

| Field      | Type    | Required | Default   |
| ---------- | ------- | -------- | --------- |
| `writable` | boolean | No       | **false** |

### Dokumentasi Baru (BENAR):

| Field      | Type    | Required | Default  | Catatan                    |
| ---------- | ------- | -------- | -------- | -------------------------- |
| `writable` | boolean | No       | **true** | Backward compatibility     |

### Implikasi:

- Register **TANPA** field `writable` akan **BISA** ditulis (default true)
- Untuk mencegah write, harus **EKSPLISIT** set `"writable": false`

---

## 5. Unified Response Parser - KODE LENGKAP

```dart
class CommandResponse {
  final bool isSuccess;
  final bool isError;
  final String? message;       // Untuk success: null, untuk error: dari field "error"
  final int? errorCode;
  final String? deviceId;
  final String? registerId;
  final double? valueWritten;
  final double? rawValue;
  final int? responseTimeMs;
  final double? minValue;
  final double? maxValue;
  final double? providedValue;

  CommandResponse({
    required this.isSuccess,
    required this.isError,
    this.message,
    this.errorCode,
    this.deviceId,
    this.registerId,
    this.valueWritten,
    this.rawValue,
    this.responseTimeMs,
    this.minValue,
    this.maxValue,
    this.providedValue,
  });

  factory CommandResponse.fromJson(Map<String, dynamic> json) {
    final status = json['status'] as String?;
    final isSuccess = status == 'ok';
    final isError = status == 'error';

    return CommandResponse(
      isSuccess: isSuccess,
      isError: isError,
      // PENTING: Error message ada di field "error", bukan "message"
      message: json['error'] as String?,
      errorCode: json['error_code'] as int?,
      deviceId: json['device_id'] as String?,
      registerId: json['register_id'] as String?,
      // PENTING: Success value ada di "value_written", bukan "written_value"
      valueWritten: (json['value_written'] as num?)?.toDouble(),
      rawValue: (json['raw_value'] as num?)?.toDouble(),
      responseTimeMs: json['response_time_ms'] as int?,
      minValue: (json['min_value'] as num?)?.toDouble(),
      maxValue: (json['max_value'] as num?)?.toDouble(),
      providedValue: (json['provided_value'] as num?)?.toDouble(),
    );
  }

  /// Get user-friendly error message
  String getUserFriendlyMessage() {
    if (isSuccess) return 'Berhasil menulis value';

    switch (errorCode) {
      case 315: return 'Sistem sibuk, coba lagi dalam beberapa detik.';
      case 316: return 'Device atau register tidak ditemukan.';
      case 317: return 'Register ini read-only dan tidak bisa ditulis.';
      case 318: return 'Register belum diaktifkan untuk write.';
      case 319: return 'Value terlalu kecil (min: ${minValue ?? "?"})';
      case 320: return 'Value terlalu besar (max: ${maxValue ?? "?"})';
      case 321: return 'Gagal terhubung ke device.';
      case 322: return 'Konfigurasi register tidak valid.';
      case 323: return 'Device tidak merespons.';
      case 324: return 'Error komunikasi dengan device.';
      case 331: return 'Device tidak mendukung operasi ini.';
      case 332: return 'Alamat register tidak valid di device.';
      case 333: return 'Value ditolak oleh device.';
      case 334: return 'Device mengalami error internal.';
      case 301: return 'Device timeout.';
      default: return message ?? 'Error tidak dikenal (code: $errorCode)';
    }
  }
}
```

---

## 6. Streaming Commands - TIDAK ADA PERUBAHAN

Format streaming sudah **BENAR**:

```dart
// Start streaming
final startCommand = {
  'op': 'read',
  'type': 'data',
  'device_id': 'D7A3F2',  // Device ID target
};

// Stop streaming
final stopCommand = {
  'op': 'read',
  'type': 'data',
  'device_id': 'stop',  // Literal string "stop"
};
```

---

## 7. Write Command Format - TIDAK ADA PERUBAHAN

Format write command sudah **BENAR**:

```dart
final writeCommand = {
  'op': 'write',
  'type': 'register',
  'device_id': 'D7A3F2',
  'register_id': 'R3C8D1',
  'value': 25.5,
};
```

---

## 8. Ringkasan Perubahan

| Item                    | Lama                          | Baru                          |
| ----------------------- | ----------------------------- | ----------------------------- |
| Error code 315          | Register not writable         | Mutex timeout                 |
| Error code 316          | Value out of range            | Device/register not found     |
| Error code 317          | Unsupported data type         | Read-only (FC2/FC4)           |
| Error code 318          | Write failed                  | writable = false              |
| Error code 319          | -                             | Value below min               |
| Error code 320          | -                             | Value above max               |
| Error code 305          | Device not found              | Serial port error             |
| Success key             | `written_value`               | `value_written`               |
| Success structure       | Nested `data` object          | Flat structure                |
| Error message key       | `message`                     | `error`                       |
| Error extras            | `domain`, `severity`          | Tidak ada                     |
| Default `writable`      | `false`                       | `true`                        |

---

## 9. Checklist Update Mobile App

- [ ] Update error code mapping (315-324, 331-334)
- [ ] Parse `"error"` instead of `"message"` for errors
- [ ] Parse `"value_written"` instead of `"written_value"` for success
- [ ] Handle flat response structure (no nested `data` object)
- [ ] Remove expectation for `domain`, `severity`, `suggestion` fields
- [ ] Update default `writable` assumption to `true`
- [ ] Add handling for min/max validation errors (319, 320)
- [ ] Add handling for Modbus exception codes (331-334)
- [ ] Update unit tests
- [ ] Update UI error messages

---

## 10. Kontak

Jika ada pertanyaan mengenai koreksi ini:

- **Firmware Team:** Lihat source code di `Main/ModbusRtuService.cpp:1505-1743`
- **Error Codes:** Lihat `Main/UnifiedErrorCodes.h:125-145`
- **Reference:** `Documentation/API_Reference/MOBILE_WRITE_FEATURE.md`

---

**SURIOTA R&D Team** | Firmware v1.3.1 | January 2026
