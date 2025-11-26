# Expert Firmware Prompting Guide

**Advanced Prompting Strategies for SRT-MGATE-1210 Gateway Development**

**Version:** 2.0.0
**Last Updated:** November 24, 2025
**Project:** SRT-MGATE-1210 Industrial IoT Gateway
**Author:** SURIOTA R&D Team

---

## 📋 Table of Contents

1. [Prompting Fundamentals](#prompting-fundamentals)
2. [MCP-Enhanced Prompting](#mcp-enhanced-prompting)
3. [Development Phase Prompts](#development-phase-prompts)
4. [Debugging & Troubleshooting Prompts](#debugging--troubleshooting-prompts)
5. [Optimization Prompts](#optimization-prompts)
6. [Testing & Validation Prompts](#testing--validation-prompts)
7. [Production Readiness Prompts](#production-readiness-prompts)
8. [Security & Compliance Prompts](#security--compliance-prompts)
9. [Documentation Prompts](#documentation-prompts)
10. [Advanced Patterns](#advanced-patterns)

---

## 🎯 Prompting Fundamentals

### The Perfect Prompt Structure

```
[CONTEXT] + [GOAL] + [CONSTRAINTS] + [MCP TOOLS] + [OUTPUT FORMAT]
```

#### ❌ Bad Prompt

```
Fix the bug
```

**Problems:**
- No context
- Vague goal
- No constraints
- No tool specification
- No expected output

#### ✅ Good Prompt

```
I'm developing the SRT-MGATE-1210 Industrial IoT Gateway on ESP32-S3.
The gateway stops publishing MQTT messages after 24 hours of continuous operation.

CONTEXT:
- Firmware version: 2.3.3
- Using PubSubClient library for MQTT
- Network: Ethernet primary, WiFi fallback
- Memory: PSRAM enabled, MemoryRecovery active

GOAL:
Diagnose and fix the MQTT disconnection issue.

CONSTRAINTS:
- Must maintain backward compatibility with existing config
- Cannot exceed current memory budget (DRAM < 300KB)
- Must follow existing code patterns in MqttManager.cpp

TOOLS:
1. use context7 for latest PubSubClient documentation
2. Build and upload firmware for testing
3. Monitor serial logs for at least 5 minutes

OUTPUT FORMAT:
1. Root cause analysis
2. Proposed fix with code changes
3. Testing plan to verify fix
4. Prevention strategy for future
```

**Result:** Comprehensive solution with clear action plan

---

## 🔌 MCP-Enhanced Prompting

### Pattern 1: Research → Implement → Test

**Template:**
```
RESEARCH (Context7):
Using [library name] version [X.Y.Z], show me [specific feature].
use context7

IMPLEMENT:
Based on the documentation above, implement [feature] in [file name]
following these requirements:
- [Requirement 1]
- [Requirement 2]
- [Requirement 3]

TEST (Arduino MCP):
Build the firmware, upload to ESP32-S3, and monitor serial output for [duration].
Verify [expected behavior].

VALIDATE (Playwright - if applicable):
Open [URL] and verify [condition].
```

**Real Example:**

```
RESEARCH (Context7):
Using ArduinoJson 7.4.2, show me the best practices for:
- Parsing streaming JSON from MQTT payloads
- Using PSRAM for large documents (>50KB)
- Handling nested objects efficiently
- Error handling for malformed JSON
use context7

IMPLEMENT:
Based on the documentation, optimize MqttManager::parseIncomingMessage()
to handle large JSON payloads efficiently:
- Use SpiRamJsonDocument instead of JsonDocument
- Implement streaming deserialization for MQTT chunks
- Add error recovery for malformed JSON
- Log memory usage before/after parsing
- Follow existing patterns in Main/MqttManager.cpp

TEST (Arduino MCP):
Build the firmware with verbose logging, upload to ESP32-S3, and monitor
serial output for 10 minutes. Send test MQTT messages with:
- Small payload (1KB)
- Medium payload (50KB)
- Large payload (100KB)
- Malformed JSON

Verify:
- All payloads parsed successfully
- PSRAM usage stays below 1MB
- No memory leaks detected
- Error handling works for malformed JSON

VALIDATE:
Check MQTT broker web UI to confirm messages are being processed correctly.
```

⏱️ **Time Saved:** 3-4 hours → 45 minutes

---

### Pattern 2: Multi-MCP Orchestration

**Template:**
```
This task requires coordination of multiple MCP servers:

PHASE 1: Documentation Research (Context7)
→ [Research query] use context7

PHASE 2: Code Implementation (Claude + Arduino MCP)
→ Implement [feature] based on research
→ Build and verify compilation

PHASE 3: Hardware Testing (Arduino MCP)
→ Upload firmware to ESP32-S3
→ Monitor serial output for [duration]
→ Capture logs for analysis

PHASE 4: Web Validation (Playwright)
→ Test [web interface]
→ Verify [expected behavior]
→ Take screenshots for documentation

PHASE 5: Integration Verification (All MCPs)
→ End-to-end test of complete workflow
→ Document results
```

**Real Example:**

```
Task: Implement and test BLE-based firmware update over the air

PHASE 1: Documentation Research (Context7)
Research the latest ESP32 BLE OTA update patterns:
- ESP32 Arduino BLE library OTA APIs
- Partition scheme for OTA (app0/app1)
- BLE MTU optimization for file transfer
- Error recovery and rollback mechanisms
use context7

PHASE 2: Code Implementation (Claude + Arduino MCP)
Based on research, implement OTA update system:
- Create OTAManager.h/cpp following singleton pattern
- Implement BLE characteristic for firmware chunks (MTU-optimized)
- Add partition management (esp_ota APIs)
- Implement rollback on failure
- Add progress reporting via BLE notifications
- Update BLEManager to include OTA characteristic
- Follow memory patterns (PSRAM allocation with fallback)

Build firmware and verify compilation succeeds.

PHASE 3: Hardware Testing (Arduino MCP)
Upload firmware to ESP32-S3 and test OTA workflow:
1. Connect via BLE
2. Initiate OTA update
3. Transfer firmware in chunks
4. Monitor serial logs for:
   - Chunk reception progress
   - Partition writes
   - Verification status
   - Reboot and activation
5. Verify rollback works on corrupted firmware

PHASE 4: Web Validation (Playwright)
If web interface exists for OTA management:
- Navigate to http://192.168.1.100/ota
- Upload firmware file
- Monitor progress bar
- Verify completion message
- Take screenshot of successful update

PHASE 5: Integration Verification
End-to-end test:
1. Create test firmware with version bump
2. Upload via BLE OTA
3. Verify new version boots successfully
4. Test rollback by uploading corrupted firmware
5. Verify system rolls back to previous version
6. Document entire workflow with logs and screenshots

Expected deliverables:
- OTAManager.h/cpp implementation
- BLE OTA characteristic added
- Serial logs of successful OTA
- Rollback test results
- Documentation update in API.md
```

⏱️ **Time Saved:** 2-3 days → 4-6 hours

---

### Pattern 3: Context7 Version-Specific Queries

**Template:**
```
Using [Library Name] version [X.Y.Z] (exact version from our dependencies),
show me [specific API/feature] including:
- Current API signature
- Breaking changes from previous versions
- Memory optimization tips
- Code examples for ESP32-S3
- Common pitfalls to avoid
use context7
```

**Real Examples:**

```
Example 1: ArduinoJson Version-Specific
Using ArduinoJson version 7.4.2 (as specified in our LIBRARIES.md),
show me the latest API for:
- SpiRamJsonDocument allocation and sizing
- Streaming JSON deserialization from Serial/Network
- Efficient nested object access (config["mqtt"]["broker"])
- measureJson() for optimal document sizing
- Common memory pitfalls on ESP32-S3 with PSRAM
use context7
```

```
Example 2: PubSubClient MQTT
Using PubSubClient version 2.12.1,
show me the complete API for:
- QoS 2 message publishing
- Persistent session configuration
- Keep-alive timeout tuning for long-running connections
- Reconnection best practices after network loss
- Buffer size configuration for large payloads
use context7
```

```
Example 3: ModbusMaster
Using ModbusMaster version 2.0.1,
show me how to:
- Handle all Modbus exception codes
- Configure timeout for slow slave devices
- Manage multiple RS485 ports simultaneously
- Implement exponential backoff for failed reads
- Parse floating point values (FLOAT32_ABCD format)
use context7
```

---

### Pattern 4: Playwright Testing Workflows

**Template:**
```
Test [web interface/dashboard] using Playwright:

SETUP:
- Navigate to [URL]
- Wait for [element] to load
- Verify initial state

TEST SCENARIO:
1. [Action 1]
2. [Action 2]
3. [Action 3]

VERIFICATION:
- Check [condition 1]
- Verify [condition 2]
- Assert [condition 3]

OUTPUT:
- Screenshot of final state
- Extracted data: [what to extract]
- Pass/fail status
```

**Real Examples:**

```
Example 1: MQTT Broker Monitoring
Test HiveMQ broker web interface to verify Gateway is publishing:

SETUP:
- Open http://broker.hivemq.com:8080
- Wait for dashboard to load
- Navigate to "Topics" section

TEST SCENARIO:
1. Search for topic "suriota/gateway/data"
2. View latest message
3. Extract message timestamp
4. Parse message payload (JSON)

VERIFICATION:
- Topic exists and is active
- Last message timestamp is recent (< 60 seconds ago)
- Payload is valid JSON with required fields:
  - device_id
  - timestamp
  - data_type
  - value
- Message rate is consistent with configured interval (5 seconds)

OUTPUT:
- Screenshot of topic view
- Extracted data: {timestamp, payload, message_count}
- Pass/fail status with details
```

```
Example 2: Gateway Web Dashboard Testing
Test Gateway status dashboard for UI regression:

SETUP:
- Navigate to http://192.168.1.100/dashboard
- Wait for "Dashboard" header to appear
- Verify page loads without errors

TEST SCENARIO:
1. Check "Network Status" indicator
2. Verify "MQTT Connection" shows "Connected"
3. Extract "Devices Online" count
4. Click on first device in list
5. Verify device details modal opens
6. Check "Last Poll" timestamp
7. Close modal

VERIFICATION:
- Network status is "Connected" (green indicator)
- MQTT shows connected with broker address
- At least 1 device is online
- Device details show valid data:
  - Device name matches config
  - Last poll time is recent (< refresh rate)
  - Register values are numeric
- No JavaScript errors in console

OUTPUT:
- Screenshot of dashboard overview
- Screenshot of device details modal
- Extracted metrics: {network_status, mqtt_status, devices_online, last_poll_time}
- Console log errors (if any)
- Pass/fail status
```

```
Example 3: Modbus Simulator Configuration
Setup Modbus TCP simulator for Gateway testing:

SETUP:
- Open http://localhost:8080/modbus-sim
- Wait for simulator interface to load

TEST SCENARIO:
1. Configure slave ID = 1
2. Set holding register 0 = 2500 (temperature * 100)
3. Set holding register 1 = 7500 (humidity * 100)
4. Set holding register 2 = 101325 (pressure in Pa)
5. Enable "Auto Response" mode
6. Click "Start Simulator"

VERIFICATION:
- Slave ID set correctly
- All registers show configured values
- Simulator status shows "Running"
- Response mode is "Auto"

OUTPUT:
- Screenshot of configured simulator
- Confirmation that simulator is ready for Gateway testing
```

---

## 🏗️ Development Phase Prompts

### Phase 1: New Feature Development

#### Template: Add New Feature

```
FEATURE: [Feature name and description]

CONTEXT:
- Current firmware version: [version]
- Affected modules: [list modules]
- Dependencies: [list dependencies]

REQUIREMENTS:
1. [Requirement 1]
2. [Requirement 2]
3. [Requirement 3]

IMPLEMENTATION PLAN:
1. Research latest API/best practices (use context7)
2. Design implementation following existing patterns
3. Implement feature with error handling
4. Add unit tests (if applicable)
5. Update documentation
6. Build and test on hardware

CONSTRAINTS:
- Memory budget: [DRAM/PSRAM limits]
- Performance: [timing requirements]
- Compatibility: [backward compatibility needs]

SUCCESS CRITERIA:
- [Criterion 1]
- [Criterion 2]
- [Criterion 3]
```

#### Real Example: Device Health Monitoring

```
FEATURE: Add device health monitoring with automatic recovery

CONTEXT:
- Current firmware version: 2.3.3
- Affected modules: ModbusRtuService, ModbusTcpService, HealthMonitor (new)
- Dependencies: None (uses existing ErrorHandler and MemoryRecovery)

REQUIREMENTS:
1. Track device health metrics (success rate, avg response time, timeouts)
2. Automatically disable devices with < 20% success rate
3. Auto-recovery: re-enable disabled devices every 5 minutes
4. Expose health metrics via BLE (new command)
5. Log health status changes to SD card

IMPLEMENTATION PLAN:

Step 1: Research Best Practices (Context7)
"Show me industrial IoT device health monitoring patterns:
- Success rate calculation with sliding windows
- Response time tracking (avg, min, max)
- Auto-disable/recovery algorithms
- Alarm thresholds and hysteresis
use context7"

Step 2: Design Implementation
Create HealthMonitor class following singleton pattern:
- Location: Main/HealthMonitor.h/cpp
- Features:
  - Per-device health metrics structure
  - Sliding window (last 100 polls) for success rate
  - Response time tracking (EWMA algorithm)
  - Configurable thresholds (success rate, timeout count)
  - Auto-recovery timer (5 min default)

Step 3: Integrate with Modbus Services
Update ModbusRtuService and ModbusTcpService:
- Call HealthMonitor::recordSuccess() on successful poll
- Call HealthMonitor::recordFailure() on failed poll
- Check HealthMonitor::isDeviceEnabled() before polling
- Respect auto-disable decisions

Step 4: Add BLE Command
Update CRUDHandler with new operation:
- Command: {"op": "read", "type": "device_health", "device_id": "D7A3F2"}
- Response includes: {success_rate, avg_response_ms, timeout_count, status, disable_reason}

Step 5: Implement SD Logging
Add health event logging:
- Log to: /logs/health_YYYYMMDD.log
- Events: DEVICE_DISABLED, DEVICE_RECOVERED, THRESHOLD_EXCEEDED
- Format: [YYYY-MM-DD HH:MM:SS][HEALTH][device_id] Event: reason

Step 6: Build and Test
1. Build firmware with verbose health logging
2. Upload to ESP32-S3
3. Test scenarios:
   - Normal operation (high success rate)
   - Simulated failures (disconnect Modbus device)
   - Auto-disable triggered (success rate < 20%)
   - Auto-recovery after 5 minutes
4. Verify BLE health command returns correct metrics
5. Check SD card logs for health events

CONSTRAINTS:
- Memory budget:
  - DRAM: < 10KB per device (metrics storage)
  - PSRAM: Use for historical data (optional)
- Performance:
  - Health check must complete in < 1ms
  - No blocking operations in record functions
- Compatibility:
  - Must work with existing device configurations
  - No breaking changes to BLE API

SUCCESS CRITERIA:
- Devices with < 20% success rate auto-disable within 1 minute
- Disabled devices auto-recover after 5 minutes
- BLE health command returns accurate metrics
- SD logs capture all health events
- No performance degradation in polling loop
- Memory usage within budget
```

---

### Phase 2: Bug Fixing & Debugging

#### Template: Debug Production Issue

```
ISSUE: [Concise problem description]

SYMPTOMS:
- [Symptom 1]
- [Symptom 2]
- [Symptom 3]

REPRODUCTION STEPS:
1. [Step 1]
2. [Step 2]
3. [Step 3]

ENVIRONMENT:
- Firmware version: [version]
- Hardware: [board/peripherals]
- Network: [WiFi/Ethernet]
- Uptime when issue occurs: [duration]

LOGS/ERROR MESSAGES:
```
[Paste relevant logs here]
```

INVESTIGATION PLAN:
1. Research known issues (use context7 for library docs)
2. Analyze logs for patterns
3. Identify root cause
4. Propose fix with minimal risk
5. Test fix thoroughly
6. Implement prevention strategy

CONSTRAINTS:
- Must fix without firmware update (if possible)
- Minimize downtime
- Preserve existing configuration
```

#### Real Example: Memory Leak Detection

```
ISSUE: Gateway crashes after 48-72 hours of continuous operation

SYMPTOMS:
- Free DRAM steadily decreases over time
- MemoryRecovery warnings increase in frequency
- Eventually system crashes with "Out of memory" error
- Crash occurs during ConfigManager::loadDevices() call

REPRODUCTION STEPS:
1. Flash firmware version 2.3.3
2. Configure 50 Modbus RTU devices
3. Enable MQTT publishing (5 second interval)
4. Run continuously for 48+ hours
5. Monitor memory usage via serial logs

ENVIRONMENT:
- Firmware version: 2.3.3
- Hardware: ESP32-S3 DevKit, 8MB PSRAM, 16MB Flash
- Network: Ethernet primary (stable connection)
- Uptime when issue occurs: 48-72 hours
- Configuration: 50 RTU devices, 200+ registers total

LOGS/ERROR MESSAGES:
```
[2025-11-24 10:15:22][WARN][MEM] DRAM free: 45230 bytes (threshold: 30000)
[2025-11-24 10:20:22][WARN][MEM] DRAM free: 42105 bytes (threshold: 30000)
[2025-11-24 10:25:22][ERROR][MEM] CRITICAL: DRAM free: 28450 bytes
[2025-11-24 10:30:22][ERROR][MEM] EMERGENCY: DRAM free: 15200 bytes
[2025-11-24 10:30:45][ERROR][CONFIG] Failed to allocate JsonDocument (size: 8192)
[2025-11-24 10:30:45][SYSTEM] Guru Meditation Error: Core 1 panic'ed (LoadProhibited)
```

INVESTIGATION PLAN:

Step 1: Research String Memory Leaks (Context7)
"In Arduino/ESP32 development, what are common causes of memory leaks with:
- String class concatenation
- JsonDocument allocation/deallocation
- std::map and std::vector growth
- Task stack overflow
Show ESP32-specific memory debugging techniques.
use context7"

Step 2: Analyze Code for Memory Leaks
Review potential leak sources:
- ConfigManager::loadDevices() - JSON document allocation
- String concatenations in log messages
- MQTT payload building (String operations)
- Device cache in std::map (unbounded growth?)
- CRUDHandler JSON response building

Add memory tracking:
- Log heap usage before/after each major operation
- Track largest memory allocation
- Monitor stack high water marks for all tasks

Step 3: Implement Memory Profiling Build
Create debug build with extensive memory logging:
```cpp
// Add to main loop
unsigned long lastMemLog = 0;
if (millis() - lastMemLog > 60000) {  // Every minute
    LOG_MEM_DEBUG("=== MEMORY PROFILE ===");
    LOG_MEM_DEBUG("Free DRAM: %d bytes", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    LOG_MEM_DEBUG("Free PSRAM: %d bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    LOG_MEM_DEBUG("Largest DRAM block: %d bytes",
                  heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    // Per-task stack usage
    UBaseType_t rtuStack = uxTaskGetStackHighWaterMark(rtuTaskHandle);
    UBaseType_t mqttStack = uxTaskGetStackHighWaterMark(mqttTaskHandle);
    LOG_MEM_DEBUG("RTU task stack remaining: %d bytes", rtuStack);
    LOG_MEM_DEBUG("MQTT task stack remaining: %d bytes", mqttStack);

    lastMemLog = millis();
}
```

Step 4: Build and Monitor
Build debug firmware and run for 24 hours with memory profiling:
- Monitor serial output
- Look for decreasing free DRAM trend
- Identify which component allocates most memory
- Check for stack overflow (stack remaining < 500 bytes)

Step 5: Root Cause Identification
Based on logs, identify leak source:
- If DRAM decreases steadily: likely String concatenation or unbounded containers
- If sudden drops: large allocations not being freed
- If stack-related: task stack too small or recursive calls

Step 6: Implement Fix
Common fixes:
- Replace String concatenation with snprintf/char buffers
- Limit std::map/vector size with cache eviction
- Free JsonDocuments immediately after use
- Increase task stack sizes if needed
- Use PSRAM for large temporary allocations

Step 7: Verify Fix
Test fixed firmware for 96 hours:
- Memory usage should remain stable
- No MemoryRecovery warnings
- No crashes
- Performance unchanged

Step 8: Prevention Strategy
Add runtime protections:
- Cache size limits in ConfigManager
- Periodic cache clearing (every 1 hour)
- Stack overflow detection
- Memory leak detection in CI/CD (if applicable)

CONSTRAINTS:
- Must preserve all existing functionality
- Cannot change wire protocol (BLE/MQTT/Modbus)
- Fix must be backward compatible with existing configs
- Minimal performance impact

EXPECTED OUTCOME:
- Stable memory usage over 96+ hours
- Free DRAM remains above 100KB
- No memory-related crashes
- Root cause documented for future reference
```

---

### Phase 3: Code Optimization

#### Template: Performance Optimization

```
OPTIMIZATION TARGET: [What to optimize]

CURRENT PERFORMANCE:
- Metric 1: [current value]
- Metric 2: [current value]
- Metric 3: [current value]

TARGET PERFORMANCE:
- Metric 1: [target value]
- Metric 2: [target value]
- Metric 3: [target value]

PROFILING PLAN:
1. Measure baseline performance
2. Identify bottlenecks
3. Research optimization techniques (use context7)
4. Implement optimizations
5. Benchmark improvements
6. Validate no regressions

CONSTRAINTS:
- Cannot break existing functionality
- Must maintain code readability
- Memory budget: [limits]
```

#### Real Example: Modbus Polling Optimization

```
OPTIMIZATION TARGET: Reduce Modbus RTU polling latency for 50 devices

CURRENT PERFORMANCE:
- Total poll cycle time: 25 seconds (50 devices × ~500ms avg)
- Device refresh rate: 1 reading per 25 seconds per device
- CPU usage during polling: 35%
- Timeout rate: 5% (2-3 devices timeout per cycle)

TARGET PERFORMANCE:
- Total poll cycle time: ≤ 10 seconds (50 devices)
- Device refresh rate: 1 reading per 10 seconds per device
- CPU usage during polling: < 40%
- Timeout rate: < 2%

PROFILING PLAN:

Step 1: Baseline Measurement
Build firmware with timing instrumentation:
```cpp
void ModbusRtuService::pollDevice(Device &device) {
    unsigned long start = micros();

    // Existing polling code

    unsigned long elapsed = micros() - start;
    LOG_RTU_DEBUG("Device %s poll time: %lu us",
                  device.device_id.c_str(), elapsed);
}
```

Upload and measure:
- Individual device poll times
- Identify slowest devices
- Measure time between baudrate switches
- Track timeout occurrences

Step 2: Identify Bottlenecks (use context7)
"Show me Modbus RTU performance optimization techniques:
- Timeout tuning for industrial environments
- Baudrate switching optimization
- Inter-frame delays and timing
- Multiple register read strategies
- RS485 transceiver settling time
use context7"

Step 3: Analyze Current Implementation
Review bottlenecks:
- Timeout too conservative (3000ms)?
- Baudrate switching delay (100ms) adds up over 50 devices
- Sequential polling (no parallelization)
- Reading registers one by one vs. batch reads
- Delay between polls unnecessary?

Step 4: Implement Optimizations

Optimization 1: Adaptive Timeout
```cpp
// Instead of fixed 3000ms timeout:
// Fast devices: 500ms
// Normal devices: 1000ms
// Slow devices: 2000ms (learned from history)

struct DeviceTiming {
    uint32_t avgResponseTime;  // EWMA
    uint32_t adaptiveTimeout;  // avgResponseTime * 3
};

// Update timeout based on response time
if (success) {
    device.timing.avgResponseTime =
        (device.timing.avgResponseTime * 0.8) + (responseTime * 0.2);
    device.timing.adaptiveTimeout =
        min(device.timing.avgResponseTime * 3, 2000);
}
```

Optimization 2: Minimize Baudrate Switching
```cpp
// Group devices by baudrate
// Poll all 9600 baud devices, then all 19200 baud devices, etc.

void ModbusRtuService::pollAllDevices() {
    // Sort devices by baudrate
    std::sort(devices.begin(), devices.end(),
              [](const Device &a, const Device &b) {
                  return a.baud_rate < b.baud_rate;
              });

    uint32_t currentBaud = 0;
    for (auto &device : devices) {
        if (device.baud_rate != currentBaud) {
            Serial1.updateBaudRate(device.baud_rate);
            currentBaud = device.baud_rate;
            vTaskDelay(pdMS_TO_TICKS(50));  // Reduced from 100ms
        }
        pollDevice(device);
    }
}
```

Optimization 3: Batch Register Reads
```cpp
// Instead of reading each register individually,
// read all consecutive registers in one request

// Before: 10 registers = 10 Modbus requests
// After: 10 consecutive registers = 1 Modbus request

std::vector<RegisterRange> groupConsecutiveRegisters(std::vector<Register> &regs) {
    std::vector<RegisterRange> ranges;
    // Group consecutive addresses
    // ...
    return ranges;
}
```

Optimization 4: Reduce Inter-Poll Delay
```cpp
// Remove unnecessary delays between device polls
// Only delay for baudrate switching

// Before:
pollDevice(device);
vTaskDelay(pdMS_TO_TICKS(50));  // ← Remove this

// After:
pollDevice(device);
// No delay if next device uses same baudrate
```

Step 5: Build and Benchmark
Build optimized firmware and measure improvements:
- Total cycle time
- Individual device poll times
- CPU usage
- Timeout rate
- Success rate (ensure no degradation)

Expected improvements:
- Adaptive timeout: Save ~1000ms per slow device
- Baudrate grouping: Save ~50ms per switch (2-3 seconds total)
- Batch reads: Save ~200ms per device with multiple registers
- Remove delays: Save ~50ms per device (2.5 seconds total)

Total expected savings: ~8-10 seconds → Target achieved!

Step 6: Validate No Regressions
Test for 24 hours continuous operation:
- All devices poll successfully
- Timeout rate ≤ 2%
- No new errors introduced
- Data accuracy unchanged
- Memory usage stable

Step 7: Document Optimizations
Update CLAUDE.md with optimization details:
- Adaptive timeout algorithm
- Baudrate grouping strategy
- Batch read implementation
- Performance benchmarks (before/after)

CONSTRAINTS:
- Must work with existing device configurations
- Cannot change Modbus protocol implementation
- Must maintain compatibility with all slave devices
- Code must remain readable and maintainable

SUCCESS CRITERIA:
- Total poll cycle ≤ 10 seconds for 50 devices
- CPU usage < 40%
- Timeout rate < 2%
- No functionality regressions
- All tests pass
```

---

## 🔒 Security & Compliance Prompts

### Template: Security Implementation

```
SECURITY FEATURE: [Feature name]

THREAT MODEL:
- Threat 1: [Description and impact]
- Threat 2: [Description and impact]
- Threat 3: [Description and impact]

SECURITY REQUIREMENTS:
1. [Requirement 1]
2. [Requirement 2]
3. [Requirement 3]

IMPLEMENTATION PLAN:
1. Research security best practices (use context7)
2. Design secure implementation
3. Implement with defense in depth
4. Security testing and validation
5. Document security features

COMPLIANCE:
- Standards: [List applicable standards]
- Certifications: [Required certifications]
```

#### Real Example: BLE Authentication

```
SECURITY FEATURE: BLE Pairing with PIN Authentication

THREAT MODEL:
- Threat 1: Unauthorized BLE connection to gateway
  - Impact: Attacker could read/modify configuration
  - Likelihood: High (BLE is wireless, can be sniffed)

- Threat 2: Man-in-the-middle (MITM) attack
  - Impact: Attacker intercepts sensitive data (WiFi password)
  - Likelihood: Medium (requires proximity and tools)

- Threat 3: Replay attack on BLE commands
  - Impact: Attacker replays captured commands
  - Likelihood: Medium (requires packet capture)

SECURITY REQUIREMENTS:
1. PIN-based pairing before any configuration access
2. BLE encryption enabled (BLE Secure Connections)
3. Session timeout after 10 minutes of inactivity
4. Rate limiting on PIN attempts (max 3 attempts)
5. Audit logging of all configuration changes
6. Secure storage of sensitive data (WiFi password encrypted)

IMPLEMENTATION PLAN:

Step 1: Research BLE Security (Context7)
"Show me ESP32 BLE security best practices:
- BLE Secure Connections implementation
- PIN/passkey authentication
- Bonding and key management
- Encryption for BLE characteristics
- Protection against replay attacks
use context7"

Step 2: Design Authentication Flow
```
Connection Flow:
1. Client scans and finds "SURIOTA GW" device
2. Client initiates connection
3. Gateway generates random 6-digit PIN
4. Gateway displays PIN on serial console / optional LCD
5. Client must send correct PIN within 60 seconds
6. On correct PIN: Grant access, enable encryption
7. On wrong PIN: Disconnect, rate limit (3 attempts/hour)
8. Session valid for 10 minutes (renewable)
```

Step 3: Implement PIN Authentication
Create AuthManager class:

```cpp
// Main/AuthManager.h
class AuthManager {
private:
    static AuthManager *instance;

    String currentPIN;
    unsigned long pinGeneratedAt;
    unsigned long sessionStartedAt;

    uint8_t failedAttempts;
    unsigned long lastFailedAttempt;

    bool authenticated;

    const uint32_t PIN_VALID_DURATION_MS = 60000;  // 60 seconds
    const uint32_t SESSION_DURATION_MS = 600000;   // 10 minutes
    const uint8_t MAX_FAILED_ATTEMPTS = 3;
    const uint32_t RATE_LIMIT_WINDOW_MS = 3600000; // 1 hour

    AuthManager();

public:
    static AuthManager *getInstance();

    String generatePIN();
    bool verifyPIN(const String &pin);
    bool isAuthenticated();
    void renewSession();
    void endSession();

    // Rate limiting
    bool isRateLimited();
};
```

Implementation:
```cpp
String AuthManager::generatePIN() {
    // Generate cryptographically secure 6-digit PIN
    currentPIN = String(esp_random() % 1000000);
    currentPIN = String("000000" + currentPIN).substring(currentPIN.length());
    pinGeneratedAt = millis();

    LOG_AUTH_INFO("New PIN generated (valid for 60s)");
    Serial.printf("\n╔══════════════════════╗\n");
    Serial.printf("║  BLE PIN: %s   ║\n", currentPIN.c_str());
    Serial.printf("╚══════════════════════╝\n\n");

    return currentPIN;
}

bool AuthManager::verifyPIN(const String &pin) {
    // Check rate limiting
    if (isRateLimited()) {
        LOG_AUTH_WARN("PIN verification rate limited");
        return false;
    }

    // Check PIN expiration
    if (millis() - pinGeneratedAt > PIN_VALID_DURATION_MS) {
        LOG_AUTH_WARN("PIN expired");
        return false;
    }

    // Verify PIN
    if (pin == currentPIN) {
        authenticated = true;
        sessionStartedAt = millis();
        failedAttempts = 0;

        LOG_AUTH_INFO("Authentication successful");
        return true;
    } else {
        failedAttempts++;
        lastFailedAttempt = millis();

        LOG_AUTH_WARN("Authentication failed (attempt %d/%d)",
                      failedAttempts, MAX_FAILED_ATTEMPTS);
        return false;
    }
}

bool AuthManager::isAuthenticated() {
    if (!authenticated) return false;

    // Check session expiration
    if (millis() - sessionStartedAt > SESSION_DURATION_MS) {
        LOG_AUTH_INFO("Session expired");
        endSession();
        return false;
    }

    return true;
}

bool AuthManager::isRateLimited() {
    if (failedAttempts < MAX_FAILED_ATTEMPTS) return false;

    // Reset counter after rate limit window
    if (millis() - lastFailedAttempt > RATE_LIMIT_WINDOW_MS) {
        failedAttempts = 0;
        return false;
    }

    return true;
}
```

Step 4: Integrate with BLEManager
Update BLEManager to enforce authentication:

```cpp
class BLEServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) {
        LOG_BLE_INFO("Client connected - generating PIN");
        AuthManager::getInstance()->generatePIN();
    }

    void onDisconnect(BLEServer *pServer) {
        LOG_BLE_INFO("Client disconnected - ending session");
        AuthManager::getInstance()->endSession();
    }
};

// Add authentication characteristic
BLECharacteristic *authCharacteristic = pService->createCharacteristic(
    AUTH_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_WRITE
);

authCharacteristic->setCallbacks(new class : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        String pin = pCharacteristic->getValue().c_str();

        if (AuthManager::getInstance()->verifyPIN(pin)) {
            // Authentication successful
            pCharacteristic->setValue("AUTH_OK");
        } else {
            // Authentication failed
            pCharacteristic->setValue("AUTH_FAILED");
        }
    }
});
```

Step 5: Protect CRUD Operations
Update CRUDHandler to check authentication:

```cpp
void CRUDHandler::processCommand(JsonDocument &doc) {
    // Check authentication first
    if (!AuthManager::getInstance()->isAuthenticated()) {
        sendErrorResponse("Not authenticated. Send PIN first.");
        return;
    }

    // Renew session on activity
    AuthManager::getInstance()->renewSession();

    // Process command
    String op = doc["op"].as<String>();
    // ... existing code
}
```

Step 6: Enable BLE Encryption
Configure BLE Secure Connections:

```cpp
void BLEManager::begin() {
    // Enable BLE encryption
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE,
                                    &auth_req,
                                    sizeof(uint8_t));

    // Set IO capability (DisplayOnly for PIN)
    uint8_t iocap = ESP_IO_CAP_OUT;
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE,
                                    &iocap,
                                    sizeof(uint8_t));

    // Enable bonding
    uint8_t key_size = 16;
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE,
                                    &key_size,
                                    sizeof(uint8_t));

    LOG_BLE_INFO("BLE security enabled (Secure Connections)");
}
```

Step 7: Audit Logging
Log all authentication events and config changes:

```cpp
void AuthManager::verifyPIN(const String &pin) {
    // ... existing code

    if (success) {
        logSecurityEvent("AUTH_SUCCESS", "PIN verified");
    } else {
        logSecurityEvent("AUTH_FAILURE",
                        String("Invalid PIN attempt ") + failedAttempts);
    }
}

void CRUDHandler::handleUpdate(JsonDocument &doc) {
    // Log configuration changes
    logSecurityEvent("CONFIG_UPDATE",
                    String("Updated ") + type + " : " + id);

    // ... existing code
}

void logSecurityEvent(const String &event, const String &details) {
    File auditLog = SD.open("/logs/security.log", FILE_APPEND);
    if (auditLog) {
        auditLog.printf("[%s][SECURITY][%s] %s\n",
                       getRTCTimestamp().c_str(),
                       event.c_str(),
                       details.c_str());
        auditLog.close();
    }
}
```

Step 8: Testing & Validation
Security testing scenarios:

Test 1: Valid PIN Authentication
- Connect to BLE
- Get PIN from serial console
- Send correct PIN
- Verify access granted
- Verify session active for 10 minutes

Test 2: Invalid PIN Protection
- Connect to BLE
- Send wrong PIN 3 times
- Verify connection rejected
- Wait 1 hour
- Verify rate limit reset

Test 3: Session Timeout
- Authenticate successfully
- Wait 11 minutes (no activity)
- Try to send command
- Verify authentication required again

Test 4: PIN Expiration
- Generate PIN
- Wait 61 seconds
- Try to authenticate with PIN
- Verify PIN rejected as expired

Test 5: Encryption Verification
- Use BLE sniffer tool
- Capture BLE traffic
- Verify data is encrypted
- Verify cannot read plaintext commands

Step 9: Documentation
Update API.md with security features:
- Authentication flow
- PIN generation/expiration
- Session management
- Rate limiting
- Audit logging

COMPLIANCE:
- Standards:
  - Bluetooth Core Specification 5.0 (Security)
  - OWASP Mobile Top 10 (M4: Insecure Authentication)
- Certifications:
  - Bluetooth SIG Qualification (if required for commercial)

SUCCESS CRITERIA:
- PIN authentication working correctly
- BLE encryption enabled and verified
- Session timeout enforced
- Rate limiting prevents brute force
- All security events logged
- No plaintext sensitive data in BLE packets
```

---

## 📚 Documentation Prompts

### Template: Auto-Documentation

```
DOCUMENTATION TASK: [What to document]

TARGET AUDIENCE:
- [Audience 1]
- [Audience 2]

CONTENT REQUIREMENTS:
1. [What to include]
2. [What to include]
3. [What to include]

FORMAT:
- Structure: [Markdown/PDF/etc]
- Sections: [List main sections]
- Examples: [How many, what type]

GENERATION PLAN:
1. Extract information from code
2. Generate documentation structure
3. Add code examples
4. Add diagrams (if applicable)
5. Review and refine
```

#### Real Example: API Documentation Generation

```
DOCUMENTATION TASK: Generate complete BLE API documentation for mobile app developers

TARGET AUDIENCE:
- Mobile app developers (iOS/Android)
- Integration partners
- QA testers

CONTENT REQUIREMENTS:
1. Complete CRUD operation reference for all types:
   - Devices
   - Server config
   - Logging config
   - Health metrics
2. Request/response format for each operation
3. Error codes and handling
4. Authentication flow
5. Code examples in JSON
6. Common use cases and workflows

FORMAT:
- Structure: Markdown (API.md)
- Sections:
  - Authentication
  - Device Management (CRUD)
  - Server Configuration (CRUD)
  - Logging Configuration (CRUD)
  - Health Monitoring
  - Error Handling
  - Examples & Workflows
- Examples: At least 2 per operation (request + response)

GENERATION PLAN:

Step 1: Extract Current API from Code
Review and extract from:
- Main/CRUDHandler.cpp - All operation handlers
- Main/AuthManager.cpp - Authentication flow
- Main/HealthMonitor.cpp - Health metrics
- Main/BLEManager.cpp - BLE characteristics and callbacks

Step 2: Generate Documentation Structure
Create comprehensive API.md with these sections:

```markdown
# SRT-MGATE-1210 BLE API Reference

## Table of Contents
1. [Overview](#overview)
2. [Authentication](#authentication)
3. [Device Management](#device-management)
4. [Server Configuration](#server-configuration)
5. [Logging Configuration](#logging-configuration)
6. [Health Monitoring](#health-monitoring)
7. [Error Handling](#error-handling)
8. [Examples & Workflows](#examples--workflows)

## Overview
[General API description, versioning, compatibility]

## Authentication
[PIN-based authentication flow]

### Generate PIN
[Description]

**Request:** None (automatic on connection)

**Response:** PIN displayed on gateway serial console

**Example:**
```
[Serial Console Output]
╔══════════════════════╗
║  BLE PIN: 123456   ║
╚══════════════════════╝
```

### Submit PIN
[Description]

**Characteristic:** `AUTH_UUID` (write)

**Request:**
```json
"123456"
```

**Response:**
```json
"AUTH_OK"
```
or
```json
"AUTH_FAILED"
```

[... Continue for all operations ...]
```

Step 3: Document Each Operation
For each CRUD operation, provide:

**Format:**
```markdown
### [Operation Name]

**Description:** [What it does]

**Request:**
```json
{
  "op": "[operation]",
  "type": "[type]",
  [... parameters ...]
}
```

**Response (Success):**
```json
{
  "status": "ok",
  [... response data ...]
}
```

**Response (Error):**
```json
{
  "status": "error",
  "error": "[error message]",
  "code": "[error code]"
}
```

**Example:**
[Real-world example with actual data]

**Notes:**
- [Important notes]
- [Limitations]
- [Best practices]
```

Step 4: Add Code Examples
Include working examples for common workflows:

Example 1: Complete Device Setup
```json
# 1. Authenticate
Write to AUTH characteristic: "123456"

# 2. Create Device
{
  "op": "create",
  "type": "device",
  "config": {
    "device_name": "Temperature Sensor",
    "protocol": "RTU",
    "slave_id": 1,
    "serial_port": 1,
    "baud_rate": 9600,
    "timeout": 3000,
    "retry_count": 3,
    "refresh_rate_ms": 5000,
    "registers": [
      {
        "register_name": "Temperature",
        "address": 0,
        "function_code": 3,
        "data_type": "FLOAT32_ABCD",
        "quantity": 2
      }
    ]
  }
}

# 3. Verify Creation
{
  "op": "read",
  "type": "device",
  "device_id": "D7A3F2"  // ID from create response
}
```

Step 5: Add Error Reference
Complete error code table:

| Error Code | Message | Cause | Resolution |
|------------|---------|-------|------------|
| `AUTH_REQUIRED` | Not authenticated | No PIN submitted | Submit PIN via AUTH characteristic |
| `AUTH_FAILED` | Invalid PIN | Wrong PIN | Check serial console for correct PIN |
| `AUTH_EXPIRED` | Session expired | 10 min timeout | Re-authenticate with new PIN |
| `DEVICE_NOT_FOUND` | Device ID not found | Invalid device_id | Check device_id with "list" operation |
| `INVALID_CONFIG` | Invalid configuration | Missing required fields | Check API reference for required fields |
| ... | ... | ... | ... |

Step 6: Review and Validate
- Test all examples with actual hardware
- Verify JSON syntax is correct
- Ensure all operations are documented
- Check for completeness and accuracy

SUCCESS CRITERIA:
- Mobile app developer can implement full integration without asking questions
- All CRUD operations documented with examples
- Error handling clearly explained
- Authentication flow complete with examples
- Document tested against actual hardware
```

---

## 🎯 Advanced Patterns

### Pattern: Multi-Step Feature Implementation

```
I'm implementing [feature name] for the SRT-MGATE-1210 Gateway.
This is a complex feature that requires multiple steps.

Please work through this systematically:

STEP 1: RESEARCH & PLANNING
Research the latest best practices for [topic] using:
- [Library/framework] documentation (use context7)
- Industrial IoT patterns
- ESP32-specific considerations

Provide:
- Architecture overview
- Component design
- Integration points
- Memory/performance impact

STEP 2: IMPLEMENTATION
Implement the feature following these requirements:
- [Requirement 1]
- [Requirement 2]
- [Requirement 3]

Follow existing code patterns from:
- [File 1] for [pattern]
- [File 2] for [pattern]

STEP 3: TESTING
1. Build firmware (Arduino MCP)
2. Upload to ESP32-S3
3. Test scenarios:
   - [Scenario 1]
   - [Scenario 2]
   - [Scenario 3]
4. Monitor serial output
5. Verify with Playwright (if web component)

STEP 4: DOCUMENTATION
Update these files:
- [File 1] - [What to update]
- [File 2] - [What to update]

STEP 5: CODE REVIEW
Review the implementation for:
- Memory leaks
- Thread safety
- Error handling
- Performance impact

After completing all steps, provide:
- Summary of changes
- Test results
- Known limitations
- Future improvements

Let's start with STEP 1. Don't proceed to next step until I confirm.
```

### Pattern: Production Deployment Checklist

```
Prepare firmware version [X.Y.Z] for production deployment.

Go through this comprehensive checklist systematically:

PHASE 1: CODE QUALITY
□ All compiler warnings resolved
□ No TODO/FIXME comments in production code
□ Code follows style guide (CLAUDE.md)
□ No debug Serial.print statements
□ PRODUCTION_MODE = 1 in Main.ino

PHASE 2: MEMORY VALIDATION
□ PSRAM allocation working correctly
□ Memory usage profiled (DRAM < 200KB free)
□ No memory leaks in 72-hour test
□ All large objects in PSRAM
□ Stack sizes validated for all tasks

PHASE 3: CONFIGURATION
□ Default configuration tested
□ Factory reset procedure works
□ Configuration backup/restore tested
□ All config validation working
□ Sensitive data encrypted (WiFi password)

PHASE 4: SECURITY
□ BLE authentication enabled
□ MQTT TLS configured (if applicable)
□ Input validation on all interfaces
□ Audit logging functional
□ No hardcoded credentials

PHASE 5: PERFORMANCE
□ Modbus polling within target (< 10s for 50 devices)
□ MQTT publishing reliable (no dropped messages)
□ Network failover < 5s
□ CPU usage < 50% average
□ Response time requirements met

PHASE 6: RELIABILITY
□ 72-hour continuous operation stable
□ Error recovery tested
□ Watchdog timer configured
□ Auto-restart on crash
□ Graceful degradation working

PHASE 7: TESTING
□ All unit tests pass
□ Integration tests complete
□ Stress test passed
□ Field test results reviewed
□ Regression tests pass

PHASE 8: DOCUMENTATION
□ VERSION_HISTORY.md updated
□ API.md current
□ USER_MANUAL.md complete
□ DEPLOYMENT_GUIDE.md ready
□ TROUBLESHOOTING.md updated

For each phase:
1. Verify current status
2. Identify any failing items
3. Provide remediation plan
4. Re-test after fixes

Generate a production readiness report with:
- Phase completion status
- Issues found and resolved
- Outstanding risks
- Go/No-Go recommendation

use context7 for any best practice research needed.
```

---

## 💡 Pro Tips

### 1. Always Provide Context

```
❌ Bad: "The gateway crashes"

✅ Good: "The SRT-MGATE-1210 Gateway (firmware v2.3.11) running on ESP32-S3
crashes after 48 hours with 'Out of memory' error during ConfigManager::loadDevices().
Free DRAM decreases from 280KB to 15KB over 48 hours.
Configuration: 50 RTU devices, 200+ registers, MQTT every 5s."
```

### 2. Use MCP Tools Explicitly

```
❌ Bad: "How do I use ArduinoJson?"

✅ Good: "Show me ArduinoJson 7.4.2 API for PSRAM allocation on ESP32-S3.
use context7"
```

### 3. Specify Success Criteria

```
❌ Bad: "Optimize the code"

✅ Good: "Optimize Modbus polling to achieve < 10 seconds for 50 devices
while maintaining < 40% CPU usage and < 2% timeout rate"
```

### 4. Request Incremental Progress

```
Don't proceed to implementation until I review the architecture.
Show me the design first, wait for my approval, then implement.
```

### 5. Use Model Selection Appropriately

```
# Quick tasks - Haiku
claude --model haiku "Build the project and show compilation results"

# Complex debugging - Opus
claude --model opus "Analyze this memory leak and design a comprehensive fix"

# Balanced development - Sonnet (default)
claude "Add BLE authentication with PIN support"
```

---

## 🎓 Learning Examples

### Example 1: Complete Feature (Beginner to Expert)

**Beginner Prompt:**
```
Add a restart command to the gateway
```

**Expert Prompt:**
```
FEATURE: Add remote restart command via BLE and MQTT

CONTEXT:
- Firmware: SRT-MGATE-1210 v2.3.11 on ESP32-S3
- Need graceful restart for remote maintenance
- Must save state before restart
- Must be secure (authenticated users only)

REQUIREMENTS:
1. BLE command: {"op": "restart", "delay_ms": 5000}
2. MQTT command: topic "suriota/gateway/[gateway_id]/command" payload {"cmd": "restart"}
3. Graceful shutdown sequence:
   - Disable new device polling
   - Complete current MQTT publish
   - Save current state to SD card
   - Close all connections
   - Delay configurable (default 5s)
   - ESP.restart()
4. Authentication required for BLE
5. MQTT command requires proper topic ACL
6. Log restart event with reason
7. Broadcast restart notification before restarting

IMPLEMENTATION:

Step 1: Research ESP32 Restart Best Practices (Context7)
"Show me ESP32 graceful restart patterns:
- esp_restart() vs ESP.restart()
- Saving state before restart
- Clean disconnect from network
- RTC memory for restart reason
use context7"

Step 2: Design RestartManager Class
Create Main/RestartManager.h/cpp:
- Singleton pattern
- Graceful shutdown sequence
- State persistence
- Restart reason tracking (stored in RTC memory)
- Restart history (last 10 restarts in SD card)

Step 3: Implement BLE Command
Update CRUDHandler to handle restart operation:
```cpp
if (op == "restart") {
    uint32_t delay = doc["delay_ms"] | 5000;
    RestartManager::getInstance()->scheduleRestart(delay, "BLE command");
    sendSuccessResponse("Restart scheduled in " + String(delay) + "ms");
}
```

Step 4: Implement MQTT Command
Update MqttManager to subscribe to command topic:
```cpp
void MqttManager::handleCommand(const String &payload) {
    JsonDocument doc;
    deserializeJson(doc, payload);

    if (doc["cmd"] == "restart") {
        uint32_t delay = doc["delay_ms"] | 5000;
        RestartManager::getInstance()->scheduleRestart(delay, "MQTT command");
    }
}
```

Step 5: Implement Graceful Shutdown
```cpp
void RestartManager::executeRestart() {
    LOG_SYS_INFO("Graceful restart initiated: %s", reason.c_str());

    // 1. Disable polling
    ModbusRtuService::getInstance()->disable();
    ModbusTcpService::getInstance()->disable();

    // 2. Flush MQTT queue
    MqttManager::getInstance()->flushQueue();

    // 3. Save state
    saveRestartState();

    // 4. Broadcast notification
    MqttManager::getInstance()->publish(
        "suriota/gateway/status",
        "{\"status\":\"restarting\",\"reason\":\"" + reason + "\"}"
    );

    // 5. Close connections
    MqttManager::getInstance()->disconnect();
    WiFi.disconnect();

    // 6. Save restart reason to RTC memory
    saveRestartReason();

    // 7. Delay
    delay(delayMs);

    // 8. Restart
    LOG_SYS_INFO("Restarting now...");
    ESP.restart();
}
```

Step 6: Build and Test (Arduino MCP)
Test scenarios:
1. BLE restart command with 5s delay
2. MQTT restart command with 10s delay
3. Verify graceful shutdown sequence
4. Check restart reason logged correctly
5. Verify state restored after restart

Step 7: Security Testing (if authenticated)
- Verify unauthenticated BLE connection cannot restart
- Verify MQTT command requires proper authentication
- Test rate limiting (max 1 restart per 5 minutes)

Step 8: Documentation
Update Documentation/API_Reference/API.md:
- Add restart command documentation
- Security considerations
- Example usage

SUCCESS CRITERIA:
- Remote restart working via BLE and MQTT
- Graceful shutdown preserves state
- No data loss during restart
- Restart reason tracked and logged
- Authentication enforced
- Documentation complete

Let's start with Step 1 - research ESP32 restart best practices.
```

---

## 📊 Prompt Effectiveness Metrics

### Measure Your Prompts

| Metric | Poor Prompt | Good Prompt | Expert Prompt |
|--------|-------------|-------------|---------------|
| **Time to Solution** | 2-4 hours | 1-2 hours | 30-60 min |
| **Iterations Needed** | 5-10 | 2-3 | 1-2 |
| **Code Quality** | Needs refactoring | Acceptable | Production-ready |
| **Documentation** | Missing | Basic | Comprehensive |
| **Test Coverage** | None | Manual testing | Automated tests |
| **Context Usage** | Wastes tokens | Efficient | Optimized |

### Self-Assessment Questions

After each prompting session, ask:

1. ✅ Did I provide enough context?
2. ✅ Did I specify the exact goal and constraints?
3. ✅ Did I use appropriate MCP tools (Context7, Playwright, Arduino MCP)?
4. ✅ Did I request proper testing and validation?
5. ✅ Did I specify documentation requirements?
6. ✅ Did I use the right model for the task?
7. ✅ Was the output production-ready, or did it need refinement?

---

## 🚀 Conclusion

**The Expert Firmware Prompt Formula:**

```
EXPERT PROMPT = Context + Goal + Constraints + MCP Tools + Output Format

Where:
- Context: Firmware version, hardware, current state
- Goal: Specific, measurable objective
- Constraints: Memory, performance, compatibility requirements
- MCP Tools: Context7, Playwright, Arduino MCP as needed
- Output Format: Code + tests + documentation
```

**Key Principles:**

1. **Be Specific:** Vague prompts = vague results
2. **Provide Context:** Help Claude understand the full picture
3. **Use MCPs:** Leverage Context7, Playwright, Arduino MCP
4. **Specify Testing:** Always include validation steps
5. **Request Documentation:** Make it part of every task
6. **Choose Right Model:** Haiku/Sonnet/Opus for task complexity
7. **Iterate Systematically:** Break complex tasks into steps

**Result:**

🎯 **4x faster development**
🎯 **Production-ready code first time**
🎯 **Comprehensive testing and documentation**
🎯 **Optimized resource usage (time, cost, quality)**

---

**Happy Prompting! 🚀**

**SURIOTA R&D Team | Powered by Claude Code + Expert Prompting**
**Version 2.0.0 | November 24, 2025**
