# 📊 Gateway Capacity Analysis

**SRT-MGATE-1210 Modbus IIoT Gateway**

Analisis mendalam tentang kapasitas register dan device yang dapat ditampung
oleh gateway.

**Updated for v2.1.1:** November 14, 2025 (Friday) - WIB (GMT+7) **Developer:**
Kemal

---

## 📋 Table of Contents

- [Hardware Resources](#hardware-resources)
- [Memory Architecture](#memory-architecture)
- [Theoretical Capacity](#theoretical-capacity)
- [Practical Capacity](#practical-capacity)
- [Performance Impact](#performance-impact)
- [Recommendations](#recommendations)

---

## Hardware Resources

### ESP32-S3 Memory Specifications

| Resource               | Capacity       | Purpose                        |
| ---------------------- | -------------- | ------------------------------ |
| **Internal SRAM**      | 512 KB         | Program execution, stack, heap |
| **External PSRAM**     | 8 MB (8192 KB) | Large data structures, buffers |
| **Flash Storage**      | 16 MB          | Firmware code, file system     |
| **LittleFS Partition** | ~14 MB         | Configuration files, queues    |

### Memory Allocation Strategy

```cpp
// ConfigManager allocation
configManager = heap_caps_malloc(sizeof(ConfigManager), MALLOC_CAP_SPIRAM);

// JSON Document allocation
devicesCache = new JsonDocument();      // Dynamic allocation in PSRAM
registersCache = new JsonDocument();    // Dynamic allocation in PSRAM

// Queue capacity
MAX_QUEUE_SIZE = 100;                   // Data points
MAX_STREAM_QUEUE_SIZE = 50;             // Streaming data points
```

---

## Memory Architecture

### Configuration Storage

**File-based Storage (LittleFS)**:

```
/devices.json          → All device configurations
/registers.json        → All register configurations (if separate)
/server_config.json    → MQTT/HTTP settings (~1 KB)
/logging_config.json   → Log settings (~0.5 KB)
/mqtt_queue.dat        → Persistent MQTT queue (~100 KB max)
```

**In-Memory Cache (PSRAM)**:

```cpp
JsonDocument *devicesCache;    // Loaded on-demand
JsonDocument *registersCache;  // Loaded on-demand

// PSRAM allocation threshold
shouldUsePsram(size > 1024);   // Use PSRAM for files > 1KB
```

### Memory Usage per Configuration Object

#### Per Device Configuration

**Minimal Device (JSON)**:

```json
{
  "D_ABC123": {
    "device_name": "Sensor 1",
    "protocol": "RTU",
    "slave_id": 1,
    "serial_port": 1,
    "baud_rate": 9600,
    "timeout": 3000,
    "enabled": true,
    "refresh_rate_ms": 5000,
    "registers": []
  }
}
```

**Estimated Size**: ~200 bytes (without registers)

#### Per Register Configuration

**Typical Register (JSON)**:

```json
{
  "address": 40001,
  "register_name": "TEMPERATURE",
  "type": "Holding Register",
  "function_code": 3,
  "data_type": "FLOAT32_BE",
  "scale": 1.0,
  "offset": 0.0,
  "unit": "°C",
  "enabled": true,
  "refresh_rate_ms": 1000
}
```

**Estimated Size**: ~180 bytes per register

#### Runtime Data Point (Queue)

**Data Point in Queue**:

```json
{
  "device_id": "D_ABC123",
  "register_name": "TEMPERATURE",
  "value": 23.5,
  "timestamp": 1704326400,
  "quality": "good"
}
```

**Estimated Size**: ~120 bytes per data point

---

## Theoretical Capacity

### Maximum Configuration Storage (LittleFS)

**Available Flash Storage**: 14 MB (14,680,064 bytes)

**Reserved System Files**: ~1 MB

- Server config, logging config, MQTT queue, system files

**Available for Configurations**: ~13 MB (13,631,488 bytes)

### Configuration Capacity Calculation

#### Scenario 1: Device-Heavy Configuration

**Assumptions**:

- Average device size: 200 bytes (minimal registers)
- Average 5 registers per device
- Total per device: 200 + (5 × 180) = 1,100 bytes

**Maximum Devices**:

```
13,631,488 bytes ÷ 1,100 bytes = 12,392 devices
```

#### Scenario 2: Register-Heavy Configuration

**Assumptions**:

- Average device size: 200 bytes
- Average 50 registers per device
- Total per device: 200 + (50 × 180) = 9,200 bytes

**Maximum Devices**:

```
13,631,488 bytes ÷ 9,200 bytes = 1,482 devices
```

#### Scenario 3: Maximum Registers per Device

**Assumptions**:

- Single device: 200 bytes
- All remaining space for registers

**Maximum Registers (Single Device)**:

```
(13,631,488 - 200) bytes ÷ 180 bytes = 75,729 registers
```

### PSRAM Runtime Capacity

**Available PSRAM**: 8 MB (8,388,608 bytes)

**System Overhead**: ~2 MB (FreeRTOS, WiFi/BLE stacks, firmware)

**Available for Configuration Cache**: ~6 MB (6,291,456 bytes)

**Maximum Registers in PSRAM Cache**:

```
6,291,456 bytes ÷ 180 bytes = 34,952 registers
```

**Queue Capacity**:

```
Data Queue: 100 data points × 120 bytes = 12,000 bytes (12 KB)
Stream Queue: 50 data points × 120 bytes = 6,000 bytes (6 KB)
Total Queue: 18 KB
```

---

## Practical Capacity

### Real-World Constraints

**1. Modbus RTU Polling Performance**

- **Polling Time**: 50-100 ms per register
- **2 Serial Ports**: Can poll ~20 registers/second (combined)
- **At 1-second refresh rate**: 20 registers polled per cycle
- **At 5-second refresh rate**: 100 registers polled per cycle

**2. JSON Parsing Performance**

- **Large JSON Documents**: Parsing time increases non-linearly
- **Recommended Max JSON Size**: 1 MB for stable performance
- **Practical Device Limit**: ~5,000 devices in single file

**3. Network Throughput**

- **MQTT Publish Rate**: ~10 messages/second (sustainable)
- **HTTP Request Rate**: ~5 requests/second (sustainable)
- **BLE Characteristic MTU**: 512 bytes (fragmentation for larger data)

**4. Memory Fragmentation**

- **PSRAM Fragmentation**: Occurs with frequent alloc/free
- **Minimum Free Buffer**: 10 KB reserved for safety
- **Recommended Usage**: <80% of PSRAM (6.4 MB used, 1.6 MB free)

### Recommended Practical Limits

#### Configuration Limits

| Scenario                | Devices | Registers/Device | Total Registers | Notes                         |
| ----------------------- | ------- | ---------------- | --------------- | ----------------------------- |
| **Small Installation**  | 10      | 10               | 100             | Typical home/small office     |
| **Medium Installation** | 50      | 20               | 1,000           | Factory floor, building       |
| **Large Installation**  | 200     | 50               | 10,000          | Industrial campus             |
| **Maximum Recommended** | 500     | 100              | 50,000          | Enterprise deployment         |
| **Theoretical Maximum** | 1,000   | 500              | 500,000         | Not recommended (performance) |

#### Performance-Based Limits

**Refresh Rate Constraints**:

| Refresh Rate   | Max Registers (RTU) | Max Registers (TCP) | Notes                       |
| -------------- | ------------------- | ------------------- | --------------------------- |
| **1 second**   | 20                  | 40                  | Real-time monitoring        |
| **5 seconds**  | 100                 | 200                 | Standard industrial polling |
| **10 seconds** | 200                 | 400                 | Slow-changing values        |
| **30 seconds** | 600                 | 1,200               | Energy meters, totals       |
| **60 seconds** | 1,200               | 2,400               | Historical trending         |

**Combined Protocol Limits**:

- 2x Modbus RTU ports + Modbus TCP = ~3,600 registers at 1-second refresh
- Recommended: 1,000-2,000 registers for stable operation

---

## Performance Impact

### Polling Performance vs. Register Count

```
Register Count → Polling Cycle Time (RTU @ 9600 baud)

10 registers    → 0.5-1 second
50 registers    → 2.5-5 seconds
100 registers   → 5-10 seconds
500 registers   → 25-50 seconds
1,000 registers → 50-100 seconds (1.7 minutes)
10,000 registers→ 500-1,000 seconds (16 minutes)
```

### Memory Usage vs. Configuration Size

```
Configuration Size → PSRAM Usage (estimated)

100 registers   → 20 KB (0.2%)
1,000 registers → 200 KB (2.4%)
10,000 registers→ 2 MB (24%)
50,000 registers→ 10 MB (exceeds PSRAM capacity)
```

### JSON Parsing Time

```
JSON File Size → Parse Time (approximate)

10 KB  → 10 ms
100 KB → 50 ms
1 MB   → 500 ms (0.5 seconds)
5 MB   → 3-5 seconds (noticeable lag)
10 MB  → 10-15 seconds (system sluggish)
```

### BLE Transmission Performance (v2.1.1+)

**📊 Performance After Optimization:**

The BLE transmission layer was significantly optimized in v2.1.1 to eliminate
timeout issues.

#### Optimization Details

```cpp
// BLEManager.h constants

// BEFORE (v2.0):
#define CHUNK_SIZE 18           // Too small
#define FRAGMENT_DELAY_MS 50    // Too slow

// AFTER (v2.1.1):
#define CHUNK_SIZE 244          // MTU-safe (512-byte MTU minus overhead)
#define FRAGMENT_DELAY_MS 10    // Reduced delay
```

#### Transmission Time Comparison

```
Payload Size → Before v2.1.1 → After v2.1.1 → Improvement

6 KB  (100 regs minimal)  → 16.7 sec → 0.6 sec  → 28x faster ⚡
10.5 KB (50 regs full)    → 29.2 sec → 1.05 sec → 28x faster ⚡
21 KB (100 regs full)     → 58.4 sec → 2.1 sec  → 28x faster ⚡
50 KB (500 regs minimal)  → 139 sec  → 5.0 sec  → 28x faster ⚡
```

**Impact:** Mobile app timeouts (typically 30 seconds) are now completely
eliminated.

#### Configuration Size vs. BLE Transmission Time

```
Configuration        → Payload (full) → Payload (minimal) → Time (full) → Time (minimal)

10 devices, 10 regs   → 2.1 KB  → 0.6 KB   → 0.2 sec → 0.06 sec
50 devices, 20 regs   → 21 KB   → 6 KB     → 2.1 sec → 0.6 sec
100 devices, 10 regs  → 21 KB   → 6 KB     → 2.1 sec → 0.6 sec
100 devices, 100 regs → 210 KB  → 60 KB    → 21 sec  → 6 sec
200 devices, 50 regs  → 210 KB  → 60 KB    → 21 sec  → 6 sec
```

**Recommendations:**

- ✅ Use `minimal=true` parameter for `devices_with_registers` API when only
  need IDs and names
- ✅ Payload size reduces by **71%** with minimal mode
- ✅ All responses under 5 seconds with up to 100 devices × 100 registers (full
  mode)

#### Network Comparison (v2.1.1)

| Transport     | Typical Latency    | Max Payload Size  | Notes               |
| ------------- | ------------------ | ----------------- | ------------------- |
| **BLE**       | 0.6-2.1 sec (21KB) | ~210 KB practical | Optimized in v2.1.1 |
| **WiFi MQTT** | 50-200 ms          | Unlimited         | For data publishing |
| **WiFi HTTP** | 100-500 ms         | Unlimited         | For data publishing |

**Key Takeaway:** BLE is now **viable for large configurations** (100+ devices)
thanks to v2.1.1 optimization.

---

## Recommendations

### Optimal Configuration Ranges

#### 1. **Recommended Range (Best Performance)**

```
Devices: 20-100
Registers per device: 10-50
Total registers: 200-5,000
Refresh rate: 1-10 seconds
```

**Benefits**:

- ✅ Fast response times (<1 second)
- ✅ Low memory usage (<1 MB PSRAM)
- ✅ Quick JSON parsing (<100 ms)
- ✅ Stable long-term operation

#### 2. **Extended Range (Good Performance)**

```
Devices: 100-300
Registers per device: 20-100
Total registers: 5,000-30,000
Refresh rate: 5-30 seconds
```

**Trade-offs**:

- ⚠️ Moderate response times (1-3 seconds)
- ⚠️ Higher memory usage (2-5 MB PSRAM)
- ⚠️ Longer polling cycles (minutes)
- ✅ Still stable with proper configuration

#### 3. **Maximum Range (Acceptable Performance)**

```
Devices: 300-500
Registers per device: 50-100
Total registers: 30,000-50,000
Refresh rate: 30-60 seconds
```

**Considerations**:

- ⚠️ Slower response times (3-10 seconds)
- ⚠️ High memory usage (5-7 MB PSRAM)
- ⚠️ Long polling cycles (10-20 minutes)
- ⚠️ Risk of memory fragmentation
- ⚠️ Requires careful configuration

#### 4. **Beyond Limits (Not Recommended)**

```
Total registers: >50,000
Devices: >500
```

**Problems**:

- ❌ PSRAM exhaustion
- ❌ JSON parsing timeouts
- ❌ System instability
- ❌ Frequent crashes/reboots
- ❌ Poor user experience

---

## Configuration Best Practices

### 1. **Distribute Load Across Ports**

```json
// Good: Balanced across 2 RTU ports
Port 1: 500 registers (25 devices × 20 registers)
Port 2: 500 registers (25 devices × 20 registers)
TCP: 500 registers (10 devices × 50 registers)
Total: 1,500 registers

// Bad: Unbalanced
Port 1: 1,400 registers
Port 2: 50 registers
TCP: 50 registers
Total: 1,500 registers (Port 1 becomes bottleneck)
```

### 2. **Optimize Refresh Rates**

```json
// Critical values → Fast refresh
"temperature": {"refresh_rate_ms": 1000}   // 1 second

// Standard values → Normal refresh
"voltage": {"refresh_rate_ms": 5000}       // 5 seconds

// Slow-changing values → Slow refresh
"total_energy": {"refresh_rate_ms": 60000} // 60 seconds

// Static values → Very slow refresh
"device_serial": {"refresh_rate_ms": 300000} // 5 minutes
```

### 3. **Use Device-Level Refresh with Register Overrides**

```json
{
  "device_name": "Power Meter",
  "refresh_rate_ms": 10000, // Default: 10 seconds
  "registers": [
    {
      "register_name": "alarm_status",
      "refresh_rate_ms": 1000 // Override: 1 second (critical)
    },
    {
      "register_name": "total_kwh"
      // Uses device default: 10 seconds
    }
  ]
}
```

### 4. **Disable Unused Registers**

```json
{
  "register_name": "reserved_register",
  "enabled": false // Skip polling
}
```

### 5. **Group Related Registers**

For devices with sequential register addresses, polling is more efficient:

```json
// Good: Sequential addresses (single Modbus read)
Address 40001-40010 → 10 registers in 1 request

// Less efficient: Scattered addresses
Address 40001, 40050, 40100 → 3 separate requests
```

---

## Scaling Strategies

### Vertical Scaling (Single Gateway)

**Current Gateway Capacity**:

- Recommended: 1,000-5,000 registers
- Maximum: 30,000-50,000 registers

**Optimization Techniques**:

1. Increase refresh rates for non-critical data
2. Disable unused registers
3. Use batch polling for sequential addresses
4. Implement selective streaming (only changed values)

### Horizontal Scaling (Multiple Gateways)

**When to Use Multiple Gateways**:

- Total registers >10,000
- Geographic distribution (multiple buildings)
- Network segmentation (separate VLANs)
- Redundancy requirements

**Architecture Example**:

```
Building A: Gateway 1 (2,000 registers)
Building B: Gateway 2 (2,000 registers)
Building C: Gateway 3 (2,000 registers)
Central MQTT Broker: Aggregates all data
Total System: 6,000 registers
```

---

## Monitoring & Diagnostics

### Memory Monitoring

```cpp
// Check PSRAM usage
Serial.printf("PSRAM free: %d bytes\n", ESP.getFreePsram());

// Should maintain >20% free (1.6 MB)
if (ESP.getFreePsram() < 1600000) {
  Serial.println("WARNING: PSRAM nearly full!");
}
```

### Performance Monitoring

```cpp
// Polling cycle time
uint32_t cycleStart = millis();
// ... poll all devices ...
uint32_t cycleTime = millis() - cycleStart;

// Should be <10 seconds for 1,000 registers
if (cycleTime > 10000) {
  Serial.println("WARNING: Polling cycle too slow!");
}
```

### Configuration Size Check

```bash
# Check devices.json size
ls -lh /littlefs/devices.json

# Should be <1 MB for good performance
# Warning if >5 MB
```

---

## Summary

### Quick Reference Table

| Configuration Size | Devices | Registers | PSRAM  | Performance | Status             |
| ------------------ | ------- | --------- | ------ | ----------- | ------------------ |
| **Small**          | 10-50   | 100-500   | <500KB | Excellent   | ✅ Recommended     |
| **Medium**         | 50-200  | 500-5K    | 1-2MB  | Good        | ✅ Recommended     |
| **Large**          | 200-500 | 5K-30K    | 2-5MB  | Acceptable  | ⚠️ Requires tuning |
| **Very Large**     | >500    | >30K      | >5MB   | Poor        | ❌ Not recommended |

### Key Takeaways

1. **Theoretical Maximum**: ~500,000 registers (storage-limited)
2. **PSRAM Maximum**: ~35,000 registers (memory-limited)
3. **Performance Maximum**: ~10,000 registers (polling-limited)
4. **Recommended Optimal**: 1,000-5,000 registers (balanced)

### Final Recommendation

**For optimal performance and stability**:

```
✅ Target: 1,000-2,000 registers
✅ Devices: 50-100 devices
✅ Registers/Device: 20-40 registers
✅ Refresh Rate: 5-10 seconds
✅ PSRAM Usage: 1-2 MB (12-24%)
```

This configuration provides:

- Fast response times
- Stable operation
- Room for growth
- Good user experience

---

**Copyright © 2025 PT Surya Inovasi Prioritas (SURIOTA)**

_This document is part of the SRT-MGATE-1210 firmware documentation. Licensed
under MIT License._
