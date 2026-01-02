# Archive

**Historical Documentation Repository**

[Home](../../README.md) > [Documentation](../README.md) > Archive

---

## ⚠️ Important Notice

This directory contains **archived documentation** from previous firmware
versions and development phases. These documents are kept for historical
reference but **are no longer applicable** to current firmware versions
(v2.3.0+).

**For current documentation:** See [Documentation Index](../README.md)

---

## Overview

This archive contains documents from multiple development phases:

- **v2.0.0 development** (October 2024): Streaming data fix implementation
- **v2.3.x development** (November 2025): MQTT payload fixes, bug analysis, and
  refactoring documentation

---

## Archived Documents

### Bug Fixes & Refactoring (November 2025)

#### MQTT Payload Issues (Archived November 22, 2025)

| Document                                                         | Archived Date | Status            | Reason                                         |
| ---------------------------------------------------------------- | ------------- | ----------------- | ---------------------------------------------- |
| [MQTT_PAYLOAD_CORRUPTION_FIX.md](MQTT_PAYLOAD_CORRUPTION_FIX.md) | Nov 22, 2025  | ✅ **INTEGRATED** | Memory overlap fix in MqttManager.cpp (v2.3.2) |
| [MQTT_SUBSCRIBER_FIX.md](MQTT_SUBSCRIBER_FIX.md)                 | Nov 22, 2025  | ✅ **INTEGRATED** | Binary publish with explicit length (v2.3.2)   |
| [UTF8_CHARACTER_ENCODING_FIX.md](UTF8_CHARACTER_ENCODING_FIX.md) | Nov 22, 2025  | ✅ **INTEGRATED** | ASCII-only units (v2.3.2)                      |

**Current Alternative:** Fixes integrated in
[MqttManager.cpp](../../Main/MqttManager.cpp) lines 844-868

**Details:**

- **MQTT_PAYLOAD_CORRUPTION_FIX.md**: Documented topic/payload memory overlap
  causing corruption at position 2005. Fixed with separate buffers
  (topicBuffer + payloadBuffer).
- **MQTT_SUBSCRIBER_FIX.md**: Documented text publish vs binary publish issue.
  Fixed by using 3-parameter publish() with explicit length.
- **UTF8_CHARACTER_ENCODING_FIX.md**: Documented UTF-8 multi-byte character (°C)
  causing buffer overflow. Fixed by using ASCII-only units (degC).

---

#### Refactoring Documentation (Archived November 22, 2025)

| Document                                                         | Archived Date | Status          | Reason                                    |
| ---------------------------------------------------------------- | ------------- | --------------- | ----------------------------------------- |
| [FINAL_REFACTORING_SUMMARY.md](FINAL_REFACTORING_SUMMARY.md)     | Nov 22, 2025  | ⚠️ **OUTDATED** | v2.0.0 refactoring summary (Nov 20, 2025) |
| [FIRMWARE_REFACTORING_REPORT.md](FIRMWARE_REFACTORING_REPORT.md) | Nov 22, 2025  | ⚠️ **OUTDATED** | v2.0.0 refactoring report (Nov 20, 2025)  |

**Current Alternative:** See
[VERSION_HISTORY.md](../Changelog/VERSION_HISTORY.md) for current version
details

**Details:**

- Documented 26 bugs/issues identified in v2.0.0
- 21 of 26 bugs fixed (81% completion)
- All CRITICAL and MODERATE bugs resolved
- Historical reference for refactoring process

---

#### Bug Analysis (v2.0.0)

| Document                           | Archived Date | Status          | Reason                                     |
| ---------------------------------- | ------------- | --------------- | ------------------------------------------ |
| [BUG_ANALYSIS.md](BUG_ANALYSIS.md) | Nov 22, 2025  | ⚠️ **OUTDATED** | v2.0.0 bug analysis (most issues resolved) |

**Current Alternative:** See
[TROUBLESHOOTING.md](../Technical_Guides/TROUBLESHOOTING.md) and
[BUG_STATUS_REPORT.md](../Changelog/BUG_STATUS_REPORT.md)

**Details:**

- Analysis of bugs discovered during v2.0.0 development
- Most issues resolved in subsequent releases (v2.1.x - v2.3.x)
- Kept for historical context

---

### Streaming Fix Documentation (October 2024)

| Document                                           | Archived Date | Status      |
| -------------------------------------------------- | ------------- | ----------- |
| [STREAMING_FIX_GUIDE.md](STREAMING_FIX_GUIDE.md)   | October 2024  | ⚠️ Archived |
| [README_STREAMING_FIX.md](README_STREAMING_FIX.md) | October 2024  | ⚠️ Archived |
| [INTEGRATION_STEPS.md](INTEGRATION_STEPS.md)       | October 2024  | ⚠️ Archived |
| [COMPARISON_MATRIX.md](COMPARISON_MATRIX.md)       | October 2024  | ⚠️ Archived |
| [DELIVERY_SUMMARY.md](DELIVERY_SUMMARY.md)         | October 2024  | ⚠️ Archived |

### Data Type Documentation

| Document                                                     | Archived Date | Status      |
| ------------------------------------------------------------ | ------------- | ----------- |
| [Modbus Data Types List.md](Modbus%20Data%20Types%20List.md) | October 2024  | ⚠️ Archived |

**Current Version:** See
[MODBUS_DATATYPES.md](../Technical_Guides/MODBUS_DATATYPES.md)

---

## Why These Are Archived

### MQTT Payload Fixes (November 2025)

**Problem:** v2.3.2 had critical MQTT payload issues:

1. **Memory Corruption**: Topic name leaked into payload at position ~2005
2. **No Subscriber Data**: Text publish failed for large payloads (>2KB)
3. **UTF-8 Buffer Overflow**: Multi-byte characters caused size mismatch

**Solution:** Three critical fixes implemented in MqttManager.cpp:

1. Separate buffers for topic and payload (prevents memory overlap)
2. Binary publish with explicit length (prevents strlen() truncation)
3. ASCII-only units (prevents UTF-8 size calculation errors)

**Status:** ✅ **RESOLVED** - Fixes integrated into v2.3.2 stable release

**Result:** These documents documented the problems and solutions during
development. Since fixes are now integrated, these guides are archived for
reference.

---

### Refactoring & Bug Analysis (November 2025)

**Context:** v2.0.0 refactoring effort (Nov 20, 2025) addressed 26 identified
bugs/issues.

**Achievement:** 21 of 26 bugs fixed (81% completion, 100% of CRITICAL +
MODERATE bugs)

**Status:** ⚠️ **OUTDATED** - Refactoring completed, documented in
VERSION_HISTORY.md

**Result:** These comprehensive reports documented the refactoring process. Now
that work is complete and integrated, they serve as historical reference only.

---

### Streaming Fix (October 2024)

**Problem:** v2.0.0 had a critical streaming data format mismatch issue that
prevented data delivery to the Flutter mobile app (0% delivery rate).

**Solution:** The development team implemented a comprehensive fix that
unwrapped nested data structures and added detailed logging.

**Status:** ✅ **RESOLVED** - Fix integrated into v2.0.0 stable release

**Result:** These documents documented the problem, analysis, and solution
during development. Since the fix is now integrated, these guides are no longer
needed for current development.

### Data Types Documentation

**Problem:** Original data types documentation was incomplete and lacked clear
organization.

**Solution:** Comprehensive data types documentation created with 40+ types,
endianness explanations, and manufacturer-specific configurations.

**Status:** ✅ **REPLACED** - See
[MODBUS_DATATYPES.md](../Technical_Guides/MODBUS_DATATYPES.md)

---

## What's in Each Document

### [STREAMING_FIX_GUIDE.md](STREAMING_FIX_GUIDE.md)

- Root cause analysis of streaming issue
- Detailed code changes required
- Logging implementation guide
- Verification steps

**Current Alternative:** [PROTOCOL.md](../Technical_Guides/PROTOCOL.md) and
[TROUBLESHOOTING.md](../Technical_Guides/TROUBLESHOOTING.md)

### [README_STREAMING_FIX.md](README_STREAMING_FIX.md)

- Executive summary of streaming fix
- System architecture analysis
- Production-grade implementation details
- Comprehensive testing procedures

**Current Alternative:** [PROTOCOL.md](../Technical_Guides/PROTOCOL.md)

### [INTEGRATION_STEPS.md](INTEGRATION_STEPS.md)

- Step-by-step integration guide
- Code replacement instructions
- Testing and validation steps

**Current Alternative:** [API Reference](../API_Reference/API.md) for current
integration

### [COMPARISON_MATRIX.md](COMPARISON_MATRIX.md)

- Before/after comparison of fix
- Performance metrics
- Data delivery statistics

**Current Alternative:** [VERSION_HISTORY.md](../Changelog/VERSION_HISTORY.md)
for performance info

### [DELIVERY_SUMMARY.md](DELIVERY_SUMMARY.md)

- Project completion report
- Delivery checklist
- Acceptance criteria

**Current Alternative:** [VERSION_HISTORY.md](../Changelog/VERSION_HISTORY.md)

### [Modbus Data Types List.md](Modbus%20Data%20Types%20List.md)

- Original data types documentation (v2.0.0)
- Basic data type list
- Limited examples

**Current Alternative:**
[MODBUS_DATATYPES.md](../Technical_Guides/MODBUS_DATATYPES.md) - Comprehensive
with 40+ types

---

### November 2025 Archived Documents

### [MQTT_PAYLOAD_CORRUPTION_FIX.md](MQTT_PAYLOAD_CORRUPTION_FIX.md)

- Root cause: Memory overlap between topic and payload buffers
- Symptoms: Corruption at position ~2005 bytes
- Solution: Separate buffers (topicBuffer + heap_caps_malloc for payload)
- Status: ✅ **INTEGRATED** in MqttManager.cpp lines 844-868

**Why Archived:** Fix fully integrated in v2.3.2 firmware

### [MQTT_SUBSCRIBER_FIX.md](MQTT_SUBSCRIBER_FIX.md)

- Root cause: Text publish (2-param) uses strlen() which stops at NULL bytes
- Symptoms: Subscriber receives no payload despite publish SUCCESS
- Solution: Binary publish (3-param) with explicit length
- Status: ✅ **INTEGRATED** in MqttManager.cpp

**Why Archived:** Fix fully integrated in v2.3.2 firmware

### [UTF8_CHARACTER_ENCODING_FIX.md](UTF8_CHARACTER_ENCODING_FIX.md)

- Root cause: Multi-byte UTF-8 character (°C = 3 bytes) causes size mismatch
- Symptoms: Buffer overflow at position ~2005
- Solution: Use ASCII-only characters (degC instead of °C)
- Status: ✅ **INTEGRATED** in device creation scripts

**Why Archived:** Fix fully integrated and documented in best practices

### [BUG_ANALYSIS.md](BUG_ANALYSIS.md)

- v2.0.0 bug tracking and analysis
- Comprehensive list of issues discovered
- Most issues resolved in v2.1.x - v2.3.x releases
- Status: ⚠️ **OUTDATED** - kept for historical reference

**Current Alternative:**
[BUG_STATUS_REPORT.md](../Changelog/BUG_STATUS_REPORT.md) for current bug
tracking

### [FINAL_REFACTORING_SUMMARY.md](FINAL_REFACTORING_SUMMARY.md)

- Executive summary of v2.0.0 refactoring (Nov 20, 2025)
- 21 of 26 bugs fixed (81% completion)
- Code quality metrics and improvements
- Status: ⚠️ **OUTDATED** - historical reference

**Current Alternative:** [VERSION_HISTORY.md](../Changelog/VERSION_HISTORY.md)

### [FIRMWARE_REFACTORING_REPORT.md](FIRMWARE_REFACTORING_REPORT.md)

- Detailed refactoring report for v2.0.0
- CRITICAL bugs fixed (7/10)
- Implementation details for each fix
- Status: ⚠️ **OUTDATED** - historical reference

**Current Alternative:** [VERSION_HISTORY.md](../Changelog/VERSION_HISTORY.md)

---

## Current Documentation

### For MQTT Issues

- **[MQTT Publish Modes](../Technical_Guides/MQTT_PUBLISH_MODES_DOCUMENTATION.md)** -
  Complete MQTT configuration
- **[Troubleshooting Guide](../Technical_Guides/TROUBLESHOOTING.md)** - MQTT
  troubleshooting section
- **[MqttManager.cpp](../../Main/MqttManager.cpp)** - Source code with
  integrated fixes (lines 844-868)

### For Bug Tracking

- **[BUG_STATUS_REPORT.md](../Changelog/BUG_STATUS_REPORT.md)** - Current bug
  tracking and analysis
- **[VERSION_HISTORY.md](../Changelog/VERSION_HISTORY.md)** - Complete bug fix
  history

### For Streaming & Data Format Issues

- **[Protocol Documentation](../Technical_Guides/PROTOCOL.md)** - All protocol
  specifications
- **[API Reference](../API_Reference/API.md)** - Current BLE API format
- **[Troubleshooting Guide](../Technical_Guides/TROUBLESHOOTING.md)** - Issue
  resolution

### For Data Types

- **[MODBUS_DATATYPES.md](../Technical_Guides/MODBUS_DATATYPES.md)** - Complete
  data type reference

### For Version Information

- **[VERSION_HISTORY.md](../Changelog/VERSION_HISTORY.md)** - Release notes and
  changes

### For Integration

- **[Quick Start Guide](../QUICKSTART.md)** - Get started in 5 minutes
- **[Best Practices](../BEST_PRACTICES.md)** - Production deployment guidelines

---

## Should I Read These Archived Documents?

**Most users: NO** - Use current documentation instead

**You might want to read these if:**

- You're maintaining v2.0.0 firmware specifically
- You're interested in the development history
- You're researching how the streaming issue was resolved
- You need historical context for a specific feature

**For all current development:** Use the [current documentation](../README.md)
instead.

---

## Archive Policy

Documents are archived when:

1. The feature/fix is fully integrated into stable firmware
2. Replacement documentation exists and is comprehensive
3. The document is no longer applicable to current versions
4. The document is kept only for historical reference

**Retention:** Archived documents are retained indefinitely for historical
reference.

---

## Related Documentation

- **[Documentation Index](../README.md)** - Main documentation hub
- **[Version History](../Changelog/VERSION_HISTORY.md)** - Current release notes
- **[Troubleshooting Guide](../Technical_Guides/TROUBLESHOOTING.md)** - Problem
  solving

---

**Archive Created:** October 2024 **Last Updated:** November 22, 2025 **Latest
Additions:** November 22, 2025 (MQTT fixes, refactoring docs, bug analysis)
**Archived From:** v2.0.0 development phase (Oct 2024) + v2.3.x development
(Nov 2025) **Current Firmware:** v2.3.3

[← Back to Documentation Index](../README.md) | [↑ Top](#archive)
