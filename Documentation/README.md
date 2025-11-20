# SRT-MGATE-1210 Documentation

**Modbus IIoT Gateway Documentation Hub**
**Current Version:** 2.2.0
**Last Updated:** November 20, 2025

---

## Welcome

This is the complete documentation for the **SRT-MGATE-1210 Modbus IIoT Gateway**, an ESP32-S3 based industrial IoT device that bridges Modbus RTU/TCP devices with modern cloud platforms via MQTT, HTTP, and BLE interfaces.

**New to the gateway?** Start with the [Quick Start Guide](QUICKSTART.md) to get up and running in 5 minutes.

---

## 📚 Documentation Structure

### Getting Started

| Document | Description | Audience |
|----------|-------------|----------|
| [Quick Start Guide](QUICKSTART.md) | Get your gateway configured in 5 minutes | All Users |
| [Best Practices](BEST_PRACTICES.md) | Recommended configurations and patterns | Developers, Integrators |
| [FAQ](FAQ.md) | Frequently asked questions | All Users |

### Core Documentation

#### API & Integration
- **[API Reference](API_Reference/API.md)** - Complete BLE CRUD API documentation with code examples
  - Device operations (CRUD)
  - Register operations (CRUD)
  - Configuration management
  - Batch operations
  - Real-time data streaming
  - Error codes and troubleshooting

#### Technical Guides
- **[Protocol Documentation](Technical_Guides/PROTOCOL.md)** - Communication protocols (BLE, Modbus RTU/TCP, MQTT, HTTP)
- **[Network Configuration](Technical_Guides/NETWORK_CONFIGURATION.md)** - WiFi, Ethernet, failover, and connectivity
- **[MQTT Publish Modes](Technical_Guides/MQTT_PUBLISH_MODES_DOCUMENTATION.md)** - MQTT configuration and payload formats
- **[Modbus Data Types](Technical_Guides/MODBUS_DATATYPES.md)** - Supported data types, endianness, manufacturer configurations
- **[Register Calibration](Technical_Guides/REGISTER_CALIBRATION_DOCUMENTATION.md)** - Sensor calibration and scaling
- **[Hardware Specifications](Technical_Guides/HARDWARE.md)** - ESP32-S3 specs, GPIO pinout, electrical characteristics
- **[Logging System](Technical_Guides/LOGGING.md)** - Log levels, filtering, analysis, and troubleshooting
- **[Libraries & Dependencies](Technical_Guides/LIBRARIES.md)** - Third-party libraries and versions

#### Problem Solving
- **[Troubleshooting Guide](Technical_Guides/TROUBLESHOOTING.md)** - Common issues, diagnosis, and solutions
  - BLE connection issues
  - Modbus communication problems
  - Network connectivity issues
  - MQTT/HTTP failures
  - Performance issues
  - LED indicator reference

#### Version Information
- **[Version History](Changelog/VERSION_HISTORY.md)** - Release notes, breaking changes, migration guides
- **[Capacity Analysis](Changelog/CAPACITY_ANALYSIS.md)** - Performance limits and scalability
- **[Use Cases & Failure Recovery](Changelog/USE_CASES_FAILURE_RECOVERY.md)** - Real-world scenarios

### Advanced Topics

#### Optimization & Development
- **[Implementation Summary](Optimizing/IMPLEMENTATION_SUMMARY.md)** - Development phases and optimizations
- **[Log Migration Guide](Optimizing/LOG_MIGRATION_GUIDE.md)** - Migrating to new log system
- **[Phase Summaries](Optimizing/)** - Detailed optimization phase documentation

#### Archive
- **[Archived Documentation](Archive/ARCHIVE_INFO.md)** - Deprecated and historical documentation
- Note: Archived files are kept for reference but may be outdated

---

## 🚀 Quick Navigation by Role

### I'm a Developer
1. Start: [Quick Start Guide](QUICKSTART.md)
2. API: [API Reference](API_Reference/API.md)
3. Code: [Code Examples](API_Reference/API.md#code-examples) (JavaScript, Python, Dart, C++)
4. Issues: [Troubleshooting Guide](Technical_Guides/TROUBLESHOOTING.md)

### I'm an Integrator
1. Start: [Best Practices](BEST_PRACTICES.md)
2. Network: [Network Configuration](Technical_Guides/NETWORK_CONFIGURATION.md)
3. Protocols: [MQTT](Technical_Guides/MQTT_PUBLISH_MODES_DOCUMENTATION.md) | [Modbus](Technical_Guides/MODBUS_DATATYPES.md)
4. Hardware: [Hardware Specifications](Technical_Guides/HARDWARE.md)

### I'm Troubleshooting
1. Quick Fixes: [Troubleshooting Guide](Technical_Guides/TROUBLESHOOTING.md)
2. Logs: [Logging System](Technical_Guides/LOGGING.md)
3. Common Issues: [FAQ](FAQ.md)
4. Bug History: [Changelog](Changelog/BUG_ANALYSIS.md) (archived)

### I'm Planning a Deployment
1. Best Practices: [Best Practices](BEST_PRACTICES.md)
2. Capacity: [Capacity Analysis](Changelog/CAPACITY_ANALYSIS.md)
3. Network Design: [Network Configuration](Technical_Guides/NETWORK_CONFIGURATION.md)
4. Failure Recovery: [Use Cases](Changelog/USE_CASES_FAILURE_RECOVERY.md)

---

## 📖 Quick Reference

### Product Specifications
- **Model:** SRT-MGATE-1210
- **MCU:** ESP32-S3-WROOM-1-N16R8 (16MB Flash, 8MB PSRAM)
- **Protocols:** Modbus RTU, Modbus TCP, MQTT, HTTP, BLE 5.0
- **Network:** WiFi 802.11 b/g/n, Ethernet (optional)
- **Firmware:** v2.2.0 (November 2025)

### Key Features
- ✅ **Multi-Protocol:** Bridge Modbus devices to MQTT/HTTP/BLE
- ✅ **Dual Connectivity:** WiFi + Ethernet with automatic failover
- ✅ **Real-Time Data:** Low-latency sensor data streaming
- ✅ **Scalable:** Support for multiple devices and registers
- ✅ **Secure:** BLE security, WiFi encryption, configurable authentication
- ✅ **Robust:** Watchdog timers, error recovery, connection management

### Supported Modbus Data Types
40+ data types including: INT16, UINT16, INT32, UINT32, FLOAT32, FLOAT64, STRING, BOOL, and manufacturer-specific formats. See [Modbus Data Types](Technical_Guides/MODBUS_DATATYPES.md).

### Communication Modes
1. **BLE** - Configuration and real-time data access
2. **MQTT** - Cloud integration (publish sensor data, subscribe to commands)
3. **HTTP** - RESTful API for data posting
4. **Modbus RTU** - RS485 serial communication with field devices
5. **Modbus TCP** - Ethernet-based Modbus communication

---

## 🔄 Version Information

### Current Version: 2.2.0 (November 14, 2025)

**Breaking Changes:**
- `data_interval` removed from root configuration
- `http_config.interval` now controls HTTP polling interval

**Key Improvements:**
- Clean API structure with dedicated config sections
- Enhanced network configuration documentation
- Improved error handling and recovery

**Migration:** See [Version History](Changelog/VERSION_HISTORY.md) for full migration guide.

### Previous Versions
- **v2.1.1** - 28x faster BLE transmission, enhanced CRUD responses
- **v2.1.0** - Priority queue, batch operations
- **v2.0** - Initial stable release

---

## 🆘 Getting Help

### Common Issues
1. **BLE won't connect** → [Troubleshooting: BLE Connection](Technical_Guides/TROUBLESHOOTING.md#ble-connection-issues)
2. **Modbus not reading** → [Troubleshooting: Modbus Issues](Technical_Guides/TROUBLESHOOTING.md#modbus-communication-problems)
3. **MQTT not publishing** → [Troubleshooting: MQTT](Technical_Guides/TROUBLESHOOTING.md#mqtt-issues)
4. **Network connectivity** → [Network Configuration](Technical_Guides/NETWORK_CONFIGURATION.md)

### Resources
- **Troubleshooting Guide:** [TROUBLESHOOTING.md](Technical_Guides/TROUBLESHOOTING.md)
- **FAQ:** [FAQ.md](FAQ.md)
- **Version History:** [VERSION_HISTORY.md](Changelog/VERSION_HISTORY.md)
- **LED Indicators:** [Hardware Specs - LED Section](Technical_Guides/HARDWARE.md#led-indicators)

---

## 📝 Contributing

Documentation contributions are welcome! See [CONTRIBUTING.md](Technical_Guides/CONTRIBUTING.md) for guidelines.

---

## 📂 Directory Structure

```
Documentation/
├── README.md (this file)                    # Documentation hub
├── QUICKSTART.md                            # 5-minute setup guide
├── BEST_PRACTICES.md                        # Recommended patterns
├── FAQ.md                                   # Frequently asked questions
├── GLOSSARY.md                              # Terminology reference
│
├── API_Reference/
│   └── API.md                               # Complete API reference
│
├── Technical_Guides/
│   ├── README.md                            # Technical guides index
│   ├── API.md                               # (Deprecated - see API_Reference/)
│   ├── PROTOCOL.md                          # Protocol specifications
│   ├── NETWORK_CONFIGURATION.md             # Network setup
│   ├── MQTT_PUBLISH_MODES_DOCUMENTATION.md  # MQTT configuration
│   ├── MODBUS_DATATYPES.md                  # Modbus data types
│   ├── REGISTER_CALIBRATION_DOCUMENTATION.md # Calibration guide
│   ├── HARDWARE.md                          # Hardware specs
│   ├── LOGGING.md                           # Logging system
│   ├── LIBRARIES.md                         # Dependencies
│   ├── TROUBLESHOOTING.md                   # Problem solving
│   └── CONTRIBUTING.md                      # Contribution guidelines
│
├── Changelog/
│   ├── README.md                            # Changelog index
│   ├── VERSION_HISTORY.md                   # Release notes
│   ├── CAPACITY_ANALYSIS.md                 # Performance limits
│   ├── USE_CASES_FAILURE_RECOVERY.md        # Real-world scenarios
│   └── BUG_ANALYSIS.md                      # Historical bugs (archived)
│
├── Optimizing/
│   ├── README.md                            # Optimization docs index
│   ├── IMPLEMENTATION_SUMMARY.md            # Development phases
│   ├── LOG_MIGRATION_GUIDE.md               # Log system migration
│   ├── PHASE_2_SUMMARY.md                   # Phase 2 details
│   ├── PHASE_3_SUMMARY.md                   # Phase 3 details
│   ├── PHASE_4_SUMMARY.md                   # Phase 4 details
│   └── OPTIMIZATION_COMPLETE.md             # Final summary
│
└── Archive/
    ├── README.md                            # Archive index
    ├── ARCHIVE_INFO.md                      # Archive information
    ├── BUG_ANALYSIS.md                      # v2.0 bugs (deprecated)
    ├── COMPARISON_MATRIX.md                 # Historical comparison
    ├── DELIVERY_SUMMARY.md                  # Old delivery docs
    ├── INTEGRATION_STEPS.md                 # Old integration guide
    ├── STREAMING_FIX_GUIDE.md               # Streaming fixes
    ├── README_STREAMING_FIX.md              # Streaming fix notes
    └── Modbus Data Types List.md            # Old data types
```

---

## 📄 Document Status

| Status | Description |
|--------|-------------|
| ✅ Current | Up-to-date documentation for v2.2.0 |
| 🔄 Active | Under active development/maintenance |
| 📦 Archived | Historical reference, may be outdated |
| ⚠️ Deprecated | Replaced by newer documentation |

---

**Documentation Version:** 1.0
**Last Review:** November 20, 2025
**Maintainer:** Kemal
**License:** Proprietary - SRT-MGATE-1210 Project
