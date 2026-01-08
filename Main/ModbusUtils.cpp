#include "ModbusUtils.h"

#include <ctype.h>
#include <string.h>

// ============================================================================
// SINGLE REGISTER VALUE PROCESSING
// ============================================================================

double ModbusUtils::processRegisterValue(const JsonObject& reg,
                                         uint16_t rawValue) {
  // BUG #31: Use const char* and manual uppercase (was String in TCP)
  const char* dataType = reg["data_type"] | "UINT16";
  char dataTypeBuf[32];
  strncpy(dataTypeBuf, dataType, sizeof(dataTypeBuf) - 1);
  dataTypeBuf[sizeof(dataTypeBuf) - 1] = '\0';

  // Manual uppercase conversion
  for (int i = 0; dataTypeBuf[i]; i++) {
    dataTypeBuf[i] = toupper(dataTypeBuf[i]);
  }

  if (strcmp(dataTypeBuf, "INT16") == 0) {
    return (int16_t)rawValue;
  } else if (strcmp(dataTypeBuf, "UINT16") == 0) {
    return rawValue;
  } else if (strcmp(dataTypeBuf, "BOOL") == 0) {
    return rawValue != 0 ? 1.0 : 0.0;
  } else if (strcmp(dataTypeBuf, "BINARY") == 0) {
    return rawValue;
  }

  // Multi-register types - need to read 2 registers
  // For now return single register value, implement multi-register later
  return rawValue;
}

// ============================================================================
// MULTI-REGISTER VALUE PROCESSING
// ============================================================================

double ModbusUtils::processMultiRegisterValue(const JsonObject& reg,
                                              uint16_t* values, int count,
                                              const char* baseType,
                                              const char* endianness_variant) {
  // ========== 2-REGISTER (32-BIT) VALUES ==========
  if (count == 2) {
    uint32_t combined;
    if (strcmp(endianness_variant, "BE") == 0) {  // Big Endian (ABCD)
      combined = ((uint32_t)values[0] << 16) | values[1];
    } else if (strcmp(endianness_variant, "LE") ==
               0) {  // True Little Endian (DCBA)
      combined = (((uint32_t)values[1] & 0xFF) << 24) |
                 (((uint32_t)values[1] & 0xFF00) << 8) |
                 (((uint32_t)values[0] & 0xFF) << 8) |
                 ((uint32_t)values[0] >> 8);
    } else if (strcmp(endianness_variant, "BE_BS") ==
               0) {  // Big Endian + Byte Swap (BADC)
      combined = (((uint32_t)values[0] & 0xFF) << 24) |
                 (((uint32_t)values[0] & 0xFF00) << 8) |
                 (((uint32_t)values[1] & 0xFF) << 8) |
                 ((uint32_t)values[1] >> 8);
    } else if (strcmp(endianness_variant, "LE_BS") ==
               0) {  // Little Endian + Word Swap (CDAB)
      combined = ((uint32_t)values[1] << 16) | values[0];
    } else {  // Default to Big Endian
      combined = ((uint32_t)values[0] << 16) | values[1];
    }

    // Interpret combined value based on base type
    if (strcmp(baseType, "INT32") == 0) {
      return (int32_t)combined;
    } else if (strcmp(baseType, "UINT32") == 0) {
      return combined;
    } else if (strcmp(baseType, "FLOAT32") == 0) {
      // FIXED Bug #5: Use union for safe type conversion (no strict aliasing
      // violation)
      union {
        uint32_t bits;
        float value;
      } converter;
      converter.bits = combined;
      return converter.value;
    }
  }

  // ========== 4-REGISTER (64-BIT) VALUES ==========
  else if (count == 4) {
    uint64_t combined;

    if (strcmp(endianness_variant, "BE") == 0) {  // Big Endian (W0, W1, W2, W3)
      combined = ((uint64_t)values[0] << 48) | ((uint64_t)values[1] << 32) |
                 ((uint64_t)values[2] << 16) | values[3];
    } else if (strcmp(endianness_variant, "LE") ==
               0) {  // True Little Endian (B8..B1)
      uint64_t b1 = values[0] >> 8;
      uint64_t b2 = values[0] & 0xFF;
      uint64_t b3 = values[1] >> 8;
      uint64_t b4 = values[1] & 0xFF;
      uint64_t b5 = values[2] >> 8;
      uint64_t b6 = values[2] & 0xFF;
      uint64_t b7 = values[3] >> 8;
      uint64_t b8 = values[3] & 0xFF;
      combined = (b8 << 56) | (b7 << 48) | (b6 << 40) | (b5 << 32) |
                 (b4 << 24) | (b3 << 16) | (b2 << 8) | b1;
    } else if (strcmp(endianness_variant, "BE_BS") ==
               0) {  // Big Endian with Byte Swap (B2B1, B4B3, B6B5, B8B7)
      // Registers have bytes swapped within each word. Extract bytes and
      // reassemble as BADCFEHG pattern
      uint64_t b1 = (values[0] >> 8) & 0xFF;  // High byte of R1
      uint64_t b2 = values[0] & 0xFF;         // Low byte of R1
      uint64_t b3 = (values[1] >> 8) & 0xFF;  // High byte of R2
      uint64_t b4 = values[1] & 0xFF;         // Low byte of R2
      uint64_t b5 = (values[2] >> 8) & 0xFF;  // High byte of R3
      uint64_t b6 = values[2] & 0xFF;         // Low byte of R3
      uint64_t b7 = (values[3] >> 8) & 0xFF;  // High byte of R4
      uint64_t b8 = values[3] & 0xFF;         // Low byte of R4
      combined = (b2 << 56) | (b1 << 48) | (b4 << 40) | (b3 << 32) |
                 (b6 << 24) | (b5 << 16) | (b8 << 8) | b7;
    } else if (strcmp(endianness_variant, "LE_BS") ==
               0) {  // Little Endian with Word Swap (W3, W2, W1, W0)
      combined = ((uint64_t)values[3] << 48) | ((uint64_t)values[2] << 32) |
                 ((uint64_t)values[1] << 16) | (uint64_t)values[0];
    } else {  // Default to Big Endian
      combined = ((uint64_t)values[0] << 48) | ((uint64_t)values[1] << 32) |
                 ((uint64_t)values[2] << 16) | values[3];
    }

    // Interpret combined value based on base type
    if (strcmp(baseType, "INT64") == 0) {
      return (double)(int64_t)combined;
    } else if (strcmp(baseType, "UINT64") == 0) {
      return (double)combined;
    } else if (strcmp(baseType, "DOUBLE64") == 0) {
      // Safe type-punning using union for IEEE 754 reinterpretation
      union {
        uint64_t bits;
        double value;
      } converter;
      converter.bits = combined;
      return converter.value;
    }
  }

  return values[0];  // Fallback
}

// ============================================================================
// v1.0.8: WRITE REGISTER SUPPORT
// ============================================================================

double ModbusUtils::reverseCalibration(double userValue, float scale,
                                       float offset) {
  // Inverse of: calibrated = (raw * scale) + offset
  // Therefore: raw = (calibrated - offset) / scale
  if (scale == 0.0f) {
    return userValue;  // Avoid division by zero
  }
  return (userValue - offset) / scale;
}

uint16_t ModbusUtils::convertToSingleRegister(double rawValue,
                                              const char* dataType) {
  char dataTypeBuf[32];
  strncpy(dataTypeBuf, dataType, sizeof(dataTypeBuf) - 1);
  dataTypeBuf[sizeof(dataTypeBuf) - 1] = '\0';

  // Manual uppercase conversion
  for (int i = 0; dataTypeBuf[i]; i++) {
    dataTypeBuf[i] = toupper(dataTypeBuf[i]);
  }

  if (strcmp(dataTypeBuf, "INT16") == 0) {
    int16_t val = (int16_t)rawValue;
    return (uint16_t)val;
  } else if (strcmp(dataTypeBuf, "UINT16") == 0) {
    return (uint16_t)rawValue;
  } else if (strcmp(dataTypeBuf, "BOOL") == 0) {
    return rawValue != 0 ? 1 : 0;
  } else if (strcmp(dataTypeBuf, "BINARY") == 0) {
    return (uint16_t)rawValue;
  }

  // Default: treat as UINT16
  return (uint16_t)rawValue;
}

bool ModbusUtils::convertToMultiRegister(double rawValue, const char* dataType,
                                         const char* endianness,
                                         uint16_t* outValues, int& outCount) {
  char dataTypeBuf[32];
  strncpy(dataTypeBuf, dataType, sizeof(dataTypeBuf) - 1);
  dataTypeBuf[sizeof(dataTypeBuf) - 1] = '\0';

  // Manual uppercase conversion
  for (int i = 0; dataTypeBuf[i]; i++) {
    dataTypeBuf[i] = toupper(dataTypeBuf[i]);
  }

  // Extract base type (remove endianness suffix like _BE, _LE, etc.)
  char baseType[16] = {0};
  char* underscore = strchr(dataTypeBuf, '_');
  if (underscore) {
    size_t len = underscore - dataTypeBuf;
    strncpy(baseType, dataTypeBuf, len);
    baseType[len] = '\0';
  } else {
    strcpy(baseType, dataTypeBuf);
  }

  // ========== 32-BIT TYPES (2 REGISTERS) ==========
  if (strcmp(baseType, "INT32") == 0 || strcmp(baseType, "UINT32") == 0 ||
      strcmp(baseType, "FLOAT32") == 0) {
    outCount = 2;
    uint32_t combined;

    if (strcmp(baseType, "FLOAT32") == 0) {
      union {
        float f;
        uint32_t bits;
      } converter;
      converter.f = (float)rawValue;
      combined = converter.bits;
    } else if (strcmp(baseType, "INT32") == 0) {
      combined = (uint32_t)(int32_t)rawValue;
    } else {
      combined = (uint32_t)rawValue;
    }

    // Apply endianness
    if (strcmp(endianness, "BE") == 0) {  // Big Endian (ABCD)
      outValues[0] = (combined >> 16) & 0xFFFF;
      outValues[1] = combined & 0xFFFF;
    } else if (strcmp(endianness, "LE") == 0) {  // Little Endian (DCBA)
      uint8_t a = (combined >> 24) & 0xFF;
      uint8_t b = (combined >> 16) & 0xFF;
      uint8_t c = (combined >> 8) & 0xFF;
      uint8_t d = combined & 0xFF;
      outValues[0] = (d << 8) | c;
      outValues[1] = (b << 8) | a;
    } else if (strcmp(endianness, "BE_BS") == 0) {  // Big Endian + Byte Swap
      uint8_t a = (combined >> 24) & 0xFF;
      uint8_t b = (combined >> 16) & 0xFF;
      uint8_t c = (combined >> 8) & 0xFF;
      uint8_t d = combined & 0xFF;
      outValues[0] = (b << 8) | a;
      outValues[1] = (d << 8) | c;
    } else if (strcmp(endianness, "LE_BS") == 0) {  // Little Endian + Word Swap
      outValues[0] = combined & 0xFFFF;
      outValues[1] = (combined >> 16) & 0xFFFF;
    } else {  // Default: Big Endian
      outValues[0] = (combined >> 16) & 0xFFFF;
      outValues[1] = combined & 0xFFFF;
    }

    return true;
  }

  // ========== 64-BIT TYPES (4 REGISTERS) ==========
  if (strcmp(baseType, "INT64") == 0 || strcmp(baseType, "UINT64") == 0 ||
      strcmp(baseType, "DOUBLE64") == 0) {
    outCount = 4;
    uint64_t combined;

    if (strcmp(baseType, "DOUBLE64") == 0) {
      union {
        double d;
        uint64_t bits;
      } converter;
      converter.d = rawValue;
      combined = converter.bits;
    } else if (strcmp(baseType, "INT64") == 0) {
      combined = (uint64_t)(int64_t)rawValue;
    } else {
      combined = (uint64_t)rawValue;
    }

    // Apply endianness (simplified for common BE case)
    if (strcmp(endianness, "BE") == 0 || strlen(endianness) == 0) {
      outValues[0] = (combined >> 48) & 0xFFFF;
      outValues[1] = (combined >> 32) & 0xFFFF;
      outValues[2] = (combined >> 16) & 0xFFFF;
      outValues[3] = combined & 0xFFFF;
    } else if (strcmp(endianness, "LE_BS") == 0) {  // Word Swap
      outValues[0] = combined & 0xFFFF;
      outValues[1] = (combined >> 16) & 0xFFFF;
      outValues[2] = (combined >> 32) & 0xFFFF;
      outValues[3] = (combined >> 48) & 0xFFFF;
    } else {  // Default: Big Endian
      outValues[0] = (combined >> 48) & 0xFFFF;
      outValues[1] = (combined >> 32) & 0xFFFF;
      outValues[2] = (combined >> 16) & 0xFFFF;
      outValues[3] = combined & 0xFFFF;
    }

    return true;
  }

  // Single register type - use convertToSingleRegister instead
  outCount = 1;
  outValues[0] = convertToSingleRegister(rawValue, dataType);
  return true;
}

uint8_t ModbusUtils::getWriteFunctionCode(uint8_t readFunctionCode,
                                          const char* dataType) {
  // FC2 (Discrete Inputs) and FC4 (Input Registers) are read-only
  if (readFunctionCode == 2 || readFunctionCode == 4) {
    return 0;  // Not writable
  }

  int regCount = getRegisterCount(dataType);

  // FC1 (Read Coils) -> FC5 (Write Single Coil) or FC15 (Write Multiple Coils)
  if (readFunctionCode == 1) {
    return (regCount > 1) ? 15 : 5;
  }

  // FC3 (Read Holding Registers) -> FC6 (Write Single) or FC16 (Write Multiple)
  if (readFunctionCode == 3) {
    return (regCount > 1) ? 16 : 6;
  }

  return 0;  // Unknown function code
}

int ModbusUtils::getRegisterCount(const char* dataType) {
  char dataTypeBuf[32];
  strncpy(dataTypeBuf, dataType, sizeof(dataTypeBuf) - 1);
  dataTypeBuf[sizeof(dataTypeBuf) - 1] = '\0';

  // Manual uppercase conversion
  for (int i = 0; dataTypeBuf[i]; i++) {
    dataTypeBuf[i] = toupper(dataTypeBuf[i]);
  }

  // Extract base type
  char baseType[16] = {0};
  char* underscore = strchr(dataTypeBuf, '_');
  if (underscore) {
    size_t len = underscore - dataTypeBuf;
    strncpy(baseType, dataTypeBuf, len);
    baseType[len] = '\0';
  } else {
    strcpy(baseType, dataTypeBuf);
  }

  // 64-bit types: 4 registers
  if (strcmp(baseType, "INT64") == 0 || strcmp(baseType, "UINT64") == 0 ||
      strcmp(baseType, "DOUBLE64") == 0) {
    return 4;
  }

  // 32-bit types: 2 registers
  if (strcmp(baseType, "INT32") == 0 || strcmp(baseType, "UINT32") == 0 ||
      strcmp(baseType, "FLOAT32") == 0) {
    return 2;
  }

  // 16-bit types: 1 register
  return 1;
}

bool ModbusUtils::isWritableType(uint8_t readFunctionCode) {
  // FC1 (Coils) and FC3 (Holding Registers) are writable
  // FC2 (Discrete Inputs) and FC4 (Input Registers) are read-only
  return (readFunctionCode == 1 || readFunctionCode == 3);
}
