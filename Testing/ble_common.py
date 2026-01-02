#!/usr/bin/env python3
"""
=============================================================================
BLE Common Module for SRT-MGATE-1210 Testing (Rich UI Edition)
=============================================================================

Shared BLE functionality for all testing scripts:
  - BLE UUIDs and device name detection
  - Enhanced terminal UI with Rich library
  - Common connection and command handling

Version: 2.1.0
Updated: December 2025
Author: SURIOTA R&D Team

Dependencies:
  pip install bleak rich

=============================================================================
"""

import asyncio
import json
import sys
import time
from datetime import datetime

# =============================================================================
# Rich Library Import (with fallback to basic output)
# =============================================================================
try:
    from rich.console import Console
    from rich.table import Table
    from rich.panel import Panel
    from rich.progress import (
        Progress,
        SpinnerColumn,
        TextColumn,
        BarColumn,
        TaskProgressColumn,
    )
    from rich.text import Text
    from rich.box import ROUNDED, DOUBLE, SIMPLE
    from rich.layout import Layout
    from rich.live import Live
    from rich import print as rprint
    from rich.style import Style
    from rich.theme import Theme

    # Custom theme for SURIOTA
    custom_theme = Theme(
        {
            "info": "cyan",
            "warning": "yellow",
            "error": "bold red",
            "success": "bold green",
            "header": "bold magenta",
            "data": "white",
        }
    )

    console = Console(theme=custom_theme)
    RICH_AVAILABLE = True
except ImportError:
    RICH_AVAILABLE = False
    console = None

    # Fallback to basic print
    def rprint(*args, **kwargs):
        print(*args)


# Colorama fallback for compatibility
try:
    from colorama import init, Fore, Back, Style

    init(autoreset=True)
    COLORAMA_AVAILABLE = True
except ImportError:
    COLORAMA_AVAILABLE = False

    class Fore:
        RED = GREEN = YELLOW = CYAN = MAGENTA = WHITE = BLUE = RESET = ""

    class Back:
        RED = GREEN = YELLOW = CYAN = MAGENTA = WHITE = BLUE = RESET = ""

    class Style:
        BRIGHT = DIM = RESET_ALL = ""


# =============================================================================
# BLE Library Import
# =============================================================================
try:
    from bleak import BleakClient, BleakScanner

    BLEAK_AVAILABLE = True
except ImportError:
    BLEAK_AVAILABLE = False
    if RICH_AVAILABLE:
        console.print(
            "[error]ERROR: bleak module not found. Install with: pip install bleak[/error]"
        )
    else:
        print("ERROR: bleak module not found. Install with: pip install bleak")

# =============================================================================
# BLE Configuration - Updated for v1.0.0+
# =============================================================================
SERVICE_UUID = "00001830-0000-1000-8000-00805f9b34fb"
COMMAND_CHAR_UUID = "11111111-1111-1111-1111-111111111101"
RESPONSE_CHAR_UUID = "11111111-1111-1111-1111-111111111102"

# Device name patterns (checked in order)
# v1.0.0+:   MGate-1210(P)-XXXX or MGate-1210-XXXX
# v2.5.31:   SURIOTA-XXXXXX
# Legacy:    SURIOTA GW
DEVICE_NAME_PREFIX = "MGate-1210"
DEVICE_NAME_LEGACY_PREFIX = "SURIOTA-"
DEVICE_NAME_LEGACY = "SURIOTA GW"

# =============================================================================
# Enhanced UI Functions (Rich-based)
# =============================================================================


def print_header(title, subtitle="", version=""):
    """Print a beautiful header box"""
    if RICH_AVAILABLE:
        header_text = Text()
        header_text.append(f"{title}\n", style="bold white")
        if subtitle:
            header_text.append(f"{subtitle}\n", style="cyan")
        if version:
            header_text.append(f"Version {version}", style="dim")

        console.print()
        console.print(
            Panel(
                header_text,
                title="[bold magenta]SURIOTA R&D[/bold magenta]",
                subtitle=f"[dim]{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}[/dim]",
                box=DOUBLE,
                border_style="magenta",
                padding=(1, 2),
            )
        )
        console.print()
    else:
        print()
        print("=" * 60)
        print(f"  {title}")
        if subtitle:
            print(f"  {subtitle}")
        if version:
            print(f"  Version {version}")
        print("=" * 60)
        print()


def print_section(title, icon=""):
    """Print a section header"""
    if RICH_AVAILABLE:
        console.print()
        console.rule(f"[bold cyan]{icon} {title}[/bold cyan]", style="cyan")
        console.print()
    else:
        print()
        print(f"--- {icon} {title} ---")
        print()


def print_step(step_num, total, message):
    """Print a step indicator"""
    if RICH_AVAILABLE:
        console.print(f"  [dim]Step {step_num}/{total}:[/dim] [white]{message}[/white]")
    else:
        print(f"  Step {step_num}/{total}: {message}")


def print_success(message):
    """Print a success message"""
    if RICH_AVAILABLE:
        console.print(f"  [success][OK][/success] {message}")
    else:
        print(f"  [OK] {message}")


def print_error(message):
    """Print an error message"""
    if RICH_AVAILABLE:
        console.print(f"  [error][ERROR][/error] {message}")
    else:
        print(f"  [ERROR] {message}")


def print_warning(message):
    """Print a warning message"""
    if RICH_AVAILABLE:
        console.print(f"  [warning][WARN][/warning] {message}")
    else:
        print(f"  [WARN] {message}")


def print_info(message):
    """Print an info message"""
    if RICH_AVAILABLE:
        console.print(f"  [info][INFO][/info] {message}")
    else:
        print(f"  [INFO] {message}")


def print_data(label, value, unit=""):
    """Print a data point"""
    if RICH_AVAILABLE:
        if unit:
            console.print(
                f"  [dim]{label}:[/dim] [white]{value}[/white] [cyan]{unit}[/cyan]"
            )
        else:
            console.print(f"  [dim]{label}:[/dim] [white]{value}[/white]")
    else:
        if unit:
            print(f"  {label}: {value} {unit}")
        else:
            print(f"  {label}: {value}")


def print_table(headers, rows, title=""):
    """Print a formatted table"""
    if RICH_AVAILABLE:
        table = Table(
            title=title, box=ROUNDED, border_style="cyan", header_style="bold magenta"
        )

        for header in headers:
            table.add_column(header, justify="left")

        for row in rows:
            str_row = [str(cell) for cell in row]
            table.add_row(*str_row)

        console.print(table)
    else:
        if title:
            print(f"\n  {title}")
            print("  " + "-" * 50)

        # Simple table format
        col_widths = [
            max(len(str(row[i])) for row in [headers] + rows)
            for i in range(len(headers))
        ]

        header_line = "  "
        for i, h in enumerate(headers):
            header_line += f"{h:<{col_widths[i]+2}}"
        print(header_line)
        print("  " + "-" * sum(col_widths))

        for row in rows:
            row_line = "  "
            for i, cell in enumerate(row):
                row_line += f"{str(cell):<{col_widths[i]+2}}"
            print(row_line)


def print_box(title, content, style="info"):
    """Print content in a box"""
    if RICH_AVAILABLE:
        if isinstance(content, dict):
            text = Text()
            for key, value in content.items():
                text.append(f"{key}: ", style="dim")
                text.append(f"{value}\n", style="white")
        elif isinstance(content, list):
            text = "\n".join(content)
        else:
            text = str(content)

        border_style = (
            "cyan" if style == "info" else "yellow" if style == "warning" else "red"
        )
        console.print(
            Panel(
                text,
                title=f"[bold]{title}[/bold]",
                border_style=border_style,
                padding=(0, 2),
            )
        )
    else:
        print(f"\n  +--- {title} ---+")
        if isinstance(content, dict):
            for key, value in content.items():
                print(f"  | {key}: {value}")
        elif isinstance(content, list):
            for line in content:
                print(f"  | {line}")
        else:
            print(f"  | {content}")
        print(f"  +{'-' * (len(title) + 10)}+")


def print_progress_bar(progress, total=100, prefix="Progress", suffix=""):
    """Print a progress bar (for non-Rich fallback)"""
    if RICH_AVAILABLE:
        # Rich uses different progress handling, this is for simple inline progress
        filled = int(30 * progress / total)
        bar = "█" * filled + "░" * (30 - filled)
        console.print(f"\r  {prefix}: [{bar}] {progress}% {suffix}", end="")
    else:
        filled = int(30 * progress / total)
        bar = "█" * filled + "░" * (30 - filled)
        print(f"\r  {prefix}: [{bar}] {progress}% {suffix}", end="")


def print_summary(title, data, success=True):
    """Print a summary box"""
    if RICH_AVAILABLE:
        text = Text()
        for key, value in data.items():
            text.append(f"{key}: ", style="dim")
            text.append(f"{value}\n", style="bold white")

        border_style = "green" if success else "red"
        status = (
            "[bold green]SUCCESS[/bold green]"
            if success
            else "[bold red]FAILED[/bold red]"
        )

        console.print()
        console.print(
            Panel(
                text,
                title=f"[bold]{title}[/bold] - {status}",
                border_style=border_style,
                box=DOUBLE,
                padding=(1, 2),
            )
        )
    else:
        status = "[SUCCESS]" if success else "[FAILED]"
        print()
        print(f"  === {title} - {status} ===")
        for key, value in data.items():
            print(f"    {key}: {value}")
        print()


def countdown(seconds, message="Starting in"):
    """Display a countdown"""
    if RICH_AVAILABLE:
        with Progress(
            SpinnerColumn(),
            TextColumn("[progress.description]{task.description}"),
            transient=True,
        ) as progress:
            task = progress.add_task(f"{message} {seconds}s...", total=seconds)
            for i in range(seconds, 0, -1):
                progress.update(task, description=f"{message} {i}s...")
                time.sleep(1)
                progress.advance(task)
    else:
        for i in range(seconds, 0, -1):
            print(f"\r  {message} {i}s...", end="")
            time.sleep(1)
        print()


def create_progress():
    """Create a Rich progress bar context manager"""
    if RICH_AVAILABLE:
        return Progress(
            SpinnerColumn(),
            TextColumn("[progress.description]{task.description}"),
            BarColumn(),
            TaskProgressColumn(),
            console=console,
            transient=False,
        )
    return None


# =============================================================================
# Dependency Check
# =============================================================================
def check_dependencies():
    """Check if all required dependencies are installed"""
    missing = []

    if not BLEAK_AVAILABLE:
        missing.append("bleak")

    if missing:
        print_error(f"Missing dependencies: {', '.join(missing)}")
        print_info("Install with: pip install " + " ".join(missing))
        return False

    if not RICH_AVAILABLE:
        print_warning("Rich library not found - using basic output")
        print_info("For enhanced UI, install: pip install rich")

    return True


# =============================================================================
# BLE Device Client Class
# =============================================================================
class BLEDeviceClient:
    """BLE client for communicating with SRT-MGATE-1210 Gateway"""

    def __init__(self):
        self.client = None
        self.device = None
        self.response_buffer = []  # Use list for chunks
        self.response_complete = asyncio.Event()

    def _is_target_device(self, name):
        """Check if device name matches our target patterns"""
        if not name:
            return False

        # v1.0.0+: MGate-1210(P)-XXXX or MGate-1210-XXXX
        if name.startswith(DEVICE_NAME_PREFIX):
            return True

        # v2.5.31: SURIOTA-XXXXXX
        if name.startswith(DEVICE_NAME_LEGACY_PREFIX):
            return True

        # Legacy: SURIOTA GW
        if name == DEVICE_NAME_LEGACY:
            return True

        return False

    async def scan_all_devices(self, timeout=10):
        """Scan for all MGate devices and return list"""
        print_info(f"Scanning for MGate devices... (timeout: {timeout}s)")

        found_devices = []

        if RICH_AVAILABLE:
            with Progress(
                SpinnerColumn(),
                TextColumn("[progress.description]{task.description}"),
                console=console,
                transient=True,
            ) as progress:
                task = progress.add_task("Scanning BLE devices...", total=None)
                devices = await BleakScanner.discover(timeout=timeout)

                for device in devices:
                    if self._is_target_device(device.name):
                        found_devices.append(device)
                        progress.update(
                            task, description=f"Found {len(found_devices)} device(s)..."
                        )
        else:
            devices = await BleakScanner.discover(timeout=timeout)
            for device in devices:
                if self._is_target_device(device.name):
                    found_devices.append(device)

        return found_devices

    def _select_device(self, devices):
        """Display device selection menu and return selected device"""
        if not devices:
            return None

        if len(devices) == 1:
            print_info(f"Found 1 gateway: {devices[0].name}")
            return devices[0]

        # Multiple devices found - show selection menu
        print_section("Gateway Selection", "")
        print_info(f"Found {len(devices)} gateways:")
        print()

        if RICH_AVAILABLE:
            table = Table(title="Available Gateways", box=ROUNDED, border_style="cyan")
            table.add_column("#", justify="center", style="bold yellow", width=4)
            table.add_column("Name", style="bold white")
            table.add_column("MAC Address", style="cyan")
            table.add_column("RSSI", justify="right", style="dim")

            for idx, device in enumerate(devices, 1):
                rssi = getattr(device, "rssi", "N/A")
                rssi_str = f"{rssi} dBm" if rssi != "N/A" else "N/A"
                table.add_row(
                    str(idx), device.name or "Unknown", device.address, rssi_str
                )

            console.print(table)
        else:
            print("  +-----+------------------------+-------------------+----------+")
            print("  |  #  | Name                   | MAC Address       | RSSI     |")
            print("  +-----+------------------------+-------------------+----------+")
            for idx, device in enumerate(devices, 1):
                rssi = getattr(device, "rssi", "N/A")
                rssi_str = f"{rssi} dBm" if rssi != "N/A" else "N/A"
                name = (device.name or "Unknown")[:22]
                print(
                    f"  | {idx:^3} | {name:<22} | {device.address:<17} | {rssi_str:<8} |"
                )
            print("  +-----+------------------------+-------------------+----------+")

        print()

        # Get user selection
        while True:
            try:
                if RICH_AVAILABLE:
                    console.print(
                        "[bold yellow]Select gateway (1-{0}):[/bold yellow] ".format(
                            len(devices)
                        ),
                        end="",
                    )
                else:
                    print(f"  Select gateway (1-{len(devices)}): ", end="")

                choice = input().strip()

                if choice.lower() == "q":
                    print_info("Selection cancelled")
                    return None

                idx = int(choice) - 1
                if 0 <= idx < len(devices):
                    selected = devices[idx]
                    print()
                    print_success(f"Selected: {selected.name} ({selected.address})")
                    return selected
                else:
                    print_warning(
                        f"Invalid selection. Enter 1-{len(devices)} or 'q' to cancel"
                    )
            except ValueError:
                print_warning("Please enter a number")
            except KeyboardInterrupt:
                print()
                print_info("Selection cancelled")
                return None

    async def scan_for_device(self, timeout=10, auto_select=False):
        """Scan for MGate device with optional auto-select

        Args:
            timeout: Scan timeout in seconds
            auto_select: If True, automatically select first device found (legacy behavior)
                        If False, show selection menu when multiple devices found
        """
        devices = await self.scan_all_devices(timeout)

        if not devices:
            return None

        if auto_select:
            # Legacy behavior - return first device
            return devices[0]

        # New behavior - let user select
        return self._select_device(devices)

    def _notification_handler(self, sender, data):
        """Handle BLE notifications"""
        try:
            chunk = data.decode("utf-8")

            # Check for end marker
            if chunk == "<END>":
                self.response_complete.set()
            else:
                self.response_buffer.append(chunk)
                # Show progress dot
                if RICH_AVAILABLE:
                    console.print(".", end="", style="cyan")
                else:
                    print(".", end="", flush=True)
        except Exception as e:
            print_warning(f"Notification decode error: {e}")

    async def connect(self, timeout=10, auto_select=False):
        """Connect to MGate device

        Args:
            timeout: Scan timeout in seconds
            auto_select: If True, automatically connect to first device found
                        If False (default), show selection menu when multiple devices found
        """
        self.device = await self.scan_for_device(timeout, auto_select=auto_select)

        if not self.device:
            print_error("MGate device not found")
            print_info("Make sure:")
            print_info("  - Device is powered on")
            print_info("  - BLE is enabled (button press or always-on)")
            print_info("  - Device is within range")
            return False

        print_success(f"Found device: {self.device.name}")
        print_info(f"Address: {self.device.address}")

        try:
            if RICH_AVAILABLE:
                with Progress(
                    SpinnerColumn(),
                    TextColumn("[progress.description]{task.description}"),
                    console=console,
                    transient=True,
                ) as progress:
                    task = progress.add_task("Connecting...", total=None)

                    self.client = BleakClient(self.device.address)
                    await self.client.connect()

                    progress.update(task, description="Subscribing to notifications...")
                    await self.client.start_notify(
                        RESPONSE_CHAR_UUID, self._notification_handler
                    )
            else:
                print_info("Connecting...")
                self.client = BleakClient(self.device.address)
                await self.client.connect()
                await self.client.start_notify(
                    RESPONSE_CHAR_UUID, self._notification_handler
                )

            print_success("Connected and subscribed to notifications")
            return True

        except Exception as e:
            print_error(f"Connection failed: {e}")
            return False

    async def send_command(self, command, timeout=30):
        """Send command and wait for response"""
        if not self.client or not self.client.is_connected:
            print_error("Not connected")
            return None

        # Reset buffer
        self.response_buffer = []
        self.response_complete.clear()

        try:
            # Send command in chunks (18 bytes like original)
            cmd_bytes = command.encode("utf-8")
            chunk_size = 18

            print_info(f"Sending command...")

            for i in range(0, len(cmd_bytes), chunk_size):
                chunk = cmd_bytes[i : i + chunk_size]
                await self.client.write_gatt_char(COMMAND_CHAR_UUID, chunk)
                await asyncio.sleep(0.05)

            # Send end marker
            await self.client.write_gatt_char(COMMAND_CHAR_UUID, b"<END>")

            # Wait for response
            try:
                await asyncio.wait_for(self.response_complete.wait(), timeout=timeout)
                print()  # New line after dots

                # Join buffer and parse JSON
                full_response = "".join(self.response_buffer)
                return json.loads(full_response)

            except asyncio.TimeoutError:
                print()
                print_warning(f"Response timeout after {timeout}s")
                if self.response_buffer:
                    partial = "".join(self.response_buffer)
                    print_info(f"Partial response: {partial[:100]}...")
                return None
            except json.JSONDecodeError as e:
                print()
                full_response = "".join(self.response_buffer)
                print_error(f"Invalid JSON response: {e}")
                print_info(f"Raw response: {full_response[:200]}...")
                return None

        except Exception as e:
            print_error(f"Command failed: {e}")
            return None

    async def create_device(self, config, name=""):
        """Create a new Modbus device"""
        # Correct command format: op, type, device_id, config
        cmd = json.dumps(
            {"op": "create", "type": "device", "device_id": None, "config": config},
            separators=(",", ":"),
        )

        if RICH_AVAILABLE:
            with Progress(
                SpinnerColumn(),
                TextColumn("[progress.description]{task.description}"),
                console=console,
                transient=True,
            ) as progress:
                task = progress.add_task(f"Creating device: {name}...", total=None)
                response = await self.send_command(cmd)
        else:
            print_info(f"Creating device: {name}...")
            response = await self.send_command(cmd)

        if response and response.get("status") == "ok":
            device_id = response.get("device_id")
            print_success(f"Device created: {device_id}")
            return device_id
        else:
            error = (
                response.get("error", "Unknown error") if response else "No response"
            )
            print_error(f"Failed to create device: {error}")
            return None

    async def create_register(self, device_id, config, name=""):
        """Create a new register for a device"""
        # Correct command format: op, type, device_id, config
        cmd = json.dumps(
            {
                "op": "create",
                "type": "register",
                "device_id": device_id,
                "config": config,
            },
            separators=(",", ":"),
        )

        response = await self.send_command(cmd, timeout=15)

        if response and response.get("status") == "ok":
            return True
        else:
            error = (
                response.get("error", "Unknown error") if response else "No response"
            )
            print_warning(f"Register creation issue: {error}")
            return False

    async def disconnect(self):
        """Disconnect from device"""
        if self.client and self.client.is_connected:
            try:
                await self.client.stop_notify(RESPONSE_CHAR_UUID)
                await self.client.disconnect()
                print_info("Disconnected from device")
            except Exception as e:
                print_warning(f"Disconnect warning: {e}")


# =============================================================================
# Utility Functions
# =============================================================================
def format_bytes(size):
    """Format bytes to human readable"""
    for unit in ["B", "KB", "MB", "GB"]:
        if size < 1024:
            return f"{size:.1f} {unit}"
        size /= 1024
    return f"{size:.1f} TB"


def format_duration(seconds):
    """Format seconds to human readable duration"""
    if seconds < 60:
        return f"{seconds:.1f}s"
    elif seconds < 3600:
        mins = seconds // 60
        secs = seconds % 60
        return f"{int(mins)}m {int(secs)}s"
    else:
        hours = seconds // 3600
        mins = (seconds % 3600) // 60
        return f"{int(hours)}h {int(mins)}m"


# =============================================================================
# Export for other modules
# =============================================================================
__all__ = [
    # BLE
    "BLEDeviceClient",
    "SERVICE_UUID",
    "COMMAND_CHAR_UUID",
    "RESPONSE_CHAR_UUID",
    "DEVICE_NAME_PREFIX",
    "DEVICE_NAME_LEGACY_PREFIX",
    "DEVICE_NAME_LEGACY",
    "check_dependencies",
    # Note: BLEDeviceClient now supports gateway selection:
    #   - scan_all_devices(timeout): Returns list of all found gateways
    #   - scan_for_device(timeout, auto_select=False): Shows selection menu
    #   - connect(timeout, auto_select=False): Connect with selection menu
    # UI Functions
    "print_header",
    "print_section",
    "print_step",
    "print_success",
    "print_error",
    "print_warning",
    "print_info",
    "print_data",
    "print_progress_bar",
    "print_table",
    "print_box",
    "print_summary",
    "countdown",
    "create_progress",
    # Utilities
    "format_bytes",
    "format_duration",
    # Rich objects (for advanced usage)
    "console",
    "RICH_AVAILABLE",
    # Colorama compatibility
    "Fore",
    "Style",
    "COLORAMA_AVAILABLE",
]
