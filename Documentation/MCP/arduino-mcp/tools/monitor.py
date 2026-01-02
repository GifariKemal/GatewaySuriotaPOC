"""
Serial Monitor Tool
Handles serial communication with Arduino/ESP32 boards.
"""

import asyncio
from typing import Dict, Any, Optional


class SerialMonitor:
    """Serial monitor using arduino-cli."""

    def __init__(self):
        self.arduino_cli = "arduino-cli"

    async def _run_command(
        self, cmd: list[str], timeout: Optional[int] = None
    ) -> Dict[str, Any]:
        """Run a shell command and return results."""
        try:
            process = await asyncio.create_subprocess_exec(
                *cmd, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE
            )

            try:
                if timeout:
                    stdout, stderr = await asyncio.wait_for(
                        process.communicate(), timeout=timeout
                    )
                else:
                    stdout, stderr = await process.communicate()

                return {
                    "success": True,
                    "stdout": stdout.decode("utf-8", errors="replace"),
                    "stderr": stderr.decode("utf-8", errors="replace"),
                }

            except asyncio.TimeoutError:
                # Timeout is expected for monitor (we want to capture output for X seconds)
                try:
                    process.terminate()
                    await asyncio.wait_for(process.wait(), timeout=2)
                except:
                    process.kill()
                    await process.wait()

                return {"success": True, "stdout": "", "stderr": "", "timeout": True}

        except FileNotFoundError:
            return {
                "success": False,
                "error": "arduino-cli not found. Please install Arduino CLI.",
                "stdout": "",
                "stderr": "",
            }
        except Exception as e:
            return {"success": False, "error": str(e), "stdout": "", "stderr": ""}

    async def start_monitor(
        self, port: str, baudrate: int = 115200, duration: int = 10
    ) -> Dict[str, Any]:
        """
        Start serial monitor and capture output.

        Args:
            port: Serial port to monitor
            baudrate: Baud rate for serial communication
            duration: Duration in seconds to monitor (0 = instructions only)

        Returns:
            Dict with monitor output
        """
        if duration == 0:
            # Return instructions for continuous monitoring
            return {
                "success": True,
                "message": "To monitor continuously, run this command in terminal:",
                "command": f"arduino-cli monitor -p {port} -c baudrate={baudrate}",
                "instructions": [
                    f"1. Open a terminal/command prompt",
                    f"2. Run: arduino-cli monitor -p {port} -c baudrate={baudrate}",
                    f"3. Press Ctrl+C to stop monitoring",
                ],
            }

        # Build monitor command
        cmd = [self.arduino_cli, "monitor", "-p", port, "-c", f"baudrate={baudrate}"]

        print(f"Monitoring {port} at {baudrate} baud for {duration} seconds...")

        # Capture output with async streaming
        output_lines = []

        try:
            process = await asyncio.create_subprocess_exec(
                *cmd, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE
            )

            # Read output for specified duration
            start_time = asyncio.get_event_loop().time()
            end_time = start_time + duration

            while asyncio.get_event_loop().time() < end_time:
                try:
                    line = await asyncio.wait_for(
                        process.stdout.readline(), timeout=0.5
                    )

                    if line:
                        decoded_line = line.decode("utf-8", errors="replace").strip()
                        if decoded_line:
                            output_lines.append(decoded_line)
                            print(f"  {decoded_line}")  # Real-time output
                    else:
                        # EOF reached
                        break

                except asyncio.TimeoutError:
                    # No data available, continue waiting
                    continue

            # Terminate monitor
            process.terminate()
            try:
                await asyncio.wait_for(process.wait(), timeout=2)
            except asyncio.TimeoutError:
                process.kill()
                await process.wait()

            return {
                "success": True,
                "port": port,
                "baudrate": baudrate,
                "duration": duration,
                "lines_captured": len(output_lines),
                "output": "\n".join(output_lines),
                "message": f"Captured {len(output_lines)} lines in {duration} seconds",
            }

        except Exception as e:
            return {
                "success": False,
                "error": f"Failed to monitor serial port: {str(e)}",
                "output": "\n".join(output_lines) if output_lines else "",
            }

    async def send_data(
        self, port: str, data: str, baudrate: int = 115200
    ) -> Dict[str, Any]:
        """
        Send data to serial port.

        Args:
            port: Serial port
            data: Data to send
            baudrate: Baud rate

        Returns:
            Dict with send results
        """
        # Arduino CLI doesn't have a direct "send" command
        # This would require using pyserial or similar library

        return {
            "success": False,
            "error": "Send data feature requires pyserial library (not implemented in arduino-cli)",
            "suggestion": "Use arduino-cli monitor with interactive mode or implement with pyserial",
        }
