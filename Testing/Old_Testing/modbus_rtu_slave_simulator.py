import minimalmodbus
import serial
import struct
import math
import time

# --- Configuration ---
SLAVE_ADDRESS = 1
SERIAL_PORT = (
    "COM8"  # Change this to your virtual serial port (e.g., 'COM2', '/dev/ttyUSB1')
)
BAUD_RATE = 9600
PARITY = serial.PARITY_NONE
DATA_BITS = 8
STOP_BITS = 1

# --- Test Values for each data type ---
# These values will be converted to raw Modbus registers by the simulator
TEST_VALUES = {
    "int16": 123,
    "uint16": 456,
    "bool": True,
    "int32-be": 123456789,
    "int32-le": -98765432,
    "int32-ws1": 2147483647,  # Max int32
    "int32-ws2": -2147483648,  # Min int32
    "uint32-be": 987654321,
    "uint32-le": 1234567890,
    "uint32-ws1": 4294967295,  # Max uint32
    "uint32-ws2": 1,
    "float32-be": 3.14159,
    "float32-le": -2.71828,
    "float32-ws1": 12345.67,
    "float32-ws2": -0.000123,
    "int64-be": 123456789012345,
    "int64-le": -987654321098765,
    "int64-ws1": 9223372036854775807,  # Max int64
    "int64-ws2": -9223372036854775808,  # Min int64
    "uint64-be": 9876543210987654321,
    "uint64-le": 1234567890123456789,
    "uint64-ws1": 18446744073709551615,  # Max uint64
    "uint64-ws2": 2,
    "float64-be": 2.718281828459045,
    "float64-le": -3.141592653589793,
    "float64-ws1": 1.0 / 3.0,
    "float64-ws2": -123456789.012345,
}

# --- Helper functions for Python-side data conversion to Modbus registers ---


def _float_to_bytes(value, num_bytes, byteorder_char):
    """Convert float to bytes."""
    if num_bytes == 4:
        return struct.pack(f"{byteorder_char}f", value)
    elif num_bytes == 8:
        return struct.pack(f"{byteorder_char}d", value)
    else:
        raise ValueError("Unsupported float byte length")


def _int_to_bytes(value, num_bytes, byteorder_str, signed):
    """Convert int to bytes."""
    return value.to_bytes(num_bytes, byteorder=byteorder_str, signed=signed)


def _bytes_to_uint16_registers(byte_data, word_order):
    """Convert bytes to a list of uint16 Modbus registers based on word order."""
    registers = []
    for i in range(0, len(byte_data), 2):
        registers.append(
            int.from_bytes(byte_data[i : i + 2], byteorder="big")
        )  # Modbus registers are big-endian

    if (
        word_order == "le"
    ):  # Little-endian word order (R2 R1 for 32-bit, R4 R3 R2 R1 for 64-bit)
        registers.reverse()
    elif word_order == "ws1":  # Word Swap 1 (R2 R1 for 32-bit, R2 R1 R4 R3 for 64-bit)
        if len(registers) == 2:  # 32-bit
            registers = [registers[1], registers[0]]
        elif len(registers) == 4:  # 64-bit
            registers = [registers[1], registers[0], registers[3], registers[2]]
    elif word_order == "ws2":  # Word Swap 2 (R3 R4 R1 R2 for 64-bit)
        if len(registers) == 4:  # 64-bit
            registers = [registers[2], registers[3], registers[0], registers[1]]

    return registers


def convert_value_to_modbus_registers(data_type_str, value):
    """Converts a Python value to a list of raw uint16 Modbus registers."""
    base_type = data_type_str
    endianness = "be"  # Default

    dash_index = data_type_str.find("-")
    if dash_index != -1:
        base_type = data_type_str[:dash_index]
        endianness = data_type_str[dash_index + 1 :]

    byteorder_str = "big"  # For int.to_bytes
    byteorder_char = ">"  # For struct.pack
    word_order = "be"  # For _bytes_to_uint16_registers

    # Map endianness string to native byteorder and word_order
    if endianness == "be":
        byteorder_str = "big"
        byteorder_char = ">"
        word_order = "be"
    elif endianness == "le":
        byteorder_str = "little"
        byteorder_char = "<"
        word_order = "le"
    elif endianness == "ws1":  # Word Swap 1 (R2 R1 for 32-bit, R2 R1 R4 R3 for 64-bit)
        byteorder_str = "big"  # Bytes within words are still big-endian
        byteorder_char = ">"
        word_order = "ws1"
    elif endianness == "ws2":  # Word Swap 2 (R1 R2 for 32-bit, R3 R4 R1 R2 for 64-bit)
        byteorder_str = "big"  # Bytes within words are still big-endian
        byteorder_char = ">"
        word_order = "ws2"

    if base_type == "int16":
        return [value & 0xFFFF]
    elif base_type == "uint16":
        return [value & 0xFFFF]
    elif base_type == "bool":
        return [1 if value else 0]
    elif base_type == "int32":
        byte_data = _int_to_bytes(value, 4, byteorder_str, signed=True)
        return _bytes_to_uint16_registers(byte_data, word_order)
    elif base_type == "uint32":
        byte_data = _int_to_bytes(value, 4, byteorder_str, signed=False)
        return _bytes_to_uint16_registers(byte_data, word_order)
    elif base_type == "float32":
        byte_data = _float_to_bytes(value, 4, byteorder_char)
        return _bytes_to_uint16_registers(byte_data, word_order)
    elif base_type == "int64":
        byte_data = _int_to_bytes(value, 8, byteorder_str, signed=True)
        return _bytes_to_uint16_registers(byte_data, word_order)
    elif base_type == "uint64":
        byte_data = _int_to_bytes(value, 8, byteorder_str, signed=False)
        return _bytes_to_uint16_registers(byte_data, word_order)
    elif base_type == "float64":
        byte_data = _float_to_bytes(value, 8, byteorder_char)
        return _bytes_to_uint16_registers(byte_data, word_order)
    else:
        print(f"Warning: Unknown data type {data_type_str}. Defaulting to uint16.")
        return [value & 0xFFFF]  # Fallback


class ModbusSlaveSimulator:
    def __init__(self, port, slave_address):
        self.instrument = minimalmodbus.Instrument(port, slave_address)
        self.instrument.serial.baudrate = BAUD_RATE
        self.instrument.serial.parity = PARITY
        self.instrument.serial.bytesize = DATA_BITS
        self.instrument.serial.stopbits = STOP_BITS
        self.instrument.serial.timeout = 0.05  # Shorter timeout for slave
        self.instrument.mode = minimalmodbus.MODE_RTU

        self.holding_registers = {}
        self.coils = {}

        print(
            f"Modbus RTU Slave Simulator initialized on {port} (Slave ID: {slave_address})"
        )
        print(
            f"Baudrate: {BAUD_RATE}, Parity: {PARITY}, Data bits: {DATA_BITS}, Stop bits: {STOP_BITS}"
        )

    def _setup_registers(self):
        """Populate holding registers and coils with test values."""
        address_counter = 1
        for data_type_str, value in TEST_VALUES.items():
            if "bool" in data_type_str:
                self.coils[address_counter] = 1 if value else 0
                print(f"Coil {address_counter} ({data_type_str}): {value}")
            else:
                registers = convert_value_to_modbus_registers(data_type_str, value)
                for i, reg_value in enumerate(registers):
                    self.holding_registers[address_counter + i] = reg_value
                print(
                    f"Holding Registers {address_counter}-{address_counter + len(registers) - 1} ({data_type_str}): {value} -> Raw: {registers}"
                )

            # Increment address counter by the number of registers used for this data type
            if (
                "int16" in data_type_str
                or "uint16" in data_type_str
                or "bool" in data_type_str
            ):
                address_counter += 1
            elif (
                "int32" in data_type_str
                or "uint32" in data_type_str
                or "float32" in data_type_str
            ):
                address_counter += 2
            elif (
                "int64" in data_type_str
                or "uint64" in data_type_str
                or "float64" in data_type_str
            ):
                address_counter += 4

        print("\n--- Initialized Modbus Registers ---")
        print(
            "Holding Registers:", {k: hex(v) for k, v in self.holding_registers.items()}
        )
        print("Coils:", self.coils)

    def _handle_request(self, request_pdu):
        """Handle incoming Modbus request PDU."""
        function_code = request_pdu[0]

        if function_code == 3:  # Read Holding Registers
            start_address = int.from_bytes(request_pdu[1:3], byteorder="big")
            quantity = int.from_bytes(request_pdu[3:5], byteorder="big")

            response_bytes = [function_code, quantity * 2]  # Function code, byte count

            for i in range(quantity):
                address = start_address + i
                if address in self.holding_registers:
                    value = self.holding_registers[address]
                    response_bytes.extend(
                        value.to_bytes(2, byteorder="big")
                    )  # Modbus registers are big-endian
                else:
                    # Respond with exception if address is invalid
                    return bytes([0x83, 0x02])  # Illegal Data Address

            return bytes(response_bytes)

        elif function_code == 1:  # Read Coils
            start_address = int.from_bytes(request_pdu[1:3], byteorder="big")
            quantity = int.from_bytes(request_pdu[3:5], byteorder="big")

            # For simplicity, only handle quantity = 1 for now
            if quantity != 1:
                return bytes([0x81, 0x03])  # Illegal Data Value

            if start_address in self.coils:
                value = self.coils[start_address]
                response_bytes = [
                    function_code,
                    1,
                    0x01 if value else 0x00,
                ]  # Function code, byte count, coil status
                return bytes(response_bytes)
            else:
                return bytes([0x81, 0x02])  # Illegal Data Address

        else:
            # Respond with exception for unsupported function code
            return bytes([function_code | 0x80, 0x01])  # Illegal Function

    def run(self):
        """Run the Modbus RTU slave simulator."""
        self._setup_registers()

        print("\n--- Starting Modbus RTU Slave Simulator ---")
        print("Waiting for requests...")

        # Minimalmodbus does not have a built-in slave simulator.
        # We need to implement a basic listener.
        # This is a simplified example and might not be robust for all cases.

        ser = serial.Serial(
            port=SERIAL_PORT,
            baudrate=BAUD_RATE,
            parity=PARITY,
            bytesize=DATA_BITS,
            stopbits=STOP_BITS,
            timeout=0.1,  # Short timeout for reading
        )

        while True:
            try:
                request = ser.read(256)  # Read up to 256 bytes
                if request:
                    # Basic Modbus RTU frame parsing (simplified)
                    # Slave ID (1 byte), Function Code (1 byte), Data (N bytes), CRC (2 bytes)

                    if (
                        len(request) < 8
                    ):  # Minimum request length (slave ID, func code, address, quantity, CRC)
                        continue

                    slave_id = request[0]
                    if slave_id != SLAVE_ADDRESS:
                        continue  # Not for this slave

                    # Extract PDU (Function Code + Data)
                    request_pdu = request[1:-2]  # Exclude slave ID and CRC

                    print(f"Received request from master: {request.hex()}")

                    response_pdu = self._handle_request(request_pdu)

                    if response_pdu:
                        # Build full RTU response frame (Slave ID + PDU + CRC)
                        response_frame = bytes([SLAVE_ADDRESS]) + response_pdu
                        crc = minimalmodbus._calculateCrcString(response_frame)
                        response_frame += crc.to_bytes(
                            2, byteorder="little"
                        )  # CRC is little-endian

                        ser.write(response_frame)
                        print(f"Sent response: {response_frame.hex()}")

            except serial.SerialException as e:
                print(f"Serial error: {e}")
                break
            except Exception as e:
                print(f"An unexpected error occurred: {e}")

            time.sleep(0.01)  # Small delay to prevent busy-waiting


if __name__ == "__main__":
    print("--- Modbus RTU Slave Simulator ---")
    print("Ensure your ESP32 is connected to a virtual serial port pair.")
    print(f"This simulator will listen on {SERIAL_PORT}.")
    print("Press Ctrl+C to stop.")

    try:
        simulator = ModbusSlaveSimulator(SERIAL_PORT, SLAVE_ADDRESS)
        simulator.run()
    except KeyboardInterrupt:
        print("\nSimulator stopped by user.")
    except Exception as e:
        print(f"Error starting simulator: {e}")
