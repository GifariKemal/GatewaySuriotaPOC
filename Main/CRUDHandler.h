#ifndef CRUD_HANDLER_H
#define CRUD_HANDLER_H

#include "JsonDocumentPSRAM.h" // BUG #31: MUST BE BEFORE ArduinoJson.h
#include <ArduinoJson.h>
#include "ConfigManager.h"
#include "ServerConfig.h"
#include "LoggingConfig.h"
#include "MemoryManager.h" // For PsramUniquePtr and make_psram_unique
#include <map>
#include <functional>
#include <queue>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// FIXED BUG #30: Define CRUD task stack size constant
// Updated: 24KB insufficient for 26+ registers, increased to 32KB
namespace CRUDConfig
{
  constexpr uint32_t CRUD_TASK_STACK_SIZE = 32768; // 32KB stack for CREATE operations with 50+ registers
}

// Forward declarations to avoid circular dependencies
class BLEManager;
class MqttManager;
class HttpManager;
class OTAManager;

// Command Priority Levels
enum class CommandPriority : uint8_t
{
  PRIORITY_LOW = 2,    // Background operations (read large datasets)
  PRIORITY_NORMAL = 1, // Standard operations (single device CRUD)
  PRIORITY_HIGH = 0    // Time-sensitive operations (emergency stop, config update)
};

// Batch Operation Modes
enum class BatchMode : uint8_t
{
  SEQUENTIAL = 0, // Execute commands one by one
  PARALLEL = 1,   // Execute multiple commands simultaneously
  ATOMIC = 2      // All-or-nothing execution
};

// Individual Command Structure
struct Command
{
  uint64_t id;               // Unique command ID
  CommandPriority priority;  // Priority level
  unsigned long enqueueTime; // When command was enqueued
  String payloadJson;        // BUG #32 FIX: Store as serialized JSON String to avoid .set() corruption
  BLEManager *manager;       // BLE Manager for sending responses

  // Default constructor
  Command() : id(0), priority(CommandPriority::PRIORITY_NORMAL), enqueueTime(0), manager(nullptr) {}

  // Default copy operations (String is copyable)
  Command(const Command &) = default;
  Command &operator=(const Command &) = default;

  // Default move operations (allow std::priority_queue to work)
  Command(Command &&) = default;
  Command &operator=(Command &&) = default;

  // Comparator for priority queue (higher priority = lower value)
  bool operator>(const Command &other) const
  {
    if ((uint8_t)priority != (uint8_t)other.priority)
    {
      return (uint8_t)priority > (uint8_t)other.priority;
    }
    // Same priority: FIFO (older commands first)
    return enqueueTime > other.enqueueTime;
  }
};

// Batch Operation Structure
struct BatchOperation
{
  String batchId;
  BatchMode mode;
  uint32_t totalCommands = 0;
  uint32_t completedCommands = 0;
  uint32_t failedCommands = 0;
  unsigned long startTime = 0;
  unsigned long endTime = 0;
  bool isActive = false;
};

// Batch/Priority Statistics
struct BatchStats
{
  uint32_t totalBatchesProcessed = 0;
  uint32_t totalCommandsProcessed = 0;
  uint32_t totalCommandsFailed = 0;
  uint32_t highPriorityCount = 0;
  uint32_t normalPriorityCount = 0;
  uint32_t lowPriorityCount = 0;
  double averageBatchExecutionTimeMs = 0.0;
  uint32_t queuePeakDepth = 0;
  uint32_t currentQueueDepth = 0;
};

class BLEManager;       // Forward declaration
class ModbusRtuService; // Forward declaration
class ModbusTcpService; // Forward declaration

class CRUDHandler
{
private:
  ConfigManager *configManager;
  ServerConfig *serverConfig;
  LoggingConfig *loggingConfig;
  MqttManager *mqttManager;           // For notifying data interval updates
  HttpManager *httpManager;           // For notifying data interval updates
  ModbusRtuService *modbusRtuService; // NEW: For device control commands
  ModbusTcpService *modbusTcpService; // NEW: For device control commands
  OTAManager *otaManager;             // For OTA update commands via BLE
  String streamDeviceId;

  // Define a type for our command handler functions
  using CommandHandler = std::function<void(BLEManager *, const JsonDocument &)>;

  // Maps to hold handlers for each operation type
  std::map<String, CommandHandler> readHandlers;
  std::map<String, CommandHandler> createHandlers;
  std::map<String, CommandHandler> updateHandlers;
  std::map<String, CommandHandler> deleteHandlers;
  std::map<String, CommandHandler> controlHandlers; // NEW: Device control operations
  std::map<String, CommandHandler> systemHandlers;  // NEW: System operations (factory reset, etc.)
  std::map<String, CommandHandler> otaHandlers;     // OTA update operations (check, update, status)

  // Private method to populate the handler maps
  void setupCommandHandlers();

  // Factory reset helper
  void performFactoryReset();

  // Helper Methods
  void notifyAllServices(); // Notify all services of config changes

  // Priority Queue and Batch Operations
  std::priority_queue<Command, std::vector<Command>, std::greater<Command>> commandQueue;
  SemaphoreHandle_t queueMutex;
  TaskHandle_t commandProcessorTaskHandle;

  std::map<String, BatchOperation> activeBatches;
  uint64_t commandIdCounter;
  BatchStats batchStats;

  // Private methods for batch/priority operations
  void enqueueCommand(BLEManager *manager, const JsonDocument &command, CommandPriority priority);
  void processPriorityQueue();
  static void commandProcessorTask(void *parameter);
  String createBatch(const JsonDocument &batchConfig, BatchMode mode);
  void executeBatchSequential(BLEManager *manager, const String &batchId, JsonArrayConst commands);
  void executeBatchAtomic(BLEManager *manager, const String &batchId, JsonArrayConst commands);
  void logBatchStats();
  void updateQueueDepth();

public:
  CRUDHandler(ConfigManager *config, ServerConfig *serverCfg, LoggingConfig *loggingCfg);
  ~CRUDHandler();

  void handle(BLEManager *manager, const JsonDocument &command);

  // Batch and Priority Methods
  void handleBatchOperation(BLEManager *manager, const JsonDocument &command);
  BatchStats getBatchStats() const;
  void reportStats(JsonObject &statsObj);

  // Set Manager references for data interval updates
  void setMqttManager(MqttManager *mqtt)
  {
    mqttManager = mqtt;
  }
  void setHttpManager(HttpManager *http)
  {
    httpManager = http;
  }

  // NEW: Set Modbus service references for device control
  void setModbusRtuService(ModbusRtuService *rtu)
  {
    modbusRtuService = rtu;
  }
  void setModbusTcpService(ModbusTcpService *tcp)
  {
    modbusTcpService = tcp;
  }

  // OTA Manager reference for OTA commands via BLE
  void setOTAManager(OTAManager *ota)
  {
    otaManager = ota;
  }

  String getStreamDeviceId() const
  {
    return streamDeviceId;
  }
  void clearStreamDeviceId()
  {
    streamDeviceId = "";
  }
  bool isStreaming() const
  {
    return !streamDeviceId.isEmpty();
  }
};

#endif