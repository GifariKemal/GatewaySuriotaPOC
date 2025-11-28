#!/usr/bin/env python3
"""
🚀 OTA Firmware Update Tool for SRT-MGATE-1210
===============================================

Automated OTA update process via BLE:
  1. Check for updates
  2. Download & verify firmware
  3. Apply update and reboot

Dependencies:
    pip install bleak colorama

Usage:
    python ota_update.py

Author: Claude Code
Version: 1.0.0
"""

import asyncio
import json
import sys
import time
from datetime import datetime

try:
    from bleak import BleakClient, BleakScanner
except ImportError:
    print("❌ Error: bleak module not found")
    print("   Install with: pip install bleak")
    sys.exit(1)

try:
    from colorama import init, Fore, Back, Style
    init(autoreset=True)
    COLORAMA_AVAILABLE = True
except ImportError:
    COLORAMA_AVAILABLE = False
    # Fallback - empty strings if colorama not available
    class Fore:
        RED = GREEN = YELLOW = CYAN = MAGENTA = WHITE = BLUE = RESET = ""
    class Back:
        RED = GREEN = YELLOW = CYAN = MAGENTA = WHITE = BLUE = RESET = ""
    class Style:
        BRIGHT = DIM = RESET_ALL = ""

# ============================================================================
# BLE Configuration
# ============================================================================
SERVICE_UUID = "00001830-0000-1000-8000-00805f9b34fb"
COMMAND_CHAR_UUID = "11111111-1111-1111-1111-111111111101"
RESPONSE_CHAR_UUID = "11111111-1111-1111-1111-111111111102"
SERVICE_NAME = "SURIOTA GW"

# Timing Configuration (in seconds)
SCAN_TIMEOUT = 10.0
CONNECT_TIMEOUT = 15.0
RESPONSE_TIMEOUT = 120  # 2 minutes for OTA operations
CHUNK_DELAY = 0.1       # Delay between BLE chunks
COMMAND_DELAY = 3.0     # Delay between OTA commands

# ============================================================================
# Global Variables
# ============================================================================
response_buffer = []
response_complete = False
start_time = None

# ============================================================================
# Terminal UI Helpers
# ============================================================================
def clear_line():
    """Clear current line"""
    print("\r" + " " * 80 + "\r", end="")

def print_header():
    """Print beautiful header"""
    print()
    print(f"{Fore.CYAN}{'═' * 70}")
    print(f"{Fore.CYAN}║{' ' * 68}║")
    print(f"{Fore.CYAN}║{Fore.WHITE}  🚀  OTA FIRMWARE UPDATE TOOL  -  SRT-MGATE-1210{' ' * 19}║")
    print(f"{Fore.CYAN}║{' ' * 68}║")
    print(f"{Fore.CYAN}{'═' * 70}")
    print(f"{Fore.WHITE}  Started: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print()

def print_step(step_num, total_steps, title, status="running"):
    """Print step indicator"""
    icons = {
        "running": f"{Fore.YELLOW}⏳",
        "success": f"{Fore.GREEN}✅",
        "error": f"{Fore.RED}❌",
        "skip": f"{Fore.CYAN}⏭️",
        "wait": f"{Fore.MAGENTA}⏱️"
    }
    icon = icons.get(status, "▶️")
    print(f"\n{Fore.CYAN}{'─' * 70}")
    print(f"{icon} {Fore.WHITE}[Step {step_num}/{total_steps}] {Style.BRIGHT}{title}")
    print(f"{Fore.CYAN}{'─' * 70}")

def print_progress_bar(progress, width=50, prefix="Progress"):
    """Print progress bar"""
    filled = int(width * progress / 100)
    bar = "█" * filled + "░" * (width - filled)
    color = Fore.GREEN if progress >= 100 else Fore.YELLOW
    print(f"\r  {prefix}: {color}[{bar}] {progress}%{Style.RESET_ALL}", end="", flush=True)

def print_box(title, content, color=Fore.WHITE):
    """Print content in a box"""
    print(f"\n  {color}┌{'─' * 64}┐")
    print(f"  {color}│ {Style.BRIGHT}{title}{Style.RESET_ALL}{color}{' ' * (63 - len(title))}│")
    print(f"  {color}├{'─' * 64}┤")

    if isinstance(content, dict):
        for key, value in content.items():
            line = f"  {key}: {value}"
            padding = 63 - len(line)
            print(f"  {color}│{Fore.WHITE}{line}{' ' * max(0, padding)}{color}│")
    else:
        for line in str(content).split('\n')[:10]:  # Max 10 lines
            padding = 63 - len(line)
            print(f"  {color}│{Fore.WHITE} {line[:62]}{' ' * max(0, padding)}{color}│")

    print(f"  {color}└{'─' * 64}┘")

def print_success(message):
    print(f"\n  {Fore.GREEN}✅ {message}{Style.RESET_ALL}")

def print_error(message):
    print(f"\n  {Fore.RED}❌ {message}{Style.RESET_ALL}")

def print_warning(message):
    print(f"\n  {Fore.YELLOW}⚠️  {message}{Style.RESET_ALL}")

def print_info(message):
    print(f"  {Fore.CYAN}ℹ️  {message}{Style.RESET_ALL}")

def print_waiting(message):
    print(f"  {Fore.MAGENTA}⏱️  {message}{Style.RESET_ALL}")

# ============================================================================
# Animation Helpers
# ============================================================================
async def animate_waiting(message, duration):
    """Show animated waiting message"""
    frames = ["⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"]
    end_time = time.time() + duration
    frame_idx = 0

    while time.time() < end_time:
        remaining = int(end_time - time.time())
        frame = frames[frame_idx % len(frames)]
        print(f"\r  {Fore.MAGENTA}{frame} {message} ({remaining}s remaining)...{Style.RESET_ALL}    ", end="", flush=True)
        frame_idx += 1
        await asyncio.sleep(0.1)

    print()

async def countdown(seconds, message="Starting in"):
    """Show countdown"""
    for i in range(seconds, 0, -1):
        print(f"\r  {Fore.YELLOW}⏳ {message} {i}...{Style.RESET_ALL}  ", end="", flush=True)
        await asyncio.sleep(1)
    print()

# ============================================================================
# BLE Functions
# ============================================================================
def notification_handler(sender, data):
    """Handle incoming BLE notifications"""
    global response_buffer, response_complete

    try:
        chunk = data.decode('utf-8')

        if chunk == "<END>":
            response_complete = True
        else:
            response_buffer.append(chunk)
            # Show progress dots
            print(f"{Fore.CYAN}.", end="", flush=True)
    except Exception as e:
        print(f"\n  {Fore.RED}⚠️  Error decoding: {e}{Style.RESET_ALL}")


async def scan_for_device():
    """Scan for SURIOTA Gateway"""
    print(f"\n  {Fore.CYAN}📡 Scanning for BLE devices...{Style.RESET_ALL}")

    devices = await BleakScanner.discover(timeout=SCAN_TIMEOUT)

    if not devices:
        return None

    # Find SURIOTA GW
    for device in devices:
        if device.name == SERVICE_NAME:
            print(f"  {Fore.GREEN}✓ Found: {device.name} ({device.address}){Style.RESET_ALL}")
            return device

    return None


async def connect_device(address):
    """Connect to BLE device"""
    print(f"\n  {Fore.CYAN}🔗 Connecting to {address}...{Style.RESET_ALL}")

    client = BleakClient(address, timeout=CONNECT_TIMEOUT)
    await client.connect()

    if client.is_connected:
        await client.start_notify(RESPONSE_CHAR_UUID, notification_handler)
        print(f"  {Fore.GREEN}✓ Connected & notifications enabled{Style.RESET_ALL}")
        return client

    return None


async def send_ota_command(client, command_type, wait_time=None):
    """Send OTA command and wait for response"""
    global response_buffer, response_complete

    # Reset buffer
    response_buffer = []
    response_complete = False

    # Build command
    command = {"op": "ota", "type": command_type}
    command_str = json.dumps(command, separators=(',', ':'))

    print(f"\n  {Fore.CYAN}📤 Sending: {Fore.WHITE}{command_str}{Style.RESET_ALL}")
    print(f"  {Fore.CYAN}📥 Receiving: ", end="", flush=True)

    # Fragment and send
    chunk_size = 18
    for i in range(0, len(command_str), chunk_size):
        chunk = command_str[i:i+chunk_size]
        await client.write_gatt_char(COMMAND_CHAR_UUID, chunk.encode('utf-8'))
        await asyncio.sleep(CHUNK_DELAY)

    # Send end marker
    await client.write_gatt_char(COMMAND_CHAR_UUID, "<END>".encode('utf-8'))

    # Wait for response
    timeout = wait_time or RESPONSE_TIMEOUT
    elapsed = 0

    while not response_complete and elapsed < timeout:
        await asyncio.sleep(0.1)
        elapsed += 0.1

        # Show progress for long waits
        if elapsed > 0 and int(elapsed) % 10 == 0 and int(elapsed) != int(elapsed - 0.1):
            print(f"{Fore.YELLOW}({int(elapsed)}s){Fore.CYAN}", end="", flush=True)

    print()  # New line after dots

    if response_complete:
        full_response = ''.join(response_buffer)
        return full_response
    else:
        return None


def parse_response(response_str):
    """Parse JSON response"""
    try:
        return json.loads(response_str)
    except:
        return {"raw": response_str}

# ============================================================================
# OTA Process Functions
# ============================================================================
async def step_check_update(client):
    """Step 1: Check for updates"""
    print_step(1, 4, "Checking for firmware updates")

    response = await send_ota_command(client, "check_update", wait_time=30)

    if not response:
        print_error("Timeout waiting for response")
        return None

    data = parse_response(response)

    # Display response
    print_box("📋 Server Response", response[:500] if len(response) > 500 else response, Fore.CYAN)

    # Check status
    if data.get("status") != "ok":
        print_error(f"Error: {data.get('message', 'Unknown error')}")
        return None

    # Parse update info - handle both response formats:
    # Format 1: {"status":"ok", "update_available":true, "current_version":"...", ...}
    # Format 2: {"status":"ok", "update_info":{"update_available":true, ...}}
    update_info = data.get("update_info", {})
    update_available = data.get("update_available", update_info.get("update_available", False))
    current = data.get("current_version", update_info.get("current_version", "?"))
    available = data.get("available_version", update_info.get("available_version", "?"))
    firmware_size = data.get("firmware_size", update_info.get("firmware_size", 0))
    available_build = data.get("available_build", update_info.get("available_build", "?"))
    mandatory = data.get("mandatory", update_info.get("mandatory", False))

    # Build info display
    info = {
        "Current Version": current,
        "Available Version": available,
        "Build Number": available_build,
        "Firmware Size": f"{firmware_size:,} bytes" if firmware_size else "Unknown",
        "Mandatory": "Yes" if mandatory else "No"
    }

    if update_available:
        print_success(f"Update available: {current} → {available}")
        print_box("📦 Update Information", info, Fore.GREEN)
        return data
    else:
        # Same version - offer re-flash option
        print_info(f"Already running latest version ({current})")
        print_box("📦 Firmware Information", info, Fore.YELLOW)

        print()
        print(f"  {Fore.YELLOW}Do you want to re-flash the same firmware anyway? (y/n){Style.RESET_ALL}")

        try:
            choice = input(f"  {Fore.WHITE}> {Style.RESET_ALL}").strip().lower()
            if choice in ['y', 'yes']:
                print_info("Proceeding with re-flash...")
                return data  # Return data to continue with update
            else:
                print_info("Update skipped")
                return None
        except KeyboardInterrupt:
            return None


async def step_start_update(client):
    """Step 2: Start firmware download"""
    print_step(2, 4, "Downloading firmware from GitHub")

    print_info("This may take 1-2 minutes depending on network speed...")
    print_info("Please wait while firmware downloads and verifies...")
    print()

    # Track download start time
    download_start = time.time()

    response = await send_ota_command(client, "start_update", wait_time=180)  # 3 minutes timeout

    download_time = time.time() - download_start

    if not response:
        print_error("Timeout waiting for download to complete")
        return False

    data = parse_response(response)

    # Display response
    print_box("📋 Server Response", response[:500] if len(response) > 500 else response, Fore.CYAN)

    if data.get("status") == "ok":
        print_success(f"Firmware downloaded and verified in {download_time:.1f} seconds!")

        # Show verification details if available
        info = {
            "Download Time": f"{download_time:.1f} seconds",
            "Status": data.get("message", "Success"),
            "State": data.get("state", "VALIDATING")
        }
        print_box("✅ Download Complete", info, Fore.GREEN)
        return True
    else:
        print_error(f"Download failed: {data.get('message', 'Unknown error')}")
        return False


async def step_confirm_apply(client):
    """Step 3: Confirm before applying update"""
    print_step(3, 4, "Confirm Update Application")

    print()
    print(f"  {Fore.YELLOW}{'═' * 60}")
    print(f"  {Fore.YELLOW}║{' ' * 58}║")
    print(f"  {Fore.YELLOW}║  {Fore.WHITE}⚠️  WARNING: Device will REBOOT after applying update!{Fore.YELLOW}  ║")
    print(f"  {Fore.YELLOW}║{' ' * 58}║")
    print(f"  {Fore.YELLOW}{'═' * 60}")
    print()

    print(f"  {Fore.CYAN}Do you want to apply the update and reboot? (y/n){Style.RESET_ALL}")

    try:
        choice = input(f"  {Fore.WHITE}> {Style.RESET_ALL}").strip().lower()
        return choice in ['y', 'yes']
    except KeyboardInterrupt:
        return False


async def step_apply_update(client):
    """Step 4: Apply update and reboot"""
    print_step(4, 4, "Applying firmware update")

    print_warning("Device will reboot in a few seconds...")
    await countdown(3, "Applying update in")

    response = await send_ota_command(client, "apply_update", wait_time=10)

    if response:
        data = parse_response(response)
        print_box("📋 Final Response", response[:300] if len(response) > 300 else response, Fore.CYAN)

    # Device will reboot, so connection will be lost
    print()
    print(f"  {Fore.GREEN}{'═' * 60}")
    print(f"  {Fore.GREEN}║{' ' * 58}║")
    print(f"  {Fore.GREEN}║  {Fore.WHITE}🔄  Device is rebooting with new firmware...{' ' * 13}║")
    print(f"  {Fore.GREEN}║{' ' * 58}║")
    print(f"  {Fore.GREEN}{'═' * 60}")

    return True


async def show_final_summary(success, update_info=None):
    """Show final summary"""
    print()
    print(f"{Fore.CYAN}{'═' * 70}")

    if success:
        print(f"{Fore.GREEN}")
        print(r"   ____  _____   _      ____                      _      _       _ ")
        print(r"  / __ \|_   _| / \    / ___|___  _ __ ___  _ __ | | ___| |_ ___| |")
        print(r" | |  | | | |  / _ \  | |   / _ \| '_ ` _ \| '_ \| |/ _ \ __/ _ \ |")
        print(r" | |__| | | | / ___ \ | |__| (_) | | | | | | |_) | |  __/ ||  __/_|")
        print(r"  \____/  |_|/_/   \_\ \____\___/|_| |_| |_| .__/|_|\___|\__\___(_)")
        print(r"                                           |_|                     ")
        print(f"{Style.RESET_ALL}")
        print(f"  {Fore.GREEN}✅ OTA Update completed successfully!{Style.RESET_ALL}")
        print(f"  {Fore.WHITE}   Device is rebooting with the new firmware.{Style.RESET_ALL}")
        print(f"  {Fore.WHITE}   Please wait ~30 seconds for device to restart.{Style.RESET_ALL}")
    else:
        print(f"{Fore.RED}")
        print(r"   ____  _____   _       _____     _ _          _ ")
        print(r"  / __ \|_   _| / \     |  ___|_ _(_) | ___  __| |")
        print(r" | |  | | | |  / _ \    | |_ / _` | | |/ _ \/ _` |")
        print(r" | |__| | | | / ___ \   |  _| (_| | | |  __/ (_| |")
        print(r"  \____/  |_|/_/   \_\  |_|  \__,_|_|_|\___|\__,_|")
        print(f"{Style.RESET_ALL}")
        print(f"  {Fore.RED}❌ OTA Update failed or was cancelled.{Style.RESET_ALL}")

    print()
    print(f"{Fore.CYAN}{'═' * 70}")
    print(f"{Fore.WHITE}  Finished: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"{Fore.CYAN}{'═' * 70}")
    print()

# ============================================================================
# Main OTA Process
# ============================================================================
async def run_ota_update():
    """Main OTA update process"""
    print_header()

    client = None
    success = False
    update_info = None

    try:
        # ═══════════════════════════════════════════════════════════════════
        # STEP 0: Connect to device
        # ═══════════════════════════════════════════════════════════════════
        print_step(0, 4, "Connecting to SURIOTA Gateway", "running")

        device = await scan_for_device()
        if not device:
            print_error(f"Device '{SERVICE_NAME}' not found!")
            print_info("Make sure device is powered on and BLE is enabled")
            return False

        client = await connect_device(device.address)
        if not client:
            print_error("Failed to connect to device")
            return False

        print_success("Connected to SURIOTA Gateway!")

        # Brief delay for connection stabilization
        await asyncio.sleep(1)

        # ═══════════════════════════════════════════════════════════════════
        # STEP 1: Check for updates
        # ═══════════════════════════════════════════════════════════════════
        update_info = await step_check_update(client)

        if not update_info:
            # User skipped or check failed - just exit cleanly
            print()
            print(f"{Fore.CYAN}{'═' * 70}")
            print(f"{Fore.WHITE}  Session ended: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
            print(f"{Fore.CYAN}{'═' * 70}")
            return False

        # Delay before next command
        print_waiting(f"Waiting {COMMAND_DELAY:.0f}s before next step...")
        await asyncio.sleep(COMMAND_DELAY)

        # ═══════════════════════════════════════════════════════════════════
        # STEP 2: Download firmware
        # ═══════════════════════════════════════════════════════════════════
        download_success = await step_start_update(client)

        if not download_success:
            print_error("Firmware download failed")
            await show_final_summary(False)
            return False

        # Delay before next command
        print_waiting(f"Waiting {COMMAND_DELAY:.0f}s before next step...")
        await asyncio.sleep(COMMAND_DELAY)

        # ═══════════════════════════════════════════════════════════════════
        # STEP 3: Confirm apply
        # ═══════════════════════════════════════════════════════════════════
        confirm = await step_confirm_apply(client)

        if not confirm:
            print_warning("Update cancelled by user")
            await show_final_summary(False)
            return False

        # ═══════════════════════════════════════════════════════════════════
        # STEP 4: Apply update
        # ═══════════════════════════════════════════════════════════════════
        success = await step_apply_update(client)

        await show_final_summary(success, update_info)
        return success

    except KeyboardInterrupt:
        print(f"\n\n  {Fore.YELLOW}⚠️  Interrupted by user{Style.RESET_ALL}")
        await show_final_summary(False)
        return False

    except Exception as e:
        print_error(f"Unexpected error: {e}")
        await show_final_summary(False)
        return False

    finally:
        # Cleanup
        if client and client.is_connected:
            try:
                await client.stop_notify(RESPONSE_CHAR_UUID)
                await client.disconnect()
                print(f"\n  {Fore.CYAN}🔌 Disconnected from device{Style.RESET_ALL}")
            except:
                pass  # Device may have already rebooted


# ============================================================================
# Entry Point
# ============================================================================
def main():
    """Entry point"""
    try:
        result = asyncio.run(run_ota_update())
        sys.exit(0 if result else 1)
    except KeyboardInterrupt:
        print(f"\n\n{Fore.YELLOW}⚠️  Interrupted{Style.RESET_ALL}")
        sys.exit(1)
    except Exception as e:
        print(f"\n{Fore.RED}❌ Fatal error: {e}{Style.RESET_ALL}")
        sys.exit(1)


if __name__ == "__main__":
    main()
