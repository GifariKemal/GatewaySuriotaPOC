#!/usr/bin/env python3
"""
MQTT Subscribe Control - GUI Testing Application
=================================================
Version: 1.1.0
Date: January 2026

Interactive GUI for testing MQTT Subscribe Control feature.
Allows sending write commands to gateway registers via MQTT.

Features:
- Connect to any MQTT broker
- Configure gateway/device/register targets
- Send write commands with various payload formats
- View request/response in real-time
- JSON syntax highlighting
- Connection status monitoring
- Message history log

Requirements:
    pip install paho-mqtt

Usage:
    python mqtt_subscribe_control_gui.py

Author: SURIOTA R&D Team
"""

import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import json
import time
import threading
import queue
from datetime import datetime
from typing import Optional, Callable

# Try to import paho-mqtt
try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("=" * 60)
    print("ERROR: paho-mqtt library not found!")
    print("Please install it using: pip install paho-mqtt")
    print("=" * 60)
    exit(1)


# ============================================================================
# CONSTANTS & STYLING
# ============================================================================

APP_TITLE = "MQTT Subscribe Control - GUI Tester v1.1.0"
APP_WIDTH = 1200
APP_HEIGHT = 800

# Color scheme (Modern Dark Theme)
COLORS = {
    "bg_dark": "#1e1e1e",
    "bg_medium": "#252526",
    "bg_light": "#2d2d30",
    "bg_input": "#3c3c3c",
    "fg_primary": "#ffffff",
    "fg_secondary": "#cccccc",
    "fg_muted": "#808080",
    "accent_blue": "#0078d4",
    "accent_green": "#4ec9b0",
    "accent_orange": "#ce9178",
    "accent_red": "#f14c4c",
    "accent_yellow": "#dcdcaa",
    "accent_purple": "#c586c0",
    "border": "#3c3c3c",
    "success": "#4caf50",
    "error": "#f44336",
    "warning": "#ff9800",
}

# JSON syntax colors
JSON_COLORS = {
    "key": "#9cdcfe",
    "string": "#ce9178",
    "number": "#b5cea8",
    "boolean": "#569cd6",
    "null": "#569cd6",
    "bracket": "#ffd700",
}


# ============================================================================
# MQTT CLIENT WRAPPER
# ============================================================================

class MQTTClientWrapper:
    """Wrapper for paho-mqtt client with thread-safe message handling."""

    def __init__(self, message_callback: Callable[[str, str], None]):
        self.client: Optional[mqtt.Client] = None
        self.connected = False
        self.message_callback = message_callback
        self.message_queue = queue.Queue()

    def connect(self, broker: str, port: int, client_id: str,
                username: str = "", password: str = "") -> bool:
        """Connect to MQTT broker."""
        try:
            # Create new client instance
            self.client = mqtt.Client(client_id=client_id, protocol=mqtt.MQTTv311)

            # Set credentials if provided
            if username and password:
                self.client.username_pw_set(username, password)

            # Set callbacks
            self.client.on_connect = self._on_connect
            self.client.on_disconnect = self._on_disconnect
            self.client.on_message = self._on_message

            # Connect
            self.client.connect(broker, port, keepalive=60)
            self.client.loop_start()

            # Wait for connection
            timeout = 5
            start = time.time()
            while not self.connected and (time.time() - start) < timeout:
                time.sleep(0.1)

            return self.connected

        except Exception as e:
            print(f"Connection error: {e}")
            return False

    def disconnect(self):
        """Disconnect from MQTT broker."""
        if self.client:
            self.client.loop_stop()
            self.client.disconnect()
            self.connected = False

    def subscribe(self, topic: str, qos: int = 1) -> bool:
        """Subscribe to a topic."""
        if self.client and self.connected:
            result, _ = self.client.subscribe(topic, qos)
            return result == mqtt.MQTT_ERR_SUCCESS
        return False

    def publish(self, topic: str, payload: str, qos: int = 1) -> bool:
        """Publish a message."""
        if self.client and self.connected:
            result = self.client.publish(topic, payload, qos)
            return result.rc == mqtt.MQTT_ERR_SUCCESS
        return False

    def _on_connect(self, client, userdata, flags, rc):
        """Callback when connected."""
        if rc == 0:
            self.connected = True
            print("Connected to MQTT broker")
        else:
            print(f"Connection failed with code: {rc}")

    def _on_disconnect(self, client, userdata, rc):
        """Callback when disconnected."""
        self.connected = False
        print("Disconnected from MQTT broker")

    def _on_message(self, client, userdata, msg):
        """Callback when message received."""
        try:
            payload = msg.payload.decode('utf-8')
            self.message_callback(msg.topic, payload)
        except Exception as e:
            print(f"Message decode error: {e}")


# ============================================================================
# CUSTOM WIDGETS
# ============================================================================

class JsonTextWidget(scrolledtext.ScrolledText):
    """Text widget with JSON syntax highlighting."""

    def __init__(self, parent, **kwargs):
        super().__init__(parent, **kwargs)

        # Configure tags for syntax highlighting
        self.tag_configure("key", foreground=JSON_COLORS["key"])
        self.tag_configure("string", foreground=JSON_COLORS["string"])
        self.tag_configure("number", foreground=JSON_COLORS["number"])
        self.tag_configure("boolean", foreground=JSON_COLORS["boolean"])
        self.tag_configure("null", foreground=JSON_COLORS["null"])
        self.tag_configure("bracket", foreground=JSON_COLORS["bracket"])

    def set_json(self, data, indent: int = 2):
        """Set JSON content with syntax highlighting."""
        self.config(state=tk.NORMAL)
        self.delete("1.0", tk.END)

        if isinstance(data, str):
            try:
                data = json.loads(data)
            except json.JSONDecodeError:
                self.insert(tk.END, data)
                self.config(state=tk.DISABLED)
                return

        json_str = json.dumps(data, indent=indent, ensure_ascii=False)
        self.insert(tk.END, json_str)
        self._apply_syntax_highlighting()
        self.config(state=tk.DISABLED)

    def _apply_syntax_highlighting(self):
        """Apply syntax highlighting to JSON content."""
        content = self.get("1.0", tk.END)

        # Remove all existing tags
        for tag in ["key", "string", "number", "boolean", "null", "bracket"]:
            self.tag_remove(tag, "1.0", tk.END)

        # Highlight brackets
        for char in "{}[]":
            start = "1.0"
            while True:
                pos = self.search(char, start, tk.END)
                if not pos:
                    break
                self.tag_add("bracket", pos, f"{pos}+1c")
                start = f"{pos}+1c"

        # Highlight strings (keys and values)
        import re
        for match in re.finditer(r'"([^"\\]|\\.)*"', content):
            start_idx = f"1.0+{match.start()}c"
            end_idx = f"1.0+{match.end()}c"

            # Check if it's a key (followed by :)
            after_match = content[match.end():match.end() + 2].strip()
            if after_match.startswith(":"):
                self.tag_add("key", start_idx, end_idx)
            else:
                self.tag_add("string", start_idx, end_idx)

        # Highlight numbers
        for match in re.finditer(r'\b-?\d+\.?\d*\b', content):
            start_idx = f"1.0+{match.start()}c"
            end_idx = f"1.0+{match.end()}c"
            self.tag_add("number", start_idx, end_idx)

        # Highlight booleans
        for match in re.finditer(r'\b(true|false)\b', content):
            start_idx = f"1.0+{match.start()}c"
            end_idx = f"1.0+{match.end()}c"
            self.tag_add("boolean", start_idx, end_idx)

        # Highlight null
        for match in re.finditer(r'\bnull\b', content):
            start_idx = f"1.0+{match.start()}c"
            end_idx = f"1.0+{match.end()}c"
            self.tag_add("null", start_idx, end_idx)


class StatusIndicator(tk.Canvas):
    """Connection status indicator widget."""

    def __init__(self, parent, size: int = 12, **kwargs):
        super().__init__(parent, width=size, height=size,
                         highlightthickness=0, **kwargs)
        self.size = size
        self.set_status("disconnected")

    def set_status(self, status: str):
        """Set status: connected, disconnected, connecting."""
        self.delete("all")
        colors = {
            "connected": COLORS["success"],
            "disconnected": COLORS["error"],
            "connecting": COLORS["warning"],
        }
        color = colors.get(status, COLORS["fg_muted"])
        padding = 2
        self.create_oval(padding, padding, self.size - padding,
                         self.size - padding, fill=color, outline="")


# ============================================================================
# MAIN APPLICATION
# ============================================================================

class MQTTSubscribeControlGUI:
    """Main GUI Application for MQTT Subscribe Control Testing."""

    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title(APP_TITLE)
        self.root.geometry(f"{APP_WIDTH}x{APP_HEIGHT}")
        self.root.configure(bg=COLORS["bg_dark"])

        # MQTT client
        self.mqtt_client = MQTTClientWrapper(self._on_mqtt_message)

        # Message history
        self.message_history = []

        # Apply dark theme
        self._setup_styles()

        # Build UI
        self._build_ui()

        # Bind window close
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    def _setup_styles(self):
        """Setup ttk styles for dark theme."""
        style = ttk.Style()
        style.theme_use('clam')

        # Configure styles
        style.configure("Dark.TFrame", background=COLORS["bg_dark"])
        style.configure("Medium.TFrame", background=COLORS["bg_medium"])
        style.configure("Light.TFrame", background=COLORS["bg_light"])

        style.configure("Dark.TLabel",
                        background=COLORS["bg_dark"],
                        foreground=COLORS["fg_primary"],
                        font=("Segoe UI", 10))

        style.configure("Header.TLabel",
                        background=COLORS["bg_dark"],
                        foreground=COLORS["fg_primary"],
                        font=("Segoe UI", 12, "bold"))

        style.configure("Muted.TLabel",
                        background=COLORS["bg_dark"],
                        foreground=COLORS["fg_muted"],
                        font=("Segoe UI", 9))

        style.configure("Dark.TEntry",
                        fieldbackground=COLORS["bg_input"],
                        foreground=COLORS["fg_primary"],
                        insertcolor=COLORS["fg_primary"])

        style.configure("Dark.TButton",
                        background=COLORS["accent_blue"],
                        foreground=COLORS["fg_primary"],
                        font=("Segoe UI", 10, "bold"),
                        padding=(15, 8))

        style.map("Dark.TButton",
                  background=[("active", "#1a8cff"), ("disabled", COLORS["bg_light"])])

        style.configure("Success.TButton",
                        background=COLORS["success"],
                        foreground=COLORS["fg_primary"],
                        font=("Segoe UI", 10, "bold"),
                        padding=(15, 8))

        style.configure("Danger.TButton",
                        background=COLORS["error"],
                        foreground=COLORS["fg_primary"],
                        font=("Segoe UI", 10, "bold"),
                        padding=(15, 8))

        style.configure("Dark.TLabelframe",
                        background=COLORS["bg_dark"],
                        foreground=COLORS["fg_primary"])

        style.configure("Dark.TLabelframe.Label",
                        background=COLORS["bg_dark"],
                        foreground=COLORS["accent_blue"],
                        font=("Segoe UI", 10, "bold"))

        style.configure("Dark.TCombobox",
                        fieldbackground=COLORS["bg_input"],
                        background=COLORS["bg_input"],
                        foreground=COLORS["fg_primary"])

        style.configure("Dark.TRadiobutton",
                        background=COLORS["bg_dark"],
                        foreground=COLORS["fg_primary"])

    def _build_ui(self):
        """Build the main UI."""
        # Main container
        main_frame = ttk.Frame(self.root, style="Dark.TFrame")
        main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)

        # Header
        self._build_header(main_frame)

        # Content area (split into left and right panels)
        content_frame = ttk.Frame(main_frame, style="Dark.TFrame")
        content_frame.pack(fill=tk.BOTH, expand=True, pady=(10, 0))

        # Left panel (Configuration)
        left_panel = ttk.Frame(content_frame, style="Dark.TFrame", width=400)
        left_panel.pack(side=tk.LEFT, fill=tk.Y, padx=(0, 10))
        left_panel.pack_propagate(False)

        self._build_connection_panel(left_panel)
        self._build_target_panel(left_panel)
        self._build_payload_panel(left_panel)

        # Right panel (Request/Response)
        right_panel = ttk.Frame(content_frame, style="Dark.TFrame")
        right_panel.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        self._build_request_response_panel(right_panel)

        # Bottom panel (Log)
        self._build_log_panel(main_frame)

    def _build_header(self, parent):
        """Build header section."""
        header_frame = ttk.Frame(parent, style="Dark.TFrame")
        header_frame.pack(fill=tk.X, pady=(0, 10))

        # Title
        title_label = ttk.Label(header_frame,
                                text="🔌 MQTT Subscribe Control Tester",
                                style="Header.TLabel",
                                font=("Segoe UI", 16, "bold"))
        title_label.pack(side=tk.LEFT)

        # Status indicator
        status_frame = ttk.Frame(header_frame, style="Dark.TFrame")
        status_frame.pack(side=tk.RIGHT)

        self.status_indicator = StatusIndicator(status_frame,
                                                bg=COLORS["bg_dark"])
        self.status_indicator.pack(side=tk.LEFT, padx=(0, 5))

        self.status_label = ttk.Label(status_frame,
                                      text="Disconnected",
                                      style="Muted.TLabel")
        self.status_label.pack(side=tk.LEFT)

    def _build_connection_panel(self, parent):
        """Build MQTT connection configuration panel."""
        frame = ttk.LabelFrame(parent, text="📡 MQTT Connection",
                               style="Dark.TLabelframe", padding=10)
        frame.pack(fill=tk.X, pady=(0, 10))

        # Broker
        ttk.Label(frame, text="Broker:", style="Dark.TLabel").grid(
            row=0, column=0, sticky=tk.W, pady=2)
        self.broker_entry = ttk.Entry(frame, width=30, style="Dark.TEntry")
        self.broker_entry.insert(0, "broker.hivemq.com")
        self.broker_entry.grid(row=0, column=1, sticky=tk.EW, pady=2, padx=(5, 0))

        # Port
        ttk.Label(frame, text="Port:", style="Dark.TLabel").grid(
            row=1, column=0, sticky=tk.W, pady=2)
        self.port_entry = ttk.Entry(frame, width=30, style="Dark.TEntry")
        self.port_entry.insert(0, "1883")
        self.port_entry.grid(row=1, column=1, sticky=tk.EW, pady=2, padx=(5, 0))

        # Client ID
        ttk.Label(frame, text="Client ID:", style="Dark.TLabel").grid(
            row=2, column=0, sticky=tk.W, pady=2)
        self.client_id_entry = ttk.Entry(frame, width=30, style="Dark.TEntry")
        self.client_id_entry.insert(0, f"mqtt_tester_{int(time.time())}")
        self.client_id_entry.grid(row=2, column=1, sticky=tk.EW, pady=2, padx=(5, 0))

        # Username (optional)
        ttk.Label(frame, text="Username:", style="Dark.TLabel").grid(
            row=3, column=0, sticky=tk.W, pady=2)
        self.username_entry = ttk.Entry(frame, width=30, style="Dark.TEntry")
        self.username_entry.grid(row=3, column=1, sticky=tk.EW, pady=2, padx=(5, 0))

        # Password (optional)
        ttk.Label(frame, text="Password:", style="Dark.TLabel").grid(
            row=4, column=0, sticky=tk.W, pady=2)
        self.password_entry = ttk.Entry(frame, width=30, style="Dark.TEntry", show="*")
        self.password_entry.grid(row=4, column=1, sticky=tk.EW, pady=2, padx=(5, 0))

        # Connect/Disconnect buttons
        btn_frame = ttk.Frame(frame, style="Dark.TFrame")
        btn_frame.grid(row=5, column=0, columnspan=2, pady=(10, 0))

        self.connect_btn = ttk.Button(btn_frame, text="Connect",
                                      style="Success.TButton",
                                      command=self._on_connect)
        self.connect_btn.pack(side=tk.LEFT, padx=(0, 5))

        self.disconnect_btn = ttk.Button(btn_frame, text="Disconnect",
                                         style="Danger.TButton",
                                         command=self._on_disconnect,
                                         state=tk.DISABLED)
        self.disconnect_btn.pack(side=tk.LEFT)

        frame.columnconfigure(1, weight=1)

    def _build_target_panel(self, parent):
        """Build target configuration panel."""
        frame = ttk.LabelFrame(parent, text="🎯 Target Configuration",
                               style="Dark.TLabelframe", padding=10)
        frame.pack(fill=tk.X, pady=(0, 10))

        # Gateway ID
        ttk.Label(frame, text="Gateway ID:", style="Dark.TLabel").grid(
            row=0, column=0, sticky=tk.W, pady=2)
        self.gateway_entry = ttk.Entry(frame, width=30, style="Dark.TEntry")
        self.gateway_entry.insert(0, "MGate1210_A3B4C5")
        self.gateway_entry.grid(row=0, column=1, sticky=tk.EW, pady=2, padx=(5, 0))

        # Device ID
        ttk.Label(frame, text="Device ID:", style="Dark.TLabel").grid(
            row=1, column=0, sticky=tk.W, pady=2)
        self.device_entry = ttk.Entry(frame, width=30, style="Dark.TEntry")
        self.device_entry.insert(0, "D7A3F2")
        self.device_entry.grid(row=1, column=1, sticky=tk.EW, pady=2, padx=(5, 0))

        # Topic Suffix (Register)
        ttk.Label(frame, text="Topic Suffix:", style="Dark.TLabel").grid(
            row=2, column=0, sticky=tk.W, pady=2)
        self.topic_suffix_entry = ttk.Entry(frame, width=30, style="Dark.TEntry")
        self.topic_suffix_entry.insert(0, "temp_setpoint")
        self.topic_suffix_entry.grid(row=2, column=1, sticky=tk.EW, pady=2, padx=(5, 0))

        # QoS
        ttk.Label(frame, text="QoS:", style="Dark.TLabel").grid(
            row=3, column=0, sticky=tk.W, pady=2)
        self.qos_var = tk.StringVar(value="1")
        qos_frame = ttk.Frame(frame, style="Dark.TFrame")
        qos_frame.grid(row=3, column=1, sticky=tk.W, pady=2, padx=(5, 0))
        for i, qos in enumerate(["0", "1", "2"]):
            ttk.Radiobutton(qos_frame, text=qos, value=qos,
                            variable=self.qos_var,
                            style="Dark.TRadiobutton").pack(side=tk.LEFT, padx=(0, 10))

        # Generated Topic Preview
        ttk.Label(frame, text="Full Topic:", style="Muted.TLabel").grid(
            row=4, column=0, sticky=tk.W, pady=(10, 2))
        self.topic_preview = ttk.Label(frame, text="", style="Muted.TLabel",
                                       wraplength=350)
        self.topic_preview.grid(row=4, column=1, sticky=tk.W, pady=(10, 2), padx=(5, 0))

        # Update preview on change
        for entry in [self.gateway_entry, self.device_entry, self.topic_suffix_entry]:
            entry.bind("<KeyRelease>", self._update_topic_preview)

        self._update_topic_preview()

        frame.columnconfigure(1, weight=1)

    def _build_payload_panel(self, parent):
        """Build payload configuration panel."""
        frame = ttk.LabelFrame(parent, text="📦 Payload",
                               style="Dark.TLabelframe", padding=10)
        frame.pack(fill=tk.BOTH, expand=True, pady=(0, 10))

        # Payload format selection
        format_frame = ttk.Frame(frame, style="Dark.TFrame")
        format_frame.pack(fill=tk.X, pady=(0, 10))

        ttk.Label(format_frame, text="Format:", style="Dark.TLabel").pack(side=tk.LEFT)

        self.payload_format = tk.StringVar(value="raw")
        formats = [
            ("Raw Value", "raw"),
            ("JSON {value}", "json_simple"),
            ("JSON with UUID", "json_uuid"),
        ]
        for text, value in formats:
            ttk.Radiobutton(format_frame, text=text, value=value,
                            variable=self.payload_format,
                            style="Dark.TRadiobutton",
                            command=self._update_payload_preview).pack(side=tk.LEFT, padx=(10, 0))

        # Value input
        value_frame = ttk.Frame(frame, style="Dark.TFrame")
        value_frame.pack(fill=tk.X, pady=(0, 10))

        ttk.Label(value_frame, text="Value:", style="Dark.TLabel").pack(side=tk.LEFT)
        self.value_entry = ttk.Entry(value_frame, width=20, style="Dark.TEntry")
        self.value_entry.insert(0, "25.5")
        self.value_entry.pack(side=tk.LEFT, padx=(10, 0))
        self.value_entry.bind("<KeyRelease>", self._update_payload_preview)

        # Payload preview
        ttk.Label(frame, text="Payload Preview:", style="Muted.TLabel").pack(
            anchor=tk.W, pady=(0, 5))

        self.payload_preview = JsonTextWidget(frame, height=4, width=40,
                                              bg=COLORS["bg_input"],
                                              fg=COLORS["fg_primary"],
                                              insertbackground=COLORS["fg_primary"],
                                              font=("Consolas", 10),
                                              relief=tk.FLAT)
        self.payload_preview.pack(fill=tk.BOTH, expand=True)

        self._update_payload_preview()

        # Send button
        self.send_btn = ttk.Button(frame, text="📤 Send Write Command",
                                   style="Dark.TButton",
                                   command=self._on_send,
                                   state=tk.DISABLED)
        self.send_btn.pack(fill=tk.X, pady=(10, 0))

    def _build_request_response_panel(self, parent):
        """Build request/response display panel."""
        # Request panel
        req_frame = ttk.LabelFrame(parent, text="📤 Request",
                                   style="Dark.TLabelframe", padding=10)
        req_frame.pack(fill=tk.BOTH, expand=True, pady=(0, 10))

        self.request_text = JsonTextWidget(req_frame, height=10,
                                           bg=COLORS["bg_input"],
                                           fg=COLORS["fg_primary"],
                                           insertbackground=COLORS["fg_primary"],
                                           font=("Consolas", 10),
                                           relief=tk.FLAT)
        self.request_text.pack(fill=tk.BOTH, expand=True)

        # Response panel
        resp_frame = ttk.LabelFrame(parent, text="📥 Response",
                                    style="Dark.TLabelframe", padding=10)
        resp_frame.pack(fill=tk.BOTH, expand=True)

        self.response_text = JsonTextWidget(resp_frame, height=10,
                                            bg=COLORS["bg_input"],
                                            fg=COLORS["fg_primary"],
                                            insertbackground=COLORS["fg_primary"],
                                            font=("Consolas", 10),
                                            relief=tk.FLAT)
        self.response_text.pack(fill=tk.BOTH, expand=True)

        # Response status
        self.response_status = ttk.Label(resp_frame, text="Waiting for response...",
                                         style="Muted.TLabel")
        self.response_status.pack(anchor=tk.W, pady=(5, 0))

    def _build_log_panel(self, parent):
        """Build message log panel."""
        frame = ttk.LabelFrame(parent, text="📋 Message Log",
                               style="Dark.TLabelframe", padding=10)
        frame.pack(fill=tk.X, pady=(10, 0))

        self.log_text = scrolledtext.ScrolledText(frame, height=6,
                                                  bg=COLORS["bg_input"],
                                                  fg=COLORS["fg_secondary"],
                                                  font=("Consolas", 9),
                                                  relief=tk.FLAT)
        self.log_text.pack(fill=tk.BOTH, expand=True)

        # Configure log tags
        self.log_text.tag_configure("info", foreground=COLORS["fg_secondary"])
        self.log_text.tag_configure("success", foreground=COLORS["success"])
        self.log_text.tag_configure("error", foreground=COLORS["error"])
        self.log_text.tag_configure("warning", foreground=COLORS["warning"])
        self.log_text.tag_configure("timestamp", foreground=COLORS["fg_muted"])

        # Clear button
        clear_btn = ttk.Button(frame, text="Clear Log",
                               command=self._clear_log)
        clear_btn.pack(anchor=tk.E, pady=(5, 0))

    def _update_topic_preview(self, event=None):
        """Update the topic preview label."""
        gateway = self.gateway_entry.get()
        device = self.device_entry.get()
        suffix = self.topic_suffix_entry.get()

        write_topic = f"suriota/{gateway}/write/{device}/{suffix}"
        response_topic = f"{write_topic}/response"

        self.topic_preview.config(
            text=f"Write: {write_topic}\nResponse: {response_topic}")

    def _update_payload_preview(self, event=None):
        """Update the payload preview."""
        value = self.value_entry.get()
        format_type = self.payload_format.get()

        try:
            # Try to parse as number
            if '.' in value:
                num_value = float(value)
            else:
                num_value = int(value)
        except ValueError:
            num_value = value

        if format_type == "raw":
            payload = str(num_value)
        elif format_type == "json_simple":
            payload = {"value": num_value}
        elif format_type == "json_uuid":
            import uuid
            payload = {
                "value": num_value,
                "uuid": str(uuid.uuid4())
            }
        else:
            payload = str(num_value)

        self.payload_preview.set_json(payload)

    def _generate_payload(self) -> str:
        """Generate the payload string."""
        value = self.value_entry.get()
        format_type = self.payload_format.get()

        try:
            if '.' in value:
                num_value = float(value)
            else:
                num_value = int(value)
        except ValueError:
            num_value = value

        if format_type == "raw":
            return str(num_value)
        elif format_type == "json_simple":
            return json.dumps({"value": num_value})
        elif format_type == "json_uuid":
            import uuid
            return json.dumps({
                "value": num_value,
                "uuid": str(uuid.uuid4())
            })
        else:
            return str(num_value)

    def _on_connect(self):
        """Handle connect button click."""
        broker = self.broker_entry.get()
        port = int(self.port_entry.get())
        client_id = self.client_id_entry.get()
        username = self.username_entry.get()
        password = self.password_entry.get()

        self._log("Connecting to MQTT broker...", "info")
        self._update_status("connecting")

        # Connect in background thread
        def connect_thread():
            success = self.mqtt_client.connect(broker, port, client_id,
                                               username, password)
            self.root.after(0, lambda: self._on_connect_result(success))

        threading.Thread(target=connect_thread, daemon=True).start()

    def _on_connect_result(self, success: bool):
        """Handle connection result."""
        if success:
            self._update_status("connected")
            self._log("Connected to MQTT broker", "success")

            # Subscribe to response topic
            gateway = self.gateway_entry.get()
            device = self.device_entry.get()
            suffix = self.topic_suffix_entry.get()
            response_topic = f"suriota/{gateway}/write/{device}/{suffix}/response"

            # Subscribe to wildcard for all responses
            wildcard_topic = f"suriota/{gateway}/write/+/+/response"
            if self.mqtt_client.subscribe(wildcard_topic):
                self._log(f"Subscribed to: {wildcard_topic}", "info")

            # Update UI
            self.connect_btn.config(state=tk.DISABLED)
            self.disconnect_btn.config(state=tk.NORMAL)
            self.send_btn.config(state=tk.NORMAL)
        else:
            self._update_status("disconnected")
            self._log("Failed to connect to MQTT broker", "error")

    def _on_disconnect(self):
        """Handle disconnect button click."""
        self.mqtt_client.disconnect()
        self._update_status("disconnected")
        self._log("Disconnected from MQTT broker", "warning")

        # Update UI
        self.connect_btn.config(state=tk.NORMAL)
        self.disconnect_btn.config(state=tk.DISABLED)
        self.send_btn.config(state=tk.DISABLED)

    def _on_send(self):
        """Handle send button click."""
        gateway = self.gateway_entry.get()
        device = self.device_entry.get()
        suffix = self.topic_suffix_entry.get()
        qos = int(self.qos_var.get())

        topic = f"suriota/{gateway}/write/{device}/{suffix}"
        payload = self._generate_payload()

        # Update request display
        request_info = {
            "topic": topic,
            "qos": qos,
            "payload": payload if not payload.startswith("{") else json.loads(payload),
            "timestamp": datetime.now().isoformat()
        }
        self.request_text.set_json(request_info)

        # Clear response
        self.response_text.config(state=tk.NORMAL)
        self.response_text.delete("1.0", tk.END)
        self.response_text.config(state=tk.DISABLED)
        self.response_status.config(text="Waiting for response...")

        # Publish
        if self.mqtt_client.publish(topic, payload, qos):
            self._log(f"Published to {topic}: {payload}", "success")
        else:
            self._log(f"Failed to publish to {topic}", "error")

    def _on_mqtt_message(self, topic: str, payload: str):
        """Handle incoming MQTT message."""
        # Update UI in main thread
        self.root.after(0, lambda: self._handle_message(topic, payload))

    def _handle_message(self, topic: str, payload: str):
        """Process received message."""
        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        self._log(f"Received from {topic}", "info")

        try:
            payload_obj = json.loads(payload)
            response_info = {
                "topic": topic,
                "received_at": timestamp,
                "payload": payload_obj
            }
        except json.JSONDecodeError:
            response_info = {
                "topic": topic,
                "received_at": timestamp,
                "payload": payload
            }

        self.response_text.set_json(response_info)

        # Update status based on response
        if isinstance(response_info.get("payload"), dict):
            status = response_info["payload"].get("status", "unknown")
            if status == "ok":
                self.response_status.config(text="✅ Write successful",
                                            foreground=COLORS["success"])
                self._log("Write command successful", "success")
            elif status == "error":
                error = response_info["payload"].get("error", "Unknown error")
                self.response_status.config(text=f"❌ Error: {error}",
                                            foreground=COLORS["error"])
                self._log(f"Write command failed: {error}", "error")
            else:
                self.response_status.config(text=f"Response received (status: {status})",
                                            foreground=COLORS["fg_secondary"])

    def _update_status(self, status: str):
        """Update connection status display."""
        self.status_indicator.set_status(status)
        status_texts = {
            "connected": "Connected",
            "disconnected": "Disconnected",
            "connecting": "Connecting...",
        }
        self.status_label.config(text=status_texts.get(status, status))

    def _log(self, message: str, level: str = "info"):
        """Add message to log."""
        timestamp = datetime.now().strftime("%H:%M:%S")
        self.log_text.config(state=tk.NORMAL)
        self.log_text.insert(tk.END, f"[{timestamp}] ", "timestamp")
        self.log_text.insert(tk.END, f"{message}\n", level)
        self.log_text.see(tk.END)
        self.log_text.config(state=tk.DISABLED)

    def _clear_log(self):
        """Clear the log."""
        self.log_text.config(state=tk.NORMAL)
        self.log_text.delete("1.0", tk.END)
        self.log_text.config(state=tk.DISABLED)

    def _on_close(self):
        """Handle window close."""
        self.mqtt_client.disconnect()
        self.root.destroy()


# ============================================================================
# ENTRY POINT
# ============================================================================

def main():
    """Main entry point."""
    root = tk.Tk()

    # Set window icon (if available)
    try:
        root.iconbitmap("mqtt_icon.ico")
    except:
        pass

    # Create application
    app = MQTTSubscribeControlGUI(root)

    # Center window on screen
    root.update_idletasks()
    x = (root.winfo_screenwidth() - APP_WIDTH) // 2
    y = (root.winfo_screenheight() - APP_HEIGHT) // 2
    root.geometry(f"{APP_WIDTH}x{APP_HEIGHT}+{x}+{y}")

    # Run
    root.mainloop()


if __name__ == "__main__":
    main()
