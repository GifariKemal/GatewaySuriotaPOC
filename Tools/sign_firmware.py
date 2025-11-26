#!/usr/bin/env python3
"""
OTA Firmware Signing Tool for SRT-MGATE-1210
Generates ECDSA P-256 signature and SHA-256 checksum for firmware binary

Usage:
    python sign_firmware.py <firmware.bin> [private_key.pem]

Example:
    python sign_firmware.py ../Main/build/Main.ino.bin ota_private_key.pem

Output:
    - firmware_manifest.json (ready for GitHub upload)
    - Console output with signature and checksum

Requirements:
    pip install ecdsa
"""

import hashlib
import json
import os
import sys
from datetime import datetime

try:
    from ecdsa import SigningKey, NIST256p
except ImportError:
    print("Error: ecdsa module not found")
    print("Install with: pip install ecdsa")
    sys.exit(1)


def sign_firmware(firmware_path, private_key_path, version=None):
    """Sign firmware and generate manifest"""

    print("=" * 60)
    print("OTA Firmware Signing Tool")
    print("=" * 60)

    # Check files exist
    if not os.path.exists(firmware_path):
        print(f"Error: Firmware file not found: {firmware_path}")
        sys.exit(1)

    if not os.path.exists(private_key_path):
        print(f"Error: Private key not found: {private_key_path}")
        print("Run generate_ota_keys.py first to create keys")
        sys.exit(1)

    # Load private key
    print(f"\n[1/5] Loading private key: {private_key_path}")
    with open(private_key_path, 'rb') as f:
        private_key = SigningKey.from_pem(f.read())

    # Read firmware
    print(f"[2/5] Reading firmware: {firmware_path}")
    with open(firmware_path, 'rb') as f:
        firmware_data = f.read()

    firmware_size = len(firmware_data)
    print(f"       Firmware size: {firmware_size:,} bytes ({firmware_size/1024/1024:.2f} MB)")

    # Calculate SHA-256 hash
    print("[3/5] Calculating SHA-256 checksum...")
    firmware_hash = hashlib.sha256(firmware_data).digest()
    hash_hex = firmware_hash.hex()
    print(f"       SHA-256: {hash_hex}")

    # Sign the hash
    print("[4/5] Signing firmware hash...")
    signature = private_key.sign(firmware_hash)
    signature_hex = signature.hex()
    print(f"       Signature ({len(signature)} bytes): {signature_hex[:64]}...")

    # Determine version
    if version is None:
        version = input("\nEnter firmware version (e.g., 2.5.0): ").strip()
        if not version:
            version = "1.0.0"

    # Parse version to build number
    try:
        parts = version.split('.')
        build_number = int(parts[0]) * 1000 + int(parts[1]) * 100 + int(parts[2])
    except:
        build_number = 1000

    # Generate manifest
    print("[5/5] Generating firmware_manifest.json...")

    manifest = {
        "version": version,
        "build_number": build_number,
        "release_date": datetime.now().strftime("%Y-%m-%d"),
        "min_version": "2.3.0",
        "firmware": {
            "filename": os.path.basename(firmware_path),
            "size": firmware_size,
            "sha256": hash_hex,
            "signature": signature_hex
        },
        "changelog": [
            f"Version {version} release"
        ],
        "mandatory": False
    }

    # Save manifest
    manifest_path = os.path.join(os.path.dirname(firmware_path), "firmware_manifest.json")
    with open(manifest_path, 'w') as f:
        json.dump(manifest, f, indent=2)

    print(f"       Saved: {manifest_path}")

    # Print summary
    print("\n" + "=" * 60)
    print("SIGNING COMPLETE!")
    print("=" * 60)

    print(f"""
Files ready for upload:
  1. {firmware_path}
  2. {manifest_path}

Upload to GitHub:
  Option A: GitHub Releases
    gh release create v{version} "{firmware_path}" "{manifest_path}"

  Option B: Raw files (ota/ folder)
    git add "{firmware_path}" "{manifest_path}"
    git commit -m "Release v{version}"
    git push

Manifest content:
""")
    print(json.dumps(manifest, indent=2))

    return manifest


def verify_signature(firmware_path, public_key_path, signature_hex):
    """Verify firmware signature (for testing)"""

    print("\nVerifying signature...")

    with open(public_key_path, 'rb') as f:
        from ecdsa import VerifyingKey
        public_key = VerifyingKey.from_pem(f.read())

    with open(firmware_path, 'rb') as f:
        firmware_data = f.read()

    firmware_hash = hashlib.sha256(firmware_data).digest()
    signature = bytes.fromhex(signature_hex)

    try:
        public_key.verify(signature, firmware_hash)
        print("Signature verification: PASSED")
        return True
    except Exception as e:
        print(f"Signature verification: FAILED - {e}")
        return False


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        print("Usage: python sign_firmware.py <firmware.bin> [private_key.pem] [version]")
        print("\nExamples:")
        print("  python sign_firmware.py firmware.bin")
        print("  python sign_firmware.py firmware.bin ota_private_key.pem")
        print("  python sign_firmware.py firmware.bin ota_private_key.pem 2.5.0")
        sys.exit(1)

    firmware_path = sys.argv[1]
    private_key_path = sys.argv[2] if len(sys.argv) > 2 else "ota_private_key.pem"
    version = sys.argv[3] if len(sys.argv) > 3 else None

    manifest = sign_firmware(firmware_path, private_key_path, version)

    # Optional: verify if public key exists
    public_key_path = private_key_path.replace("private", "public")
    if os.path.exists(public_key_path):
        verify_signature(firmware_path, public_key_path, manifest["firmware"]["signature"])
