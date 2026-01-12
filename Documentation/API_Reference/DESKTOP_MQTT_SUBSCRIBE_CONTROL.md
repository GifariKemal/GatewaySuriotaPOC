# Desktop App Integration: MQTT Subscribe Control v1.2.0

**Version:** 1.2.0 | **Date:** January 2026 | **Target:** Desktop App Developers
(Python/PyQt, Electron, C#/.NET)

---

## Table of Contents

1. [Overview](#1-overview)
2. [Architecture](#2-architecture)
3. [BLE Configuration API](#3-ble-configuration-api)
4. [MQTT Protocol](#4-mqtt-protocol)
5. [Python Implementation](#5-python-implementation)
6. [Electron Implementation](#6-electron-implementation)
7. [Error Handling](#7-error-handling)
8. [Testing](#8-testing)

---

## 1. Overview

### What is MQTT Subscribe Control?

MQTT Subscribe Control memungkinkan desktop app untuk **menulis nilai ke register
Modbus secara remote melalui MQTT**, tanpa koneksi BLE langsung ke gateway.

```
┌──────────────┐      MQTT       ┌─────────────┐     Modbus     ┌─────────────┐
│  Desktop App │ ──────────────► │   Gateway   │ ─────────────► │   Device    │
│ (Python/Qt)  │ ◄────────────── │ MGATE-1210  │ ◄───────────── │  (PLC/RTU)  │
└──────────────┘    Response     └─────────────┘    Response    └─────────────┘
```

### v1.2.0 Topic-Centric Design

| Aspect              | v1.1.0 (Register-centric)                   | v1.2.0 (Topic-centric)                           |
| ------------------- | ------------------------------------------- | ------------------------------------------------ |
| Config Location     | Per-register `mqtt_subscribe`               | `server_config.custom_subscribe_mode`            |
| Topic Format        | Auto: `suriota/{gw}/write/{dev}/{suffix}`   | User-defined custom topic                        |
| 1 Topic Controls    | 1 register                                  | 1 or N registers                                 |
| Payload             | `{"value": X}` only                         | `{"value": X}` or `{"RegA": X, "RegB": Y}`       |
| Desktop App Aligned | No                                          | Yes                                              |

### Writable Registers (FC01/FC03 Only)

| FC   | Type             | Writable | Write FC |
| ---- | ---------------- | -------- | -------- |
| FC01 | Coil             | Yes      | FC05/FC15|
| FC02 | Discrete Input   | No       | -        |
| FC03 | Holding Register | Yes      | FC06/FC16|
| FC04 | Input Register   | No       | -        |

---

## 2. Architecture

### Topic-Centric Data Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  Desktop App - Configuration Phase (via BLE)                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  1. Connect to gateway via BLE                                              │
│  2. Read devices and registers (identify writable ones)                     │
│  3. Create subscriptions in server_config.custom_subscribe_mode             │
│  4. Save configuration → Gateway restarts MQTT with new subscriptions       │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│  Desktop App - Runtime Phase (via MQTT)                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ Subscription: "factory/hvac/setpoint"                               │   │
│  │   registers: [TempSetpoint]                                         │   │
│  │   Publish: {"value": 25.5}                                          │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ Subscription: "factory/valves/control"                              │   │
│  │   registers: [ValveA, ValveB]                                       │   │
│  │   Publish: {"ValveA": 1, "ValveB": 0}                               │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
                            ┌───────────────┐
                            │  MQTT Broker  │
                            └───────────────┘
                                    │
                                    ▼
                        ┌───────────────────────┐
                        │  Gateway (MGATE-1210) │
                        │  - Parse topic/payload│
                        │  - Write to Modbus    │
                        │  - Publish response   │
                        └───────────────────────┘
```

### Configuration Structure

```json
{
  "mqtt_config": {
    "enabled": true,
    "broker_address": "192.168.1.100",
    "broker_port": 1883,
    "client_id": "mgate_001",
    "username": "user",
    "password": "pass123",
    "topic_mode": "custom_subscribe",
    "custom_subscribe_mode": {
      "enabled": true,
      "subscriptions": [
        {
          "topic": "factory/hvac/setpoint",
          "qos": 1,
          "response_topic": "factory/hvac/setpoint/response",
          "registers": [{ "device_id": "HVAC01", "register_id": "TempSetpoint" }]
        },
        {
          "topic": "factory/valves/control",
          "qos": 2,
          "response_topic": "factory/valves/status",
          "registers": [
            { "device_id": "Valve01", "register_id": "ValveA" },
            { "device_id": "Valve01", "register_id": "ValveB" }
          ]
        }
      ]
    }
  }
}
```

---

## 3. BLE Configuration API

### 3.1 Read Server Config

**Request:**

```json
{ "op": "read", "type": "server_config" }
```

**Response:**

```json
{
  "status": "ok",
  "server_config": {
    "mqtt_config": {
      "enabled": true,
      "broker_address": "192.168.1.100",
      "broker_port": 1883,
      "topic_mode": "custom_subscribe",
      "custom_subscribe_mode": {
        "enabled": true,
        "subscriptions": [...]
      }
    }
  }
}
```

### 3.2 Read Devices (Get Writable Registers)

**Request:**

```json
{ "op": "read", "type": "devices" }
```

**Response:** Filter registers where `function_code` is 1 (Coil) or 3 (Holding Register).

```json
{
  "status": "ok",
  "devices": [
    {
      "device_id": "HVAC01",
      "device_name": "HVAC Controller",
      "registers": [
        {
          "register_id": "TempSetpoint",
          "register_name": "Temperature Setpoint",
          "function_code": 3,
          "address": 100,
          "data_type": "FLOAT32"
        },
        {
          "register_id": "TempReading",
          "register_name": "Temperature Reading",
          "function_code": 4,
          "address": 0
        }
      ]
    }
  ]
}
```

**Writable:** TempSetpoint (FC03) | **Read-only:** TempReading (FC04)

### 3.3 Update Subscriptions

**Request:**

```json
{
  "op": "update",
  "type": "server_config",
  "config": {
    "mqtt_config": {
      "topic_mode": "custom_subscribe",
      "custom_subscribe_mode": {
        "enabled": true,
        "subscriptions": [
          {
            "topic": "factory/hvac/setpoint",
            "qos": 1,
            "response_topic": "factory/hvac/setpoint/response",
            "registers": [{ "device_id": "HVAC01", "register_id": "TempSetpoint" }]
          }
        ]
      }
    }
  }
}
```

**Response:**

```json
{
  "status": "ok",
  "message": "Server config updated successfully"
}
```

### 3.4 Schema Reference

#### Subscription Object

| Field            | Type    | Required | Description                                      |
| ---------------- | ------- | -------- | ------------------------------------------------ |
| `topic`          | string  | Yes      | MQTT topic to subscribe                          |
| `qos`            | integer | No       | QoS level 0, 1, or 2 (default: 1)                |
| `response_topic` | string  | No       | Response topic (default: `{topic}/response`)     |
| `registers`      | array   | Yes      | Array of register references                     |

#### Register Reference

| Field         | Type   | Required | Description            |
| ------------- | ------ | -------- | ---------------------- |
| `device_id`   | string | Yes      | Modbus device ID       |
| `register_id` | string | Yes      | Register ID to write   |

---

## 4. MQTT Protocol

### 4.1 Payload Formats

#### Single Register Subscription

```
Subscription: {"topic": "factory/temp", "registers": [{device_id, register_id}]}
Payload: {"value": 25.5}
```

#### Multi-Register Subscription

```
Subscription: {"topic": "factory/valves", "registers": [ValveA, ValveB]}
Payload: {"ValveA": 1, "ValveB": 0}
```

### 4.2 Payload Rules

| Scenario                        | Payload                   | Result                                                   |
| ------------------------------- | ------------------------- | -------------------------------------------------------- |
| 1 register + `{"value": X}`     | `{"value": 25.5}`         | Write 25.5 to register                                   |
| N registers + per-register keys | `{"RegA": 1, "RegB": 0}`  | Write each value to matching register                    |
| N registers + `{"value": X}`    | `{"value": 25.5}`         | **ERROR**: "Multiple registers require explicit values"  |

### 4.3 Response Format

#### Success Response

```json
{
  "status": "ok",
  "topic": "factory/valves/control",
  "results": [
    {
      "device_id": "Valve01",
      "register_id": "ValveA",
      "status": "ok",
      "written_value": 1,
      "raw_value": 1
    },
    {
      "device_id": "Valve01",
      "register_id": "ValveB",
      "status": "ok",
      "written_value": 0,
      "raw_value": 0
    }
  ],
  "timestamp": 1736697600000
}
```

#### Partial Success Response

```json
{
  "status": "partial",
  "topic": "factory/valves/control",
  "results": [
    { "device_id": "Valve01", "register_id": "ValveA", "status": "ok", "written_value": 1 },
    { "device_id": "Valve01", "register_id": "ValveB", "status": "error", "error": "Device offline" }
  ],
  "timestamp": 1736697600000
}
```

#### Error Response

```json
{
  "status": "error",
  "topic": "factory/valves/control",
  "error": "Multiple registers require explicit values per register_id",
  "error_code": 400,
  "timestamp": 1736697600000
}
```

---

## 5. Python Implementation

### 5.1 Data Models

```python
# models/mqtt_subscription.py

from dataclasses import dataclass, field
from typing import List, Optional
import json

@dataclass
class RegisterReference:
    device_id: str
    register_id: str

    def to_dict(self) -> dict:
        return {
            "device_id": self.device_id,
            "register_id": self.register_id
        }

    @classmethod
    def from_dict(cls, data: dict) -> "RegisterReference":
        return cls(
            device_id=data["device_id"],
            register_id=data["register_id"]
        )


@dataclass
class MqttSubscription:
    topic: str
    registers: List[RegisterReference]
    qos: int = 1
    response_topic: Optional[str] = None

    def __post_init__(self):
        if self.response_topic is None:
            self.response_topic = f"{self.topic}/response"

    def to_dict(self) -> dict:
        return {
            "topic": self.topic,
            "qos": self.qos,
            "response_topic": self.response_topic,
            "registers": [r.to_dict() for r in self.registers]
        }

    @classmethod
    def from_dict(cls, data: dict) -> "MqttSubscription":
        return cls(
            topic=data["topic"],
            qos=data.get("qos", 1),
            response_topic=data.get("response_topic"),
            registers=[RegisterReference.from_dict(r) for r in data["registers"]]
        )

    @property
    def is_multi_register(self) -> bool:
        return len(self.registers) > 1


@dataclass
class CustomSubscribeMode:
    enabled: bool
    subscriptions: List[MqttSubscription] = field(default_factory=list)

    def to_dict(self) -> dict:
        return {
            "enabled": self.enabled,
            "subscriptions": [s.to_dict() for s in self.subscriptions]
        }

    @classmethod
    def from_dict(cls, data: dict) -> "CustomSubscribeMode":
        return cls(
            enabled=data.get("enabled", False),
            subscriptions=[
                MqttSubscription.from_dict(s)
                for s in data.get("subscriptions", [])
            ]
        )


@dataclass
class WriteResult:
    device_id: str
    register_id: str
    status: str
    written_value: Optional[float] = None
    raw_value: Optional[int] = None
    error: Optional[str] = None

    @property
    def is_success(self) -> bool:
        return self.status == "ok"

    @classmethod
    def from_dict(cls, data: dict) -> "WriteResult":
        return cls(
            device_id=data["device_id"],
            register_id=data["register_id"],
            status=data["status"],
            written_value=data.get("written_value"),
            raw_value=data.get("raw_value"),
            error=data.get("error")
        )


@dataclass
class WriteResponse:
    status: str
    topic: str
    timestamp: int
    results: List[WriteResult] = field(default_factory=list)
    error: Optional[str] = None
    error_code: Optional[int] = None

    @property
    def is_success(self) -> bool:
        return self.status == "ok"

    @property
    def is_partial(self) -> bool:
        return self.status == "partial"

    @property
    def is_error(self) -> bool:
        return self.status == "error"

    @classmethod
    def from_dict(cls, data: dict) -> "WriteResponse":
        return cls(
            status=data["status"],
            topic=data["topic"],
            timestamp=data["timestamp"],
            results=[WriteResult.from_dict(r) for r in data.get("results", [])],
            error=data.get("error"),
            error_code=data.get("error_code")
        )
```

### 5.2 MQTT Write Service

```python
# services/mqtt_write_service.py

import json
import logging
from typing import Callable, Dict, Optional, Union
import paho.mqtt.client as mqtt
from paho.mqtt.client import CallbackAPIVersion

from models.mqtt_subscription import MqttSubscription, WriteResponse

logger = logging.getLogger(__name__)


class MqttWriteService:
    def __init__(
        self,
        broker: str,
        port: int = 1883,
        client_id: str = "desktop_app",
        username: Optional[str] = None,
        password: Optional[str] = None
    ):
        self.broker = broker
        self.port = port
        self.client = mqtt.Client(
            callback_api_version=CallbackAPIVersion.VERSION2,
            client_id=client_id
        )

        if username and password:
            self.client.username_pw_set(username, password)

        self.client.on_connect = self._on_connect
        self.client.on_disconnect = self._on_disconnect
        self.client.on_message = self._on_message

        self._response_callbacks: Dict[str, Callable[[WriteResponse], None]] = {}
        self._connected = False

    def connect(self) -> bool:
        """Connect to MQTT broker."""
        try:
            self.client.connect(self.broker, self.port, keepalive=60)
            self.client.loop_start()
            return True
        except Exception as e:
            logger.error(f"Failed to connect: {e}")
            return False

    def disconnect(self):
        """Disconnect from MQTT broker."""
        self.client.loop_stop()
        self.client.disconnect()

    def subscribe_response(
        self,
        response_topic: str,
        callback: Callable[[WriteResponse], None]
    ):
        """Subscribe to response topic with callback."""
        self._response_callbacks[response_topic] = callback
        self.client.subscribe(response_topic, qos=1)
        logger.info(f"Subscribed to response: {response_topic}")

    def write_single_value(
        self,
        subscription: MqttSubscription,
        value: Union[int, float]
    ):
        """Write single value to single-register subscription."""
        if subscription.is_multi_register:
            raise ValueError("Use write_multiple_values for multi-register subscriptions")

        payload = json.dumps({"value": value})
        self._publish(subscription.topic, payload, subscription.qos)
        logger.info(f"Published: {subscription.topic} = {payload}")

    def write_multiple_values(
        self,
        subscription: MqttSubscription,
        values: Dict[str, Union[int, float]]
    ):
        """Write multiple values to multi-register subscription."""
        payload = json.dumps(values)
        self._publish(subscription.topic, payload, subscription.qos)
        logger.info(f"Published: {subscription.topic} = {payload}")

    def _publish(self, topic: str, payload: str, qos: int):
        """Publish message to topic."""
        result = self.client.publish(topic, payload, qos=qos)
        if result.rc != mqtt.MQTT_ERR_SUCCESS:
            logger.error(f"Publish failed: {result.rc}")

    def _on_connect(self, client, userdata, flags, reason_code, properties):
        """Handle connection event."""
        if reason_code == 0:
            self._connected = True
            logger.info("Connected to MQTT broker")
            # Resubscribe to all response topics
            for topic in self._response_callbacks:
                self.client.subscribe(topic, qos=1)
        else:
            logger.error(f"Connection failed: {reason_code}")

    def _on_disconnect(self, client, userdata, flags, reason_code, properties):
        """Handle disconnection event."""
        self._connected = False
        logger.warning(f"Disconnected: {reason_code}")

    def _on_message(self, client, userdata, msg):
        """Handle incoming message."""
        topic = msg.topic
        try:
            payload = json.loads(msg.payload.decode())
            response = WriteResponse.from_dict(payload)

            if topic in self._response_callbacks:
                self._response_callbacks[topic](response)
            else:
                logger.debug(f"No callback for topic: {topic}")

        except json.JSONDecodeError as e:
            logger.error(f"Invalid JSON: {e}")
        except Exception as e:
            logger.error(f"Error processing message: {e}")

    @property
    def is_connected(self) -> bool:
        return self._connected
```

### 5.3 BLE Configuration Service

```python
# services/ble_config_service.py

import json
import asyncio
from typing import List, Optional
from bleak import BleakClient, BleakScanner

from models.mqtt_subscription import MqttSubscription, CustomSubscribeMode

# BLE UUIDs for MGATE-1210
SERVICE_UUID = "12345678-1234-5678-1234-56789abcdef0"
WRITE_CHAR_UUID = "12345678-1234-5678-1234-56789abcdef1"
NOTIFY_CHAR_UUID = "12345678-1234-5678-1234-56789abcdef2"


class BleConfigService:
    def __init__(self):
        self.client: Optional[BleakClient] = None
        self._response_buffer = bytearray()
        self._response_event = asyncio.Event()
        self._current_response = None

    async def scan_gateways(self, timeout: float = 5.0) -> List[dict]:
        """Scan for MGATE-1210 gateways."""
        devices = await BleakScanner.discover(timeout)
        gateways = []
        for d in devices:
            if d.name and d.name.startswith("MGate-1210"):
                gateways.append({
                    "address": d.address,
                    "name": d.name,
                    "rssi": d.rssi
                })
        return gateways

    async def connect(self, address: str) -> bool:
        """Connect to gateway via BLE."""
        try:
            self.client = BleakClient(address)
            await self.client.connect()
            await self.client.start_notify(NOTIFY_CHAR_UUID, self._notification_handler)
            return True
        except Exception as e:
            print(f"Connection failed: {e}")
            return False

    async def disconnect(self):
        """Disconnect from gateway."""
        if self.client:
            await self.client.disconnect()
            self.client = None

    async def get_subscribe_config(self) -> CustomSubscribeMode:
        """Read current subscription configuration."""
        response = await self._send_command({
            "op": "read",
            "type": "server_config"
        })

        if response.get("status") == "ok":
            mqtt_config = response.get("server_config", {}).get("mqtt_config", {})
            custom_sub = mqtt_config.get("custom_subscribe_mode", {})
            return CustomSubscribeMode.from_dict(custom_sub)

        raise Exception(response.get("error", "Failed to read config"))

    async def update_subscriptions(self, subscriptions: List[MqttSubscription]) -> bool:
        """Update subscription configuration."""
        response = await self._send_command({
            "op": "update",
            "type": "server_config",
            "config": {
                "mqtt_config": {
                    "topic_mode": "custom_subscribe",
                    "custom_subscribe_mode": {
                        "enabled": True,
                        "subscriptions": [s.to_dict() for s in subscriptions]
                    }
                }
            }
        })

        return response.get("status") == "ok"

    async def get_writable_registers(self) -> List[dict]:
        """Get all writable registers (FC01, FC03)."""
        response = await self._send_command({
            "op": "read",
            "type": "devices"
        })

        writable = []
        if response.get("status") == "ok":
            for device in response.get("devices", []):
                for reg in device.get("registers", []):
                    fc = reg.get("function_code", 0)
                    if fc in [1, 3]:  # Coil or Holding Register
                        writable.append({
                            "device_id": device["device_id"],
                            "device_name": device.get("device_name", ""),
                            "register_id": reg["register_id"],
                            "register_name": reg.get("register_name", ""),
                            "function_code": fc,
                            "data_type": reg.get("data_type", "INT16")
                        })

        return writable

    async def _send_command(self, command: dict) -> dict:
        """Send BLE command and wait for response."""
        self._response_buffer.clear()
        self._response_event.clear()

        payload = json.dumps(command).encode()
        await self.client.write_gatt_char(WRITE_CHAR_UUID, payload)

        try:
            await asyncio.wait_for(self._response_event.wait(), timeout=10.0)
            return self._current_response
        except asyncio.TimeoutError:
            return {"status": "error", "error": "Response timeout"}

    def _notification_handler(self, sender, data):
        """Handle BLE notifications (fragmented response)."""
        self._response_buffer.extend(data)

        # Check if response is complete (ends with })
        try:
            text = self._response_buffer.decode()
            if text.strip().endswith("}"):
                self._current_response = json.loads(text)
                self._response_event.set()
        except:
            pass  # Still receiving fragments
```

### 5.4 PyQt5 GUI Example

```python
# gui/main_window.py

import sys
import asyncio
from typing import List
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QTableWidget, QTableWidgetItem, QPushButton, QLineEdit, QLabel,
    QComboBox, QSpinBox, QMessageBox, QDialog, QCheckBox, QGroupBox,
    QDoubleSpinBox, QStatusBar
)
from PyQt5.QtCore import Qt, QThread, pyqtSignal

from models.mqtt_subscription import MqttSubscription, RegisterReference, WriteResponse
from services.mqtt_write_service import MqttWriteService
from services.ble_config_service import BleConfigService


class MqttSubscribeControlWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("MQTT Subscribe Control v1.2.0")
        self.setMinimumSize(900, 600)

        self.subscriptions: List[MqttSubscription] = []
        self.mqtt_service: MqttWriteService = None
        self.ble_service = BleConfigService()

        self._setup_ui()
        self._connect_signals()

    def _setup_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        layout = QVBoxLayout(central)

        # Connection settings
        conn_group = QGroupBox("Connection")
        conn_layout = QHBoxLayout(conn_group)

        conn_layout.addWidget(QLabel("Broker:"))
        self.broker_input = QLineEdit("192.168.1.100")
        conn_layout.addWidget(self.broker_input)

        conn_layout.addWidget(QLabel("Port:"))
        self.port_input = QSpinBox()
        self.port_input.setRange(1, 65535)
        self.port_input.setValue(1883)
        conn_layout.addWidget(self.port_input)

        self.connect_btn = QPushButton("Connect MQTT")
        conn_layout.addWidget(self.connect_btn)

        self.ble_btn = QPushButton("Configure via BLE")
        conn_layout.addWidget(self.ble_btn)

        layout.addWidget(conn_group)

        # Subscriptions table
        sub_group = QGroupBox("Subscriptions")
        sub_layout = QVBoxLayout(sub_group)

        self.sub_table = QTableWidget()
        self.sub_table.setColumnCount(4)
        self.sub_table.setHorizontalHeaderLabels([
            "Topic", "Registers", "QoS", "Actions"
        ])
        self.sub_table.horizontalHeader().setStretchLastSection(True)
        sub_layout.addWidget(self.sub_table)

        btn_layout = QHBoxLayout()
        self.add_sub_btn = QPushButton("Add Subscription")
        self.refresh_btn = QPushButton("Refresh from Gateway")
        btn_layout.addWidget(self.add_sub_btn)
        btn_layout.addWidget(self.refresh_btn)
        btn_layout.addStretch()
        sub_layout.addLayout(btn_layout)

        layout.addWidget(sub_group)

        # Control panel
        ctrl_group = QGroupBox("Write Control")
        ctrl_layout = QVBoxLayout(ctrl_group)

        self.topic_combo = QComboBox()
        ctrl_layout.addWidget(self.topic_combo)

        self.value_layout = QVBoxLayout()
        ctrl_layout.addLayout(self.value_layout)

        self.write_btn = QPushButton("Write Values")
        self.write_btn.setEnabled(False)
        ctrl_layout.addWidget(self.write_btn)

        layout.addWidget(ctrl_group)

        # Status bar
        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)
        self.status_bar.showMessage("Disconnected")

    def _connect_signals(self):
        self.connect_btn.clicked.connect(self._toggle_mqtt_connection)
        self.ble_btn.clicked.connect(self._open_ble_config)
        self.add_sub_btn.clicked.connect(self._add_subscription_dialog)
        self.refresh_btn.clicked.connect(self._refresh_subscriptions)
        self.topic_combo.currentIndexChanged.connect(self._on_topic_selected)
        self.write_btn.clicked.connect(self._write_values)

    def _toggle_mqtt_connection(self):
        if self.mqtt_service and self.mqtt_service.is_connected:
            self.mqtt_service.disconnect()
            self.mqtt_service = None
            self.connect_btn.setText("Connect MQTT")
            self.status_bar.showMessage("Disconnected")
        else:
            self.mqtt_service = MqttWriteService(
                broker=self.broker_input.text(),
                port=self.port_input.value(),
                client_id="desktop_mqtt_control"
            )
            if self.mqtt_service.connect():
                self.connect_btn.setText("Disconnect")
                self.status_bar.showMessage("Connected to MQTT broker")
                self._subscribe_to_responses()
            else:
                QMessageBox.warning(self, "Error", "Failed to connect to MQTT broker")

    def _subscribe_to_responses(self):
        for sub in self.subscriptions:
            self.mqtt_service.subscribe_response(
                sub.response_topic,
                self._handle_response
            )

    def _handle_response(self, response: WriteResponse):
        if response.is_success:
            self.status_bar.showMessage(f"Write successful: {response.topic}")
        elif response.is_partial:
            failed = [r for r in response.results if not r.is_success]
            self.status_bar.showMessage(f"Partial success: {len(failed)} failed")
        else:
            QMessageBox.warning(self, "Write Error", response.error or "Unknown error")

    def _update_table(self):
        self.sub_table.setRowCount(len(self.subscriptions))
        self.topic_combo.clear()

        for i, sub in enumerate(self.subscriptions):
            self.sub_table.setItem(i, 0, QTableWidgetItem(sub.topic))
            reg_text = ", ".join(r.register_id for r in sub.registers)
            self.sub_table.setItem(i, 1, QTableWidgetItem(reg_text))
            self.sub_table.setItem(i, 2, QTableWidgetItem(str(sub.qos)))

            delete_btn = QPushButton("Delete")
            delete_btn.clicked.connect(lambda checked, idx=i: self._delete_subscription(idx))
            self.sub_table.setCellWidget(i, 3, delete_btn)

            self.topic_combo.addItem(sub.topic)

    def _on_topic_selected(self, index: int):
        # Clear previous value inputs
        while self.value_layout.count():
            item = self.value_layout.takeAt(0)
            if item.widget():
                item.widget().deleteLater()

        if 0 <= index < len(self.subscriptions):
            sub = self.subscriptions[index]
            self.write_btn.setEnabled(True)

            if sub.is_multi_register:
                # Multi-register: show input for each register
                for reg in sub.registers:
                    row = QHBoxLayout()
                    row.addWidget(QLabel(f"{reg.register_id}:"))
                    spin = QDoubleSpinBox()
                    spin.setRange(-999999, 999999)
                    spin.setDecimals(2)
                    spin.setObjectName(reg.register_id)
                    row.addWidget(spin)
                    self.value_layout.addLayout(row)
            else:
                # Single register: show single value input
                row = QHBoxLayout()
                row.addWidget(QLabel("Value:"))
                spin = QDoubleSpinBox()
                spin.setRange(-999999, 999999)
                spin.setDecimals(2)
                spin.setObjectName("value")
                row.addWidget(spin)
                self.value_layout.addLayout(row)

    def _write_values(self):
        if not self.mqtt_service or not self.mqtt_service.is_connected:
            QMessageBox.warning(self, "Error", "Not connected to MQTT broker")
            return

        index = self.topic_combo.currentIndex()
        if index < 0:
            return

        sub = self.subscriptions[index]

        if sub.is_multi_register:
            values = {}
            for i in range(self.value_layout.count()):
                item = self.value_layout.itemAt(i)
                if item.layout():
                    for j in range(item.layout().count()):
                        widget = item.layout().itemAt(j).widget()
                        if isinstance(widget, QDoubleSpinBox):
                            values[widget.objectName()] = widget.value()

            self.mqtt_service.write_multiple_values(sub, values)
        else:
            # Find the single value input
            for i in range(self.value_layout.count()):
                item = self.value_layout.itemAt(i)
                if item.layout():
                    for j in range(item.layout().count()):
                        widget = item.layout().itemAt(j).widget()
                        if isinstance(widget, QDoubleSpinBox):
                            self.mqtt_service.write_single_value(sub, widget.value())
                            break

    def _delete_subscription(self, index: int):
        del self.subscriptions[index]
        self._update_table()

    def _add_subscription_dialog(self):
        # Simplified - in production, show dialog to add subscription
        pass

    def _open_ble_config(self):
        # Open BLE configuration dialog
        pass

    def _refresh_subscriptions(self):
        # Refresh from gateway via BLE
        pass


def main():
    app = QApplication(sys.argv)
    window = MqttSubscribeControlWindow()
    window.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
```

---

## 6. Electron Implementation

### 6.1 TypeScript Models

```typescript
// src/models/mqtt-subscription.ts

export interface RegisterReference {
  device_id: string;
  register_id: string;
}

export interface MqttSubscription {
  topic: string;
  qos: number;
  response_topic: string;
  registers: RegisterReference[];
}

export interface CustomSubscribeMode {
  enabled: boolean;
  subscriptions: MqttSubscription[];
}

export interface WriteResult {
  device_id: string;
  register_id: string;
  status: "ok" | "error";
  written_value?: number;
  raw_value?: number;
  error?: string;
}

export interface WriteResponse {
  status: "ok" | "partial" | "error";
  topic: string;
  results: WriteResult[];
  error?: string;
  error_code?: number;
  timestamp: number;
}

export function createSubscription(
  topic: string,
  registers: RegisterReference[],
  qos: number = 1,
  responseTopic?: string
): MqttSubscription {
  return {
    topic,
    qos,
    response_topic: responseTopic || `${topic}/response`,
    registers,
  };
}

export function isMultiRegister(sub: MqttSubscription): boolean {
  return sub.registers.length > 1;
}
```

### 6.2 MQTT Service

```typescript
// src/services/mqtt-write-service.ts

import * as mqtt from "mqtt";
import { MqttSubscription, WriteResponse } from "../models/mqtt-subscription";

export class MqttWriteService {
  private client: mqtt.MqttClient | null = null;
  private responseCallbacks: Map<string, (response: WriteResponse) => void> =
    new Map();

  constructor(
    private broker: string,
    private port: number = 1883,
    private clientId: string = "desktop_app",
    private username?: string,
    private password?: string
  ) {}

  connect(): Promise<void> {
    return new Promise((resolve, reject) => {
      const url = `mqtt://${this.broker}:${this.port}`;

      this.client = mqtt.connect(url, {
        clientId: this.clientId,
        username: this.username,
        password: this.password,
        keepalive: 60,
        clean: true,
      });

      this.client.on("connect", () => {
        console.log("Connected to MQTT broker");
        resolve();
      });

      this.client.on("error", (err) => {
        console.error("MQTT error:", err);
        reject(err);
      });

      this.client.on("message", (topic, payload) => {
        this.handleMessage(topic, payload.toString());
      });
    });
  }

  disconnect(): void {
    if (this.client) {
      this.client.end();
      this.client = null;
    }
  }

  subscribeResponse(
    responseTopic: string,
    callback: (response: WriteResponse) => void
  ): void {
    if (!this.client) throw new Error("Not connected");

    this.responseCallbacks.set(responseTopic, callback);
    this.client.subscribe(responseTopic, { qos: 1 });
    console.log(`Subscribed to: ${responseTopic}`);
  }

  writeSingleValue(subscription: MqttSubscription, value: number): void {
    if (!this.client) throw new Error("Not connected");

    const payload = JSON.stringify({ value });
    this.client.publish(subscription.topic, payload, { qos: subscription.qos });
    console.log(`Published: ${subscription.topic} = ${payload}`);
  }

  writeMultipleValues(
    subscription: MqttSubscription,
    values: Record<string, number>
  ): void {
    if (!this.client) throw new Error("Not connected");

    const payload = JSON.stringify(values);
    this.client.publish(subscription.topic, payload, { qos: subscription.qos });
    console.log(`Published: ${subscription.topic} = ${payload}`);
  }

  private handleMessage(topic: string, payload: string): void {
    const callback = this.responseCallbacks.get(topic);
    if (callback) {
      try {
        const response: WriteResponse = JSON.parse(payload);
        callback(response);
      } catch (e) {
        console.error("Failed to parse response:", e);
      }
    }
  }

  get isConnected(): boolean {
    return this.client?.connected || false;
  }
}
```

### 6.3 React Component Example

```tsx
// src/components/SubscriptionControl.tsx

import React, { useState, useEffect } from "react";
import {
  MqttSubscription,
  WriteResponse,
  isMultiRegister,
} from "../models/mqtt-subscription";
import { MqttWriteService } from "../services/mqtt-write-service";

interface Props {
  subscription: MqttSubscription;
  mqttService: MqttWriteService;
}

export const SubscriptionControl: React.FC<Props> = ({
  subscription,
  mqttService,
}) => {
  const [values, setValues] = useState<Record<string, number>>({});
  const [singleValue, setSingleValue] = useState<number>(0);
  const [lastResponse, setLastResponse] = useState<WriteResponse | null>(null);
  const [isLoading, setIsLoading] = useState(false);

  useEffect(() => {
    // Subscribe to response topic
    mqttService.subscribeResponse(
      subscription.response_topic,
      (response: WriteResponse) => {
        setLastResponse(response);
        setIsLoading(false);
      }
    );

    // Initialize values
    const initial: Record<string, number> = {};
    subscription.registers.forEach((reg) => {
      initial[reg.register_id] = 0;
    });
    setValues(initial);
  }, [subscription, mqttService]);

  const handleWrite = () => {
    setIsLoading(true);

    if (isMultiRegister(subscription)) {
      mqttService.writeMultipleValues(subscription, values);
    } else {
      mqttService.writeSingleValue(subscription, singleValue);
    }
  };

  return (
    <div className="subscription-control">
      <h3>{subscription.topic}</h3>

      {isMultiRegister(subscription) ? (
        <div className="multi-register-inputs">
          {subscription.registers.map((reg) => (
            <div key={reg.register_id} className="register-input">
              <label>{reg.register_id}:</label>
              <input
                type="number"
                value={values[reg.register_id] || 0}
                onChange={(e) =>
                  setValues({
                    ...values,
                    [reg.register_id]: parseFloat(e.target.value),
                  })
                }
              />
            </div>
          ))}
        </div>
      ) : (
        <div className="single-value-input">
          <label>Value:</label>
          <input
            type="number"
            value={singleValue}
            onChange={(e) => setSingleValue(parseFloat(e.target.value))}
          />
        </div>
      )}

      <button onClick={handleWrite} disabled={isLoading}>
        {isLoading ? "Writing..." : "Write"}
      </button>

      {lastResponse && (
        <div className={`response ${lastResponse.status}`}>
          <strong>Status:</strong> {lastResponse.status}
          {lastResponse.error && <p className="error">{lastResponse.error}</p>}
          {lastResponse.results.map((result, i) => (
            <div key={i} className="result">
              {result.register_id}: {result.status}
              {result.written_value !== undefined &&
                ` (${result.written_value})`}
            </div>
          ))}
        </div>
      )}
    </div>
  );
};
```

---

## 7. Error Handling

### Error Codes Reference

| Code | Domain | Description                              | User Action                       |
| ---- | ------ | ---------------------------------------- | --------------------------------- |
| 400  | MQTT   | Invalid JSON payload                     | Check payload format              |
| 400  | MQTT   | Multiple registers require explicit vals | Use per-register payload          |
| 404  | Modbus | Device not found                         | Verify device_id in config        |
| 404  | Modbus | Register not found                       | Verify register_id in config      |
| 315  | Modbus | Register not writable                    | Only FC01/FC03 are writable       |
| 316  | Modbus | Value out of range                       | Check value limits for data type  |
| 318  | Modbus | Write operation failed                   | Check device connection           |

### Python Error Handler

```python
def handle_write_response(response: WriteResponse):
    if response.is_success:
        print(f"Success: All {len(response.results)} registers written")
        return True

    elif response.is_partial:
        success = [r for r in response.results if r.is_success]
        failed = [r for r in response.results if not r.is_success]
        print(f"Partial: {len(success)} succeeded, {len(failed)} failed")
        for r in failed:
            print(f"  - {r.register_id}: {r.error}")
        return False

    else:  # Error
        error_messages = {
            400: "Invalid request format or payload",
            404: "Device or register not found",
            315: "Register is read-only (FC02/FC04)",
            316: "Value out of range for data type",
            318: "Modbus communication failed"
        }
        msg = error_messages.get(response.error_code, response.error)
        print(f"Error ({response.error_code}): {msg}")
        return False
```

---

## 8. Testing

### 8.1 Test Checklist

Configuration Tests:

- [ ] Read current subscriptions via BLE
- [ ] Add single-register subscription via BLE
- [ ] Add multi-register subscription via BLE
- [ ] Update existing subscription
- [ ] Delete subscription
- [ ] Verify gateway restarts MQTT with new config

MQTT Write Tests:

- [ ] Write single value to single-register subscription
- [ ] Write multiple values to multi-register subscription
- [ ] Verify error when sending single value to multi-register
- [ ] Verify response on configured response_topic
- [ ] Test all QoS levels (0, 1, 2)
- [ ] Test reconnect after broker disconnect

### 8.2 Python Test Script

```python
#!/usr/bin/env python3
"""
MQTT Subscribe Control v1.2.0 - Test Script
Tests topic-centric write functionality
"""

import json
import time
import paho.mqtt.client as mqtt
from paho.mqtt.client import CallbackAPIVersion

BROKER = "192.168.1.100"
PORT = 1883

# Test configurations
SINGLE_REGISTER_TOPIC = "factory/hvac/setpoint"
SINGLE_REGISTER_RESPONSE = f"{SINGLE_REGISTER_TOPIC}/response"

MULTI_REGISTER_TOPIC = "factory/valves/control"
MULTI_REGISTER_RESPONSE = "factory/valves/status"


def on_message(client, userdata, msg):
    print(f"\n[RESPONSE] {msg.topic}")
    try:
        payload = json.loads(msg.payload.decode())
        print(json.dumps(payload, indent=2))
    except:
        print(msg.payload.decode())


def create_client():
    client = mqtt.Client(callback_api_version=CallbackAPIVersion.VERSION2)
    client.on_message = on_message
    client.connect(BROKER, PORT)
    client.loop_start()
    return client


def test_single_register_write():
    """Test: Single register with {"value": X} payload"""
    print("\n" + "=" * 60)
    print("TEST: Single Register Write")
    print("=" * 60)

    client = create_client()
    client.subscribe(SINGLE_REGISTER_RESPONSE)
    time.sleep(0.5)

    # Write single value
    payload = {"value": 25.5}
    print(f"Publishing to: {SINGLE_REGISTER_TOPIC}")
    print(f"Payload: {json.dumps(payload)}")
    client.publish(SINGLE_REGISTER_TOPIC, json.dumps(payload), qos=1)

    time.sleep(3)
    client.disconnect()


def test_multi_register_write():
    """Test: Multi-register with per-register values"""
    print("\n" + "=" * 60)
    print("TEST: Multi-Register Write")
    print("=" * 60)

    client = create_client()
    client.subscribe(MULTI_REGISTER_RESPONSE)
    time.sleep(0.5)

    # Write multiple values
    payload = {"ValveA": 1, "ValveB": 0}
    print(f"Publishing to: {MULTI_REGISTER_TOPIC}")
    print(f"Payload: {json.dumps(payload)}")
    client.publish(MULTI_REGISTER_TOPIC, json.dumps(payload), qos=1)

    time.sleep(3)
    client.disconnect()


def test_multi_register_error():
    """Test: Error when single value sent to multi-register"""
    print("\n" + "=" * 60)
    print("TEST: Multi-Register Error (Single Value)")
    print("=" * 60)

    client = create_client()
    client.subscribe(MULTI_REGISTER_RESPONSE)
    time.sleep(0.5)

    # This should return an error
    payload = {"value": 25.5}
    print(f"Publishing to: {MULTI_REGISTER_TOPIC}")
    print(f"Payload: {json.dumps(payload)}")
    print("Expected: ERROR - Multiple registers require explicit values")
    client.publish(MULTI_REGISTER_TOPIC, json.dumps(payload), qos=1)

    time.sleep(3)
    client.disconnect()


def test_qos_levels():
    """Test: QoS 0, 1, 2"""
    print("\n" + "=" * 60)
    print("TEST: QoS Levels")
    print("=" * 60)

    client = create_client()
    client.subscribe(SINGLE_REGISTER_RESPONSE)
    time.sleep(0.5)

    for qos in [0, 1, 2]:
        print(f"\nTesting QoS {qos}...")
        payload = {"value": 20.0 + qos}
        client.publish(SINGLE_REGISTER_TOPIC, json.dumps(payload), qos=qos)
        time.sleep(2)

    client.disconnect()


if __name__ == "__main__":
    print("MQTT Subscribe Control v1.2.0 - Test Suite")
    print(f"Broker: {BROKER}:{PORT}")

    test_single_register_write()
    test_multi_register_write()
    test_multi_register_error()
    test_qos_levels()

    print("\n" + "=" * 60)
    print("All tests completed!")
    print("=" * 60)
```

---

**Document Version:** 1.0 | **Last Updated:** January 12, 2026

**SURIOTA R&D Team** | support@suriota.com
