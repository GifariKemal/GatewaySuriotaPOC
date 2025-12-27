#!/usr/bin/env python3
"""
=============================================================================
Simulator Common Module for Modbus Slave Simulators (Rich UI Edition)
=============================================================================

Shared functionality for Modbus RTU/TCP slave simulators:
  - Enhanced terminal UI with Rich library
  - Common display functions for register values
  - pymodbus version compatibility

Version: 2.1.0
Updated: December 2025
Author: SURIOTA R&D Team

Dependencies:
  pip install pymodbus pyserial rich

=============================================================================
"""

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
    from rich.progress import Progress, SpinnerColumn, TextColumn
    from rich.text import Text
    from rich.box import ROUNDED, DOUBLE, SIMPLE, HEAVY
    from rich.live import Live
    from rich.layout import Layout
    from rich.style import Style
    from rich.theme import Theme

    # Custom theme for SURIOTA simulators
    custom_theme = Theme({
        "info": "cyan",
        "warning": "yellow",
        "error": "bold red",
        "success": "bold green",
        "header": "bold magenta",
        "data": "white",
        "register": "bold cyan",
        "value": "bold white",
        "unit": "dim cyan",
    })

    console = Console(theme=custom_theme)
    RICH_AVAILABLE = True
except ImportError:
    RICH_AVAILABLE = False
    console = None

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
        console.print(Panel(
            header_text,
            title="[bold magenta]SURIOTA Modbus Simulator[/bold magenta]",
            subtitle=f"[dim]{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}[/dim]",
            box=DOUBLE,
            border_style="magenta",
            padding=(1, 2)
        ))
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


def print_table(headers, rows, title=""):
    """Print a formatted table"""
    if RICH_AVAILABLE:
        table = Table(title=title, box=ROUNDED, border_style="cyan", header_style="bold magenta")

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
        col_widths = [max(len(str(row[i])) for row in [headers] + rows) for i in range(len(headers))]

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

        border_style = "cyan" if style == "info" else "yellow" if style == "warning" else "red"
        console.print(Panel(text, title=f"[bold]{title}[/bold]", border_style=border_style, padding=(0, 2)))
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


def print_register_values(register_info, values, update_count=0, slave_id=1):
    """Print register values in a formatted display"""
    if RICH_AVAILABLE:
        # Create a table for register values
        table = Table(
            title=f"Update #{update_count:04d} | Slave ID: {slave_id}",
            box=HEAVY,
            border_style="cyan",
            header_style="bold magenta",
            title_style="bold white"
        )

        table.add_column("Addr", justify="right", style="dim", width=5)
        table.add_column("Name", justify="left", style="register", width=18)
        table.add_column("Value", justify="right", style="value", width=8)
        table.add_column("Unit", justify="left", style="unit", width=8)
        table.add_column("Range", justify="center", style="dim", width=12)

        num_to_show = min(12, len(register_info))
        for addr in range(num_to_show):
            if addr in register_info:
                info = register_info[addr]
                value = values[addr] if addr < len(values) else 0
                table.add_row(
                    str(addr),
                    info['name'][:17],
                    str(value),
                    info.get('unit', ''),
                    f"{info.get('min', 0)}-{info.get('max', 100)}"
                )

        if len(register_info) > num_to_show:
            table.add_row("...", f"+ {len(register_info) - num_to_show} more", "...", "", "")

        console.print()
        console.print(table)
        console.print()
    else:
        print()
        print("=" * 60)
        print(f"  Update #{update_count:04d} | Slave ID: {slave_id}")
        print("=" * 60)

        num_to_show = min(10, len(register_info))
        for addr in range(num_to_show):
            if addr in register_info:
                info = register_info[addr]
                value = values[addr] if addr < len(values) else 0
                print(f"  [{addr:2d}] {info['name'][:15]:<15s}: {value:5d} {info.get('unit', '')}")

        if len(register_info) > num_to_show:
            print(f"  ... and {len(register_info) - num_to_show} more registers")

        print("=" * 60)
        print()


def print_connection_info(protocol, config):
    """Print connection configuration info"""
    if RICH_AVAILABLE:
        text = Text()

        if protocol == "TCP":
            text.append("Protocol: ", style="dim")
            text.append("Modbus TCP\n", style="bold cyan")
            text.append("IP Address: ", style="dim")
            text.append(f"{config.get('ip', '0.0.0.0')}\n", style="bold white")
            text.append("Port: ", style="dim")
            text.append(f"{config.get('port', 502)}\n", style="bold white")
        else:  # RTU
            text.append("Protocol: ", style="dim")
            text.append("Modbus RTU\n", style="bold cyan")
            text.append("Serial Port: ", style="dim")
            text.append(f"{config.get('port', 'COM1')}\n", style="bold white")
            text.append("Baud Rate: ", style="dim")
            text.append(f"{config.get('baud_rate', 9600)}\n", style="bold white")
            text.append("Format: ", style="dim")
            text.append(f"{config.get('data_bits', 8)}{config.get('parity', 'N')}{config.get('stop_bits', 1)}\n", style="bold white")

        text.append("Slave ID: ", style="dim")
        text.append(f"{config.get('slave_id', 1)}\n", style="bold white")
        text.append("Registers: ", style="dim")
        text.append(f"{config.get('num_registers', 0)}", style="bold green")

        console.print(Panel(
            text,
            title="[bold]Connection Configuration[/bold]",
            border_style="cyan",
            padding=(0, 2)
        ))
    else:
        print(f"\n  Connection Configuration:")
        if protocol == "TCP":
            print(f"    Protocol: Modbus TCP")
            print(f"    IP: {config.get('ip', '0.0.0.0')}:{config.get('port', 502)}")
        else:
            print(f"    Protocol: Modbus RTU")
            print(f"    Port: {config.get('port', 'COM1')}")
            print(f"    Baud: {config.get('baud_rate', 9600)}")
        print(f"    Slave ID: {config.get('slave_id', 1)}")
        print(f"    Registers: {config.get('num_registers', 0)}")
        print()


def print_startup_banner(protocol, num_registers):
    """Print startup banner"""
    if RICH_AVAILABLE:
        banner = Text()
        banner.append("MODBUS ", style="bold magenta")
        banner.append(protocol, style="bold cyan")
        banner.append(" SLAVE SIMULATOR\n", style="bold magenta")
        banner.append(f"{num_registers} Input Registers", style="white")

        console.print()
        console.print(Panel(
            banner,
            box=DOUBLE,
            border_style="magenta",
            padding=(1, 4)
        ))
    else:
        print()
        print("=" * 60)
        print(f"  MODBUS {protocol} SLAVE SIMULATOR")
        print(f"  {num_registers} Input Registers")
        print("=" * 60)
        print()


def print_ready_message(protocol, config):
    """Print ready message with connection details"""
    if RICH_AVAILABLE:
        if protocol == "TCP":
            addr = f"{config.get('ip', '0.0.0.0')}:{config.get('port', 502)}"
        else:
            addr = config.get('port', 'COM1')

        text = Text()
        text.append("Server READY\n", style="bold green")
        text.append(f"Listening on: ", style="dim")
        text.append(f"{addr}\n", style="bold white")
        text.append("Press ", style="dim")
        text.append("Ctrl+C", style="bold yellow")
        text.append(" to stop", style="dim")

        console.print()
        console.print(Panel(
            text,
            border_style="green",
            padding=(0, 2)
        ))
        console.print()
    else:
        if protocol == "TCP":
            addr = f"{config.get('ip', '0.0.0.0')}:{config.get('port', 502)}"
        else:
            addr = config.get('port', 'COM1')

        print()
        print(f"  [READY] Server listening on {addr}")
        print("  Press Ctrl+C to stop")
        print()


def print_dependencies():
    """Print dependency check info"""
    deps = []

    # Check pymodbus
    try:
        import pymodbus
        version = getattr(pymodbus, '__version__', 'unknown')
        deps.append(("pymodbus", version, True))
    except ImportError:
        deps.append(("pymodbus", "NOT FOUND", False))

    # Check pyserial
    try:
        import serial
        version = getattr(serial, '__version__', 'unknown')
        deps.append(("pyserial", version, True))
    except ImportError:
        deps.append(("pyserial", "NOT FOUND", False))

    # Check rich
    if RICH_AVAILABLE:
        deps.append(("rich", "installed", True))
    else:
        deps.append(("rich", "NOT FOUND (optional)", False))

    if RICH_AVAILABLE:
        table = Table(title="Dependencies", box=SIMPLE, border_style="dim")
        table.add_column("Package", style="white")
        table.add_column("Version", style="cyan")
        table.add_column("Status", style="white")

        for name, version, ok in deps:
            status = "[green]OK[/green]" if ok else "[red]MISSING[/red]"
            table.add_row(name, version, status)

        console.print(table)
    else:
        print("\n  Dependencies:")
        for name, version, ok in deps:
            status = "[OK]" if ok else "[MISSING]"
            print(f"    {name}: {version} {status}")
        print()


def create_live_display():
    """Create a Rich Live display context for real-time updates"""
    if RICH_AVAILABLE:
        return Live(console=console, refresh_per_second=2, transient=True)
    return None


# =============================================================================
# Export for other modules
# =============================================================================
__all__ = [
    # UI Functions
    'print_header', 'print_section',
    'print_success', 'print_error', 'print_warning', 'print_info',
    'print_table', 'print_box',
    'print_register_values', 'print_connection_info',
    'print_startup_banner', 'print_ready_message',
    'print_dependencies', 'create_live_display',

    # Rich objects (for advanced usage)
    'console', 'RICH_AVAILABLE',

    # Colorama compatibility
    'Fore', 'Style', 'COLORAMA_AVAILABLE'
]
