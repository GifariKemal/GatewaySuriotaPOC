# Server Configuration Reference

## Overview

This document explains the server configuration structure used in
`update_server_config.py`.

## Configuration Structure (API v2.3.0)

```json
{
  "op": "update",
  "type": "server_config",
  "config": {
    "communication": { ... },
    "wifi": { ... },
    "ethernet": { ... },
    "protocol": "mqtt" | "http" | "both",
    "mqtt_config": { ... },
    "http_config": { ... }
  }
}
```

---

## 1. Communication Mode

Determines which network interface is primary.

```json
"communication": {
  "mode": "ETH"  // or "WIFI"
}
```

**Options:**

- `"ETH"` - Ethernet (wired) is primary
- `"WIFI"` - WiFi (wireless) is primary

**Current Setting:** `"ETH"`

---

## 2. WiFi Configuration

WiFi settings (can be enabled even if Ethernet is primary).

```json
"wifi": {
  "enabled": true,
  "ssid": "KOWI",
  "password": "@Kremi201"
}
```

**Fields:**

- `enabled` (boolean) - Enable/disable WiFi
- `ssid` (string) - WiFi network name
- `password` (string) - WiFi password

**Current Settings:**

- Enabled: ✅ Yes
- SSID: `KOWI`
- Password: `@Kremi201`

**Notes:**

- ESP32-S3 only supports 2.4GHz WiFi
- SSID and password are case-sensitive
- Max SSID length: 32 characters
- Max password length: 63 characters

---

## 3. Ethernet Configuration

Ethernet settings with DHCP or static IP.

```json
"ethernet": {
  "enabled": true,
  "use_dhcp": true,
  "static_ip": "",
  "gateway": "",
  "subnet": ""
}
```

**Fields:**

- `enabled` (boolean) - Enable/disable Ethernet
- `use_dhcp` (boolean) - Use DHCP (automatic IP) or static IP
- `static_ip` (string) - Static IP address (only if `use_dhcp` is `false`)
- `gateway` (string) - Gateway IP (only if `use_dhcp` is `false`)
- `subnet` (string) - Subnet mask (only if `use_dhcp` is `false`)

**Current Settings:**

- Enabled: ✅ Yes
- DHCP: ✅ Yes (Automatic IP)
- Static IP: Not used (DHCP enabled)

**Example Static IP Configuration:**

```json
"ethernet": {
  "enabled": true,
  "use_dhcp": false,
  "static_ip": "192.168.1.100",
  "gateway": "192.168.1.1",
  "subnet": "255.255.255.0"
}
```

---

## 4. Protocol Selection

Determines which telemetry protocol(s) to use.

```json
"protocol": "mqtt"
```

**Options:**

- `"mqtt"` - Only MQTT enabled
- `"http"` - Only HTTP enabled
- `"both"` - Both MQTT and HTTP enabled

**Current Setting:** `"mqtt"`

---

## 5. MQTT Configuration

MQTT broker and publish mode settings.

```json
"mqtt_config": {
  "enabled": true,
  "broker_address": "broker.hivemq.com",
  "broker_port": 1883,
  "client_id": "SuriotaGateway-001",
  "username": "mqtt_user",
  "password": "mqtt_password",
  "keep_alive": 60,
  "clean_session": true,
  "use_tls": false,
  "publish_mode": "default",
  "default_mode": { ... },
  "customize_mode": { ... }
}
```

### 5.1 MQTT Broker Settings

**Fields:**

- `enabled` (boolean) - Enable/disable MQTT
- `broker_address` (string) - MQTT broker hostname/IP
- `broker_port` (integer) - MQTT broker port (default: 1883, TLS: 8883)
- `client_id` (string) - Unique client identifier
- `username` (string) - MQTT username (optional)
- `password` (string) - MQTT password (optional)
- `keep_alive` (integer) - Keep-alive interval in seconds
- `clean_session` (boolean) - Clean session flag
- `use_tls` (boolean) - Enable TLS/SSL encryption

**Current Settings:**

- Broker: `broker.hivemq.com:1883`
- Client ID: `SuriotaGateway-001`
- Keep Alive: 60 seconds
- Clean Session: ✅ Yes
- TLS: ❌ No

### 5.2 MQTT Publish Modes

MQTT supports two publish modes:

#### A. Default Mode

All registers published to a single topic in one JSON payload.

```json
"publish_mode": "default",
"default_mode": {
  "enabled": true,
  "topic_publish": "v1/devices/me/telemetry",
  "topic_subscribe": "v1/devices/me/rpc",
  "interval": 20,
  "interval_unit": "s"
}
```

**Fields:**

- `enabled` (boolean) - Enable default mode
- `topic_publish` (string) - Topic for publishing telemetry data
- `topic_subscribe` (string) - Topic for receiving commands
- `interval` (integer) - Publish interval value
- `interval_unit` (string) - `"ms"` (milliseconds), `"s"` (seconds), or `"m"`
  (minutes)

**Current Settings:**

- Mode: ✅ Default Mode
- Publish Topic: `v1/devices/me/telemetry`
- Subscribe Topic: `v1/devices/me/rpc`
- Interval: 20 seconds

**Payload Example:**

```json
{
  "timestamp": 1700000000,
  "temperature": 25.5,
  "humidity": 60.2,
  "pressure": 1013.25,
  "voltage": 230,
  "current": 5.2
}
```

#### B. Customize Mode

Registers organized into multiple topics with different intervals.

```json
"publish_mode": "customize",
"customize_mode": {
  "enabled": true,
  "topic_subscribe": "v1/devices/me/rpc",
  "custom_topics": [
    {
      "topic": "sensor/temperature",
      "registers": ["temp_room_1", "humidity_room_1"],
      "interval": 5,
      "interval_unit": "s"
    },
    {
      "topic": "power/meter",
      "registers": ["voltage_l1", "current_l1", "power_total"],
      "interval": 10,
      "interval_unit": "s"
    }
  ]
}
```

**Fields:**

- `enabled` (boolean) - Enable customize mode
- `topic_subscribe` (string) - Topic for receiving commands
- `custom_topics` (array) - Array of custom topic configurations
  - `topic` (string) - MQTT topic name
  - `registers` (array of strings) - Register IDs to publish
  - `interval` (integer) - Publish interval for this topic
  - `interval_unit` (string) - `"ms"`, `"s"`, or `"m"`

**Current Settings:**

- Mode: ❌ Disabled (using Default Mode instead)

---

## 6. HTTP Configuration

HTTP REST API telemetry settings.

```json
"http_config": {
  "enabled": false,
  "endpoint_url": "",
  "method": "POST",
  "body_format": "json",
  "timeout": 5000,
  "retry": 3,
  "interval": 5,
  "interval_unit": "s",
  "headers": {}
}
```

**Fields:**

- `enabled` (boolean) - Enable/disable HTTP
- `endpoint_url` (string) - Full URL of REST API endpoint
- `method` (string) - HTTP method: `"POST"`, `"PUT"`, or `"PATCH"`
- `body_format` (string) - Request body format: `"json"` or `"form"`
- `timeout` (integer) - Request timeout in milliseconds
- `retry` (integer) - Max retry attempts on failure
- `interval` (integer) - Transmission interval value (v2.3.0+)
- `interval_unit` (string) - `"ms"`, `"s"`, or `"m"` (v2.3.0+)
- `headers` (object) - Custom HTTP headers

**Current Settings:**

- Enabled: ❌ No

**Example Enabled HTTP Config:**

```json
"http_config": {
  "enabled": true,
  "endpoint_url": "https://api.example.com/iot/data",
  "method": "POST",
  "body_format": "json",
  "timeout": 5000,
  "retry": 3,
  "interval": 10,
  "interval_unit": "s",
  "headers": {
    "Authorization": "Bearer YOUR_TOKEN_HERE",
    "Content-Type": "application/json"
  }
}
```

---

## Modifying Configuration

### Change WiFi Credentials

```python
"wifi": {
  "enabled": True,
  "ssid": "YourNetworkName",      # Change this
  "password": "YourWiFiPassword"   # Change this
}
```

### Change MQTT Broker

```python
"mqtt_config": {
  "enabled": True,
  "broker_address": "your.broker.com",  # Change this
  "broker_port": 1883,                  # Change if needed
  "client_id": "YourClientID",          # Change this
  # ... other settings
}
```

### Change MQTT Interval

```python
"default_mode": {
  "enabled": True,
  "topic_publish": "v1/devices/me/telemetry",
  "topic_subscribe": "v1/devices/me/rpc",
  "interval": 30,        # Change from 20 to 30 seconds
  "interval_unit": "s"
}
```

### Switch to Static IP

```python
"ethernet": {
  "enabled": True,
  "use_dhcp": False,                 # Change to False
  "static_ip": "192.168.1.100",      # Set your IP
  "gateway": "192.168.1.1",          # Set gateway
  "subnet": "255.255.255.0"          # Set subnet
}
```

### Enable HTTP Instead of MQTT

```python
"protocol": "http",  # Change from "mqtt"

"mqtt_config": {
  "enabled": False,  # Disable MQTT
  # ...
},

"http_config": {
  "enabled": True,   # Enable HTTP
  "endpoint_url": "https://api.example.com/data",
  "method": "POST",
  "interval": 10,
  "interval_unit": "s",
  # ...
}
```

### Enable Both MQTT and HTTP

```python
"protocol": "both",  # Use both protocols

"mqtt_config": {
  "enabled": True,
  # ... MQTT settings
},

"http_config": {
  "enabled": True,
  # ... HTTP settings
}
```

---

## Important Notes

### Device Restart Behavior

⚠️ **The device ALWAYS restarts 5 seconds after receiving server_config update**

This is firmware behavior and cannot be disabled. Make sure:

- Network cable is connected (if using Ethernet)
- WiFi credentials are correct (if using WiFi)
- Configuration is correct before sending

### Interval Units

Three units are supported:

- `"ms"` - Milliseconds (1000ms = 1 second)
- `"s"` - Seconds
- `"m"` - Minutes (1m = 60 seconds)

**Examples:**

- `"interval": 500, "interval_unit": "ms"` → Every 500 milliseconds
- `"interval": 30, "interval_unit": "s"` → Every 30 seconds
- `"interval": 2, "interval_unit": "m"` → Every 2 minutes

### API Version Changes

**v2.3.0 Breaking Changes:**

- ❌ `data_interval` removed from root config
- ✅ `http_config.interval` and `http_config.interval_unit` added
- ✅ MQTT intervals moved to mode-specific configs

**Old (v2.1.1 - No longer supported):**

```json
"config": {
  "data_interval": {"value": 10, "unit": "s"},  // ❌ Removed
  "http_config": {"enabled": true}
}
```

**New (v2.3.0+):**

```json
"config": {
  "http_config": {
    "enabled": true,
    "interval": 10,        // ✅ Moved here
    "interval_unit": "s"   // ✅ Moved here
  }
}
```

---

## Complete Example Configurations

### Example 1: MQTT Only with Ethernet DHCP

```json
{
  "op": "update",
  "type": "server_config",
  "config": {
    "communication": { "mode": "ETH" },
    "wifi": { "enabled": false, "ssid": "", "password": "" },
    "ethernet": {
      "enabled": true,
      "use_dhcp": true,
      "static_ip": "",
      "gateway": "",
      "subnet": ""
    },
    "protocol": "mqtt",
    "mqtt_config": {
      "enabled": true,
      "broker_address": "mqtt.example.com",
      "broker_port": 1883,
      "client_id": "Gateway001",
      "username": "user",
      "password": "pass",
      "keep_alive": 60,
      "clean_session": true,
      "use_tls": false,
      "publish_mode": "default",
      "default_mode": {
        "enabled": true,
        "topic_publish": "sensor/data",
        "topic_subscribe": "sensor/control",
        "interval": 10,
        "interval_unit": "s"
      },
      "customize_mode": { "enabled": false, "custom_topics": [] }
    },
    "http_config": { "enabled": false }
  }
}
```

### Example 2: HTTP Only with Static IP

```json
{
  "op": "update",
  "type": "server_config",
  "config": {
    "communication": { "mode": "ETH" },
    "wifi": { "enabled": false, "ssid": "", "password": "" },
    "ethernet": {
      "enabled": true,
      "use_dhcp": false,
      "static_ip": "192.168.1.100",
      "gateway": "192.168.1.1",
      "subnet": "255.255.255.0"
    },
    "protocol": "http",
    "mqtt_config": { "enabled": false },
    "http_config": {
      "enabled": true,
      "endpoint_url": "https://api.company.com/sensors",
      "method": "POST",
      "body_format": "json",
      "timeout": 5000,
      "retry": 3,
      "interval": 30,
      "interval_unit": "s",
      "headers": {
        "Authorization": "Bearer abc123",
        "Content-Type": "application/json"
      }
    }
  }
}
```

### Example 3: MQTT Customize Mode with WiFi

```json
{
  "op": "update",
  "type": "server_config",
  "config": {
    "communication": { "mode": "WIFI" },
    "wifi": {
      "enabled": true,
      "ssid": "FactoryWiFi",
      "password": "SecurePass123"
    },
    "ethernet": {
      "enabled": false,
      "use_dhcp": true,
      "static_ip": "",
      "gateway": "",
      "subnet": ""
    },
    "protocol": "mqtt",
    "mqtt_config": {
      "enabled": true,
      "broker_address": "mqtt.factory.com",
      "broker_port": 8883,
      "client_id": "Sensor_Gateway_A1",
      "username": "gateway",
      "password": "secure_pass",
      "keep_alive": 120,
      "clean_session": true,
      "use_tls": true,
      "publish_mode": "customize",
      "default_mode": { "enabled": false },
      "customize_mode": {
        "enabled": true,
        "topic_subscribe": "command/gateway/a1",
        "custom_topics": [
          {
            "topic": "factory/temperature",
            "registers": ["temp_zone_1", "temp_zone_2", "temp_zone_3"],
            "interval": 5,
            "interval_unit": "s"
          },
          {
            "topic": "factory/power",
            "registers": ["voltage_l1", "current_l1", "power_total"],
            "interval": 1,
            "interval_unit": "s"
          },
          {
            "topic": "factory/environment",
            "registers": ["humidity", "pressure"],
            "interval": 1,
            "interval_unit": "m"
          }
        ]
      }
    },
    "http_config": { "enabled": false }
  }
}
```

---

## References

- Full API Documentation: `../../Docs/API.md`
- MQTT Publish Modes: `../../Docs/MQTT_PUBLISH_MODES_DOCUMENTATION.md`
- Network Configuration Guide: `../../Docs/NETWORK_CONFIGURATION.md`

---

**Version:** 2.3.0 **Last Updated:** 2025-11-15 **Author:** Kemal - SURIOTA R&D
Team
