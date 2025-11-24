#!/usr/bin/env python3
"""
Arduino MCP Server
A Model Context Protocol server for Arduino CLI operations.
Supports building, uploading, monitoring ESP32-S3 projects.
"""

import asyncio
import json
import os
import sys
from typing import Any, Optional

from mcp.server import Server
from mcp.types import Tool, TextContent
from tools.builder import ArduinoBuilder
from tools.flasher import ArduinoFlasher
from tools.monitor import SerialMonitor
from tools.library_manager import LibraryManager

# Initialize MCP server
app = Server("arduino-mcp")

# Initialize Arduino tools
builder = ArduinoBuilder()
flasher = ArduinoFlasher()
monitor = SerialMonitor()
lib_manager = LibraryManager()


@app.list_tools()
async def list_tools() -> list[Tool]:
    """List all available Arduino CLI tools."""
    return [
        Tool(
            name="arduino_build",
            description="Compile an Arduino project using arduino-cli. Requires project path.",
            inputSchema={
                "type": "object",
                "properties": {
                    "project_path": {
                        "type": "string",
                        "description": "Path to the Arduino project directory (contains .ino file)"
                    },
                    "fqbn": {
                        "type": "string",
                        "description": "Fully Qualified Board Name (e.g., esp32:esp32:esp32s3). Optional if set in project.",
                        "default": "esp32:esp32:esp32s3"
                    },
                    "verbose": {
                        "type": "boolean",
                        "description": "Enable verbose build output",
                        "default": False
                    }
                },
                "required": ["project_path"]
            }
        ),
        Tool(
            name="arduino_upload",
            description="Upload compiled firmware to connected Arduino/ESP32 board.",
            inputSchema={
                "type": "object",
                "properties": {
                    "project_path": {
                        "type": "string",
                        "description": "Path to the Arduino project directory"
                    },
                    "port": {
                        "type": "string",
                        "description": "Serial port (e.g., COM3, /dev/ttyUSB0). Auto-detect if not specified.",
                        "default": ""
                    },
                    "fqbn": {
                        "type": "string",
                        "description": "Fully Qualified Board Name",
                        "default": "esp32:esp32:esp32s3"
                    },
                    "verify": {
                        "type": "boolean",
                        "description": "Verify upload after writing",
                        "default": True
                    }
                },
                "required": ["project_path"]
            }
        ),
        Tool(
            name="arduino_monitor",
            description="Open serial monitor to view output from connected board.",
            inputSchema={
                "type": "object",
                "properties": {
                    "port": {
                        "type": "string",
                        "description": "Serial port to monitor"
                    },
                    "baudrate": {
                        "type": "integer",
                        "description": "Baud rate for serial communication",
                        "default": 115200
                    },
                    "duration": {
                        "type": "integer",
                        "description": "Duration in seconds to monitor (0 = continuous)",
                        "default": 10
                    }
                },
                "required": ["port"]
            }
        ),
        Tool(
            name="arduino_clean",
            description="Clean build artifacts from Arduino project.",
            inputSchema={
                "type": "object",
                "properties": {
                    "project_path": {
                        "type": "string",
                        "description": "Path to the Arduino project directory"
                    }
                },
                "required": ["project_path"]
            }
        ),
        Tool(
            name="arduino_board_list",
            description="List all connected Arduino boards and their ports.",
            inputSchema={
                "type": "object",
                "properties": {},
                "required": []
            }
        ),
        Tool(
            name="arduino_lib_install",
            description="Install Arduino library using arduino-cli.",
            inputSchema={
                "type": "object",
                "properties": {
                    "library_name": {
                        "type": "string",
                        "description": "Name of the library to install (e.g., 'ArduinoJson')"
                    },
                    "version": {
                        "type": "string",
                        "description": "Specific version to install. Latest if not specified.",
                        "default": ""
                    }
                },
                "required": ["library_name"]
            }
        ),
        Tool(
            name="arduino_lib_search",
            description="Search for Arduino libraries.",
            inputSchema={
                "type": "object",
                "properties": {
                    "query": {
                        "type": "string",
                        "description": "Search query for library name"
                    }
                },
                "required": ["query"]
            }
        ),
        Tool(
            name="arduino_compile_fix",
            description="Attempt to automatically fix compilation errors based on error logs.",
            inputSchema={
                "type": "object",
                "properties": {
                    "project_path": {
                        "type": "string",
                        "description": "Path to the Arduino project directory"
                    },
                    "error_log": {
                        "type": "string",
                        "description": "Compilation error log from previous build attempt"
                    }
                },
                "required": ["project_path", "error_log"]
            }
        )
    ]


@app.call_tool()
async def call_tool(name: str, arguments: Any) -> list[TextContent]:
    """Execute Arduino CLI tool based on requested operation."""

    try:
        if name == "arduino_build":
            result = await builder.build(
                project_path=arguments["project_path"],
                fqbn=arguments.get("fqbn", "esp32:esp32:esp32s3"),
                verbose=arguments.get("verbose", False)
            )
            return [TextContent(type="text", text=json.dumps(result, indent=2))]

        elif name == "arduino_upload":
            result = await flasher.upload(
                project_path=arguments["project_path"],
                port=arguments.get("port", ""),
                fqbn=arguments.get("fqbn", "esp32:esp32:esp32s3"),
                verify=arguments.get("verify", True)
            )
            return [TextContent(type="text", text=json.dumps(result, indent=2))]

        elif name == "arduino_monitor":
            result = await monitor.start_monitor(
                port=arguments["port"],
                baudrate=arguments.get("baudrate", 115200),
                duration=arguments.get("duration", 10)
            )
            return [TextContent(type="text", text=json.dumps(result, indent=2))]

        elif name == "arduino_clean":
            result = await builder.clean(arguments["project_path"])
            return [TextContent(type="text", text=json.dumps(result, indent=2))]

        elif name == "arduino_board_list":
            result = await flasher.list_boards()
            return [TextContent(type="text", text=json.dumps(result, indent=2))]

        elif name == "arduino_lib_install":
            result = await lib_manager.install_library(
                library_name=arguments["library_name"],
                version=arguments.get("version", "")
            )
            return [TextContent(type="text", text=json.dumps(result, indent=2))]

        elif name == "arduino_lib_search":
            result = await lib_manager.search_library(arguments["query"])
            return [TextContent(type="text", text=json.dumps(result, indent=2))]

        elif name == "arduino_compile_fix":
            result = await builder.auto_fix_errors(
                project_path=arguments["project_path"],
                error_log=arguments["error_log"]
            )
            return [TextContent(type="text", text=json.dumps(result, indent=2))]

        else:
            return [TextContent(
                type="text",
                text=json.dumps({
                    "success": False,
                    "error": f"Unknown tool: {name}"
                }, indent=2)
            )]

    except Exception as e:
        return [TextContent(
            type="text",
            text=json.dumps({
                "success": False,
                "error": str(e),
                "tool": name
            }, indent=2)
        )]


async def main():
    """Main entry point for Arduino MCP server."""
    from mcp.server.stdio import stdio_server

    async with stdio_server() as (read_stream, write_stream):
        await app.run(
            read_stream,
            write_stream,
            app.create_initialization_options()
        )


if __name__ == "__main__":
    asyncio.run(main())
