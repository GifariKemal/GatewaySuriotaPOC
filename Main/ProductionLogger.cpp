#include "ProductionLogger.h"
#include "RTCManager.h"

// Static instance
ProductionLogger *ProductionLogger::instance = nullptr;

ProductionLogger::ProductionLogger()
    : logMutex(nullptr),
      heartbeatIntervalMs(60000), // Default: 1 minute
      enabled(true),
      jsonFormat(true),
      bootTime(0),
      errorCount(0),
      warnCount(0),
      lastHeartbeat(0),
      modbusErrorCount(0),
      modbusSuccessCount(0),
      networkReconnectCount(0),
      protocolReconnectCount(0),
      currentNetStatus(NetStatus::NONE),
      mqttStatus(ProtoStatus::OFF),
      httpStatus(ProtoStatus::OFF),
      activeProtocol("none"),
      firmwareVersion("0.0.0"),
      deviceId("UNKNOWN")
{
    logMutex = xSemaphoreCreateMutex();
}

ProductionLogger::~ProductionLogger()
{
    if (logMutex)
    {
        vSemaphoreDelete(logMutex);
        logMutex = nullptr;
    }
}

ProductionLogger *ProductionLogger::getInstance()
{
    if (!instance)
    {
        instance = new ProductionLogger();
    }
    return instance;
}

bool ProductionLogger::begin(const String &version, const String &devId)
{
    if (!logMutex)
    {
        logMutex = xSemaphoreCreateMutex();
    }

    firmwareVersion = version;
    deviceId = devId;
    bootTime = millis();
    errorCount = 0;
    warnCount = 0;

    return true;
}

void ProductionLogger::stop()
{
    enabled = false;
}

void ProductionLogger::setHeartbeatInterval(uint32_t intervalMs)
{
    heartbeatIntervalMs = intervalMs;
}

void ProductionLogger::setEnabled(bool enable)
{
    enabled = enable;
}

void ProductionLogger::setJsonFormat(bool useJson)
{
    jsonFormat = useJson;
}

void ProductionLogger::setNetworkStatus(NetStatus status)
{
    if (currentNetStatus != status)
    {
        if (currentNetStatus != NetStatus::NONE && status != NetStatus::NONE)
        {
            networkReconnectCount++;
        }
        currentNetStatus = status;

        // Log network change
        if (enabled && xSemaphoreTake(logMutex, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            if (jsonFormat)
            {
                Serial.printf("{\"t\":\"NET\",\"up\":%lu,\"net\":\"%s\",\"rc\":%lu}\n",
                              getUptime(), netStatusStr(status), networkReconnectCount);
            }
            else
            {
                Serial.printf("[%lu][NET] Status: %s (reconnects: %lu)\n",
                              getUptime(), netStatusStr(status), networkReconnectCount);
            }
            xSemaphoreGive(logMutex);
        }
    }
}

void ProductionLogger::setMqttStatus(ProtoStatus status)
{
    if (mqttStatus != status)
    {
        if (mqttStatus == ProtoStatus::ERROR || mqttStatus == ProtoStatus::CONNECTING)
        {
            if (status == ProtoStatus::OK)
            {
                protocolReconnectCount++;
            }
        }
        mqttStatus = status;
    }
}

void ProductionLogger::setHttpStatus(ProtoStatus status)
{
    if (httpStatus != status)
    {
        httpStatus = status;
    }
}

void ProductionLogger::setActiveProtocol(const String &protocol)
{
    activeProtocol = protocol;
}

uint32_t ProductionLogger::getUptime()
{
    return (millis() - bootTime) / 1000; // Return seconds
}

const char *ProductionLogger::netStatusStr(NetStatus status)
{
    switch (status)
    {
    case NetStatus::WIFI:
        return "WIFI";
    case NetStatus::ETHERNET:
        return "ETH";
    default:
        return "NONE";
    }
}

const char *ProductionLogger::protoStatusStr(ProtoStatus status)
{
    switch (status)
    {
    case ProtoStatus::OK:
        return "OK";
    case ProtoStatus::CONNECTING:
        return "CONN";
    case ProtoStatus::ERROR:
        return "ERR";
    default:
        return "OFF";
    }
}

const char *ProductionLogger::logTypeStr(ProdLogType type)
{
    switch (type)
    {
    case ProdLogType::HEARTBEAT:
        return "HB";
    case ProdLogType::ERROR:
        return "ERR";
    case ProdLogType::WARNING:
        return "WARN";
    case ProdLogType::SYSTEM:
        return "SYS";
    case ProdLogType::MODBUS:
        return "MB";
    case ProdLogType::NETWORK:
        return "NET";
    case ProdLogType::PROTOCOL:
        return "PROTO";
    default:
        return "UNK";
    }
}

void ProductionLogger::logBoot()
{
    if (!enabled)
        return;

    if (xSemaphoreTake(logMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        size_t freeDram = heap_caps_get_free_size(MALLOC_CAP_8BIT) -
                          heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        size_t freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

        if (jsonFormat)
        {
            Serial.println();
            Serial.printf("{\"t\":\"SYS\",\"e\":\"BOOT\",\"v\":\"%s\",\"id\":\"%s\",\"mem\":{\"d\":%d,\"p\":%d}}\n",
                          firmwareVersion.c_str(),
                          deviceId.c_str(),
                          freeDram,
                          freePsram);
        }
        else
        {
            Serial.println();
            Serial.println("========================================");
            Serial.printf("[SYS] BOOT - SRT-MGATE-1210 v%s\n", firmwareVersion.c_str());
            Serial.printf("[SYS] Device ID: %s\n", deviceId.c_str());
            Serial.printf("[SYS] Memory - DRAM: %d, PSRAM: %d\n", freeDram, freePsram);
            Serial.println("========================================");
        }
        xSemaphoreGive(logMutex);
    }
}

void ProductionLogger::heartbeat()
{
    if (!enabled)
        return;

    uint32_t now = millis();
    if (now - lastHeartbeat < heartbeatIntervalMs)
        return;

    lastHeartbeat = now;

    if (xSemaphoreTake(logMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        size_t freeDram = heap_caps_get_free_size(MALLOC_CAP_8BIT) -
                          heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        size_t freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

        // Get protocol status based on active protocol
        const char *protoStat = "OFF";
        if (activeProtocol == "mqtt")
        {
            protoStat = protoStatusStr(mqttStatus);
        }
        else if (activeProtocol == "http")
        {
            protoStat = protoStatusStr(httpStatus);
        }

        if (jsonFormat)
        {
            // Compact JSON format for easy parsing
            // {"t":"HB","up":3600,"mem":{"d":150000,"p":7500000},"net":"ETH","proto":"mqtt","st":"OK","err":0,"mb":{"ok":100,"er":2}}
            Serial.printf("{\"t\":\"HB\",\"up\":%lu,\"mem\":{\"d\":%d,\"p\":%d},\"net\":\"%s\",\"proto\":\"%s\",\"st\":\"%s\",\"err\":%lu,\"mb\":{\"ok\":%lu,\"er\":%lu}}\n",
                          getUptime(),
                          freeDram,
                          freePsram,
                          netStatusStr(currentNetStatus),
                          activeProtocol.c_str(),
                          protoStat,
                          errorCount,
                          modbusSuccessCount,
                          modbusErrorCount);
        }
        else
        {
            // Human-readable format
            Serial.printf("[%lu][HB] NET:%s PROTO:%s/%s MEM:D%d/P%d ERR:%lu MB:%lu/%lu\n",
                          getUptime(),
                          netStatusStr(currentNetStatus),
                          activeProtocol.c_str(),
                          protoStat,
                          freeDram / 1000, // KB
                          freePsram / 1000,
                          errorCount,
                          modbusSuccessCount,
                          modbusErrorCount);
        }
        xSemaphoreGive(logMutex);
    }
}

void ProductionLogger::forceHeartbeat()
{
    lastHeartbeat = 0; // Reset timer to force next heartbeat
    heartbeat();
}

void ProductionLogger::logError(const char *module, const char *message)
{
    if (!enabled)
        return;

    errorCount++;

    if (xSemaphoreTake(logMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        if (jsonFormat)
        {
            Serial.printf("{\"t\":\"ERR\",\"up\":%lu,\"m\":\"%s\",\"msg\":\"%s\",\"cnt\":%lu}\n",
                          getUptime(), module, message, errorCount);
        }
        else
        {
            Serial.printf("[%lu][ERR][%s] %s (#%lu)\n",
                          getUptime(), module, message, errorCount);
        }
        xSemaphoreGive(logMutex);
    }
}

void ProductionLogger::logWarning(const char *module, const char *message)
{
    if (!enabled)
        return;

    warnCount++;

    if (xSemaphoreTake(logMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        if (jsonFormat)
        {
            Serial.printf("{\"t\":\"WARN\",\"up\":%lu,\"m\":\"%s\",\"msg\":\"%s\"}\n",
                          getUptime(), module, message);
        }
        else
        {
            Serial.printf("[%lu][WARN][%s] %s\n",
                          getUptime(), module, message);
        }
        xSemaphoreGive(logMutex);
    }
}

void ProductionLogger::logSystem(const char *event, const char *detail)
{
    if (!enabled)
        return;

    if (xSemaphoreTake(logMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        if (jsonFormat)
        {
            if (detail)
            {
                Serial.printf("{\"t\":\"SYS\",\"up\":%lu,\"e\":\"%s\",\"d\":\"%s\"}\n",
                              getUptime(), event, detail);
            }
            else
            {
                Serial.printf("{\"t\":\"SYS\",\"up\":%lu,\"e\":\"%s\"}\n",
                              getUptime(), event);
            }
        }
        else
        {
            if (detail)
            {
                Serial.printf("[%lu][SYS] %s: %s\n", getUptime(), event, detail);
            }
            else
            {
                Serial.printf("[%lu][SYS] %s\n", getUptime(), event);
            }
        }
        xSemaphoreGive(logMutex);
    }
}

void ProductionLogger::logModbusStats(uint32_t success, uint32_t errors)
{
    modbusSuccessCount = success;
    modbusErrorCount = errors;
}
