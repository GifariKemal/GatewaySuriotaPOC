#!/usr/bin/env python3
"""
🚀 OTA Firmware Update Tool for SRT-MGATE-1210
===============================================

Automated OTA update process via BLE:
  1. Set GitHub token (for private repos)
  2. Check for updates
  3. Download & verify firmware (with real-time progress)
  4. Apply update and reboot

Features (v2.0.0):
  - Real-time progress notifications (push from device)
  - Download speed and ETA display`
  - Network mode indicator (WiFi/Ethernet)
  - Retry count monitoring
  - Interactive progress bar

Dependencies:
    pip install bleak colorama

Usage:
    python ota_update.py                    # Interactive menu
    python ota_update.py --set-token TOKEN  # Set GitHub token
    python ota_update.py --check            # Check for updates only
    python ota_update.py --update           # Full update flow
    python ota_update.py --status           # Monitor OTA status
    python ota_update.py --auto             # Auto update (no prompts)

Author: Claude Code
Version: 2.0.0
"""

import asyncio
import json
import sys
import time
import argparse
import io
from datetime import datetime

# Fix Windows console encoding for Unicode characters
if sys.platform == "win32":
    # Set UTF-8 encoding for stdout/stderr
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8", errors="replace")

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
# v2.5.33+: BLE name format is now "MGate-1210(P)XXXX" or "MGate-1210XXXX"
# Where (P) = POE variant, XXXX = last 2 bytes of MAC (4 hex chars)
# Legacy: "SURIOTA-XXXXXX" (v2.5.31), "SURIOTA GW" (older)
SERVICE_NAME_PREFIX = "MGate-1210"  # New format: MGate-1210(P)XXXX or MGate-1210XXXX
SERVICE_NAME_LEGACY_PREFIX = "SURIOTA-"  # v2.5.31 format: SURIOTA-XXXXXX
SERVICE_NAME_LEGACY = "SURIOTA GW"  # Older firmware format

# Timing Configuration (in seconds)
SCAN_TIMEOUT = 10.0
CONNECT_TIMEOUT = 15.0
RESPONSE_TIMEOUT = 120  # 2 minutes for OTA operations
CHUNK_DELAY = 0.1  # Delay between BLE chunks
COMMAND_DELAY = 3.0  # Delay between OTA commands

# GitHub Token for Private Repository Access
# Set this ONLY if repository is PRIVATE. Leave empty for public repos.
# Note: Token can cause 404 errors on public repos!
GITHUB_TOKEN = "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"  # Set your GitHub Personal Access Token here for private repos

# ============================================================================
# Global Variables
# ============================================================================
response_buffer = []
response_complete = False
start_time = None

# v2.0.0: OTA Progress Tracking (for push notifications)
ota_progress_data = {
    "progress": 0,
    "bytes_downloaded": 0,
    "total_bytes": 0,
    "bytes_per_second": 0,
    "eta_seconds": 0,
    "network_mode": "Unknown",
    "state": "IDLE",
    "retry_count": 0,
    "max_retries": 3,
    "last_update": 0,
}
ota_progress_callback = None  # Optional callback for progress updates


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
    print(
        f"{Fore.CYAN}║{Fore.WHITE}  🚀  OTA FIRMWARE UPDATE TOOL  -  SRT-MGATE-1210{' ' * 19}║"
    )
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
        "wait": f"{Fore.MAGENTA}⏱️",
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
    print(
        f"\r  {prefix}: {color}[{bar}] {progress}%{Style.RESET_ALL}", end="", flush=True
    )


def print_box(title, content, color=Fore.WHITE):
    """Print content in a box"""
    print(f"\n  {color}┌{'─' * 64}┐")
    print(
        f"  {color}│ {Style.BRIGHT}{title}{Style.RESET_ALL}{color}{' ' * (63 - len(title))}│"
    )
    print(f"  {color}├{'─' * 64}┤")

    if isinstance(content, dict):
        for key, value in content.items():
            line = f"  {key}: {value}"
            padding = 63 - len(line)
            print(f"  {color}│{Fore.WHITE}{line}{' ' * max(0, padding)}{color}│")
    else:
        for line in str(content).split("\n")[:10]:  # Max 10 lines
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
        print(
            f"\r  {Fore.MAGENTA}{frame} {message} ({remaining}s remaining)...{Style.RESET_ALL}    ",
            end="",
            flush=True,
        )
        frame_idx += 1
        await asyncio.sleep(0.1)

    print()


async def countdown(seconds, message="Starting in"):
    """Show countdown"""
    for i in range(seconds, 0, -1):
        print(
            f"\r  {Fore.YELLOW}⏳ {message} {i}...{Style.RESET_ALL}  ",
            end="",
            flush=True,
        )
        await asyncio.sleep(1)
    print()


# ============================================================================
# BLE Functions
# ============================================================================
def notification_handler(sender, data):
    """Handle incoming BLE notifications including OTA progress push"""
    global response_buffer, response_complete, ota_progress_data, ota_progress_callback

    try:
        chunk = data.decode("utf-8")

        if chunk == "<END>":
            response_complete = True
        else:
            # v2.0.0: Check if this is an OTA progress push notification
            # These come as complete JSON objects, not fragments
            if chunk.startswith('{"type":"ota_progress"'):
                try:
                    progress_json = json.loads(chunk)
                    handle_ota_progress_notification(progress_json)
                    return  # Don't add to response buffer
                except json.JSONDecodeError:
                    pass  # Not a complete JSON, treat as fragment

            response_buffer.append(chunk)
            # Show progress dots (only for non-progress notifications)
            if not ota_progress_data.get("downloading", False):
                print(f"{Fore.CYAN}.", end="", flush=True)
    except Exception as e:
        print(f"\n  {Fore.RED}⚠️  Error decoding: {e}{Style.RESET_ALL}")


def handle_ota_progress_notification(data):
    """Handle OTA progress push notification from device (v2.0.0)"""
    global ota_progress_data, ota_progress_callback

    # Update global progress data
    ota_progress_data["progress"] = data.get("progress", 0)
    ota_progress_data["bytes_downloaded"] = data.get("bytes_downloaded", 0)
    ota_progress_data["total_bytes"] = data.get("total_bytes", 0)
    ota_progress_data["bytes_per_second"] = data.get("bytes_per_second", 0)
    ota_progress_data["eta_seconds"] = data.get("eta_seconds", 0)
    ota_progress_data["network_mode"] = data.get("network_mode", "Unknown")
    ota_progress_data["state"] = data.get("state", "DOWNLOADING")
    ota_progress_data["retry_count"] = data.get("retry_count", 0)
    ota_progress_data["max_retries"] = data.get("max_retries", 3)
    ota_progress_data["last_update"] = time.time()
    ota_progress_data["downloading"] = True

    # Display progress
    display_ota_progress(ota_progress_data)

    # Call optional callback
    if ota_progress_callback:
        ota_progress_callback(ota_progress_data)


def display_ota_progress(data):
    """Display OTA progress with speed and ETA (v2.0.0)"""
    progress = data.get("progress", 0)
    downloaded = data.get("bytes_downloaded", 0)
    total = data.get("total_bytes", 0)
    speed = data.get("bytes_per_second", 0)
    eta = data.get("eta_seconds", 0)
    network = data.get("network_mode", "Unknown")
    retry = data.get("retry_count", 0)
    max_retry = data.get("max_retries", 3)

    # Format size
    def format_size(bytes_val):
        if bytes_val >= 1024 * 1024:
            return f"{bytes_val / (1024 * 1024):.1f} MB"
        elif bytes_val >= 1024:
            return f"{bytes_val / 1024:.1f} KB"
        return f"{bytes_val} B"

    # Format speed
    def format_speed(bps):
        if bps >= 1024 * 1024:
            return f"{bps / (1024 * 1024):.1f} MB/s"
        elif bps >= 1024:
            return f"{bps / 1024:.1f} KB/s"
        return f"{bps} B/s"

    # Format ETA
    def format_eta(seconds):
        if seconds <= 0:
            return "calculating..."
        elif seconds < 60:
            return f"{seconds}s"
        elif seconds < 3600:
            return f"{seconds // 60}m {seconds % 60}s"
        return f"{seconds // 3600}h {(seconds % 3600) // 60}m"

    # Build progress bar
    bar_width = 30
    filled = int(bar_width * progress / 100)
    bar = "█" * filled + "░" * (bar_width - filled)

    # Color based on progress
    if progress >= 100:
        color = Fore.GREEN
    elif progress >= 50:
        color = Fore.YELLOW
    else:
        color = Fore.CYAN

    # Build status line
    size_info = f"{format_size(downloaded)}/{format_size(total)}"
    speed_info = format_speed(speed) if speed > 0 else "..."
    eta_info = format_eta(eta)
    retry_info = f"[Retry {retry}/{max_retry}]" if retry > 0 else ""

    # Clear line and print progress
    status_line = (
        f"\r  {color}[{bar}] {progress:3d}%{Style.RESET_ALL} | "
        f"{Fore.WHITE}{size_info}{Style.RESET_ALL} | "
        f"{Fore.CYAN}⚡{speed_info}{Style.RESET_ALL} | "
        f"{Fore.MAGENTA}⏱️ {eta_info}{Style.RESET_ALL} | "
        f"{Fore.YELLOW}📡{network}{Style.RESET_ALL} "
        f"{Fore.RED}{retry_info}{Style.RESET_ALL}"
    )

    print(status_line + "    ", end="", flush=True)


async def scan_for_device():
    """Scan for MGate/SURIOTA Gateway (supports current and legacy naming)"""
    print(f"\n  {Fore.CYAN}📡 Scanning for BLE devices...{Style.RESET_ALL}")

    devices = await BleakScanner.discover(timeout=SCAN_TIMEOUT)

    if not devices:
        return None

    # Find MGate/SURIOTA Gateway - check formats in order:
    # 1. v2.5.33+: MGate-1210(P)XXXX or MGate-1210XXXX
    # 2. v2.5.31:  SURIOTA-XXXXXX
    # 3. Older:    SURIOTA GW
    suriota_devices = []
    for device in devices:
        if device.name:
            # Check new format: MGate-1210(P)XXXX or MGate-1210XXXX (v2.5.33+)
            if device.name.startswith(SERVICE_NAME_PREFIX):
                suriota_devices.append(device)
                print(
                    f"  {Fore.GREEN}✓ Found: {device.name} ({device.address}){Style.RESET_ALL}"
                )
            # Check v2.5.31 format: SURIOTA-XXXXXX
            elif device.name.startswith(SERVICE_NAME_LEGACY_PREFIX):
                suriota_devices.append(device)
                print(
                    f"  {Fore.GREEN}✓ Found (v2.5.31): {device.name} ({device.address}){Style.RESET_ALL}"
                )
            # Check legacy format: SURIOTA GW (older firmware)
            elif device.name == SERVICE_NAME_LEGACY:
                suriota_devices.append(device)
                print(
                    f"  {Fore.GREEN}✓ Found (legacy): {device.name} ({device.address}){Style.RESET_ALL}"
                )

    if len(suriota_devices) == 0:
        return None
    elif len(suriota_devices) == 1:
        return suriota_devices[0]
    else:
        # Multiple devices found - let user choose
        print(f"\n  {Fore.YELLOW}Multiple MGate devices found:{Style.RESET_ALL}")
        for i, dev in enumerate(suriota_devices):
            print(f"    {i+1}. {dev.name} ({dev.address})")
        try:
            choice = input(
                f"  {Fore.WHITE}Select device (1-{len(suriota_devices)}): {Style.RESET_ALL}"
            ).strip()
            idx = int(choice) - 1
            if 0 <= idx < len(suriota_devices):
                return suriota_devices[idx]
        except (ValueError, KeyboardInterrupt):
            pass
        return suriota_devices[0]  # Default to first if invalid input


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
    command_str = json.dumps(command, separators=(",", ":"))

    print(f"\n  {Fore.CYAN}📤 Sending: {Fore.WHITE}{command_str}{Style.RESET_ALL}")
    print(f"  {Fore.CYAN}📥 Receiving: ", end="", flush=True)

    # Fragment and send
    chunk_size = 18
    for i in range(0, len(command_str), chunk_size):
        chunk = command_str[i : i + chunk_size]
        await client.write_gatt_char(COMMAND_CHAR_UUID, chunk.encode("utf-8"))
        await asyncio.sleep(CHUNK_DELAY)

    # Send end marker
    await client.write_gatt_char(COMMAND_CHAR_UUID, "<END>".encode("utf-8"))

    # Wait for response
    timeout = wait_time or RESPONSE_TIMEOUT
    elapsed = 0

    while not response_complete and elapsed < timeout:
        await asyncio.sleep(0.1)
        elapsed += 0.1

        # Show progress for long waits
        if (
            elapsed > 0
            and int(elapsed) % 10 == 0
            and int(elapsed) != int(elapsed - 0.1)
        ):
            print(f"{Fore.YELLOW}({int(elapsed)}s){Fore.CYAN}", end="", flush=True)

    print()  # New line after dots

    if response_complete:
        full_response = "".join(response_buffer)
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
    print_box(
        "📋 Server Response",
        response[:500] if len(response) > 500 else response,
        Fore.CYAN,
    )

    # Check status
    if data.get("status") != "ok":
        print_error(f"Error: {data.get('message', 'Unknown error')}")
        return None

    # Parse update info from firmware response format:
    # {
    #   "status": "ok",
    #   "command": "check_update",
    #   "update_available": true,
    #   "current_version": "2.5.10",
    #   "target_version": "2.5.11",
    #   "mandatory": false,
    #   "manifest": { "version": "...", "size": 2001344, "release_notes": "..." }
    # }
    update_available = data.get("update_available", False)
    current = data.get("current_version", "?")
    available = data.get("target_version", "?")  # Firmware uses "target_version"
    mandatory = data.get("mandatory", False)

    # Get firmware size from manifest object
    manifest = data.get("manifest", {})
    firmware_size = manifest.get("size", 0)
    available_build = manifest.get(
        "version", available
    )  # Use version as build identifier
    release_notes = manifest.get("release_notes", "")

    # Build info display
    info = {
        "Current Version": current,
        "Target Version": available,
        "Firmware Size": f"{firmware_size:,} bytes" if firmware_size else "Unknown",
        "Mandatory": "Yes" if mandatory else "No",
    }

    # Add release notes if available
    if release_notes:
        info["Release Notes"] = (
            release_notes[:50] + "..." if len(release_notes) > 50 else release_notes
        )

    if update_available:
        print_success(f"Update available: {current} → {available}")
        print_box("📦 Update Information", info, Fore.GREEN)
        return data
    else:
        # Same version - offer re-flash option
        print_info(f"Already running latest version ({current})")
        print_box("📦 Firmware Information", info, Fore.YELLOW)

        print()
        print(
            f"  {Fore.YELLOW}Do you want to re-flash the same firmware anyway? (y/n){Style.RESET_ALL}"
        )

        try:
            choice = input(f"  {Fore.WHITE}> {Style.RESET_ALL}").strip().lower()
            if choice in ["y", "yes"]:
                print_info("Proceeding with re-flash...")
                return data  # Return data to continue with update
            else:
                print_info("Update skipped")
                return None
        except KeyboardInterrupt:
            return None


async def step_start_update(client):
    """Step 2: Start firmware download with real-time progress (v2.0.0)"""
    global ota_progress_data

    print_step(2, 4, "Downloading firmware from GitHub")

    print_info("This may take 1-2 minutes depending on network speed...")
    print_info("Real-time progress will be displayed via push notifications...")
    print()

    # Reset progress tracking
    ota_progress_data["progress"] = 0
    ota_progress_data["downloading"] = True
    ota_progress_data["state"] = "STARTING"

    # Show initial progress bar
    print(f"  {Fore.CYAN}📥 Download Progress:{Style.RESET_ALL}")
    print()

    # Track download start time
    download_start = time.time()

    response = await send_ota_command(
        client, "start_update", wait_time=360
    )  # 6 minutes timeout

    download_time = time.time() - download_start

    # Mark download complete
    ota_progress_data["downloading"] = False
    print()  # New line after progress bar
    print()

    if not response:
        print_error("Timeout waiting for download to complete")
        return False

    data = parse_response(response)

    # Display response
    print_box(
        "📋 Server Response",
        response[:500] if len(response) > 500 else response,
        Fore.CYAN,
    )

    if data.get("status") == "ok":
        # Get final stats from progress data
        final_speed = ota_progress_data.get("bytes_per_second", 0)
        final_network = ota_progress_data.get("network_mode", "Unknown")
        total_bytes = ota_progress_data.get("total_bytes", 0)

        avg_speed = total_bytes / download_time if download_time > 0 else 0

        print_success(
            f"Firmware downloaded and verified in {download_time:.1f} seconds!"
        )

        # Show verification details if available
        info = {
            "Download Time": f"{download_time:.1f} seconds",
            "Total Size": (
                f"{total_bytes:,} bytes ({total_bytes/(1024*1024):.2f} MB)"
                if total_bytes
                else "Unknown"
            ),
            "Average Speed": (
                f"{avg_speed/1024:.1f} KB/s" if avg_speed > 0 else "Unknown"
            ),
            "Network Mode": final_network,
            "Status": data.get("message", "Success"),
            "State": data.get("state", "VALIDATING"),
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
    print(
        f"  {Fore.YELLOW}║  {Fore.WHITE}⚠️  WARNING: Device will REBOOT after applying update!{Fore.YELLOW}  ║"
    )
    print(f"  {Fore.YELLOW}║{' ' * 58}║")
    print(f"  {Fore.YELLOW}{'═' * 60}")
    print()

    print(
        f"  {Fore.CYAN}Do you want to apply the update and reboot? (y/n){Style.RESET_ALL}"
    )

    try:
        choice = input(f"  {Fore.WHITE}> {Style.RESET_ALL}").strip().lower()
        return choice in ["y", "yes"]
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
        print_box(
            "📋 Final Response",
            response[:300] if len(response) > 300 else response,
            Fore.CYAN,
        )

    # Device will reboot, so connection will be lost
    print()
    print(f"  {Fore.GREEN}{'═' * 60}")
    print(f"  {Fore.GREEN}║{' ' * 58}║")
    print(
        f"  {Fore.GREEN}║  {Fore.WHITE}🔄  Device is rebooting with new firmware...{' ' * 13}║"
    )
    print(f"  {Fore.GREEN}║{' ' * 58}║")
    print(f"  {Fore.GREEN}{'═' * 60}")

    return True


async def step_set_github_token(client, token):
    """Set GitHub token for private repo access"""
    print_step(0, 1, "Setting GitHub Token")

    global response_buffer, response_complete
    response_buffer = []
    response_complete = False

    # Build command
    command = {"op": "ota", "type": "set_github_token", "token": token}
    command_str = json.dumps(command, separators=(",", ":"))

    print(
        f"\n  {Fore.CYAN}📤 Sending: set_github_token (token: {token[:10]}...{token[-4:]}){Style.RESET_ALL}"
    )
    print(f"  {Fore.CYAN}📥 Receiving: ", end="", flush=True)

    # Fragment and send
    chunk_size = 18
    for i in range(0, len(command_str), chunk_size):
        chunk = command_str[i : i + chunk_size]
        await client.write_gatt_char(COMMAND_CHAR_UUID, chunk.encode("utf-8"))
        await asyncio.sleep(CHUNK_DELAY)

    await client.write_gatt_char(COMMAND_CHAR_UUID, "<END>".encode("utf-8"))

    # Wait for response
    elapsed = 0
    while not response_complete and elapsed < 30:
        await asyncio.sleep(0.1)
        elapsed += 0.1

    print()

    if response_complete:
        full_response = "".join(response_buffer)
        data = parse_response(full_response)

        print_box("📋 Response", full_response[:300], Fore.CYAN)

        if data.get("status") == "ok":
            print_success(
                f"GitHub token configured! (Length: {data.get('token_length', len(token))} chars)"
            )
            return True
        else:
            print_error(f"Failed: {data.get('message', 'Unknown error')}")
            return False
    else:
        print_error("Timeout waiting for response")
        return False


async def step_get_config(client):
    """Get OTA configuration"""
    print_step(0, 1, "Getting OTA Configuration")

    response = await send_ota_command(client, "get_config", wait_time=15)

    if not response:
        print_error("Timeout waiting for response")
        return None

    data = parse_response(response)
    print_box("📋 OTA Configuration", response[:600], Fore.CYAN)

    if data.get("status") == "ok":
        config = data.get("config", {})
        info = {
            "Enabled": "Yes" if config.get("enabled") else "No",
            "GitHub Owner": config.get("github_owner", "Not set"),
            "GitHub Repo": config.get("github_repo", "Not set"),
            "Branch": config.get("github_branch", "main"),
            "Verify Signature": "Yes" if config.get("verify_signature") else "No",
            "Auto Update": "Yes" if config.get("auto_update") else "No",
        }
        print_box("⚙️ Current Settings", info, Fore.GREEN)
        return config

    return None


async def step_get_status(client):
    """Get OTA status with enhanced progress info (v2.0.0)"""
    print_step(0, 1, "Getting OTA Status")

    response = await send_ota_command(client, "ota_status", wait_time=15)

    if not response:
        print_error("Timeout waiting for response")
        return None

    data = parse_response(response)
    print_box("📋 Raw Response", response[:800], Fore.CYAN)

    if data.get("status") == "ok":
        # Extract all status fields
        state_name = data.get("state_name", "UNKNOWN")
        progress = data.get("progress", 0)
        bytes_downloaded = data.get("bytes_downloaded", 0)
        total_bytes = data.get("total_bytes", 0)
        bytes_per_second = data.get("bytes_per_second", 0)
        eta_seconds = data.get("eta_seconds", 0)
        network_mode = data.get("network_mode", "Unknown")
        retry_count = data.get("retry_count", 0)
        max_retries = data.get("max_retries", 3)
        current_version = data.get("current_version", "?")
        target_version = data.get("target_version", "?")
        update_available = data.get("update_available", False)

        # Format values
        def format_size(b):
            if b >= 1024 * 1024:
                return f"{b / (1024 * 1024):.2f} MB"
            elif b >= 1024:
                return f"{b / 1024:.1f} KB"
            return f"{b} B"

        def format_speed(bps):
            if bps >= 1024 * 1024:
                return f"{bps / (1024 * 1024):.1f} MB/s"
            elif bps >= 1024:
                return f"{bps / 1024:.1f} KB/s"
            return f"{bps} B/s"

        def format_eta(s):
            if s <= 0:
                return "N/A"
            elif s < 60:
                return f"{s}s"
            return f"{s // 60}m {s % 60}s"

        # Build status info
        info = {
            "State": f"{state_name} ({data.get('state', 0)})",
            "Progress": f"{progress}%",
            "Downloaded": f"{format_size(bytes_downloaded)} / {format_size(total_bytes)}",
            "Speed": format_speed(bytes_per_second) if bytes_per_second > 0 else "N/A",
            "ETA": format_eta(eta_seconds),
            "Network": network_mode,
            "Retries": f"{retry_count} / {max_retries}",
            "Current Version": current_version,
            "Target Version": target_version if target_version else "N/A",
            "Update Available": "Yes" if update_available else "No",
        }

        # Add error info if present
        if data.get("last_error"):
            info["Last Error"] = (
                f"[{data.get('last_error')}] {data.get('last_error_message', '')}"
            )

        # Color based on state
        state_colors = {
            "IDLE": Fore.WHITE,
            "CHECKING": Fore.CYAN,
            "DOWNLOADING": Fore.YELLOW,
            "VALIDATING": Fore.MAGENTA,
            "APPLYING": Fore.BLUE,
            "REBOOTING": Fore.GREEN,
            "ERROR": Fore.RED,
        }
        color = state_colors.get(state_name, Fore.WHITE)

        print_box(f"📊 OTA Status - {state_name}", info, color)

        # If downloading, show progress bar
        if state_name == "DOWNLOADING" and total_bytes > 0:
            print()
            display_ota_progress(
                {
                    "progress": progress,
                    "bytes_downloaded": bytes_downloaded,
                    "total_bytes": total_bytes,
                    "bytes_per_second": bytes_per_second,
                    "eta_seconds": eta_seconds,
                    "network_mode": network_mode,
                    "retry_count": retry_count,
                    "max_retries": max_retries,
                }
            )
            print()

        return data

    return None


async def step_monitor_status(client, interval=2.0, max_polls=60):
    """Continuously monitor OTA status until complete or error (v2.0.0)"""
    print_step(0, 1, "Monitoring OTA Progress")

    print_info(f"Polling every {interval}s (max {max_polls} polls)...")
    print_info("Press Ctrl+C to stop monitoring")
    print()

    polls = 0
    last_state = None

    try:
        while polls < max_polls:
            global response_buffer, response_complete
            response_buffer = []
            response_complete = False

            # Send status command
            command = {"op": "ota", "type": "ota_status"}
            command_str = json.dumps(command, separators=(",", ":"))

            chunk_size = 18
            for i in range(0, len(command_str), chunk_size):
                chunk = command_str[i : i + chunk_size]
                await client.write_gatt_char(COMMAND_CHAR_UUID, chunk.encode("utf-8"))
                await asyncio.sleep(0.05)

            await client.write_gatt_char(COMMAND_CHAR_UUID, "<END>".encode("utf-8"))

            # Wait for response
            elapsed = 0
            while not response_complete and elapsed < 10:
                await asyncio.sleep(0.1)
                elapsed += 0.1

            if response_complete:
                response = "".join(response_buffer)
                data = parse_response(response)

                if data.get("status") == "ok":
                    state = data.get("state_name", "UNKNOWN")
                    progress = data.get("progress", 0)
                    speed = data.get("bytes_per_second", 0)
                    eta = data.get("eta_seconds", 0)
                    network = data.get("network_mode", "Unknown")

                    # Update display
                    display_ota_progress(
                        {
                            "progress": progress,
                            "bytes_downloaded": data.get("bytes_downloaded", 0),
                            "total_bytes": data.get("total_bytes", 0),
                            "bytes_per_second": speed,
                            "eta_seconds": eta,
                            "network_mode": network,
                            "retry_count": data.get("retry_count", 0),
                            "max_retries": data.get("max_retries", 3),
                        }
                    )

                    # Check for state change
                    if state != last_state:
                        print()
                        print(
                            f"  {Fore.CYAN}State changed: {last_state} → {state}{Style.RESET_ALL}"
                        )
                        last_state = state

                    # Check for completion
                    if state in ["IDLE", "VALIDATING", "REBOOTING"]:
                        if progress >= 100 or state == "VALIDATING":
                            print()
                            print_success("Download complete!")
                            return True

                    # Check for error
                    if state == "ERROR":
                        print()
                        print_error(
                            f"OTA Error: {data.get('last_error_message', 'Unknown')}"
                        )
                        return False

            polls += 1
            await asyncio.sleep(interval)

        print()
        print_warning("Max polls reached. Monitoring stopped.")
        return False

    except KeyboardInterrupt:
        print()
        print_info("Monitoring stopped by user")
        return False


async def show_menu():
    """Show interactive menu (v2.0.0)"""
    print()
    print(f"  {Fore.CYAN}{'─' * 50}")
    print(f"  {Fore.WHITE}{Style.BRIGHT}Select an option:{Style.RESET_ALL}")
    print(f"  {Fore.CYAN}{'─' * 50}")
    print(f"  {Fore.WHITE}  1. {Fore.GREEN}Check for updates{Style.RESET_ALL}")
    print(
        f"  {Fore.WHITE}  2. {Fore.YELLOW}Set GitHub token (for private repos){Style.RESET_ALL}"
    )
    print(f"  {Fore.WHITE}  3. {Fore.CYAN}Get OTA configuration{Style.RESET_ALL}")
    print(
        f"  {Fore.WHITE}  4. {Fore.BLUE}Get OTA status (enhanced v2.0){Style.RESET_ALL}"
    )
    print(
        f"  {Fore.WHITE}  5. {Fore.MAGENTA}Monitor OTA progress (real-time){Style.RESET_ALL}"
    )
    print(f"  {Fore.WHITE}  6. {Fore.WHITE}Full OTA update{Style.RESET_ALL}")
    print(f"  {Fore.WHITE}  7. {Fore.RED}Exit{Style.RESET_ALL}")
    print(f"  {Fore.CYAN}{'─' * 50}")
    print()

    try:
        choice = input(f"  {Fore.WHITE}Enter choice (1-7): {Style.RESET_ALL}").strip()
        return choice
    except (KeyboardInterrupt, EOFError):
        return "7"


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
        print(
            f"  {Fore.WHITE}   Device is rebooting with the new firmware.{Style.RESET_ALL}"
        )
        print(
            f"  {Fore.WHITE}   Please wait ~30 seconds for device to restart.{Style.RESET_ALL}"
        )
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
        print_step(0, 4, "Connecting to MGate Gateway", "running")

        device = await scan_for_device()
        if not device:
            print_error(
                f"No MGate device found (looking for '{SERVICE_NAME_PREFIX}*', '{SERVICE_NAME_LEGACY_PREFIX}*', or '{SERVICE_NAME_LEGACY}')"
            )
            print_info("Make sure device is powered on and BLE is enabled")
            return False

        client = await connect_device(device.address)
        if not client:
            print_error("Failed to connect to device")
            return False

        print_success("Connected to MGate Gateway!")

        # Brief delay for connection stabilization
        await asyncio.sleep(1)

        # Auto-set GitHub token for private repo access
        await auto_set_token(client)

        # ═══════════════════════════════════════════════════════════════════
        # STEP 1: Check for updates
        # ═══════════════════════════════════════════════════════════════════
        update_info = await step_check_update(client)

        if not update_info:
            # User skipped or check failed - just exit cleanly
            print()
            print(f"{Fore.CYAN}{'═' * 70}")
            print(
                f"{Fore.WHITE}  Session ended: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}"
            )
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
# Interactive Menu Mode
# ============================================================================
async def auto_set_token(client):
    """Automatically set GitHub token if configured"""
    if not GITHUB_TOKEN:
        return True

    print_info(
        f"Auto-configuring GitHub token ({GITHUB_TOKEN[:10]}...{GITHUB_TOKEN[-4:]})"
    )

    global response_buffer, response_complete
    response_buffer = []
    response_complete = False

    command = {"op": "ota", "type": "set_github_token", "token": GITHUB_TOKEN}
    command_str = json.dumps(command, separators=(",", ":"))

    # Fragment and send
    chunk_size = 18
    for i in range(0, len(command_str), chunk_size):
        chunk = command_str[i : i + chunk_size]
        await client.write_gatt_char(COMMAND_CHAR_UUID, chunk.encode("utf-8"))
        await asyncio.sleep(CHUNK_DELAY)

    await client.write_gatt_char(COMMAND_CHAR_UUID, "<END>".encode("utf-8"))

    # Wait for response
    elapsed = 0
    while not response_complete and elapsed < 10:
        await asyncio.sleep(0.1)
        elapsed += 0.1

    if response_complete:
        full_response = "".join(response_buffer)
        data = parse_response(full_response)
        if data.get("status") == "ok":
            print_success("GitHub token configured automatically!")
            return True

    print_warning("Could not auto-set token (may already be set)")
    return True  # Continue anyway


async def run_interactive_menu():
    """Run interactive menu mode"""
    print_header()

    client = None

    try:
        # Connect first
        print_step(0, 0, "Connecting to MGate Gateway", "running")

        device = await scan_for_device()
        if not device:
            print_error(
                f"No MGate device found (looking for '{SERVICE_NAME_PREFIX}*', '{SERVICE_NAME_LEGACY_PREFIX}*', or '{SERVICE_NAME_LEGACY}')"
            )
            print_info("Make sure device is powered on and BLE is enabled")
            return False

        client = await connect_device(device.address)
        if not client:
            print_error("Failed to connect to device")
            return False

        print_success("Connected to MGate Gateway!")
        await asyncio.sleep(1)

        # Auto-set GitHub token
        await auto_set_token(client)

        # Menu loop
        while True:
            choice = await show_menu()

            if choice == "1":
                # Check for updates
                await step_check_update(client)
                print()
                input(f"  {Fore.CYAN}Press Enter to continue...{Style.RESET_ALL}")

            elif choice == "2":
                # Set GitHub token
                print()
                print(
                    f"  {Fore.YELLOW}Enter GitHub Personal Access Token:{Style.RESET_ALL}"
                )
                print(
                    f"  {Fore.WHITE}(Format: ghp_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx){Style.RESET_ALL}"
                )
                try:
                    token = input(f"  {Fore.WHITE}> {Style.RESET_ALL}").strip()
                    if token and token.startswith("ghp_"):
                        await step_set_github_token(client, token)
                    else:
                        print_error("Invalid token format. Should start with 'ghp_'")
                except (KeyboardInterrupt, EOFError):
                    pass
                print()
                input(f"  {Fore.CYAN}Press Enter to continue...{Style.RESET_ALL}")

            elif choice == "3":
                # Get config
                await step_get_config(client)
                print()
                input(f"  {Fore.CYAN}Press Enter to continue...{Style.RESET_ALL}")

            elif choice == "4":
                # Get OTA status (v2.0.0)
                await step_get_status(client)
                print()
                input(f"  {Fore.CYAN}Press Enter to continue...{Style.RESET_ALL}")

            elif choice == "5":
                # Monitor OTA progress (v2.0.0)
                await step_monitor_status(client)
                print()
                input(f"  {Fore.CYAN}Press Enter to continue...{Style.RESET_ALL}")

            elif choice == "6":
                # Full OTA update - run the full flow
                update_info = await step_check_update(client)

                if update_info:
                    await asyncio.sleep(COMMAND_DELAY)
                    download_success = await step_start_update(client)

                    if download_success:
                        await asyncio.sleep(COMMAND_DELAY)
                        confirm = await step_confirm_apply(client)

                        if confirm:
                            success = await step_apply_update(client)
                            await show_final_summary(success, update_info)
                            return success
                        else:
                            print_warning("Update cancelled by user")
                    else:
                        print_error("Download failed")
                print()
                input(f"  {Fore.CYAN}Press Enter to continue...{Style.RESET_ALL}")

            elif choice == "7":
                print_info("Exiting...")
                break

            else:
                print_error("Invalid choice. Please enter 1-7.")

    except KeyboardInterrupt:
        print(f"\n\n  {Fore.YELLOW}⚠️  Interrupted by user{Style.RESET_ALL}")

    finally:
        if client and client.is_connected:
            try:
                await client.stop_notify(RESPONSE_CHAR_UUID)
                await client.disconnect()
                print(f"\n  {Fore.CYAN}🔌 Disconnected from device{Style.RESET_ALL}")
            except:
                pass

    return False


async def run_set_token(token):
    """Set GitHub token mode"""
    print_header()

    client = None
    try:
        print_step(0, 1, "Connecting to set GitHub token", "running")

        device = await scan_for_device()
        if not device:
            print_error(
                f"No MGate device found (looking for '{SERVICE_NAME_PREFIX}*', '{SERVICE_NAME_LEGACY_PREFIX}*', or '{SERVICE_NAME_LEGACY}')"
            )
            return False

        client = await connect_device(device.address)
        if not client:
            print_error("Failed to connect to device")
            return False

        print_success("Connected!")
        await asyncio.sleep(1)

        result = await step_set_github_token(client, token)
        return result

    finally:
        if client and client.is_connected:
            try:
                await client.stop_notify(RESPONSE_CHAR_UUID)
                await client.disconnect()
                print(f"\n  {Fore.CYAN}🔌 Disconnected{Style.RESET_ALL}")
            except:
                pass


async def run_check_only():
    """Check for updates only"""
    print_header()

    client = None
    try:
        print_step(0, 1, "Connecting to check updates", "running")

        device = await scan_for_device()
        if not device:
            print_error(
                f"No MGate device found (looking for '{SERVICE_NAME_PREFIX}*', '{SERVICE_NAME_LEGACY_PREFIX}*', or '{SERVICE_NAME_LEGACY}')"
            )
            return False

        client = await connect_device(device.address)
        if not client:
            print_error("Failed to connect to device")
            return False

        print_success("Connected!")
        await asyncio.sleep(1)

        # Auto-set GitHub token for private repo access
        await auto_set_token(client)

        result = await step_check_update(client)
        return result is not None

    finally:
        if client and client.is_connected:
            try:
                await client.stop_notify(RESPONSE_CHAR_UUID)
                await client.disconnect()
                print(f"\n  {Fore.CYAN}🔌 Disconnected{Style.RESET_ALL}")
            except:
                pass


# ============================================================================
# Entry Point
# ============================================================================
def parse_args():
    """Parse command line arguments (v2.0.0)"""
    parser = argparse.ArgumentParser(
        description="OTA Firmware Update Tool for SRT-MGATE-1210 (v2.0.0)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python ota_update.py                    # Interactive menu
  python ota_update.py --set-token ghp_xxx  # Set GitHub token
  python ota_update.py --check            # Check for updates only
  python ota_update.py --status           # Get OTA status (enhanced)
  python ota_update.py --monitor          # Monitor OTA progress in real-time
  python ota_update.py --update           # Full update flow
  python ota_update.py --auto             # Auto update (no prompts)

v2.0.0 Features:
  - Real-time progress notifications from device
  - Download speed and ETA display
  - Network mode indicator (WiFi/Ethernet)
  - Retry count monitoring
        """,
    )
    parser.add_argument(
        "--set-token",
        metavar="TOKEN",
        help="Set GitHub Personal Access Token for private repos",
    )
    parser.add_argument(
        "--check", action="store_true", help="Check for updates only (no download)"
    )
    parser.add_argument(
        "--status",
        action="store_true",
        help="Get OTA status with enhanced progress info (v2.0.0)",
    )
    parser.add_argument(
        "--monitor",
        action="store_true",
        help="Monitor OTA progress in real-time (v2.0.0)",
    )
    parser.add_argument(
        "--update", action="store_true", help="Run full OTA update flow"
    )
    parser.add_argument(
        "--auto", action="store_true", help="Auto update without prompts (dangerous!)"
    )
    parser.add_argument(
        "--menu", action="store_true", help="Interactive menu mode (default)"
    )

    return parser.parse_args()


async def run_status_only():
    """Get OTA status only (v2.0.0)"""
    print_header()

    client = None
    try:
        print_step(0, 1, "Connecting to get OTA status", "running")

        device = await scan_for_device()
        if not device:
            print_error(f"No MGate device found")
            return False

        client = await connect_device(device.address)
        if not client:
            print_error("Failed to connect to device")
            return False

        print_success("Connected!")
        await asyncio.sleep(1)

        result = await step_get_status(client)
        return result is not None

    finally:
        if client and client.is_connected:
            try:
                await client.stop_notify(RESPONSE_CHAR_UUID)
                await client.disconnect()
                print(f"\n  {Fore.CYAN}🔌 Disconnected{Style.RESET_ALL}")
            except:
                pass


async def run_monitor():
    """Monitor OTA progress in real-time (v2.0.0)"""
    print_header()

    client = None
    try:
        print_step(0, 1, "Connecting to monitor OTA progress", "running")

        device = await scan_for_device()
        if not device:
            print_error(f"No MGate device found")
            return False

        client = await connect_device(device.address)
        if not client:
            print_error("Failed to connect to device")
            return False

        print_success("Connected!")
        await asyncio.sleep(1)

        result = await step_monitor_status(client)
        return result

    finally:
        if client and client.is_connected:
            try:
                await client.stop_notify(RESPONSE_CHAR_UUID)
                await client.disconnect()
                print(f"\n  {Fore.CYAN}🔌 Disconnected{Style.RESET_ALL}")
            except:
                pass


def main():
    """Entry point (v2.0.0)"""
    args = parse_args()

    try:
        if args.set_token:
            # Set token mode
            if not args.set_token.startswith("ghp_"):
                print(
                    f"{Fore.RED}❌ Invalid token format. Should start with 'ghp_'{Style.RESET_ALL}"
                )
                sys.exit(1)
            result = asyncio.run(run_set_token(args.set_token))
            sys.exit(0 if result else 1)

        elif args.check:
            # Check only mode
            result = asyncio.run(run_check_only())
            sys.exit(0 if result else 1)

        elif args.status:
            # Get OTA status (v2.0.0)
            result = asyncio.run(run_status_only())
            sys.exit(0 if result else 1)

        elif args.monitor:
            # Monitor OTA progress (v2.0.0)
            result = asyncio.run(run_monitor())
            sys.exit(0 if result else 1)

        elif args.update or args.auto:
            # Full update mode
            result = asyncio.run(run_ota_update())
            sys.exit(0 if result else 1)

        elif args.menu:
            # Interactive menu mode
            result = asyncio.run(run_interactive_menu())
            sys.exit(0 if result else 1)

        else:
            # Default: Interactive menu mode
            # Token is auto-configured from GITHUB_TOKEN constant
            result = asyncio.run(run_interactive_menu())
            sys.exit(0 if result else 1)

    except KeyboardInterrupt:
        print(f"\n\n{Fore.YELLOW}⚠️  Interrupted{Style.RESET_ALL}")
        sys.exit(1)
    except Exception as e:
        print(f"\n{Fore.RED}❌ Fatal error: {e}{Style.RESET_ALL}")
        sys.exit(1)


if __name__ == "__main__":
    main()
