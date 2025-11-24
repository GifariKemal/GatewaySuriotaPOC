"""
Arduino Flasher Tool
Handles firmware uploading to Arduino/ESP32 boards.
"""

import asyncio
import os
import re
from pathlib import Path
from typing import Dict, Any, Optional, List


class ArduinoFlasher:
    """Arduino firmware uploader using arduino-cli."""

    def __init__(self):
        self.arduino_cli = "arduino-cli"

    async def _run_command(self, cmd: list[str], cwd: Optional[str] = None) -> Dict[str, Any]:
        """Run a shell command and return results."""
        try:
            process = await asyncio.create_subprocess_exec(
                *cmd,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE,
                cwd=cwd
            )

            stdout, stderr = await process.communicate()

            return {
                "success": process.returncode == 0,
                "returncode": process.returncode,
                "stdout": stdout.decode('utf-8', errors='replace'),
                "stderr": stderr.decode('utf-8', errors='replace')
            }

        except FileNotFoundError:
            return {
                "success": False,
                "error": "arduino-cli not found. Please install Arduino CLI.",
                "stdout": "",
                "stderr": ""
            }
        except Exception as e:
            return {
                "success": False,
                "error": str(e),
                "stdout": "",
                "stderr": ""
            }

    async def list_boards(self) -> Dict[str, Any]:
        """
        List all connected Arduino boards.

        Returns:
            Dict with list of connected boards
        """
        cmd = [self.arduino_cli, "board", "list", "--format", "json"]

        result = await self._run_command(cmd)

        if result["success"]:
            try:
                import json
                boards_data = json.loads(result["stdout"])

                boards = []
                for board in boards_data.get("detected_ports", []):
                    port_info = {
                        "port": board.get("port", {}).get("address", ""),
                        "protocol": board.get("port", {}).get("protocol", ""),
                        "label": board.get("port", {}).get("label", ""),
                        "boards": []
                    }

                    # Get matching boards
                    matching_boards = board.get("matching_boards", [])
                    for b in matching_boards:
                        port_info["boards"].append({
                            "name": b.get("name", ""),
                            "fqbn": b.get("fqbn", "")
                        })

                    boards.append(port_info)

                return {
                    "success": True,
                    "boards": boards,
                    "count": len(boards),
                    "message": f"Found {len(boards)} connected board(s)"
                }

            except json.JSONDecodeError:
                # Fallback to text parsing
                return self._parse_board_list_text(result["stdout"])
        else:
            return {
                "success": False,
                "error": "Failed to list boards",
                "details": result.get("stderr", "")
            }

    def _parse_board_list_text(self, output: str) -> Dict[str, Any]:
        """Parse board list from text output (fallback)."""
        boards = []
        lines = output.strip().split('\n')

        for line in lines[1:]:  # Skip header
            if line.strip():
                parts = re.split(r'\s{2,}', line.strip())
                if len(parts) >= 2:
                    boards.append({
                        "port": parts[0],
                        "protocol": parts[1] if len(parts) > 1 else "",
                        "type": parts[2] if len(parts) > 2 else "",
                        "boards": []
                    })

        return {
            "success": True,
            "boards": boards,
            "count": len(boards),
            "message": f"Found {len(boards)} connected port(s)"
        }

    async def upload(self, project_path: str, port: str = "",
                     fqbn: str = "esp32:esp32:esp32s3",
                     verify: bool = True) -> Dict[str, Any]:
        """
        Upload firmware to connected board.

        Args:
            project_path: Path to project directory containing .ino file
            port: Serial port (auto-detect if empty)
            fqbn: Fully Qualified Board Name
            verify: Verify upload after writing

        Returns:
            Dict with upload results
        """
        # Validate project path
        project_path = os.path.abspath(project_path)
        if not os.path.isdir(project_path):
            return {
                "success": False,
                "error": f"Project directory not found: {project_path}"
            }

        # Find .ino file
        ino_files = list(Path(project_path).glob("*.ino"))
        if not ino_files:
            return {
                "success": False,
                "error": f"No .ino file found in {project_path}"
            }

        sketch_path = str(ino_files[0])

        # Auto-detect port if not specified
        if not port:
            boards_result = await self.list_boards()
            if boards_result["success"] and boards_result["count"] > 0:
                port = boards_result["boards"][0]["port"]
                print(f"Auto-detected port: {port}")
            else:
                return {
                    "success": False,
                    "error": "No boards detected. Please connect a board or specify port manually."
                }

        # Build upload command
        cmd = [
            self.arduino_cli, "upload",
            "--fqbn", fqbn,
            "--port", port
        ]

        if verify:
            cmd.append("--verify")

        cmd.append(sketch_path)

        print(f"Uploading to {port}...")
        print(f"FQBN: {fqbn}")

        result = await self._run_command(cmd, cwd=project_path)

        # Parse upload output
        if result["success"]:
            output = result["stdout"] + result["stderr"]

            # Extract upload info
            size_match = re.search(r'(\d+) bytes', output)
            upload_size = int(size_match.group(1)) if size_match else 0

            result.update({
                "message": "Upload successful",
                "port": port,
                "fqbn": fqbn,
                "upload_size": upload_size,
                "verified": verify
            })
        else:
            # Parse error messages
            error_output = result["stderr"]

            if "No such file or directory" in error_output:
                result["error"] = f"Port {port} not found. Check connection."
            elif "Permission denied" in error_output:
                result["error"] = f"Permission denied on {port}. Try running as administrator or check port permissions."
            elif "Device or resource busy" in error_output:
                result["error"] = f"Port {port} is busy. Close other programs using this port."
            else:
                result["error"] = "Upload failed. Check error details."

            result["message"] = "Upload failed"

        return result

    async def auto_detect_port(self) -> Optional[str]:
        """
        Auto-detect the first available board port.

        Returns:
            Port string or None if no boards found
        """
        boards_result = await self.list_boards()

        if boards_result["success"] and boards_result["count"] > 0:
            return boards_result["boards"][0]["port"]

        return None
