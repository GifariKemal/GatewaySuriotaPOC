#ifndef PRODUCTION_LOGGER_H
#define PRODUCTION_LOGGER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_heap_caps.h>

/**
 * ProductionLogger - Minimal logging for production deployment (1-5+ years)
 *
 * Features:
 * - Periodic heartbeat (configurable interval)
 * - Critical error logging only
 * - System health statistics
 * - Clean, parseable JSON format
 * - No sensitive data exposure
 * - Minimal CPU/memory overhead
 *
 * Output Format:
 * {"t":"HB","up":3600,"mem":{"d":150000,"p":7500000},"net":"ETH","mqtt":"OK"}
 *
 * Legend:
 * - t: Type (HB=Heartbeat, ERR=Error, WARN=Warning, SYS=System)
 * - up: Uptime in seconds
 * - mem.d: Free DRAM bytes
 * - mem.p: Free PSRAM bytes
 * - net: Network type (ETH/WIFI/NONE)
 * - mqtt/http: Protocol status (OK/ERR/OFF)
 */

// Production log types
enum class ProdLogType : uint8_t
{
    HEARTBEAT,  // Periodic status
    ERROR,      // Critical errors
    WARNING,    // Warnings
    SYSTEM,     // System events (boot, OTA, reset)
    MODBUS,     // Modbus errors only
    NETWORK,    // Network state changes
    PROTOCOL    // MQTT/HTTP state changes
};

// Network status for production log
enum class NetStatus : uint8_t
{
    NONE,
    WIFI,
    ETHERNET
};

// Protocol status
enum class ProtoStatus : uint8_t
{
    OFF,
    CONNECTING,
    OK,
    ERROR
};

class ProductionLogger
{
private:
    static ProductionLogger *instance;
    SemaphoreHandle_t logMutex;

    // Configuration
    uint32_t heartbeatIntervalMs;
    bool enabled;
    bool jsonFormat; // true = JSON, false = human readable

    // Statistics
    uint32_t bootTime;
    uint32_t errorCount;
    uint32_t warnCount;
    uint32_t lastHeartbeat;
    uint32_t modbusErrorCount;
    uint32_t modbusSuccessCount;
    uint32_t networkReconnectCount;
    uint32_t protocolReconnectCount;

    // Current state
    NetStatus currentNetStatus;
    ProtoStatus mqttStatus;
    ProtoStatus httpStatus;
    String activeProtocol; // "mqtt" or "http"
    String firmwareVersion;
    String deviceId;

    ProductionLogger();
    ~ProductionLogger();

    // Internal helpers
    void printTimestamp();
    const char *netStatusStr(NetStatus status);
    const char *protoStatusStr(ProtoStatus status);
    const char *logTypeStr(ProdLogType type);

public:
    static ProductionLogger *getInstance();

    // Initialization
    bool begin(const String &version, const String &devId);
    void stop();

    // Configuration
    void setHeartbeatInterval(uint32_t intervalMs);
    void setEnabled(bool enable);
    void setJsonFormat(bool useJson);

    // State updates (call these when state changes)
    void setNetworkStatus(NetStatus status);
    void setMqttStatus(ProtoStatus status);
    void setHttpStatus(ProtoStatus status);
    void setActiveProtocol(const String &protocol);

    // Logging methods
    void heartbeat();                                               // Call periodically
    void logError(const char *module, const char *message);         // Critical errors
    void logWarning(const char *module, const char *message);       // Warnings
    void logSystem(const char *event, const char *detail = nullptr); // System events
    void logModbusStats(uint32_t success, uint32_t errors);         // Modbus summary

    // Statistics
    uint32_t getUptime();
    uint32_t getErrorCount() { return errorCount; }
    uint32_t getWarnCount() { return warnCount; }

    // Boot message (call once at startup)
    void logBoot();

    // Manual heartbeat trigger (for testing)
    void forceHeartbeat();

    // Singleton pattern
    ProductionLogger(const ProductionLogger &) = delete;
    ProductionLogger &operator=(const ProductionLogger &) = delete;
};

// ============================================
// PRODUCTION LOG MACROS
// ============================================

// Only log errors in production mode
#if PRODUCTION_MODE == 1

#define PROD_LOG_ERROR(module, msg)                                    \
    do                                                                 \
    {                                                                  \
        ProductionLogger::getInstance()->logError(module, msg);        \
    } while (0)

#define PROD_LOG_WARN(module, msg)                                     \
    do                                                                 \
    {                                                                  \
        ProductionLogger::getInstance()->logWarning(module, msg);      \
    } while (0)

#define PROD_LOG_SYSTEM(event, detail)                                 \
    do                                                                 \
    {                                                                  \
        ProductionLogger::getInstance()->logSystem(event, detail);     \
    } while (0)

#define PROD_HEARTBEAT()                                               \
    do                                                                 \
    {                                                                  \
        ProductionLogger::getInstance()->heartbeat();                  \
    } while (0)

#else
// Development mode - these become no-ops (use regular LOG_* macros instead)
#define PROD_LOG_ERROR(module, msg) do {} while(0)
#define PROD_LOG_WARN(module, msg) do {} while(0)
#define PROD_LOG_SYSTEM(event, detail) do {} while(0)
#define PROD_HEARTBEAT() do {} while(0)
#endif

#endif // PRODUCTION_LOGGER_H
