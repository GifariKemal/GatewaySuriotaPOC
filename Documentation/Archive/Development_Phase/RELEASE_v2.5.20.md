# 🚀 Release Guide: Firmware v2.5.20

## 📦 Firmware Details

**Version:** 2.5.20  
**Build Number:** 2520  
**Release Date:** 2025-12-02  
**Binary Size:** 2,010,336 bytes (1.92 MB)

**SHA-256 Hash:**

```
4d1c08b99303a1bf0bdf6eda9029a40a8ed3ee11fe14697c0e11ae7921526eab
```

**ECDSA Signature (DER format):**

```
3045022100c24e85a8ef77ecda56d9ea2f6bdb3522717b4ad9411b0e95007603cb8a80734b02202e7416e507cf8f0a45ad7659b1e180160df196933d317e8cc4c253ab06e238cf
```

---

## 📋 Release Steps

### Step 1: Prepare OTA Repository

Navigate to your OTA repository:

```bash
cd /path/to/GatewaySuriotaOTA
```

### Step 2: Create Release Directory

```bash
mkdir -p releases/v2.5.20
```

### Step 3: Copy Firmware Binary

Copy the compiled binary from POC project:

```bash
cp /path/to/GatewaySuriotaPOC/Main/build/esp32.esp32.esp32s3/Main.ino.bin releases/v2.5.20/firmware.bin
```

**Verify file size:**

```bash
ls -lh releases/v2.5.20/firmware.bin
# Should show: 2010336 bytes (1.92 MB)
```

### Step 4: Commit Binary to OTA Repository

```bash
git add releases/v2.5.20/firmware.bin
git commit -m "feat: Add firmware binary v2.5.20 - OTA stability fixes"
git push origin main
```

### Step 5: Update Manifest

Edit `firmware_manifest.json` in the root of OTA repository with the following
content:

```json
{
  "version": "2.5.20",
  "build_number": 2520,
  "release_date": "2025-12-02",
  "min_version": "2.3.0",
  "firmware": {
    "url": "https://api.github.com/repos/GifariKemal/GatewaySuriotaOTA/contents/releases/v2.5.20/firmware.bin?ref=main",
    "filename": "firmware.bin",
    "size": 2010336,
    "sha256": "4d1c08b99303a1bf0bdf6eda9029a40a8ed3ee11fe14697c0e11ae7921526eab",
    "signature": "3045022100c24e85a8ef77ecda56d9ea2f6bdb3522717b4ad9411b0e95007603cb8a80734b02202e7416e507cf8f0a45ad7659b1e180160df196933d317e8cc4c253ab06e238cf"
  },
  "changelog": [
    "v2.5.20: OTA timeout fixes & SSL buffer optimization",
    "- Fixed connection timeout at 97% download completion",
    "- Reduced SSL buffer for better stability with slow networks"
  ],
  "mandatory": false
}
```

### Step 6: Commit Manifest

```bash
git add firmware_manifest.json
git commit -m "release: Update manifest to v2.5.20"
git push origin main
```

---

## ✅ Verification Checklist

Before testing OTA update, verify:

- [ ] Binary file exists at `releases/v2.5.20/firmware.bin`
- [ ] Binary size is exactly **2,010,336 bytes**
- [ ] Binary is pushed to GitHub (check via web interface)
- [ ] Manifest `firmware_manifest.json` is updated
- [ ] Manifest is pushed to GitHub
- [ ] URL in manifest uses API format: `api.github.com/.../contents/...`
- [ ] SHA-256 hash matches the binary
- [ ] Signature is valid (already verified by sign_firmware.py)

---

## 🧪 Testing OTA Update

### Test 1: Check for Update

Send BLE command:

```json
{ "op": "ota", "type": "check_update" }
```

**Expected response:**

```
Update available: v2.5.20
```

### Test 2: Start OTA Update

Send BLE command:

```json
{ "op": "ota", "type": "start_update" }
```

**Expected log output:**

```
[OTA] Fetching manifest...
[OTA] Manifest parsed: v2.5.20 (build 2520), size 2010336
[OTA] Starting download...
[OTA] [=============================>] 100% 1.92MB/1.92MB @ 17.7 KB/s
[OTA] Download complete - verification passed!
[OTA] Firmware ready for reboot
```

**Success criteria:**

- ✅ Download reaches 100% completion
- ✅ No "Connection closed unexpectedly" error
- ✅ SHA-256 verification passes
- ✅ Signature verification passes
- ✅ Device reboots with new firmware

---

## 🐛 Troubleshooting

| Issue                | Solution                                                  |
| -------------------- | --------------------------------------------------------- |
| **HTTP 404**         | Verify binary is pushed to GitHub and URL uses API format |
| **Signature Failed** | Ensure binary in GitHub matches the signed binary exactly |
| **Size Mismatch**    | Check manifest size (2010336) matches actual binary size  |
| **Timeout at 97%**   | This should be FIXED in v2.5.20 (5s timeout)              |

---

## 📝 Changelog

**v2.5.20 Changes:**

- Fixed OTA download timeout at 97% completion
- Increased "no data" timeout from 1s to 5s
- Reduced SSL buffer from 16KB to 8KB for stability
- Optimized for slow network connections (~17 KB/s)

---

## 🔐 Security Notes

- Binary is signed with ECDSA P-256 private key
- Signature is verified on device before flashing
- SHA-256 hash ensures binary integrity
- TLS 1.2+ used for secure download from GitHub

---

**Generated:** 2025-12-02 07:25 WIB  
**Signed by:** sign_firmware.py v1.0  
**Ready for deployment:** ✅ YES
