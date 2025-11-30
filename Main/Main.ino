/*
 * BLE CRUD Manager for ESP32
 * Production-ready BLE service for Modbus device configuration
 *
 * Hardware: ESP32 with BLE support
 * Features: CRUD operations, fragmentation, FreeRTOS tasks
 * Optimization: 9 firmware optimizations integrated
 * Debug: Development vs Production mode (PRODUCTION_MODE defined in DebugConfig.h)
 *
 * MQTT Fix: Payload corruption fix with separate buffers (2025-11-22)
 * Task Optimization: Low-priority tasks moved to Core 0 for load balancing (2025-11-22)
 */

// PRODUCTION_MODE is now defined in DebugConfig.h (0 = Development, 1 = Production)

// CRITICAL: Override MQTT_MAX_PACKET_SIZE BEFORE any library includes
// This MUST be defined here (not in MqttManager.h) to ensure Arduino IDE
// compiles TBPubSubClient library with the correct packet size limit
#ifndef MQTT_MAX_PACKET_SIZE
#define MQTT_MAX_PACKET_SIZE 16384 // 16KB - allows large payloads without truncation
#endif

#include <LittleFS.h>          // ← Early include for config loading
#include "DebugConfig.h"       // ← MUST BE FIRST (before all other includes)

// Global runtime production mode (switchable via BLE)
// Initialized from compile-time PRODUCTION_MODE, can be changed at runtime
uint8_t g_productionMode = PRODUCTION_MODE;
#include "MemoryRecovery.h"    // Phase 2 optimization
#include "JsonDocumentPSRAM.h" // BUG #31: Global PSRAM allocator for ALL JsonDocument instances
#include "ProductionLogger.h"  // Production mode minimal logging

// Firmware version and device identification
#define FIRMWARE_VERSION "2.5.17" // v2.5.17: BLE Pagination support (page/limit parameters)
#define DEVICE_ID "SRT-MGATE-1210"

// Smart Serial wrapper - runtime mode checking (supports mode switching via BLE)
// These macros now check g_productionMode at runtime instead of compile-time
#define DEV_SERIAL_PRINT(...) do { if (IS_DEV_MODE()) Serial.print(__VA_ARGS__); } while(0)
#define DEV_SERIAL_PRINTF(...) do { if (IS_DEV_MODE()) Serial.printf(__VA_ARGS__); } while(0)
#define DEV_SERIAL_PRINTLN(...) do { if (IS_DEV_MODE()) Serial.println(__VA_ARGS__); } while(0)

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
#include "OTAManager.h"
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
OTAManager *otaManager = nullptr;

// v2.5.1 FIX: Track allocation method for safe cleanup (PSRAM placement new vs standard new)
// If true: allocated with heap_caps_malloc + placement new (use ~destructor + heap_caps_free)
// If false: allocated with standard new (use delete)
static bool bleManagerInPsram = false;
static bool crudHandlerInPsram = false;
static bool configManagerInPsram = false;

// FIXED BUG #1: Complete cleanup function for all global objects
// v2.5.1 FIX: Safe cleanup that handles both PSRAM (placement new) and DRAM (standard new) allocations
void cleanup()
{
  // Stop all services first (prevent dangling references)
  if (bleManager)
  {
    bleManager->stop();
    // v2.5.1 FIX: Use correct deallocation based on allocation method
    if (bleManagerInPsram)
    {
      bleManager->~BLEManager();
      heap_caps_free(bleManager);
    }
    else
    {
      delete bleManager;
    }
    bleManager = nullptr;
    bleManagerInPsram = false;
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
    // v2.5.1 FIX: Use correct deallocation based on allocation method
    if (crudHandlerInPsram)
    {
      crudHandler->~CRUDHandler();
      heap_caps_free(crudHandler);
    }
    else
    {
      delete crudHandler;
    }
    crudHandler = nullptr;
    crudHandlerInPsram = false;
  }

  if (configManager)
  {
    // v2.5.1 FIX: Use correct deallocation based on allocation method
    if (configManagerInPsram)
    {
      configManager->~ConfigManager();
      heap_caps_free(configManager);
    }
    else
    {
      delete configManager;
    }
    configManager = nullptr;
    configManagerInPsram = false;
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
  otaManager = nullptr;

  DEV_SERIAL_PRINTLN("[CLEANUP] All resources released");
}

void setup()
{
  Serial.begin(115200);
  vTaskDelay(pdMS_TO_TICKS(1000));

  // ============================================
  // CRITICAL: LOAD PRODUCTION MODE FROM CONFIG FIRST
  // ============================================
  // Initialize LittleFS for config file access
  if (!LittleFS.begin(true))
  {
    Serial.println("[SYSTEM] WARNING: LittleFS mount failed, using compile-time defaults");
  }

  // Initialize logging config EARLY to load production mode
  loggingConfig = new LoggingConfig();
  if (loggingConfig)
  {
    loggingConfig->begin(); // This will load saved config if available

    // Load production mode from config (allows runtime switching via BLE)
    uint8_t savedMode = loggingConfig->getProductionMode();
    if (savedMode <= 1 && savedMode != g_productionMode)
    {
      g_productionMode = savedMode;
      Serial.printf("[SYSTEM] Production mode loaded from config: %d (%s)\n",
                    g_productionMode, (g_productionMode == 0) ? "Development" : "Production");
    }
  }
  else
  {
    Serial.println("[SYSTEM] WARNING: LoggingConfig init failed, using compile-time defaults");
  }

  // ============================================
  // LOG LEVEL SYSTEM INITIALIZATION (Phase 1) - Runtime check
  // ============================================
  if (IS_PRODUCTION_MODE())
  {
    setLogLevel(LOG_ERROR); // Production: ONLY critical errors (suppress WARN/INFO)
  }
  else
  {
    setLogLevel(LOG_INFO); // Development: ERROR + WARN + INFO
  }

  // ============================================
  // RTC TIMESTAMPS (Phase 3)
  // ============================================
  setLogTimestamps(true); // Enable timestamps (default: true)
  // To disable: setLogTimestamps(false);

  // Development mode: show log status (runtime check)
  if (IS_DEV_MODE())
  {
    Serial.println("\n[SYSTEM] DEVELOPMENT MODE - ENHANCED LOGGING");
    printLogLevelStatus();
  }

  // ============================================
  // MEMORY RECOVERY INITIALIZATION (Phase 2)
  // ============================================
  MemoryRecovery::setAutoRecovery(true);      // Enable automatic recovery
  MemoryRecovery::setCheckInterval(5000);     // Check every 5 seconds
  MemoryRecovery::logMemoryStatus("STARTUP"); // Log initial memory state

  // Development mode: show memory recovery details (runtime check)
  if (IS_DEV_MODE())
  {
    Serial.println("[SYSTEM] MEMORY RECOVERY SYSTEM");
    Serial.println("  Auto-recovery: ENABLED");
    Serial.println("  Check interval: 5 seconds");
    Serial.println("  Thresholds:");
    Serial.printf("    WARNING: %lu bytes\n", MemoryThresholds::DRAM_WARNING);
    Serial.printf("    CRITICAL: %lu bytes\n", MemoryThresholds::DRAM_CRITICAL);
    Serial.printf("    EMERGENCY: %lu bytes\n\n", MemoryThresholds::DRAM_EMERGENCY);
  }

  // FIXED BUG #2: Serial.end() moved to END of setup() to avoid crash
  // Previously called here, causing all subsequent Serial.printf() to crash

  uint32_t seed = esp_random();
  randomSeed(seed); // Seed the random number generator for unique IDs
  DEV_SERIAL_PRINTF("[SYSTEM] Random seed initialized: %u\n", seed);

  // Check PSRAM availability
  if (esp_psram_is_initialized())
  {
    DEV_SERIAL_PRINTF("[SYSTEM] PSRAM available: %d bytes | Free: %d bytes\n",
                  ESP.getPsramSize(), ESP.getFreePsram());
  }
  else
  {
    DEV_SERIAL_PRINTLN("[SYSTEM] PSRAM not available - using internal RAM");
  }

  DEV_SERIAL_PRINTLN("[MAIN] Starting BLE CRUD Manager...");

  // Initialize configuration manager in PSRAM
  configManager = (ConfigManager *)heap_caps_malloc(sizeof(ConfigManager), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (configManager)
  {
    new (configManager) ConfigManager();
    configManagerInPsram = true;  // v2.5.1 FIX: Track allocation method
  }
  else
  {
    configManager = new ConfigManager(); // Fallback to internal RAM
    configManagerInPsram = false;  // v2.5.1 FIX: Track allocation method
    if (!configManager)
    {
      DEV_SERIAL_PRINTLN("[MAIN] ERROR: Failed to allocate ConfigManager");
      return;
    }
  }

  if (!configManager->begin())
  {
    DEV_SERIAL_PRINTLN("[MAIN] ERROR: Failed to initialize ConfigManager");
    cleanup();
    return;
  }

  // Debug: Show devices file content (runtime check)
  if (IS_DEV_MODE())
  {
    configManager->debugDevicesFile();
  }

  // Pre-load the cache once to prevent race conditions from other tasks
  DEV_SERIAL_PRINTLN("[MAIN] Forcing initial cache load...");
  configManager->refreshCache();

  // FIXED: Removed automatic config clear - only clear manually when needed
  // configManager->clearAllConfigurations();  // DISABLED - Uncomment only for factory reset

  DEV_SERIAL_PRINTLN("[MAIN] Configuration initialization completed.");

  // Initialize LED Manager
  ledManager = LEDManager::getInstance();
  if (ledManager)
  {
    ledManager->begin();
  }
  else
  {
    DEV_SERIAL_PRINTLN("[MAIN] ERROR: Failed to get LEDManager instance");
    cleanup();
    return;
  }

  // Initialize queue manager
  queueManager = QueueManager::getInstance();
  if (!queueManager || !queueManager->init())
  {
    DEV_SERIAL_PRINTLN("[MAIN] ERROR: Failed to initialize QueueManager");
    cleanup();
    return;
  }

  // Initialize server config
  serverConfig = new ServerConfig();
  if (!serverConfig || !serverConfig->begin())
  {
    DEV_SERIAL_PRINTLN("[MAIN] ERROR: Failed to initialize ServerConfig");
    cleanup();
    return;
  }

  // NOTE: LoggingConfig already initialized at start of setup() to load production mode early
  // No need to re-initialize here

  // FIXED BUG #11: Proper handling of network init failure
  // Previous code continued without network, causing MQTT/HTTP failures
  bool networkInitialized = false;

  networkManager = NetworkMgr::getInstance();
  if (!networkManager)
  {
    DEV_SERIAL_PRINTLN("[MAIN] ERROR: Failed to get NetworkManager instance");
    cleanup();
    return;
  }

  if (!networkManager->init(serverConfig))
  {
    DEV_SERIAL_PRINTLN("[MAIN] WARNING: NetworkManager init failed - network services will be disabled");
    DEV_SERIAL_PRINTLN("[MAIN] System will continue in offline mode (BLE/Modbus RTU only)");
    networkInitialized = false;
  }
  else
  {
    DEV_SERIAL_PRINTLN("[MAIN] NetworkManager initialized successfully");
    networkInitialized = true;
  }

  // Initialize RTC manager
  rtcManager = RTCManager::getInstance();
  if (!rtcManager || !rtcManager->init())
  {
    DEV_SERIAL_PRINTLN("[MAIN] WARNING: Failed to initialize RTCManager");
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
      DEV_SERIAL_PRINTLN("[MAIN] Modbus TCP service started");
    }
    else
    {
      DEV_SERIAL_PRINTLN("[MAIN] ERROR: Failed to initialize Modbus TCP service");
    }
  }
  else
  {
    DEV_SERIAL_PRINTLN("[MAIN] WARNING: EthernetManager not available for Modbus TCP");
  }
  DEV_SERIAL_PRINTLN("[MAIN] Modbus TCP service initialized (TCP/IP polling enabled)");

  // Initialize CRUD handler in PSRAM FIRST (before Modbus services)
  crudHandler = (CRUDHandler *)heap_caps_malloc(sizeof(CRUDHandler), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (crudHandler)
  {
    new (crudHandler) CRUDHandler(configManager, serverConfig, loggingConfig);
    crudHandlerInPsram = true;  // v2.5.1 FIX: Track allocation method
  }
  else
  {
    crudHandler = new CRUDHandler(configManager, serverConfig, loggingConfig); // Fallback to internal RAM
    crudHandlerInPsram = false;  // v2.5.1 FIX: Track allocation method
    if (!crudHandler)
    {
      DEV_SERIAL_PRINTLN("[MAIN] ERROR: Failed to allocate CRUDHandler");
      cleanup();
      return;
    }
  }
  DEV_SERIAL_PRINTLN("[MAIN] CRUDHandler initialized");

  // Link Modbus TCP Service to CRUD Handler for device control commands
  if (crudHandler && modbusTcpService)
  {
    crudHandler->setModbusTcpService(modbusTcpService);
    DEV_SERIAL_PRINTLN("[MAIN] Modbus TCP Service linked to CRUD Handler");
  }

  // Initialize Modbus RTU service (after CRUDHandler)
  modbusRtuService = new ModbusRtuService(configManager);
  if (modbusRtuService && modbusRtuService->init())
  {
    modbusRtuService->start();
    DEV_SERIAL_PRINTLN("[MAIN] Modbus RTU service started");

    // Link Modbus RTU Service to CRUD Handler for device control commands
    if (crudHandler)
    {
      crudHandler->setModbusRtuService(modbusRtuService);
      DEV_SERIAL_PRINTLN("[MAIN] Modbus RTU Service linked to CRUD Handler");
    }
  }
  else
  {
    DEV_SERIAL_PRINTLN("[MAIN] ERROR: Failed to initialize Modbus RTU service");
  }

  // Initialize protocol managers based on server configuration
  String protocol = serverConfig->getProtocol();
  DEV_SERIAL_PRINTF("[MAIN] Selected protocol: %s\n", protocol.c_str());

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
        DEV_SERIAL_PRINTLN("[MAIN] MQTT Manager linked to CRUD Handler");
      }

      if (protocol == "mqtt")
      {
        mqttManager->start();
        DEV_SERIAL_PRINTLN("[MAIN] MQTT Manager started (active protocol)");
      }
      else
      {
        DEV_SERIAL_PRINTLN("[MAIN] MQTT Manager initialized but not started (inactive protocol)");
      }
    }
    else
    {
      DEV_SERIAL_PRINTLN("[MAIN] ERROR: Failed to initialize MQTT Manager");
    }
  }
  else
  {
    DEV_SERIAL_PRINTLN("[MAIN] Skipping MQTT Manager - network not initialized");
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
        DEV_SERIAL_PRINTLN("[MAIN] HTTP Manager linked to CRUD Handler");
      }

      if (protocol == "http")
      {
        httpManager->start();
        DEV_SERIAL_PRINTLN("[MAIN] HTTP Manager started (active protocol)");
      }
      else
      {
        DEV_SERIAL_PRINTLN("[MAIN] HTTP Manager initialized but not started (inactive protocol)");
      }
    }
    else
    {
      DEV_SERIAL_PRINTLN("[MAIN] ERROR: Failed to initialize HTTP Manager");
    }
  }
  else
  {
    DEV_SERIAL_PRINTLN("[MAIN] Skipping HTTP Manager - network not initialized");
  }

  // CRUDHandler already initialized above

  // Initialize BLE manager in PSRAM (but don't start yet if production mode)
  bleManager = (BLEManager *)heap_caps_malloc(sizeof(BLEManager), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (bleManager)
  {
    new (bleManager) BLEManager("SURIOTA GW", crudHandler);
    bleManagerInPsram = true;  // v2.5.1 FIX: Track allocation method
  }
  else
  {
    bleManager = new BLEManager("SURIOTA GW", crudHandler); // Fallback to internal RAM
    bleManagerInPsram = false;  // v2.5.1 FIX: Track allocation method
    if (!bleManager)
    {
      DEV_SERIAL_PRINTLN("[MAIN] ERROR: Failed to allocate BLE Manager");
      cleanup();
      return;
    }
  }

  // Initialize Button Manager
  buttonManager = ButtonManager::getInstance();
  if (buttonManager)
  {
    // v2.5.1 FIX: Use runtime production mode from config, not compile-time constant
    // This allows button to work correctly when production mode is set via BLE
    buttonManager->begin(IS_PRODUCTION_MODE()); // Use runtime flag from config
    buttonManager->setBLEManager(bleManager);   // Set BLE manager reference
  }
  else
  {
    DEV_SERIAL_PRINTLN("[MAIN] ERROR: Failed to initialize Button Manager");
    cleanup();
    return;
  }

  // Start BLE based on mode (runtime check - supports mode switching)
  // Development mode: BLE always ON
  // Production mode: BLE starts OFF, controlled by button
  if (IS_DEV_MODE())
  {
    // Development mode: start BLE immediately
    if (!bleManager->begin())
    {
      DEV_SERIAL_PRINTLN("[MAIN] ERROR: Failed to initialize BLE Manager");
      cleanup();
      return;
    }
    DEV_SERIAL_PRINTLN("[MAIN] BLE started (Development mode)");
  }
  else
  {
    // Production mode: BLE controlled by button (starts OFF)
    DEV_SERIAL_PRINTLN("[MAIN] BLE controlled by button (Production mode)");
  }

  DEV_SERIAL_PRINTLN("[MAIN] BLE CRUD Manager started successfully");

  // ============================================
  // OTA MANAGER INITIALIZATION
  // ============================================
  otaManager = OTAManager::getInstance();
  if (otaManager)
  {
    // Set current firmware version
    otaManager->setCurrentVersion(FIRMWARE_VERSION, 2517); // Build number derived from version (2.5.17 = 2517)

    // Get BLE server from BLEManager for OTA BLE service
    // Note: BLE server is internal to BLEManager, OTA BLE will create its own service
    if (otaManager->begin(nullptr))  // Pass BLE server if needed for BLE OTA
    {
      DEV_SERIAL_PRINTLN("[MAIN] OTA Manager initialized");

      // Link OTA Manager to CRUD Handler for BLE commands
      if (crudHandler)
      {
        crudHandler->setOTAManager(otaManager);
        DEV_SERIAL_PRINTLN("[MAIN] OTA Manager linked to CRUD Handler");
      }

      // Mark firmware as valid after successful boot (rollback protection)
      // This should be called after all critical services are running
      otaManager->markFirmwareValid();
      DEV_SERIAL_PRINTLN("[MAIN] Firmware marked as valid (rollback protection)");
    }
    else
    {
      DEV_SERIAL_PRINTLN("[MAIN] WARNING: OTA Manager init failed - OTA updates disabled");
    }
  }
  else
  {
    DEV_SERIAL_PRINTLN("[MAIN] WARNING: Failed to get OTA Manager instance");
  }

  // ============================================
  // PRODUCTION LOGGER INITIALIZATION (Always init, enable based on mode)
  // ============================================
  // Always initialize ProductionLogger (supports runtime mode switching)
  productionLogger = ProductionLogger::getInstance();
  if (productionLogger)
  {
    productionLogger->begin(FIRMWARE_VERSION, DEVICE_ID);
    productionLogger->setHeartbeatInterval(60000);  // Heartbeat every 60 seconds
    productionLogger->setJsonFormat(true);          // Use JSON format for parsing
    productionLogger->setActiveProtocol(serverConfig->getProtocol());
    productionLogger->setEnabled(IS_PRODUCTION_MODE()); // Enable only in production mode

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

    DEV_SERIAL_PRINTLN("[MAIN] Production Logger initialized");
  }

  // System ready message based on mode
  if (IS_DEV_MODE())
  {
    Serial.println("\n[MAIN] === SYSTEM READY (Development Mode) ===\n");
  }
  else if (productionLogger)
  {
    productionLogger->logBoot(); // Log boot in production mode
  }
}

void loop()
{
  // FreeRTOS-friendly delay - yields to other tasks
  vTaskDelay(pdMS_TO_TICKS(100));

  // OTA Manager process - handles auto-check intervals and update state machine
  if (otaManager)
  {
    otaManager->process();
  }

  // Production mode: Periodic heartbeat logging (runtime check)
  // This is automatically throttled by ProductionLogger (default: every 60s)
  if (IS_PRODUCTION_MODE() && productionLogger)
  {
    productionLogger->heartbeat();
  }

  // Optional: Add watchdog feed or system monitoring here
  // esp_task_wdt_reset();
}