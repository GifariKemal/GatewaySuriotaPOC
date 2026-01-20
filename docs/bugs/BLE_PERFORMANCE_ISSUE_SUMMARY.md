# Bug Report Summary: BLE Performance Issue

**Date:** 2026-01-20  
**Severity:** HIGH  
**Status:** OPEN

---

## 🔴 Problem

BLE read device command takes **~28+ seconds** (expected: 3-5s) and causes occasional disconnection.

---

## 🎯 Root Causes

1. **RTU Auto-Recovery Blocking** (~12-15s overhead)
   - 3 RTU devices failing continuously
   - 5 retries per device with exponential backoff
   - Runs during BLE operations

2. **MQTT Reconnection Loop** (~18-20s overhead)
   - Reconnects 5x in 24 seconds
   - Public broker instability
   - Runs during BLE operations

3. **Low DRAM Memory** (~3s overhead)
   - Only 13.5KB free
   - Forces SLOW BLE mode (100 bytes chunks, 35ms delay)

4. **No BLE Priority** (Critical)
   - Background tasks not paused during BLE operations

---

## ✅ Solutions (Priority Order)

### P0: BLE Priority Management (2-3 days)

```cpp
void processBLECommand() {
  pause_rtu_auto_recovery();
  pause_mqtt_keepalive();
  handle_ble_command();
  resume_rtu_auto_recovery();
  resume_mqtt_keepalive();
}
```

### P1: Disable RTU During BLE (1 day)

- Pause RTU auto-recovery when BLE active
- Or increase recovery interval to 5 minutes

### P1: Pause MQTT During BLE (1 day)

- Pause MQTT keepalive during BLE operations

### P2: Fix MQTT Stability (2-3 days)

- Use local broker instead of public
- Increase keepalive to 60-120s

### P3: Optimize Memory (3-5 days)

- Implement pagination for large devices
- Free unused buffers before BLE

---

## 📊 Expected Impact

| Metric            | Before     | After  | Improvement |
| ----------------- | ---------- | ------ | ----------- |
| Response Time     | ~28s       | 3-5s   | **80-85%**  |
| BLE Disconnection | Occasional | None   | **100%**    |
| MQTT Stability    | Unstable   | Stable | **100%**    |

---

## 📎 Full Report

See: `docs/bugs/BLE_PERFORMANCE_ISSUE_2026-01-20.md`

---

**Action Required:** Implement P0 and P1 fixes (estimated 4-5 days)
