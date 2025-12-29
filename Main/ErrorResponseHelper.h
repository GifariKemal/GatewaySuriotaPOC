#ifndef ERROR_RESPONSE_HELPER_H
#define ERROR_RESPONSE_HELPER_H

#include <ArduinoJson.h>
#include "UnifiedErrorCodes.h"

/**
 * @brief Error Response Helper - Standardized BLE Error Responses (v1.0.2)
 *
 * Provides helper functions to create consistent error responses
 * that include:
 * - Numeric error_code (from UnifiedErrorCodes)
 * - Domain classification (NETWORK, MQTT, BLE, MODBUS, etc.)
 * - Severity level (INFO, WARNING, ERROR, CRITICAL)
 * - Human-readable message
 * - Recovery suggestion (when available)
 *
 * Response Format:
 * {
 *   "status": "error",
 *   "error_code": 301,
 *   "domain": "MODBUS",
 *   "severity": "ERROR",
 *   "message": "Modbus device timeout",
 *   "suggestion": "Check device power and serial connection",
 *   "type": "modbus"
 * }
 *
 * Mobile App Usage:
 * - Parse error_code for programmatic handling
 * - Display message to user
 * - Show suggestion as actionable hint
 * - Use severity for UI styling (colors, icons)
 */

namespace ErrorResponse
{

  /**
   * @brief Populate a JsonDocument with standardized error response
   *
   * @param doc JsonDocument to populate
   * @param code UnifiedErrorCode from UnifiedErrorCodes.h
   * @param customMessage Optional custom message (overrides default)
   * @param type Optional type field for backward compatibility
   */
  inline void create(JsonDocument &doc, UnifiedErrorCode code,
                     const char *customMessage = nullptr,
                     const char *type = nullptr)
  {
    ErrorDomain domain = getErrorDomain(code);
    ErrorSeverity severity = getDefaultSeverity(code);

    doc["status"] = "error";
    doc["error_code"] = static_cast<int>(code);
    doc["domain"] = getErrorDomainString(domain);
    doc["severity"] = getErrorSeverityString(severity);

    // Use custom message if provided, otherwise use default description
    if (customMessage && strlen(customMessage) > 0)
    {
      doc["message"] = customMessage;
    }
    else
    {
      doc["message"] = getErrorCodeDescription(code);
    }

    // Add recovery suggestion if available
    const char *suggestion = getRecoverySuggestion(code);
    if (suggestion && strlen(suggestion) > 0 && strcmp(suggestion, "Check logs for details") != 0)
    {
      doc["suggestion"] = suggestion;
    }

    // Add type for backward compatibility with mobile app
    if (type && strlen(type) > 0)
    {
      doc["type"] = type;
    }
    else
    {
      // Default type based on domain
      doc["type"] = getErrorDomainString(domain);
    }
  }

  /**
   * @brief Create error response with String message
   */
  inline void create(JsonDocument &doc, UnifiedErrorCode code,
                     const String &customMessage,
                     const String &type = "")
  {
    create(doc, code,
           customMessage.isEmpty() ? nullptr : customMessage.c_str(),
           type.isEmpty() ? nullptr : type.c_str());
  }

  /**
   * @brief Create generic error response (for legacy/unknown errors)
   *
   * Use this when you don't have a specific UnifiedErrorCode
   *
   * @param doc JsonDocument to populate
   * @param message Error message
   * @param type Error type for categorization
   * @param domain Optional domain (defaults to SYSTEM)
   */
  inline void createGeneric(JsonDocument &doc,
                            const String &message,
                            const String &type = "unknown",
                            ErrorDomain domain = DOMAIN_SYSTEM)
  {
    // Map domain to a generic error code
    UnifiedErrorCode code;
    switch (domain)
    {
    case DOMAIN_NETWORK:
      code = ERR_NET_UNKNOWN;
      break;
    case DOMAIN_MQTT:
      code = ERR_MQTT_UNKNOWN;
      break;
    case DOMAIN_BLE:
      code = ERR_BLE_UNKNOWN;
      break;
    case DOMAIN_MODBUS:
      code = ERR_MODBUS_UNKNOWN;
      break;
    case DOMAIN_MEMORY:
      code = ERR_MEM_UNKNOWN;
      break;
    case DOMAIN_CONFIG:
      code = ERR_CFG_UNKNOWN;
      break;
    default:
      code = ERR_SYS_UNKNOWN;
      break;
    }

    doc["status"] = "error";
    doc["error_code"] = static_cast<int>(code);
    doc["domain"] = getErrorDomainString(domain);
    doc["severity"] = getErrorSeverityString(SEVERITY_ERROR);
    doc["message"] = message;
    doc["type"] = type;
  }

  // ============================================================
  // Convenience functions for common error scenarios
  // ============================================================

  // --- Device Errors ---
  inline void deviceNotFound(JsonDocument &doc, const String &deviceId = "")
  {
    create(doc, ERR_CFG_FILE_NOT_FOUND,
           deviceId.isEmpty() ? "Device not found" : ("Device not found: " + deviceId).c_str(),
           "device");
  }

  // --- Register Errors ---
  inline void registerNotFound(JsonDocument &doc, const String &registerId = "")
  {
    create(doc, ERR_CFG_FILE_NOT_FOUND,
           registerId.isEmpty() ? "Register not found" : ("Register not found: " + registerId).c_str(),
           "register");
  }

  // --- Config Errors ---
  inline void configSaveFailed(JsonDocument &doc, const String &details = "")
  {
    create(doc, ERR_CFG_SAVE_FAILED,
           details.isEmpty() ? nullptr : details.c_str(),
           "config");
  }

  inline void configLoadFailed(JsonDocument &doc, const String &details = "")
  {
    create(doc, ERR_CFG_LOAD_FAILED,
           details.isEmpty() ? nullptr : details.c_str(),
           "config");
  }

  inline void invalidJson(JsonDocument &doc, const String &parseError = "")
  {
    create(doc, ERR_CFG_INVALID_JSON,
           parseError.isEmpty() ? nullptr : ("Invalid JSON: " + parseError).c_str(),
           "parse");
  }

  // --- Memory Errors ---
  inline void memoryAllocationFailed(JsonDocument &doc, const String &details = "")
  {
    create(doc, ERR_MEM_ALLOCATION_FAILED,
           details.isEmpty() ? nullptr : details.c_str(),
           "memory");
  }

  // --- BLE Errors ---
  inline void bleWriteFailed(JsonDocument &doc, const String &details = "")
  {
    create(doc, ERR_BLE_WRITE_FAILED,
           details.isEmpty() ? nullptr : details.c_str(),
           "ble");
  }

  // --- Modbus Errors ---
  inline void modbusTimeout(JsonDocument &doc, const String &deviceId = "")
  {
    create(doc, ERR_MODBUS_DEVICE_TIMEOUT,
           deviceId.isEmpty() ? nullptr : ("Device timeout: " + deviceId).c_str(),
           "modbus");
  }

  // --- System Errors ---
  inline void systemError(JsonDocument &doc, const String &message = "")
  {
    create(doc, ERR_SYS_UNKNOWN,
           message.isEmpty() ? nullptr : message.c_str(),
           "system");
  }

  // --- Network Errors ---
  inline void networkUnavailable(JsonDocument &doc)
  {
    create(doc, ERR_NET_NO_NETWORK_AVAILABLE, nullptr, "network");
  }

  // --- MQTT Errors ---
  inline void mqttConnectionFailed(JsonDocument &doc, const String &broker = "")
  {
    create(doc, ERR_MQTT_CONNECTION_FAILED,
           broker.isEmpty() ? nullptr : ("Failed to connect to: " + broker).c_str(),
           "mqtt");
  }

} // namespace ErrorResponse

#endif // ERROR_RESPONSE_HELPER_H
