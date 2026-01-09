#!/usr/bin/env python3
"""
MQTT Subscribe Control - Professional Testing GUI
==================================================
Version: 2.0.0

Modern, professional GUI for testing MQTT Subscribe Control feature.
Designed as a reference for mobile app developers.

Author: SURIOTA R&D Team
"""

import tkinter as tk
from tkinter import ttk, font as tkfont
import json
import time
import uuid
import threading
from datetime import datetime

try:
    import paho.mqtt.client as mqtt
    from paho.mqtt.enums import CallbackAPIVersion
    PAHO_V2 = True
except ImportError:
    try:
        import paho.mqtt.client as mqtt
        PAHO_V2 = False
    except ImportError:
        print("=" * 60)
        print("ERROR: paho-mqtt library not found!")
        print("Please install it using: pip install paho-mqtt")
        print("=" * 60)
        exit(1)


class Colors:
    """Modern color palette - Sleek dark theme."""
    BG_PRIMARY = "#0d1117"
    BG_SECONDARY = "#161b22"
    BG_TERTIARY = "#21262d"
    BG_INPUT = "#0d1117"
    BG_HOVER = "#30363d"

    ACCENT = "#58a6ff"
    ACCENT_HOVER = "#79b8ff"
    SUCCESS = "#3fb950"
    ERROR = "#f85149"
    WARNING = "#d29922"

    TEXT_PRIMARY = "#f0f6fc"
    TEXT_SECONDARY = "#8b949e"
    TEXT_MUTED = "#6e7681"

    BORDER = "#30363d"
    BORDER_FOCUS = "#58a6ff"


class StyledFrame(tk.Frame):
    """Card-style frame with rounded appearance."""

    def __init__(self, parent, **kwargs):
        super().__init__(parent, bg=Colors.BG_SECONDARY, **kwargs)
        self.configure(highlightbackground=Colors.BORDER,
                      highlightthickness=1)


class StyledLabel(tk.Label):
    """Styled label with consistent typography."""

    def __init__(self, parent, text="", style="default", **kwargs):
        styles = {
            "default": {"fg": Colors.TEXT_PRIMARY, "font": ("Segoe UI", 10)},
            "header": {"fg": Colors.TEXT_PRIMARY, "font": ("Segoe UI", 13, "bold")},
            "title": {"fg": Colors.TEXT_PRIMARY, "font": ("Segoe UI", 20, "bold")},
            "subtitle": {"fg": Colors.TEXT_SECONDARY, "font": ("Segoe UI", 10)},
            "muted": {"fg": Colors.TEXT_MUTED, "font": ("Segoe UI", 9)},
            "accent": {"fg": Colors.ACCENT, "font": ("Segoe UI", 10, "bold")},
        }
        config = styles.get(style, styles["default"])
        super().__init__(parent, text=text, bg=parent.cget("bg"), **config, **kwargs)


class StyledEntry(tk.Frame):
    """Modern styled entry with label and border effect."""

    def __init__(self, parent, label="", width=30, **kwargs):
        super().__init__(parent, bg=parent.cget("bg"))

        if label:
            lbl = StyledLabel(self, text=label, style="muted")
            lbl.pack(anchor="w", pady=(0, 6))

        self.border_frame = tk.Frame(self, bg=Colors.BORDER, padx=1, pady=1)
        self.border_frame.pack(fill="x")

        self.entry = tk.Entry(self.border_frame, bg=Colors.BG_INPUT,
                             fg=Colors.TEXT_PRIMARY,
                             insertbackground=Colors.TEXT_PRIMARY,
                             font=("Segoe UI", 11), relief="flat",
                             width=width)
        self.entry.pack(fill="x", padx=10, pady=10)

        self.entry.bind("<FocusIn>", lambda e: self.border_frame.config(bg=Colors.BORDER_FOCUS))
        self.entry.bind("<FocusOut>", lambda e: self.border_frame.config(bg=Colors.BORDER))

    def get(self):
        return self.entry.get()

    def set(self, value):
        self.entry.delete(0, "end")
        self.entry.insert(0, value)

    def config(self, **kwargs):
        self.entry.config(**kwargs)


class StyledButton(tk.Canvas):
    """Modern button with hover effects."""

    def __init__(self, parent, text, command=None, width=140, height=42,
                 style="primary", **kwargs):
        super().__init__(parent, width=width, height=height,
                        bg=parent.cget("bg"), highlightthickness=0, **kwargs)

        styles = {
            "primary": (Colors.ACCENT, Colors.ACCENT_HOVER, Colors.TEXT_PRIMARY),
            "success": (Colors.SUCCESS, "#4cd964", Colors.TEXT_PRIMARY),
            "danger": (Colors.ERROR, "#ff6b6b", Colors.TEXT_PRIMARY),
            "ghost": (Colors.BG_TERTIARY, Colors.BG_HOVER, Colors.TEXT_PRIMARY),
        }

        self.bg_color, self.hover_color, self.fg = styles.get(style, styles["primary"])
        self.text = text
        self.command = command
        self.width = width
        self.height = height
        self._enabled = True

        self._draw(self.bg_color)

        self.bind("<Enter>", lambda e: self._draw(self.hover_color) if self._enabled else None)
        self.bind("<Leave>", lambda e: self._draw(self.bg_color) if self._enabled else None)
        self.bind("<Button-1>", lambda e: self.command() if self._enabled and self.command else None)

    def _draw(self, color):
        self.delete("all")
        r = 8
        self._rounded_rect(2, 2, self.width-2, self.height-2, r, color)
        self.create_text(self.width//2, self.height//2, text=self.text,
                        fill=self.fg, font=("Segoe UI", 10, "bold"))

    def _rounded_rect(self, x1, y1, x2, y2, r, color):
        points = [x1+r, y1, x2-r, y1, x2, y1, x2, y1+r, x2, y2-r, x2, y2,
                  x2-r, y2, x1+r, y2, x1, y2, x1, y2-r, x1, y1+r, x1, y1]
        self.create_polygon(points, fill=color, smooth=True)

    def set_enabled(self, enabled):
        self._enabled = enabled
        self._draw(self.bg_color if enabled else Colors.BG_TERTIARY)
        self.config(cursor="hand2" if enabled else "")


class StatusDot(tk.Canvas):
    """Connection status indicator."""

    def __init__(self, parent, size=10, **kwargs):
        super().__init__(parent, width=size, height=size,
                        bg=parent.cget("bg"), highlightthickness=0, **kwargs)
        self.size = size
        self.set_status("offline")

    def set_status(self, status):
        self.delete("all")
        colors = {"online": Colors.SUCCESS, "connecting": Colors.WARNING, "offline": Colors.ERROR}
        self.create_oval(1, 1, self.size-1, self.size-1,
                        fill=colors.get(status, Colors.TEXT_MUTED), outline="")


class CodeViewer(tk.Frame):
    """JSON/Code viewer with syntax highlighting."""

    def __init__(self, parent, title="", **kwargs):
        super().__init__(parent, bg=Colors.BG_SECONDARY)

        header = tk.Frame(self, bg=Colors.BG_SECONDARY)
        header.pack(fill="x", pady=(0, 8))

        StyledLabel(header, text=title, style="header").pack(side="left")
        self.status_lbl = StyledLabel(header, text="", style="muted")
        self.status_lbl.pack(side="right")

        text_border = tk.Frame(self, bg=Colors.BORDER, padx=1, pady=1)
        text_border.pack(fill="both", expand=True)

        self.text = tk.Text(text_border, bg=Colors.BG_INPUT, fg=Colors.TEXT_PRIMARY,
                           font=("JetBrains Mono", 10), relief="flat",
                           padx=16, pady=12, wrap="word",
                           insertbackground=Colors.TEXT_PRIMARY)
        self.text.pack(fill="both", expand=True)

        self.text.tag_configure("key", foreground="#79c0ff")
        self.text.tag_configure("string", foreground="#a5d6ff")
        self.text.tag_configure("number", foreground="#56d364")
        self.text.tag_configure("bool", foreground="#ff7b72")
        self.text.tag_configure("null", foreground="#8b949e")
        self.text.tag_configure("bracket", foreground="#d2a8ff")

    def set_json(self, data, status=""):
        self.text.config(state="normal")
        self.text.delete("1.0", "end")

        if data:
            if isinstance(data, str):
                try:
                    data = json.loads(data)
                except:
                    self.text.insert("1.0", data)
                    self.text.config(state="disabled")
                    return

            formatted = json.dumps(data, indent=2, ensure_ascii=False)
            self._highlight_json(formatted)

        self.text.config(state="disabled")
        self.status_lbl.config(text=status)

    def _highlight_json(self, text):
        i = 0
        while i < len(text):
            c = text[i]
            if c == '"':
                end = i + 1
                while end < len(text) and text[end] != '"':
                    if text[end] == '\\':
                        end += 1
                    end += 1
                end += 1
                s = text[i:end]
                tag = "key" if text[end:].lstrip().startswith(':') else "string"
                self.text.insert("end", s, tag)
                i = end
            elif c in '{}[]':
                self.text.insert("end", c, "bracket")
                i += 1
            elif c == '-' or c.isdigit():
                end = i + 1
                while end < len(text) and (text[end].isdigit() or text[end] in '.eE+-'):
                    end += 1
                self.text.insert("end", text[i:end], "number")
                i = end
            elif text[i:i+4] in ('true', 'null') or text[i:i+5] == 'false':
                word = 'true' if text[i:i+4] == 'true' else ('null' if text[i:i+4] == 'null' else 'false')
                tag = "null" if word == "null" else "bool"
                self.text.insert("end", word, tag)
                i += len(word)
            else:
                self.text.insert("end", c)
                i += 1

    def clear(self):
        self.text.config(state="normal")
        self.text.delete("1.0", "end")
        self.text.config(state="disabled")
        self.status_lbl.config(text="")


class LogPanel(tk.Frame):
    """Message log panel."""

    def __init__(self, parent, **kwargs):
        super().__init__(parent, bg=Colors.BG_SECONDARY)

        header = tk.Frame(self, bg=Colors.BG_SECONDARY)
        header.pack(fill="x", pady=(0, 8))

        StyledLabel(header, text="Activity Log", style="header").pack(side="left")

        clear_lbl = tk.Label(header, text="Clear", bg=Colors.BG_SECONDARY,
                            fg=Colors.ACCENT, font=("Segoe UI", 9), cursor="hand2")
        clear_lbl.pack(side="right")
        clear_lbl.bind("<Button-1>", lambda e: self.clear())

        log_border = tk.Frame(self, bg=Colors.BORDER, padx=1, pady=1)
        log_border.pack(fill="both", expand=True)

        self.log = tk.Text(log_border, bg=Colors.BG_INPUT, fg=Colors.TEXT_SECONDARY,
                          font=("JetBrains Mono", 9), relief="flat", height=6,
                          padx=12, pady=8, state="disabled")
        self.log.pack(fill="both", expand=True)

        self.log.tag_configure("time", foreground=Colors.TEXT_MUTED)
        self.log.tag_configure("info", foreground=Colors.ACCENT)
        self.log.tag_configure("success", foreground=Colors.SUCCESS)
        self.log.tag_configure("error", foreground=Colors.ERROR)
        self.log.tag_configure("warning", foreground=Colors.WARNING)

    def add(self, msg, level="info"):
        self.log.config(state="normal")
        ts = datetime.now().strftime("%H:%M:%S")
        self.log.insert("end", f"[{ts}] ", "time")
        self.log.insert("end", f"{msg}\n", level)
        self.log.see("end")
        self.log.config(state="disabled")

    def clear(self):
        self.log.config(state="normal")
        self.log.delete("1.0", "end")
        self.log.config(state="disabled")


class MQTTSubscribeControlGUI:
    """Main application."""

    def __init__(self):
        self.root = tk.Tk()
        self.root.title("MQTT Subscribe Control Tester")
        self.root.geometry("1150x780")
        self.root.configure(bg=Colors.BG_PRIMARY)
        self.root.minsize(1000, 650)

        self.client = None
        self.connected = False
        self.response_topic = None

        self._build_ui()
        self._center_window()

    def _center_window(self):
        self.root.update_idletasks()
        x = (self.root.winfo_screenwidth() - 1150) // 2
        y = (self.root.winfo_screenheight() - 780) // 2
        self.root.geometry(f"+{x}+{y}")

    def _build_ui(self):
        main = tk.Frame(self.root, bg=Colors.BG_PRIMARY)
        main.pack(fill="both", expand=True, padx=24, pady=24)

        self._build_header(main)

        content = tk.Frame(main, bg=Colors.BG_PRIMARY)
        content.pack(fill="both", expand=True, pady=(24, 0))

        left = tk.Frame(content, bg=Colors.BG_PRIMARY, width=400)
        left.pack(side="left", fill="y")
        left.pack_propagate(False)

        self._build_connection(left)
        self._build_target(left)
        self._build_payload(left)

        right = tk.Frame(content, bg=Colors.BG_PRIMARY)
        right.pack(side="left", fill="both", expand=True, padx=(24, 0))

        self._build_panels(right)

        self._build_log(main)

    def _build_header(self, parent):
        header = tk.Frame(parent, bg=Colors.BG_PRIMARY)
        header.pack(fill="x")

        title_frame = tk.Frame(header, bg=Colors.BG_PRIMARY)
        title_frame.pack(side="left")

        StyledLabel(title_frame, text="MQTT Subscribe Control", style="title").pack(anchor="w")
        StyledLabel(title_frame, text="Remote Register Write Testing Tool v2.0",
                   style="subtitle").pack(anchor="w", pady=(4, 0))

        status_frame = tk.Frame(header, bg=Colors.BG_PRIMARY)
        status_frame.pack(side="right")

        self.status_dot = StatusDot(status_frame)
        self.status_dot.pack(side="left", padx=(0, 8))

        self.status_lbl = StyledLabel(status_frame, text="Offline", style="muted")
        self.status_lbl.pack(side="left")

    def _build_card(self, parent, title):
        card = StyledFrame(parent)
        card.pack(fill="x", pady=(0, 16))

        inner = tk.Frame(card, bg=Colors.BG_SECONDARY)
        inner.pack(fill="x", padx=20, pady=20)

        StyledLabel(inner, text=title, style="accent").pack(anchor="w", pady=(0, 16))
        return inner

    def _build_connection(self, parent):
        card = self._build_card(parent, "CONNECTION")

        row = tk.Frame(card, bg=Colors.BG_SECONDARY)
        row.pack(fill="x", pady=(0, 12))

        self.broker_entry = StyledEntry(row, label="Broker", width=22)
        self.broker_entry.pack(side="left", fill="x", expand=True)
        self.broker_entry.set("broker.hivemq.com")

        tk.Frame(row, bg=Colors.BG_SECONDARY, width=12).pack(side="left")

        self.port_entry = StyledEntry(row, label="Port", width=8)
        self.port_entry.pack(side="left")
        self.port_entry.set("1883")

        btn_frame = tk.Frame(card, bg=Colors.BG_SECONDARY)
        btn_frame.pack(fill="x")

        self.connect_btn = StyledButton(btn_frame, "Connect", self._toggle_connection,
                                        width=120, style="primary")
        self.connect_btn.pack(side="left")

    def _build_target(self, parent):
        card = self._build_card(parent, "TARGET")

        self.gateway_entry = StyledEntry(card, label="Gateway ID")
        self.gateway_entry.pack(fill="x", pady=(0, 12))
        self.gateway_entry.set("MGate1210_A3B4C5")

        self.device_entry = StyledEntry(card, label="Device ID")
        self.device_entry.pack(fill="x", pady=(0, 12))
        self.device_entry.set("D7A3F2")

        self.topic_entry = StyledEntry(card, label="Topic Suffix")
        self.topic_entry.pack(fill="x", pady=(0, 12))
        self.topic_entry.set("temp_setpoint")

        qos_frame = tk.Frame(card, bg=Colors.BG_SECONDARY)
        qos_frame.pack(fill="x")

        StyledLabel(qos_frame, text="QoS", style="muted").pack(anchor="w", pady=(0, 6))

        self.qos_var = tk.IntVar(value=1)
        qos_opts = tk.Frame(qos_frame, bg=Colors.BG_SECONDARY)
        qos_opts.pack(anchor="w")

        for i in range(3):
            rb = tk.Radiobutton(qos_opts, text=str(i), variable=self.qos_var, value=i,
                               bg=Colors.BG_SECONDARY, fg=Colors.TEXT_PRIMARY,
                               selectcolor=Colors.BG_INPUT, activebackground=Colors.BG_SECONDARY,
                               activeforeground=Colors.TEXT_PRIMARY, font=("Segoe UI", 10),
                               highlightthickness=0)
            rb.pack(side="left", padx=(0, 20))

    def _build_payload(self, parent):
        card = self._build_card(parent, "PAYLOAD")

        fmt_frame = tk.Frame(card, bg=Colors.BG_SECONDARY)
        fmt_frame.pack(fill="x", pady=(0, 12))

        StyledLabel(fmt_frame, text="Format", style="muted").pack(anchor="w", pady=(0, 6))

        self.format_var = tk.StringVar(value="raw")

        style = ttk.Style()
        style.configure("Dark.TCombobox", fieldbackground=Colors.BG_INPUT,
                       background=Colors.BG_TERTIARY, foreground=Colors.TEXT_PRIMARY)

        fmt_combo = ttk.Combobox(fmt_frame, textvariable=self.format_var,
                                values=["raw", "json", "json + uuid"],
                                state="readonly", font=("Segoe UI", 10),
                                style="Dark.TCombobox")
        fmt_combo.pack(fill="x")

        self.value_entry = StyledEntry(card, label="Value")
        self.value_entry.pack(fill="x", pady=(0, 16))
        self.value_entry.set("25.5")

        self.send_btn = StyledButton(card, "Send Write Command", self._send_command,
                                     width=360, height=48, style="success")
        self.send_btn.pack(fill="x")
        self.send_btn.set_enabled(False)

    def _build_panels(self, parent):
        self.request_viewer = CodeViewer(parent, title="Request")
        self.request_viewer.pack(fill="x", pady=(0, 16))

        self.response_viewer = CodeViewer(parent, title="Response")
        self.response_viewer.pack(fill="both", expand=True)

    def _build_log(self, parent):
        self.log_panel = LogPanel(parent)
        self.log_panel.pack(fill="x", pady=(16, 0))

    def _toggle_connection(self):
        if self.connected:
            self._disconnect()
        else:
            self._connect()

    def _connect(self):
        broker = self.broker_entry.get()
        try:
            port = int(self.port_entry.get())
        except ValueError:
            self.log_panel.add("Invalid port number", "error")
            return

        self.status_dot.set_status("connecting")
        self.status_lbl.config(text="Connecting...")
        self.log_panel.add(f"Connecting to {broker}:{port}...")

        def do_connect():
            try:
                client_id = f"suriota_gui_{int(time.time())}"

                if PAHO_V2:
                    self.client = mqtt.Client(callback_api_version=CallbackAPIVersion.VERSION2,
                                             client_id=client_id, protocol=mqtt.MQTTv311)
                    self.client.on_connect = self._on_connect_v2
                    self.client.on_message = self._on_message_v2
                    self.client.on_disconnect = self._on_disconnect_v2
                else:
                    self.client = mqtt.Client(client_id=client_id, protocol=mqtt.MQTTv311)
                    self.client.on_connect = self._on_connect_v1
                    self.client.on_message = self._on_message_v1
                    self.client.on_disconnect = self._on_disconnect_v1

                self.client.connect(broker, port, keepalive=60)
                self.client.loop_start()

            except Exception as e:
                self.root.after(0, lambda: self._connection_failed(str(e)))

        threading.Thread(target=do_connect, daemon=True).start()

    def _disconnect(self):
        if self.client:
            self.client.loop_stop()
            self.client.disconnect()
            self.client = None

        self.connected = False
        self.status_dot.set_status("offline")
        self.status_lbl.config(text="Offline")
        self.connect_btn.text = "Connect"
        self.connect_btn._draw(Colors.ACCENT)
        self.send_btn.set_enabled(False)
        self.log_panel.add("Disconnected", "warning")

    def _connection_failed(self, error):
        self.status_dot.set_status("offline")
        self.status_lbl.config(text="Offline")
        self.log_panel.add(f"Connection failed: {error}", "error")

    def _on_connect_v2(self, client, userdata, flags, reason_code, properties):
        self.root.after(0, self._handle_connected)

    def _on_message_v2(self, client, userdata, msg):
        self.root.after(0, lambda: self._handle_message(msg))

    def _on_disconnect_v2(self, client, userdata, flags, reason_code, properties):
        self.root.after(0, self._handle_disconnected)

    def _on_connect_v1(self, client, userdata, flags, rc):
        if rc == 0:
            self.root.after(0, self._handle_connected)

    def _on_message_v1(self, client, userdata, msg):
        self.root.after(0, lambda: self._handle_message(msg))

    def _on_disconnect_v1(self, client, userdata, rc):
        self.root.after(0, self._handle_disconnected)

    def _handle_connected(self):
        self.connected = True
        self.status_dot.set_status("online")
        self.status_lbl.config(text="Online")
        self.connect_btn.text = "Disconnect"
        self.connect_btn.bg_color = Colors.ERROR
        self.connect_btn.hover_color = "#ff6b6b"
        self.connect_btn._draw(Colors.ERROR)
        self.send_btn.set_enabled(True)
        self.log_panel.add("Connected to broker", "success")

    def _handle_disconnected(self):
        if self.connected:
            self.connected = False
            self.status_dot.set_status("offline")
            self.status_lbl.config(text="Offline")
            self.connect_btn.text = "Connect"
            self.connect_btn.bg_color = Colors.ACCENT
            self.connect_btn.hover_color = Colors.ACCENT_HOVER
            self.connect_btn._draw(Colors.ACCENT)
            self.send_btn.set_enabled(False)
            self.log_panel.add("Connection lost", "warning")

    def _handle_message(self, msg):
        try:
            payload = msg.payload.decode('utf-8')

            if msg.topic == self.response_topic:
                try:
                    data = json.loads(payload)
                    status = data.get("status", "unknown")

                    if status == "ok":
                        self.response_viewer.set_json(data, "SUCCESS")
                        self.log_panel.add(f"Write successful: {data.get('written_value', 'N/A')}", "success")
                    else:
                        self.response_viewer.set_json(data, "FAILED")
                        self.log_panel.add(f"Write failed: {data.get('error', 'Unknown')}", "error")
                except:
                    self.response_viewer.set_json({"raw": payload}, "RAW")

        except Exception as e:
            self.log_panel.add(f"Message error: {e}", "error")

    def _send_command(self):
        if not self.connected:
            self.log_panel.add("Not connected", "error")
            return

        gateway = self.gateway_entry.get()
        device = self.device_entry.get()
        topic = self.topic_entry.get()
        value_str = self.value_entry.get()
        qos = self.qos_var.get()
        fmt = self.format_var.get()

        try:
            value = float(value_str) if '.' in value_str else int(value_str)
        except:
            value = value_str

        write_topic = f"suriota/{gateway}/write/{device}/{topic}"
        self.response_topic = f"{write_topic}/response"

        if fmt == "raw":
            payload = str(value)
        elif fmt == "json":
            payload = json.dumps({"value": value})
        else:
            payload = json.dumps({"value": value, "uuid": str(uuid.uuid4())[:8]})

        request = {
            "topic": write_topic,
            "response_topic": self.response_topic,
            "qos": qos,
            "payload": json.loads(payload) if payload.startswith("{") else payload
        }

        self.request_viewer.set_json(request, f"QoS {qos}")
        self.response_viewer.clear()
        self.response_viewer.status_lbl.config(text="Waiting...")

        self.client.subscribe(self.response_topic, qos)
        result = self.client.publish(write_topic, payload, qos)

        if result.rc == mqtt.MQTT_ERR_SUCCESS:
            self.log_panel.add(f"Published to {write_topic}", "info")
            self.root.after(5000, self._check_timeout)
        else:
            self.log_panel.add("Publish failed", "error")

    def _check_timeout(self):
        if self.response_viewer.status_lbl.cget("text") == "Waiting...":
            self.response_viewer.set_json({
                "status": "timeout",
                "error": "No response received within 5 seconds"
            }, "TIMEOUT")
            self.log_panel.add("Response timeout", "warning")

    def run(self):
        self.log_panel.add("Application started", "info")
        self.root.mainloop()

        if self.client:
            self.client.loop_stop()
            self.client.disconnect()


if __name__ == "__main__":
    app = MQTTSubscribeControlGUI()
    app.run()
