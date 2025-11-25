#include "BLEManager.h"
#include "CRUDHandler.h"
#include "QueueManager.h"
#include "MemoryManager.h" // Include the new memory manager
#include <esp_heap_caps.h>
#include <esp_bt.h> // For BT controller memory release
#include <new>

BLEManager::BLEManager(const String &name, CRUDHandler *cmdHandler)
    : serviceName(name), handler(cmdHandler), processing(false),
      pServer(nullptr), pService(nullptr), pCommandChar(nullptr), pResponseChar(nullptr),
      streamTaskHandle(nullptr), metricsTaskHandle(nullptr), commandBufferIndex(0),
      lastFragmentTime(0) // CRITICAL FIX: Initialize fragment timeout tracking
{
  memset(commandBuffer, 0, COMMAND_BUFFER_SIZE);
  commandQueue = xQueueCreate(20, sizeof(char *)); // Queue now holds char pointers

  // Initialize metrics
  initializeMetrics();
}

BLEManager::~BLEManager()
{
  stop();
  if (commandQueue)
  {
    // Purge the queue of any remaining pointers
    char *cmd;
    while (xQueueReceive(commandQueue, &cmd, 0))
    {
      heap_caps_free(cmd);
    }
    vQueueDelete(commandQueue);
  }

  // Clean up metrics mutex
  if (metricsMutex)
  {
    vSemaphoreDelete(metricsMutex);
    metricsMutex = nullptr;
  }

  // Clean up MTU control mutex
  if (mtuControlMutex)
  {
    vSemaphoreDelete(mtuControlMutex);
    mtuControlMutex = nullptr;
  }

  // Clean up transmission mutex (FIXED: was missing - memory leak)
  if (transmissionMutex)
  {
    vSemaphoreDelete(transmissionMutex);
    transmissionMutex = nullptr;
  }

  // Clean up streaming state mutex (FIXED: was missing - memory leak)
  if (streamingStateMutex)
  {
    vSemaphoreDelete(streamingStateMutex);
    streamingStateMutex = nullptr;
  }

  Serial.println("[BLE] Manager destroyed, resources cleaned up");
}

bool BLEManager::begin()
{
  // Check if already running (prevent double initialization)
  if (pServer != nullptr)
  {
    Serial.println("[BLE] Already running, skipping initialization");
    return true;
  }

  // FIXED BUG #4: Verify all mutexes were created successfully by initializeMetrics()
  // Previous code didn't check if mutex creation succeeded → NULL pointer crashes!
  if (!metricsMutex || !mtuControlMutex || !transmissionMutex || !streamingStateMutex)
  {
    Serial.println("[BLE] CRITICAL ERROR: Mutex initialization failed!");
    Serial.printf("[BLE] metricsMutex: %p, mtuControlMutex: %p, transmissionMutex: %p, streamingStateMutex: %p\n",
                  metricsMutex, mtuControlMutex, transmissionMutex, streamingStateMutex);
    return false; // Abort initialization - unsafe to continue
  }

  // CRITICAL FIX: Release Classic Bluetooth memory BEFORE BLE init
  // This frees ~50KB DRAM to prevent "BLE_INIT: Malloc failed" errors
  esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
  Serial.println("[BLE] Classic BT memory released (~50KB DRAM freed)");

  // Log free DRAM before BLE init
  size_t freeDRAM_before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  Serial.printf("[BLE] Free DRAM before init: %zu KB\n", freeDRAM_before / 1024);

  // Initialize BLE
  BLEDevice::init(serviceName.c_str());

  // Log free DRAM after BLE init
  size_t freeDRAM_after = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  Serial.printf("[BLE] Free DRAM after init: %zu KB (used: %zu KB)\n",
                freeDRAM_after / 1024,
                (freeDRAM_before - freeDRAM_after) / 1024);

  // FIXED BUG #12: Use conservative MTU for better compatibility
  // Previous: Hardcoded 517 bytes - not all clients support this (especially iOS)
  // New: Start with BLE_MTU_SAFE_DEFAULT (safe for all devices), negotiate higher if supported
  BLEDevice::setMTU(BLE_MTU_SAFE_DEFAULT); // Conservative MTU for maximum compatibility
  Serial.printf("[BLE] MTU set to %d bytes (safe default, will negotiate higher if supported)\n", BLE_MTU_SAFE_DEFAULT);

  // Create BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(this);

  // Create BLE Service
  pService = pServer->createService(SERVICE_UUID);

  // Create Command Characteristic (Write)
  pCommandChar = pService->createCharacteristic(
      COMMAND_CHAR_UUID,
      BLECharacteristic::PROPERTY_WRITE);
  pCommandChar->setCallbacks(this);

  // Create Response Characteristic (Notify)
  pResponseChar = pService->createCharacteristic(
      RESPONSE_CHAR_UUID,
      BLECharacteristic::PROPERTY_NOTIFY);
  // Deskriptor 2902 ditambahkan secara otomatis oleh library

  // Start service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);

  // Set device name in advertising (critical for device to be discoverable with name)
  pAdvertising->setName(serviceName.c_str());

  BLEDevice::startAdvertising();
  Serial.printf("[BLE] Advertising started with name: %s\n", serviceName.c_str());

  // Create command processing task with PSRAM stack
  xTaskCreatePinnedToCore(
      commandProcessingTask,
      "BLE_CMD_TASK",
      8192, // Increased stack size for PSRAM usage
      this,
      1,
      &commandTaskHandle,
      1 // Pin to core 1
  );

  // Create streaming task
  xTaskCreatePinnedToCore(
      streamingTask,
      "BLE_STREAM_TASK",
      4096,
      this,
      1,
      &streamTaskHandle,
      1);

  // Create metrics monitoring task
  // MUST stay on Core 1: Accesses BLE data structures managed by BLE_CMD_TASK (Core 1)
  xTaskCreatePinnedToCore(
      metricsMonitorTask,
      "BLE_METRICS_TASK",
      4096,
      this,
      0, // Lower priority for monitoring
      &metricsTaskHandle,
      1); // Core 1 (must stay with other BLE tasks to avoid race conditions)

  Serial.println("[BLE] Manager initialized: " + serviceName);
  Serial.println("[BLE] MTU Metrics and Queue Monitoring enabled");
  return true;
}

void BLEManager::stop()
{
  // Check if already stopped
  if (pServer == nullptr)
  {
    Serial.println("[BLE] Already stopped");
    return;
  }

  // Tasks will exit gracefully when they check 'started' flag in their loop
  // Give them a moment to finish current iteration before cleanup
  if (commandTaskHandle || streamTaskHandle)
  {
    vTaskDelay(pdMS_TO_TICKS(50)); // Brief delay for graceful exit
  }

  if (commandTaskHandle)
  {
    vTaskDelete(commandTaskHandle);
    commandTaskHandle = nullptr;
  }

  if (streamTaskHandle)
  {
    vTaskDelete(streamTaskHandle);
    streamTaskHandle = nullptr;
  }

  // Delete metrics monitoring task
  if (metricsTaskHandle)
  {
    vTaskDelete(metricsTaskHandle);
    metricsTaskHandle = nullptr;
  }

  if (pServer)
  {
    BLEDevice::deinit();
    pServer = nullptr;
    pService = nullptr;
    pCommandChar = nullptr;
    pResponseChar = nullptr;
    Serial.println("[BLE] Manager stopped");
  }
}

void BLEManager::onConnect(BLEServer *pServer)
{
  Serial.println("[BLE] Client connected");

  // Log connection and MTU negotiation
  if (xSemaphoreTake(metricsMutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    connectionMetrics.isConnected = true;
    connectionMetrics.connectionStartTime = millis();
    connectionMetrics.fragmentsSent = 0;
    connectionMetrics.fragmentsReceived = 0;
    connectionMetrics.bytesTransmitted = 0;
    connectionMetrics.bytesReceived = 0;

    xSemaphoreGive(metricsMutex);
  }

  // Initiate MTU negotiation with timeout protection
  initiateMTUNegotiation();

  // OPTIMIZED: Wait for MTU negotiation to complete before logging
  // BLE MTU negotiation is asynchronous and takes ~100-200ms
  vTaskDelay(pdMS_TO_TICKS(200)); // Wait 200ms for negotiation

  // Log MTU negotiation
  logMTUNegotiation();
}

void BLEManager::onDisconnect(BLEServer *pServer)
{
  Serial.println("[BLE] Client disconnected");

  // Log connection duration and metrics
  if (xSemaphoreTake(metricsMutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    unsigned long connectionDuration = millis() - connectionMetrics.connectionStartTime;
    connectionMetrics.totalConnectionTime += connectionDuration;
    connectionMetrics.isConnected = false;

    Serial.printf("[BLE METRICS] Duration: %lu ms  Fragments: %lu  Bytes TX: %lu\n",
                  connectionDuration, connectionMetrics.fragmentsSent, connectionMetrics.bytesTransmitted);

    xSemaphoreGive(metricsMutex);
  }

  // Stop streaming when client disconnects
  extern CRUDHandler *crudHandler;
  if (crudHandler)
  {
    crudHandler->clearStreamDeviceId();
    QueueManager *queueMgr = QueueManager::getInstance();
    if (queueMgr)
    {
      queueMgr->clearStream();
    }
    Serial.println("[BLE] Cleared streaming on disconnect");
  }

  BLEDevice::startAdvertising(); // Restart advertising
}

void BLEManager::onWrite(BLECharacteristic *pCharacteristic)
{
  if (pCharacteristic == pCommandChar)
  {
    String value = pCharacteristic->getValue().c_str();
    receiveFragment(value);
  }
}

void BLEManager::receiveFragment(const String &fragment)
{
  if (processing)
    return;

  // CRITICAL FIX: Timeout protection - clear buffer if incomplete command stuck
  // If buffer has data but no fragment received for 5 seconds, assume command failed
  constexpr unsigned long COMMAND_TIMEOUT_MS = 5000; // 5 seconds timeout
  unsigned long now = millis();

  if (commandBufferIndex > 0 && (now - lastFragmentTime) > COMMAND_TIMEOUT_MS)
  {
    Serial.printf("[BLE] WARNING: Command timeout! Buffer had %d bytes but no <END> received for %lu ms\n",
                  commandBufferIndex, now - lastFragmentTime);
    Serial.println("[BLE] Clearing dirty buffer to prevent corruption");
    commandBufferIndex = 0;
    memset(commandBuffer, 0, COMMAND_BUFFER_SIZE);
    processing = false;
  }

  // Update last fragment time
  lastFragmentTime = now;

  // Track fragment reception
  if (xSemaphoreTake(metricsMutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    connectionMetrics.fragmentsReceived++;
    connectionMetrics.bytesReceived += fragment.length();
    xSemaphoreGive(metricsMutex);
  }

  // CRITICAL FIX: Handle <START> marker to clear dirty buffer
  // Prevents data corruption from incomplete previous commands
  if (fragment == "<START>")
  {
#if PRODUCTION_MODE == 0
    Serial.println("[BLE] <START> marker received - clearing buffer");
#endif
    commandBufferIndex = 0;
    memset(commandBuffer, 0, COMMAND_BUFFER_SIZE);
    processing = false; // Ensure we're ready for new command
    return;
  }

  if (fragment == "<END>")
  {
    // Validate buffer has data before processing
    if (commandBufferIndex == 0)
    {
      Serial.println("[BLE] WARNING: <END> received but buffer is empty (missing <START>?)");
      return;
    }

    processing = true;
    commandBuffer[commandBufferIndex] = '\0'; // Null-terminate the command

#if PRODUCTION_MODE == 0
    Serial.printf("[BLE] <END> marker received - assembling command (%d bytes)\n", commandBufferIndex);
#endif

    // Allocate a buffer in PSRAM for the complete command
    char *cmdBuffer = (char *)heap_caps_malloc(commandBufferIndex + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (cmdBuffer)
    {
      strcpy(cmdBuffer, commandBuffer);
      if (xQueueSend(commandQueue, &cmdBuffer, 0) != pdPASS)
      {
        // If queue is full, free the memory we just allocated
        heap_caps_free(cmdBuffer);
        Serial.println("[BLE] Command queue full, command dropped.");

        // Track dropped commands
        if (xSemaphoreTake(metricsMutex, pdMS_TO_TICKS(100)) == pdTRUE)
        {
          queueMetrics.dropCount++;
          xSemaphoreGive(metricsMutex);
        }
      }
    }
    else
    {
      Serial.println("[BLE] ERROR: Failed to allocate memory for BLE command!");
    }

    // Reset buffer
    commandBufferIndex = 0;
    memset(commandBuffer, 0, COMMAND_BUFFER_SIZE);
    processing = false;
  }
  else
  {
    // Regular fragment (data payload)
    size_t fragmentLen = fragment.length();
    if (commandBufferIndex + fragmentLen < COMMAND_BUFFER_SIZE)
    {
      memcpy(commandBuffer + commandBufferIndex, fragment.c_str(), fragmentLen);
      commandBufferIndex += fragmentLen;

#if PRODUCTION_MODE == 0
      // Only log every 5th fragment to reduce serial spam
      static uint32_t fragmentCount = 0;
      fragmentCount++;
      if (fragmentCount % 5 == 0)
      {
        Serial.printf("[BLE] Fragment received (%d bytes, total: %d bytes)\n",
                      fragmentLen, commandBufferIndex);
      }
#endif
    }
    else
    {
      Serial.printf("[BLE] ERROR: Command buffer overflow! (buffer: %d, fragment: %d, total would be: %d)\n",
                    commandBufferIndex, fragmentLen, commandBufferIndex + fragmentLen);
      commandBufferIndex = 0;
      memset(commandBuffer, 0, COMMAND_BUFFER_SIZE);
      sendError("Command too long, buffer overflow.");
    }
  }
}

void BLEManager::commandProcessingTask(void *parameter)
{
  BLEManager *manager = static_cast<BLEManager *>(parameter);
  char *command;

  while (true)
  {
    // Use timeout instead of portMAX_DELAY to allow graceful task deletion
    // 1000ms timeout allows task to respond to vTaskDelete() within reasonable time
    if (xQueueReceive(manager->commandQueue, &command, pdMS_TO_TICKS(1000)))
    {
      manager->handleCompleteCommand(command);
      heap_caps_free(command); // Free the memory allocated in receiveFragment
    }
    // If timeout, loop continues and checks for task deletion
  }
}

void BLEManager::handleCompleteCommand(const char *command)
{
  // FIXED BUG #32: Improved logging for large commands (prevent serial buffer overflow)
  // Log complete command for debugging (with size limit for very large commands)
  size_t cmdLen = strlen(command);

#if PRODUCTION_MODE == 0
  // Development mode: Show full command for normal operations
  if (cmdLen <= 2000)
  {
    Serial.printf("\n[BLE CMD] Received command (%u bytes):\n", cmdLen);
    Serial.printf("  %s\n\n", command);
  }
  else
  {
    Serial.printf("\n[BLE CMD] Received large command (%u bytes) - too large to display\n\n", cmdLen);
  }
#else
  // Production mode: Just log length
  Serial.printf("[BLE CMD] Received %u bytes JSON\n", cmdLen);
#endif

  // FIXED BUG #32: Validate command length before parsing
  // Restore commands can be 3-4KB, backup responses can be 10-20KB
  if (cmdLen > COMMAND_BUFFER_SIZE)
  {
    Serial.printf("[BLE CMD] ERROR: Command too large (%u bytes > %u buffer)\n",
                  cmdLen, COMMAND_BUFFER_SIZE);
    sendError("Command exceeds buffer size");
    return;
  }

  // FIXED BUG #32: Pre-allocate JsonDocument with sufficient size
  // Previous: make_psram_unique<JsonDocument>() uses dynamic allocation (slow for large JSON)
  // New: Use SpiRamJsonDocument directly with PSRAM allocator (handles any size dynamically)
  // ArduinoJson v7 with PSRAMAllocator automatically grows as needed in PSRAM
  SpiRamJsonDocument doc;

  // Attempt deserialization
  DeserializationError error = deserializeJson(doc, command);

  if (error)
  {
    Serial.printf("[BLE CMD] ERROR: JSON parse failed - %s\n", error.c_str());
    Serial.printf("[BLE CMD] Command length: %u bytes\n", cmdLen);

// Show where parsing failed for debugging
#if PRODUCTION_MODE == 0
    if (cmdLen > 100)
    {
      char context[101];
      size_t errorPos = error.code() == DeserializationError::IncompleteInput ? cmdLen : 0;
      size_t contextStart = (errorPos > 50) ? (errorPos - 50) : 0;
      size_t contextLen = min((size_t)100, cmdLen - contextStart); // Cast to size_t
      strncpy(context, command + contextStart, contextLen);
      context[contextLen] = '\0';
      Serial.printf("[BLE CMD] Context: ...%s...\n", context);
    }
#endif

    sendError("Invalid JSON: " + String(error.c_str()));
    return;
  }

// Validation successful
#if PRODUCTION_MODE == 0
  // ArduinoJson v7: memoryUsage() is deprecated (always returns 0)
  // Just log successful parse with input size
  Serial.printf("[BLE CMD] JSON parsed successfully (%u bytes input)\n", cmdLen);
#endif

  if (handler)
  {
    handler->handle(this, doc);
  }
  else
  {
    sendError("No handler configured");
  }
}

void BLEManager::sendResponse(const JsonDocument &data)
{
  // FIXED BUG #9: Check size BEFORE allocating String to prevent OOM
  // Previous code serialized first, then checked → String already allocated!
  size_t estimatedSize = measureJson(data);

  // Early size check to prevent DRAM exhaustion
  if (estimatedSize > MAX_RESPONSE_SIZE_BYTES)
  {
    // Payload too large - send simplified error WITHOUT creating full response
    Serial.printf("[BLE] ERROR: Response too large (%u bytes > %d KB). Sending error.\n",
                  estimatedSize, MAX_RESPONSE_SIZE_BYTES / 1024);

    // Create minimal error response (< 200 bytes, safe)
    const char *errorMsg = "{\"status\":\"error\",\"message\":\"Response too large\",\"size\":";
    char buffer[ERROR_BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "%s%u,\"max\":%d}", errorMsg, estimatedSize, MAX_RESPONSE_SIZE_BYTES);

    pResponseChar->setValue(buffer);
    pResponseChar->notify();
    vTaskDelay(pdMS_TO_TICKS(50));
    pResponseChar->setValue("<END>");
    pResponseChar->notify();

    return; // Abort large response
  }

  // Size OK - proceed with PSRAM-based serialization
  // BUG #31 PART 2: Allocate buffer in PSRAM instead of String (DRAM)
  // String class ALWAYS uses DRAM → causes exhaustion with large responses

  // Allocate buffer in PSRAM with some margin for JSON overhead
  size_t bufferSize = estimatedSize + 512; // +512 bytes margin
  char *psramBuffer = (char *)heap_caps_malloc(bufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  if (!psramBuffer)
  {
    // PSRAM allocation failed - try DRAM as last resort
    Serial.printf("[BLE] WARNING: PSRAM allocation failed, trying DRAM (%u bytes)\n", bufferSize);
    psramBuffer = (char *)heap_caps_malloc(bufferSize, MALLOC_CAP_8BIT);

    if (!psramBuffer)
    {
      // Both failed - send error
      Serial.printf("[BLE] ERROR: Buffer allocation failed (%u bytes)\n", bufferSize);
      const char *errorMsg = "{\"status\":\"error\",\"message\":\"Memory allocation failed\"}";
      pResponseChar->setValue(errorMsg);
      pResponseChar->notify();
      vTaskDelay(pdMS_TO_TICKS(50));
      pResponseChar->setValue("<END>");
      pResponseChar->notify();
      return;
    }
  }

  // Serialize directly to PSRAM buffer
  size_t serializedLength = serializeJson(data, psramBuffer, bufferSize);

  if (serializedLength == 0)
  {
    Serial.printf("[BLE] ERROR: Serialization failed\n");
    heap_caps_free(psramBuffer);
    return;
  }

  // Verbose log suppressed - summary shown in [STREAM] logs
  // Serial.printf("[BLE] Serialized %u bytes to PSRAM buffer\n", serializedLength);

  // Send fragmented data from PSRAM buffer
  sendFragmented(psramBuffer, serializedLength);

  // Free PSRAM buffer after transmission
  heap_caps_free(psramBuffer);
}

void BLEManager::sendError(const String &message)
{
  JsonDocument doc;
  doc["status"] = "error";
  doc["message"] = message;
  sendResponse(doc);
}

void BLEManager::sendSuccess()
{
  JsonDocument doc;
  doc["status"] = "ok";
  sendResponse(doc);
}

void BLEManager::sendFragmented(const char *data, size_t length)
{
  if (!pResponseChar || !data)
    return;

  // CRITICAL: Emergency DRAM check FIRST before ANY allocation
  // If DRAM < 5KB, even error response allocation will crash
  // Check INTERNAL DRAM only (not PSRAM)
  size_t freeDRAM = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

  if (freeDRAM < 5000)
  { // <5KB is critically low - EMERGENCY MODE
    Serial.printf("[BLE] CRITICAL: DRAM exhausted (%zu bytes). Aborting transmission.\n", freeDRAM);

    // Use stack-allocated const char* - NO heap allocation
    const char *emergencyMsg = "{\"status\":\"error\",\"msg\":\"Out of memory\"}";
    pResponseChar->setValue(emergencyMsg);
    pResponseChar->notify();
    vTaskDelay(pdMS_TO_TICKS(50));
    pResponseChar->setValue("<END>");
    pResponseChar->notify();

    return; // Abort immediately without any heap operations
  }

  // CRITICAL FIX: Reject payloads that are too large (>MAX_RESPONSE_SIZE_BYTES)
  // This should never happen since sendResponse() already checks, but double-check for safety
  if (length > MAX_RESPONSE_SIZE_BYTES)
  {
    Serial.printf("[BLE] ERROR: Payload too large (%u bytes > %d KB limit).\n",
                  length, MAX_RESPONSE_SIZE_BYTES / 1024);

    // Send error response instead (stack-allocated, no heap)
    const char *errorMsg = "{\"status\":\"error\",\"message\":\"Response too large for BLE transmission\"}";
    pResponseChar->setValue(errorMsg);
    pResponseChar->notify();
    vTaskDelay(pdMS_TO_TICKS(50));
    pResponseChar->setValue("<END>");
    pResponseChar->notify();

    return; // Abort large transmission
  }

  // Log low DRAM warning (non-critical)
  // Threshold lowered to 25KB based on empirical testing:
  // - BLE connected with no devices: stable at 28-29KB DRAM
  // - 30KB threshold caused excessive warnings during normal operation
  // - 25KB threshold provides safety margin while reducing log noise
  if (freeDRAM < 25000)
  { // <25KB free DRAM
    Serial.printf("[BLE] WARNING: Low DRAM (%zu bytes). Proceeding with caution.\n", freeDRAM);
  }

  // Track active transmission (atomic increment)
  __atomic_add_fetch(&activeTransmissions, 1, __ATOMIC_SEQ_CST);

  // Acquire transmission mutex to prevent race conditions
  if (xSemaphoreTake(transmissionMutex, pdMS_TO_TICKS(5000)) != pdTRUE)
  {
    Serial.println("[BLE] ERROR: Mutex timeout");
    __atomic_sub_fetch(&activeTransmissions, 1, __ATOMIC_SEQ_CST);
    return;
  }

  // CRITICAL FIX: Smart chunk size and delay based on payload size
  // BUG FIX: Always use slower delay for large payloads to prevent mobile app timeout
  // Previous behavior: Only slow down if DRAM is low - caused timeouts on healthy DRAM
  // New behavior: Payload size determines delay, DRAM determines chunk size
  // Three-tier system: Small (<5KB), Large (5-50KB), Extra-Large (>50KB)
  size_t adaptiveChunkSize = CHUNK_SIZE;
  int adaptiveDelay = FRAGMENT_DELAY_MS;

  if (length > XLARGE_PAYLOAD_THRESHOLD)
  { // >50KB payload (backup/restore operations)
    // Extra-large payload: slowest delay to prevent mobile app timeout
    adaptiveDelay = ADAPTIVE_DELAY_XLARGE_MS; // 50ms - critical for large backups

    // Chunk size depends on DRAM availability
    // v2.3.6 OPTIMIZED: Lowered from 30KB to 25KB to accommodate post-restore DRAM levels (~28KB)
    if (freeDRAM < 25000)
    {
      adaptiveChunkSize = ADAPTIVE_CHUNK_SIZE_LARGE; // 100 bytes
      Serial.printf("[BLE] XLARGE payload (%u bytes) + LOW DRAM (%zu bytes) = using SMALL chunks with XSLOW delay (size:%zu, delay:%dms)\n",
                    length, freeDRAM, adaptiveChunkSize, adaptiveDelay);
    }
    else
    {
      Serial.printf("[BLE] XLARGE payload (%u bytes) with healthy DRAM (%zu bytes) = using NORMAL chunks with XSLOW delay (size:%zu, delay:%dms)\n",
                    length, freeDRAM, adaptiveChunkSize, adaptiveDelay);
    }
  }
  else if (length > LARGE_PAYLOAD_THRESHOLD)
  { // 5-50KB payload
    // Large payload: moderate delay
    adaptiveDelay = ADAPTIVE_DELAY_LARGE_MS; // 20ms

    // Chunk size depends on DRAM availability
    // v2.3.6 OPTIMIZED: Lowered from 30KB to 25KB to accommodate post-restore DRAM levels (~28KB)
    if (freeDRAM < 25000)
    {
      adaptiveChunkSize = ADAPTIVE_CHUNK_SIZE_LARGE; // 100 bytes
      Serial.printf("[BLE] Large payload (%u bytes) + LOW DRAM (%zu bytes) = using SMALL chunks with SLOW delay (size:%zu, delay:%dms)\n",
                    length, freeDRAM, adaptiveChunkSize, adaptiveDelay);
    }
    else
    {
      Serial.printf("[BLE] Large payload (%u bytes) with healthy DRAM (%zu bytes) = using NORMAL chunks with SLOW delay (size:%zu, delay:%dms)\n",
                    length, freeDRAM, adaptiveChunkSize, adaptiveDelay);
    }
  }
  // else: Small payload (<5KB) - use default fast settings (244 bytes, 10ms)

  // BUG #31 PART 2: Data already in PSRAM buffer (from sendResponse)
  // No String overhead, no DRAM allocation
  const char *dataPtr = data;
  size_t dataLen = length;
  size_t i = 0;

  // Allocate static buffer for chunks to avoid repeated allocations
  char chunkBuffer[256]; // Max chunk size + safety margin

  while (i < dataLen)
  {
    size_t chunkLen = min(adaptiveChunkSize, dataLen - i);

    // SAFETY: Ensure chunk size doesn't exceed buffer
    if (chunkLen > 255)
    {
      chunkLen = 255;
    }

    // Copy chunk to buffer (avoid String::substring allocation)
    memcpy(chunkBuffer, dataPtr + i, chunkLen);
    chunkBuffer[chunkLen] = '\0'; // Null terminate

    // Send chunk
    pResponseChar->setValue(chunkBuffer);
    pResponseChar->notify();

    // Track fragment transmission
    if (xSemaphoreTake(metricsMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
      connectionMetrics.fragmentsSent++;
      connectionMetrics.bytesTransmitted += chunkLen;
      xSemaphoreGive(metricsMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(adaptiveDelay));
    i += chunkLen;
  }

  // Send end marker
  pResponseChar->setValue("<END>");
  pResponseChar->notify();

  xSemaphoreGive(transmissionMutex);

  // Mark transmission as complete (atomic decrement)
  __atomic_sub_fetch(&activeTransmissions, 1, __ATOMIC_SEQ_CST);
}

void BLEManager::streamingTask(void *parameter)
{
  BLEManager *manager = static_cast<BLEManager *>(parameter);
  QueueManager *queueMgr = QueueManager::getInstance();

  Serial.println("[BLE] Streaming task started");

  while (true)
  {
    // Check if streaming is active before processing queue
    if (manager->isStreamingActive() && queueMgr && !queueMgr->isStreamEmpty())
    {
      JsonDocument dataDoc;
      JsonObject dataPoint = dataDoc.to<JsonObject>();

      if (queueMgr->dequeueStream(dataPoint))
      {
        // Prepare response data
        JsonDocument response;
        response["status"] = "data";
        response["data"] = dataPoint;

        // Acquire transmission mutex BEFORE final check to prevent race condition
        if (xSemaphoreTake(manager->transmissionMutex, pdMS_TO_TICKS(5000)) == pdTRUE)
        {
          // Check streaming is still active (final check with mutex protection)
          if (manager->isStreamingActive())
          {
            // Release mutex before sendResponse (it will reacquire inside sendFragmented)
            xSemaphoreGive(manager->transmissionMutex);
            manager->sendResponse(response);
          }
          else
          {
            xSemaphoreGive(manager->transmissionMutex);
          }
        }
        else
        {
          Serial.println("[BLE] ERROR: Stream mutex timeout");
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// ============================================================================
// METRICS IMPLEMENTATION
// ============================================================================

void BLEManager::initializeMetrics()
{
  // Atomic initialization: all mutexes created or none (cleanup on failure)
  metricsMutex = xSemaphoreCreateMutex();
  if (!metricsMutex)
  {
    Serial.println("[BLE] ERROR: Failed to create metrics mutex");
    // No cleanup needed, first allocation
    return;
  }

  // Initialize MTU control mutex
  mtuControlMutex = xSemaphoreCreateMutex();
  if (!mtuControlMutex)
  {
    Serial.println("[BLE] ERROR: Failed to create MTU control mutex");
    // Cleanup partial allocations
    vSemaphoreDelete(metricsMutex);
    metricsMutex = nullptr;
    return;
  }

  // Initialize transmission mutex
  transmissionMutex = xSemaphoreCreateMutex();
  if (!transmissionMutex)
  {
    Serial.println("[BLE] ERROR: Failed to create transmission mutex");
    // Cleanup partial allocations
    vSemaphoreDelete(metricsMutex);
    vSemaphoreDelete(mtuControlMutex);
    metricsMutex = nullptr;
    mtuControlMutex = nullptr;
    return;
  }

  // Initialize streaming state mutex
  streamingStateMutex = xSemaphoreCreateMutex();
  if (!streamingStateMutex)
  {
    Serial.println("[BLE] ERROR: Failed to create streaming state mutex");
    // Cleanup partial allocations
    vSemaphoreDelete(metricsMutex);
    vSemaphoreDelete(mtuControlMutex);
    vSemaphoreDelete(transmissionMutex);
    metricsMutex = nullptr;
    mtuControlMutex = nullptr;
    transmissionMutex = nullptr;
    return;
  }
  streamingActive = false; // Initially not streaming
  activeTransmissions = 0; // No transmissions in progress

  // Initialize all metrics to zero
  mtuMetrics = MTUMetrics();
  queueMetrics = QueueMetrics();
  connectionMetrics = ConnectionMetrics();

  // Initialize MTU control
  mtuControl = MTUNegotiationControl();

  Serial.println("[BLE] Metrics initialized");
  Serial.println("[BLE] MTU Negotiation timeout protection enabled");
}

void BLEManager::logMTUNegotiation()
{
  if (xSemaphoreTake(metricsMutex, pdMS_TO_TICKS(100)) != pdTRUE)
  {
    return;
  }

  // FIXED: Get actual MTU size from BLE layer after negotiation
  uint16_t actualMTU = BLEDevice::getMTU();                       // Returns full MTU (includes overhead)
  uint16_t effectiveMTU = (actualMTU > 3) ? (actualMTU - 3) : 20; // Subtract ATT header (3 bytes)

  mtuMetrics.mtuSize = effectiveMTU;
  mtuMetrics.maxMTUSize = BLE_MTU_MAX_SUPPORTED; // Our maximum supported MTU
  mtuMetrics.lastNegotiationTime = millis();
  mtuMetrics.negotiationAttempts++;

  // OPTIMIZED: Only mark as negotiated if MTU is above default (23 bytes)
  if (actualMTU > 23)
  {
    mtuMetrics.mtuNegotiated = true;
    Serial.printf("[BLE MTU] Negotiation OK  Actual MTU: %d bytes  Effective: %d bytes  Max: %d bytes  Time: %lu ms  Attempts: %d\n",
                  actualMTU, mtuMetrics.mtuSize, mtuMetrics.maxMTUSize, mtuMetrics.lastNegotiationTime, mtuMetrics.negotiationAttempts);

    // CRITICAL FIX: Mark negotiation as completed to stop timeout monitoring
    // Without this, checkMTUNegotiationTimeout() continues checking and triggers
    // false positive timeouts (e.g., after 54s even though negotiation succeeded at 10s)
    xSemaphoreGive(metricsMutex); // Release metrics mutex before acquiring MTU mutex
    completeMTUNegotiation();
  }
  else
  {
    mtuMetrics.mtuNegotiated = false;
    Serial.printf("[BLE MTU] Negotiation pending  Current MTU: %d bytes (waiting for higher MTU)\n", actualMTU);
    xSemaphoreGive(metricsMutex);
    // State remains INITIATING - timeout monitoring continues
  }
}

void BLEManager::updateQueueMetrics()
{
  if (xSemaphoreTake(metricsMutex, pdMS_TO_TICKS(100)) != pdTRUE)
  {
    return;
  }

  // Get current queue depth
  UBaseType_t queueCurrentMessages = uxQueueMessagesWaiting(commandQueue);
  queueMetrics.currentDepth = queueCurrentMessages;

  // Track peak depth
  if (queueCurrentMessages > queueMetrics.peakDepth)
  {
    queueMetrics.peakDepth = queueCurrentMessages;
  }

  // Calculate utilization percentage (out of 20 max)
  queueMetrics.utilizationPercent = (queueCurrentMessages / 20.0) * 100.0;

  xSemaphoreGive(metricsMutex);
}

void BLEManager::metricsMonitorTask(void *parameter)
{
  BLEManager *manager = static_cast<BLEManager *>(parameter);

  Serial.println("[BLE METRICS] Monitoring task started");

  uint32_t lastMetricsPublish = 0;
  const uint32_t METRICS_PUBLISH_INTERVAL = 60000; // Publish metrics every 60s
  const uint32_t MTU_CHECK_INTERVAL = 500;         // Check MTU timeout every 500ms

  while (true)
  {
    // Check for MTU negotiation timeout frequently (every 500ms)
    manager->checkMTUNegotiationTimeout();

    // Update and publish queue metrics only every 60 seconds
    uint32_t now = millis();
    if (now - lastMetricsPublish >= METRICS_PUBLISH_INTERVAL)
    {
      manager->updateQueueMetrics();
      manager->publishMetrics();
      lastMetricsPublish = now;
    }

    // Sleep for short interval to check MTU frequently
    vTaskDelay(pdMS_TO_TICKS(MTU_CHECK_INTERVAL));
  }
}

void BLEManager::publishMetrics()
{
  if (xSemaphoreTake(metricsMutex, pdMS_TO_TICKS(100)) != pdTRUE)
  {
    return;
  }

  // Log queue metrics
  Serial.printf("[BLE QUEUE] Depth: %lu/%d (%.1f%%)  Peak: %lu  Enqueued: %lu  Dequeued: %lu  Dropped: %lu\n",
                queueMetrics.currentDepth, 20, queueMetrics.utilizationPercent,
                queueMetrics.peakDepth, queueMetrics.totalEnqueued,
                queueMetrics.totalDequeued, queueMetrics.dropCount);

  // Log connection metrics
  if (connectionMetrics.isConnected)
  {
    unsigned long currentDuration = millis() - connectionMetrics.connectionStartTime;
    Serial.printf("[BLE CONN] Duration: %lu ms  Fragments TX/RX: %lu/%lu  Bytes TX/RX: %lu/%lu\n",
                  currentDuration, connectionMetrics.fragmentsSent, connectionMetrics.fragmentsReceived,
                  connectionMetrics.bytesTransmitted, connectionMetrics.bytesReceived);
  }

  xSemaphoreGive(metricsMutex);
}

void BLEManager::logConnectionMetrics()
{
  if (xSemaphoreTake(metricsMutex, pdMS_TO_TICKS(100)) != pdTRUE)
  {
    return;
  }

  Serial.println("\n[BLE] CONNECTION METRICS");
  Serial.printf("  Status: %s\n", connectionMetrics.isConnected ? "CONNECTED" : "DISCONNECTED");
  Serial.printf("  Fragments Sent: %lu\n", connectionMetrics.fragmentsSent);
  Serial.printf("  Fragments Received: %lu\n", connectionMetrics.fragmentsReceived);
  Serial.printf("  Bytes Transmitted: %lu\n", connectionMetrics.bytesTransmitted);
  Serial.printf("  Bytes Received: %lu\n", connectionMetrics.bytesReceived);
  Serial.printf("  Total Connection Time: %lu ms\n\n", connectionMetrics.totalConnectionTime);

  xSemaphoreGive(metricsMutex);
}

MTUMetrics BLEManager::getMTUMetrics() const
{
  return mtuMetrics;
}

QueueMetrics BLEManager::getQueueMetrics() const
{
  return queueMetrics;
}

ConnectionMetrics BLEManager::getConnectionMetrics() const
{
  return connectionMetrics;
}

void BLEManager::reportMetrics(JsonObject &metricsObj)
{
  if (xSemaphoreTake(metricsMutex, pdMS_TO_TICKS(100)) != pdTRUE)
  {
    Serial.println("[BLE] Could not acquire metrics mutex");
    return;
  }

  // MTU Metrics
  JsonObject mtuObj = metricsObj["mtu"].to<JsonObject>();
  mtuObj["size"] = mtuMetrics.mtuSize;
  mtuObj["max_size"] = mtuMetrics.maxMTUSize;
  mtuObj["negotiated"] = mtuMetrics.mtuNegotiated;
  mtuObj["negotiation_attempts"] = mtuMetrics.negotiationAttempts;
  mtuObj["last_negotiation_ms"] = mtuMetrics.lastNegotiationTime;

  // Queue Metrics
  JsonObject queueObj = metricsObj["queue"].to<JsonObject>();
  queueObj["current_depth"] = queueMetrics.currentDepth;
  queueObj["peak_depth"] = queueMetrics.peakDepth;
  queueObj["max_capacity"] = 20;
  queueObj["utilization_percent"] = queueMetrics.utilizationPercent;
  queueObj["total_enqueued"] = queueMetrics.totalEnqueued;
  queueObj["total_dequeued"] = queueMetrics.totalDequeued;
  queueObj["dropped_count"] = queueMetrics.dropCount;

  // Connection Metrics
  JsonObject connObj = metricsObj["connection"].to<JsonObject>();
  connObj["is_connected"] = connectionMetrics.isConnected;
  connObj["fragments_sent"] = connectionMetrics.fragmentsSent;
  connObj["fragments_received"] = connectionMetrics.fragmentsReceived;
  connObj["bytes_transmitted"] = connectionMetrics.bytesTransmitted;
  connObj["bytes_received"] = connectionMetrics.bytesReceived;
  connObj["total_connection_time_ms"] = connectionMetrics.totalConnectionTime;

  xSemaphoreGive(metricsMutex);
}

// ============================================================================
// STREAMING STATE CONTROL IMPLEMENTATION
// ============================================================================

void BLEManager::setStreamingActive(bool active)
{
  if (xSemaphoreTake(streamingStateMutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    streamingActive = active;
    xSemaphoreGive(streamingStateMutex);
    Serial.printf("[BLE] Streaming state set to: %s\n", active ? "ACTIVE" : "INACTIVE");
  }
  else
  {
    Serial.println("[BLE] WARNING: Failed to acquire streaming state mutex");
  }
}

bool BLEManager::isStreamingActive() const
{
  bool active = false;
  if (xSemaphoreTake(streamingStateMutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    active = streamingActive;
    xSemaphoreGive(streamingStateMutex);
  }
  return active;
}

bool BLEManager::waitForTransmissionsComplete(uint32_t timeoutMs)
{
  unsigned long startTime = millis();
  uint8_t lastCount = 0xFF;
  uint8_t currentCount = __atomic_load_n(&activeTransmissions, __ATOMIC_SEQ_CST);

  Serial.printf("[BLE] Waiting for %d in-flight transmissions to complete (timeout %dms)...\n", currentCount, timeoutMs);

  while (currentCount > 0)
  {
    // Print status if count changed
    if (currentCount != lastCount)
    {
      Serial.printf("[BLE] Active transmissions: %d\n", currentCount);
      lastCount = currentCount;
    }

    // Check timeout
    if (millis() - startTime > timeoutMs)
    {
      Serial.printf("[BLE] ERROR: Timeout waiting for transmissions (still active: %d)\n", currentCount);
      return false; // Timeout - still has active transmissions
    }

    vTaskDelay(pdMS_TO_TICKS(50)); // Check every 50ms
    currentCount = __atomic_load_n(&activeTransmissions, __ATOMIC_SEQ_CST);
  }

  Serial.println("[BLE] All transmissions completed");
  return true; // Success - all transmissions completed
}

// ============================================================================
// MTU NEGOTIATION TIMEOUT CONTROL IMPLEMENTATION
// ============================================================================

void BLEManager::initiateMTUNegotiation()
{
  if (xSemaphoreTake(mtuControlMutex, pdMS_TO_TICKS(100)) != pdTRUE)
  {
    Serial.println("[BLE MTU] WARNING: Could not acquire MTU control mutex for initiation");
    return;
  }

  mtuControl.state = MTU_STATE_INITIATING;
  mtuControl.negotiationStartTime = millis();
  mtuControl.retryCount = 0;
  mtuControl.usesFallback = false;

  xSemaphoreGive(mtuControlMutex);
}

void BLEManager::completeMTUNegotiation()
{
  if (xSemaphoreTake(mtuControlMutex, pdMS_TO_TICKS(100)) != pdTRUE)
  {
    return;
  }

  mtuControl.state = MTU_STATE_COMPLETED;

  xSemaphoreGive(mtuControlMutex);
}

void BLEManager::checkMTUNegotiationTimeout()
{
  if (xSemaphoreTake(mtuControlMutex, pdMS_TO_TICKS(100)) != pdTRUE)
  {
    return;
  }

  // Only check if negotiation is actively in progress
  if (mtuControl.state != MTU_STATE_INITIATING && mtuControl.state != MTU_STATE_IN_PROGRESS)
  {
    xSemaphoreGive(mtuControlMutex);
    return;
  }

  unsigned long elapsed = millis() - mtuControl.negotiationStartTime;

  // Check if timeout threshold exceeded
  if (elapsed > mtuControl.negotiationTimeoutMs)
  {
    Serial.printf("[BLE MTU] WARNING: Timeout detected after %lu ms\n", elapsed);
    xSemaphoreGive(mtuControlMutex);

    // Handle timeout (don't hold mutex during this operation)
    handleMTUNegotiationTimeout();
    return;
  }

  xSemaphoreGive(mtuControlMutex);
}

void BLEManager::handleMTUNegotiationTimeout()
{
  if (xSemaphoreTake(mtuControlMutex, pdMS_TO_TICKS(100)) != pdTRUE)
  {
    return;
  }

  mtuControl.state = MTU_STATE_TIMEOUT;

  // Update metrics
  if (xSemaphoreTake(metricsMutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    mtuMetrics.timeoutCount++;
    mtuMetrics.lastTimeoutTime = millis();
    xSemaphoreGive(metricsMutex);
  }

  // Check if we should retry
  if (mtuControl.retryCount < mtuControl.maxRetries)
  {
    Serial.printf("[BLE MTU] Retry %d/%d after timeout\n",
                  mtuControl.retryCount + 1, mtuControl.maxRetries);

    xSemaphoreGive(mtuControlMutex);
    retryMTUNegotiation();
  }
  else
  {
    // Max retries exceeded - use fallback
    Serial.printf("[BLE MTU] Max retries exceeded, using fallback MTU (%d bytes)\n",
                  mtuControl.fallbackMTU);

    mtuControl.state = MTU_STATE_FAILED;
    mtuControl.usesFallback = true;

    // Update metrics with fallback
    if (xSemaphoreTake(metricsMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
      mtuMetrics.failureCount++;
      mtuMetrics.mtuSize = mtuControl.fallbackMTU;
      xSemaphoreGive(metricsMutex);
    }

    xSemaphoreGive(mtuControlMutex);
  }
}

void BLEManager::retryMTUNegotiation()
{
  if (xSemaphoreTake(mtuControlMutex, pdMS_TO_TICKS(100)) != pdTRUE)
  {
    return;
  }

  mtuControl.retryCount++;

  // Phase 4: Calculate exponential backoff with reduced base delay (100ms instead of 200ms)
  // Retry 1: 100ms + jitter, Retry 2: 200ms + jitter (faster reconnection)
  uint32_t baseDelay = 100 * (1 << (mtuControl.retryCount - 1));
  uint32_t jitter = esp_random() % 100;
  uint32_t totalDelay = baseDelay + jitter;

  // Reset state for retry
  mtuControl.state = MTU_STATE_INITIATING;
  mtuControl.negotiationStartTime = millis();

  xSemaphoreGive(mtuControlMutex);

  // Wait before retrying
  vTaskDelay(pdMS_TO_TICKS(totalDelay));

  // Attempt negotiation again
  logMTUNegotiation();
}

bool BLEManager::isMTUNegotiationActive() const
{
  // FIXED BUG #3: Add mutex protection to prevent race condition
  // Previous code accessed mtuControl.state without lock → data race!
  bool isActive = false;

  // mtuControlMutex is mutable, no const_cast needed
  if (xSemaphoreTake(mtuControlMutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    isActive = (mtuControl.state == MTU_STATE_INITIATING || mtuControl.state == MTU_STATE_IN_PROGRESS);
    xSemaphoreGive(mtuControlMutex);
  }
  else
  {
    // Mutex timeout - assume not active to avoid blocking
    Serial.println("[BLE MTU] WARNING: Mutex timeout in isMTUNegotiationActive()");
  }

  return isActive;
}

void BLEManager::setMTUFallback(uint16_t fallbackSize)
{
  if (xSemaphoreTake(mtuControlMutex, pdMS_TO_TICKS(100)) != pdTRUE)
  {
    return;
  }

  // Validate fallback size (minimum 100 bytes for safety)
  if (fallbackSize < 100)
  {
    fallbackSize = 100;
  }

  mtuControl.fallbackMTU = fallbackSize;

  xSemaphoreGive(mtuControlMutex);
}