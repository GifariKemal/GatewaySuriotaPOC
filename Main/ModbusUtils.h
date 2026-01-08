#ifndef MODBUS_UTILS_H
#define MODBUS_UTILS_H

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * ModbusUtils - Shared Modbus Data Parsing Utilities
 *
 * CRITICAL REFACTORING (Code Deduplication):
 * This class consolidates duplicate Modbus data parsing logic previously
 * scattered across ModbusRtuService and ModbusTcpService.
 *
 * Benefits:
 * - Single source of truth for data type handling
 * - Bug fixes automatically apply to both RTU and TCP
 * - Reduced flash memory usage (~200 lines eliminated)
 * - Easier to add new data types (modify once, works everywhere)
 *
 * Supported Data Types:
 * - Single Register: INT16, UINT16, BOOL, BINARY
 * - Multi-Register (2 words): INT32, UINT32, FLOAT32
 * - Multi-Register (4 words): INT64, UINT64, DOUBLE64
 *
 * Endianness Variants (for multi-register types):
 * - BE: Big Endian (ABCD or W0W1W2W3)
 * - LE: Little Endian (DCBA or B8..B1)
 * - BE_BS: Big Endian + Byte Swap (BADC)
 * - LE_BS: Little Endian + Word Swap (CDAB)
 *
 * Usage Example (RTU Service):
 *   double value = ModbusUtils::processRegisterValue(reg, rawValue);
 *
 * Usage Example (TCP Service):
 *   double value = ModbusUtils::processMultiRegisterValue(
 *     reg, values, count, "FLOAT32", "BE"
 *   );
 *
 * Version: 1.0.0
 * Created: November 25, 2025
 */
class ModbusUtils {
 public:
  /**
   * Process single-register value (INT16, UINT16, BOOL, BINARY)
   *
   * @param reg JsonObject containing "data_type" field
   * @param rawValue Raw 16-bit register value from Modbus
   * @return Parsed value as double (for JSON compatibility)
   */
  static double processRegisterValue(const JsonObject& reg, uint16_t rawValue);

  /**
   * Process multi-register value (INT32, UINT32, FLOAT32, INT64, UINT64,
   * DOUBLE64)
   *
   * @param reg JsonObject containing register configuration (optional, for
   * future use)
   * @param values Array of raw 16-bit register values
   * @param count Number of registers (2 for 32-bit, 4 for 64-bit)
   * @param baseType Data type string ("INT32", "FLOAT32", etc.)
   * @param endianness_variant Byte order string ("BE", "LE", "BE_BS", "LE_BS")
   * @return Parsed value as double
   */
  static double processMultiRegisterValue(const JsonObject& reg,
                                          uint16_t* values, int count,
                                          const char* baseType,
                                          const char* endianness_variant);

  // =========================================================================
  // v1.0.8: WRITE REGISTER SUPPORT
  // =========================================================================

  /**
   * Reverse calibration: Convert user value to raw Modbus value
   * Formula: raw_value = (user_value - offset) / scale
   *
   * @param userValue Calibrated value from user (e.g., 24.56)
   * @param scale Scale factor from register config (default 1.0)
   * @param offset Offset from register config (default 0.0)
   * @return Raw value to write to Modbus register
   */
  static double reverseCalibration(double userValue, float scale, float offset);

  /**
   * Convert value to single register format (16-bit)
   *
   * @param rawValue Raw value after reverse calibration
   * @param dataType Data type string ("INT16", "UINT16", "BOOL")
   * @return 16-bit value ready for Modbus write
   */
  static uint16_t convertToSingleRegister(double rawValue, const char* dataType);

  /**
   * Convert value to multi-register format (32-bit or 64-bit)
   *
   * @param rawValue Raw value after reverse calibration
   * @param dataType Data type string ("INT32", "UINT32", "FLOAT32", etc.)
   * @param endianness Byte order ("BE", "LE", "BE_BS", "LE_BS")
   * @param outValues Output array for register values (must be pre-allocated)
   * @param outCount Output: number of registers written
   * @return true if conversion successful
   */
  static bool convertToMultiRegister(double rawValue, const char* dataType,
                                     const char* endianness, uint16_t* outValues,
                                     int& outCount);

  /**
   * Get the appropriate write function code based on read function code and
   * data type
   *
   * @param readFunctionCode Original read FC (1, 2, 3, 4)
   * @param dataType Data type string
   * @return Write function code (5, 6, 15, 16) or 0 if not writable
   */
  static uint8_t getWriteFunctionCode(uint8_t readFunctionCode,
                                      const char* dataType);

  /**
   * Get number of registers needed for a data type
   *
   * @param dataType Data type string
   * @return Number of 16-bit registers (1, 2, or 4)
   */
  static int getRegisterCount(const char* dataType);

  /**
   * Check if a data type is writable (output type)
   *
   * @param readFunctionCode Original read FC
   * @return true if writable (FC1 coils, FC3 holding registers)
   */
  static bool isWritableType(uint8_t readFunctionCode);

 private:
  // Private constructor (utility class, no instances)
  ModbusUtils() {}
};

#endif  // MODBUS_UTILS_H
