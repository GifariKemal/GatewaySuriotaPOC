#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include "JsonDocumentPSRAM.h" // BUG #31: MUST BE BEFORE ArduinoJson.h
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

// BLE UUIDs
#define SERVICE_UUID "00001830-0000-1000-8000-00805f9b34fb"
#define COMMAND_CHAR_UUID "11111111-1111-1111-1111-111111111101"
#define RESPONSE_CHAR_UUID "11111111-1111-1111-1111-111111111102"

// Constants
// BLE Transmission Optimization (v2.1.1 - Critical timeout fix)
// CHUNK_SIZE increased from 18 to 244 bytes (MTU-safe for 512-byte MTU)
// FRAGMENT_DELAY reduced from 50ms to 10ms for faster transmission
// Impact: 21KB payload transmission time: 58s → 2.1s (28x faster)
#define CHUNK_SIZE 244
#define FRAGMENT_DELAY_MS 10
#define COMMAND_BUFFER_SIZE 16384        // 16KB - supports CREATE/UPDATE with ~80 registers (v2.3.1)
#define BLE_QUEUE_MONITOR_INTERVAL 60000 // Monitor queue every 60 seconds

// FIXED BUG #21: Define named constants for magic numbers
#define BLE_MTU_SAFE_DEFAULT 247       // Conservative MTU for all devices (iOS compatible)
#define BLE_MTU_MAX_REQUESTED 517      // Maximum MTU we request (not all clients support)
#define BLE_MTU_MAX_SUPPORTED 512      // Maximum MTU we actually support
#define MAX_RESPONSE_SIZE_BYTES 204800 // 200KB maximum response size (for full_config backup with 50+ devices)

// ============================================
// ADAPTIVE TRANSMISSION TUNING (v2.3.4 - Option 3)
// ============================================
// Problem: Intermittent BLE timeout on 9.4KB payloads (45 registers)
// Root Cause: 20ms delay too aggressive for mobile OS (iOS/Android scheduler ~30-50ms quantum)
//
// Solution: Option 3 - Conservative timing for maximum stability
//   - Lower LARGE threshold: 5KB → 3KB (catch more medium payloads)
//   - Increase LARGE delay: 20ms → 35ms (match mobile OS timing)
//   - Increase XLARGE delay: 50ms → 60ms (backup reliability)
//
// Impact:
//   - Small payloads (<3KB): UNCHANGED (10ms, fast)
//   - Medium payloads (3-9KB): +75% transmission time, 95%+ success rate
//   - Large payloads (9-50KB): More stable, no timeout
//   - XLarge backups (>50KB): +20% transmission time, within 60s limit
//
// Trade-off: Speed vs Stability - prioritize NO TIMEOUT over fast transmission
// ============================================
#define LARGE_PAYLOAD_THRESHOLD 3072   // 3KB threshold for adaptive chunking (medium payloads) - v2.3.4 Option 3: Lowered from 5KB for better stability
#define XLARGE_PAYLOAD_THRESHOLD 51200 // 50KB threshold for extra-large payloads (backup/restore)
#define ADAPTIVE_CHUNK_SIZE_LARGE 100  // Chunk size for large payloads
#define ADAPTIVE_DELAY_LARGE_MS 35     // Delay for large payload chunks (3-50KB) - v2.3.4 Option 3: Increased from 20ms for mobile OS compatibility
#define ADAPTIVE_DELAY_XLARGE_MS 60    // Delay for extra-large payload chunks (>50KB) - v2.3.4 Option 3: Increased from 50ms for backup reliability
#define ERROR_BUFFER_SIZE 256          // Buffer size for error messages

// MTU Negotiation Timeout Control
enum MTUNegotiationState
{
  MTU_STATE_IDLE = 0,        // No negotiation in progress
  MTU_STATE_INITIATING = 1,  // Negotiation just started
  MTU_STATE_IN_PROGRESS = 2, // Waiting for MTU exchange
  MTU_STATE_COMPLETED = 3,   // Successfully negotiated
  MTU_STATE_TIMEOUT = 4,     // Negotiation timed out
  MTU_STATE_FAILED = 5       // Negotiation failed after retries
};

// MTU Negotiation Control Structure
struct MTUNegotiationControl
{
  MTUNegotiationState state = MTU_STATE_IDLE;
  unsigned long negotiationStartTime = 0;
  unsigned long negotiationTimeoutMs = 5000; // Phase 4: 5-second timeout (optimized from 3s)
  uint8_t retryCount = 0;
  uint8_t maxRetries = 2;     // Phase 4: 2 retries (optimized from 3, total ~15s)
  uint16_t fallbackMTU = 100; // Fallback MTU size (min safe value)
  bool usesFallback = false;
};

// MTU Metrics Structure
struct MTUMetrics
{
  uint16_t mtuSize = 23; // Default BLE MTU
  uint16_t maxMTUSize = 23;
  unsigned long lastNegotiationTime = 0;
  bool mtuNegotiated = false;
  uint8_t negotiationAttempts = 0;

  // Extended metrics for timeout tracking
  uint8_t timeoutCount = 0;
  uint8_t failureCount = 0;
  unsigned long lastTimeoutTime = 0;
};

// Queue Metrics Structure
struct QueueMetrics
{
  uint32_t currentDepth = 0;
  uint32_t maxDepth = 0;
  uint32_t peakDepth = 0;
  uint32_t totalEnqueued = 0;
  uint32_t totalDequeued = 0;
  unsigned long lastMonitorTime = 0;
  double utilizationPercent = 0.0;
  uint32_t dropCount = 0;
};

// Connection Metrics Structure
struct ConnectionMetrics
{
  unsigned long connectionStartTime = 0;
  unsigned long totalConnectionTime = 0;
  uint32_t fragmentsSent = 0;
  uint32_t fragmentsReceived = 0;
  uint32_t bytesTransmitted = 0;
  uint32_t bytesReceived = 0;
  bool isConnected = false;
};

class CRUDHandler; // Forward declaration

class BLEManager : public BLEServerCallbacks, public BLECharacteristicCallbacks
{
private:
  BLEServer *pServer;
  BLEService *pService;
  BLECharacteristic *pCommandChar;
  BLECharacteristic *pResponseChar;

  String serviceName;
  CRUDHandler *handler;

  // Command processing
  char commandBuffer[COMMAND_BUFFER_SIZE];
  size_t commandBufferIndex;
  bool processing;
  unsigned long lastFragmentTime; // CRITICAL FIX: Track last fragment reception time for timeout detection
  QueueHandle_t commandQueue;
  TaskHandle_t commandTaskHandle;
  TaskHandle_t streamTaskHandle;
  TaskHandle_t metricsTaskHandle; // Metrics monitoring task

  // BLE Metrics
  MTUMetrics mtuMetrics;
  QueueMetrics queueMetrics;
  ConnectionMetrics connectionMetrics;
  mutable SemaphoreHandle_t metricsMutex; // Thread-safe metrics access (mutable for const methods)

  // MTU Negotiation Timeout Control
  MTUNegotiationControl mtuControl;
  mutable SemaphoreHandle_t mtuControlMutex; // Separate mutex for MTU control (mutable for const methods)

  // BLE Transmission Protection
  SemaphoreHandle_t transmissionMutex; // Protect BLE transmission from race conditions

  // Streaming State Control
  bool streamingActive;                  // Flag to control streaming state
  SemaphoreHandle_t streamingStateMutex; // Protect streaming state access
  uint8_t activeTransmissions;           // Count of in-flight transmissions (protected by transmissionMutex)

  // FreeRTOS task functions
  static void commandProcessingTask(void *parameter);
  static void streamingTask(void *parameter);
  static void metricsMonitorTask(void *parameter); // Metrics monitoring

  // Metrics methods
  void updateQueueMetrics();
  void logMTUNegotiation();
  void logConnectionMetrics();
  void publishMetrics();
  void initializeMetrics();

  // MTU Negotiation Timeout Control Methods
  void initiateMTUNegotiation();
  void checkMTUNegotiationTimeout();
  void completeMTUNegotiation();
  void handleMTUNegotiationTimeout();
  void retryMTUNegotiation();
  bool isMTUNegotiationActive() const;
  void setMTUFallback(uint16_t fallbackSize);

  // Fragment handling
  void receiveFragment(const String &fragment);
  void handleCompleteCommand(const char *command);
  void sendFragmented(const char *data, size_t length);

public:
  BLEManager(const String &name, CRUDHandler *cmdHandler);
  ~BLEManager();

  bool begin();
  void stop();

  // Response methods
  void sendResponse(const JsonDocument &data);
  void sendError(const String &message);
  void sendSuccess();

  // Metrics access methods
  MTUMetrics getMTUMetrics() const;
  QueueMetrics getQueueMetrics() const;
  ConnectionMetrics getConnectionMetrics() const;
  void reportMetrics(JsonObject &metricsObj);

  // Streaming state control methods
  void setStreamingActive(bool active);
  bool isStreamingActive() const;
  bool waitForTransmissionsComplete(uint32_t timeoutMs = 2000);

  // v2.5.17: Get BLE server for OTA service
  BLEServer* getServer() const { return pServer; }

  // BLE callbacks
  void onConnect(BLEServer *pServer) override;
  void onDisconnect(BLEServer *pServer) override;
  void onWrite(BLECharacteristic *pCharacteristic) override;
};

#endif