# Best Practices Guide

**SRT-MGATE-1210 Modbus IIoT Gateway**
Recommended Configurations and Patterns

[Home](../README.md) > [Documentation](README.md) > Best Practices

---

## Overview

This guide provides recommended configurations, design patterns, and best practices for deploying the SRT-MGATE-1210 gateway in production environments. Following these guidelines will help ensure reliable operation, optimal performance, and easier maintenance.

---

## Table of Contents

1. [Network Configuration](#network-configuration)
2. [Device & Register Setup](#device--register-setup)
3. [MQTT Configuration](#mqtt-configuration)
4. [HTTP Configuration](#http-configuration)
5. [Security Best Practices](#security-best-practices)
6. [Performance Optimization](#performance-optimization)
7. [Deployment Checklist](#deployment-checklist)
8. [Monitoring & Maintenance](#monitoring--maintenance)
9. [Common Mistakes to Avoid](#common-mistakes-to-avoid)

---

## Network Configuration

### Use Dual Network with Failover (Recommended for Production)

**Configuration:**
```json
{
  "wifi": {
    "enabled": true,
    "ssid": "PRIMARY_WIFI",
    "password": "secure_password"
  },
  "ethernet": {
    "enabled": true,
    "dhcp": true
  },
  "communication": {
    "mode": "auto",
    "prefer_ethernet": true,
    "failover_delay": 5000
  }
}
```

**Why:**
- Ethernet provides stable, low-latency connection
- WiFi acts as automatic backup if Ethernet fails
- No manual intervention needed during network issues
- Gateway continues operating during cable disconnection

**When to use WiFi only:**
- Testing/development environments
- Temporary installations
- No Ethernet infrastructure available

**When to use Ethernet only:**
- Critical industrial environments
- High EMI/interference areas
- Maximum reliability required

### Network Settings Recommendations

| Setting | Recommended | Notes |
|---------|-------------|-------|
| **Primary Network** | Ethernet | More stable, lower latency |
| **Failover Delay** | 5000ms (5s) | Balance between responsiveness and stability |
| **DHCP** | Enabled | Easier deployment; use static only if required |
| **Preferred Network** | prefer_ethernet: true | Ethernet more reliable for industrial use |

### WiFi Configuration Tips

**Signal Strength:**
- Maintain RSSI > -70dBm for reliable operation
- Test connection at actual installation location
- Consider external antenna if signal is weak

**Channel Selection:**
- Avoid crowded 2.4GHz channels (1, 6, 11 recommended)
- Use WiFi analyzer to find least congested channel
- Industrial environments: hardwire if possible

**Security:**
- Always use WPA2 or WPA3 encryption
- Use strong passwords (min 12 characters)
- Change default SSID and password

---

## Device & Register Setup

### Device Configuration Best Practices

#### 1. Use Meaningful Device Names

**Good:**
```json
{
  "device_id": "DEVICE_001",
  "name": "Power Meter - Production Line 1",
  "slave_id": 1
}
```

**Bad:**
```json
{
  "device_id": "DEVICE_001",
  "name": "Device 1",
  "slave_id": 1
}
```

#### 2. Organize Devices Logically

**Recommended Device Naming Pattern:**
- `[Type] - [Location] - [Function]`
- Examples:
  - "Temp Sensor - Warehouse Zone A"
  - "Flow Meter - Line 2 Inlet"
  - "Power Meter - Main Distribution"

#### 3. Set Appropriate Polling Intervals

| Device Type | Recommended Interval | Reason |
|-------------|---------------------|---------|
| **Critical Sensors** | 1000-2000ms (1-2s) | Fast response to changes |
| **Temperature Sensors** | 5000-10000ms (5-10s) | Slow-changing values |
| **Power Meters** | 2000-5000ms (2-5s) | Balance accuracy & load |
| **Status Indicators** | 1000ms (1s) | Quick state detection |
| **Historical Data** | 30000-60000ms (30-60s) | Reduce load, save bandwidth |

**Example:**
```json
{
  "device_id": "DEVICE_001",
  "name": "Temperature Sensor - Server Room",
  "slave_id": 1,
  "protocol": "rtu",
  "refresh_rate_ms": 5000
}
```

### Register Configuration Best Practices

#### 1. Use Meaningful Register IDs

**Good:**
```json
{
  "register_id": "temp_server_room_1",
  "register_name": "Server Room Temperature",
  "address": 4001,
  "data_type": "FLOAT32_ABCD"
}
```

**Bad:**
```json
{
  "register_id": "reg1",
  "register_name": "Temperature",
  "address": 4001,
  "data_type": "FLOAT32_ABCD"
}
```

#### 2. Always Specify Units

```json
{
  "register_name": "Inlet Pressure",
  "unit": "bar",
  "scale": 0.01,
  "offset": 0.0
}
```

#### 3. Use Calibration When Needed

**Example: Voltage Divider**
```json
{
  "register_name": "DC Voltage",
  "scale": 0.01,
  "offset": 0.0,
  "unit": "V"
}
```
Raw: 2500 → Displayed: 25.0 V

**Example: Offset Correction**
```json
{
  "register_name": "Room Temperature",
  "scale": 1.0,
  "offset": -2.5,
  "unit": "°C"
}
```
Raw: 27.5°C → Displayed: 25.0°C

#### 4. Choose Correct Data Type

**Common Mistakes:**
- Using `INT16` for float values (loses decimal precision)
- Wrong endianness (ABCD vs CDAB)
- Wrong word order for 32-bit values

**Test Your Data Type:**
1. Read raw value from device manual
2. Try different data types
3. Verify displayed value matches expected
4. Document the correct type for future reference

See [MODBUS_DATATYPES.md](Technical_Guides/MODBUS_DATATYPES.md) for complete list.

### Modbus RTU/TCP Guidelines

**Baud Rate Selection:**
- **9600 baud**: Most compatible, use as default
- **19200 baud**: Faster, good if all devices support it
- **115200 baud**: Very fast, test thoroughly for stability

**Timeout & Retry Settings:**
```json
{
  "modbus_rtu": {
    "timeout": 1000,
    "retry_count": 3,
    "inter_frame_delay": 100
  }
}
```

**Recommendations:**
- Start with 1000ms timeout, adjust if needed
- Use 3 retries (balance reliability vs latency)
- Inter-frame delay: 100ms minimum for slower devices

---

## MQTT Configuration

### Production MQTT Setup

#### Default Mode (Recommended for Most Cases)

**Use when:**
- Sending all data to single platform (ThingsBoard, AWS IoT, etc.)
- Simplicity is priority
- All data has same update frequency

**Configuration:**
```json
{
  "mqtt": {
    "enabled": true,
    "broker": "mqtt.example.com",
    "port": 8883,
    "use_ssl": true,
    "client_id": "gateway-001",
    "username": "device_token",
    "password": "secure_password",
    "keepalive": 60,
    "qos": 1,
    "publish_mode": "default",
    "default_mode": {
      "enabled": true,
      "topic_publish": "v1/devices/me/telemetry",
      "interval": 5,
      "interval_unit": "s"
    }
  }
}
```

**Benefits:**
- Single message per interval (efficient)
- All data in one payload
- Easy backend integration

#### Customize Mode (Advanced)

**Use when:**
- Different consumers need different data
- Different update frequencies needed per sensor type
- Topic-based access control required

**Example: Multi-Consumer Architecture**
```json
{
  "publish_mode": "customize",
  "customize_mode": {
    "enabled": true,
    "custom_topics": [
      {
        "topic": "factory/realtime/critical",
        "registers": ["temp_reactor", "pressure_main"],
        "interval": 1,
        "interval_unit": "s"
      },
      {
        "topic": "factory/historical/environment",
        "registers": ["temp_zone_1", "temp_zone_2", "humidity"],
        "interval": 1,
        "interval_unit": "m"
      },
      {
        "topic": "factory/alerts/safety",
        "registers": ["smoke_detector", "co2_level"],
        "interval": 500,
        "interval_unit": "ms"
      }
    ]
  }
}
```

### MQTT Connection Settings

| Setting | Production | Development | Notes |
|---------|-----------|-------------|-------|
| **TLS/SSL** | Enabled | Optional | Always enable for production |
| **QoS** | 1 | 0 or 1 | QoS 1 ensures delivery |
| **Keep-Alive** | 60s | 60s | Standard value |
| **Clean Session** | false | true | Persist session in production |
| **Client ID** | Unique | Any | Use gateway serial/MAC |

### MQTT Topics Naming Convention

**Recommended Pattern:**
```
[company]/[location]/[line]/[device_type]/[metric]
```

**Examples:**
- `acme/factory1/line2/temperature/zone_a`
- `acme/warehouse/zone_b/humidity/sensor_1`
- `acme/building_a/floor_3/power/meter_main`

**Benefits:**
- Clear hierarchy
- Easy filtering
- Logical access control
- Scalable structure

---

## HTTP Configuration

### Production HTTP Setup

```json
{
  "http_config": {
    "enabled": true,
    "url": "https://api.example.com/gateway/data",
    "method": "POST",
    "interval": 5000,
    "timeout": 10000,
    "retry_count": 3,
    "headers": {
      "Authorization": "Bearer YOUR_API_TOKEN",
      "Content-Type": "application/json",
      "X-Gateway-ID": "gateway-001"
    }
  }
}
```

### HTTP Best Practices

**Interval Selection:**
- **High-frequency**: 1-5 seconds (real-time monitoring)
- **Medium-frequency**: 10-30 seconds (general monitoring)
- **Low-frequency**: 60+ seconds (historical data)

**Error Handling:**
- Set retry_count to 3 minimum
- Use reasonable timeout (10-15 seconds)
- Monitor HTTP errors in logs

**Security:**
- Always use HTTPS in production
- Use authentication tokens
- Rotate tokens regularly
- Don't hardcode credentials

**Bandwidth Optimization:**
- Use HTTP for data posting, MQTT for commands
- Combine with MQTT Default Mode for efficient payloads
- Adjust interval based on data change frequency

---

## Security Best Practices

### Network Security

**WiFi:**
- Use WPA2-Personal minimum (WPA3 preferred)
- Strong passwords (12+ characters, mixed case, numbers, symbols)
- Change default credentials immediately
- Use hidden SSID in sensitive environments

**Ethernet:**
- Use VLANs to isolate IoT devices
- Implement network segmentation
- Firewall rules to restrict traffic

### API Security

**BLE Security:**
- Enable BLE pairing if supported
- Limit physical access to device
- Disconnect BLE when not in use
- Monitor unauthorized connection attempts

**MQTT Security:**
- Always use TLS/SSL in production (port 8883)
- Use strong username/password or certificates
- Implement client certificate auth for critical deployments
- Rotate credentials regularly

**HTTP Security:**
- Use HTTPS only (never HTTP)
- API tokens instead of passwords
- Implement token expiration
- Validate SSL certificates

### Data Security

**Sensitive Data:**
- Don't log passwords or tokens
- Use environment variables for credentials
- Encrypt stored configurations
- Implement secure boot if available

---

## Performance Optimization

### Polling Interval Optimization

**Rule of Thumb:**
- Match polling interval to data change rate
- Temperature: 5-10s (slow changing)
- Pressure/Flow: 2-5s (medium changing)
- Alarms/Status: 1s (fast changing)

**Example Configuration:**
```json
{
  "devices": [
    {
      "name": "Temperature Sensors",
      "refresh_rate_ms": 5000
    },
    {
      "name": "Flow Meters",
      "refresh_rate_ms": 2000
    },
    {
      "name": "Alarm Panel",
      "refresh_rate_ms": 1000
    }
  ]
}
```

### Modbus Performance

**Multiple Devices:**
- Stagger polling times
- Avoid all devices polling simultaneously
- Use sequential polling (device 1, then 2, then 3...)

**Register Grouping:**
- Read consecutive registers together when possible
- Reduces Modbus transactions
- Faster overall read time

**Bandwidth Considerations:**

| Scenario | Registers | Interval | Bandwidth |
|----------|-----------|----------|-----------|
| Low | 10 | 10s | ~1 KB/min |
| Medium | 50 | 5s | ~10 KB/min |
| High | 100 | 2s | ~50 KB/min |

### Memory Management

**PSRAM Usage:**
- Gateway has 8MB PSRAM
- JsonDocument uses PSRAM for large payloads
- Monitor memory via logs
- Reduce polling frequency if memory issues occur

### Network Optimization

**Reduce MQTT Bandwidth:**
1. Use Default Mode (batch publishing)
2. Increase publish interval for slow-changing data
3. Use QoS 0 for non-critical data
4. Implement payload compression if supported

**Reduce HTTP Bandwidth:**
1. Increase posting interval
2. Only send changed values
3. Use efficient JSON structure
4. Enable gzip compression if backend supports

---

## Deployment Checklist

### Pre-Deployment

- [ ] Gateway powered and LEDs indicate normal operation
- [ ] Network connectivity tested (WiFi and/or Ethernet)
- [ ] BLE connection working from mobile device
- [ ] All Modbus devices responding
- [ ] Register data types verified correct
- [ ] Data displayed correctly (units, scaling)
- [ ] MQTT/HTTP connection successful
- [ ] Data visible on backend platform

### Network Configuration

- [ ] Dual network configured (if applicable)
- [ ] Failover tested (disconnect primary, verify backup)
- [ ] Static IP assigned (if DHCP not used)
- [ ] Firewall rules configured
- [ ] Network segmentation implemented

### Device Configuration

- [ ] All devices have meaningful names
- [ ] Slave IDs unique and documented
- [ ] Polling intervals optimized
- [ ] Timeout and retry settings tested
- [ ] All registers configured and tested

### Security Configuration

- [ ] Default passwords changed
- [ ] WPA2/WPA3 enabled on WiFi
- [ ] MQTT TLS/SSL enabled
- [ ] HTTPS used for HTTP endpoints
- [ ] API tokens configured
- [ ] Credentials documented securely

### Monitoring Setup

- [ ] Log level configured (INFO for production)
- [ ] Remote monitoring configured
- [ ] Alert thresholds set
- [ ] Backup communication path tested
- [ ] Maintenance schedule defined

### Documentation

- [ ] Network diagram created
- [ ] Device list documented
- [ ] Register map documented
- [ ] Configuration backed up
- [ ] Contact information updated

---

## Monitoring & Maintenance

### Regular Monitoring

**Daily:**
- Check gateway online status
- Verify data is being received
- Monitor for network drops

**Weekly:**
- Review error logs
- Check memory usage
- Verify all devices responding
- Test failover (if applicable)

**Monthly:**
- Update firmware if available
- Review and optimize polling intervals
- Clean physical connections
- Backup configuration

### Log Levels

**Production:**
```json
{
  "log_level": "INFO"
}
```

**Troubleshooting:**
```json
{
  "log_level": "DEBUG"
}
```

**Log Analysis:**
- Look for repeated errors
- Monitor memory warnings
- Track network disconnections
- Review Modbus timeouts

See [LOGGING.md](Technical_Guides/LOGGING.md) for complete logging guide.

### Performance Monitoring

**Key Metrics:**
- Network uptime %
- MQTT connection stability
- Modbus read success rate
- Memory usage
- CPU usage

**Thresholds:**
- Network uptime: > 99%
- MQTT connection: Should reconnect within 30s
- Modbus success: > 95%
- Memory: < 80% usage

---

## Common Mistakes to Avoid

### Configuration Errors

**❌ Mistake 1: Wrong Data Type**
```json
{
  "data_type": "INT16"  // For a float value
}
```
**✅ Solution:**
```json
{
  "data_type": "FLOAT32_ABCD"  // Correct for 32-bit float
}
```

**❌ Mistake 2: Polling Too Fast**
```json
{
  "refresh_rate_ms": 100  // 100ms for temperature sensor
}
```
**✅ Solution:**
```json
{
  "refresh_rate_ms": 5000  // 5s appropriate for temperature
}
```

**❌ Mistake 3: No Network Redundancy**
```json
{
  "wifi": {"enabled": false},
  "ethernet": {"enabled": true}
}
```
**✅ Solution:**
```json
{
  "wifi": {"enabled": true},
  "ethernet": {"enabled": true},
  "communication": {"mode": "auto"}
}
```

### Network Issues

**❌ Weak WiFi Signal**
- Problem: Gateway too far from router
- Solution: Move closer, use external antenna, or use Ethernet

**❌ Network Congestion**
- Problem: Too many devices on 2.4GHz
- Solution: Use less crowded channel or switch to Ethernet

**❌ No Failover**
- Problem: Single point of failure
- Solution: Enable dual network with auto failover

### Modbus Issues

**❌ Wrong Slave ID**
- Symptom: "Modbus timeout" errors
- Check: Verify slave_id matches device DIP switches/config

**❌ Wrong Baud Rate**
- Symptom: Garbled data or timeouts
- Check: Match baud_rate to device setting

**❌ Wrong Register Address**
- Symptom: Wrong values displayed
- Check: Verify address from device manual (watch for 0-based vs 1-based)

### MQTT Issues

**❌ QoS 0 for Critical Data**
- Problem: Data loss possible
- Solution: Use QoS 1 for important data

**❌ Very Short Intervals**
- Problem: Bandwidth waste, broker overload
- Solution: Use appropriate intervals (5s+ for most sensors)

**❌ No TLS in Production**
- Problem: Security vulnerability
- Solution: Always use TLS/SSL (port 8883)

---

## Related Documentation

- [Quick Start Guide](QUICKSTART.md) - Get started in 5 minutes
- [Network Configuration](Technical_Guides/NETWORK_CONFIGURATION.md) - Detailed network setup
- [MQTT Publish Modes](Technical_Guides/MQTT_PUBLISH_MODES_DOCUMENTATION.md) - MQTT configuration guide
- [Modbus Data Types](Technical_Guides/MODBUS_DATATYPES.md) - Complete data type reference
- [Troubleshooting Guide](Technical_Guides/TROUBLESHOOTING.md) - Problem solving
- [API Reference](API_Reference/API.md) - Complete API documentation

---

**Document Version:** 1.0
**Last Updated:** November 22, 2025
**Firmware Version:** 2.3.3
**Maintainer:** Kemal

[← Back to Documentation Index](README.md) | [↑ Top](#best-practices-guide)
