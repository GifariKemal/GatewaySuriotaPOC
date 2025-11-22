# Technical Guides

**SRT-MGATE-1210 Technical Documentation**

[Home](../../README.md) > [Documentation](../README.md) > Technical Guides

---

## Overview

This directory contains comprehensive technical documentation for the SRT-MGATE-1210 Modbus IIoT Gateway. These guides provide in-depth information about hardware specifications, protocols, configuration, and troubleshooting.

---

## Quick Navigation

### Getting Started
- **[Hardware Specifications](HARDWARE.md)** - ESP32-S3 specs, GPIO pinout, electrical characteristics
- **[Network Configuration](NETWORK_CONFIGURATION.md)** - WiFi, Ethernet, failover setup
- **[Protocol Documentation](PROTOCOL.md)** - BLE, Modbus RTU/TCP, MQTT, HTTP protocols

### Configuration Guides
- **[MQTT Publish Modes](MQTT_PUBLISH_MODES_DOCUMENTATION.md)** - MQTT configuration and payload formats
- **[Modbus Data Types](MODBUS_DATATYPES.md)** - Supported data types, endianness, manufacturer configs
- **[Register Calibration](REGISTER_CALIBRATION_DOCUMENTATION.md)** - Sensor calibration and scaling

### Troubleshooting & Maintenance
- **[Troubleshooting Guide](TROUBLESHOOTING.md)** - Common issues, diagnosis, and solutions
- **[Logging System](LOGGING.md)** - Log levels, filtering, analysis
- **[Libraries & Dependencies](LIBRARIES.md)** - Third-party libraries

### Contributing
- **[Contributing Guidelines](CONTRIBUTING.md)** - How to contribute to documentation

---

## Document Index

| Document | Description | Topics Covered |
|----------|-------------|----------------|
| [API.md](../API_Reference/API.md) | Complete BLE CRUD API | Device operations, register operations, batch operations, streaming |
| [HARDWARE.md](HARDWARE.md) | Hardware specifications | ESP32-S3, GPIO, LEDs, power, electrical specs |
| [NETWORK_CONFIGURATION.md](NETWORK_CONFIGURATION.md) | Network setup | WiFi, Ethernet, dual network, failover |
| [PROTOCOL.md](PROTOCOL.md) | Communication protocols | BLE, Modbus RTU, Modbus TCP, MQTT, HTTP |
| [MQTT_PUBLISH_MODES_DOCUMENTATION.md](MQTT_PUBLISH_MODES_DOCUMENTATION.md) | MQTT configuration | Default mode, customize mode, payload formats |
| [MODBUS_DATATYPES.md](MODBUS_DATATYPES.md) | Modbus data types | 40+ data types, endianness, troubleshooting |
| [REGISTER_CALIBRATION_DOCUMENTATION.md](REGISTER_CALIBRATION_DOCUMENTATION.md) | Register calibration | Scale, offset, unit conversion |
| [TROUBLESHOOTING.md](TROUBLESHOOTING.md) | Problem solving | 30+ common issues with solutions |
| [LOGGING.md](LOGGING.md) | Logging system | Log levels, filtering, analysis techniques |
| [LIBRARIES.md](LIBRARIES.md) | Dependencies | Third-party libraries and versions |

---

## By Topic

### Hardware & Installation
- [Hardware Specifications](HARDWARE.md) - Complete hardware reference
- [Network Configuration](NETWORK_CONFIGURATION.md) - Network setup and connectivity

### Configuration & Setup
- [Network Configuration](NETWORK_CONFIGURATION.md) - WiFi/Ethernet setup
- [MQTT Publish Modes](MQTT_PUBLISH_MODES_DOCUMENTATION.md) - MQTT configuration
- [Register Calibration](REGISTER_CALIBRATION_DOCUMENTATION.md) - Sensor calibration
- [Modbus Data Types](MODBUS_DATATYPES.md) - Data type selection

### Communication Protocols
- [Protocol Documentation](PROTOCOL.md) - All protocol specifications
- [MQTT Publish Modes](MQTT_PUBLISH_MODES_DOCUMENTATION.md) - MQTT details
- [API Reference](../API_Reference/API.md) - BLE API

### Troubleshooting & Maintenance
- [Troubleshooting Guide](TROUBLESHOOTING.md) - Issue diagnosis and resolution
- [Logging System](LOGGING.md) - Log analysis
- [Hardware Specifications](HARDWARE.md#led-indicators) - LED indicator meanings

### Development
- [Libraries & Dependencies](LIBRARIES.md) - Required libraries
- [Contributing Guidelines](CONTRIBUTING.md) - Contribution process
- [Protocol Documentation](PROTOCOL.md) - Protocol implementation details

---

## Related Documentation

- **[Quick Start Guide](../QUICKSTART.md)** - Get started in 5 minutes
- **[Best Practices](../BEST_PRACTICES.md)** - Production deployment guidelines
- **[FAQ](../FAQ.md)** - Frequently asked questions
- **[API Reference](../API_Reference/API.md)** - Complete API documentation

---

## Support

For additional help:
- **Quick Fixes:** [Troubleshooting Guide](TROUBLESHOOTING.md)
- **Common Questions:** [FAQ](../FAQ.md)
- **Version Info:** [Version History](../Changelog/VERSION_HISTORY.md)

---

**Last Updated:** November 21, 2025
**Firmware Version:** 2.3.0

[← Back to Documentation Index](../README.md) | [↑ Top](#technical-guides)
