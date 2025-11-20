# Hardware and Firmware Documentation for ESP32-S3 Dev Module

## 1. Development Environment

### Arduino IDE
- **Version**: 2.3.6
- **Documentation**: [https://docs.arduino.cc/software/ide/](https://docs.arduino.cc/software/ide/)
- **Platform**: Arduino IDE for ESP32-S3 firmware development

### Board Manager Configuration
- **ESP32 Board Manager URL**: `https://dl.espressif.com/dl/package_esp32_index.json`
- **Installation**: 
  1. Open Arduino IDE
  2. Go to File → Preferences
  3. Add the URL above to "Additional Board Manager URLs"
  4. Go to Tools → Board → Board Manager
  5. Search for "esp32" and install "esp32 by Espressif Systems"

---

## 2. Installed Libraries

### 2.1 BLE Communication (Bluetooth Low Energy)

#### BLE (ESP32 Built-in)
- **Version**: Built-in ESP32
- **Components**: BLEDevice, BLEServer, BLEUtils, BLE2902
- **Description**: ESP32 built-in BLE library for Bluetooth Low Energy communication
- **Function**: BLE server, characteristics, advertising, notifications

### 2.2 Web and Network Communication Protocols

#### ArduinoHttpClient
- **Version**: 0.6.1
- **Developer**: Arduino
- **Status**: [EXPERIMENTAL]
- **Description**: Library for interacting with web servers using HTTP and WebSockets
- **Function**: HTTP client (POST, PUT, PATCH), WebSocket communication
- **Usage**: HttpManager for sending data to HTTP endpoints

### 2.3 MQTT Protocol

#### TBPubSubClient (PubSubClient)
- **Version**: 2.12.1
- **Developer**: ThingsBoard
- **Description**: Client library for MQTT messaging
- **Function**: MQTT messaging protocol, ideal for IoT devices with low bandwidth connections
- **Usage**: MqttManager for publishing data to MQTT broker

### 2.4 Networking

#### Ethernet
- **Version**: 2.0.2
- **Developer**: Various (see AUTHORS file)
- **Description**: Enables network connection using Ethernet Shield (W5500)
- **Function**: Ethernet connectivity for ESP32
- **Hardware**: W5500 Ethernet module via SPI
- **Usage**: EthernetManager, ModbusTcpService

#### WiFi (ESP32 Built-in)
- **Version**: Built-in ESP32
- **Description**: ESP32 built-in WiFi library
- **Function**: WiFi connectivity (STA and AP mode)
- **Usage**: WiFiManager, NetworkManager

### 2.5 Serial Communication Protocol - Modbus

#### ModbusMaster
- **Version**: 2.0.1
- **Developer**: Doc Walker
- **Description**: Enables ESP32 to become Modbus master
- **Function**: Communication with Modbus slave devices via RTU (serial)
- **Usage**: ModbusRtuService for polling Modbus RTU devices

### 2.6 Time Synchronization

#### NTPClient
- **Version**: 3.2.1
- **Developer**: Fabrice Weinberg
- **Description**: NTPClient for connecting to time server
- **Function**: Gets time from NTP server and maintains synchronization
- **Usage**: RTCManager for time synchronization via NTP

### 2.7 Real-Time Clock (RTC)

#### RTClib
- **Version**: 2.1.4
- **Developer**: Adafruit
- **Description**: Fork of JeeLab RTC library
- **Compatibility**: DS3231, DS1307, PCF8523, PCF8563
- **Hardware**: RTC DS3231 via I2C (SDA=5, SCL=6)
- **Function**: RTC hardware interface, temperature compensation
- **Usage**: RTCManager for storing and reading real-time

### 2.8 File System

#### LittleFS (ESP32 Built-in)
- **Version**: Built-in ESP32
- **Description**: Lightweight filesystem for embedded devices
- **Function**: File storage for configurations (JSON files)
- **Usage**: ConfigManager, ServerConfig, LoggingConfig

### 2.9 Data Processing

#### ArduinoJson
- **Version**: 7.4.2
- **Developer**: Benoît Blanchon
- **Popularity**: ⭐ 6953 stars on GitHub
- **Description**: Simple and efficient JSON library for embedded C++
- **Features**: Supports serialization, deserialization, zero-copy, streaming
- **Usage**: Used in all components for parsing and serializing JSON

### 2.10 User Input Handling

#### OneButton
- **Version**: Latest (check library manager)
- **Developer**: Matthias Hertel
- **Repository**: [https://github.com/mathertel/OneButton](https://github.com/mathertel/OneButton)
- **Description**: Library for handling button inputs with debouncing and event detection
- **Features**:
  - Single click detection
  - Double click detection
  - Long press detection
  - Multi-click support
  - Debouncing built-in
- **Installation**: Available via Arduino Library Manager (search "OneButton")
- **Usage**: Button event handling for user interface controls

### 2.11 Hardware Interface

#### Wire (ESP32 Built-in)
- **Version**: Built-in ESP32
- **Description**: ESP32 built-in I2C library
- **Function**: I2C communication protocol
- **Usage**: RTCManager for communication with RTC DS3231

#### SPI (ESP32 Built-in)
- **Version**: Built-in ESP32
- **Description**: ESP32 built-in SPI library
- **Function**: SPI communication protocol
- **Usage**: EthernetManager for communication with W5500

---

## 3. ESP32-S3 Dev Module Board Configuration

### 3.1 Board Selection
- **Board**: ESP32-S3 Dev Module
- **Platform**: ESP32-S3 (Espressif)
- **Board Manager**: esp32 by Espressif Systems

### 3.2 USB Configuration

#### USB CDC On Boot
- **Setting**: Disabled
- **Function**: USB Communication Device Class on boot

#### USB DFU On Boot
- **Setting**: Disabled
- **Function**: USB Device Firmware Update on boot

#### USB Firmware MSC On Boot
- **Setting**: Disabled
- **Function**: USB Mass Storage Class for firmware on boot

#### USB Mode
- **Setting**: Hardware CDC and JTAG
- **Function**: USB mode for communication and debugging

### 3.3 CPU and Core Configuration

#### CPU Frequency
- **Setting**: 240MHz (WiFi)
- **Description**: CPU clock frequency with WiFi support

#### Core Debug Level
- **Setting**: None
- **Description**: Debug level for ESP32 core

#### Events Run On
- **Setting**: Core 1
- **Description**: Core that runs event loop

#### Arduino Runs On
- **Setting**: Core 1
- **Description**: Core that runs Arduino code

### 3.4 Flash Memory Configuration

#### Flash Mode
- **Setting**: QIO 80MHz
- **Description**: Quad I/O mode with 80MHz clock for flash memory

#### Flash Size
- **Setting**: 16MB (128Mb)
- **Description**: Available flash memory size

#### Erase All Flash Before Sketch Upload
- **Setting**: Disabled
- **Description**: Option to erase all flash before upload

#### Partition Scheme
- **Setting**: 8M with spiffs (3MB APP/1.5MB SPIFFS)
- **Description**: Memory partition scheme
  - 3MB for application (APP)
  - 1.5MB for SPIFFS (file system)

### 3.5 PSRAM Configuration

#### PSRAM
- **Setting**: OPI PSRAM
- **Description**: Octal-SPI Pseudo Static RAM for extended memory
- **Usage**: Dynamic allocation for JsonDocument, BLEManager, CRUDHandler, ConfigManager

### 3.6 Upload Configuration

#### Upload Mode
- **Setting**: UART0 / Hardware CDC
- **Description**: Firmware upload mode via UART0 or Hardware CDC

#### Upload Speed
- **Setting**: 921600
- **Description**: Baud rate for firmware upload

### 3.7 Debugging and Interface Configuration

#### JTAG Adapter
- **Setting**: Disabled
- **Description**: Joint Test Action Group interface for debugging

#### Zigbee Mode
- **Setting**: Disabled
- **Description**: Zigbee communication mode

---

## 4. Hardware Specifications

### ESP32-S3 Dev Module
- **Microcontroller**: ESP32-S3 (Xtensa dual-core 32-bit LX7)
- **Flash Memory**: 16MB
- **PSRAM**: OPI PSRAM (Octal-SPI)
- **CPU Frequency**: 240MHz (with WiFi active)
- **Connectivity**:
  - WiFi 802.11 b/g/n (Built-in)
  - Bluetooth 5.0 (LE) (Built-in)
- **USB**: Native USB with CDC and JTAG support
- **Core**: Dual-core (FreeRTOS)

### Peripheral Hardware

#### W5500 Ethernet Module
- **Interface**: SPI
- **Pin Configuration**:
  - CS (Chip Select): GPIO 48
  - INT (Interrupt): GPIO 9
  - MOSI: GPIO 14
  - MISO: GPIO 21
  - SCK: GPIO 47
- **Function**: Ethernet connectivity for Modbus TCP and network communication

#### RTC DS3231
- **Interface**: I2C
- **Pin Configuration**:
  - SDA: GPIO 5
  - SCL: GPIO 6
- **Function**: Real-time clock with temperature compensation
- **Features**: Battery backup, high accuracy (±2ppm)

#### Modbus RTU Serial Ports
- **Serial1 (HardwareSerial 1)**:
  - RX: GPIO 15
  - TX: GPIO 16
  - Baud Rate: 9600 (configurable)
  - Format: 8N1
- **Serial2 (HardwareSerial 2)**:
  - RX: GPIO 17
  - TX: GPIO 18
  - Baud Rate: 9600 (configurable)
  - Format: 8N1

#### LED Indicator
- **Pin**: GPIO 7
- **Function**: Network activity indicator (LED_NET)

#### User Input Buttons (if applicable)
- **Library**: OneButton
- **Debounce Time**: Configurable (default ~50ms)
- **Supported Events**: Click, Double-click, Long-press, Multi-click

---

## 5. Modbus Data Types Implementation

### 5.1 Fundamental Concepts

#### Basic Units
- **16-bit Register**: Basic unit for Holding/Input Registers
- **1-bit Coil**: Basic unit for Coils/Discrete Inputs
- **Multi-Register Data**:
  - 32-bit types require 2 consecutive registers
  - 64-bit types require 4 consecutive registers

### 5.2 Supported Data Types

#### Single Register Types (16-bit)
- **INT16**: Signed 16-bit integer (-32,768 to 32,767)
- **UINT16**: Unsigned 16-bit integer (0 to 65,535)
- **BOOL**: Boolean value (0 or 1)
- **BINARY**: Raw binary representation

#### Multi-Register Types (32-bit)
- **INT32**: Signed 32-bit integer
- **UINT32**: Unsigned 32-bit integer
- **FLOAT32**: IEEE 754 single-precision float

#### Multi-Register Types (64-bit)
- **INT64**: Signed 64-bit integer
- **UINT64**: Unsigned 64-bit integer
- **DOUBLE64**: IEEE 754 double-precision float

### 5.3 Byte Order (Endianness) Variants

All multi-register data types support 4 endianness variants:
- **BE** (Big Endian): Standard format - ABCD
- **LE** (Little Endian): Word swap - CDAB
- **BE_BS** (Big Endian Byte Swap): Byte swap - BADC
- **LE_BS** (Little Endian Byte Swap): Full reversal - DCBA

**Example naming convention**: `FLOAT32_BE`, `INT32_LE`, `DOUBLE64_BE_BS`

### 5.4 Supported Function Codes

| Function Code | Type                   | Description                                |
| ------------- | ---------------------- | ------------------------------------------ |
| **1**         | Read Coils             | Read 1-bit coils (read-write)              |
| **2**         | Read Discrete Inputs   | Read 1-bit discrete inputs (read-only)     |
| **3**         | Read Holding Registers | Read 16-bit holding registers (read-write) |
| **4**         | Read Input Registers   | Read 16-bit input registers (read-only)    |

### 5.5 Configuration Example

```json
{
  "device_id": "D1a2b3c",
  "device_name": "Temperature Sensor",
  "protocol": "RTU",
  "slave_id": 1,
  "registers": [
    {
      "register_id": "R4d5e6f",
      "register_name": "Temperature",
      "address": 100,
      "function_code": 3,
      "data_type": "FLOAT32_BE",
      "refresh_rate_ms": 5000
    },
    {
      "register_id": "R7g8h9i",
      "register_name": "Pressure",
      "address": 200,
      "function_code": 4,
      "data_type": "UINT32_LE",
      "refresh_rate_ms": 10000
    }
  ]
}
```

---

## 6. System Architecture

### 6.1 Core Services

#### BLEManager
- **Function**: BLE server for device configuration via smartphone
- **Features**: 
  - Command/response with fragmentation
  - MTU metrics monitoring
  - Queue depth tracking
  - Connection metrics
- **Service UUID**: `00001830-0000-1000-8000-00805f9b34fb`

#### CRUDHandler
- **Function**: Handler for CRUD operations (Create, Read, Update, Delete)
- **Features**:
  - Batch operations (sequential, atomic, parallel)
  - Priority queue (high, normal, low)
  - Command processing task

#### ConfigManager
- **Function**: Device and register configuration management
- **Storage**: LittleFS (devices.json, registers.json)
- **Features**:
  - PSRAM cache for performance
  - Thread-safe with mutex
  - Cache TTL management

#### NetworkManager
- **Function**: Network failover management (WiFi ↔ Ethernet)
- **Features**:
  - Automatic failover
  - Connection pooling
  - WiFi signal strength monitoring
  - Configurable failover timeouts

### 6.2 Communication Services

#### MqttManager
- **Function**: MQTT client for publishing data
- **Features**:
  - Auto-reconnect
  - Configurable data transmission interval
  - Queue-based data buffering

#### HttpManager
- **Function**: HTTP client for publishing data
- **Features**:
  - Multiple HTTP methods (POST, PUT, PATCH)
  - Retry mechanism
  - Configurable timeout

### 6.3 Modbus Services

#### ModbusRtuService
- **Function**: Modbus RTU master (serial communication)
- **Features**:
  - Dual serial port support
  - 3-level polling hierarchy (register, device, server)
  - Exponential backoff retry
  - Multi-register data types support
  - All endianness variants support

#### ModbusTcpService
- **Function**: Modbus TCP master (Ethernet/WiFi communication)
- **Features**:
  - Connection pooling
  - Atomic transaction counter
  - Dynamic buffer sizing
  - Multi-register data types support
  - All endianness variants support

### 6.4 Utility Services

#### QueueManager
- **Function**: Data queue management
- **Features**:
  - Main queue (100 items)
  - Stream queue (50 items)
  - PSRAM allocation
  - Thread-safe operations

#### RTCManager
- **Function**: Real-time clock management
- **Features**:
  - NTP synchronization (WIB/GMT+7)
  - Automatic time updates
  - Temperature monitoring

#### LEDManager
- **Function**: LED indicator management
- **Features**:
  - Success notification blink
  - Non-blocking FreeRTOS task

---

## 7. Library Installation Guide

### 7.1 ESP32 Board Manager Setup

1. Open Arduino IDE
2. Navigate to **File → Preferences**
3. In "Additional Board Manager URLs" field, add:
   ```
   https://dl.espressif.com/dl/package_esp32_index.json
   ```
4. Click **OK**
5. Go to **Tools → Board → Board Manager**
6. Search for "**esp32**"
7. Install "**esp32 by Espressif Systems**"
8. Select **Tools → Board → esp32 → ESP32S3 Dev Module**

### 7.2 Required Libraries Installation

Install the following libraries via **Sketch → Include Library → Manage Libraries**:

| Library Name | Search Term  | Developer        |
| ------------ | ------------ | ---------------- |
| ArduinoJson  | ArduinoJson  | Benoît Blanchon  |
| RTClib       | RTClib       | Adafruit         |
| NTPClient    | NTPClient    | Fabrice Weinberg |
| ModbusMaster | ModbusMaster | Doc Walker       |
| PubSubClient | PubSubClient | Nick O'Leary     |
| Ethernet     | Ethernet     | Various          |
| OneButton    | OneButton    | Matthias Hertel  |

### 7.3 Built-in ESP32 Libraries (No Installation Required)

These libraries are included with ESP32 board package:
- **BLE** (BLEDevice, BLEServer, BLEUtils, BLE2902)
- **WiFi**
- **Wire** (I2C)
- **SPI**
- **LittleFS**

---

## 8. References

### Official Documentation
- Arduino IDE Documentation: [https://docs.arduino.cc/software/ide/](https://docs.arduino.cc/software/ide/)
- ESP32-S3 Technical Reference: [Espressif Documentation](https://www.espressif.com/en/products/socs/esp32-s3)
- ESP32 Arduino Core: [https://github.com/espressif/arduino-esp32](https://github.com/espressif/arduino-esp32)

### Library Documentation
- OneButton Library: [https://github.com/mathertel/OneButton](https://github.com/mathertel/OneButton)
- ArduinoJson: [https://arduinojson.org/](https://arduinojson.org/)
- ModbusMaster: [https://github.com/4-20ma/ModbusMaster](https://github.com/4-20ma/ModbusMaster)
- RTClib: [https://github.com/adafruit/RTClib](https://github.com/adafruit/RTClib)

### Modbus Protocol References
- **Modbus Poll Display Formats**: [https://www.modbustools.com/poll_display_formats.html](https://www.modbustools.com/poll_display_formats.html)
- **MinimalModbus Documentation**: [https://minimalmodbus.readthedocs.io/en/stable/modbusdetails.html](https://minimalmodbus.readthedocs.io/en/stable/modbusdetails.html)
- **Modbus Organization**: [https://modbus.org/](https://modbus.org/)

---

**This document is based on actual configuration from Arduino IDE version 2.3.6 and source code analysis**

*Last updated: November 2025*