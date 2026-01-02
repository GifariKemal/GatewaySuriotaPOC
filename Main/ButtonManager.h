#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include <Arduino.h>
#include <OneButton.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "DebugConfig.h"

// Define button and LED pins
#define PIN_BUTTON 0
#define LED_STATUS 8

// System mode enum
enum SystemMode {
  MODE_RUNNING = 0,  // Production running mode - BLE OFF
  MODE_CONFIG = 1    // Configuration mode - BLE ON
};

// Forward declaration
class BLEManager;

class ButtonManager {
 private:
  static ButtonManager* instance;

  OneButton* button;
  TaskHandle_t buttonTaskHandle;

  SystemMode currentMode;
  bool productionMode;  // true if PRODUCTION_MODE == 1
  SemaphoreHandle_t modeMutex;

  BLEManager* bleManager;  // Reference to BLE manager

  // LED Status control
  unsigned long lastStatusBlinkMillis;
  int ledStatusState;

  // Private constructor for Singleton pattern
  ButtonManager();

  // Button callbacks (must be static for OneButton)
  static void handleLongPress();
  static void handleDoubleClick();

  // FreeRTOS task function
  static void buttonTask(void* parameter);
  void buttonLoop();

  // Mode switching
  void enterConfigMode();
  void enterRunningMode();

  // LED Status control
  void updateLEDStatus();
  unsigned long getStatusBlinkInterval();

 public:
  // Singleton getInstance method
  static ButtonManager* getInstance();

  // Initialize button and LED
  void begin(bool isProductionMode);

  // Set BLE manager reference
  void setBLEManager(BLEManager* ble) { bleManager = ble; }

  // Get current mode
  SystemMode getCurrentMode() const { return currentMode; }
  bool isConfigMode() const { return currentMode == MODE_CONFIG; }
  bool isProductionMode() const { return productionMode; }

  // Stop the button task
  void stop();

  // Destructor - cleanup resources
  ~ButtonManager();
};

extern ButtonManager* buttonManager;  // Declare global instance

#endif  // BUTTON_MANAGER_H
