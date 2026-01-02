# Changelog

**SRT-MGATE-1210 Version History and Changes**

[Home](../../README.md) > [Documentation](../README.md) > Changelog

---

## Overview

This directory contains version history, release notes, bug analysis, and
capacity documentation for the SRT-MGATE-1210 firmware.

---

## Quick Navigation

### Current Documentation

- **[Version History](VERSION_HISTORY.md)** - Complete release notes (v2.0 -
  v2.5.34)
- **[Capacity Analysis](CAPACITY_ANALYSIS.md)** - Performance limits and
  scalability
- **[Use Cases & Failure Recovery](USE_CASES_FAILURE_RECOVERY.md)** - Real-world
  scenarios

### Archived Documentation

- **[Bug Analysis](BUG_ANALYSIS.md)** ⚠️ ARCHIVED - v2.0.0 bug analysis (most
  issues resolved)

---

## Version History

### Current Version: 2.5.34 (December 10, 2025)

**Latest Fixes & Features (v2.5.x):**

- v2.5.34: CRITICAL memory allocator mismatch fix (new/delete vs malloc/free)
- v2.5.33: Network failover task (NET_FAILOVER_TASK) for auto-reconnection
- v2.5.32: Centralized ProductConfig.h, new BLE name format (MGate-1210(P)-XXXX)
- v2.5.31: Multi-gateway BLE support with unique names from MAC address
- v2.5.30: OTA buffer optimization (32KB for faster downloads)
- v2.5.11: Private GitHub repo OTA support via GitHub API
- v2.5.10: OTA signature bug fix (double-hash issue)

**Previous Critical Fixes (v2.5.1):**

- DRAM exhaustion fix: saveJson() uses PSRAM buffer instead of Arduino String
- MQTT loop fix: Interval timestamp updated BEFORE queue check
- Memory thresholds adjusted for realistic BLE operation
- SSL compatibility: setCACert() with DigiCert for ESP32 Arduino 3.x

**Key Features (v2.3.x series):**

- BLE command corruption fix + ModbusTCP optimization
- TCP connection pool (99% connection reuse)
- Backup/restore system, factory reset, device control

**See:** [VERSION_HISTORY.md](VERSION_HISTORY.md) for complete details

### Previous Versions

| Version     | Release Date | Key Features                                     |
| ----------- | ------------ | ------------------------------------------------ |
| **v2.5.34** | Dec 10, 2025 | Memory allocator fix (new/delete vs malloc/free) |
| **v2.5.33** | Dec 09, 2025 | Network failover task (NET_FAILOVER_TASK)        |
| **v2.5.32** | Dec 05, 2025 | ProductConfig.h, BLE name MGate-1210(P)-XXXX     |
| **v2.5.31** | Dec 04, 2025 | Multi-gateway BLE support                        |
| **v2.5.11** | Nov 28, 2025 | Private GitHub repo OTA support                  |
| **v2.5.1**  | Nov 27, 2025 | DRAM exhaustion fix, MQTT loop fix               |
| **v2.3.15** | Nov 26, 2025 | CRITICAL MQTT retain flag fix                    |
| **v2.3.11** | Nov 26, 2025 | BLE corruption fix, ModbusTCP optimization       |
| **v2.3.0**  | Nov 21, 2025 | Backup/restore, factory reset, device control    |
| **v2.0**    | Oct 2024     | Initial stable release                           |

---

## Document Index

| Document                                                       | Status      | Description                                    |
| -------------------------------------------------------------- | ----------- | ---------------------------------------------- |
| [VERSION_HISTORY.md](VERSION_HISTORY.md)                       | ✅ Current  | Complete version history with migration guides |
| [CAPACITY_ANALYSIS.md](CAPACITY_ANALYSIS.md)                   | ✅ Current  | Performance limits, scalability, benchmarks    |
| [USE_CASES_FAILURE_RECOVERY.md](USE_CASES_FAILURE_RECOVERY.md) | ✅ Current  | Real-world deployment scenarios                |
| [BUG_ANALYSIS.md](BUG_ANALYSIS.md)                             | ⚠️ Archived | v2.0.0 bug analysis (historical reference)     |

---

## What's New

### v2.5.34 Highlights (Current)

**Critical Bug Fixes:**

- Fixed memory allocator mismatch (new/delete vs malloc/free)
- Prevents heap corruption and random crashes
- Improved stability for long-term deployment

**See:** [VERSION_HISTORY.md](VERSION_HISTORY.md#v2534) for details

### v2.5.32 Highlights

**New Features:**

- **ProductConfig.h** - Single source of truth for product identity
- **New BLE Name Format** - `MGate-1210(P)-XXXX` (4 hex chars from MAC)
- **POE Variant Support** - Easy switch between POE and Non-POE builds
- **Serial Number Format** - `SRT-MGATE1210P-YYYYMMDD-XXXXXX`

**Breaking Changes:**

- BLE name changed from `SURIOTA-XXXXXX` to `MGate-1210(P)-XXXX`
- Mobile apps need to update BLE scan filter

### v2.5.1 Highlights

**Critical Fixes:**

- DRAM exhaustion fix in saveJson() - now uses PSRAM buffer
- MQTT loop spam fix - interval checked BEFORE queue empty check
- Memory thresholds adjusted (CRITICAL: 20KB→12KB, EMERGENCY: 10KB→8KB)
- SSL compatibility for ESP32 Arduino 3.x

**See:** [VERSION_HISTORY.md](VERSION_HISTORY.md#v251) for details

---

## Migration Guides

### Upgrading from v2.3.x to v2.5.x

**BLE Name Change (v2.5.32+):**

```
// OLD (v2.3.x - v2.5.31)
BLE Name: "SURIOTA-XXXXXX"  (6 hex chars)

// NEW (v2.5.32+)
BLE Name: "MGate-1210(P)-XXXX"  (4 hex chars)
```

**Mobile App Update Required:**

- Update BLE scan filter from `"SURIOTA-"` to `"MGate-1210"`
- Both formats supported in transition period

**Full Guide:** [VERSION_HISTORY.md - v2.5.32](VERSION_HISTORY.md#v2532)

### Upgrading from v2.1.1 to v2.3.0

**Configuration Changes:**

```json
// OLD (v2.1.1)
{
  "data_interval": 5000,
  "http": {
    "enabled": true
  }
}

// NEW (v2.3.0)
{
  "http_config": {
    "enabled": true,
    "interval": 5000
  }
}
```

**Full Guide:**
[VERSION_HISTORY.md - v2.3.0 Migration](VERSION_HISTORY.md#v220-migration-guide)

### Upgrading from v2.0 to v2.1.x

**Key Changes:**

- New batch operations API
- Priority queue system
- Enhanced BLE transmission speed

**Full Guide:**
[VERSION_HISTORY.md - v2.1.0 Migration](VERSION_HISTORY.md#v210-migration-guide)

---

## Bug Fixes by Version

### v2.3.0

- Fixed: Network configuration edge cases
- Improved: Error recovery mechanisms
- Enhanced: Configuration validation

### v2.1.1

- Fixed: BLE transmission performance
- Fixed: CRUD response format inconsistencies
- Fixed: N+1 query problem in device loading

### v2.1.0

- Fixed: Batch operation race conditions
- Fixed: Priority queue edge cases
- Fixed: Memory leaks in high-load scenarios

### v2.0

- Initial stable release
- Resolved streaming data issues
- Fixed network failover bugs

**Detailed History:** [VERSION_HISTORY.md](VERSION_HISTORY.md)

---

## Capacity & Performance

### System Limits

| Resource        | Limit     | Notes                    |
| --------------- | --------- | ------------------------ |
| **Devices**     | 50        | Recommended maximum      |
| **Registers**   | 500       | Across all devices       |
| **MQTT Topics** | 20        | Customize mode           |
| **BLE Payload** | 20KB      | Per transmission         |
| **Memory**      | 8MB PSRAM | Available for operations |

**Detailed Analysis:** [CAPACITY_ANALYSIS.md](CAPACITY_ANALYSIS.md)

### Performance Benchmarks

| Operation            | Performance  | Version      |
| -------------------- | ------------ | ------------ |
| **BLE Transmission** | 21KB in 2.1s | v2.1.1+      |
| **Device Read**      | < 100ms      | All versions |
| **MQTT Publish**     | < 50ms       | All versions |
| **Network Failover** | < 5s         | v2.0+        |

**See:** [CAPACITY_ANALYSIS.md](CAPACITY_ANALYSIS.md) for complete benchmarks

---

## Use Cases & Examples

### Production Deployments

**Manufacturing Plant:**

- 30 devices (temperature, pressure, flow)
- Dual network (Ethernet primary, WiFi backup)
- MQTT to ThingsBoard
- 99.8% uptime

**Warehouse Monitoring:**

- 15 temperature/humidity sensors
- WiFi only
- HTTP to custom backend
- Real-time alerting

**See:** [USE_CASES_FAILURE_RECOVERY.md](USE_CASES_FAILURE_RECOVERY.md) for
detailed case studies

### Failure Recovery Scenarios

- Network disconnection recovery
- MQTT broker failover
- Modbus device timeout handling
- Power loss recovery

**See:** [USE_CASES_FAILURE_RECOVERY.md](USE_CASES_FAILURE_RECOVERY.md)

---

## Known Issues & Workarounds

### Current Issues (v2.3.0)

**No critical issues reported**

For troubleshooting common problems:

- **[Troubleshooting Guide](../Technical_Guides/TROUBLESHOOTING.md)** - Complete
  problem-solving guide
- **[FAQ](../FAQ.md)** - Frequently asked questions

### Resolved Issues

See [VERSION_HISTORY.md](VERSION_HISTORY.md) for complete list of resolved
issues by version.

---

## Archived Documentation

### Bug Analysis (v2.0.0)

**Status:** ⚠️ ARCHIVED - October 2024

The [BUG_ANALYSIS.md](BUG_ANALYSIS.md) file contains historical bug analysis
from v2.0.0 development. Most issues have been resolved in subsequent versions.

**For current issues:** Use
[Troubleshooting Guide](../Technical_Guides/TROUBLESHOOTING.md) instead.

---

## Related Documentation

- **[Quick Start Guide](../QUICKSTART.md)** - Get started quickly
- **[Best Practices](../BEST_PRACTICES.md)** - Production deployment guidelines
- **[Troubleshooting Guide](../Technical_Guides/TROUBLESHOOTING.md)** - Problem
  solving
- **[API Reference](../API_Reference/API.md)** - API documentation

---

## Feedback & Issues

Found a bug or have a feature request?

1. Check [VERSION_HISTORY.md](VERSION_HISTORY.md) - may already be fixed
2. Review [Troubleshooting Guide](../Technical_Guides/TROUBLESHOOTING.md) - may
   have workaround
3. Contact: Kemal (Firmware Developer)

---

**Last Updated:** December 10, 2025 **Current Version:** 2.5.34 **Next
Release:** TBA

[← Back to Documentation Index](../README.md) | [↑ Top](#changelog)
