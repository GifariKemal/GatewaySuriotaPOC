/*
 * BLE CRUD Manager for ESP32
 * Production-ready BLE service for Modbus device configuration
 *
 * Hardware: ESP32 with BLE support
 * Features: CRUD operations, fragmentation, FreeRTOS tasks
 * Optimization: 9 firmware optimizations integrated
 * Debug: Development vs Production mode (PRODUCTION_MODE = 0/1)
 *
 * MQTT Fix: Payload corruption fix with separate buffers (2025-11-22)
 * Task Optimization: Low-priority tasks moved to Core 0 for load balancing (2025-11-22)
 */

#define PRODUCTION_MODE 0 // 0 = Development, 1 = Production

// CRITICAL: Override MQTT_MAX_PACKET_SIZE BEFORE any library includes
// This MUST be defined here (not in MqttManager.h) to ensure Arduino IDE
// compiles TBPubSubClient library with the correct packet size limit
#ifndef MQTT_MAX_PACKET_SIZE
#define MQTT_MAX_PACKET_SIZE 16384 // 16KB - allows large payloads without truncation
#endif

#include "DebugConfig.h"       // ← MUST BE FIRST (before all other includes)
#include "MemoryRecovery.h"    // Phase 2 optimization
#include "JsonDocumentPSRAM.h" // BUG #31: Global PSRAM allocator for ALL JsonDocument instances
#include "ProductionLogger.h"  // Production mode minimal logging

// Firmware version and device identification
#define FIRMWARE_VERSION "2.3.3"
#define DEVICE_ID "SRT-MGATE-1210"

#include "BLEManager.h"
#include "CRUDHandler.h"
#include "ConfigManager.h"
#include "ServerConfig.h"
#include "LoggingConfig.h"
#include "NetworkManager.h"
#include "RTCManager.h"
#include "ModbusTcpService.h"
#include "ModbusRtuService.h"
#include "QueueManager.h"
#include "MqttManager.h"
#include "HttpManager.h"
#include "LEDManager.h"
#include "ButtonManager.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>
#include <esp_psram.h>
#include <esp_system.h>

// Add missing include
#include <new>

// Global objects - initialized to nullptr for safety
BLEManager *bleManager = nullptr;
CRUDHandler *crudHandler = nullptr;
ConfigManager *configManager = nullptr;
ServerConfig *serverConfig = nullptr;
LoggingConfig *loggingConfig = nullptr;
NetworkMgr *networkManager = nullptr;
RTCManager *rtcManager = nullptr;
ModbusTcpService *modbusTcpService = nullptr;
ModbusRtuService *modbusRtuService = nullptr;
QueueManager *queueManager = nullptr;
MqttManager *mqttManager = nullptr;
HttpManager *httpManager = nullptr;
LEDManager *ledManager = nullptr;
ButtonManager *buttonManager = nullptr;
ProductionLogger *productionLogger = nullptr;

// FIXED BUG #1: Complete cleanup function for all global objects
// Previously leaked memory for singleton instances (networkManager, queueManager, etc.)
void cleanup()
{
  // Stop all services first (prevent dangling references)
  if (bleManager)
  {
    bleManager->stop();
    bleManager->~BLEManager();
    heap_caps_free(bleManager);
    bleManager = nullptr;
  }

  if (mqttManager)
  {
    mqttManager->stop();
    delete mqttManager; // Singleton - uses regular delete
    mqttManager = nullptr;
  }

  if (httpManager)
  {
    httpManager->stop();
    delete httpManager; // Singleton - uses regular delete
    httpManager = nullptr;
  }

  if (modbusRtuService)
  {
    modbusRtuService->stop();
    delete modbusRtuService;
    modbusRtuService = nullptr;
  }

  if (modbusTcpService)
  {
    modbusTcpService->stop();
    delete modbusTcpService;
    modbusTcpService = nullptr;
  }

  // Clean up managers and handlers
  if (crudHandler)
  {
    crudHandler->~CRUDHandler();
    heap_caps_free(crudHandler);
    crudHandler = nullptr;
  }

  if (configManager)
  {
    configManager->~ConfigManager();
    heap_caps_free(configManager);
    configManager = nullptr;
  }

  if (serverConfig)
  {
    delete serverConfig;
    serverConfig = nullptr;
  }

  if (loggingConfig)
  {
    delete loggingConfig;
    loggingConfig = nullptr;
  }

  // Clean up singleton instances (NOTE: These use getInstance() pattern)
  // networkManager, rtcManager, queueManager, ledManager, buttonManager
  // are NOT deleted here as they are static singletons that persist
  // throughout firmware lifetime. Only set pointers to nullptr.
  networkManager = nullptr;
  rtcManager = nullptr;
  queueManager = nullptr;
  ledManager = nullptr;
  buttonManager = nullptr;
  productionLogger = nullptr;

  Serial.println("[CLEANUP] All resources released");
}

void setup()
{
  Serial.begin(115200);
  vTaskDelay(pdMS_TO_TICKS(1000));

  // ============================================
  // LOG LEVEL SYSTEM INITIALIZATION (Phase 1)
  // ============================================
  setLogLevel(LOG_INFO); // Production default: ERROR + WARN + INFO

  // ============================================
  // RTC TIMESTAMPS (Phase 3)
  // ============================================
  setLogTimestamps(true); // Enable timestamps (default: true)
  // To disable: setLogTimestamps(false);

#if PRODUCTION_MODE == 0
                          // Development mode: show log status
  Serial.println("\n[SYSTEM] DEVELOPMENT MODE - ENHANCED LOGGING");
  printLogLevelStatus();
#endif

  // ============================================
  // MEMORY RECOVERY INITIALIZATION (Phase 2)
  // ============================================
  MemoryRecovery::setAutoRecovery(true);      // Enable automatic recovery
  MemoryRecovery::setCheckInterval(5000);     // Check every 5 seconds
  MemoryRecovery::logMemoryStatus("STARTUP"); // Log initial memory state

#if PRODUCTION_MODE == 0
  Serial.println("[SYSTEM] MEMORY RECOVERY SYSTEM");
  Serial.println("  Auto-recovery: ENABLED");
  Serial.println("  Check interval: 5 seconds");
  Serial.println("  Thresholds:");
  Serial.printf("    WARNING: %lu bytes\n", MemoryThresholds::DRAM_WARNING);
  Serial.printf("    CRITICAL: %lu bytes\n", MemoryThresholds::DRAM_CRITICAL);
  Serial.printf("    EMERGENCY: %lu bytes\n\n", MemoryThresholds::DRAM_EMERGENCY);
#endif

  // FIXED BUG #2: Serial.end() moved to END of setup() to avoid crash
  // Previously called here, causing all subsequent Serial.printf() to crash

  uint32_t seed = esp_random();
  randomSeed(seed); // Seed the random number generator for unique IDs
  Serial.printf("[SYSTEM] Random seed initialized: %u\n", seed);

  // Check PSRAM availability
  if (esp_psram_is_initialized())
  {
    Serial.printf("[SYSTEM] PSRAM available: %d bytes | Free: %d bytes\n",
                  ESP.getPsramSize(), ESP.getFreePsram());
  }
  else
  {
    Serial.println("[SYSTEM] PSRAM not available - using internal RAM");
  }

  Serial.println("[MAIN] Starting BLE CRUD Manager...");

  // Initialize configuration manager in PSRAM
  configManager = (ConfigManager *)heap_caps_malloc(sizeof(ConfigManager), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (configManager)
  {
    new (configManager) ConfigManager();
  }
  else
  {
    configManager = new ConfigManager(); // Fallback to internal RAM
    if (!configManager)
    {
      Serial.println("[MAIN] ERROR: Failed to allocate ConfigManager");
      return;
    }
  }

  if (!configManager->begin())
  {
    Serial.println("[MAIN] ERROR: Failed to initialize ConfigManager");
    cleanup();
    return;
  }

  // Debug: Show devices file content
  configManager->debugDevicesFile();

  // Pre-load the cache once to prevent race conditions from other tasks
  Serial.println("[MAIN] Forcing initial cache load...");
  configManager->refreshCache();

  // FIXED: Removed automatic config clear - only clear manually when needed
  // configManager->clearAllConfigurations();  // DISABLED - Uncomment only for factory reset

  Serial.println("[MAIN] Configuration initialization completed.");

  // Initialize LED Manager
  ledManager = LEDManager::getInstance();
  if (ledManager)
  {
    ledManager->begin();
  }
  else
  {
    Serial.println("[MAIN] ERROR: Failed to get LEDManager instance");
    cleanup();
    return;
  }

  // Initialize queue manager
  queueManager = QueueManager::getInstance();
  if (!queueManager || !queueManager->init())
  {
    Serial.println("[MAIN] ERROR: Failed to initialize QueueManager");
    cleanup();
    return;
  }

  // Initialize server config
  serverConfig = new ServerConfig();
  if (!serverConfig || !serverConfig->begin())
  {
    Serial.println("[MAIN] ERROR: Failed to initialize ServerConfig");
    cleanup();
    return;
  }

  // Initialize logging config
  loggingConfig = new LoggingConfig();
  if (!loggingConfig || !loggingConfig->begin())
  {
    Serial.println("[MAIN] ERROR: Failed to initialize LoggingConfig");
    cleanup();
    return;
  }

  // FIXED BUG #11: Proper handling of network init failure
  // Previous code continued without network, causing MQTT/HTTP failures
  bool networkInitialized = false;

  networkManager = NetworkMgr::getInstance();
  if (!networkManager)
  {
    Serial.println("[MAIN] ERROR: Failed to get NetworkManager instance");
    cleanup();
    return;
  }

  if (!networkManager->init(serverConfig))
  {
    Serial.println("[MAIN] WARNING: NetworkManager init failed - network services will be disabled");
    Serial.println("[MAIN] System will continue in offline mode (BLE/Modbus RTU only)");
    networkInitialized = false;
  }
  else
  {
    Serial.println("[MAIN] NetworkManager initialized successfully");
    networkInitialized = true;
  }

  // Initialize RTC manager
  rtcManager = RTCManager::getInstance();
  if (!rtcManager || !rtcManager->init())
  {
    Serial.println("[MAIN] WARNING: Failed to initialize RTCManager");
  }
  else
  {
    rtcManager->startSync();
  }

  // Initialize Modbus TCP service (watchdog-safe implementation)
  EthernetManager *ethernetMgr = EthernetManager::getInstance();
  if (ethernetMgr)
  {
    modbusTcpService = new ModbusTcpService(configManager, ethernetMgr);
    if (modbusTcpService && modbusTcpService->init())
    {
      modbusTcpService->start();
      Serial.println("[MAIN] Modbus TCP service started");
    }
    else
    {
      Serial.println("[MAIN] ERROR: Failed to initialize Modbus TCP service");
    }
  }
  else
  {
    Serial.println("[MAIN] WARNING: EthernetManager not available for Modbus TCP");
  }
  Serial.println("[MAIN] Modbus TCP service initialized (TCP/IP polling enabled)");

  // Initialize CRUD handler in PSRAM FIRST (before Modbus services)
  crudHandler = (CRUDHandler *)heap_caps_malloc(sizeof(CRUDHandler), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (crudHandler)
  {
    new (crudHandler) CRUDHandler(configManager, serverConfig, loggingConfig);
  }
  else
  {
    crudHandler = new CRUDHandler(configManager, serverConfig, loggingConfig); // Fallback to internal RAM
    if (!crudHandler)
    {
      Serial.println("[MAIN] ERROR: Failed to allocate CRUDHandler");
      cleanup();
      return;
    }
  }
  Serial.println("[MAIN] CRUDHandler initialized");

  // Link Modbus TCP Service to CRUD Handler for device control commands
  if (crudHandler && modbusTcpService)
  {
    crudHandler->setModbusTcpService(modbusTcpService);
    Serial.println("[MAIN] Modbus TCP Service linked to CRUD Handler");
  }

  // Initialize Modbus RTU service (after CRUDHandler)
  modbusRtuService = new ModbusRtuService(configManager);
  if (modbusRtuService && modbusRtuService->init())
  {
    modbusRtuService->start();
    Serial.println("[MAIN] Modbus RTU service started");

    // Link Modbus RTU Service to CRUD Handler for device control commands
    if (crudHandler)
    {
      crudHandler->setModbusRtuService(modbusRtuService);
      Serial.println("[MAIN] Modbus RTU Service linked to CRUD Handler");
    }
  }
  else
  {
    Serial.println("[MAIN] ERROR: Failed to initialize Modbus RTU service");
  }

  // Initialize protocol managers based on server configuration
  String protocol = serverConfig->getProtocol();
  Serial.printf("[MAIN] Selected protocol: %s\n", protocol.c_str());

  // FIXED BUG #11: Initialize MQTT only if network is available
  if (networkInitialized)
  {
    mqttManager = MqttManager::getInstance(configManager, serverConfig, networkManager);
    if (mqttManager && mqttManager->init())
    {
      // Link MQTT Manager to CRUD Handler for config change notifications
      if (crudHandler)
      {
        crudHandler->setMqttManager(mqttManager);
        Serial.println("[MAIN] MQTT Manager linked to CRUD Handler");
      }

      if (protocol == "mqtt")
      {
        mqttManager->start();
        Serial.println("[MAIN] MQTT Manager started (active protocol)");
      }
      else
      {
        Serial.println("[MAIN] MQTT Manager initialized but not started (inactive protocol)");
      }
    }
    else
    {
      Serial.println("[MAIN] ERROR: Failed to initialize MQTT Manager");
    }
  }
  else
  {
    Serial.println("[MAIN] Skipping MQTT Manager - network not initialized");
  }

  // FIXED BUG #11: Initialize HTTP only if network is available
  if (networkInitialized)
  {
    httpManager = HttpManager::getInstance(configManager, serverConfig, networkManager);
    if (httpManager && httpManager->init())
    {
      // Link HTTP Manager to CRUD Handler for config change notifications
      if (crudHandler)
      {
        crudHandler->setHttpManager(httpManager);
        Serial.println("[MAIN] HTTP Manager linked to CRUD Handler");
      }

      if (protocol == "http")
      {
        httpManager->start();
        Serial.println("[MAIN] HTTP Manager started (active protocol)");
      }
      else
      {
        Serial.println("[MAIN] HTTP Manager initialized but not started (inactive protocol)");
      }
    }
    else
    {
      Serial.println("[MAIN] ERROR: Failed to initialize HTTP Manager");
    }
  }
  else
  {
    Serial.println("[MAIN] Skipping HTTP Manager - network not initialized");
  }

  // CRUDHandler already initialized above

  // Initialize BLE manager in PSRAM (but don't start yet if production mode)
  bleManager = (BLEManager *)heap_caps_malloc(sizeof(BLEManager), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (bleManager)
  {
    new (bleManager) BLEManager("SURIOTA GW", crudHandler);
  }
  else
  {
    bleManager = new BLEManager("SURIOTA GW", crudHandler); // Fallback to internal RAM
    if (!bleManager)
    {
      Serial.println("[MAIN] ERROR: Failed to allocate BLE Manager");
      cleanup();
      return;
    }
  }

  // Initialize Button Manager
  buttonManager = ButtonManager::getInstance();
  if (buttonManager)
  {
    buttonManager->begin(PRODUCTION_MODE == 1); // Pass production mode flag
    buttonManager->setBLEManager(bleManager);   // Set BLE manager reference
  }
  else
  {
    Serial.println("[MAIN] ERROR: Failed to initialize Button Manager");
    cleanup();
    return;
  }

  // Start BLE based on mode
  // Development mode (PRODUCTION_MODE == 0): BLE always ON
  // Production mode (PRODUCTION_MODE == 1): BLE starts OFF, controlled by button
  if (PRODUCTION_MODE == 0)
  {
    // Development mode: start BLE immediately
    if (!bleManager->begin())
    {
      Serial.println("[MAIN] ERROR: Failed to initialize BLE Manager");
      cleanup();
      return;
    }
    Serial.println("[MAIN] BLE started (Development mode)");
  }
  else
  {
    // Production mode: BLE controlled by button (starts OFF)
    Serial.println("[MAIN] BLE controlled by button (Production mode)");
  }

  Serial.println("[MAIN] BLE CRUD Manager started successfully");

  // ============================================
  // PRODUCTION LOGGER INITIALIZATION
  // ============================================
#if PRODUCTION_MODE == 1
  // Production mode: Initialize minimal production logger
  // This replaces the previous Serial.end() approach to maintain visibility
  productionLogger = ProductionLogger::getInstance();
  if (productionLogger)
  {
    productionLogger->begin(FIRMWARE_VERSION, DEVICE_ID);
    productionLogger->setHeartbeatInterval(60000);  // Heartbeat every 60 seconds
    productionLogger->setJsonFormat(true);          // Use JSON format for parsing
    productionLogger->setActiveProtocol(serverConfig->getProtocol());

    // Set initial network status
    if (networkInitialized)
    {
      // Check which network is active (simplified check)
      productionLogger->setNetworkStatus(NetStatus::ETHERNET); // Default to ETH
    }

    // Set protocol status
    if (mqttManager && serverConfig->getProtocol() == "mqtt")
    {
      productionLogger->setMqttStatus(ProtoStatus::CONNECTING);
    }
    else if (httpManager && serverConfig->getProtocol() == "http")
    {
      productionLogger->setHttpStatus(ProtoStatus::CONNECTING);
    }

    // Log boot message
    productionLogger->logBoot();
    Serial.println("[MAIN] Production Logger initialized - minimal logging active");
  }
#else
  // Development mode: Full logging already active
  Serial.println("\n[MAIN] === SYSTEM READY (Development Mode) ===\n");
#endif
}

void loop()
{
  // FreeRTOS-friendly delay - yields to other tasks
  vTaskDelay(pdMS_TO_TICKS(100));

#if PRODUCTION_MODE == 1
  // Production mode: Periodic heartbeat logging
  // This is automatically throttled by ProductionLogger (default: every 60s)
  if (productionLogger)
  {
    productionLogger->heartbeat();
  }
#endif

  // Optional: Add watchdog feed or system monitoring here
  // esp_task_wdt_reset();
}