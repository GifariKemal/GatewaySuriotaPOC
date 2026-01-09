#!/usr/bin/env python3
"""
Simple MQTT Write Test - Command Line Tool
==========================================
Version: 1.1.0

Quick command-line tool for testing MQTT write commands.

Usage:
    python test_mqtt_write_simple.py --gateway MGate1210_A3B4C5 \
                                      --device D7A3F2 \
                                      --register temp_setpoint \
                                      --value 25.5

Requirements:
    pip install paho-mqtt

Author: SURIOTA R&D Team
"""

import argparse
import json
import time
import sys

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("ERROR: paho-mqtt not installed. Run: pip install paho-mqtt")
    sys.exit(1)


class MQTTWriteTester:
    """Simple MQTT write command tester."""

    def __init__(self, broker: str, port: int):
        self.broker = broker
        self.port = port
        self.client = None
        self.connected = False
        self.response_received = False
        self.response_data = None

    def connect(self, client_id: str = None) -> bool:
        """Connect to MQTT broker."""
        if client_id is None:
            client_id = f"mqtt_tester_{int(time.time())}"

        self.client = mqtt.Client(client_id=client_id)
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message

        try:
            self.client.connect(self.broker, self.port, keepalive=60)
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
        """Disconnect from broker."""
        if self.client:
            self.client.loop_stop()
            self.client.disconnect()

    def write_register(self, gateway_id: str, device_id: str,
                       topic_suffix: str, value, qos: int = 1,
                       timeout: float = 5.0) -> dict:
        """
        Send write command and wait for response.

        Returns:
            dict with status and response data
        """
        # Build topics
        write_topic = f"suriota/{gateway_id}/write/{device_id}/{topic_suffix}"
        response_topic = f"{write_topic}/response"

        print(f"\n{'='*60}")
        print(f"Write Topic:    {write_topic}")
        print(f"Response Topic: {response_topic}")
        print(f"Value:          {value}")
        print(f"QoS:            {qos}")
        print(f"{'='*60}\n")

        # Subscribe to response topic
        self.response_received = False
        self.response_data = None
        self.client.subscribe(response_topic, qos)

        # Publish write command
        payload = str(value)
        result = self.client.publish(write_topic, payload, qos)

        if result.rc != mqtt.MQTT_ERR_SUCCESS:
            return {"status": "error", "error": "Failed to publish"}

        print(f"[TX] Published: {payload}")

        # Wait for response
        start = time.time()
        while not self.response_received and (time.time() - start) < timeout:
            time.sleep(0.1)

        if self.response_received:
            return {"status": "ok", "response": self.response_data}
        else:
            return {"status": "timeout", "error": "No response received"}

    def _on_connect(self, client, userdata, flags, rc):
        """Connection callback."""
        if rc == 0:
            self.connected = True
            print("[OK] Connected to MQTT broker")
        else:
            print(f"[ERROR] Connection failed: {rc}")

    def _on_message(self, client, userdata, msg):
        """Message received callback."""
        try:
            payload = msg.payload.decode('utf-8')
            print(f"\n[RX] Response received:")
            print(f"     Topic: {msg.topic}")

            # Try to parse as JSON
            try:
                data = json.loads(payload)
                print(f"     Payload: {json.dumps(data, indent=2)}")
                self.response_data = data
            except json.JSONDecodeError:
                print(f"     Payload: {payload}")
                self.response_data = payload

            self.response_received = True

        except Exception as e:
            print(f"[ERROR] Message decode error: {e}")


def main():
    parser = argparse.ArgumentParser(
        description="MQTT Write Command Tester",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    # Basic write
    python test_mqtt_write_simple.py --gateway MGate1210_A3B4C5 \\
                                      --device D7A3F2 \\
                                      --register temp_setpoint \\
                                      --value 25.5

    # With custom broker
    python test_mqtt_write_simple.py --broker mqtt.example.com \\
                                      --port 1883 \\
                                      --gateway MGate1210_A3B4C5 \\
                                      --device D7A3F2 \\
                                      --register temp_setpoint \\
                                      --value 100
        """
    )

    parser.add_argument("--broker", "-b", default="broker.hivemq.com",
                        help="MQTT broker address (default: broker.hivemq.com)")
    parser.add_argument("--port", "-p", type=int, default=1883,
                        help="MQTT broker port (default: 1883)")
    parser.add_argument("--gateway", "-g", required=True,
                        help="Gateway ID (e.g., MGate1210_A3B4C5)")
    parser.add_argument("--device", "-d", required=True,
                        help="Device ID (e.g., D7A3F2)")
    parser.add_argument("--register", "-r", required=True,
                        help="Topic suffix / register name (e.g., temp_setpoint)")
    parser.add_argument("--value", "-v", required=True,
                        help="Value to write (number or string)")
    parser.add_argument("--qos", "-q", type=int, default=1, choices=[0, 1, 2],
                        help="QoS level (default: 1)")
    parser.add_argument("--timeout", "-t", type=float, default=5.0,
                        help="Response timeout in seconds (default: 5.0)")

    args = parser.parse_args()

    # Create tester
    tester = MQTTWriteTester(args.broker, args.port)

    print("\n" + "=" * 60)
    print("   MQTT Subscribe Control - Write Tester")
    print("=" * 60)
    print(f"Broker: {args.broker}:{args.port}")

    # Connect
    if not tester.connect():
        print("[ERROR] Failed to connect to broker")
        sys.exit(1)

    try:
        # Parse value
        try:
            if '.' in args.value:
                value = float(args.value)
            else:
                value = int(args.value)
        except ValueError:
            value = args.value

        # Send write command
        result = tester.write_register(
            gateway_id=args.gateway,
            device_id=args.device,
            topic_suffix=args.register,
            value=value,
            qos=args.qos,
            timeout=args.timeout
        )

        # Print result
        print("\n" + "=" * 60)
        print("RESULT:")
        print("=" * 60)

        if result["status"] == "ok":
            response = result.get("response", {})
            if isinstance(response, dict):
                status = response.get("status", "unknown")
                if status == "ok":
                    print(f"[SUCCESS] Write completed successfully")
                    print(f"  Written value: {response.get('written_value', 'N/A')}")
                    print(f"  Raw value: {response.get('raw_value', 'N/A')}")
                else:
                    print(f"[FAILED] Write failed")
                    print(f"  Error: {response.get('error', 'Unknown')}")
                    print(f"  Error code: {response.get('error_code', 'N/A')}")
            else:
                print(f"Response: {response}")
        elif result["status"] == "timeout":
            print("[TIMEOUT] No response received within timeout period")
            print("  - Check if gateway is connected to MQTT broker")
            print("  - Check if subscribe_control is enabled")
            print("  - Check if register has mqtt_subscribe.enabled=true")
        else:
            print(f"[ERROR] {result.get('error', 'Unknown error')}")

    finally:
        tester.disconnect()
        print("\n[OK] Disconnected from broker")


if __name__ == "__main__":
    main()
