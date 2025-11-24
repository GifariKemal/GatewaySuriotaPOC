"""
Arduino Builder Tool
Handles compilation and cleaning of Arduino projects.
"""

import asyncio
import os
import re
from pathlib import Path
from typing import Dict, Any, Optional


class ArduinoBuilder:
    """Arduino project builder using arduino-cli."""

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
                "error": f"arduino-cli not found. Please install Arduino CLI from https://arduino.github.io/arduino-cli/",
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

    async def build(self, project_path: str, fqbn: str = "esp32:esp32:esp32s3",
                    verbose: bool = False) -> Dict[str, Any]:
        """
        Compile Arduino project.

        Args:
            project_path: Path to project directory containing .ino file
            fqbn: Fully Qualified Board Name
            verbose: Enable verbose output

        Returns:
            Dict with compilation results
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

        # Build command
        cmd = [self.arduino_cli, "compile", "--fqbn", fqbn]

        if verbose:
            cmd.append("--verbose")

        cmd.append(sketch_path)

        print(f"Building project: {sketch_path}")
        print(f"FQBN: {fqbn}")

        result = await self._run_command(cmd, cwd=project_path)

        # Parse build output
        if result["success"]:
            # Extract binary info
            output = result["stdout"] + result["stderr"]

            # Find sketch size
            size_match = re.search(r'Sketch uses (\d+) bytes', output)
            sketch_size = int(size_match.group(1)) if size_match else 0

            # Find binary path
            bin_match = re.search(r'Writing at.*\.bin', output)

            result.update({
                "message": "Build successful",
                "sketch_size": sketch_size,
                "project_path": project_path,
                "fqbn": fqbn
            })
        else:
            # Parse error messages
            errors = self._parse_build_errors(result["stderr"])
            result.update({
                "message": "Build failed",
                "errors": errors,
                "suggestion": self._suggest_fix(errors)
            })

        return result

    async def clean(self, project_path: str) -> Dict[str, Any]:
        """
        Clean build artifacts.

        Args:
            project_path: Path to project directory

        Returns:
            Dict with clean results
        """
        project_path = os.path.abspath(project_path)

        # Arduino CLI doesn't have a clean command, so we manually remove build directory
        build_dir = os.path.join(project_path, "build")

        try:
            if os.path.exists(build_dir):
                import shutil
                shutil.rmtree(build_dir)
                return {
                    "success": True,
                    "message": f"Cleaned build directory: {build_dir}"
                }
            else:
                return {
                    "success": True,
                    "message": "No build directory found (already clean)"
                }
        except Exception as e:
            return {
                "success": False,
                "error": f"Failed to clean: {str(e)}"
            }

    def _parse_build_errors(self, stderr: str) -> list[Dict[str, str]]:
        """Parse compilation errors from stderr."""
        errors = []

        # Common error patterns
        error_pattern = r'(.+?):(\d+):(\d+):\s*(error|warning):\s*(.+)'

        for match in re.finditer(error_pattern, stderr, re.MULTILINE):
            file_path, line, col, severity, message = match.groups()
            errors.append({
                "file": file_path,
                "line": int(line),
                "column": int(col),
                "severity": severity,
                "message": message.strip()
            })

        return errors

    def _suggest_fix(self, errors: list[Dict[str, str]]) -> str:
        """Suggest fixes based on common error patterns."""
        if not errors:
            return ""

        suggestions = []

        for error in errors[:3]:  # Limit to first 3 errors
            msg = error["message"].lower()

            if "was not declared" in msg or "not declared in this scope" in msg:
                suggestions.append(
                    f"Missing declaration: Check if you need to #include a library or declare the variable/function"
                )
            elif "no matching function" in msg:
                suggestions.append(
                    "Function signature mismatch: Check function parameters and types"
                )
            elif "does not name a type" in msg:
                suggestions.append(
                    "Missing type definition: Check if you need to #include the appropriate header"
                )
            elif "expected" in msg and "before" in msg:
                suggestions.append(
                    "Syntax error: Check for missing semicolons, braces, or parentheses"
                )
            elif "undefined reference" in msg:
                suggestions.append(
                    "Linker error: Check if library is properly installed and linked"
                )

        return "\n".join(suggestions) if suggestions else "Check compilation errors above"

    async def auto_fix_errors(self, project_path: str, error_log: str) -> Dict[str, Any]:
        """
        Attempt to automatically fix common compilation errors.

        This is a basic implementation that can be enhanced with LLM integration.

        Args:
            project_path: Path to project directory
            error_log: Error log from failed compilation

        Returns:
            Dict with fix suggestions
        """
        errors = self._parse_build_errors(error_log)

        fixes = []
        for error in errors:
            msg = error["message"].lower()

            # Check for common fixable issues
            if "arduinojson" in msg and "was not declared" in msg:
                fixes.append({
                    "error": error["message"],
                    "fix": "Install ArduinoJson library",
                    "command": "arduino-cli lib install ArduinoJson"
                })
            elif "rtclib" in msg and "was not declared" in msg:
                fixes.append({
                    "error": error["message"],
                    "fix": "Install RTClib library",
                    "command": "arduino-cli lib install RTClib"
                })
            elif "ethernet" in msg and "was not declared" in msg:
                fixes.append({
                    "error": error["message"],
                    "fix": "Install Ethernet library",
                    "command": "arduino-cli lib install Ethernet"
                })

        return {
            "success": True,
            "errors_found": len(errors),
            "fixes_suggested": len(fixes),
            "fixes": fixes,
            "message": f"Found {len(fixes)} potential fixes for {len(errors)} errors"
        }
