# ✅ TESTING CHECKLIST - BLE Backup & Restore

**Tester:** _________________
**Date:** ___________________
**Firmware Version:** ________
**Gateway SN:** ______________

---

## 📋 PRE-TESTING SETUP

### Hardware & Software Setup

- [ ] ESP32-S3 Gateway powered ON
- [ ] Firmware v2.3.0+ uploaded
- [ ] Serial Monitor open (115200 baud)
- [ ] Python 3.8+ installed
- [ ] Library `bleak` installed (`pip3 install bleak`)
- [ ] PC/Laptop Bluetooth enabled

### Gateway Configuration

- [ ] Gateway has 3-5 devices configured
- [ ] BLE is ON (check serial monitor)
- [ ] Gateway is advertising "SURIOTA GW"
- [ ] MTU negotiation successful (check serial)

**Notes:**
```
_____________________________________________________________

_____________________________________________________________

_____________________________________________________________
```

---

## 🧪 TEST 1: BACKUP CONFIGURATION

**Test ID:** TEST-BACKUP-001
**Duration:** ~2-5 minutes
**Payload:** `{"op": "read", "type": "full_config"}`

### Execution Steps

- [ ] Run test script: `python3 test_backup_restore.py`
- [ ] Script connected to gateway successfully
- [ ] Select option 1 from menu
- [ ] Command sent via BLE

### Response Validation

- [ ] Response received (no timeout)
- [ ] Status = "ok"
- [ ] `backup_info` object present
- [ ] `backup_info.timestamp` present (number)
- [ ] `backup_info.firmware_version` = "2.3.0"
- [ ] `backup_info.device_name` = "SURIOTA_GW"
- [ ] `backup_info.total_devices` matches actual count
- [ ] `backup_info.total_registers` matches actual count
- [ ] `backup_info.processing_time_ms` < 2000ms
- [ ] `backup_info.backup_size_bytes` > 0 and < 210000
- [ ] `config` object present
- [ ] `config.devices` is array
- [ ] `config.devices` length matches `total_devices`
- [ ] Each device has `device_id` field
- [ ] Each device has `registers` array
- [ ] `config.server_config` present
- [ ] `config.logging_config` present

### Serial Monitor Validation

- [ ] Serial shows: "[CRUD] Full config backup requested"
- [ ] Serial shows: "[CRUD] Full config backup complete: X devices, Y registers, Z bytes, N ms"
- [ ] No error messages in serial

### Performance Metrics

**Actual Values:**
- Total devices: _______
- Total registers: _______
- Backup size: _______ bytes (_______ KB)
- Processing time: _______ ms
- BLE transfer time: _______ seconds

### Result

- [ ] ✅ PASSED
- [ ] ❌ FAILED - Reason: _________________________________

**Notes:**
```
_____________________________________________________________

_____________________________________________________________

_____________________________________________________________
```

---

## 🧪 TEST 2: RESTORE CONFIGURATION

**Test ID:** TEST-RESTORE-001
**Duration:** ~3-8 minutes
**Payload:** `{"op": "system", "type": "restore_config", "config": {...}}`

### Execution Steps

- [ ] Select option 2 from menu (or option 4 for full cycle)
- [ ] Confirmation prompt displayed
- [ ] Type "yes" to confirm
- [ ] Command sent via BLE

### Response Validation

- [ ] Response received (no timeout)
- [ ] Status = "ok"
- [ ] `restored_configs` array present
- [ ] `restored_configs` contains "devices.json"
- [ ] `restored_configs` contains "server_config.json"
- [ ] `restored_configs` contains "logging_config.json"
- [ ] `success_count` = 3
- [ ] `fail_count` = 0
- [ ] `message` present
- [ ] `requires_restart` = true

### Serial Monitor Validation

- [ ] Serial shows: "[CONFIG RESTORE] ⚠️  INITIATED by BLE client"
- [ ] Serial shows: "[CONFIG RESTORE] [1/3] Restoring devices configuration..."
- [ ] Serial shows: "[CONFIG RESTORE] Existing devices cleared"
- [ ] Serial shows: "[CONFIG RESTORE] Restored X devices"
- [ ] Serial shows: "[CONFIG RESTORE] [2/3] Restoring server configuration..."
- [ ] Serial shows: "[CONFIG RESTORE] Server config restored successfully"
- [ ] Serial shows: "[CONFIG RESTORE] [3/3] Restoring logging configuration..."
- [ ] Serial shows: "[CONFIG RESTORE] Logging config restored successfully"
- [ ] Serial shows: "[CONFIG RESTORE] Restore complete: 3 succeeded, 0 failed"
- [ ] No error messages (except expected warnings)

### Post-Restore Validation

- [ ] Gateway still responsive
- [ ] BLE still connectable
- [ ] Can perform backup again (verify restore worked)
- [ ] Device count matches restored count

### Result

- [ ] ✅ PASSED
- [ ] ❌ FAILED - Reason: _________________________________

**Notes:**
```
_____________________________________________________________

_____________________________________________________________

_____________________________________________________________
```

---

## 🧪 TEST 3: ERROR HANDLING

**Test ID:** TEST-ERROR-001
**Duration:** ~1-2 minutes
**Payload:** `{"op": "system", "type": "restore_config"}` (missing config)

### Execution Steps

- [ ] Select option 3 from menu
- [ ] Command sent via BLE

### Response Validation

- [ ] Response received (no timeout)
- [ ] Status = "error"
- [ ] `error` field present
- [ ] Error message contains "Missing 'config' object"

### Serial Monitor Validation

- [ ] Serial shows: "[CONFIG RESTORE] ❌ ERROR: Missing 'config' object in payload"
- [ ] Gateway did not crash
- [ ] Gateway still responsive after error

### Result

- [ ] ✅ PASSED
- [ ] ❌ FAILED - Reason: _________________________________

**Notes:**
```
_____________________________________________________________

_____________________________________________________________

_____________________________________________________________
```

---

## 🧪 TEST 4: BACKUP-RESTORE-COMPARE CYCLE

**Test ID:** TEST-INTEGRITY-001
**Duration:** ~10-15 minutes
**Priority:** ⭐⭐⭐ CRITICAL TEST

### Execution Steps

- [ ] Select option 4 from menu
- [ ] STEP 1: Initial backup created
- [ ] Backup file saved: `backup_before_restore_*.json`
- [ ] STEP 2: Restore executed
- [ ] Type "yes" to confirm restore
- [ ] Wait 3 seconds for restore completion
- [ ] STEP 3: Second backup created
- [ ] Backup file saved: `backup_after_restore_*.json`
- [ ] STEP 4: Comparison executed

### Data Integrity Validation

- [ ] Device count matches (before = after)
- [ ] Device IDs match (no missing, no extra)
- [ ] Register count matches (before = after)
- [ ] Server config present in both backups
- [ ] Logging config present in both backups

### File Validation

- [ ] Both backup files exist
- [ ] Both files are valid JSON
- [ ] File sizes are similar (±10%)
- [ ] Manual inspection: files are identical (use JSON diff tool)

### Result

- [ ] ✅ PASSED - Data integrity verified
- [ ] ❌ FAILED - Reason: _________________________________

**Backup File Comparison:**
- Before file size: _______ bytes
- After file size: _______ bytes
- Difference: _______ bytes (_______ %)

**Notes:**
```
_____________________________________________________________

_____________________________________________________________

_____________________________________________________________
```

---

## 🔍 MEMORY MONITORING

**During all tests, monitor memory usage:**

### DRAM Usage

- [ ] Free DRAM never drops below 20KB
- [ ] No "[MEM] CRITICAL" warnings in serial
- [ ] No "[MEM] WARNING" warnings (or rare)

**Measurements:**
- Initial free DRAM: _______ bytes
- During backup: _______ bytes
- During restore: _______ bytes
- After all tests: _______ bytes

### PSRAM Usage

- [ ] PSRAM used for large allocations
- [ ] No "[MEM] PSRAM allocation failed" errors

**Measurements:**
- Initial free PSRAM: _______ bytes
- During backup: _______ bytes
- During restore: _______ bytes

---

## 🚨 ERROR LOG

**Record any errors encountered:**

| Time | Test | Error Description | Resolution |
|------|------|------------------|------------|
|      |      |                  |            |
|      |      |                  |            |
|      |      |                  |            |
|      |      |                  |            |

---

## 📊 PERFORMANCE SUMMARY

### Test Results Overview

| Test ID | Test Name | Status | Duration | Notes |
|---------|-----------|--------|----------|-------|
| TEST-BACKUP-001 | Backup | ⬜ | _____ | _____ |
| TEST-RESTORE-001 | Restore | ⬜ | _____ | _____ |
| TEST-ERROR-001 | Error Handling | ⬜ | _____ | _____ |
| TEST-INTEGRITY-001 | Backup-Restore-Compare | ⬜ | _____ | _____ |

**Legend:** ✅ = Passed, ❌ = Failed, ⚠️ = Passed with warnings, ⬜ = Not tested

### Overall Assessment

**Total Tests:** 4
**Tests Passed:** _______
**Tests Failed:** _______
**Pass Rate:** _______ %

### Critical Issues Found

- [ ] None - All tests passed
- [ ] Minor issues (list below)
- [ ] Major issues (list below)
- [ ] Critical issues - DO NOT DEPLOY

**Issues:**
```
_____________________________________________________________

_____________________________________________________________

_____________________________________________________________

_____________________________________________________________
```

---

## ✅ POST-TESTING VERIFICATION

### Gateway Status

- [ ] Gateway still responsive
- [ ] BLE still functional
- [ ] Can connect and disconnect multiple times
- [ ] Modbus polling still active (check serial)
- [ ] Network connection stable
- [ ] No memory leaks detected
- [ ] No crashes during testing

### File Artifacts

- [ ] Backup files saved successfully
- [ ] Backup files are valid JSON
- [ ] Backup files can be opened in text editor
- [ ] File sizes are reasonable

**Files Generated:**
```
_____________________________________________________________

_____________________________________________________________

_____________________________________________________________
```

---

## 📝 RECOMMENDATIONS

### For Production Deployment

- [ ] ✅ Approved for production use
- [ ] ⚠️ Approved with conditions (specify below)
- [ ] ❌ Not approved - fixes required

**Conditions/Required Fixes:**
```
_____________________________________________________________

_____________________________________________________________

_____________________________________________________________

_____________________________________________________________
```

### Additional Testing Needed

- [ ] None - testing complete
- [ ] Long-term stability testing (24+ hours)
- [ ] Large configuration testing (50+ devices)
- [ ] Concurrent operations testing
- [ ] Cross-version compatibility testing
- [ ] Other: _________________________________________________

---

## 📋 SIGN-OFF

**Tester Signature:** _____________________
**Date:** _____________________

**Reviewer Signature:** _____________________
**Date:** _____________________

**Approved for Production:** [ ] YES  [ ] NO

---

**Notes for Next Testing Session:**
```
_____________________________________________________________

_____________________________________________________________

_____________________________________________________________

_____________________________________________________________

_____________________________________________________________
```

---

**Made with ❤️ by SURIOTA R&D Team**
*Empowering Industrial IoT Solutions*
