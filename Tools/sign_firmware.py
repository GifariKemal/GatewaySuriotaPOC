#!/usr/bin/env python3
"""
OTA Firmware Signing Tool for SRT-MGATE-1210
Generates ECDSA P-256 signature and SHA-256 checksum for firmware binary

================================================================================
FIRMWARE BINARY NAMING CONVENTION
================================================================================

Format: {MODEL}_{VARIANT}_v{VERSION}.bin

Components:
  - MODEL    : Product model code (e.g., "MGATE-1210")
  - VARIANT  : Hardware variant ("P" for POE, omit for non-POE)
  - VERSION  : Semantic version (e.g., "1.0.0")

Examples:
  - MGATE-1210_P_v1.0.0.bin     (POE variant, version 1.0.0)
  - MGATE-1210_P_v1.2.5.bin     (POE variant, version 1.2.5)
  - MGATE-1210_v1.0.0.bin       (Non-POE variant, version 1.0.0)
  - MGATE-1210_v2.0.0.bin       (Non-POE variant, version 2.0.0)

This naming convention:
  1. Identifies product model at a glance
  2. Distinguishes hardware variants (POE vs Non-POE)
  3. Shows version clearly for OTA management
  4. Avoids confusion when multiple binaries exist

================================================================================

Usage:
    python sign_firmware.py <input.bin> [private_key.pem] [version] [variant]

Example:
    python sign_firmware.py ../Main/build/Main.ino.bin
    python sign_firmware.py ../Main/build/Main.ino.bin OTA_Keys/ota_private_key.pem 1.0.0 P

Output:
    - MGATE-1210_{VARIANT}_v{VERSION}.bin (renamed firmware)
    - firmware_manifest.json (ready for GitHub upload)
    - Console output with signature and checksum

Requirements:
    pip install ecdsa

Copyright (c) 2026 Suriota IoT Solutions
"""

import hashlib
import json
import os
import shutil
import sys
from datetime import datetime

try:
    from ecdsa import SigningKey, NIST256p
    from ecdsa.util import sigencode_der  # For DER/ASN.1 format (mbedtls compatible)
except ImportError:
    print("Error: ecdsa module not found")
    print("Install with: pip install ecdsa")
    sys.exit(1)

# ============================================================================
# PRODUCT CONFIGURATION - Must match ProductConfig.h
# ============================================================================
PRODUCT_MODEL = "MGATE-1210"  # Short model name for filename
PRODUCT_FULL_NAME = "SRT-MGATE-1210"  # Full product name
MANUFACTURER = "SURIOTA IoT Solutions"
COPYRIGHT = "Copyright (c) 2026 Suriota IoT Solutions"

# Default variant (P = POE, empty = Non-POE)
DEFAULT_VARIANT = "P"

# Minimum supported version for OTA
MIN_VERSION = "0.1.0"


def generate_firmware_filename(version, variant=""):
    """
    Generate firmware filename following naming convention.

    Format: {MODEL}_{VARIANT}_v{VERSION}.bin

    Examples:
        generate_firmware_filename("1.0.0", "P")  -> "MGATE-1210_P_v1.0.0.bin"
        generate_firmware_filename("1.0.0", "")   -> "MGATE-1210_v1.0.0.bin"
    """
    if variant:
        return f"{PRODUCT_MODEL}_{variant}_v{version}.bin"
    else:
        return f"{PRODUCT_MODEL}_v{version}.bin"


def sign_firmware(firmware_path, private_key_path, version=None, variant=None):
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
    with open(private_key_path, "rb") as f:
        private_key = SigningKey.from_pem(f.read())

    # Read firmware
    print(f"[2/5] Reading firmware: {firmware_path}")
    with open(firmware_path, "rb") as f:
        firmware_data = f.read()

    firmware_size = len(firmware_data)
    print(
        f"       Firmware size: {firmware_size:,} bytes ({firmware_size/1024/1024:.2f} MB)"
    )

    # Calculate SHA-256 hash (for manifest checksum field)
    print("[3/5] Calculating SHA-256 checksum...")
    firmware_hash = hashlib.sha256(firmware_data).digest()
    hash_hex = firmware_hash.hex()
    print(f"       SHA-256: {hash_hex}")

    # Sign the firmware DATA (not the hash!) - ecdsa.sign_deterministic hashes internally
    # This ensures both Python and mbedtls compute SHA256(firmware_data) before signing/verifying
    print("[4/5] Signing firmware data...")
    signature = private_key.sign_deterministic(
        firmware_data, hashfunc=hashlib.sha256, sigencode=sigencode_der
    )
    signature_hex = signature.hex()
    print(
        f"       Signature ({len(signature)} bytes, DER format): {signature_hex[:64]}..."
    )

    # Determine version
    if version is None:
        version = input("\nEnter firmware version (e.g., 1.0.0): ").strip()
        if not version:
            version = "1.0.0"

    # Determine variant
    if variant is None:
        variant = input(
            f"Enter variant (P=POE, empty=Non-POE) [{DEFAULT_VARIANT}]: "
        ).strip()
        if not variant:
            variant = DEFAULT_VARIANT

    # Parse version to build number (MAJOR*1000 + MINOR*100 + PATCH)
    try:
        parts = version.split(".")
        build_number = int(parts[0]) * 1000 + int(parts[1]) * 100 + int(parts[2])
    except:
        build_number = 1000

    # Generate proper firmware filename
    output_filename = generate_firmware_filename(version, variant)
    output_dir = os.path.dirname(firmware_path)
    output_path = os.path.join(output_dir, output_filename)

    # Copy/rename firmware to proper name
    print(f"[5/6] Copying firmware with proper naming...")
    print(f"       {os.path.basename(firmware_path)} -> {output_filename}")
    shutil.copy2(firmware_path, output_path)

    # Generate manifest
    print("[6/6] Generating firmware_manifest.json...")

    manifest = {
        "product": {
            "name": PRODUCT_FULL_NAME,
            "model": f"{PRODUCT_MODEL}({variant})" if variant else PRODUCT_MODEL,
            "manufacturer": MANUFACTURER,
            "copyright": COPYRIGHT,
        },
        "version": version,
        "build_number": build_number,
        "release_date": datetime.now().strftime("%Y-%m-%d"),
        "min_version": MIN_VERSION,
        "firmware": {
            "filename": output_filename,
            "size": firmware_size,
            "sha256": hash_hex,
            "signature": signature_hex,
        },
        "changelog": [f"Version {version} release"],
        "mandatory": False,
    }

    # Save manifest
    manifest_path = os.path.join(output_dir, "firmware_manifest.json")
    with open(manifest_path, "w") as f:
        json.dump(manifest, f, indent=2)

    print(f"       Saved: {manifest_path}")
    print(f"       Firmware: {output_path}")

    # Print summary
    print("\n" + "=" * 60)
    print("SIGNING COMPLETE!")
    print("=" * 60)

    print(
        f"""
Output Files:
  1. Firmware: {output_path}
  2. Manifest: {manifest_path}

Firmware Naming Convention:
  Format: {{MODEL}}_{{VARIANT}}_v{{VERSION}}.bin
  Result: {output_filename}

Upload to GitHub OTA Repository:
  # Copy to OTA repo releases folder
  mkdir -p releases/v{version}
  cp "{output_path}" releases/v{version}/

  # Update root manifest with proper URL
  # URL format: https://api.github.com/repos/{{owner}}/{{repo}}/contents/releases/v{version}/{output_filename}?ref=main

  # Commit and push
  git add releases/v{version}/ firmware_manifest.json
  git commit -m "feat(v{version}): Release {PRODUCT_FULL_NAME} firmware"
  git push

Manifest content:
"""
    )
    print(json.dumps(manifest, indent=2))

    return manifest, output_path


def verify_signature(firmware_path, public_key_path, signature_hex):
    """Verify firmware signature (for testing)"""

    print("\nVerifying signature...")

    with open(public_key_path, "rb") as f:
        from ecdsa import VerifyingKey
        from ecdsa.util import sigdecode_der  # For DER/ASN.1 format

        public_key = VerifyingKey.from_pem(f.read())

    with open(firmware_path, "rb") as f:
        firmware_data = f.read()

    signature = bytes.fromhex(signature_hex)

    try:
        # Verify against firmware DATA (not hash) - ecdsa.verify hashes internally
        public_key.verify(
            signature, firmware_data, hashfunc=hashlib.sha256, sigdecode=sigdecode_der
        )
        print("Signature verification: PASSED")
        return True
    except Exception as e:
        print(f"Signature verification: FAILED - {e}")
        return False


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        print(
            "Usage: python sign_firmware.py <input.bin> [private_key.pem] [version] [variant]"
        )
        print("\nArguments:")
        print("  input.bin      : Input firmware binary (e.g., Main.ino.bin)")
        print(
            "  private_key    : ECDSA private key (default: OTA_Keys/ota_private_key.pem)"
        )
        print("  version        : Firmware version (e.g., 1.0.0)")
        print("  variant        : Hardware variant (P=POE, empty=Non-POE)")
        print("\nExamples:")
        print("  python sign_firmware.py ../Main/build/Main.ino.bin")
        print(
            "  python sign_firmware.py ../Main/build/Main.ino.bin OTA_Keys/ota_private_key.pem 1.0.0"
        )
        print(
            "  python sign_firmware.py ../Main/build/Main.ino.bin OTA_Keys/ota_private_key.pem 1.0.0 P"
        )
        print("\nOutput Naming Convention:")
        print("  Format: {MODEL}_{VARIANT}_v{VERSION}.bin")
        print("  Example: MGATE-1210_P_v1.0.0.bin (POE variant)")
        print("  Example: MGATE-1210_v1.0.0.bin (Non-POE variant)")
        sys.exit(1)

    firmware_path = sys.argv[1]

    # Default private key location is OTA_Keys folder (relative to script)
    script_dir = os.path.dirname(os.path.abspath(__file__))
    default_key = os.path.join(script_dir, "OTA_Keys", "ota_private_key.pem")

    private_key_path = sys.argv[2] if len(sys.argv) > 2 else default_key
    version = sys.argv[3] if len(sys.argv) > 3 else None
    variant = sys.argv[4] if len(sys.argv) > 4 else None

    manifest, output_path = sign_firmware(
        firmware_path, private_key_path, version, variant
    )

    # Optional: verify if public key exists
    public_key_path = private_key_path.replace("private", "public")
    if os.path.exists(public_key_path):
        verify_signature(
            output_path, public_key_path, manifest["firmware"]["signature"]
        )
