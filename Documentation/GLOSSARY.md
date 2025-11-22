# Glossary

**SRT-MGATE-1210 Technical Terminology Reference**

[Home](../README.md) > [Documentation](README.md) > Glossary

---

## Overview

This glossary provides definitions for technical terms, acronyms, and concepts used throughout the SRT-MGATE-1210 documentation.

---

## A

**Address (Modbus)**
The memory location of a register in a Modbus device. Different manufacturers use different addressing schemes (0-based vs 1-based). See [MODBUS_DATATYPES.md](Technical_Guides/MODBUS_DATATYPES.md).

**API (Application Programming Interface)**
The BLE-based interface for configuring and controlling the gateway. See [API Reference](API_Reference/API.md).

---

## B

**Batch Operations**
A feature allowing multiple API commands to be executed in a single transaction, improving efficiency. See [API.md - Batch Operations](API_Reference/API.md#batch-operations).

**Baud Rate**
The speed of serial communication in bits per second (bps). Common values: 9600, 19200, 115200. See [BEST_PRACTICES.md](BEST_PRACTICES.md#modbus-rtu).

**BLE (Bluetooth Low Energy)**
Wireless communication protocol used for gateway configuration and control. Max range ~10m. See [PROTOCOL.md](Technical_Guides/PROTOCOL.md).

**Broker (MQTT)**
A server that receives, filters, and distributes MQTT messages between publishers and subscribers.

---

## C

**Calibration**
Process of adjusting raw register values using scale and offset to get accurate measurements. Formula: `final_value = (raw_value × scale) + offset`. See [REGISTER_CALIBRATION_DOCUMENTATION.md](Technical_Guides/REGISTER_CALIBRATION_DOCUMENTATION.md).

**Clean Session (MQTT)**
MQTT setting that determines whether the broker should persist session state. Use `false` for production. See [BEST_PRACTICES.md](BEST_PRACTICES.md#mqtt-configuration).

**Client ID (MQTT)**
Unique identifier for MQTT connection. Should be unique per gateway. Recommended format: `gateway-{serial_number}`.

**CRUD**
Create, Read, Update, Delete - basic operations for managing devices and registers via BLE API.

**Customize Mode (MQTT)**
Advanced MQTT publishing mode allowing multiple topics with different registers and intervals. See [MQTT_PUBLISH_MODES_DOCUMENTATION.md](Technical_Guides/MQTT_PUBLISH_MODES_DOCUMENTATION.md#customize-mode).

---

## D

**Data Type**
Format specifying how raw Modbus register bytes should be interpreted (e.g., INT16, FLOAT32_ABCD). See [MODBUS_DATATYPES.md](Technical_Guides/MODBUS_DATATYPES.md).

**Default Mode (MQTT)**
Simple MQTT publishing mode sending all data in one payload to one topic. See [MQTT_PUBLISH_MODES_DOCUMENTATION.md](Technical_Guides/MQTT_PUBLISH_MODES_DOCUMENTATION.md#default-mode).

**DHCP (Dynamic Host Configuration Protocol)**
Automatic IP address assignment. Recommended for easier deployment. Can be disabled for static IP configuration.

**Device**
A Modbus slave device (sensor, meter, actuator) connected to the gateway. Each device has a unique slave ID.

---

## E

**Endianness**
Byte order for multi-byte values. Common formats:
- **ABCD** (Big-endian): Most significant byte first
- **CDAB** (Little-endian): Least significant byte first
- **BADC**, **DCBA**: Mixed endian variants

See [MODBUS_DATATYPES.md - Endianness](Technical_Guides/MODBUS_DATATYPES.md#endianness-explained).

**ESP32-S3**
Microcontroller used in the gateway. Features: dual-core, WiFi, BLE, 16MB Flash, 8MB PSRAM.

**Ethernet**
Wired network connection via W5500 chip. More stable than WiFi, recommended for production. See [NETWORK_CONFIGURATION.md](Technical_Guides/NETWORK_CONFIGURATION.md).

---

## F

**Failover**
Automatic switch from primary to backup network when primary fails. Occurs within 5 seconds. See [NETWORK_CONFIGURATION.md - Failover](Technical_Guides/NETWORK_CONFIGURATION.md#failover-logic).

**Firmware**
Software running on the gateway. Current version: v2.3.0. See [VERSION_HISTORY.md](Changelog/VERSION_HISTORY.md).

**Flash Memory**
Non-volatile storage (16MB) for firmware and configuration. Data persists across reboots.

**Function Code (Modbus)**
Specifies the type of Modbus operation:
- **FC3 (0x03)**: Read Holding Registers
- **FC4 (0x04)**: Read Input Registers
- **FC6 (0x06)**: Write Single Register
- **FC16 (0x10)**: Write Multiple Registers

See [PROTOCOL.md - Modbus](Technical_Guides/PROTOCOL.md#modbus-protocols).

---

## G

**Gateway**
The SRT-MGATE-1210 device that bridges Modbus devices to MQTT/HTTP/BLE networks.

**GPIO (General Purpose Input/Output)**
Programmable pins on the ESP32-S3. See [HARDWARE.md - GPIO Pinout](Technical_Guides/HARDWARE.md#gpio-pinout).

---

## H

**Holding Register**
Read/write Modbus register type (Function Code 3). Can store setpoints or configuration values.

**HTTP (Hypertext Transfer Protocol)**
Protocol for posting sensor data to web servers. Use HTTPS (port 443) in production. See [BEST_PRACTICES.md - HTTP](BEST_PRACTICES.md#http-configuration).

---

## I

**IIoT (Industrial Internet of Things)**
Industrial applications of IoT technology, including sensor monitoring and automation.

**Input Register**
Read-only Modbus register type (Function Code 4). Typically used for sensor measurements.

**Interval**
Time between data reads or MQTT publishes. Units: milliseconds (ms), seconds (s), or minutes (m).

---

## J

**JSON (JavaScript Object Notation)**
Data format used for BLE API commands and MQTT payloads.

---

## K

**Keep-Alive (MQTT)**
Time interval (in seconds) for MQTT ping messages to maintain connection. Recommended: 60s.

---

## L

**LED Indicators**
Status lights on the gateway showing power, network, and error states. See [HARDWARE.md - LED Indicators](Technical_Guides/HARDWARE.md#led-indicators).

**Log Level**
Verbosity of logging output:
- **DEBUG**: Detailed debugging (development)
- **INFO**: General information (production)
- **WARNING**: Warning conditions
- **ERROR**: Error conditions only

See [LOGGING.md](Technical_Guides/LOGGING.md).

---

## M

**Modbus**
Industrial communication protocol for connecting sensors and devices. Variants: RTU (serial) and TCP (Ethernet).

**Modbus RTU (Remote Terminal Unit)**
Serial version of Modbus using RS485 physical layer. See [PROTOCOL.md - Modbus RTU](Technical_Guides/PROTOCOL.md#modbus-rtu-protocol).

**Modbus TCP**
Ethernet version of Modbus using TCP/IP. See [PROTOCOL.md - Modbus TCP](Technical_Guides/PROTOCOL.md#modbus-tcp-protocol).

**MQTT (Message Queuing Telemetry Transport)**
Lightweight publish-subscribe messaging protocol for IoT. See [MQTT_PUBLISH_MODES_DOCUMENTATION.md](Technical_Guides/MQTT_PUBLISH_MODES_DOCUMENTATION.md).

---

## N

**Network Mode**
Configuration determining which network interfaces are used:
- **wifi_only**: WiFi only
- **ethernet_only**: Ethernet only
- **auto**: Automatic failover between both

See [NETWORK_CONFIGURATION.md - Communication Mode](Technical_Guides/NETWORK_CONFIGURATION.md#communication-mode).

---

## O

**Offset**
Constant value added during calibration. Example: `-2.5` to correct sensor bias. Formula: `(raw × scale) + offset`.

**Over-the-Air (OTA)**
Firmware update method without physical connection (if supported by firmware).

---

## P

**Payload**
Data content of a message. MQTT payloads contain sensor readings in JSON format.

**Polling**
Repeatedly reading Modbus registers at specified intervals. Configured per device.

**Port**
Network communication endpoint:
- **MQTT:** 1883 (unencrypted), 8883 (TLS/SSL)
- **HTTP:** 80 (unencrypted), 443 (HTTPS)
- **Modbus TCP:** 502 (default)

**PSRAM (Pseudo Static RAM)**
External RAM (8MB) for large data structures and buffering.

**Publish (MQTT)**
Sending data to an MQTT broker on a specific topic.

---

## Q

**QoS (Quality of Service)**
MQTT message delivery guarantee level:
- **QoS 0**: At most once (may lose)
- **QoS 1**: At least once (recommended for production)
- **QoS 2**: Exactly once (slowest)

See [FAQ - MQTT QoS](FAQ.md#should-i-use-qos-0-1-or-2-for-mqtt).

---

## R

**Register**
A memory location in a Modbus device storing a sensor value or configuration setting.

**Register ID**
Unique string identifier for a register (e.g., `temp_room_1`, `voltage_l1`). Used in MQTT Customize Mode. See [MQTT_PUBLISH_MODES_DOCUMENTATION.md - Register ID](Technical_Guides/MQTT_PUBLISH_MODES_DOCUMENTATION.md#what-is-register_id).

**Retry Count**
Number of times to retry a failed Modbus read before giving up. Recommended: 3.

**RS485**
Serial communication standard used by Modbus RTU. Wiring: A (D+), B (D-), GND.

**RSSI (Received Signal Strength Indicator)**
WiFi signal strength measurement. Good: > -70 dBm. Poor: < -80 dBm.

**RTU (Remote Terminal Unit)**
See Modbus RTU.

---

## S

**Scale**
Multiplication factor for calibration. Example: `0.01` to convert `2500` to `25.0`. Formula: `(raw × scale) + offset`.

**Slave ID**
Unique identifier (1-247) for Modbus RTU devices on the same bus. Must match device configuration.

**SSID (Service Set Identifier)**
WiFi network name. Case-sensitive.

**Streaming**
Real-time data transmission over BLE for monitoring. See [API.md - Data Streaming](API_Reference/API.md#data-streaming).

**Subscribe (MQTT)**
Registering to receive messages on specific MQTT topics.

---

## T

**TCP (Transmission Control Protocol)**
Reliable, connection-oriented network protocol. Used by Modbus TCP and MQTT.

**Telemetry**
Remote measurement and data transmission from sensors.

**Timeout**
Maximum wait time for Modbus device response. Recommended: 1000ms (1 second).

**TLS/SSL (Transport Layer Security/Secure Sockets Layer)**
Encryption protocols for secure MQTT and HTTP connections. Always use in production.

**Topic (MQTT)**
Hierarchical path for MQTT messages. Example: `factory/line1/temperature/zone_a`. See [BEST_PRACTICES.md - MQTT Topics](BEST_PRACTICES.md#mqtt-topics-naming-convention).

---

## U

**UART (Universal Asynchronous Receiver-Transmitter)**
Serial communication interface. Used by BLE service for data exchange.

**Unit**
Measurement unit for a register value (e.g., `°C`, `V`, `A`, `bar`, `PSI`).

**UUID (Universally Unique Identifier)**
Unique identifier for BLE services and characteristics. Gateway UART service: `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`.

---

## V

**VLAN (Virtual Local Area Network)**
Network segmentation technique. Recommended for isolating IoT devices. See [BEST_PRACTICES.md - Network Security](BEST_PRACTICES.md#network-security).

---

## W

**W5500**
Ethernet controller chip used in the gateway. Supports 10/100 Mbps with hardware TCP/IP stack.

**Watchdog Timer**
Hardware safety mechanism that resets the system if firmware hangs. Prevents permanent freezes.

**WiFi**
Wireless network connection using 2.4GHz 802.11 b/g/n. Max range depends on environment. See [NETWORK_CONFIGURATION.md - WiFi](Technical_Guides/NETWORK_CONFIGURATION.md#2-wifi-esp32-s3).

**Word**
Two bytes (16 bits) in Modbus protocol. Register addresses typically refer to 16-bit words.

**WPA2/WPA3**
WiFi security protocols. Always use for secure wireless connections. See [BEST_PRACTICES.md - WiFi Security](BEST_PRACTICES.md#wifi).

---

## Acronyms Quick Reference

| Acronym | Full Name |
|---------|-----------|
| **API** | Application Programming Interface |
| **BLE** | Bluetooth Low Energy |
| **CRUD** | Create, Read, Update, Delete |
| **DHCP** | Dynamic Host Configuration Protocol |
| **FC** | Function Code |
| **GPIO** | General Purpose Input/Output |
| **HTTP** | Hypertext Transfer Protocol |
| **IIoT** | Industrial Internet of Things |
| **IP** | Internet Protocol |
| **JSON** | JavaScript Object Notation |
| **LED** | Light Emitting Diode |
| **MQTT** | Message Queuing Telemetry Transport |
| **OTA** | Over-the-Air |
| **PSRAM** | Pseudo Static RAM |
| **QoS** | Quality of Service |
| **RS485** | Recommended Standard 485 |
| **RSSI** | Received Signal Strength Indicator |
| **RTU** | Remote Terminal Unit |
| **SSID** | Service Set Identifier |
| **TCP** | Transmission Control Protocol |
| **TLS** | Transport Layer Security |
| **UART** | Universal Asynchronous Receiver-Transmitter |
| **UUID** | Universally Unique Identifier |
| **VLAN** | Virtual Local Area Network |
| **WPA** | WiFi Protected Access |

---

## Common Value Ranges

### Modbus
- **Slave ID**: 1-247
- **Baud Rate**: 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200
- **Data Bits**: 7 or 8
- **Stop Bits**: 1 or 2
- **Parity**: None (N), Even (E), Odd (O)

### MQTT
- **Port**: 1883 (unencrypted), 8883 (TLS)
- **QoS**: 0, 1, or 2
- **Keep-Alive**: 30-120 seconds (recommended: 60)

### Network
- **WiFi RSSI**: Good > -70 dBm, Poor < -80 dBm
- **Failover Delay**: 3-10 seconds (recommended: 5)

### Performance
- **Polling Interval**: 100ms - 60s (device dependent)
- **MQTT Publish**: 1-60 seconds (data dependent)
- **HTTP Post**: 5-60 seconds (data dependent)

---

## Related Documentation

- **[Quick Start Guide](QUICKSTART.md)** - Get started with the gateway
- **[Best Practices](BEST_PRACTICES.md)** - Production deployment guidelines
- **[FAQ](FAQ.md)** - Frequently asked questions
- **[API Reference](API_Reference/API.md)** - Complete API documentation
- **[Troubleshooting Guide](Technical_Guides/TROUBLESHOOTING.md)** - Problem solving

---

**Document Version:** 1.0
**Last Updated:** November 21, 2025
**Firmware Version:** 2.3.0

[← Back to Documentation Index](README.md) | [↑ Top](#glossary)
