# OTA Deployment Guide - Step by Step

**Document Version:** 1.2 **Date:** December 10, 2025 **Firmware Version:**
2.5.34

Panduan lengkap untuk deploy OTA firmware update pada SRT-MGATE-1210 Gateway.

---

## Table of Contents

1. [Prerequisites](#1-prerequisites)
2. [Generate OTA Keys](#2-generate-ota-keys)
3. [Sign Firmware](#3-sign-firmware)
4. [Create GitHub Release](#4-create-github-release)
5. [Setup Manifest di Repository](#5-setup-manifest-di-repository)
6. [Konfigurasi Device via BLE](#6-konfigurasi-device-via-ble)
7. [Testing OTA Update](#7-testing-ota-update)
8. [Troubleshooting](#8-troubleshooting)

---

## 1. Prerequisites

### Software Requirements

- Python 3.x dengan `ecdsa` module
- GitHub CLI (`gh`)
- Arduino IDE (untuk compile firmware)
- BLE testing tool (Python script atau nRF Connect)

### Install Dependencies

```bash
# Install Python ecdsa module
pip install ecdsa

# Install GitHub CLI (Windows)
winget install GitHub.cli

# Login ke GitHub
gh auth login --web --git-protocol https
```

### File Structure

```
GatewaySuriotaPOC/
├── Main/
│   └── build/esp32.esp32.esp32s3/
│       └── Main.ino.bin          # Compiled firmware
├── Tools/
│   ├── OTA_Keys/
│   │   ├── ota_private_key.pem   # PRIVATE KEY (JANGAN COMMIT!)
│   │   ├── ota_public_key.pem    # Public key
│   │   └── ota_public_key.h      # C header for firmware
│   ├── generate_ota_keys.py      # Key generator
│   └── sign_firmware.py          # Firmware signer
├── releases/
│   └── v{VERSION}/
│       └── SRT-MGATE-1210_v{VERSION}.bin  # Signed binary (upload to GitHub Releases)
└── firmware_manifest.json        # OTA manifest (fetched via raw.githubusercontent.com)
```

---

## 2. Generate OTA Keys

**Jalankan sekali saja** saat pertama kali setup OTA. Key pair ini akan
digunakan untuk semua firmware release.

### Command

```bash
cd Tools
python generate_ota_keys.py
```

### Output

```
============================================================
OTA Key Generator for SRT-MGATE-1210
============================================================

[1/4] Generating ECDSA P-256 private key...
[2/4] Saving private key: .\ota_private_key.pem
[3/4] Saving public key: .\ota_public_key.pem
[4/4] Generating C header: .\ota_public_key.h

============================================================
KEY GENERATION COMPLETE!
============================================================

Files created:
  1. .\ota_private_key.pem  <- KEEP THIS SECRET!
  2. .\ota_public_key.pem
  3. .\ota_public_key.h        <- Copy to firmware

Public Key (hex):
0485c84ede29e014a17ae7bd9c904d6fd92115e269e0ecc8248533bd952f8e28ba...
```

### Files Generated

| File                  | Deskripsi                 | Commit ke Git?      |
| --------------------- | ------------------------- | ------------------- |
| `ota_private_key.pem` | Private key untuk signing | **TIDAK! RAHASIA!** |
| `ota_public_key.pem`  | Public key                | Ya                  |
| `ota_public_key.h`    | C header untuk firmware   | Ya                  |

### Backup Private Key

```bash
# Backup ke lokasi aman
mkdir -p ~/Documents/OTA_Keys_Backup
cp ota_private_key.pem ~/Documents/OTA_Keys_Backup/
cp ota_public_key.pem ~/Documents/OTA_Keys_Backup/
cp ota_public_key.h ~/Documents/OTA_Keys_Backup/
```

### Update Firmware dengan Public Key

Copy content dari `ota_public_key.h` ke `Main/OTAValidator.cpp`:

```cpp
static const char* DEFAULT_PUBLIC_KEY_PEM = R"(
-----BEGIN PUBLIC KEY-----
MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEhchO3ingFKF6572ckE1v2SEV4mng7MgkhTO9lS+O
KLr5ryCfkRlgBkNFzf2ZZzrWD9mnhoOfHZbvxF5lSOu/Fg==
-----END PUBLIC KEY-----
)";
```

---

## 3. Sign Firmware

Setelah compile firmware baru, sign dengan private key.

### Command

```bash
python Tools/sign_firmware.py <firmware.bin> <private_key.pem> <version>
```

### Example

```bash
python Tools/sign_firmware.py \
  "Main/build/esp32.esp32.esp32s3/Main.ino.bin" \
  "ota_private_key.pem" \
  "2.5.0"
```

### Output

```
============================================================
OTA Firmware Signing Tool for SRT-MGATE-1210
============================================================

[1/5] Loading private key: ota_private_key.pem
[2/5] Reading firmware: Main/build/esp32.esp32.esp32s3/Main.ino.bin
       Firmware size: 1,891,968 bytes (1.80 MB)
[3/5] Calculating SHA-256 checksum...
       SHA-256: c5964cae5f7b0e57f4e2ce75a6e9d5804c29a4662d6a7ec57fdae5b6220fe710
[4/5] Signing firmware with ECDSA P-256 (DER format)...
       Signature (71 bytes DER): 3045022057c665a0b3bc5287d618ea...
[5/5] Generating firmware_manifest.json...
       Saved: Main/build/esp32.esp32.esp32s3/firmware_manifest.json

============================================================
SIGNING COMPLETE!
============================================================

Verifying signature...
Signature verification: PASSED
```

**IMPORTANT:** Signature must be in DER format (70-72 bytes). If you see 64
bytes, the signing script needs to be updated with `sigencode=sigencode_der`
parameter. See v2.5.10 fix for details.

### Generated Manifest

```json
{
  "version": "2.5.10",
  "build_number": 25010,
  "release_date": "2025-11-28",
  "min_version": "2.3.0",
  "firmware": {
    "filename": "Main.ino.bin",
    "size": 1891968,
    "sha256": "c5964cae5f7b0e57f4e2ce75a6e9d5804c29a4662d6a7ec57fdae5b6220fe710",
    "signature": "3045022057c665a0b3bc5287d618ea4d9c1cf655b611b5362af04cb6c55fc0e66c75ce28022100998bf5103d6a593f42de0cd0bb1cbfea225842be2cae6df8b0d51b984c1dc87f"
  },
  "changelog": [
    "Fixed OTA signature verification (v2.5.10)",
    "DER format signature support"
  ],
  "mandatory": false
}
```

**Signature Format Notes:**

- Format: DER encoded (starts with `3044` or `3045`)
- Size: 70-72 bytes (140-144 hex characters)
- Encoding: Hexadecimal string (NOT base64)

---

## 4. Create GitHub Release

### Command

```bash
gh release create v2.5.0 \
  "Main/build/esp32.esp32.esp32s3/Main.ino.bin" \
  "Main/build/esp32.esp32.esp32s3/firmware_manifest.json" \
  --title "v2.5.0 - OTA Support" \
  --notes "## What's New in v2.5.0

### OTA Firmware Update System
- HTTPS OTA from GitHub Releases
- BLE OTA direct transfer
- ECDSA P-256 signature verification
- Anti-rollback protection

### Files
- Main.ino.bin: Firmware binary
- firmware_manifest.json: Signed manifest"
```

### Output

```
https://github.com/GifariKemal/GatewaySuriotaPOC/releases/tag/v2.5.0
```

---

## 5. Setup Manifest di Repository

Device mencari manifest di `raw.githubusercontent.com`. Perlu commit manifest ke
repository.

### Update Manifest dengan URL

Edit `firmware_manifest.json` dan tambahkan URL firmware:

```json
{
  "version": "2.5.0",
  "build_number": 2500,
  "release_date": "2025-11-27",
  "min_version": "2.3.0",
  "firmware": {
    "filename": "Main.ino.bin",
    "url": "https://github.com/GifariKemal/GatewaySuriotaPOC/releases/download/v2.5.0/Main.ino.bin",
    "size": 1891968,
    "sha256": "c5964cae5f7b0e57f4e2ce75a6e9d5804c29a4662d6a7ec57fdae5b6220fe710",
    "signature": "9d23e08ab120fe5604f53746ddd34a920299003dabc562fea1e896217fb8dd7f..."
  },
  "changelog": [
    "Added OTA firmware update system",
    "HTTPS OTA from GitHub Releases",
    "BLE OTA direct transfer",
    "ECDSA P-256 signature verification"
  ],
  "mandatory": false
}
```

### Copy dan Commit

```bash
# Copy manifest ke root dan ota folder
cp Main/build/esp32.esp32.esp32s3/firmware_manifest.json ./
mkdir -p ota
cp firmware_manifest.json ota/

# Commit dan push
git add firmware_manifest.json ota/
git commit -m "Add OTA manifest for v2.5.0"
git push
```

### Manifest URL

Setelah push, manifest tersedia di:

```
https://raw.githubusercontent.com/GifariKemal/GatewaySuriotaPOC/main/firmware_manifest.json
```

---

## 6. Konfigurasi Device via BLE

Gunakan BLE testing tool untuk konfigurasi device.

### Set GitHub Token (untuk private repo)

```json
{
  "op": "ota",
  "type": "set_github_token",
  "token": "ghp_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
}
```

**Response:**

```json
{
  "status": "ok",
  "command": "set_github_token",
  "message": "GitHub token configured",
  "token_length": 40
}
```

### Set GitHub Repository

```json
{
  "op": "ota",
  "type": "set_github_repo",
  "owner": "GifariKemal",
  "repo": "GatewaySuriotaPOC",
  "branch": "main"
}
```

**Response:**

```json
{
  "status": "ok",
  "command": "set_github_repo",
  "message": "GitHub repository configured",
  "owner": "GifariKemal",
  "repo": "GatewaySuriotaPOC",
  "branch": "main"
}
```

---

## 7. Testing OTA Update

### Check for Update

```json
{ "op": "ota", "type": "check_update" }
```

**Response (no update):**

```json
{
  "status": "ok",
  "command": "check_update",
  "update_available": false,
  "current_version": "2.5.0"
}
```

**Response (update available):**

```json
{
  "status": "ok",
  "command": "check_update",
  "update_available": true,
  "current_version": "2.5.0",
  "new_version": "2.6.0",
  "size": 1900000,
  "mandatory": false
}
```

### Start Update

```json
{ "op": "ota", "type": "start_update" }
```

**Response (success):**

```json
{
  "status": "ok",
  "command": "start_update",
  "message": "Update started",
  "version": "2.6.0"
}
```

### Check OTA Status

```json
{ "op": "ota", "type": "ota_status" }
```

**Response:**

```json
{
  "status": "ok",
  "command": "ota_status",
  "state": "downloading",
  "progress": 45,
  "bytes_downloaded": 850000,
  "total_bytes": 1891968,
  "current_version": "2.5.0",
  "target_version": "2.6.0"
}
```

### Apply Update (Reboot)

```json
{ "op": "ota", "type": "apply_update" }
```

---

## 8. Troubleshooting

### Error: "Manifest fetch failed: HTTP -1"

**Penyebab:**

- WiFi/Ethernet tidak terhubung
- DNS resolution gagal
- SSL handshake failed

**Solusi:**

1. Cek koneksi network:

   ```json
   { "op": "read", "type": "network_status" }
   ```

2. Pastikan device terhubung ke internet

3. Cek Serial Monitor untuk log detail

### Error: "update_available: false" padahal ada versi baru

**Penyebab:**

- Manifest belum di-push ke repository
- Cache belum expire

**Solusi:**

1. Verifikasi manifest di browser:

   ```
   https://raw.githubusercontent.com/GifariKemal/GatewaySuriotaPOC/main/firmware_manifest.json
   ```

2. Pastikan version di manifest lebih tinggi dari current_version

### Error: "Signature verification failed"

**Penyebab:**

- Firmware tidak di-sign dengan private key yang benar
- Public key di firmware tidak match

**Solusi:**

1. Re-sign firmware dengan private key yang benar
2. Pastikan public key di `OTAValidator.cpp` match dengan private key

### Error: "Downgrade rejected"

**Penyebab:**

- Trying to install older version

**Solusi:**

- OTA hanya bisa ke versi lebih tinggi (anti-rollback)

---

## Quick Reference

### OTA BLE Commands

| Command            | Deskripsi                    |
| ------------------ | ---------------------------- |
| `check_update`     | Cek update tersedia          |
| `start_update`     | Mulai download               |
| `ota_status`       | Status download              |
| `abort_update`     | Batalkan update              |
| `apply_update`     | Apply dan reboot             |
| `enable_ble_ota`   | Enable BLE OTA mode          |
| `disable_ble_ota`  | Disable BLE OTA mode         |
| `rollback`         | Rollback ke versi sebelumnya |
| `get_config`       | Get OTA configuration        |
| `set_github_repo`  | Set repository               |
| `set_github_token` | Set auth token               |

### Release Checklist

- [ ] Compile firmware baru
- [ ] Update version di `Main.ino`
- [ ] Sign firmware: `python sign_firmware.py ...`
- [ ] Create GitHub Release: `gh release create ...`
- [ ] Update manifest dengan URL
- [ ] Commit manifest ke repository
- [ ] Test `check_update` via BLE
- [ ] Test full OTA flow

---

## Files Reference

| File            | Lokasi                         | Deskripsi                  |
| --------------- | ------------------------------ | -------------------------- |
| Private Key     | `ota_private_key.pem`          | **RAHASIA!** Untuk signing |
| Public Key      | `ota_public_key.pem`           | Untuk verifikasi           |
| C Header        | `ota_public_key.h`             | Embed di firmware          |
| Key Generator   | `Tools/generate_ota_keys.py`   | Generate key pair          |
| Firmware Signer | `Tools/sign_firmware.py`       | Sign firmware              |
| Manifest        | `firmware_manifest.json`       | Info firmware              |
| Backup Keys     | `~/Documents/OTA_Keys_Backup/` | Backup lokasi              |

---

**Document End**
