#include "DebugConfig.h"  // MUST BE FIRST for LOG_* macros
#include "LEDManager.h"

LEDManager *LEDManager::instance = nullptr;

// Private constructor
LEDManager::LEDManager()
    : ledTaskHandle(nullptr), currentState(LED_OFF),
      lastBlinkMillis(0), lastDataMillis(0), ledState(LOW),
      mqttConnected(false), httpConnected(false), stateMutex(nullptr)
{
}

// Singleton getInstance method
LEDManager *LEDManager::getInstance()
{
  // Thread-safe Meyers Singleton (C++11 guarantees thread-safe static init)
  static LEDManager instance;
  static LEDManager *ptr = &instance;
  return ptr;
}

// Initialize the LED pin and start the task
void LEDManager::begin()
{
  pinMode(LED_NET, OUTPUT);
  digitalWrite(LED_NET, LOW); // Ensure LED is off initially

  // Create state mutex
  stateMutex = xSemaphoreCreateMutex();
  if (!stateMutex)
  {
    LOG_LED_INFO("[LED] ERROR: Failed to create state mutex");
    return;
  }

  // Create LED blinking task
  // OPTIMIZATION: Moved to Core 0 to balance load (low-priority task)
  // v2.5.39: Increased stack from 2048 to 3072 to prevent stack overflow
  // (LOG_LED_INFO with printf formatting requires more stack space)
  xTaskCreatePinnedToCore(
      ledBlinkTask,
      "LED_Blink_Task",
      3072, // Stack size (v2.5.39: increased from 2048 to prevent overflow)
      this,
      1, // Priority (low but higher than 0)
      &ledTaskHandle,
      0 // Core 0 (moved from Core 1 for load balancing)
  );
  LOG_LED_INFO("[LED] Manager initialized");
}

// Set MQTT connection status
void LEDManager::setMqttConnectionStatus(bool connected)
{
  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    mqttConnected = connected;
    updateLEDState();
    xSemaphoreGive(stateMutex);
  }
}

// Set HTTP connection status
void LEDManager::setHttpConnectionStatus(bool connected)
{
  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    httpConnected = connected;
    updateLEDState();
    xSemaphoreGive(stateMutex);
  }
}

// Notify data transmission (MQTT/HTTP publish success)
void LEDManager::notifyDataTransmission()
{
  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    lastDataMillis = millis();
    updateLEDState();
    xSemaphoreGive(stateMutex);
  }
}

// Update LED state based on connection and data transmission
void LEDManager::updateLEDState()
{
  LEDState newState;
  bool hasConnection = mqttConnected || httpConnected;

  if (!hasConnection)
  {
    // No connection - LED OFF
    newState = LED_OFF;
  }
  else
  {
    // Check if we have recent data transmission
    if (lastDataMillis > 0 && (millis() - lastDataMillis < LED_DATA_TIMEOUT))
    {
      // Connected with recent data - Fast blink
      newState = LED_FAST_BLINK;
    }
    else
    {
      // Connected but no recent data - Slow blink
      newState = LED_SLOW_BLINK;
    }
  }

  // Update state if changed
  if (newState != currentState)
  {
    currentState = newState;
    LOG_LED_INFO("[LED] State changed to: %s (MQTT:%s HTTP:%s)\n",
                  currentState == LED_OFF ? "OFF" : currentState == LED_SLOW_BLINK ? "SLOW_BLINK"
                                                                                   : "FAST_BLINK",
                  mqttConnected ? "ON" : "OFF",
                  httpConnected ? "ON" : "OFF");
  }
}

// Get current blink interval based on state
unsigned long LEDManager::getBlinkInterval()
{
  switch (currentState)
  {
  case LED_FAST_BLINK:
    return LED_FAST_BLINK_INTERVAL;
  case LED_SLOW_BLINK:
    return LED_SLOW_BLINK_INTERVAL;
  default:
    return 0; // LED_OFF - no blinking
  }
}

// Stop the LED task (if needed)
void LEDManager::stop()
{
  if (ledTaskHandle)
  {
    vTaskDelete(ledTaskHandle);
    ledTaskHandle = nullptr;
    digitalWrite(LED_NET, LOW); // Ensure LED is off
    LOG_LED_INFO("[LED] Manager task stopped");
  }
}

// FreeRTOS task function
void LEDManager::ledBlinkTask(void *parameter)
{
  LEDManager *manager = static_cast<LEDManager *>(parameter);
  manager->ledLoop();
}

// Main loop for the LED task
void LEDManager::ledLoop()
{
  while (true)
  {
    // Update LED state (check for timeouts, etc.)
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
      updateLEDState();

      // Handle LED blinking based on current state
      if (currentState == LED_OFF)
      {
        // Turn off LED
        if (ledState == HIGH)
        {
          digitalWrite(LED_NET, LOW);
          ledState = LOW;
        }
      }
      else
      {
        // Blink LED with appropriate interval
        unsigned long interval = getBlinkInterval();
        if (millis() - lastBlinkMillis >= interval)
        {
          lastBlinkMillis = millis();
          ledState = !ledState; // Toggle LED state
          digitalWrite(LED_NET, ledState);
        }
      }

      xSemaphoreGive(stateMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(50)); // Check every 50ms
  }
}

// Destructor - cleanup resources to prevent memory leaks
LEDManager::~LEDManager()
{
  // Stop task first
  stop();

  // Delete mutex
  if (stateMutex)
  {
    vSemaphoreDelete(stateMutex);
    stateMutex = nullptr;
  }

  LOG_LED_INFO("[LED] Manager destroyed, resources cleaned up");
}
