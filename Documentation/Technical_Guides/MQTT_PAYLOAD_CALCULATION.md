# MQTT Payload Size Calculation Guide

**Version:** 1.0
**Date:** 2025-11-22
**Project:** SRT-MGATE-1210 Gateway

---

## 📐 Formula Kalkulasi

### Base Formula
```
Total Payload Size = Base Overhead + (Register Count × Bytes per Register)
                   = 200 bytes + (N × 42 bytes)

Total MQTT Packet = Fixed Header (5) + Topic Length (2) + Topic Name + Payload
```

### Bytes per Register
```
Average bytes per register: 42 bytes
├── Register name: ~12 bytes (e.g., "Temp_Zone_1")
├── Value: ~5 bytes (e.g., 26, 1033, 9999)
├── Unit: ~5 bytes (e.g., "degC", "Pa", "kWh")
└── JSON overhead: ~20 bytes ({}, :, ",")
```

---

## 📊 Kalkulasi untuk Berbagai Jumlah Register

| Registers | Calculation | Payload Size | MQTT Packet | Broker Compatibility |
|-----------|-------------|--------------|-------------|---------------------|
| **10** | 200 + (10 × 42) | **620 bytes** | ~680 bytes | ✅ All brokers (safe) |
| **15** | 200 + (15 × 42) | **830 bytes** | ~890 bytes | ✅ All brokers (safe) |
| **20** | 200 + (20 × 42) | **1040 bytes** | ~1100 bytes | ✅ All brokers (safe) |
| **25** | 200 + (25 × 42) | **1250 bytes** | ~1310 bytes | ✅ Most brokers (safe) |
| **30** | 200 + (30 × 42) | **1460 bytes** | ~1520 bytes | ✅ Most brokers (safe) |
| **35** | 200 + (35 × 42) | **1670 bytes** | ~1730 bytes | ✅ Most brokers (safe) |
| **40** | 200 + (40 × 42) | **1880 bytes** | ~1940 bytes | ⚠️ Public brokers (risky) |
| **45** | 200 + (45 × 42) | **2090 bytes** | ~2150 bytes | ⚠️ Public brokers (risky) |
| **50** | 200 + (50 × 42) | **2300 bytes** | ~2360 bytes | ❌ Public brokers (fail) |
| **55** | 200 + (55 × 42) | **2510 bytes** | ~2570 bytes | ❌ Public brokers (fail) |
| **59** | 200 + (59 × 42) | **2678 bytes** | ~2738 bytes | ❌ Public brokers (fail) |
| **70** | 200 + (70 × 42) | **3140 bytes** | ~3200 bytes | ❌ Paid brokers only |
| **100** | 200 + (100 × 42) | **4400 bytes** | ~4460 bytes | ❌ Private brokers only |

**Note:** Actual measurements from our system:
- 45 registers = 1856 bytes (actual) vs 2090 bytes (calculated)
- Formula gives ~12% overhead for safety margin

---

## 🌐 MQTT Broker Limits

### Public Brokers (FREE)

| Broker | Max Payload | Max Packet | QoS | Notes |
|--------|-------------|------------|-----|-------|
| **broker.hivemq.com** | ~2 KB | ~2.1 KB | 0, 1 | Most common, strict limit |
| **mqtt.eclipse.org** | ~2 KB | ~2.1 KB | 0, 1, 2 | Similar to HiveMQ |
| **test.mosquitto.org** | ~4 KB | ~4.1 KB | 0, 1, 2 | More relaxed |
| **broker.emqx.io** | ~1 MB | ~1 MB | 0, 1, 2 | Very relaxed (free tier) |

### Private Brokers (PAID)

| Type | Max Payload | Max Packet | Cost |
|------|-------------|------------|------|
| **AWS IoT Core** | 128 KB | 128 KB | $1-5/month |
| **Azure IoT Hub** | 256 KB | 256 KB | $10-50/month |
| **ThingsBoard** | 64 KB | 64 KB | $0-100/month |
| **Mosquitto (self-hosted)** | Configurable | Up to 256 MB | Free (server cost) |

---

## 📈 Register Limits by Broker Type

### broker.hivemq.com (Current Setup)
```
Safe limit: ≤ 40 registers (~1880 bytes)
Warning zone: 41-45 registers (~1880-2090 bytes)
Fail zone: ≥ 46 registers (>2090 bytes)

Recommendation: MAX 40 REGISTERS for default mode
```

### test.mosquitto.org (Alternative)
```
Safe limit: ≤ 90 registers (~3980 bytes)
Warning zone: 91-95 registers
Fail zone: ≥ 96 registers

Recommendation: MAX 90 REGISTERS for default mode
```

### Private Broker (Recommended for Production)
```
Safe limit: ≤ 3000 registers (~128 KB)
No practical limit for industrial IoT use cases

Recommendation: Use custom mode for organization
```

---

## 💡 Recommendations

### Scenario 1: ≤ 40 Registers
**Use: Default Mode**
```json
{
  "protocol": "mqtt",
  "mqtt": {
    "publish_mode": "default",
    "default_mode": {
      "enabled": true,
      "topic_publish": "v1/devices/me/telemetry",
      "interval": 60,
      "interval_unit": "s"
    }
  }
}
```
✅ Simple configuration
✅ Works with all public brokers
✅ Single topic, easy to debug

---

### Scenario 2: 41-90 Registers (test.mosquitto.org)
**Option A: Default Mode (switch broker)**
```json
{
  "mqtt": {
    "broker_address": "test.mosquitto.org",
    "broker_port": 1883,
    "publish_mode": "default"
  }
}
```
✅ Still simple
✅ More relaxed broker
⚠️ Still single point of failure

**Option B: Custom Mode (keep broker.hivemq.com)**
Split into 2-3 topics
```json
{
  "publish_mode": "customize",
  "customize_mode": {
    "enabled": true,
    "custom_topics": [
      {
        "topic": "device/group1",
        "interval": 60,
        "registers": [ /* 30 registers */ ]
      },
      {
        "topic": "device/group2",
        "interval": 60,
        "registers": [ /* 30 registers */ ]
      }
    ]
  }
}
```
✅ Works with strict brokers
✅ Better organization
✅ Flexible intervals

---

### Scenario 3: 59 Registers (YOUR CASE)
**Recommendation: Custom Mode (4 topics)**

Split by sensor type:
```
Topic 1: Temperature (10 regs)  → ~620 bytes  ✅
Topic 2: Humidity (10 regs)     → ~620 bytes  ✅
Topic 3: Electrical (20 regs)   → ~1040 bytes ✅
Topic 4: Sensors (19 regs)      → ~998 bytes  ✅
```

**Benefits:**
- All payloads < 2KB (broker safe)
- Different update intervals possible
- Better data organization
- Selective subscription

---

### Scenario 4: 91-200 Registers
**Recommendation: Custom Mode (mandatory)**

Split into 5-10 topics by:
- Sensor type (temperature, pressure, etc.)
- Update frequency (fast: 10s, medium: 60s, slow: 300s)
- Criticality (critical, normal, diagnostic)

Use private broker or test.mosquitto.org

---

### Scenario 5: 200+ Registers
**Recommendation: Private Broker + Custom Mode**

Requirements:
- Dedicated MQTT broker (AWS IoT, Azure, self-hosted)
- Custom topic hierarchy
- Database for historical data
- Multiple gateways support

Example:
```
factory/
├── building1/
│   ├── floor1/
│   │   ├── temperature (50 regs)
│   │   ├── electrical (80 regs)
│   │   └── machinery (70 regs)
│   └── floor2/...
└── building2/...
```

---

## 🎯 Quick Decision Matrix

| Register Count | Broker Type | Mode | Topics | Notes |
|---------------|-------------|------|--------|-------|
| 1-40 | Public (free) | Default | 1 | ✅ Simplest |
| 41-50 | Public (free) | Custom | 2 | ⚠️ Split recommended |
| 51-90 | test.mosquitto.org | Default/Custom | 1-3 | ⚠️ Consider private |
| 91-200 | Private (paid) | Custom | 5-10 | ✅ Production ready |
| 200+ | Private (paid) | Custom | 10+ | ✅ Enterprise |

---

## 📝 Calculation Examples

### Example 1: Your Current Setup (45 registers)
```
Payload = 200 + (45 × 42) = 2090 bytes (calculated)
Actual measurement: 1856 bytes
MQTT packet = 5 + 2 + 25 + 1856 = ~1888 bytes

Topic: "v1/devices/me/telemetry" (25 bytes)
Status: ⚠️ RISKY (close to 2KB limit)
```

### Example 2: Optimized with Custom Mode (59 registers → 4 topics)
```
Topic 1 "device/temp":     200 + (10 × 42) = 620 bytes  ✅
Topic 2 "device/humid":    200 + (10 × 42) = 620 bytes  ✅
Topic 3 "device/electric": 200 + (20 × 42) = 1040 bytes ✅
Topic 4 "device/sensors":  200 + (19 × 42) = 998 bytes  ✅

Total bandwidth: 3278 bytes (spread over 4 publishes)
Status: ✅ SAFE (all < 2KB)
```

### Example 3: Maximum for broker.hivemq.com
```
Max safe registers = (2000 - 200) / 42 = ~42 registers
Recommended safe limit = 40 registers (safety margin)
```

---

## ⚙️ Configuration Generator

Use this formula to split registers optimally:

```python
def calculate_optimal_split(total_registers, max_payload=2000):
    bytes_per_register = 42
    base_overhead = 200

    max_regs_per_topic = (max_payload - base_overhead) / bytes_per_register
    num_topics = ceil(total_registers / max_regs_per_topic)
    regs_per_topic = ceil(total_registers / num_topics)

    return num_topics, regs_per_topic

# Example: 59 registers
topics, regs = calculate_optimal_split(59)
# Result: 2 topics, 30 registers each
# But we use 4 topics for better organization!
```

---

## 🔍 Validation Checklist

Before publishing with new register count:

- [ ] Calculate payload size: `200 + (N × 42)` bytes
- [ ] Check if < broker limit (2KB for public)
- [ ] Add 10% safety margin
- [ ] Test with actual data
- [ ] Monitor broker connection stability
- [ ] Check for disconnect errors in logs
- [ ] Verify all registers present in payload

---

## 📚 References

- [MQTT 3.1.1 Specification](http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/mqtt-v3.1.1.html)
- [PubSubClient Library](https://pubsubclient.knolleary.net/)
- [HiveMQ Public Broker](https://www.hivemq.com/mqtt-cloud-broker/)
- ArduinoJson v7 Documentation

---

**Last Updated:** 2025-11-22
**Author:** Kemal (Suriota R&D)
