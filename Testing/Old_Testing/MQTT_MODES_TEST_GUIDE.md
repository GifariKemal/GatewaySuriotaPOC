# MQTT Publish Modes Test Client

Python script untuk testing MQTT Publish Modes (Default & Customize) pada
SRT-MGATE-1210 Firmware v2.0 via BLE.

## 🎯 Fitur Testing

### Default Mode Tests

- ✅ Test interval dengan unit **seconds** (5s)
- ✅ Test interval dengan unit **milliseconds** (3000ms)
- ✅ Test interval dengan unit **minutes** (1m)

### Customize Mode Tests

- ✅ Test basic 2 topics (Temperature & Pressure)
- ✅ Test mixed interval units (ms/s/m) dalam satu config
- ✅ Test register overlap (register yang sama di multiple topics)
- ✅ Test warehouse scenario (4 topics dengan kategorisasi)

### Other Tests

- ✅ Test disable both modes (MQTT tetap connected tapi tidak publish)
- ✅ Read current configuration

---

## 📋 Requirements

```bash
pip install bleak asyncio
```

**Dependencies:**

- Python 3.7+
- `bleak` - BLE library
- `asyncio` - Async I/O

### Prerequisites

**PENTING:** Sebelum testing MQTT modes, pastikan:

1. **Device sudah dibuat** via BLE (TCP atau RTU)
2. **Registers sudah dibuat** untuk device tersebut
3. **Register index sudah auto-generated**

**Mengapa register_index penting?**

Customize mode menggunakan **register_index** (bukan address Modbus) untuk
mapping register ke topic.

**Contoh:**

```json
// Device dengan 3 registers
{
  "registers": [
    {
      "register_index": 1,    // Auto-generated saat create
      "address": 40001,
      "register_name": "Temperature Sensor 1"
    },
    {
      "register_index": 2,    // Auto-generated
      "address": 40005,
      "register_name": "Pressure Sensor 1"
    },
    {
      "register_index": 3,    // Auto-generated
      "address": 40010,
      "register_name": "Humidity Sensor 1"
    }
  ]
}

// Custom topic menggunakan register_index
{
  "topic": "sensor/temperature",
  "registers": [1, 2]  // Register index 1 & 2, bukan address!
}
```

**Setup Steps:**

1. Run `python modbus_Config_test.py` untuk create device & registers
2. Verify `register_index` ada di setiap register (check serial monitor)
3. Run `python mqtt_publish_modes_test.py` untuk test MQTT modes

---

## 🚀 Cara Menggunakan

### 1. Pastikan Gateway dalam Mode BLE

Gateway harus dalam mode BLE advertising dengan nama `SURIOTA GW`.

### 2. Jalankan Script

```bash
python mqtt_publish_modes_test.py
```

### 3. Interactive Menu

Script akan menampilkan menu interaktif:

```
╔══════════════════════════════════════════════════════════════╗
║     MQTT PUBLISH MODES TEST CLIENT                           ║
║     SRT-MGATE-1210 Firmware v2.0                            ║
╚══════════════════════════════════════════════════════════════╝

🧪 MQTT PUBLISH MODES TEST MENU
==============================================================
DEFAULT MODE TESTS:
  1. Default Mode - 5 seconds
  2. Default Mode - 3000 milliseconds
  3. Default Mode - 1 minute

CUSTOMIZE MODE TESTS:
  4. Customize Mode - Basic (2 topics)
  5. Customize Mode - Mixed Intervals (ms/s/m)
  6. Customize Mode - Register Overlap
  7. Warehouse Monitoring Scenario

OTHER:
  8. Disable Both Modes
  9. Read Current Configuration
  10. Run All Tests Sequentially
  0. Exit
==============================================================

👉 Select option (0-10):
```

---

## 📝 Penjelasan Test Scenarios

### Test 1: Default Mode - 5 Seconds

**Tujuan:** Test interval dengan unit seconds

**Config:**

```json
{
  "publish_mode": "default",
  "default_mode": {
    "enabled": true,
    "topic_publish": "v1/devices/me/telemetry",
    "interval": 5,
    "interval_unit": "s"
  }
}
```

**Expected Behavior:**

- Firmware convert: 5s → 5000ms
- Publish semua register ke 1 topic setiap 5 detik
- Serial output: `[MQTT] Default Mode: ENABLED, Interval: 5s (5000ms)`

---

### Test 2: Default Mode - 3000 Milliseconds

**Tujuan:** Test interval dengan unit milliseconds

**Config:**

```json
{
  "publish_mode": "default",
  "default_mode": {
    "enabled": true,
    "interval": 3000,
    "interval_unit": "ms"
  }
}
```

**Expected Behavior:**

- Firmware convert: 3000ms → 3000ms (no conversion)
- Publish setiap 3 detik

---

### Test 3: Default Mode - 1 Minute

**Tujuan:** Test interval dengan unit minutes

**Config:**

```json
{
  "publish_mode": "default",
  "default_mode": {
    "enabled": true,
    "interval": 1,
    "interval_unit": "m"
  }
}
```

**Expected Behavior:**

- Firmware convert: 1m → 60000ms
- Publish setiap 1 menit
- Serial output: `[MQTT] Default Mode: ENABLED, Interval: 1m (60000ms)`

---

### Test 4: Customize Mode - Basic

**Tujuan:** Test customize mode dengan 2 topics

**Config:**

```json
{
  "publish_mode": "customize",
  "customize_mode": {
    "enabled": true,
    "custom_topics": [
      {
        "topic": "sensor/temperature",
        "registers": [1, 2, 3],
        "interval": 5,
        "interval_unit": "s"
      },
      {
        "topic": "sensor/pressure",
        "registers": [4, 5],
        "interval": 10,
        "interval_unit": "s"
      }
    ]
  }
}
```

**Expected Behavior:**

- Topic `sensor/temperature` publish register 1,2,3 setiap 5 detik
- Topic `sensor/pressure` publish register 4,5 setiap 10 detik
- Serial output menampilkan 2 custom topics

---

### Test 5: Customize Mode - Mixed Intervals

**Tujuan:** Test 3 unit interval berbeda (ms/s/m) dalam satu config

**Config:**

```json
{
  "custom_topics": [
    {
      "topic": "alerts/critical",
      "registers": [1, 5],
      "interval": 500,
      "interval_unit": "ms"
    },
    {
      "topic": "dashboard/realtime",
      "registers": [1, 2, 3, 4, 5, 6],
      "interval": 2,
      "interval_unit": "s"
    },
    {
      "topic": "database/historical",
      "registers": [1, 2, 3, 4, 5, 6],
      "interval": 1,
      "interval_unit": "m"
    }
  ]
}
```

**Expected Behavior:**

- `alerts/critical` → setiap 500ms
- `dashboard/realtime` → setiap 2s (2000ms)
- `database/historical` → setiap 1m (60000ms)
- Serial output menampilkan conversion untuk setiap topic

---

### Test 6: Customize Mode - Register Overlap

**Tujuan:** Test register yang sama di multiple topics

**Config:**

```json
{
  "custom_topics": [
    {
      "topic": "sensor/temperature",
      "registers": [1, 2, 3]
    },
    {
      "topic": "sensor/all_sensors",
      "registers": [1, 2, 3, 4, 5]
    },
    {
      "topic": "sensor/critical",
      "registers": [1]
    }
  ]
}
```

**Expected Behavior:**

- Register 1 publish ke 3 topics: temperature, all_sensors, critical
- Register 2-3 publish ke 2 topics: temperature, all_sensors
- Register 4-5 publish ke 1 topic: all_sensors
- Tidak ada error duplicate

---

### Test 7: Warehouse Scenario

**Tujuan:** Test real-world scenario dengan 4 kategori sensor

**Config:**

```json
{
  "custom_topics": [
    {
      "topic": "warehouse/environment/temperature",
      "registers": [1, 2, 3, 4],
      "interval": 5,
      "interval_unit": "s"
    },
    {
      "topic": "warehouse/environment/humidity",
      "registers": [5, 6, 7, 8],
      "interval": 5,
      "interval_unit": "s"
    },
    {
      "topic": "warehouse/safety/smoke",
      "registers": [9, 10],
      "interval": 1,
      "interval_unit": "s"
    },
    {
      "topic": "warehouse/safety/co2",
      "registers": [11, 12],
      "interval": 2,
      "interval_unit": "s"
    }
  ]
}
```

**Expected Behavior:**

- Environment sensors: Temp & Humidity setiap 5s
- Safety sensors: Smoke setiap 1s, CO2 setiap 2s
- Total 4 topics dengan interval berbeda

---

### Test 8: Disable Both Modes

**Tujuan:** Test MQTT connection tanpa publishing

**Config:**

```json
{
  "mqtt_config": {
    "enabled": true,
    "default_mode": {
      "enabled": false
    },
    "customize_mode": {
      "enabled": false
    }
  }
}
```

**Expected Behavior:**

- MQTT tetap connected ke broker
- Tidak ada publishing data
- Serial output: `[MQTT] No active publish mode`

---

## 🔍 Monitoring

### Serial Monitor Output

Setelah mengirim config, monitor serial ESP32 untuk melihat:

```
[MQTT] Loading MQTT configuration...
[MQTT] Config loaded - Broker: demo.thingsboard.io:1883
[MQTT] Auth: YES, Mode: customize
[MQTT] Default Mode: DISABLED
[MQTT] Custom Topic: sensor/temperature, Registers: 3, Interval: 5s (5000ms)
[MQTT] Custom Topic: sensor/pressure, Registers: 2, Interval: 10s (10000ms)
[MQTT] Customize Mode: ENABLED, Topics: 2
```

### MQTT Publish Log

```
[MQTT] Customize Mode: Published 3 registers to sensor/temperature (Interval: 5s)
[MQTT] Customize Mode: Published 2 registers to sensor/pressure (Interval: 10s)
```

---

## 🧪 Checklist Testing

### Default Mode

- [ ] Test interval seconds (5s)
- [ ] Test interval milliseconds (3000ms)
- [ ] Test interval minutes (1m)
- [ ] Verify conversion di serial monitor
- [ ] Verify batch publishing (all registers in 1 message)

### Customize Mode

- [ ] Test 2 topics dengan interval berbeda
- [ ] Test 3 unit interval (ms/s/m) dalam 1 config
- [ ] Test register overlap (register di multiple topics)
- [ ] Test warehouse scenario (4 topics)
- [ ] Verify per-topic interval tracking
- [ ] Verify register filtering by index

### Edge Cases

- [ ] Disable both modes (MQTT connected, no publish)
- [ ] Switch from default to customize
- [ ] Switch from customize to default
- [ ] Test dengan 10+ custom topics
- [ ] Test dengan 50+ registers

---

## 📊 Expected Results

### ✅ Success Indicators

1. **BLE Connection:**

   ```
   ✅ Connected to SURIOTA GW (XX:XX:XX:XX:XX:XX)
   ```

2. **Command Sent:**

   ```
   📤 Sending command (XXX bytes):
   📥 Fragment: '...'
   📦 Complete response received
   📋 Parsed response: {...}
   ```

3. **Serial Monitor (ESP32):**
   ```
   [MQTT] Default Mode: ENABLED, Interval: 5s (5000ms)
   [MQTT] Customize Mode: ENABLED, Topics: 2
   [MQTT] Custom Topic: sensor/temperature, Registers: 3, Interval: 5s (5000ms)
   ```

### ❌ Failure Indicators

1. **BLE Not Found:**

   ```
   ❌ Service SURIOTA GW not found
   ```

2. **Parse Error:**

   ```
   ❌ Failed to parse response: Expecting value: line 1 column 1
   ```

3. **Serial Monitor Error:**
   ```
   [MQTT] Failed to load config
   [MQTT] Invalid interval_unit
   ```

---

## 🐛 Troubleshooting

### Issue: BLE tidak terdeteksi

**Solusi:**

- Pastikan gateway dalam mode BLE advertising
- Restart gateway
- Pastikan Bluetooth adapter PC aktif

### Issue: Response tidak lengkap

**Solusi:**

- Increase `await asyncio.sleep(2.0)` di send_command()
- Check MTU size BLE

### Issue: Config tidak tersimpan

**Solusi:**

- Verify JSON format valid
- Check serial monitor untuk error message
- Pastikan SPIFFS tidak full

---

## 📚 Reference

- **Dokumentasi MQTT Modes:** `MQTT_PUBLISH_MODES_DOCUMENTATION.md`
- **HTML Mockup:** `Gateway_Server_Configuration_Enhanced_Fixed.html`
- **Firmware Code:**
  - `ServerConfig.cpp`
  - `MqttManager.h`
  - `MqttManager.cpp`

---

## 👨‍💻 Author

Test script untuk SRT-MGATE-1210 Firmware v2.0 MQTT Publish Modes feature.

Last Updated: November 8, 2025
