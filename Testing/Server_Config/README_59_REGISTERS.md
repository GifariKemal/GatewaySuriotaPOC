# Server Config untuk 59 Registers - Custom Mode

**Created:** 2025-11-22 **Purpose:** Konfigurasi MQTT untuk publish 59 registers
menggunakan Custom Mode

---

## 📊 Overview

Konfigurasi ini memecah **59 registers** menjadi **4 topics** untuk bypass
broker limit 2KB:

| Topic                        | Registers | Payload     | Interval | Priority    |
| ---------------------------- | --------- | ----------- | -------- | ----------- |
| `suriota/device/temperature` | 10        | ~620 bytes  | 60s      | Normal      |
| `suriota/device/humidity`    | 10        | ~620 bytes  | 60s      | Normal      |
| `suriota/device/electrical`  | 20        | ~1040 bytes | **30s**  | **HIGH** ⚡ |
| `suriota/device/sensors`     | 19        | ~998 bytes  | 120s     | Low         |

**Total:** 59 registers, semua payload < 2KB ✅

---

## 🚀 Cara Penggunaan

### 1. Upload Config via BLE

```json
{
  "op": "update",
  "type": "server_config",
  "config": {
    "protocol": "mqtt",
    "mqtt": {
      "broker_address": "broker.hivemq.com",
      "broker_port": 1883,
      "client_id": "SURIOTA_GW_001",
      "publish_mode": "customize",
      "customize_mode": {
        "enabled": true,
        "custom_topics": [
          {
            "topic": "suriota/device/temperature",
            "interval": 60,
            "interval_unit": "s",
            "registers": [
              "D57ff4_Temp_Zone_1",
              "D57ff4_Temp_Zone_2",
              "D57ff4_Temp_Zone_3",
              "D57ff4_Temp_Zone_4",
              "D57ff4_Temp_Zone_5",
              "D57ff4_Temp_Zone_6",
              "D57ff4_Temp_Zone_7",
              "D57ff4_Temp_Zone_8",
              "D57ff4_Temp_Zone_9",
              "D57ff4_Temp_Zone_10"
            ]
          },
          {
            "topic": "suriota/device/humidity",
            "interval": 60,
            "interval_unit": "s",
            "registers": [
              "D57ff4_Humid_Zone_1",
              "D57ff4_Humid_Zone_2",
              "D57ff4_Humid_Zone_3",
              "D57ff4_Humid_Zone_4",
              "D57ff4_Humid_Zone_5",
              "D57ff4_Humid_Zone_6",
              "D57ff4_Humid_Zone_7",
              "D57ff4_Humid_Zone_8",
              "D57ff4_Humid_Zone_9",
              "D57ff4_Humid_Zone_10"
            ]
          },
          {
            "topic": "suriota/device/electrical",
            "interval": 30,
            "interval_unit": "s",
            "registers": [
              "D57ff4_Voltage_L1",
              "D57ff4_Voltage_L2",
              "D57ff4_Voltage_L3",
              "D57ff4_Voltage_L4",
              "D57ff4_Voltage_L5",
              "D57ff4_Current_L1",
              "D57ff4_Current_L2",
              "D57ff4_Current_L3",
              "D57ff4_Current_L4",
              "D57ff4_Current_L5",
              "D57ff4_Power_1",
              "D57ff4_Power_2",
              "D57ff4_Power_3",
              "D57ff4_Power_4",
              "D57ff4_Power_5",
              "D57ff4_Energy_1",
              "D57ff4_Energy_2",
              "D57ff4_Energy_3",
              "D57ff4_Energy_4",
              "D57ff4_Energy_5"
            ]
          },
          {
            "topic": "suriota/device/sensors",
            "interval": 120,
            "interval_unit": "s",
            "registers": [
              "D57ff4_Press_Sensor_1",
              "D57ff4_Press_Sensor_2",
              "D57ff4_Press_Sensor_3",
              "D57ff4_Press_Sensor_4",
              "D57ff4_Press_Sensor_5"
            ]
          }
        ]
      }
    }
  }
}
```

### 2. Copy File ke SD Card

```bash
# Copy config file ke gateway
cp server_config_59_registers_custom_mode.json /path/to/sdcard/server_config.json

# Restart gateway
```

### 3. Subscribe di MQTT Client

**Subscribe semua topics:**

```bash
mosquitto_sub -h broker.hivemq.com -t "suriota/device/#" -v
```

**Subscribe topic tertentu:**

```bash
# Temperature only
mosquitto_sub -h broker.hivemq.com -t "suriota/device/temperature"

# Electrical only (high priority)
mosquitto_sub -h broker.hivemq.com -t "suriota/device/electrical"

# Humidity only
mosquitto_sub -h broker.hivemq.com -t "suriota/device/humidity"

# Sensors only
mosquitto_sub -h broker.hivemq.com -t "suriota/device/sensors"
```

---

## 📋 Expected Serial Monitor Output

```
[MQTT] Config loaded - Broker: broker.hivemq.com:1883, Client: SURIOTA_GW_001
[MQTT] Customize Mode: ENABLED, Topics: 4
[MQTT] Custom Topic: suriota/device/temperature, Registers: 10, Interval: 60s
[MQTT] Custom Topic: suriota/device/humidity, Registers: 10, Interval: 60s
[MQTT] Custom Topic: suriota/device/electrical, Registers: 20, Interval: 30s
[MQTT] Custom Topic: suriota/device/sensors, Registers: 19, Interval: 120s
[MQTT] Connected to broker.hivemq.com:1883 via ETH (192.168.1.100)

[MQTT] Customize Mode: Published 10 registers from 1 devices to suriota/device/temperature (0.6 KB) / 60s
[MQTT] Customize Mode: Published 20 registers from 1 devices to suriota/device/electrical (1.0 KB) / 30s
[MQTT] Customize Mode: Published 10 registers from 1 devices to suriota/device/humidity (0.6 KB) / 60s
[MQTT] Customize Mode: Published 19 registers from 1 devices to suriota/device/sensors (1.0 KB) / 120s
```

---

## 📊 Publish Timeline

```
Time     Topic                     Registers  Payload
─────────────────────────────────────────────────────
0:00     electrical (30s)          20         1040 bytes ✅
0:00     temperature (60s)         10          620 bytes ✅
0:00     humidity (60s)            10          620 bytes ✅
0:00     sensors (120s)            19          998 bytes ✅

0:30     electrical (30s)          20         1040 bytes ✅
1:00     electrical (30s)          20         1040 bytes ✅
1:00     temperature (60s)         10          620 bytes ✅
1:00     humidity (60s)            10          620 bytes ✅

1:30     electrical (30s)          20         1040 bytes ✅
2:00     electrical (30s)          20         1040 bytes ✅
2:00     temperature (60s)         10          620 bytes ✅
2:00     humidity (60s)            10          620 bytes ✅
2:00     sensors (120s)            19          998 bytes ✅
```

**Observation:**

- Electrical data updates **TWICE as fast** (30s vs 60s)
- Sensor data updates **HALF as fast** (120s vs 60s)
- Total bandwidth optimized based on priority

---

## 📈 Bandwidth Analysis

### Default Mode (Would FAIL):

```
Interval: 60s
Payload: 2678 bytes (59 registers)
Bandwidth: 2678 bytes / 60s = 44.6 bytes/sec
Status: ❌ FAIL (>2KB limit)
```

### Custom Mode (SUCCESS):

```
Topic 1 (60s):   620 bytes / 60s  = 10.3 bytes/sec
Topic 2 (60s):   620 bytes / 60s  = 10.3 bytes/sec
Topic 3 (30s):  1040 bytes / 30s  = 34.7 bytes/sec
Topic 4 (120s):  998 bytes / 120s = 8.3 bytes/sec
────────────────────────────────────────────────────
Total:                              63.6 bytes/sec
Status: ✅ SUCCESS (all topics < 2KB)
```

**Bandwidth increased by 43%** but all publishes succeed! ✅

---

## 🎯 Benefits

### 1. Bypass Broker Limit ✅

```
Default mode:  2678 bytes → ❌ FAIL
Custom mode:   4 × <1.1KB → ✅ SUCCESS
```

### 2. Prioritized Updates ✅

```
Electrical:   30s  (critical - power monitoring)
Temp/Humid:   60s  (normal - environmental)
Sensors:     120s  (low priority - misc)
```

### 3. Selective Subscription ✅

```
Dashboard A: Subscribe "suriota/device/electrical" only
Dashboard B: Subscribe "suriota/device/temperature" only
Dashboard C: Subscribe "suriota/device/#" (all)
```

### 4. Better Organization ✅

```
suriota/device/
├── temperature    (thermal monitoring)
├── humidity       (environmental)
├── electrical     (power monitoring)
└── sensors        (misc sensors)
```

---

## ⚠️ Important Notes

1. **Register ID Format:**
   - Must match device ID + register name
   - Format: `{device_id}_{register_name}`
   - Example: `D57ff4_Temp_Zone_1`

2. **Interval Units:**
   - Supported: `"s"` (seconds), `"m"` (minutes), `"ms"` (milliseconds)
   - Recommended: Use seconds for readability

3. **Broker Connection:**
   - Public broker: broker.hivemq.com (no auth)
   - Private broker: Update `broker_address`, `username`, `password`

4. **QoS Level:**
   - Default: QoS 1 (at least once delivery)
   - Public brokers may not support QoS 2

---

## 🔧 Troubleshooting

### Problem: "Waiting for batch completion"

```
Solution: Check if all registers exist in device config
Verify: All register IDs match device_id + register_name format
```

### Problem: "Publish failed"

```
Check: Payload size < 2KB for each topic
Check: Broker connection stable
Check: All register IDs valid
```

### Problem: "No data on topic"

```
Check: Register IDs correct in custom_topics
Check: Device enabled and polling
Check: Subscribe to correct topic name
```

---

## 📚 References

- [MQTT Payload Calculation Guide](./MQTT_PAYLOAD_CALCULATION.md)
- [BLE API Reference](../../Documentation/API_Reference/API.md)
- [Server Config Documentation](../../Documentation/Technical_Guides/SERVER_CONFIG.md)

---

**Last Updated:** 2025-11-22 **Author:** Kemal (Suriota R&D)
