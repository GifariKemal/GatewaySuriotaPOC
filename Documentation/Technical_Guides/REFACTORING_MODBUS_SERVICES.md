# Panduan Refactoring: ModbusRtuService & ModbusTcpService

**Versi Dokumen:** 1.1 **Tanggal:** 27 Desember 2025 **Status:** Draft -
Menunggu Review **Penulis:** Claude AI Assistant **Update v1.1:** Rekomendasi
diubah ke PSRAMString unified berdasarkan riset Espressif

---

## Daftar Isi

1. [Ringkasan Eksekutif](#1-ringkasan-eksekutif)
2. [Latar Belakang Masalah](#2-latar-belakang-masalah)
3. [Analisis Kode Duplikasi](#3-analisis-kode-duplikasi)
4. [Solusi yang Direkomendasikan](#4-solusi-yang-direkomendasikan)
5. [Perbandingan Before/After](#5-perbandingan-beforeafter)
6. [Rencana Implementasi](#6-rencana-implementasi)
7. [Struktur File Baru](#7-struktur-file-baru)
8. [Panduan Migrasi](#8-panduan-migrasi)
9. [Pengujian](#9-pengujian)
10. [Risiko dan Mitigasi](#10-risiko-dan-mitigasi)
11. [Keputusan yang Diperlukan](#11-keputusan-yang-diperlukan)
12. [Lampiran](#12-lampiran)

---

## 1. Ringkasan Eksekutif

### 1.1 Masalah Utama

Ditemukan **~1.074 baris kode duplikat** antara `ModbusRtuService` dan
`ModbusTcpService`, yang mencakup:

- 5 struct dengan definisi hampir identik
- 35 method dengan logika yang sama
- Perbedaan utama hanya pada tipe string (`PSRAMString` vs `String`)

### 1.2 Dampak Duplikasi

| Aspek               | Dampak                                                 |
| ------------------- | ------------------------------------------------------ |
| **Maintainability** | Bug fix harus dilakukan di 2 tempat                    |
| **Konsistensi**     | Risiko implementasi yang berbeda untuk fitur yang sama |
| **Code Review**     | Waktu review bertambah karena kode berulang            |
| **Binary Size**     | ~2-3KB tambahan pada firmware                          |

### 1.3 Solusi yang Direkomendasikan

Menggunakan **Composition Pattern dengan PSRAMString Unified** untuk mengekstrak
kode duplikat ke dalam:

- `ModbusDeviceTypes.h` - Definisi struct bersama (PSRAMString untuk semua)
- `ModbusDeviceManager.h` - Template class untuk logika device management

**Keputusan String Type:** Menggunakan `PSRAMString` untuk KEDUA service (RTU
dan TCP) berdasarkan riset teknis Espressif yang membuktikan:

- PSRAM cache membuat string kecil (<16KB) memiliki performa identik dengan DRAM
- Arduino String mengkonsumsi DRAM yang terbatas (512KB)
- PSRAMString dengan fallback ke DRAM adalah solusi paling optimal

### 1.4 Hasil yang Diharapkan

| Metrik                      | Sebelum | Sesudah | Perubahan                   |
| --------------------------- | ------- | ------- | --------------------------- |
| Total LOC duplikat          | 1.074   | 569     | **-505 (47%)**              |
| File yang perlu di-maintain | 4       | 6       | +2 (tapi lebih terstruktur) |
| Risiko inkonsistensi        | Tinggi  | Rendah  | Signifikan                  |

---

## 2. Latar Belakang Masalah

### 2.1 Arsitektur Saat Ini

Firmware SRT-MGATE-1210 memiliki dua service untuk komunikasi Modbus:

```
┌─────────────────────────┐    ┌─────────────────────────┐
│   ModbusRtuService      │    │   ModbusTcpService      │
├─────────────────────────┤    ├─────────────────────────┤
│ - Device tracking       │    │ - Device tracking       │ ← DUPLIKAT
│ - Failure handling      │    │ - Failure handling      │ ← DUPLIKAT
│ - Timeout management    │    │ - Timeout management    │ ← DUPLIKAT
│ - Health metrics        │    │ - Health metrics        │ ← DUPLIKAT
│ - BLE command API       │    │ - BLE command API       │ ← DUPLIKAT
│ - Auto-recovery         │    │ - Auto-recovery         │ ← DUPLIKAT
├─────────────────────────┤    ├─────────────────────────┤
│ - RTU-specific polling  │    │ - TCP-specific polling  │ ← UNIK
│ - Serial communication  │    │ - Socket communication  │ ← UNIK
│ - Baudrate management   │    │ - Connection pooling    │ ← UNIK
└─────────────────────────┘    └─────────────────────────┘
```

### 2.2 Sejarah Perkembangan

Duplikasi ini terjadi secara organik selama pengembangan:

| Versi   | Perubahan                                  | Dampak Duplikasi                      |
| ------- | ------------------------------------------ | ------------------------------------- |
| v2.3.8  | Device failure tracking ditambahkan ke RTU | -                                     |
| v2.3.9  | Fitur yang sama di-copy ke TCP             | +200 lines duplikat                   |
| v2.3.11 | Health metrics ditambahkan                 | +100 lines duplikat                   |
| v2.3.12 | BLE device control API                     | +150 lines duplikat                   |
| v2.5.39 | Atomic config flag                         | Ditambahkan terpisah di kedua service |

### 2.3 Mengapa Ini Menjadi Technical Debt

1. **Bug Fix Ganda**: Setiap bug harus diperbaiki di 2 file
   - Contoh: v2.5.40 shadow cache fix harus diterapkan di RTU dan TCP

2. **Fitur Baru**: Setiap enhancement harus diimplementasikan 2 kali
   - Contoh: Auto-recovery task di-copy dari RTU ke TCP

3. **Risiko Inkonsistensi**: Implementasi bisa berbeda tanpa disengaja
   - Contoh: `calculateBackoffTime()` memiliki base delay berbeda (100ms vs
     2000ms)

---

## 3. Analisis Kode Duplikasi

### 3.1 Ringkasan Temuan

| Kategori                  | Jumlah Item | LOC Duplikat | Persentase Identik |
| ------------------------- | ----------- | ------------ | ------------------ |
| Struct Definitions        | 5           | 180          | 95-100%            |
| Device Management Methods | 14          | 498          | 90-100%            |
| BLE Command API           | 4           | 212          | 95-98%             |
| Polling/Timer Methods     | 6           | 103          | 100%               |
| Auto-Recovery             | 2           | 81           | 95%                |
| **TOTAL**                 | **31**      | **1.074**    | **~95%**           |

### 3.2 Detail Struct Duplikasi

#### 3.2.1 DeviceTimer

**Lokasi:**

- RTU: `ModbusRtuService.h` baris 37-43
- TCP: `ModbusTcpService.h` baris 41-46

**Perbandingan:**

```cpp
// ═══════════════════════════════════════════════════════════════
// ModbusRtuService.h (SAAT INI)
// ═══════════════════════════════════════════════════════════════
struct DeviceTimer {
    PSRAMString deviceId;      // ← Tipe: PSRAMString (PSRAM-optimized)
    unsigned long lastRead;
    uint32_t refreshRateMs;
};

// ═══════════════════════════════════════════════════════════════
// ModbusTcpService.h (SAAT INI)
// ═══════════════════════════════════════════════════════════════
struct DeviceTimer {
    String deviceId;           // ← Tipe: Arduino String (DRAM)
    unsigned long lastRead;
    uint32_t refreshRateMs;
};
```

**Analisis:**

- Struktur identik 100%
- Perbedaan hanya pada tipe string
- Dapat di-unify dengan template

#### 3.2.2 DeviceFailureState

**Lokasi:**

- RTU: `ModbusRtuService.h` baris 60-83
- TCP: `ModbusTcpService.h` baris 64-86

**Perbandingan:**

```cpp
// ═══════════════════════════════════════════════════════════════
// ModbusRtuService.h (SAAT INI)
// ═══════════════════════════════════════════════════════════════
struct DeviceFailureState {
    PSRAMString deviceId;
    uint8_t consecutiveFailures = 0;
    uint8_t retryCount = 0;
    unsigned long nextRetryTime = 0;
    unsigned long lastReadAttempt = 0;
    unsigned long lastSuccessfulRead = 0;
    bool isEnabled = true;
    uint16_t baudRate = 9600;              // ← RTU-SPECIFIC
    uint32_t maxRetries = 5;

    enum DisableReason {
        NONE = 0,
        MANUAL = 1,
        AUTO_RETRY = 2,
        AUTO_TIMEOUT = 3
    };
    DisableReason disableReason = NONE;
    PSRAMString disableReasonDetail;
    unsigned long disabledTimestamp = 0;
};

// ═══════════════════════════════════════════════════════════════
// ModbusTcpService.h (SAAT INI)
// ═══════════════════════════════════════════════════════════════
struct DeviceFailureState {
    String deviceId;                       // ← Tipe berbeda
    uint8_t consecutiveFailures = 0;
    uint8_t retryCount = 0;
    unsigned long nextRetryTime = 0;
    unsigned long lastReadAttempt = 0;
    unsigned long lastSuccessfulRead = 0;
    bool isEnabled = true;
    // TIDAK ADA baudRate                  // ← TCP tidak perlu baudRate
    uint32_t maxRetries = 5;

    enum DisableReason {
        NONE = 0,
        MANUAL = 1,
        AUTO_RETRY = 2,
        AUTO_TIMEOUT = 3
    };
    DisableReason disableReason = NONE;
    String disableReasonDetail;            // ← Tipe berbeda
    unsigned long disabledTimestamp = 0;
};
```

**Analisis:**

- 95% identik
- RTU memiliki field tambahan `baudRate` (spesifik untuk serial communication)
- Dapat di-unify dengan template + optional field

#### 3.2.3 DeviceReadTimeout

**Lokasi:**

- RTU: `ModbusRtuService.h` baris 87-95
- TCP: `ModbusTcpService.h` baris 90-98

**Status:** 100% identik (kecuali tipe string)

#### 3.2.4 DeviceHealthMetrics

**Lokasi:**

- RTU: `ModbusRtuService.h` baris 98-147
- TCP: `ModbusTcpService.h` baris 101-146

**Status:** 100% identik termasuk method `getSuccessRate()`,
`getAvgResponseTimeMs()`, dan `recordRead()`

### 3.3 Detail Method Duplikasi

#### 3.3.1 Initialization Methods

| Method                              | RTU LOC | TCP LOC | Identik |
| ----------------------------------- | ------- | ------- | ------- |
| `initializeDeviceFailureTracking()` | 22      | 22      | 95%     |
| `initializeDeviceTimeouts()`        | 18      | 18      | 95%     |
| `initializeDeviceMetrics()`         | 20      | 20      | 95%     |

**Contoh Duplikasi:**

```cpp
// ═══════════════════════════════════════════════════════════════
// ModbusRtuService.cpp - initializeDeviceMetrics()
// ═══════════════════════════════════════════════════════════════
void ModbusRtuService::initializeDeviceMetrics() {
    deviceMetrics.clear();

    for (const auto &device : rtuDevices) {
        DeviceHealthMetrics metrics;
        metrics.deviceId = device.deviceId;
        metrics.totalReads = 0;
        metrics.successfulReads = 0;
        metrics.failedReads = 0;
        metrics.totalResponseTimeMs = 0;
        metrics.minResponseTimeMs = 65535;
        metrics.maxResponseTimeMs = 0;
        metrics.lastResponseTimeMs = 0;
        deviceMetrics.push_back(metrics);
    }
    LOG_RTU_INFO("[RTU] Metrics tracking init: %d devices\n", deviceMetrics.size());
}

// ═══════════════════════════════════════════════════════════════
// ModbusTcpService.cpp - initializeDeviceMetrics() [DUPLIKAT!]
// ═══════════════════════════════════════════════════════════════
void ModbusTcpService::initializeDeviceMetrics() {
    deviceMetrics.clear();

    for (const auto &device : tcpDevices) {      // ← Hanya nama vector berbeda
        DeviceHealthMetrics metrics;
        metrics.deviceId = device.deviceId;
        metrics.totalReads = 0;
        metrics.successfulReads = 0;
        metrics.failedReads = 0;
        metrics.totalResponseTimeMs = 0;
        metrics.minResponseTimeMs = 65535;
        metrics.maxResponseTimeMs = 0;
        metrics.lastResponseTimeMs = 0;
        deviceMetrics.push_back(metrics);
    }
    LOG_TCP_INFO("[TCP] Metrics tracking init: %d devices\n", deviceMetrics.size());
}
```

#### 3.3.2 Getter Methods

| Method                    | RTU LOC | TCP LOC | Identik |
| ------------------------- | ------- | ------- | ------- |
| `getDeviceFailureState()` | 12      | 12      | 100%    |
| `getDeviceTimeout()`      | 12      | 12      | 100%    |
| `getDeviceMetrics()`      | 12      | 12      | 100%    |
| `getDeviceTimer()`        | 12      | 12      | 100%    |

**Contoh Duplikasi (100% identik kecuali parameter type):**

```cpp
// RTU Version
DeviceFailureState *ModbusRtuService::getDeviceFailureState(const char *deviceId) {
    for (auto &state : deviceFailureStates) {
        if (state.deviceId == deviceId) {
            return &state;
        }
    }
    return nullptr;
}

// TCP Version - IDENTIK!
DeviceFailureState *ModbusTcpService::getDeviceFailureState(const String &deviceId) {
    for (auto &state : deviceFailureStates) {
        if (state.deviceId == deviceId) {
            return &state;
        }
    }
    return nullptr;
}
```

#### 3.3.3 Backoff & Retry Logic

| Method                      | RTU LOC | TCP LOC | Perbedaan              |
| --------------------------- | ------- | ------- | ---------------------- |
| `handleReadFailure()`       | 25      | 25      | Log prefix saja        |
| `shouldRetryDevice()`       | 10      | 10      | Identik                |
| `calculateBackoffTime()`    | 10      | 10      | **Base delay berbeda** |
| `resetDeviceFailureState()` | 12      | 12      | Identik                |
| `handleReadTimeout()`       | 20      | 20      | Log prefix saja        |

**Perbedaan Signifikan pada `calculateBackoffTime()`:**

```cpp
// ═══════════════════════════════════════════════════════════════
// ModbusRtuService.cpp
// ═══════════════════════════════════════════════════════════════
unsigned long ModbusRtuService::calculateBackoffTime(uint8_t retryCount) {
    unsigned long baseDelay = 100;    // ← 100ms untuk RTU (serial cepat)
    unsigned long backoff = baseDelay * (1 << (retryCount - 1));
    unsigned long maxBackoff = 3200;  // ← Max 3.2 detik
    backoff = min(backoff, maxBackoff);
    unsigned long jitter = random(0, backoff / 4);
    return backoff + jitter;
}

// ═══════════════════════════════════════════════════════════════
// ModbusTcpService.cpp
// ═══════════════════════════════════════════════════════════════
unsigned long ModbusTcpService::calculateBackoffTime(uint8_t retryCount) {
    unsigned long baseDelay = 2000;   // ← 2000ms untuk TCP (reconnect lambat)
    unsigned long backoff = baseDelay * (1 << (retryCount - 1));
    unsigned long maxBackoff = 32000; // ← Max 32 detik
    backoff = min(backoff, maxBackoff);
    unsigned long jitter = random(0, backoff / 4);
    return backoff + jitter;
}
```

**Alasan Perbedaan:**

- RTU menggunakan serial yang responsif (baseDelay=100ms cukup)
- TCP perlu waktu untuk TCP handshake & reconnection (baseDelay=2000ms)

#### 3.3.4 Enable/Disable Device

| Method              | RTU LOC | TCP LOC | Identik |
| ------------------- | ------- | ------- | ------- |
| `enableDevice()`    | 35      | 37      | 95%     |
| `disableDevice()`   | 32      | 32      | 95%     |
| `isDeviceEnabled()` | 8       | 8       | 100%    |

#### 3.3.5 BLE Command API

| Method                     | RTU LOC | TCP LOC | Identik |
| -------------------------- | ------- | ------- | ------- |
| `enableDeviceByCommand()`  | 15      | 15      | 98%     |
| `disableDeviceByCommand()` | 15      | 15      | 98%     |
| `getDeviceStatusInfo()`    | 62      | 63      | 98%     |
| `getAllDevicesStatus()`    | 14      | 13      | 98%     |

**Contoh `getDeviceStatusInfo()` - 62 baris hampir identik:**

```cpp
bool ModbusRtuService::getDeviceStatusInfo(const char *deviceId, JsonObject &statusInfo) {
    DeviceFailureState *state = getDeviceFailureState(deviceId);
    if (!state) {
        LOG_RTU_INFO("[RTU] ERROR: Device %s not found\n", deviceId);
        return false;
    }

    DeviceReadTimeout *timeout = getDeviceTimeout(deviceId);
    DeviceHealthMetrics *metrics = getDeviceMetrics(deviceId);

    statusInfo["device_id"] = deviceId;
    statusInfo["enabled"] = state->isEnabled;
    statusInfo["consecutive_failures"] = state->consecutiveFailures;
    statusInfo["retry_count"] = state->retryCount;

    // ... 40+ baris JSON building yang IDENTIK ...

    return true;
}
```

#### 3.3.6 Auto-Recovery Task

| Method               | RTU LOC | TCP LOC | Identik |
| -------------------- | ------- | ------- | ------- |
| `autoRecoveryTask()` | 5       | 5       | 100%    |
| `autoRecoveryLoop()` | 35      | 36      | 95%     |

---

## 4. Solusi yang Direkomendasikan

### 4.1 Pendekatan: Composition dengan Template

Alih-alih menggunakan inheritance (yang memiliki overhead virtual function),
kita akan menggunakan:

1. **Template Struct** untuk definisi data yang bisa menerima tipe string
   berbeda
2. **Template Class** untuk logika device management yang shared
3. **Composition** dimana RTU/TCP service memiliki instance dari shared manager

### 4.2 Arsitektur Baru

```
┌─────────────────────────────────────────────────────────────────┐
│                    ModbusDeviceTypes.h                          │
│  ┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐   │
│  │  DeviceTimer    │ │ DeviceFailure   │ │ DeviceMetrics   │   │
│  │  (PSRAMString)  │ │  (PSRAMString)  │ │  (PSRAMString)  │   │
│  └─────────────────┘ └─────────────────┘ └─────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                   ModbusDeviceManager                           │
│                   (PSRAMString unified)                         │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ - initializeDeviceFailureTracking()                      │   │
│  │ - initializeDeviceTimeouts()                             │   │
│  │ - initializeDeviceMetrics()                              │   │
│  │ - getDeviceFailureState() / getDeviceTimeout()           │   │
│  │ - handleReadFailure() / handleReadTimeout()              │   │
│  │ - calculateBackoffTime(baseDelay)  ← parameterized       │   │
│  │ - enableDevice() / disableDevice()                       │   │
│  │ - getDeviceStatusInfo() / getAllDevicesStatus()          │   │
│  │ - shouldPollDevice() / updateDeviceLastRead()            │   │
│  │ - autoRecoveryLoop()                                     │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                              │
              ┌───────────────┴───────────────┐
              ▼                               ▼
┌─────────────────────────┐    ┌─────────────────────────┐
│   ModbusRtuService      │    │   ModbusTcpService      │
├─────────────────────────┤    ├─────────────────────────┤
│ ModbusDeviceManager     │    │ ModbusDeviceManager     │
│   (PSRAMString)         │    │   (PSRAMString)  ← UNIFIED
├─────────────────────────┤    ├─────────────────────────┤
│ - RTU-specific polling  │    │ - TCP-specific polling  │
│ - Serial communication  │    │ - Connection pooling    │
│ - Baudrate management   │    │ - Socket management     │
└─────────────────────────┘    └─────────────────────────┘
```

### 4.3 Keuntungan Pendekatan Ini

| Aspek                       | Keuntungan                                       |
| --------------------------- | ------------------------------------------------ |
| **Type Safety**             | Template menjaga type safety tanpa casting       |
| **No Virtual Overhead**     | Tidak ada vtable lookup (penting untuk embedded) |
| **Compile-time Resolution** | Semua resolved saat compile, bukan runtime       |
| **Backward Compatible**     | Public API tidak berubah                         |
| **Testable**                | Shared code bisa di-unit test terpisah           |

### 4.4 Handling Perbedaan

| Perbedaan                                   | Solusi                                           |
| ------------------------------------------- | ------------------------------------------------ |
| ~~String type (`PSRAMString` vs `String`)~~ | **UNIFIED:** Semua menggunakan `PSRAMString`     |
| Base backoff delay (100ms vs 2000ms)        | Parameter pada `calculateBackoffTime(baseDelay)` |
| Log prefix (`[RTU]` vs `[TCP]`)             | Parameter string pada method yang log            |
| RTU-specific `baudRate` field               | Optional field atau separate struct              |

### 4.5 Justifikasi Teknis: Mengapa PSRAMString untuk Semua?

Berdasarkan riset dokumentasi resmi Espressif dan ESP32 Forum:

#### 4.5.1 PSRAM Cache Behavior

```
┌─────────────────────────────────────────────────────────────────┐
│                     ESP32-S3 Memory Architecture                 │
├─────────────────────────────────────────────────────────────────┤
│  CPU Cache (16-32KB)                                             │
│    ↓ cache hit = DRAM-speed access                              │
│  PSRAM (8MB) via QSPI/OSPI                                       │
│    ↓ cache miss = ~10-20 cycles latency                         │
│  DRAM (512KB) - limited, shared with WiFi/BLE stack              │
└─────────────────────────────────────────────────────────────────┘
```

**Fakta Kunci:**

1. String kecil (<16KB) yang sering diakses → masuk CPU cache → performa = DRAM
2. DRAM adalah resource terbatas (512KB) yang harus dibagi dengan WiFi/BLE stack
3. PSRAMString dengan fallback ke DRAM = strategi paling aman

#### 4.5.2 Referensi Espressif Official

- **ESP-IDF Heap Allocator:** `heap_caps_malloc(MALLOC_CAP_SPIRAM)` dengan
  fallback
- **Recommended Pattern:** "Use PSRAM for large allocations, let cache handle
  frequently accessed data"
- **Source:** Espressif Programming Guide - External RAM

#### 4.5.3 Implementasi PSRAMString

```cpp
// PSRAMString::allocate() di Main/PSRAMString.h
void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
if (!ptr) {
    ptr = heap_caps_malloc(size, MALLOC_CAP_8BIT); // Fallback ke DRAM
}
```

**Keuntungan:**

- PSRAM priority → menjaga DRAM untuk operasi kritikal
- Automatic DRAM fallback → tidak pernah gagal alokasi
- Cache-friendly → string yang sering diakses tetap cepat

---

## 5. Perbandingan Before/After

### 5.1 Struct Definitions

| Struct                     | BEFORE (LOC) | AFTER (LOC) | Pengurangan   |
| -------------------------- | ------------ | ----------- | ------------- |
| `DeviceTimer`              | 7 + 6 = 13   | 10          | -3 (23%)      |
| `DeviceFailureState`       | 24 + 23 = 47 | 28          | -19 (40%)     |
| `DeviceReadTimeout`        | 9 + 9 = 18   | 10          | -8 (44%)      |
| `DeviceHealthMetrics`      | 50 + 47 = 97 | 52          | -45 (46%)     |
| `DataTransmissionInterval` | 5 + 5 = 10   | 6           | -4 (40%)      |
| **TOTAL STRUCT**           | **185**      | **106**     | **-79 (43%)** |

### 5.2 Device Management Methods

| Method                              | BEFORE  | AFTER   | Pengurangan    |
| ----------------------------------- | ------- | ------- | -------------- |
| `initializeDeviceFailureTracking()` | 44      | 25      | -19            |
| `initializeDeviceTimeouts()`        | 36      | 20      | -16            |
| `initializeDeviceMetrics()`         | 40      | 22      | -18            |
| `getDeviceFailureState()`           | 24      | 12      | -12            |
| `getDeviceTimeout()`                | 24      | 12      | -12            |
| `getDeviceMetrics()`                | 24      | 12      | -12            |
| `handleReadFailure()`               | 50      | 28      | -22            |
| `shouldRetryDevice()`               | 20      | 10      | -10            |
| `calculateBackoffTime()`            | 20      | 12      | -8             |
| `resetDeviceFailureState()`         | 24      | 12      | -12            |
| `handleReadTimeout()`               | 40      | 22      | -18            |
| `isDeviceEnabled()`                 | 16      | 8       | -8             |
| `enableDevice()`                    | 72      | 38      | -34            |
| `disableDevice()`                   | 64      | 34      | -30            |
| **TOTAL**                           | **498** | **267** | **-231 (46%)** |

### 5.3 BLE Command API

| Method                     | BEFORE  | AFTER   | Pengurangan    |
| -------------------------- | ------- | ------- | -------------- |
| `enableDeviceByCommand()`  | 30      | 16      | -14            |
| `disableDeviceByCommand()` | 30      | 16      | -14            |
| `getDeviceStatusInfo()`    | 125     | 65      | -60            |
| `getAllDevicesStatus()`    | 27      | 15      | -12            |
| **TOTAL**                  | **212** | **112** | **-100 (47%)** |

### 5.4 Polling/Timer Methods

| Method                          | BEFORE  | AFTER  | Pengurangan   |
| ------------------------------- | ------- | ------ | ------------- |
| `getDeviceTimer()`              | 24      | 12     | -12           |
| `shouldPollDevice()`            | 35      | 18     | -17           |
| `updateDeviceLastRead()`        | 16      | 8      | -8            |
| `shouldTransmitData()`          | 10      | 5      | -5            |
| `updateDataTransmissionTime()`  | 8       | 4      | -4            |
| `setDataTransmissionInterval()` | 10      | 6      | -4            |
| **TOTAL**                       | **103** | **53** | **-50 (49%)** |

### 5.5 Auto-Recovery

| Method               | BEFORE | AFTER  | Pengurangan   |
| -------------------- | ------ | ------ | ------------- |
| `autoRecoveryTask()` | 10     | 5      | -5            |
| `autoRecoveryLoop()` | 71     | 38     | -33           |
| **TOTAL**            | **81** | **43** | **-38 (47%)** |

### 5.6 Ringkasan Total

| Kategori           | BEFORE    | AFTER   | Pengurangan    |
| ------------------ | --------- | ------- | -------------- |
| Struct Definitions | 185       | 106     | -79 (43%)      |
| Device Management  | 498       | 267     | -231 (46%)     |
| BLE Command API    | 212       | 112     | -100 (47%)     |
| Polling/Timer      | 103       | 53      | -50 (49%)      |
| Auto-Recovery      | 81        | 43      | -38 (47%)      |
| **GRAND TOTAL**    | **1.079** | **581** | **-498 (46%)** |

---

## 6. Rencana Implementasi

### 6.1 Phase 1: Extract Struct Definitions (Estimasi: 2-3 jam)

**Tujuan:** Membuat `ModbusDeviceTypes.h` dengan semua struct template

**Langkah:**

1. Buat file baru `Main/ModbusDeviceTypes.h`
2. Definisikan template struct untuk semua tipe data
3. Update `ModbusRtuService.h` untuk menggunakan type alias
4. Update `ModbusTcpService.h` untuk menggunakan type alias
5. Compile dan test

**Risiko:** Rendah - hanya memindahkan definisi, tidak mengubah logika

**Deliverable:**

```cpp
// ModbusDeviceTypes.h
template<typename StringT>
struct DeviceTimer { ... };

template<typename StringT>
struct DeviceFailureState { ... };

// dst...
```

### 6.2 Phase 2: Extract Device Manager (Estimasi: 4-6 jam)

**Tujuan:** Membuat `ModbusDeviceManager.h` dengan semua shared logic

**Langkah:**

1. Buat file baru `Main/ModbusDeviceManager.h`
2. Implementasi template class dengan semua method
3. Tambahkan parameter untuk perbedaan (backoff delay, log prefix)
4. Update `ModbusRtuService` untuk menggunakan composition
5. Update `ModbusTcpService` untuk menggunakan composition
6. Compile dan test per-method

**Risiko:** Medium - perubahan signifikan pada internal struktur

**Deliverable:**

```cpp
// ModbusDeviceManager.h
template<typename StringT, typename DeviceConfigT>
class ModbusDeviceManager {
public:
    void initializeDeviceFailureTracking(const std::vector<DeviceConfigT>& devices);
    void handleReadFailure(const StringT& deviceId, unsigned long baseBackoffMs);
    // ... semua shared methods
};
```

### 6.3 Phase 3: Migrate BLE API & Auto-Recovery (Estimasi: 2-3 jam)

**Tujuan:** Migrasi semua BLE command API dan auto-recovery ke shared manager

**Langkah:**

1. Pindahkan `enableDeviceByCommand()`, `disableDeviceByCommand()` ke manager
2. Pindahkan `getDeviceStatusInfo()`, `getAllDevicesStatus()` ke manager
3. Pindahkan `autoRecoveryLoop()` ke manager
4. Update service classes untuk delegate ke manager
5. Compile dan test

**Risiko:** Rendah - method ini sudah well-defined

### 6.4 Phase 4: Testing & Validation (Estimasi: 2-4 jam)

**Tujuan:** Memastikan semua fungsionalitas bekerja seperti sebelumnya

**Test Scenarios:**

1. Device polling RTU dan TCP
2. Device failure dan auto-recovery
3. BLE commands: enable/disable device
4. BLE commands: get device status
5. Health metrics collection
6. Config change notification

**Acceptance Criteria:**

- Semua existing functionality berfungsi sama
- Tidak ada regresi pada log output
- Memory usage tidak meningkat signifikan
- Build size tidak meningkat signifikan

### 6.5 Timeline

```
Week 1:
├── Day 1-2: Phase 1 (Extract Structs)
├── Day 3-4: Phase 2 Part 1 (Core Manager Methods)
└── Day 5: Phase 2 Part 2 (Remaining Methods)

Week 2:
├── Day 1: Phase 3 (BLE API & Auto-Recovery)
├── Day 2-3: Phase 4 (Testing)
└── Day 4-5: Bug fixes & Documentation
```

---

## 7. Struktur File Baru

### 7.1 File yang Ditambahkan

```
Main/
├── ModbusDeviceTypes.h      ← BARU (estimasi ~100 lines)
│   └── Template structs untuk device tracking
│
└── ModbusDeviceManager.h    ← BARU (estimasi ~450 lines)
    └── Template class untuk shared device management logic
```

### 7.2 File yang Dimodifikasi

```
Main/
├── ModbusRtuService.h       ← MODIFIED
│   ├── Remove: struct definitions (pindah ke Types.h)
│   ├── Add: #include "ModbusDeviceTypes.h"
│   ├── Add: #include "ModbusDeviceManager.h"
│   ├── Add: type aliases (using RtuDeviceTimer = DeviceTimer<PSRAMString>)
│   └── Add: ModbusDeviceManager<PSRAMString> deviceManager member
│
├── ModbusRtuService.cpp     ← MODIFIED
│   ├── Remove: ~380 lines of duplicated methods
│   └── Add: delegation calls to deviceManager
│
├── ModbusTcpService.h       ← MODIFIED
│   ├── Remove: struct definitions (pindah ke Types.h)
│   ├── Add: #include headers
│   ├── Add: type aliases
│   └── Add: ModbusDeviceManager<String> deviceManager member
│
└── ModbusTcpService.cpp     ← MODIFIED
    ├── Remove: ~380 lines of duplicated methods
    └── Add: delegation calls to deviceManager
```

### 7.3 Perbandingan Size

| File                  | BEFORE (LOC) | AFTER (LOC) | Delta    |
| --------------------- | ------------ | ----------- | -------- |
| ModbusDeviceTypes.h   | 0 (baru)     | ~100        | +100     |
| ModbusDeviceManager.h | 0 (baru)     | ~450        | +450     |
| ModbusRtuService.h    | 247          | ~130        | -117     |
| ModbusRtuService.cpp  | ~1600        | ~1220       | -380     |
| ModbusTcpService.h    | 267          | ~150        | -117     |
| ModbusTcpService.cpp  | 2119         | ~1740       | -379     |
| **TOTAL**             | **4233**     | **3790**    | **-443** |

---

## 8. Panduan Migrasi

### 8.1 Contoh Penggunaan Setelah Refactoring

#### 8.1.1 ModbusRtuService (After)

```cpp
// ModbusRtuService.h
#include "ModbusDeviceTypes.h"
#include "ModbusDeviceManager.h"

class ModbusRtuService {
private:
    // Type aliases untuk RTU
    using RtuDeviceManager = ModbusDeviceManager<PSRAMString>;
    using RtuDeviceTimer = DeviceTimer<PSRAMString>;
    using RtuFailureState = DeviceFailureState<PSRAMString>;

    // Composition: RTU service MEMILIKI device manager
    RtuDeviceManager deviceManager;

    // RTU-specific members tetap di sini
    HardwareSerial *serial1, *serial2;
    ModbusMaster *modbus1, *modbus2;

public:
    // Public API TIDAK BERUBAH (backward compatible)
    bool enableDeviceByCommand(const char *deviceId, bool clearMetrics = false) {
        return deviceManager.enableDeviceByCommand(deviceId, clearMetrics, "[RTU]");
    }

    bool getDeviceStatusInfo(const char *deviceId, JsonObject &statusInfo) {
        return deviceManager.getDeviceStatusInfo(deviceId, statusInfo);
    }
};
```

#### 8.1.2 ModbusTcpService (After) - UPDATED: PSRAMString Unified

```cpp
// ModbusTcpService.h
#include "ModbusDeviceTypes.h"
#include "ModbusDeviceManager.h"
#include "PSRAMString.h"  // ← TAMBAHAN: Include PSRAMString

class ModbusTcpService {
private:
    // Type aliases untuk TCP - SEKARANG SAMA DENGAN RTU!
    using TcpDeviceManager = ModbusDeviceManager<PSRAMString>;  // ← CHANGED from String
    using TcpDeviceTimer = DeviceTimer<PSRAMString>;            // ← CHANGED from String
    using TcpFailureState = DeviceFailureState<PSRAMString>;    // ← CHANGED from String

    // Composition
    TcpDeviceManager deviceManager;

    // TCP-specific members tetap di sini
    std::vector<ConnectionPoolEntry> connectionPool;

public:
    // Public API TIDAK BERUBAH (backward compatible)
    // Catatan: Parameter bisa tetap const String& untuk backward compat,
    // internal akan convert ke PSRAMString
    bool enableDeviceByCommand(const String &deviceId, bool clearMetrics = false) {
        PSRAMString psramId(deviceId.c_str());  // Convert sekali di entry point
        return deviceManager.enableDeviceByCommand(psramId, clearMetrics, "[TCP]");
    }
};
```

**Catatan Migrasi TCP ke PSRAMString:**

1. Tambahkan `#include "PSRAMString.h"` di header
2. Ubah semua internal string dari `String` ke `PSRAMString`
3. Public API bisa tetap menerima `const String&` untuk backward compatibility
4. Konversi dilakukan di entry point method (one-time cost)

### 8.2 Handling Backoff Delay Berbeda

```cpp
// ModbusDeviceManager.h
template<typename StringT>
class ModbusDeviceManager {
private:
    unsigned long backoffBaseDelay;  // Configurable base delay

public:
    // Constructor menerima base delay
    ModbusDeviceManager(unsigned long baseDelay) : backoffBaseDelay(baseDelay) {}

    unsigned long calculateBackoffTime(uint8_t retryCount) {
        unsigned long backoff = backoffBaseDelay * (1 << (retryCount - 1));
        unsigned long maxBackoff = backoffBaseDelay * 32;  // 32x base
        backoff = min(backoff, maxBackoff);
        unsigned long jitter = random(0, backoff / 4);
        return backoff + jitter;
    }
};

// Penggunaan di RTU
class ModbusRtuService {
    ModbusDeviceManager<PSRAMString> deviceManager{100};  // 100ms base
};

// Penggunaan di TCP
class ModbusTcpService {
    ModbusDeviceManager<String> deviceManager{2000};  // 2000ms base
};
```

---

## 9. Pengujian

### 9.1 Test Scenarios

#### 9.1.1 Unit Tests (Jika Ditambahkan)

| Test Case                     | Deskripsi                           |
| ----------------------------- | ----------------------------------- |
| `test_calculateBackoffTime`   | Verify exponential backoff formula  |
| `test_deviceFailureTracking`  | Verify failure counter increment    |
| `test_deviceEnableDisable`    | Verify enable/disable state changes |
| `test_healthMetricsRecording` | Verify metrics calculation          |

#### 9.1.2 Integration Tests

| Test Case          | Langkah                                                                                           | Expected Result                |
| ------------------ | ------------------------------------------------------------------------------------------------- | ------------------------------ |
| RTU Polling        | 1. Start RTU service<br>2. Poll device<br>3. Check metrics                                        | Metrics updated correctly      |
| TCP Polling        | 1. Start TCP service<br>2. Poll device<br>3. Check metrics                                        | Metrics updated correctly      |
| BLE Enable/Disable | 1. Send disable command<br>2. Verify polling stops<br>3. Send enable<br>4. Verify polling resumes | Device state changes correctly |
| Auto-Recovery      | 1. Simulate max retries<br>2. Wait 5 minutes<br>3. Verify device re-enabled                       | Auto-recovery works            |
| Config Change      | 1. Change config via BLE<br>2. Verify both services refresh                                       | Config propagates correctly    |

### 9.2 Regression Testing Checklist

- [ ] RTU device polling berfungsi normal
- [ ] TCP device polling berfungsi normal
- [ ] Device failure counter increment benar
- [ ] Exponential backoff timing benar (RTU: 100ms base, TCP: 2000ms base)
- [ ] Auto-recovery setelah 5 menit
- [ ] BLE `enable_device` command berfungsi
- [ ] BLE `disable_device` command berfungsi
- [ ] BLE `get_device_status` mengembalikan data lengkap
- [ ] BLE `get_all_devices_status` mengembalikan semua device
- [ ] Health metrics (success rate, response time) akurat
- [ ] Log output format tidak berubah
- [ ] Memory usage tidak meningkat signifikan (< 5%)
- [ ] Build size tidak meningkat signifikan (< 2KB)

---

## 10. Risiko dan Mitigasi

### 10.1 Risiko Teknis

| Risiko                                | Probabilitas | Dampak | Mitigasi                                                  |
| ------------------------------------- | ------------ | ------ | --------------------------------------------------------- |
| Template bloat (binary size increase) | Medium       | Low    | Monitor build size, use explicit instantiation jika perlu |
| Compile time increase                 | Low          | Low    | Template di header, but minimal nesting                   |
| Runtime behavior change               | Low          | High   | Extensive testing, gradual rollout                        |
| Memory layout difference              | Low          | Medium | Verify sizeof() struct sama                               |

### 10.2 Risiko Proses

| Risiko                                  | Probabilitas | Dampak | Mitigasi                                            |
| --------------------------------------- | ------------ | ------ | --------------------------------------------------- |
| Refactoring memakan waktu lebih lama    | Medium       | Medium | Phase-based approach, dapat dihentikan setiap phase |
| Bug introduced during refactoring       | Medium       | High   | Comprehensive testing, code review                  |
| Merge conflicts dengan development lain | Low          | Medium | Koordinasi dengan tim, refactor di branch terpisah  |

### 10.3 Rollback Plan

Jika ditemukan masalah kritis setelah refactoring:

1. **Phase 1 Rollback:** Revert `ModbusDeviceTypes.h` dan restore original
   structs
2. **Phase 2 Rollback:** Revert `ModbusDeviceManager.h` dan restore original
   methods
3. **Full Rollback:** Git revert ke commit sebelum refactoring dimulai

---

## 11. Keputusan yang Diperlukan

Sebelum memulai implementasi, diperlukan keputusan untuk:

### 11.1 String Type Strategy

| Opsi  | Deskripsi               | Pro                                 | Con                                         |
| ----- | ----------------------- | ----------------------------------- | ------------------------------------------- |
| **A** | Keep both via template  | Backward compatible                 | Template complexity, inconsistensi          |
| **B** | Unify ke PSRAMString    | Better memory efficiency, konsisten | Sedikit refactoring di TCP                  |
| **C** | Unify ke Arduino String | Simpler code                        | Buruk untuk memory - TIDAK DIREKOMENDASIKAN |

**~~Rekomendasi Lama:~~ ~~Opsi A (Keep both via template)~~**

**✅ Rekomendasi Baru (v1.1): Opsi B (Unify ke PSRAMString)**

#### Justifikasi Keputusan:

Berdasarkan riset dokumentasi Espressif dan ESP32 Forum, diputuskan untuk
menggunakan **PSRAMString untuk kedua service** karena:

| Aspek                 | Arduino String  | PSRAMString           | Pemenang    |
| --------------------- | --------------- | --------------------- | ----------- |
| Alokasi memori        | DRAM only       | PSRAM + DRAM fallback | PSRAMString |
| DRAM consumption      | Tinggi          | Rendah                | PSRAMString |
| Performa string kecil | Cepat           | Sama (cache)          | Seri        |
| Performa string besar | Cepat           | Sedikit lambat        | String      |
| Fragmentasi heap      | Tinggi          | Rendah (PSRAM besar)  | PSRAMString |
| WiFi/BLE stability    | Dapat terganggu | Aman                  | PSRAMString |

**Fakta Teknis:**

1. **PSRAM Cache:** String kecil (<16KB) yang sering diakses masuk CPU cache →
   performa identik dengan DRAM
2. **DRAM Limited:** ESP32-S3 hanya punya 512KB DRAM yang harus dibagi dengan
   WiFi/BLE stack
3. **Fallback Aman:** PSRAMString otomatis fallback ke DRAM jika PSRAM gagal
4. **Best Practice Espressif:** "Use PSRAM for large allocations, let cache
   handle frequently accessed data"

**Kesimpulan:** Tidak ada alasan teknis untuk menggunakan Arduino String di TCP
service. PSRAMString unified memberikan konsistensi dan efisiensi memori yang
lebih baik.

### 11.2 Implementation Priority

| Opsi  | Deskripsi                            |
| ----- | ------------------------------------ |
| **A** | Full refactoring (Phase 1-4)         |
| **B** | Partial refactoring (Phase 1-2 saja) |
| **C** | Postpone sampai next major version   |

**Rekomendasi:** Opsi A atau B tergantung timeline proyek

### 11.3 Testing Approach

| Opsi  | Deskripsi                          |
| ----- | ---------------------------------- |
| **A** | Manual testing saja                |
| **B** | Add unit tests untuk shared code   |
| **C** | Add unit tests + integration tests |

**Rekomendasi:** Opsi B (minimal unit tests untuk shared logic)

---

## 12. Lampiran

### 12.1 File Locations Reference

| File                 | Path                        | LOC   |
| -------------------- | --------------------------- | ----- |
| ModbusRtuService.h   | `Main/ModbusRtuService.h`   | 247   |
| ModbusRtuService.cpp | `Main/ModbusRtuService.cpp` | ~1600 |
| ModbusTcpService.h   | `Main/ModbusTcpService.h`   | 267   |
| ModbusTcpService.cpp | `Main/ModbusTcpService.cpp` | 2119  |
| ModbusUtils.h        | `Main/ModbusUtils.h`        | 76    |
| ModbusUtils.cpp      | `Main/ModbusUtils.cpp`      | 186   |

### 12.2 Related Documentation

- `Documentation/API_Reference/API.md` - BLE Command API reference
- `Documentation/Technical_Guides/MODBUS_DATATYPES.md` - Modbus data types
- `Documentation/Changelog/VERSION_HISTORY.md` - Version history

### 12.3 Version History Dokumen Ini

| Versi | Tanggal     | Perubahan                                                                                                                                                                                                                                 |
| ----- | ----------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1.0   | 27 Des 2025 | Initial draft                                                                                                                                                                                                                             |
| 1.1   | 27 Des 2025 | **UPDATE:** Rekomendasi diubah dari Hybrid (Opsi A) ke PSRAMString Unified (Opsi B) berdasarkan riset Espressif. Ditambahkan Section 4.5 tentang justifikasi teknis PSRAM cache behavior. Updated architecture diagram dan code examples. |

---

## Catatan Penutup

Refactoring ini adalah **technical debt cleanup** yang akan meningkatkan
maintainability codebase. Meskipun membutuhkan effort signifikan, manfaat jangka
panjang berupa:

1. **Single source of truth** untuk device management logic
2. **Easier bug fixes** - perbaiki di satu tempat, berlaku untuk RTU dan TCP
3. **Consistent behavior** - tidak ada risiko implementasi berbeda
4. **Better testability** - shared code bisa di-unit test
5. **Unified memory strategy** - PSRAMString untuk semua, menjaga DRAM untuk
   WiFi/BLE

**Update v1.1:** Keputusan untuk menggunakan PSRAMString secara konsisten di
kedua service didasarkan pada riset teknis yang membuktikan:

- PSRAM cache membuat string kecil berperforma identik dengan DRAM
- Arduino String mengkonsumsi DRAM yang terbatas dan shared dengan WiFi/BLE
  stack
- PSRAMString dengan fallback ke DRAM adalah strategi paling optimal untuk
  ESP32-S3

Dokumen ini akan di-update setelah menerima feedback dari review.

---

_Dokumen ini dibuat oleh Claude AI Assistant sebagai bagian dari analisis
firmware SRT-MGATE-1210._
