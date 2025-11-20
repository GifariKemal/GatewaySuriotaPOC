# Contributing to Suriota Gateway Modbus IIoT

Thank you for your interest in contributing to the Suriota Gateway Modbus IIoT project! This document provides guidelines and instructions for contributing.

## Code of Conduct

- Be respectful and inclusive to all contributors
- Focus on constructive feedback and collaboration
- Report inappropriate behavior to the maintainers

## How to Contribute

### 1. Reporting Bugs

Before creating a bug report, please check the [Issues](https://github.com/suriota/srt-mgate-1210/issues) page to ensure the bug hasn't already been reported.

**When reporting a bug, include:**
- Clear, descriptive title
- Detailed description of the issue
- Steps to reproduce the problem
- Expected behavior vs. actual behavior
- Device configuration (ESP32-S3 variant, network type, connected peripherals)
- Firmware version
- Serial console output or logs
- Screenshots/diagrams if applicable

### 2. Suggesting Enhancements

Enhancement suggestions are tracked as GitHub Issues. When creating an enhancement suggestion:

- Use a clear, descriptive title
- Provide a detailed description of the suggested enhancement
- List some examples of how this enhancement would be used
- Explain why this enhancement would be useful
- List some alternative implementations or approaches

### 3. Contributing Testing Tools & Scripts

We welcome contributions to our testing infrastructure:

**Python Testing Scripts:**
- BLE communication test utilities
- Modbus slave simulators with various configurations
- Server configuration automation tools
- Network testing and validation scripts

**When contributing Python scripts:**
- Place in appropriate directory (Device_Testing, Modbus_Slave_Simulator, etc.)
- Include `requirements.txt` with pinned versions
- Add comprehensive docstrings and comments
- Create `00_START_HERE.txt` or update existing one
- Document script purpose in DOCUMENTATION.md
- Use virtual environments for development
- Follow PEP 8 style guidelines

**Hardware Testing Samples:**
- Add to `Sample Code Test HW Related/` directory
- Include wiring diagrams or pin configurations
- Document expected behavior and output
- Test on actual hardware before submission

**UI/UX Contributions:**
- Mockup files go in `Mockup UI/` directory
- Include screenshots of rendered output
- Document interaction flows
- Ensure mobile responsiveness for web interfaces

### 4. Submitting Changes

#### Fork and Branch

1. Fork the repository on GitHub
2. Clone your fork locally:
   ```bash
   git clone https://github.com/your-username/srt-mgate-1210.git
   cd srt-mgate-1210
   git checkout -b feature/your-feature-name
   ```

#### Make Changes

1. **Code Style Guidelines:**

   **C++/Arduino Firmware:**
   - Follow Arduino/C++ coding conventions
   - Use consistent indentation (2 spaces for Arduino, 4 spaces for C++)
   - Use meaningful variable and function names
   - Add comments for complex logic
   - Keep lines under 100 characters where possible
   - Use camelCase for variables and functions
   - Use PascalCase for class names

   **Python Scripts:**
   - Follow PEP 8 style guide
   - Use 4 spaces for indentation (no tabs)
   - Maximum line length: 100 characters
   - Use snake_case for functions and variables
   - Use PascalCase for class names
   - Add docstrings for all functions and classes
   - Type hints recommended for function signatures
   - Use meaningful variable names (no single letters except loop counters)

2. **Commit Guidelines:**
   - Write clear, descriptive commit messages
   - Use present tense ("Add feature" not "Added feature")
   - Limit commit titles to 50 characters
   - Reference issues and pull requests (e.g., "Fixes #123")
   - Example:
     ```
     Fix NTP synchronization timeout issue

     Increases NTP timeout from 10s to 30s and adds multi-server
     fallback (pool.ntp.org, time.nist.gov, time.google.com)
     to improve reliability on slow networks.

     Fixes #42
     ```

3. **Testing:**

   **Firmware Testing:**
   - Test your changes thoroughly on actual ESP32-S3 hardware
   - Document testing methodology in your pull request
   - Include before/after serial output if applicable
   - Test with different configurations:
     - Different Modbus register counts (use Device_Testing scripts)
     - Different data transmission intervals
     - Network interruptions and reconnection
     - Power cycling scenarios
     - BLE configuration changes (use Python test scripts)

   **Python Testing Scripts:**
   - If you add/modify Python test scripts:
     - Test in isolated virtual environment
     - Update `requirements.txt` if adding dependencies
     - Verify BLE connectivity with actual gateway
     - Document expected output in docstrings
     - Add usage instructions in script header

   **Modbus Simulator Testing:**
   - Use provided Modbus slave simulators for development
   - Test RTU communication on both serial ports
   - Verify TCP communication over network
   - Test with various register counts (5-50 registers)

   **Documentation Testing:**
   - Verify all code examples in documentation work
   - Test API endpoints documented in Docs/API.md
   - Validate configuration examples in README.md

#### Push and Create Pull Request

1. Push to your fork:
   ```bash
   git push origin feature/your-feature-name
   ```

2. Submit a pull request to the main repository with:
   - Clear description of the changes
   - Reference to related issues (Fixes #123)
   - Testing results and methodology
   - Any breaking changes or new dependencies
   - Hardware/firmware configuration used for testing

## Development Setup

### Prerequisites

**Firmware Development:**
- Arduino IDE 2.0+ or PlatformIO Core
- ESP32-S3 board package installed
- Required libraries:
  - ArduinoJson (7.4.2+)
  - RTClib (2.1.4+) by Adafruit
  - NTPClient (3.2.1+) by Fabrice Weinberg
  - Ethernet (2.0.2+) by Arduino
  - TBPubSubClient (2.12.1+) by ThingsBoard
  - ArduinoHttpClient (0.6.1+) by Arduino
  - ModbusMaster (2.0.1+) by Doc Walker
  - OneButton (2.0+) by Matthias Hertel
  - See [Docs/LIBRARIES.md](Docs/LIBRARIES.md) for complete list

**Python Testing & Development:**
- Python 3.8 or higher
- pip package manager
- Virtual environment tool (venv or virtualenv)
- Bluetooth adapter with BLE support (for testing BLE features)
- Serial port access (for Modbus RTU testing)

### Building and Testing

**Using PlatformIO:**
```bash
# Build firmware
platformio run

# Upload to device
platformio run --target upload

# Monitor serial output
platformio device monitor
```

**Using Arduino IDE:**
1. Open the main sketch file
2. Select ESP32-S3 board and COM port
3. Click Upload
4. Monitor serial output at 115200 baud

### Python Testing Environment

The project includes Python scripts for testing BLE functionality, Modbus simulation, and server configuration. To set up the testing environment:

**Prerequisites:**
- Python 3.8 or higher
- pip package manager
- Bluetooth adapter (for BLE testing)

**Setup Virtual Environment:**
```bash
# Create virtual environment
python -m venv .venv

# Activate virtual environment
# On Windows:
.venv\Scripts\activate
# On Linux/Mac:
source .venv/bin/activate

# Install dependencies for specific test suite
cd Device_Testing
pip install -r requirements.txt

# Or for Modbus Slave Simulator
cd Modbus_Slave_Simulator
pip install -r requirements.txt
```

**Running Tests:**

1. **Device Testing (BLE CRUD Operations):**
   ```bash
   cd Device_Testing
   python create_device_10_registers.py  # Creates device with 10 registers via BLE
   ```

2. **Modbus Slave Simulator:**
   ```bash
   cd Modbus_Slave_Simulator
   python modbus_slave_10_registers.py   # Starts Modbus RTU slave with 10 registers
   ```

3. **Server Configuration:**
   ```bash
   cd Server_Config_Testing
   python update_server_config.py        # Updates server config via BLE
   ```

4. **General Testing:**
   ```bash
   cd "Pyhton Testing"
   python mqtt_publish_modes_test.py     # Tests MQTT publish modes
   ```

**Testing Guidelines:**
- Always activate virtual environment before running tests
- Read `00_START_HERE.txt` in each testing directory
- Check `DOCUMENTATION.md` for detailed test descriptions
- Use `requirements.txt` to ensure correct dependencies
- Test BLE changes with actual ESP32-S3 hardware
- Verify Modbus communication with physical devices or simulators

### Documentation

- Keep README.md updated with new features
- Update relevant documentation files in `/docs`
- Include code examples for new features
- Add inline comments for complex implementations
- Document any new configuration parameters in FIRMWARE_ANALYSIS.md

## Project Structure

```
SRT-MGATE-1210/
├── Main/                           # ESP32-S3 firmware source
│   ├── Main.ino                   # Main Arduino sketch
│   ├── RTCManager.cpp/h           # RTC/NTP synchronization
│   ├── ModbusRtuService.cpp/h     # Modbus RTU implementation
│   ├── ModbusTcpService.cpp/h     # Modbus TCP implementation
│   ├── NetworkManager.cpp/h       # WiFi/Ethernet/BLE
│   ├── CRUDHandler.cpp/h          # Device/register configuration
│   ├── BLEManager.cpp/h           # BLE communication
│   ├── MqttManager.cpp/h          # MQTT client
│   ├── HttpManager.cpp/h          # HTTP client
│   ├── ConfigManager.cpp/h        # Configuration persistence
│   └── ...                        # Other firmware modules
├── Docs/                           # Complete documentation
│   ├── API.md                     # BLE CRUD API reference
│   ├── MODBUS_DATATYPES.md        # Data types reference
│   ├── LIBRARIES.md               # Dependencies guide
│   ├── TROUBLESHOOTING.md         # Common issues
│   ├── VERSION_HISTORY.md         # Changelog
│   └── Archive/                   # Archived documentation
├── Device_Testing/                 # Python BLE device testing scripts
│   ├── create_device_*.py         # Device creation with N registers
│   ├── requirements.txt           # Python dependencies
│   └── DOCUMENTATION.md           # Testing guide
├── Modbus_Slave_Simulator/         # Modbus slave simulators
│   ├── modbus_slave_*.py          # Simulator with N registers
│   ├── requirements.txt           # Python dependencies
│   ├── .venv/                     # Virtual environment (ignored)
│   └── DOCUMENTATION.md           # Simulator guide
├── Pyhton Testing/                 # General Python test scripts
│   ├── mqtt_publish_modes_test.py # MQTT modes testing
│   ├── network_config_test.py     # Network configuration
│   └── ...                        # Other test utilities
├── Server_Config_Testing/          # Server configuration scripts
│   ├── update_server_config.py    # Server config via BLE
│   └── requirements.txt           # Python dependencies
├── Sample Code Test HW Related/    # Hardware testing samples
│   ├── ETH_WebServer/             # Ethernet examples
│   ├── RS485_modbus/              # Modbus RTU examples
│   ├── RTC_ds3231/                # RTC examples
│   └── ...                        # Other hardware tests
├── Mockup UI/                      # HTML UI mockups
│   ├── Add Register.html          # Register form mockup
│   ├── Device List.html           # Device list mockup
│   └── ...                        # Other UI mockups
├── README.md                       # Main GitHub documentation
├── LICENSE                         # MIT License
├── .gitignore                      # Git ignore rules
└── CONTRIBUTING.md                 # This file
```

## Pull Request Process

1. Update documentation and README.md with any new features or changes
2. Ensure all tests pass and code compiles without errors
3. Update CHANGELOG.md with a summary of your changes
4. Request review from maintainers
5. Address any feedback or requested changes
6. Once approved, your changes will be merged

## Review Process

Pull requests will be reviewed for:

- **Code Quality:** Consistency with project style, readability, efficiency
- **Functionality:** Correct implementation, edge case handling
- **Documentation:** Clear comments, updated docs, example code
- **Testing:** Thorough testing, reproducible results
- **Backward Compatibility:** No breaking changes without justification
- **Performance:** No significant performance degradation

## Licensing

By contributing to this project, you agree that your contributions will be licensed under its MIT License.

## Questions?

- Check existing documentation in `/docs`
- Review existing issues and pull requests
- Contact the maintainers through GitHub Issues

## Recognition

Contributors will be:
- Listed in the project README.md CONTRIBUTORS section
- Credited in the CHANGELOG.md for their contributions
- Acknowledged in release notes

Thank you for contributing to make Suriota Gateway Modbus IIoT better!
