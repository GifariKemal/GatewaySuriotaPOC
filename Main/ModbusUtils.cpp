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
