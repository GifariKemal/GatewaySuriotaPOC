"""
Arduino Library Manager Tool
Handles library installation and search operations.
"""

import asyncio
import json
from typing import Dict, Any, List, Optional


class LibraryManager:
    """Arduino library manager using arduino-cli."""

    def __init__(self):
        self.arduino_cli = "arduino-cli"

    async def _run_command(self, cmd: list[str]) -> Dict[str, Any]:
        """Run a shell command and return results."""
        try:
            process = await asyncio.create_subprocess_exec(
                *cmd,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE
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

    async def search_library(self, query: str) -> Dict[str, Any]:
        """
        Search for Arduino libraries.

        Args:
            query: Search query

        Returns:
            Dict with search results
        """
        cmd = [self.arduino_cli, "lib", "search", query, "--format", "json"]

        result = await self._run_command(cmd)

        if result["success"]:
            try:
                libraries_data = json.loads(result["stdout"])

                libraries = []
                for lib in libraries_data.get("libraries", []):
                    lib_info = {
                        "name": lib.get("name", ""),
                        "version": lib.get("latest", {}).get("version", ""),
                        "author": lib.get("latest", {}).get("author", ""),
                        "description": lib.get("latest", {}).get("sentence", ""),
                        "website": lib.get("latest", {}).get("website", "")
                    }
                    libraries.append(lib_info)

                return {
                    "success": True,
                    "query": query,
                    "results": libraries,
                    "count": len(libraries),
                    "message": f"Found {len(libraries)} library(ies) matching '{query}'"
                }

            except json.JSONDecodeError:
                return {
                    "success": False,
                    "error": "Failed to parse library search results",
                    "raw_output": result["stdout"]
                }
        else:
            return {
                "success": False,
                "error": "Library search failed",
                "details": result.get("stderr", "")
            }

    async def install_library(self, library_name: str, version: str = "") -> Dict[str, Any]:
        """
        Install an Arduino library.

        Args:
            library_name: Name of the library to install
            version: Specific version (empty = latest)

        Returns:
            Dict with installation results
        """
        # Build install command
        if version:
            lib_spec = f"{library_name}@{version}"
        else:
            lib_spec = library_name

        cmd = [self.arduino_cli, "lib", "install", lib_spec]

        print(f"Installing library: {lib_spec}...")

        result = await self._run_command(cmd)

        if result["success"]:
            return {
                "success": True,
                "library": library_name,
                "version": version if version else "latest",
                "message": f"Successfully installed {lib_spec}",
                "output": result["stdout"]
            }
        else:
            error_output = result["stderr"]

            if "already installed" in error_output.lower():
                return {
                    "success": True,
                    "library": library_name,
                    "message": f"{library_name} is already installed",
                    "already_installed": True
                }
            else:
                return {
                    "success": False,
                    "library": library_name,
                    "error": "Installation failed",
                    "details": error_output
                }

    async def list_installed(self) -> Dict[str, Any]:
        """
        List all installed libraries.

        Returns:
            Dict with list of installed libraries
        """
        cmd = [self.arduino_cli, "lib", "list", "--format", "json"]

        result = await self._run_command(cmd)

        if result["success"]:
            try:
                libraries_data = json.loads(result["stdout"])

                libraries = []
                for lib in libraries_data.get("installed_libraries", []):
                    lib_info = {
                        "name": lib.get("library", {}).get("name", ""),
                        "version": lib.get("library", {}).get("version", ""),
                        "author": lib.get("library", {}).get("author", ""),
                        "location": lib.get("library", {}).get("install_dir", "")
                    }
                    libraries.append(lib_info)

                return {
                    "success": True,
                    "libraries": libraries,
                    "count": len(libraries),
                    "message": f"{len(libraries)} library(ies) installed"
                }

            except json.JSONDecodeError:
                return {
                    "success": False,
                    "error": "Failed to parse installed libraries",
                    "raw_output": result["stdout"]
                }
        else:
            return {
                "success": False,
                "error": "Failed to list libraries",
                "details": result.get("stderr", "")
            }

    async def update_library(self, library_name: str = "") -> Dict[str, Any]:
        """
        Update library to latest version.

        Args:
            library_name: Name of library to update (empty = all)

        Returns:
            Dict with update results
        """
        if library_name:
            cmd = [self.arduino_cli, "lib", "upgrade", library_name]
        else:
            cmd = [self.arduino_cli, "lib", "upgrade"]

        print(f"Updating {'all libraries' if not library_name else library_name}...")

        result = await self._run_command(cmd)

        if result["success"]:
            return {
                "success": True,
                "library": library_name if library_name else "all",
                "message": "Update successful",
                "output": result["stdout"]
            }
        else:
            return {
                "success": False,
                "library": library_name if library_name else "all",
                "error": "Update failed",
                "details": result["stderr"]
            }

    async def uninstall_library(self, library_name: str) -> Dict[str, Any]:
        """
        Uninstall a library.

        Args:
            library_name: Name of library to uninstall

        Returns:
            Dict with uninstall results
        """
        cmd = [self.arduino_cli, "lib", "uninstall", library_name]

        print(f"Uninstalling library: {library_name}...")

        result = await self._run_command(cmd)

        if result["success"]:
            return {
                "success": True,
                "library": library_name,
                "message": f"Successfully uninstalled {library_name}"
            }
        else:
            return {
                "success": False,
                "library": library_name,
                "error": "Uninstall failed",
                "details": result["stderr"]
            }
