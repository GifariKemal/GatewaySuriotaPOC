#ifndef UNIFIED_ERROR_CODES_H
#define UNIFIED_ERROR_CODES_H

#include <Arduino.h>

/*
 * @brief Unified Error Code System - Centralized error definitions
 *
 * Provides a unified error code system across all firmware subsystems:
 * - Network (WiFi/Ethernet)
 * - MQTT Broker
 * - BLE Connectivity
 * - Modbus Devices
 * - Memory Management
 * - Configuration/Storage
 * - System Health
 *
 * Features:
 * - Hierarchical error codes by domain
 * - Severity levels for error classification
 * - Descriptive error messages
 * - Recovery suggestions
 * - Error history tracking
 */

// Error Severity Levels
enum ErrorSeverity
{
  SEVERITY_INFO = 0,    // Informational - no action needed
  SEVERITY_WARNING = 1, // Warning - monitor but operational
  SEVERITY_ERROR = 2,   // Error - functionality impaired
  SEVERITY_CRITICAL = 3 // Critical - immediate attention required
};

// Error Domains
enum ErrorDomain
{
  DOMAIN_NETWORK = 0, // Network/WiFi/Ethernet
  DOMAIN_MQTT = 1,    // MQTT Broker communication
  DOMAIN_BLE = 2,     // Bluetooth Low Energy
  DOMAIN_MODBUS = 3,  // Modbus RTU devices
  DOMAIN_MEMORY = 4,  // Memory/PSRAM allocation
  DOMAIN_CONFIG = 5,  // Configuration/Storage
  DOMAIN_SYSTEM = 6   // System health (uptime, temp, etc)
};

// Unified Error Codes (0-999)
//  Format: DOMAIN_SPECIFIC_ERROR
//  Ranges by domain:
//    Network:    0-99
//    MQTT:       100-199
//    BLE:        200-299
//    Modbus:     300-399
//    Memory:     400-499
//    Config:     500-599
//    System:     600-699

enum UnifiedErrorCode
{
  // NETWORK ERRORS (0-99)
  ERR_NET_SUCCESS = 0,                 // No error
  ERR_NET_NO_WIFI_AP = 1,              // WiFi AP not found
  ERR_NET_WIFI_AUTH_FAILED = 2,        // WiFi authentication failed
  ERR_NET_WIFI_CONNECTION_TIMEOUT = 3, // WiFi connection timeout
  ERR_NET_WIFI_DISCONNECTED = 4,       // WiFi disconnected unexpectedly
  ERR_NET_ETHERNET_CABLE_DOWN = 5,     // Ethernet cable disconnected
  ERR_NET_ETHERNET_NO_IP = 6,          // Ethernet no IP assigned
  ERR_NET_NO_NETWORK_AVAILABLE = 7,    // No network available (WiFi+Ethernet both down)
  ERR_NET_DNS_RESOLUTION_FAILED = 8,   // DNS resolution failed
  ERR_NET_HYSTERESIS_BLOCKING = 9,     // Network switch blocked by hysteresis
  ERR_NET_SIGNAL_QUALITY_LOW = 10,     // WiFi signal quality too low
  ERR_NET_FAILOVER_TIMEOUT = 11,       // Network failover took too long
  ERR_NET_CONFIGURATION_INVALID = 12,  // Network configuration invalid
  ERR_NET_INTERFACE_ERROR = 13,        // Physical interface error
  ERR_NET_UNKNOWN = 99,                // Unknown network error

  // MQTT ERRORS (100-199)
  ERR_MQTT_SUCCESS = 100,               // No error
  ERR_MQTT_BROKER_UNREACHABLE = 101,    // Broker not reachable
  ERR_MQTT_CONNECTION_FAILED = 102,     // Connection to broker failed
  ERR_MQTT_AUTHENTICATION_FAILED = 103, // Authentication failed
  ERR_MQTT_PUBLISH_FAILED = 104,        // Publish operation failed
  ERR_MQTT_SUBSCRIBE_FAILED = 105,      // Subscribe operation failed
  ERR_MQTT_TIMEOUT = 106,               // MQTT operation timeout
  ERR_MQTT_INVALID_CREDENTIALS = 107,   // Invalid credentials
  ERR_MQTT_BROKER_DISCONNECT = 108,     // Broker disconnected
  ERR_MQTT_QUEUE_FULL = 109,            // Persistent queue full
  ERR_MQTT_MESSAGE_EXPIRED = 110,       // Message timeout expired
  ERR_MQTT_SERIALIZE_FAILED = 111,      // JSON serialization failed
  ERR_MQTT_DESERIALIZE_FAILED = 112,    // JSON deserialization failed
  ERR_MQTT_NETWORK_UNAVAILABLE = 113,   // Network unavailable for MQTT
  ERR_MQTT_UNKNOWN = 199,               // Unknown MQTT error

  // BLE ERRORS (200-299)
  ERR_BLE_SUCCESS = 200,                  // No error
  ERR_BLE_MTU_NEGOTIATION_TIMEOUT = 201,  // MTU negotiation timeout
  ERR_BLE_MTU_NEGOTIATION_FAILED = 202,   // MTU negotiation failed
  ERR_BLE_CONNECTION_TIMEOUT = 203,       // BLE connection timeout
  ERR_BLE_CONNECTION_REJECTED = 204,      // BLE connection rejected
  ERR_BLE_DISCONNECTED = 205,             // BLE device disconnected
  ERR_BLE_GATT_ERROR = 206,               // GATT communication error
  ERR_BLE_CHARACTERISTIC_NOT_FOUND = 207, // GATT characteristic not found
  ERR_BLE_NOTIFY_FAILED = 208,            // Notification enable failed
  ERR_BLE_WRITE_FAILED = 209,             // Write operation failed
  ERR_BLE_READ_FAILED = 210,              // Read operation failed
  ERR_BLE_INSUFFICIENT_MEMORY = 211,      // Insufficient memory for BLE
  ERR_BLE_ADAPTER_ERROR = 212,            // BLE adapter hardware error
  ERR_BLE_SCAN_FAILED = 213,              // Device scan failed
  ERR_BLE_UNKNOWN = 299,                  // Unknown BLE error

  // MODBUS ERRORS (300-399)
  ERR_MODBUS_SUCCESS = 300,                // No error
  ERR_MODBUS_DEVICE_TIMEOUT = 301,         // Device no response
  ERR_MODBUS_CRC_ERROR = 302,              // CRC check failed
  ERR_MODBUS_INVALID_RESPONSE = 303,       // Invalid response format
  ERR_MODBUS_EXCEPTION_CODE = 304,         // Device returned exception
  ERR_MODBUS_SERIAL_ERROR = 305,           // Serial port error
  ERR_MODBUS_BUFFER_OVERFLOW = 306,        // Buffer overflow
  ERR_MODBUS_FUNCTION_NOT_SUPPORTED = 307, // Function not supported by device
  ERR_MODBUS_INVALID_ADDRESS = 308,        // Invalid register address
  ERR_MODBUS_COIL_READ_FAILED = 309,       // Coil read failed
  ERR_MODBUS_REGISTER_READ_FAILED = 310,   // Register read failed
  ERR_MODBUS_DEVICE_DISABLED = 311,        // Device disabled/backoff
  ERR_MODBUS_INVALID_PARAMETER = 312,      // Invalid parameter
  ERR_MODBUS_UNKNOWN = 399,                // Unknown Modbus error

  // MEMORY ERRORS (400-499)
  ERR_MEM_SUCCESS = 400,                // No error
  ERR_MEM_ALLOCATION_FAILED = 401,      // Memory allocation failed
  ERR_MEM_FRAGMENTATION_HIGH = 402,     // Memory fragmentation too high
  ERR_MEM_UTILIZATION_HIGH = 403,       // Memory utilization high
  ERR_MEM_UTILIZATION_CRITICAL = 404,   // Memory utilization critical
  ERR_MEM_SAFETY_MARGIN_BREACHED = 405, // Safety margin breached
  ERR_MEM_PSRAM_UNAVAILABLE = 406,      // PSRAM not available
  ERR_MEM_STACK_OVERFLOW = 407,         // Stack overflow detected
  ERR_MEM_HEAP_CORRUPTION = 408,        // Heap corruption detected
  ERR_MEM_ALLOCATION_TOO_LARGE = 409,   // Allocation request too large
  ERR_MEM_UNKNOWN = 499,                // Unknown memory error

  // CONFIG/STORAGE ERRORS (500-599)
  ERR_CFG_SUCCESS = 500,            // No error
  ERR_CFG_FILE_NOT_FOUND = 501,     // Config file not found
  ERR_CFG_FILE_CORRUPTED = 502,     // Config file corrupted
  ERR_CFG_INVALID_JSON = 503,       // Invalid JSON in config
  ERR_CFG_SAVE_FAILED = 504,        // Config save failed
  ERR_CFG_LOAD_FAILED = 505,        // Config load failed
  ERR_CFG_FILESYSTEM_ERROR = 506,   // Filesystem error (LittleFS)
  ERR_CFG_INSUFFICIENT_SPACE = 507, // Insufficient storage space
  ERR_CFG_WRITE_PROTECTED = 508,    // Storage write protected
  ERR_CFG_INVALID_VALUE = 509,      // Invalid configuration value
  ERR_CFG_UNKNOWN = 599,            // Unknown config error

  // SYSTEM ERRORS (600-699)
  ERR_SYS_SUCCESS = 600,            // No error
  ERR_SYS_WATCHDOG_TRIGGERED = 601, // Watchdog timeout
  ERR_SYS_STACK_OVERFLOW = 602,     // Stack overflow
  ERR_SYS_BROWNOUT_DETECTED = 603,  // Brownout reset (low voltage)
  ERR_SYS_THERMAL_WARNING = 604,    // Temperature warning
  ERR_SYS_THERMAL_CRITICAL = 605,   // Temperature critical
  ERR_SYS_POWER_ISSUE = 606,        // Power supply issue
  ERR_SYS_UNKNOWN_RESET = 607,      // Unknown reset reason
  ERR_SYS_UNKNOWN = 699             // Unknown system error
};

// Error Context Information
struct ErrorContext
{
  // Identification
  UnifiedErrorCode code = ERR_SYS_UNKNOWN;
  ErrorSeverity severity = SEVERITY_INFO;
  ErrorDomain domain = DOMAIN_SYSTEM;

  // Timing
  unsigned long timestamp = 0;          // When error occurred
  unsigned long occurrenceCount = 0;    // How many times occurred
  unsigned long lastOccurrenceTime = 0; // Last occurrence timestamp

  // Details
  String deviceId = "";      // Device identifier (for Modbus)
  uint32_t detailValue1 = 0; // Custom detail (e.g., register address)
  uint32_t detailValue2 = 0; // Custom detail (e.g., expected vs actual)
  String description = "";   // Human-readable description

  // Recovery
  bool isRecoverable = true;          // Can auto-recover?
  String recoveryHint = "";           // Recovery suggestion
  uint32_t suggestedRetryDelayMs = 0; // Suggested retry delay
};

// Error Statistics
struct ErrorStatistics
{
  uint32_t totalErrorsReported = 0; // Total errors seen
  uint32_t criticalErrorCount = 0;  // Critical severity count
  uint32_t errorErrorCount = 0;     // Error severity count
  uint32_t warningCount = 0;        // Warning severity count
  uint32_t infoCount = 0;           // Info severity count

  uint32_t errorsThisHour = 0; // Errors in last hour
  uint32_t errorsThisDay = 0;  // Errors in last day

  unsigned long lastErrorTime = 0;                  // When last error occurred
  UnifiedErrorCode lastErrorCode = ERR_SYS_UNKNOWN; // Last error code
  ErrorDomain mostFrequentDomain = DOMAIN_SYSTEM;   // Domain with most errors
};

// Helper Functions for Error Code Conversion
inline ErrorDomain getErrorDomain(UnifiedErrorCode code)
{
  if (code < 100)
    return DOMAIN_NETWORK;
  if (code < 200)
    return DOMAIN_MQTT;
  if (code < 300)
    return DOMAIN_BLE;
  if (code < 400)
    return DOMAIN_MODBUS;
  if (code < 500)
    return DOMAIN_MEMORY;
  if (code < 600)
    return DOMAIN_CONFIG;
  return DOMAIN_SYSTEM;
}

inline ErrorSeverity getDefaultSeverity(UnifiedErrorCode code)
{
  if (code == ERR_NET_SUCCESS || code == ERR_MQTT_SUCCESS ||
      code == ERR_BLE_SUCCESS || code == ERR_MODBUS_SUCCESS ||
      code == ERR_MEM_SUCCESS || code == ERR_CFG_SUCCESS ||
      code == ERR_SYS_SUCCESS)
  {
    return SEVERITY_INFO;
  }
  if (code == ERR_MEM_UTILIZATION_CRITICAL || code == ERR_MEM_SAFETY_MARGIN_BREACHED ||
      code == ERR_NET_NO_NETWORK_AVAILABLE || code == ERR_SYS_WATCHDOG_TRIGGERED ||
      code == ERR_SYS_THERMAL_CRITICAL)
  {
    return SEVERITY_CRITICAL;
  }
  if (code == ERR_NET_SIGNAL_QUALITY_LOW || code == ERR_MEM_UTILIZATION_HIGH ||
      code == ERR_MEM_FRAGMENTATION_HIGH)
  {
    return SEVERITY_WARNING;
  }
  return SEVERITY_ERROR;
}

inline const char *getErrorDomainString(ErrorDomain domain)
{
  switch (domain)
  {
  case DOMAIN_NETWORK:
    return "NETWORK";
  case DOMAIN_MQTT:
    return "MQTT";
  case DOMAIN_BLE:
    return "BLE";
  case DOMAIN_MODBUS:
    return "MODBUS";
  case DOMAIN_MEMORY:
    return "MEMORY";
  case DOMAIN_CONFIG:
    return "CONFIG";
  case DOMAIN_SYSTEM:
    return "SYSTEM";
  default:
    return "UNKNOWN";
  }
}

inline const char *getErrorSeverityString(ErrorSeverity severity)
{
  switch (severity)
  {
  case SEVERITY_INFO:
    return "INFO";
  case SEVERITY_WARNING:
    return "WARN";
  case SEVERITY_ERROR:
    return "ERR";
  case SEVERITY_CRITICAL:
    return "CRIT";
  default:
    return "UNKNOWN";
  }
}

inline const char *getErrorCodeDescription(UnifiedErrorCode code)
{
  switch (code)
  {
  case ERR_NET_SUCCESS:
    return "Network OK";
  case ERR_NET_NO_WIFI_AP:
    return "WiFi AP not found";
  case ERR_NET_WIFI_AUTH_FAILED:
    return "WiFi authentication failed";
  case ERR_NET_WIFI_CONNECTION_TIMEOUT:
    return "WiFi connection timeout";
  case ERR_NET_WIFI_DISCONNECTED:
    return "WiFi disconnected";
  case ERR_NET_ETHERNET_CABLE_DOWN:
    return "Ethernet cable down";
  case ERR_NET_ETHERNET_NO_IP:
    return "Ethernet no IP";
  case ERR_NET_NO_NETWORK_AVAILABLE:
    return "No network available";
  case ERR_NET_DNS_RESOLUTION_FAILED:
    return "DNS resolution failed";
  case ERR_NET_HYSTERESIS_BLOCKING:
    return "Hysteresis blocking switch";
  case ERR_NET_SIGNAL_QUALITY_LOW:
    return "WiFi signal quality low";
  case ERR_NET_FAILOVER_TIMEOUT:
    return "Network failover timeout";

  case ERR_MQTT_SUCCESS:
    return "MQTT OK";
  case ERR_MQTT_BROKER_UNREACHABLE:
    return "MQTT broker unreachable";
  case ERR_MQTT_CONNECTION_FAILED:
    return "MQTT connection failed";
  case ERR_MQTT_AUTHENTICATION_FAILED:
    return "MQTT auth failed";
  case ERR_MQTT_PUBLISH_FAILED:
    return "MQTT publish failed";
  case ERR_MQTT_SUBSCRIBE_FAILED:
    return "MQTT subscribe failed";
  case ERR_MQTT_TIMEOUT:
    return "MQTT timeout";
  case ERR_MQTT_QUEUE_FULL:
    return "MQTT queue full";
  case ERR_MQTT_MESSAGE_EXPIRED:
    return "MQTT message expired";

  case ERR_BLE_SUCCESS:
    return "BLE OK";
  case ERR_BLE_MTU_NEGOTIATION_TIMEOUT:
    return "BLE MTU negotiation timeout";
  case ERR_BLE_CONNECTION_TIMEOUT:
    return "BLE connection timeout";
  case ERR_BLE_DISCONNECTED:
    return "BLE device disconnected";
  case ERR_BLE_GATT_ERROR:
    return "BLE GATT error";
  case ERR_BLE_WRITE_FAILED:
    return "BLE write failed";
  case ERR_BLE_READ_FAILED:
    return "BLE read failed";

  case ERR_MODBUS_SUCCESS:
    return "Modbus OK";
  case ERR_MODBUS_DEVICE_TIMEOUT:
    return "Modbus device timeout";
  case ERR_MODBUS_CRC_ERROR:
    return "Modbus CRC error";
  case ERR_MODBUS_INVALID_RESPONSE:
    return "Modbus invalid response";
  case ERR_MODBUS_DEVICE_DISABLED:
    return "Modbus device disabled";

  case ERR_MEM_SUCCESS:
    return "Memory OK";
  case ERR_MEM_ALLOCATION_FAILED:
    return "Memory allocation failed";
  case ERR_MEM_FRAGMENTATION_HIGH:
    return "Memory fragmentation high";
  case ERR_MEM_UTILIZATION_HIGH:
    return "Memory utilization high";
  case ERR_MEM_UTILIZATION_CRITICAL:
    return "Memory utilization critical";

  case ERR_CFG_SUCCESS:
    return "Config OK";
  case ERR_CFG_FILE_NOT_FOUND:
    return "Config file not found";
  case ERR_CFG_FILE_CORRUPTED:
    return "Config file corrupted";
  case ERR_CFG_INVALID_JSON:
    return "Invalid JSON in config";
  case ERR_CFG_SAVE_FAILED:
    return "Config save failed";
  case ERR_CFG_FILESYSTEM_ERROR:
    return "Filesystem error";

  case ERR_SYS_SUCCESS:
    return "System OK";
  case ERR_SYS_WATCHDOG_TRIGGERED:
    return "Watchdog triggered";
  case ERR_SYS_THERMAL_WARNING:
    return "Thermal warning";
  case ERR_SYS_THERMAL_CRITICAL:
    return "Thermal critical";
  case ERR_SYS_POWER_ISSUE:
    return "Power supply issue";

  default:
    return "Unknown error";
  }
}

inline const char *getRecoverySuggestion(UnifiedErrorCode code)
{
  switch (code)
  {
  case ERR_NET_NO_WIFI_AP:
    return "Check WiFi SSID and availability";
  case ERR_NET_WIFI_AUTH_FAILED:
    return "Verify WiFi password";
  case ERR_NET_WIFI_CONNECTION_TIMEOUT:
    return "Move closer to AP or check signal";
  case ERR_NET_ETHERNET_CABLE_DOWN:
    return "Check Ethernet connection";
  case ERR_NET_DNS_RESOLUTION_FAILED:
    return "Check DNS settings or internet";
  case ERR_NET_SIGNAL_QUALITY_LOW:
    return "Improve WiFi signal or move closer";

  case ERR_MQTT_BROKER_UNREACHABLE:
    return "Check broker connectivity and network";
  case ERR_MQTT_AUTHENTICATION_FAILED:
    return "Verify MQTT credentials";
  case ERR_MQTT_QUEUE_FULL:
    return "Wait for queue to drain or restart";

  case ERR_BLE_MTU_NEGOTIATION_TIMEOUT:
    return "Restart BLE device or restart gateway";
  case ERR_BLE_CONNECTION_TIMEOUT:
    return "Move closer to device and retry";
  case ERR_BLE_DISCONNECTED:
    return "Reconnect BLE device";

  case ERR_MODBUS_DEVICE_TIMEOUT:
    return "Check device power and serial connection";
  case ERR_MODBUS_CRC_ERROR:
    return "Check serial connection quality";

  case ERR_MEM_ALLOCATION_FAILED:
    return "Restart gateway or reduce features";
  case ERR_MEM_FRAGMENTATION_HIGH:
    return "Restart gateway to defragment memory";
  case ERR_MEM_UTILIZATION_CRITICAL:
    return "Immediately reduce memory usage";

  case ERR_CFG_FILE_CORRUPTED:
    return "Restore config from backup or reconfigure";
  case ERR_CFG_FILESYSTEM_ERROR:
    return "Check storage or restart gateway";

  case ERR_SYS_THERMAL_CRITICAL:
    return "Cool device or shutdown immediately";
  case ERR_SYS_POWER_ISSUE:
    return "Check power supply";

  default:
    return "Check logs for details";
  }
}

#endif // UNIFIED_ERROR_CODES_H
