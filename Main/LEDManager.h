#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

// Define the LED pin
#define LED_NET 7

// LED State definitions
enum LEDState {
  LED_OFF = 0,         // No connection - LED OFF
  LED_SLOW_BLINK = 1,  // Connected but no data - Slow blink (1Hz)
  LED_FAST_BLINK = 2   // Connected with data - Fast blink (5Hz)
};

// Blink interval definitions (ms)
#define LED_FAST_BLINK_INTERVAL 200   // 200ms = 5Hz (0.1s on, 0.1s off)
#define LED_SLOW_BLINK_INTERVAL 1000  // 1000ms = 1Hz (0.5s on, 0.5s off)
#define LED_DATA_TIMEOUT \
  5000  // 5 seconds - transition from FAST to SLOW if no data

class LEDManager {
 private:
  static LEDManager* instance;
  TaskHandle_t ledTaskHandle;

  LEDState currentState;          // Current LED state
  unsigned long lastBlinkMillis;  // Last time LED state was changed
  unsigned long lastDataMillis;   // Last time data was transmitted
  int ledState;                  // Current physical state of the LED (HIGH/LOW)
  bool mqttConnected;            // Flag to track MQTT connection
  bool httpConnected;            // Flag to track HTTP connection
  SemaphoreHandle_t stateMutex;  // Mutex to protect state changes

  // Private constructor for Singleton pattern
  LEDManager();

  // FreeRTOS task function
  static void ledBlinkTask(void* parameter);
  void ledLoop();

  // Get current blink interval based on state
  unsigned long getBlinkInterval();

  // Update LED state based on connection and data transmission
  void updateLEDState();

 public:
  // Singleton getInstance method
  static LEDManager* getInstance();

  // Initialize the LED pin and start the task
  void begin();

  // Set MQTT connection status
  void setMqttConnectionStatus(bool connected);

  // Set HTTP connection status
  void setHttpConnectionStatus(bool connected);

  // Notify data transmission (MQTT/HTTP publish success)
  void notifyDataTransmission();

  // Stop the LED task (if needed)
  void stop();

  // Get current LED state (for debugging)
  LEDState getCurrentState() const { return currentState; }

  // Destructor - cleanup resources
  ~LEDManager();
};

extern LEDManager* ledManager;  // Declare global instance

#endif  // LED_MANAGER_H
