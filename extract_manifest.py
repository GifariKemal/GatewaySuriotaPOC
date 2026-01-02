import json
import sys

# Read manifest
with open("Main/build/esp32.esp32.esp32s3/firmware_manifest.json", "r") as f:
    manifest = json.load(f)

# Print each field on separate line
print("=" * 80)
print("FIRMWARE v2.5.20 MANIFEST")
print("=" * 80)
print(f"Version: {manifest['version']}")
print(f"Build Number: {manifest['build_number']}")
print(f"Release Date: {manifest['release_date']}")
print(f"Size: {manifest['firmware']['size']} bytes")
print()
print("SHA256:")
print(manifest["firmware"]["sha256"])
print()
print("Signature:")
print(manifest["firmware"]["signature"])
print()
print("=" * 80)

# Also save to a clean file for GitHub
# Also save to a clean file for GitHub
github_manifest = {
    "version": "2.5.27",
    "build_number": 2527,
    "release_date": "2025-12-02",
    "min_version": "2.3.0",
    "firmware": {
        "url": "https://api.github.com/repos/GifariKemal/GatewaySuriotaOTA/contents/releases/v2.5.27/firmware.bin?ref=main",
        "filename": "firmware.bin",
        "size": manifest["firmware"]["size"],
        "sha256": manifest["firmware"]["sha256"],
        "signature": manifest["firmware"]["signature"],
    },
    "changelog": [
        "v2.5.27: Stable - Robust OTA with Resume & WDT Fixes",
        "- Implemented HTTP Range Resume for slow connections",
        "- Fixed over-downloading issue during resume",
        "- Added Watchdog Timeout (WDT) prevention",
    ],
    "mandatory": False,
}

with open("firmware_manifest_v2.5.27.json", "w") as f:
    json.dump(github_manifest, f, indent=2)

print("GitHub manifest saved to: firmware_manifest_v2.5.27.json")
