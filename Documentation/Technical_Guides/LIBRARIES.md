# 📚 Third-Party Libraries Reference

**SRT-MGATE-1210 Modbus IIoT Gateway**

Complete reference for all third-party libraries used in the firmware, including
installation, configuration, and version requirements.

---

## 📋 Table of Contents

- [Overview](#overview)
- [Core Libraries](#core-libraries)
- [Communication Libraries](#communication-libraries)
- [Hardware Interface Libraries](#hardware-interface-libraries)
- [Data Processing Libraries](#data-processing-libraries)
- [System Libraries](#system-libraries)
- [Installation Guide](#installation-guide)
- [Version Compatibility](#version-compatibility)
- [License Information](#license-information)

---

## Overview

The SRT-MGATE-1210 firmware is built on a robust foundation of well-tested,
industry-standard libraries. This document provides comprehensive information
about each library used in the project.

### Library Categories

- **Core Libraries**: ESP32 framework and FreeRTOS
- **Communication**: BLE, WiFi, Ethernet, MQTT, HTTP
- **Hardware Interface**: I2C, SPI, UART/RS485
- **Data Processing**: JSON parsing, data structures
- **System**: Memory management, file system, watchdog

---

## Core Libraries

### 1. Arduino Core for ESP32

**Purpose**: Base framework for ESP32-S3 development

**Provider**: Espressif Systems **Repository**:
https://github.com/espressif/arduino-esp32 **Version Required**: 2.0.0 or higher
**License**: LGPL 2.1

**Installation (Arduino IDE)**:

```
File → Preferences → Additional Boards Manager URLs:
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

Tools → Board → Boards Manager → Search "esp32" → Install "esp32 by Espressif Systems"
```

**Key Components**:

- ESP32-S3 hardware abstraction
- FreeRTOS kernel
- WiFi and Bluetooth stacks
- Peripheral drivers (SPI, I2C, UART)

**Configuration**:

```cpp
// Board: ESP32-S3 Dev Module
// Flash Size: 16MB (128Mb)
// PSRAM: OPI PSRAM
// Partition Scheme: Default 4MB with spiffs
```

---

### 2. FreeRTOS

**Purpose**: Real-time operating system for multitasking

**Provider**: Espressif (integrated in ESP32 Arduino Core) **Version**: 10.4.3
(ESP-IDF v4.4) **License**: MIT

**Headers Used**:

```cpp
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
```

**Key Features**:

- **Task Management**: 15+ concurrent tasks
- **Queue Communication**: Inter-task messaging
- **Semaphore/Mutex**: Resource locking
- **Memory Management**: Heap allocation with PSRAM support

**Task Examples**:

```cpp
// BLE Task (Core 0, Priority 5)
xTaskCreatePinnedToCore(
    bleTask,
    "BLE_Task",
    8192,
    nullptr,
    5,
    &bleTaskHandle,
    0
);

// Modbus RTU Task (Core 1, Priority 4)
xTaskCreatePinnedToCore(
    modbusRtuTask,
    "ModbusRTU_Task",
    8192,
    nullptr,
    4,
    &modbusRtuTaskHandle,
    1
);
```

---

## Communication Libraries

### 3. ESP32 BLE Arduino

**Purpose**: Bluetooth Low Energy server and GATT services

**Provider**: Espressif (integrated in Arduino Core) **Version**: Built-in
(ESP32 Arduino Core 2.0+) **License**: Apache 2.0

**Headers Used**:

```cpp
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
```

**Key Features**:

- **GATT Server**: Custom service UUID
- **Characteristics**: RX (write), TX (notify), Stream (notify)
- **MTU Negotiation**: 512 bytes maximum
- **Connection Management**: Auto-reconnect, timeouts

**BLE Service Configuration**:

```cpp
// Custom GATT Service
#define SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define RX_CHAR_UUID        "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define TX_CHAR_UUID        "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define STREAM_CHAR_UUID    "6E400004-B5A3-F393-E0A9-E50E24DCCA9E"
```

**Documentation**: [docs/API.md - BLE CRUD Operations](API.md)

---

### 4. WiFi (ESP32 Built-in)

**Purpose**: WiFi connectivity with Station and AP modes

**Provider**: Espressif (integrated) **Version**: Built-in **License**: Apache
2.0

**Headers Used**:

```cpp
#include <WiFi.h>
```

**Key Features**:

- **Station Mode**: Connect to existing WiFi networks
- **Fallback Support**: Automatic Ethernet → WiFi failover
- **Multi-network**: Supports multiple SSID configurations
- **WPA2/WPA3**: Enterprise security support

**Configuration Example**:

```cpp
WiFi.mode(WIFI_STA);
WiFi.begin(ssid, password);
WiFi.setAutoReconnect(true);
```

**Related**: [NetworkManager.cpp](../Main/NetworkManager.cpp)

---

### 5. Ethernet Library (Arduino)

**Purpose**: W5500 Ethernet controller interface

**Provider**: Arduino **Repository**:
https://github.com/arduino-libraries/Ethernet **Version Required**: 2.0.0 or
higher **License**: LGPL 2.1

**Installation (Arduino IDE)**:

```
Tools → Manage Libraries → Search "Ethernet" → Install "Ethernet by Arduino"
```

**Headers Used**:

```cpp
#include <SPI.h>
#include <Ethernet.h>
```

**Hardware Configuration**:

```cpp
// W5500 on SPI3 (FSPI)
#define ETH_CS   48
#define ETH_MOSI 14
#define ETH_MISO 21
#define ETH_SCK  47
#define ETH_RST  3
#define ETH_INT  9

SPIClass SPI3(FSPI);
SPI3.begin(ETH_SCK, ETH_MISO, ETH_MOSI, ETH_CS);
Ethernet.init(ETH_CS);
```

**Related**: [EthernetManager.cpp](../Main/EthernetManager.cpp),
[docs/HARDWARE.md](HARDWARE.md)

---

### 6. TBPubSubClient (MQTT)

**Purpose**: MQTT client for publish/subscribe messaging

**Provider**: ThingsBoard **Repository**:
https://github.com/thingsboard/pubsubclient **Version Required**: 2.12.1 or
higher **License**: MIT

**Installation (Arduino IDE)**:

```
Tools → Manage Libraries → Search "TBPubSubClient" → Install "TBPubSubClient by ThingsBoard"
```

**Note**: This is a fork of PubSubClient optimized for ThingsBoard but works
with any MQTT broker

**Headers Used**:

```cpp
#include <PubSubClient.h>
```

**Key Features**:

- **QoS 0/1**: Quality of Service levels
- **Persistent Queue**: Failed publish retry mechanism
- **Auto-reconnect**: Exponential backoff
- **TLS Support**: Secure MQTT connections

**Configuration**:

```cpp
mqttClient.setServer(broker, port);
mqttClient.setBufferSize(2048);
mqttClient.setKeepAlive(60);
mqttClient.setSocketTimeout(30);
```

**Custom Enhancements**:

- **MQTTPersistentQueue**: Offline message buffering (LittleFS)
- **Exponential Backoff**: Smart reconnection strategy

**Related**: [MqttManager.cpp](../Main/MqttManager.cpp),
[MQTTPersistentQueue.cpp](../Main/MQTTPersistentQueue.cpp)

---

### 7. ArduinoHttpClient

**Purpose**: HTTP/HTTPS client for RESTful API calls and WebSocket support

**Provider**: Arduino **Repository**:
https://github.com/arduino-libraries/ArduinoHttpClient **Version Required**:
0.6.1 or higher **Status**: [EXPERIMENTAL] **License**: Apache 2.0

**Installation (Arduino IDE)**:

```
Tools → Manage Libraries → Search "ArduinoHttpClient" → Install "ArduinoHttpClient by Arduino"
```

**Headers Used**:

```cpp
#include <ArduinoHttpClient.h>
```

**Key Features**:

- **HTTP Methods**: GET, POST, PUT, PATCH, DELETE
- **WebSocket Support**: Bidirectional communication
- **Headers**: Custom headers support
- **TLS/SSL**: Certificate validation
- **Authentication**: Basic Auth, Bearer tokens

**Usage Example**:

```cpp
#include <ArduinoHttpClient.h>

HttpClient http(client, serverAddress, port);
http.beginRequest();
http.post("/api/data");
http.sendHeader("Content-Type", "application/json");
http.sendHeader("Authorization", "Bearer " + token);
http.beginBody();
http.print(jsonPayload);
http.endRequest();
int statusCode = http.responseStatusCode();
```

**Related**: [HttpManager.cpp](../Main/HttpManager.cpp)

---

### 8. ModbusMaster

**Purpose**: Modbus RTU Master implementation

**Provider**: 4-20ma **Repository**: https://github.com/4-20ma/ModbusMaster
**Version Required**: 2.0.1 or higher **License**: Apache 2.0

**Installation (Arduino IDE)**:

```
Tools → Manage Libraries → Search "ModbusMaster" → Install "ModbusMaster by Doc Walker"
```

**Headers Used**:

```cpp
#include <ModbusMaster.h>
```

**Key Features**:

- **Function Codes**: 1, 2, 3, 4, 5, 6, 15, 16
- **Exception Handling**: Timeout, CRC errors
- **Prepost Transmission**: RS485 DE/RE pin control
- **Multi-slave**: Up to 247 slaves per bus

**Configuration**:

```cpp
ModbusMaster node;
node.begin(slaveId, Serial1); // RS485 Port 1
node.preTransmission(preTransmission);
node.postTransmission(postTransmission);
```

**Supported Operations**:

```cpp
// Read Holding Registers (FC 03)
uint8_t result = node.readHoldingRegisters(address, quantity);

// Read Input Registers (FC 04)
uint8_t result = node.readInputRegisters(address, quantity);

// Write Single Register (FC 06)
uint8_t result = node.writeSingleRegister(address, value);
```

**Related**: [ModbusRtuService.cpp](../Main/ModbusRtuService.cpp),
[docs/MODBUS_DATATYPES.md](MODBUS_DATATYPES.md)

---

## Hardware Interface Libraries

### 9. Wire (I2C - ESP32 Built-in)

**Purpose**: I2C communication for RTC DS3231

**Provider**: Espressif (integrated) **Version**: Built-in **License**: LGPL 2.1

**Headers Used**:

```cpp
#include <Wire.h>
```

**Configuration**:

```cpp
#define I2C_SDA 5
#define I2C_SCL 6

Wire.begin(I2C_SDA, I2C_SCL);
Wire.setClock(100000); // 100kHz standard I2C
```

**Connected Devices**:

- DS3231 RTC (0x68)

**Related**: [RTCManager.cpp](../Main/RTCManager.cpp)

---

### 10. NTPClient

**Purpose**: Network Time Protocol client for time synchronization

**Provider**: Fabrice Weinberg **Repository**:
https://github.com/arduino-libraries/NTPClient **Version Required**: 3.2.1 or
higher **License**: MIT

**Installation (Arduino IDE)**:

```
Tools → Manage Libraries → Search "NTPClient" → Install "NTPClient by Fabrice Weinberg"
```

**Headers Used**:

```cpp
#include <NTPClient.h>
#include <WiFiUdp.h>
```

**Key Features**:

- **NTP Synchronization**: Gets time from NTP server
- **Timezone Support**: GMT offset configuration
- **Update Interval**: Configurable sync interval
- **Multiple Servers**: Fallback server support

**Usage Example**:

```cpp
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 25200, 60000); // GMT+7 (WIB)

timeClient.begin();
timeClient.update();
unsigned long epochTime = timeClient.getEpochTime();
```

**Related**: [RTCManager.cpp](../Main/RTCManager.cpp)

---

### 11. RTClib (Adafruit)

**Purpose**: DS3231 RTC hardware interface

**Provider**: Adafruit Industries **Repository**:
https://github.com/adafruit/RTClib **Version Required**: 2.1.4 or higher
**License**: MIT

**Installation (Arduino IDE)**:

```
Tools → Manage Libraries → Search "RTClib" → Install "RTClib by Adafruit"
```

**Headers Used**:

```cpp
#include <RTClib.h>
```

**Key Features**:

- **DS3231 Support**: High-precision RTC (±2ppm)
- **Temperature Compensation**: Built-in
- **Alarms**: Two programmable alarms
- **Battery Backup**: CR2032 coin cell

**Usage Example**:

```cpp
RTC_DS3231 rtc;
rtc.begin();

// Set time from NTP
DateTime now = DateTime(year, month, day, hour, minute, second);
rtc.adjust(now);

// Read time
DateTime currentTime = rtc.now();
```

**Custom Features**:

- **NTP Sync**: Periodic synchronization with NTP servers
- **Timezone Support**: UTC offset configuration
- **Drift Correction**: Automatic time drift detection

**Related**: [RTCManager.cpp](../Main/RTCManager.cpp),
[docs/HARDWARE.md](HARDWARE.md)

---

### 11. SPI (ESP32 Built-in)

**Purpose**: SPI communication for Ethernet and SD Card

**Provider**: Espressif (integrated) **Version**: Built-in **License**: Apache
2.0

**Headers Used**:

```cpp
#include <SPI.h>
```

**SPI Instances**:

**SPI (Default) - MicroSD Card**:

```cpp
#define SD_CS   11
#define SD_MOSI 10
#define SD_SCK  12
#define SD_MISO 13

SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
```

**SPI3 (FSPI) - W5500 Ethernet**:

```cpp
#define ETH_CS   48
#define ETH_MOSI 14
#define ETH_MISO 21
#define ETH_SCK  47

SPIClass SPI3(FSPI);
SPI3.begin(ETH_SCK, ETH_MISO, ETH_MOSI, ETH_CS);
```

**Related**: [EthernetManager.cpp](../Main/EthernetManager.cpp),
[docs/HARDWARE.md](HARDWARE.md)

---

### 12. HardwareSerial (ESP32 Built-in)

**Purpose**: UART/RS485 communication for Modbus RTU

**Provider**: Espressif (integrated) **Version**: Built-in **License**: Apache
2.0

**Headers Used**:

```cpp
#include <HardwareSerial.h>
```

**Configuration**:

**Serial1 (Modbus RTU Port 1)**:

```cpp
#define RXD1_RS485 15
#define TXD1_RS485 16

Serial1.begin(baudrate, SERIAL_8N1, RXD1_RS485, TXD1_RS485);
```

**Serial2 (Modbus RTU Port 2)**:

```cpp
#define RXD2_RS485 17
#define TXD2_RS485 18

Serial2.begin(baudrate, SERIAL_8N1, RXD2_RS485, TXD2_RS485);
```

**Dynamic Baudrate Support**:

- 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200 baud

**Related**: [ModbusRtuService.cpp](../Main/ModbusRtuService.cpp)

---

### 13. OneButton

**Purpose**: Advanced button handling with debouncing

**Provider**: Matthias Hertel **Repository**:
https://github.com/mathertel/OneButton **Version Required**: 2.0.0 or higher
**License**: BSD 3-Clause

**Installation (Arduino IDE)**:

```
Tools → Manage Libraries → Search "OneButton" → Install "OneButton by Matthias Hertel"
```

**Headers Used**:

```cpp
#include <OneButton.h>
```

**Key Features**:

- **Debouncing**: Hardware noise filtering
- **Click Detection**: Single, double, multi-click
- **Long Press**: Configurable duration
- **Events**: Press start, during, end

**Configuration**:

```cpp
#define BUTTON_PIN 0

OneButton button(BUTTON_PIN, true); // Active LOW

button.attachClick(handleClick);
button.attachLongPressStart(handleLongPress);
button.setPressTicks(5000); // 5 seconds for long press
```

**BLE Configuration Mode**:

- **Long Press (5s)**: Enter BLE configuration mode
- **Button GPIO 0**: Boot mode selection

**Related**: [ButtonManager.cpp](../Main/ButtonManager.cpp),
[docs/HARDWARE.md](HARDWARE.md)

---

## Data Processing Libraries

### 14. ArduinoJson

**Purpose**: JSON parsing and serialization

**Provider**: Benoît Blanchon **Repository**:
https://github.com/bblanchon/ArduinoJson **Version Required**: 7.4.2 or higher
(v7.x series) **Popularity**: ⭐ 6,953 stars on GitHub **License**: MIT

**Installation (Arduino IDE)**:

```
Tools → Manage Libraries → Search "ArduinoJson" → Install "ArduinoJson by Benoit Blanchon" (v7.4.2+)
```

**⚠️ Important**: This firmware uses ArduinoJson v7.x (latest version)

**Headers Used**:

```cpp
#include <ArduinoJson.h>
```

**Key Features**:

- **JSON Parsing**: Fast, memory-efficient parser
- **JSON Generation**: Builder with fluent API
- **PSRAM Support**: Large documents (64KB+)
- **Zero-Copy**: Efficient string handling

**Document Sizing**:

```cpp
// Small documents (< 1KB) - Stack
StaticJsonDocument<1024> doc;

// Medium documents (1KB - 16KB) - Heap
DynamicJsonDocument doc(8192);

// Large documents (> 16KB) - PSRAM
DynamicJsonDocument doc(65536, heap_caps_malloc, MALLOC_CAP_SPIRAM);
```

**Usage Example**:

```cpp
// Parse JSON
DynamicJsonDocument doc(2048);
DeserializationError error = deserializeJson(doc, jsonString);

// Generate JSON
JsonObject obj = doc.to<JsonObject>();
obj["device_id"] = "D123ABC";
obj["status"] = "online";
serializeJson(doc, output);
```

**Related**: All manager classes (BLEManager, ConfigManager, CRUDHandler, etc.)

---

### 15. C++ STL (Standard Template Library)

**Purpose**: Data structures and algorithms

**Provider**: GNU C++ Standard Library (integrated) **Version**: C++11/C++14
**License**: GPL with runtime exception

**Headers Used**:

```cpp
#include <vector>
#include <map>
#include <queue>
#include <deque>
#include <functional>
#include <memory>
#include <atomic>
#include <new>
```

**Key Components**:

**std::vector** - Dynamic arrays:

```cpp
std::vector<String> deviceIds;
std::vector<ModbusRegister> registers;
```

**std::map** - Key-value storage:

```cpp
std::map<String, DeviceConfig> deviceCache;
std::map<String, uint32_t> retryTimers;
```

**std::queue** - FIFO queues:

```cpp
std::queue<BLECommand> commandQueue;
std::priority_queue<Task> taskQueue;
```

**std::deque** - Double-ended queues:

```cpp
std::deque<MqttMessage> persistentQueue;
std::deque<uint32_t> metricsWindow;
```

**std::unique_ptr** - Smart pointers:

```cpp
std::unique_ptr<ConfigManager> configMgr;
```

**Related**: All firmware components

---

## System Libraries

### 16. LittleFS (ESP32 Built-in)

**Purpose**: Flash file system for configuration storage

**Provider**: Espressif (integrated) **Version**: Built-in **License**: BSD
3-Clause

**Headers Used**:

```cpp
#include <LittleFS.h>
```

**Key Features**:

- **Wear Leveling**: Automatic flash wear management
- **Power-Safe**: Atomic operations with WAL
- **Crash Recovery**: Automatic filesystem repair
- **Namespace**: Isolated storage areas

**Configuration**:

```cpp
LittleFS.begin(true); // format on mount failure
```

**File Structure**:

```
/devices.json           → Device configurations
/server_config.json     → MQTT/HTTP server settings
/logging_config.json    → Log level configuration
/mqtt_queue.dat         → Persistent MQTT queue
/devices.tmp            → Atomic write temporary file
/devices.wal            → Write-ahead log
```

**Atomic File Operations**:

```cpp
// Write-ahead logging pattern
1. Write to /devices.tmp
2. Create /devices.wal with operation log
3. Atomic rename /devices.tmp → /devices.json
4. Remove /devices.wal
```

**Related**: [ConfigManager.cpp](../Main/ConfigManager.cpp),
[AtomicFileOps.cpp](../Main/AtomicFileOps.cpp)

---

### 17. ESP32 Memory Management

**Purpose**: Heap allocation with PSRAM support

**Provider**: Espressif (integrated) **Version**: Built-in **License**: Apache
2.0

**Headers Used**:

```cpp
#include <esp_heap_caps.h>
#include <esp_psram.h>
```

**Key Features**:

- **PSRAM Allocation**: 8MB external RAM
- **Heap Caps**: Memory capability flags
- **Fragmentation Management**: Placement new
- **Memory Monitoring**: Free/used tracking

**PSRAM Allocation**:

```cpp
// Allocate in PSRAM
void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

// Placement new for C++ objects
ConfigManager* cfg = (ConfigManager*)heap_caps_malloc(
    sizeof(ConfigManager),
    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
);
new (cfg) ConfigManager();
```

**Memory Monitoring**:

```cpp
Serial.printf("PSRAM total: %d bytes\n", ESP.getPsramSize());
Serial.printf("PSRAM free: %d bytes\n", ESP.getFreePsram());
Serial.printf("Heap free: %d bytes\n", ESP.getFreeHeap());
```

**Related**: [MemoryManager.h](../Main/MemoryManager.h),
[PSRAMValidator.cpp](../Main/PSRAMValidator.cpp)

---

### 18. ESP32 Task Watchdog

**Purpose**: Task monitoring and deadlock prevention

**Provider**: Espressif (integrated) **Version**: Built-in **License**: Apache
2.0

**Headers Used**:

```cpp
#include <esp_task_wdt.h>
```

**Configuration**:

```cpp
// Enable watchdog for task
esp_task_wdt_add(NULL);

// Reset watchdog (call regularly in task loop)
esp_task_wdt_reset();

// Disable watchdog before blocking operations
esp_task_wdt_delete(NULL);
```

**Watchdog Timeout**: 10 seconds (default)

**Related**: [MqttManager.cpp](../Main/MqttManager.cpp)

---

### 19. ESP32 System

**Purpose**: System-level functions and utilities

**Provider**: Espressif (integrated) **Version**: Built-in **License**: Apache
2.0

**Headers Used**:

```cpp
#include <esp_system.h>
```

**Key Functions**:

```cpp
// Random number generation
uint32_t seed = esp_random();

// System restart
esp_restart();

// Chip information
esp_chip_info_t chip_info;
esp_chip_info(&chip_info);
```

---

## Installation Guide

### Arduino IDE Setup

**1. Install Arduino IDE**:

- Download from https://www.arduino.cc/en/software
- Install version 2.0 or higher

**2. Add ESP32 Board Support**:

```
File → Preferences → Additional Boards Manager URLs:
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

**3. Install ESP32 Platform**:

```
Tools → Board → Boards Manager → Search "esp32" → Install "esp32 by Espressif Systems"
```

**4. Install Required Libraries**:

```
Tools → Manage Libraries → Search and install:
- ArduinoJson (v7.4.2+) by Benoit Blanchon
- RTClib (v2.1.4+) by Adafruit
- NTPClient (v3.2.1+) by Fabrice Weinberg
- Ethernet (v2.0.2+) by Arduino
- TBPubSubClient (v2.12.1+) by ThingsBoard
- ArduinoHttpClient (v0.6.1+) by Arduino [EXPERIMENTAL]
- ModbusMaster (v2.0.1+) by Doc Walker
- OneButton (v2.0+) by Matthias Hertel
```

**5. Board Configuration**:

```
Tools → Board → ESP32 Arduino → ESP32-S3 Dev Module
Tools → Flash Size → 16MB (128Mb)
Tools → PSRAM → OPI PSRAM
Tools → Partition Scheme → Default 4MB with spiffs
Tools → Upload Speed → 921600
Tools → USB CDC On Boot → Enabled
```

---

## Version Compatibility

### Tested Versions

| Library                | Minimum Version | Tested Version | Status           |
| ---------------------- | --------------- | -------------- | ---------------- |
| **Arduino Core ESP32** | 2.0.0           | 2.0.11         | ✅ Stable        |
| **ArduinoJson**        | 7.4.2           | 7.4.2          | ✅ Stable (v7.x) |
| **RTClib**             | 2.1.4           | 2.1.4          | ✅ Stable        |
| **NTPClient**          | 3.2.1           | 3.2.1          | ✅ Stable        |
| **Ethernet**           | 2.0.2           | 2.0.2          | ✅ Stable        |
| **TBPubSubClient**     | 2.12.1          | 2.12.1         | ✅ Stable        |
| **ArduinoHttpClient**  | 0.6.1           | 0.6.1          | ⚠️ Experimental  |
| **ModbusMaster**       | 2.0.1           | 2.0.1          | ✅ Stable        |
| **OneButton**          | 2.0.0           | 2.5.0          | ✅ Stable        |

### Compatibility Notes

**ArduinoJson v7.x**:

- ✅ **v7.4.2 (Used)**: Latest version with improved performance
- ⚠️ **v6.x (Legacy)**: Not compatible with this firmware
- **Migration**: Firmware uses v7 API with JsonDocument

**ESP32 Arduino Core**:

- ✅ **2.0.x**: Fully supported
- ⚠️ **3.0.x**: Beta, not recommended for production

**TBPubSubClient**:

- ThingsBoard fork with enhanced features
- Buffer size increased to 2048 bytes (default 256)
- Socket timeout set to 30 seconds
- Compatible with all MQTT brokers (not only ThingsBoard)

**ArduinoHttpClient**:

- Experimental status, stable in production
- WebSocket support available
- Works with WiFi and Ethernet clients

---

## License Information

### Library Licenses Summary

| Library            | License      | Commercial Use | Attribution Required |
| ------------------ | ------------ | -------------- | -------------------- |
| Arduino Core ESP32 | LGPL 2.1     | ✅ Yes         | Yes                  |
| ArduinoJson        | MIT          | ✅ Yes         | Yes                  |
| RTClib             | MIT          | ✅ Yes         | Yes                  |
| NTPClient          | MIT          | ✅ Yes         | Yes                  |
| Ethernet           | LGPL 2.1     | ✅ Yes         | Yes                  |
| TBPubSubClient     | MIT          | ✅ Yes         | Yes                  |
| ArduinoHttpClient  | Apache 2.0   | ✅ Yes         | Yes                  |
| ModbusMaster       | Apache 2.0   | ✅ Yes         | Yes                  |
| OneButton          | BSD 3-Clause | ✅ Yes         | Yes                  |
| FreeRTOS           | MIT          | ✅ Yes         | Yes                  |

**All libraries are compatible with commercial use in the SRT-MGATE-1210
firmware.**

---

## Custom Library Modifications

### Modified Libraries

None of the third-party libraries have been modified. All customizations are
implemented in separate wrapper classes:

- **MQTTPersistentQueue**: Custom implementation (not library modification)
- **NetworkHysteresis**: Custom implementation
- **AtomicFileOps**: Custom implementation
- **ErrorHandler**: Custom implementation

---

## Dependency Graph

```
Main.ino
├── BLEManager.h
│   ├── BLEDevice.h (ESP32 BLE)
│   ├── BLEServer.h
│   ├── BLEUtils.h
│   ├── BLE2902.h
│   └── ArduinoJson.h
├── ConfigManager.h
│   ├── ArduinoJson.h
│   ├── LittleFS.h
│   └── AtomicFileOps.h
├── NetworkManager.h
│   ├── WiFiManager.h
│   │   └── WiFi.h
│   ├── EthernetManager.h
│   │   ├── SPI.h
│   │   └── Ethernet.h
│   └── NetworkHysteresis.h
├── ModbusRtuService.h
│   ├── HardwareSerial.h
│   └── ModbusMaster.h
├── MqttManager.h
│   ├── PubSubClient.h
│   ├── WiFi.h
│   ├── Ethernet.h
│   └── MQTTPersistentQueue.h
├── HttpManager.h
│   ├── HTTPClient.h
│   └── WiFi.h
├── RTCManager.h
│   ├── Wire.h
│   ├── RTClib.h
│   └── WiFi.h (for NTP)
├── LEDManager.h
│   └── Arduino.h
├── ButtonManager.h
│   └── OneButton.h
└── QueueManager.h
    └── FreeRTOS.h
```

---

## Troubleshooting

### Library Installation Issues

**Problem**: Library not found during compilation

**Solution**:

```
Arduino IDE:
Sketch → Include Library → Manage Libraries → Search library name → Reinstall
```

---

**Problem**: PSRAM allocation fails

**Solution**:

```cpp
// Check PSRAM availability
if (!esp_psram_is_initialized()) {
    Serial.println("PSRAM not initialized!");
    // Fallback to internal RAM
}
```

---

**Problem**: Modbus timeout errors

**Solution**:

```cpp
// Increase timeout
node.setTimeout(2000); // 2 seconds

// Check RS485 baudrate
Serial1.updateBaudRate(9600);
```

---

## Additional Resources

- **API Documentation**: [docs/API.md](API.md)
- **Hardware Specifications**: [docs/HARDWARE.md](HARDWARE.md)
- **Modbus Data Types**: [docs/MODBUS_DATATYPES.md](MODBUS_DATATYPES.md)
- **Troubleshooting Guide**: [docs/TROUBLESHOOTING.md](TROUBLESHOOTING.md)
- **Contributing Guidelines**: [CONTRIBUTING.md](../CONTRIBUTING.md)

---

**Copyright © 2025 PT Surya Inovasi Prioritas (SURIOTA)**

_This document is part of the SRT-MGATE-1210 firmware documentation. Licensed
under MIT License._
