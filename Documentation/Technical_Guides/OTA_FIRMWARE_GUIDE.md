# OTA Firmware Preparation & Deployment Guide

**Version:** 2.5.34 **Last Updated:** December 10, 2025

Panduan lengkap untuk menyiapkan firmware .bin, upload ke GitHub, dan deployment
OTA update ke device.

---

## Table of Contents

1. [Prerequisites](#1-prerequisites)
2. [Compile Firmware .bin](#2-compile-firmware-bin)
3. [Generate Security Files](#3-generate-security-files)
4. [Create Manifest File](#4-create-manifest-file)
5. [Upload to GitHub](#5-upload-to-github)
6. [Deploy OTA Update](#6-deploy-ota-update)
7. [Troubleshooting](#7-troubleshooting)

---

## 1. Prerequisites

### Tools Required

| Tool                  | Purpose             | Installation                                     |
| --------------------- | ------------------- | ------------------------------------------------ |
| Arduino IDE 2.x       | Compile firmware    | [arduino.cc](https://www.arduino.cc/en/software) |
| Python 3.8+           | Generate signatures | `python.org`                                     |
| Git                   | Version control     | `git-scm.com`                                    |
| GitHub CLI (optional) | Upload releases     | `gh` command                                     |

### Python Dependencies

```bash
pip install ecdsa
```

> Note: `hashlib` sudah built-in di Python 3, tidak perlu di-install.

### OpenSSL Alternatives untuk Windows

OpenSSL TIDAK diperlukan jika menggunakan Python scripts yang disediakan di
folder `Tools/`:

- `generate_ota_keys.py` - Generate ECDSA key pair
- `sign_firmware.py` - Sign firmware binary

Jika tetap ingin menggunakan OpenSSL di Windows:

1. **Git Bash** - Sudah include OpenSSL (jalankan dari Git Bash)
2. **WSL** - Windows Subsystem for Linux
3. **Download** - https://slproweb.com/products/Win32OpenSSL.html

### Board Configuration (Arduino IDE)

```
Board: ESP32-S3 Dev Module
Flash Size: 16MB (128Mb)
Partition Scheme: Custom (see partitions.csv)
PSRAM: OPI PSRAM
Upload Speed: 921600
CPU Frequency: 240MHz
```

---

## 2. Compile Firmware .bin

### Step 2.1: Update Version Number

Edit `Main/Main.ino`:

```cpp
// Update version sebelum compile
#define FIRMWARE_VERSION "2.5.0"  // Increment dari versi sebelumnya
```

Edit OTA initialization (juga di `Main.ino`):

```cpp
otaManager->setCurrentVersion(FIRMWARE_VERSION, 2500); // 2.5.0 = 2500
```

### Step 2.2: Set Production Mode

Edit `Main/DebugConfig.h`:

```cpp
#define PRODUCTION_MODE 1  // 1 = Production (smaller binary, less logs)
```

### Step 2.3: Compile Binary

**Arduino IDE:**

1. Buka `Main/Main.ino`
2. Menu: **Sketch** > **Export Compiled Binary**
3. Tunggu kompilasi selesai
4. File .bin tersimpan di folder `Main/build/`

**Atau via Arduino CLI:**

```bash
# Compile
arduino-cli compile --fqbn esp32:esp32:esp32s3 Main/Main.ino --output-dir ./build

# File output:
# ./build/Main.ino.bin          <- File ini yang digunakan untuk OTA
# ./build/Main.ino.bootloader.bin
# ./build/Main.ino.partitions.bin
```

### Step 2.4: Verify Binary Size

```bash
# Linux/Mac
ls -lh build/Main.ino.bin

# Windows PowerShell
Get-Item build\Main.ino.bin | Select-Object Name, Length

# Output contoh:
# Main.ino.bin    1,892,352 bytes (1.8 MB)
```

**Maximum Size:** 4MB (sesuai partition OTA)

---

## 3. Generate Security Files

### Step 3.1: Generate ECDSA Key Pair (One-Time Setup)

**Menggunakan Python Script (Recommended untuk Windows):**

```powershell
cd Tools
python generate_ota_keys.py
```

Script akan generate 3 file:

- `ota_private_key.pem` - **SIMPAN DENGAN AMAN!**
- `ota_public_key.pem` - Public key
- `ota_public_key.h` - C header siap copy ke firmware

**Alternatif: Menggunakan OpenSSL (Git Bash / Linux / Mac):**

```bash
# Generate private key (SIMPAN DENGAN AMAN!)
openssl ecparam -name prime256v1 -genkey -noout -out ota_private_key.pem

# Extract public key
openssl ec -in ota_private_key.pem -pubout -out ota_public_key.pem

# View public key (untuk embed di firmware)
openssl ec -in ota_public_key.pem -pubin -text -noout
```

**Output public key:**

```
Public-Key: (256 bit)
pub:
    04:a1:b2:c3:d4:e5:f6:...  <- 65 bytes uncompressed
ASN1 OID: prime256v1
```

### Step 3.2: Embed Public Key di Firmware

Edit `Main/OTAConfig.h`:

```cpp
// Public key untuk verifikasi signature (65 bytes, uncompressed format)
static const uint8_t OTA_PUBLIC_KEY[] = {
    0x04,  // Uncompressed point indicator
    // X coordinate (32 bytes)
    0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6, 0x07, 0x18,
    0x29, 0x3a, 0x4b, 0x5c, 0x6d, 0x7e, 0x8f, 0x90,
    0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6, 0x07, 0x18,
    0x29, 0x3a, 0x4b, 0x5c, 0x6d, 0x7e, 0x8f, 0x90,
    // Y coordinate (32 bytes)
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
    0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00,
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
    0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00
};
```

### Step 3.3: Generate SHA-256 Checksum

```bash
# Linux/Mac
sha256sum build/Main.ino.bin

# Windows PowerShell
Get-FileHash -Algorithm SHA256 build\Main.ino.bin

# Output:
# a1b2c3d4e5f6...  build/Main.ino.bin
```

### Step 3.4: Sign Firmware

**IMPORTANT: Use the correct signing approach to avoid double-hash bug!**

**Python Script: `Tools/sign_firmware.py`**

```python
#!/usr/bin/env python3
"""
OTA Firmware Signing Tool for SRT-MGATE-1210
Generates ECDSA P-256 signature in DER format

CRITICAL: Uses sign_deterministic with hashfunc parameter
to avoid double-hashing bug. The Python ecdsa library
internally hashes data passed to sign_deterministic().

v2.5.10 Fix: Pass firmware_data directly, not firmware_hash!
"""

import hashlib
import sys
import os
import json
from datetime import datetime
from ecdsa import SigningKey, NIST256p
from ecdsa.util import sigencode_der

def sign_firmware(firmware_path, private_key_path, version=None):
    # Load private key
    with open(private_key_path, 'r') as f:
        private_key = SigningKey.from_pem(f.read())

    # Read firmware binary
    with open(firmware_path, 'rb') as f:
        firmware_data = f.read()

    firmware_size = len(firmware_data)
    print(f"Firmware size: {firmware_size} bytes ({firmware_size/1024/1024:.2f} MB)")

    # Calculate SHA-256 hash (for manifest, NOT for signing)
    firmware_hash = hashlib.sha256(firmware_data).hexdigest()
    print(f"SHA-256: {firmware_hash}")

    # CRITICAL: Sign the RAW firmware data with hashfunc parameter
    # Python ecdsa will hash internally - DO NOT pre-hash!
    signature = private_key.sign_deterministic(
        firmware_data,              # Raw data, NOT hash!
        hashfunc=hashlib.sha256,    # Library hashes internally
        sigencode=sigencode_der     # DER format (70-72 bytes)
    )

    sig_hex = signature.hex()
    print(f"Signature ({len(signature)} bytes DER): {sig_hex[:40]}...{sig_hex[-20:]}")

    # Output for manifest
    print("\n=== For firmware_manifest.json ===")
    print(f'"size": {firmware_size},')
    print(f'"sha256": "{firmware_hash}",')
    print(f'"signature": "{sig_hex}"')

    return firmware_hash, sig_hex, firmware_size

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python sign_firmware.py <firmware.bin> <private_key.pem> [version]")
        sys.exit(1)

    version = sys.argv[3] if len(sys.argv) > 3 else "1.0.0"
    sign_firmware(sys.argv[1], sys.argv[2], version)
```

**Jalankan:**

```bash
python Tools/sign_firmware.py "Main/build/esp32.esp32.esp32s3/Main.ino.bin" "Tools/OTA_Keys/ota_private_key.pem" "2.5.10"

# Output:
# Firmware size: 1891968 bytes (1.80 MB)
# SHA-256: c5964cae5f7b0e57f4e2ce75a6e9d5804c29a4662d6a7ec57fdae5b6220fe710
# Signature (71 bytes DER): 3045022057c665a0b3bc5287d618ea...022100998bf5103d6a593f42de0cd0
#
# === For firmware_manifest.json ===
# "size": 1891968,
# "sha256": "c5964cae5f7b0e57f4e2ce75a6e9d5804c29a4662d6a7ec57fdae5b6220fe710",
# "signature": "3045022057c665a0b3bc5287d618ea4d9c1cf655b611b5362af04cb6c55fc0e66c75ce28022100998bf5103d6a593f42de0cd0bb1cbfea225842be2cae6df8b0d51b984c1dc87f"
```

**Signature Format:** | Property | Value | |----------|-------| | Algorithm |
ECDSA P-256 (secp256r1) | | Format | DER encoded | | Size | 70-72 bytes
(variable) | | Encoding | Hexadecimal string |

---

## 4. Create Manifest File

### firmware_manifest.json

```json
{
  "version": "2.5.0",
  "build_number": 2500,
  "release_date": "2025-11-27",
  "min_version": "2.3.0",
  "firmware_file": "firmware.bin",
  "size": 1892352,
  "sha256": "a1b2c3d4e5f6078192a3b4c5d6e7f8091a2b3c4d5e6f7081920a1b2c3d4e5f607",
  "signature": "304402201a2b3c4d5e6f708192a3b4c5d6e7f8091a2b3c4d5e6f7081920a1b2c3d4e5f60702201a2b3c4d5e6f708192a3b4c5d6e7f8091a2b3c4d5e6f7081920a1b2c3d4e5f607",
  "release_notes": "OTA Update System - New Features:\n- HTTPS OTA from GitHub\n- BLE OTA direct transfer\n- Automatic rollback protection",
  "mandatory": false,
  "changelog_url": "https://github.com/GifariKemal/GatewaySuriotaPOC/releases/tag/v2.5.0"
}
```

### Manifest Fields Explanation

| Field           | Required | Description                                           |
| --------------- | -------- | ----------------------------------------------------- |
| `version`       | Yes      | Semantic version (e.g., "2.5.0")                      |
| `build_number`  | Yes      | Numeric build (e.g., 2500)                            |
| `release_date`  | Yes      | ISO date format                                       |
| `min_version`   | No       | Minimum required version to update from               |
| `firmware_file` | Yes      | Filename of .bin file                                 |
| `size`          | Yes      | Exact file size in bytes                              |
| `sha256`        | Yes      | SHA-256 hash of firmware                              |
| `signature`     | No\*     | ECDSA signature (\*required if verify_signature=true) |
| `release_notes` | No       | Human-readable changelog                              |
| `mandatory`     | No       | Force update (default: false)                         |
| `changelog_url` | No       | Link to full changelog                                |

---

## 5. Upload to GitHub

### Option A: GitHub Releases (Recommended)

#### Step 5.1: Create Release Tag

```bash
# Commit changes
git add .
git commit -m "Release v2.5.0 - OTA Update System"

# Create tag
git tag -a v2.5.0 -m "Version 2.5.0 - OTA Update System"

# Push to GitHub
git push origin main
git push origin v2.5.0
```

#### Step 5.2: Create GitHub Release

**Via GitHub Web UI:**

1. Go to repository > **Releases** > **Create new release**
2. Select tag: `v2.5.0`
3. Release title: `v2.5.0 - OTA Update System`
4. Description: Copy from release_notes
5. **Attach files:**
   - `firmware.bin` (rename dari Main.ino.bin)
   - `firmware_manifest.json`
6. Click **Publish release**

**Via GitHub CLI:**

```bash
# Rename file
cp build/Main.ino.bin firmware.bin

# Create release with assets
gh release create v2.5.0 \
    firmware.bin \
    firmware_manifest.json \
    --title "v2.5.0 - OTA Update System" \
    --notes "OTA Update System release"
```

#### Step 5.3: Verify Release URLs

```
Firmware URL (in manifest):
https://github.com/GifariKemal/GatewaySuriotaPOC/releases/download/v2.5.0/SRT-MGATE-1210_v2.5.0.bin
```

### Option B: Raw Files (Manifest)

OTA system fetches manifest from repository root via raw.githubusercontent.com:

#### Repository Structure

```
repository/
├── firmware_manifest.json      # Main manifest (root)
├── releases/
│   └── v{VERSION}/
│       └── SRT-MGATE-1210_v{VERSION}.bin  # Binary for GitHub Releases
├── Main/
│   └── ...
└── ...
```

#### Update and Push Manifest

```bash
# Edit firmware_manifest.json at root with new version info
git add firmware_manifest.json
git commit -m "Update OTA manifest for v2.5.0"
git push origin main
```

#### Raw File URL

```
Manifest URL:
https://raw.githubusercontent.com/GifariKemal/GatewaySuriotaPOC/main/firmware_manifest.json

Firmware URL (from GitHub Releases):
https://github.com/GifariKemal/GatewaySuriotaPOC/releases/download/v2.5.0/SRT-MGATE-1210_v2.5.0.bin
```

### Private Repository Setup

#### Step 5.4: Generate Personal Access Token

1. GitHub > **Settings** > **Developer settings** > **Personal access tokens**
2. Click **Generate new token (classic)**
3. Select scopes: `repo` (full control)
4. Copy token: `ghp_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx`

#### Step 5.5: Configure Device

Via BLE:

```json
{
  "op": "ota",
  "type": "set_github_token",
  "token": "ghp_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
}
```

---

## 6. Deploy OTA Update

### Step 6.1: Configure Device (First Time)

```json
// Set GitHub repository
{"op":"ota","type":"set_github_repo","owner":"GifariKemal","repo":"GatewaySuriotaPOC","branch":"main"}

// (Optional) Set token untuk private repo
{"op":"ota","type":"set_github_token","token":"ghp_xxx..."}
```

### Step 6.2: Check for Update

```json
{ "op": "ota", "type": "check_update" }
```

**Response:**

```json
{
  "status": "ok",
  "update_available": true,
  "current_version": "2.4.0",
  "target_version": "2.5.0",
  "manifest": {
    "version": "2.5.0",
    "size": 1892352,
    "release_notes": "OTA Update System..."
  }
}
```

### Step 6.3: Start Download

```json
{ "op": "ota", "type": "start_update" }
```

### Step 6.4: Monitor Progress

```json
{ "op": "ota", "type": "ota_status" }
```

**Response (downloading):**

```json
{
  "state": "downloading",
  "progress": 45,
  "bytes_downloaded": 851558,
  "total_bytes": 1892352
}
```

**Response (validated):**

```json
{
  "state": "idle",
  "progress": 100,
  "update_available": true
}
```

### Step 6.5: Apply Update

```json
{ "op": "ota", "type": "apply_update" }
```

Device akan reboot dengan firmware baru.

### Step 6.6: Verify Update

Setelah reboot:

```json
{ "op": "ota", "type": "ota_status" }
```

**Response:**

```json
{
  "state": "idle",
  "current_version": "2.5.0",
  "update_available": false
}
```

---

## 7. Troubleshooting

### Error: Network not available

```json
{ "status": "error", "error_code": 1, "error_message": "Network not available" }
```

**Solution:**

- Pastikan WiFi atau Ethernet terhubung
- Check `{"op":"read","type":"network_status"}`

### Error: HTTP request failed

```json
{ "status": "error", "error_code": 2, "error_message": "HTTP error: 404" }
```

**Solution:**

- Verify manifest URL accessible
- Check GitHub token untuk private repo
- Pastikan file ada di GitHub

### Error: Signature verification failed

```json
{
  "status": "error",
  "error_code": 5,
  "error_message": "Signature verification failed"
}
```

**Common Causes & Solutions:**

1. **Double-Hash Bug (Most Common - Fixed in v2.5.10):**

   ```python
   # WRONG - Python ecdsa hashes internally, causing double-hash:
   firmware_hash = hashlib.sha256(firmware_data).digest()
   signature = private_key.sign_deterministic(firmware_hash)

   # CORRECT - Pass raw data with hashfunc parameter:
   signature = private_key.sign_deterministic(firmware_data, hashfunc=hashlib.sha256, sigencode=sigencode_der)
   ```

2. **Wrong Signature Format:**
   - Must use DER format (`sigencode=sigencode_der`)
   - Size should be 70-72 bytes (not 64 bytes raw)
   - Must be hex encoded (not base64)

3. **Key Mismatch:**
   - Verify public key di firmware match dengan private key yang digunakan

4. **File Corruption:**
   - Re-download firmware dan re-sign

### Error: SHA-256 checksum mismatch

```json
{ "status": "error", "error_code": 4, "error_message": "Checksum mismatch" }
```

**Solution:**

- Re-calculate SHA-256 dari file yang di-upload
- Pastikan file tidak corrupt saat upload
- Verify `size` di manifest cocok

### Error: No previous firmware available (Rollback)

```json
{ "status": "error", "error_message": "No previous firmware available" }
```

**Solution:**

- Rollback hanya tersedia setelah minimal 1x OTA update
- Factory rollback selalu tersedia

### Download Stuck / Timeout

**Solution:**

- Check WiFi signal strength
- Try abort and restart: `{"op":"ota","type":"abort_update"}`
- Check free heap memory
- Restart device dan coba lagi

---

## Complete Workflow Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                     OTA FIRMWARE DEPLOYMENT                          │
└─────────────────────────────────────────────────────────────────────┘

Developer Workstation                    GitHub                    Device
        │                                   │                        │
        │  1. Update version number         │                        │
        │  2. Compile firmware              │                        │
        │     arduino-cli compile           │                        │
        │                                   │                        │
        │  3. Generate SHA-256              │                        │
        │     sha256sum firmware.bin        │                        │
        │                                   │                        │
        │  4. Sign firmware                 │                        │
        │     python sign_firmware.py       │                        │
        │                                   │                        │
        │  5. Create manifest.json          │                        │
        │                                   │                        │
        │  6. Upload to GitHub              │                        │
        │ ─────────────────────────────────>│                        │
        │     git push / gh release         │                        │
        │                                   │                        │
        │                                   │  7. check_update       │
        │                                   │<───────────────────────│
        │                                   │                        │
        │                                   │  8. Return manifest    │
        │                                   │───────────────────────>│
        │                                   │                        │
        │                                   │  9. start_update       │
        │                                   │<───────────────────────│
        │                                   │                        │
        │                                   │  10. Download firmware │
        │                                   │───────────────────────>│
        │                                   │     (streaming)        │
        │                                   │                        │
        │                                   │  11. Verify signature  │
        │                                   │         & checksum     │
        │                                   │                        │
        │                                   │  12. apply_update      │
        │                                   │                        │
        │                                   │  13. Reboot            │
        │                                   │         ↺              │
        │                                   │                        │
        │                                   │  14. Boot validation   │
        │                                   │                        │
        │                                   │  15. Mark firmware OK  │
        │                                   │                        │
```

---

## File Checklist

Before release, verify all files:

- [ ] `Main.ino` - Version updated
- [ ] `DebugConfig.h` - PRODUCTION_MODE = 1
- [ ] `OTAConfig.h` - Public key embedded
- [ ] `firmware.bin` - Compiled successfully
- [ ] `firmware.bin.sig` - Signature generated
- [ ] `firmware_manifest.json` - All fields correct
- [ ] GitHub Release/Raw files - Uploaded and accessible

---

## Security Best Practices

1. **Private Key:** Simpan `ota_private_key.pem` dengan SANGAT AMAN
   - Jangan commit ke Git
   - Gunakan hardware security module untuk production
   - Backup securely offline

2. **GitHub Token:** Untuk private repo
   - Gunakan token dengan minimum scope
   - Rotate token secara berkala
   - Jangan hardcode di source code

3. **Signature Verification:** Selalu enable di production
   - Mencegah firmware palsu/modified
   - Wajib untuk deployment production

4. **Version Control:** Gunakan semantic versioning
   - Anti-rollback protection mencegah downgrade
   - `min_version` untuk compatibility

---

## Related Documentation

- [BLE OTA API Reference](../API_Reference/BLE_OTA_UPDATE.md)
- [OTA Architecture](OTA_UPDATE.md)
- [Network Configuration](NETWORK_CONFIGURATION.md)

---

**End of OTA Firmware Preparation & Deployment Guide**
