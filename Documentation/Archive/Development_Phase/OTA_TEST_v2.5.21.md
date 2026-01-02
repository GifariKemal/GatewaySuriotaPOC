# 🧪 OTA Update Test Guide - v2.5.21

**Test Date:** 2025-12-02  
**Current Device Version:** 2.5.20  
**Target Version (Manifest):** 2.5.21  
**Binary:** Same as v2.5.20 (2,010,336 bytes)

---

## 📋 Test Objective

Test the OTA update mechanism with the timeout and buffer fixes:

- ✅ Verify update detection (v2.5.20 → v2.5.21)
- ✅ Verify download completes 100% (no timeout at 97%)
- ✅ Verify SHA-256 and signature validation
- ✅ Verify device reboots successfully

---

## 🚀 Running the Test

### **Method 1: Automated Script (Recommended)**

Navigate to test directory:

```bash
cd Testing\BLE_Testing\OTA_Test
```

Run OTA update:

```bash
python ota_update.py --update
```

Or use auto mode (no prompts):

```bash
python ota_update.py --auto
```

### **Method 2: Interactive Menu**

```bash
python ota_update.py --menu
```

Then select:

1. Check for updates
2. Full OTA update

### **Method 3: Check Only**

To just check if update is detected:

```bash
python ota_update.py --check
```

---

## 📊 Expected Output

### **Step 1: Update Detection**

```
════════════════════════════════════════════════════════════════════
║  🚀  OTA FIRMWARE UPDATE TOOL  -  SRT-MGATE-1210                ║
════════════════════════════════════════════════════════════════════

──────────────────────────────────────────────────────────────────
⏳ [Step 1/4] Checking for firmware updates
──────────────────────────────────────────────────────────────────

  📡 Scanning for BLE devices...
  ✓ Found: SURIOTA GW (XX:XX:XX:XX:XX:XX)

  🔗 Connecting to XX:XX:XX:XX:XX:XX...
  ✓ Connected & notifications enabled

  📤 Sending: {"op":"ota","type":"check_update"}
  📥 Receiving: .........

  ✅ Update available: 2.5.20 → 2.5.21

  ┌────────────────────────────────────────────────────────────────┐
  │ 📦 Update Information                                          │
  ├────────────────────────────────────────────────────────────────┤
  │  Current Version: 2.5.20                                       │
  │  Target Version: 2.5.21                                        │
  │  Firmware Size: 2,010,336 bytes                                │
  │  Mandatory: No                                                 │
  └────────────────────────────────────────────────────────────────┘
```

### **Step 2: Firmware Download**

```
──────────────────────────────────────────────────────────────────
⏳ [Step 2/4] Downloading firmware from GitHub
──────────────────────────────────────────────────────────────────

  ℹ️  This may take 1-2 minutes depending on network speed...
  ℹ️  Please wait while firmware downloads and verifies...

  📤 Sending: {"op":"ota","type":"start_update"}
  📥 Receiving: .......(10s).......(20s).......(30s)...

  ✅ Firmware downloaded and verified in 113.5 seconds!

  ┌────────────────────────────────────────────────────────────────┐
  │ ✅ Download Complete                                           │
  ├────────────────────────────────────────────────────────────────┤
  │  Download Time: 113.5 seconds                                  │
  │  Status: Success                                               │
  │  State: VALIDATING                                             │
  └────────────────────────────────────────────────────────────────┘
```

**CRITICAL:** Watch for these log lines on device serial monitor:

```
[OTA] Starting download from GitHub...
[OTA] Downloading firmware: 1.92 MB
[OTA] [=============================>] 100% 1.92MB/1.92MB @ 17.7 KB/s
[OTA] Download complete - verification passed!
```

**Should NOT see:**

```
❌ [WARN][OTA] Connection closed, no data for 1009 ms
❌ [ERROR][OTA] Incomplete: 1961619 / 2010336
```

### **Step 3: Confirmation**

```
──────────────────────────────────────────────────────────────────
⏳ [Step 3/4] Confirm Update Application
──────────────────────────────────────────────────────────────────

  ════════════════════════════════════════════════════════
  ║  ⚠️  WARNING: Device will REBOOT after applying update!  ║
  ════════════════════════════════════════════════════════

  Do you want to apply the update and reboot? (y/n)
  > y
```

### **Step 4: Apply & Reboot**

```
──────────────────────────────────────────────────────────────────
⏳ [Step 4/4] Applying firmware update
──────────────────────────────────────────────────────────────────

  ⚠️  Device will reboot in a few seconds...
  ⏳ Applying update in 3...
  ⏳ Applying update in 2...
  ⏳ Applying update in 1...

  📤 Sending: {"op":"ota","type":"apply_update"}

  ════════════════════════════════════════════════════════
  ║  🔄  Device is rebooting with new firmware...         ║
  ════════════════════════════════════════════════════════
```

---

## ✅ Success Criteria

### **Download Phase**

- [ ] Update detected (v2.5.20 → v2.5.21)
- [ ] Download starts successfully
- [ ] Progress reaches **100%** (not stuck at 97%)
- [ ] No "Connection closed unexpectedly" error
- [ ] Download time: ~110-120 seconds at 17.7 KB/s
- [ ] SHA-256 verification: PASSED
- [ ] Signature verification: PASSED

### **Apply Phase**

- [ ] Device accepts apply command
- [ ] Device reboots within 5 seconds
- [ ] Device comes back online after ~30 seconds

### **Post-Reboot**

- [ ] Device version still shows 2.5.20 (same binary)
- [ ] All services running normally
- [ ] No boot errors in logs

---

## 🐛 Troubleshooting

### **Issue: Update Not Detected**

**Symptom:**

```
Already running latest version (2.5.20)
```

**Solution:**

1. Check manifest on GitHub is v2.5.21
2. Clear device cache (reboot device)
3. Check GitHub token is set correctly

### **Issue: Download Fails at 97%**

**Symptom:**

```
[ERROR][OTA] Incomplete: 1961619 / 2010336
```

**Solution:**

- ❌ This means the fix DIDN'T work
- Check if you flashed the correct v2.5.20 firmware
- Verify timeout is 5000ms in OTAHttps.cpp
- Verify buffer is 8192 in OTAHttps.h

### **Issue: Signature Verification Failed**

**Symptom:**

```
[ERROR][OTA] Signature verification failed
```

**Solution:**

- Binary mismatch between local and GitHub
- Re-upload binary to GitHub
- Verify SHA-256 hash matches

### **Issue: Device Won't Reboot**

**Symptom:**

- Apply command sent but device doesn't reboot

**Solution:**

- Wait 30 seconds
- Power cycle device manually
- Check logs for errors

---

## 📊 Monitoring Device Logs

While running the test, monitor device serial output:

```bash
# Windows (Arduino IDE Serial Monitor)
Tools → Serial Monitor → 115200 baud

# Or use PlatformIO
pio device monitor -b 115200
```

**Key log lines to watch:**

```
[OTA] Fetching manifest...
[OTA] Manifest parsed: v2.5.21 (build 2521), size 2010336
[OTA] Starting download...
[OTA] Content length: 2010336 bytes
[OTA] [=====>] 25% 0.48MB/1.92MB @ 17.7 KB/s
[OTA] [===========>] 50% 0.96MB/1.92MB @ 17.7 KB/s
[OTA] [=================>] 75% 1.44MB/1.92MB @ 17.7 KB/s
[OTA] [============================>] 97% 1.86MB/1.92MB @ 17.7 KB/s  ← CRITICAL
[OTA] [=============================>] 100% 1.92MB/1.92MB @ 17.7 KB/s ← SUCCESS!
[OTA] Download complete: 2010336 bytes in 113450 ms
[OTA] SHA-256 verification: PASSED
[OTA] Signature verification: PASSED
[OTA] OTA finalized, boot partition set
```

---

## 📝 Test Results Template

```
OTA Update Test - v2.5.21
Date: 2025-12-02
Tester: [Your Name]

Pre-Test:
- Device Version: 2.5.20
- Manifest Version: 2.5.21
- Binary Size: 2,010,336 bytes

Test Results:
[ ] Update detected successfully
[ ] Download started
[ ] Download progress: ____%
[ ] Download completed: YES / NO
[ ] Download time: _____ seconds
[ ] SHA-256 verified: YES / NO
[ ] Signature verified: YES / NO
[ ] Device rebooted: YES / NO
[ ] Post-reboot status: OK / ERROR

Issues Encountered:
[Describe any issues]

Conclusion:
[ ] PASS - OTA update successful
[ ] FAIL - [Reason]
```

---

## 🎯 Next Steps After Test

### **If Test PASSES:**

1. ✅ OTA mechanism is working correctly
2. ✅ Timeout fix is effective
3. ✅ Buffer optimization is stable
4. 🎉 Ready for production deployment

### **If Test FAILS:**

1. Review device logs for error messages
2. Check network connection stability
3. Verify firmware was flashed correctly
4. Report issue with logs for debugging

---

**Ready to test!** Run the command and monitor the output carefully.

Good luck! 🚀
