# 🚀 OTA Release Procedure (Private Repository)

This guide details the correct procedure for releasing firmware updates for the
SRT-MGATE-1210 gateway, specifically tailored for **PRIVATE GitHub
Repositories**.

> **⚠️ CRITICAL FOR PRIVATE REPOS:** You CANNOT use standard GitHub Release URLs
> (e.g., `github.com/.../releases/download/...`). You MUST use the **GitHub API
> URL** in the manifest to allow token-based authentication.

---

## 1. Prerequisites

- **Firmware Binary**: Compiled `.bin` file (e.g., `Main.ino.bin`).
- **Signing Tools**: `Tools/sign_firmware.py` and your private key
  (`ota_private_key.pem`).
- **OTA Repository**: Access to `GatewaySuriotaOTA` (or equivalent).
- **Python Environment**: `ecdsa` library installed (`pip install ecdsa`).

---

## 2. Release Steps

### Step 1: Compile Firmware

Compile the firmware in Arduino IDE or PlatformIO.

- **Output**: `Main/build/esp32.esp32.esp32s3/Main.ino.bin`

### Step 2: Sign Firmware

Run the signing script to generate the signature and initial manifest.

```bash
python Tools/sign_firmware.py Main/build/esp32.esp32.esp32s3/Main.ino.bin Tools/OTA_Keys/ota_private_key.pem
```

- Enter the version number when prompted (e.g., `2.5.19`).
- **Note**: The script does NOT create a `.signed.bin` file. It signs the input
  file and outputs the signature to the console and `firmware_manifest.json`.

### Step 3: Prepare OTA Repository

1. Navigate to your OTA repository.
2. Create a folder for the new version:
   ```bash
   mkdir -p releases/v2.5.19
   ```
3. **Copy the binary** to this folder:
   ```bash
   cp /path/to/POC/Main/build/.../Main.ino.bin releases/v2.5.19/firmware.bin
   ```
4. **Commit and Push** the binary to the OTA repository:
   ```bash
   git add releases/v2.5.19/firmware.bin
   git commit -m "feat: Add firmware binary v2.5.19"
   git push origin main
   ```

### Step 4: Configure Manifest (CRITICAL STEP)

Edit `firmware_manifest.json` in the root of the OTA repository.

**Required Fields:**

- `version`: New version (e.g., `2.5.20` if testing update from 2.5.19).
- `url`: **MUST BE THE API URL** (see below).
- `signature`: From Step 2 output.
- `sha256`: From Step 2 output.
- `size`: From Step 2 output.

**✅ CORRECT URL FORMAT (Private Repo):**

```json
"url": "https://api.github.com/repos/{OWNER}/{REPO}/contents/releases/{VERSION}/firmware.bin?ref=main"
```

**Example:**

```json
{
  "version": "2.5.20",
  "build_number": 2520,
  "release_date": "2025-12-01",
  "min_version": "2.3.0",
  "firmware": {
    "url": "https://api.github.com/repos/GifariKemal/GatewaySuriotaOTA/contents/releases/v2.5.19/firmware.bin?ref=main",
    "filename": "firmware.bin",
    "size": 2004880,
    "sha256": "b2a2c88140fe9d4a...",
    "signature": "304402201ff4c27d..."
  },
  "changelog": ["v2.5.20: Critical Fixes & MQTT Optimization"],
  "mandatory": false
}
```

> **Why API URL?** The firmware uses the `Authorization: token {token}` header.
>
> - `github.com` release URLs do NOT accept this header (returns 404).
> - `api.github.com` contents endpoint DOES accept this header and returns the
>   raw binary file (with `Accept: application/vnd.github.v3.raw`).

### Step 5: Deploy Manifest

Commit and push the updated `firmware_manifest.json` to the OTA repository.

```bash
git add firmware_manifest.json
git commit -m "release: Update manifest to v2.5.20"
git push origin main
```

---

## 3. Testing the Update

1. **Check Update**: Send BLE command `{"op":"ota","type":"check_update"}`.
   - Should return: `Update available: v2.5.20`.
2. **Start Update**: Send BLE command `{"op":"ota","type":"start_update"}`.
   - Should log: `Fetching manifest...` -> `Manifest parsed` ->
     `Starting download...`.
   - **Success**: `[OTA] OTA finalized, boot partition set`.
   - **Failure (404)**: Check if URL is API format and binary exists in repo.
   - **Failure (Signature)**: Check if binary in repo matches the one signed.

---

## 4. Troubleshooting

| Error                    | Cause                            | Solution                                                                       |
| ------------------------ | -------------------------------- | ------------------------------------------------------------------------------ |
| **HTTP 404**             | Wrong URL format or file missing | Use `api.github.com/.../contents/...` format. Ensure binary is pushed to repo. |
| **Signature Failed**     | Binary mismatch                  | Ensure the binary you signed is EXACTLY the same file you pushed to the repo.  |
| **Manifest Parse Error** | Invalid JSON                     | Check JSON syntax (commas, quotes).                                            |
| **Network Error**        | No internet/DNS                  | Check WiFi/Ethernet connection.                                                |

---

## 5. Summary Checklist

- [ ] Binary compiled
- [ ] Binary signed (Signature saved)
- [ ] Binary copied to `releases/vX.Y.Z/firmware.bin`
- [ ] Binary PUSHED to OTA repo
- [ ] Manifest updated with **API URL**
- [ ] Manifest PUSHED to OTA repo
