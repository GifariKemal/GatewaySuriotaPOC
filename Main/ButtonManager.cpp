#include "ButtonManager.h"

#include "BLEManager.h"

// NOTE: This static instance is unused (Meyers Singleton pattern used in
// getInstance()) Kept for header declaration compatibility - does not cause
// memory issues
ButtonManager* ButtonManager::instance = nullptr;

// Private constructor
ButtonManager::ButtonManager()
    : button(nullptr),
      buttonTaskHandle(nullptr),
      currentMode(MODE_RUNNING),
      productionMode(true),
      modeMutex(nullptr),
      bleManager(nullptr),
      lastStatusBlinkMillis(0),
      ledStatusState(LOW) {}

// Singleton getInstance method
ButtonManager* ButtonManager::getInstance() {
  // Thread-safe Meyers Singleton (C++11 guarantees thread-safe static init)
  static ButtonManager instance;
  static ButtonManager* ptr = &instance;
  return ptr;
}

// Initialize button and LED
void ButtonManager::begin(bool isProductionMode) {
  productionMode = isProductionMode;

  // Initialize LED Status pin
  pinMode(LED_STATUS, OUTPUT);
  digitalWrite(LED_STATUS, LOW);

  // Initialize button pin
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  // Create OneButton instance
  button = new OneButton(PIN_BUTTON, true);  // true = active LOW (with pullup)

  // Attach button callbacks
  button->attachLongPressStop(handleLongPress);
  button->attachDoubleClick(handleDoubleClick);

  // Create mode mutex
  modeMutex = xSemaphoreCreateMutex();
  if (!modeMutex) {
    LOG_LED_INFO("[BUTTON] ERROR: Failed to create mode mutex");
    return;
  }

  // Set initial mode based on production/development
  if (productionMode) {
    // Production mode: start in RUNNING mode (BLE OFF)
    currentMode = MODE_RUNNING;
    LOG_LED_INFO(
        "[BUTTON] Production mode: Starting in RUNNING mode (BLE OFF)");
  } else {
    // Development mode: start in CONFIG mode (BLE ON, button disabled)
    currentMode = MODE_CONFIG;
    LOG_LED_INFO(
        "[BUTTON] Development mode: Starting in CONFIG mode (BLE ON, button "
        "disabled)");
  }

  // Create button monitoring task
  xTaskCreatePinnedToCore(buttonTask, "Button_Task",
                          3072,  // Stack size
                          this,
                          2,  // Priority (higher than LED tasks)
                          &buttonTaskHandle,
                          APP_CPU_NUM  // Pin to APP_CPU_NUM (core 1)
  );
  LOG_LED_INFO("[BUTTON] Manager initialized");
}

// Static callback for long press
void ButtonManager::handleLongPress() {
  ButtonManager* mgr = ButtonManager::getInstance();
  if (mgr && mgr->productionMode) {
    mgr->enterConfigMode();
  }
}

// Static callback for double click
void ButtonManager::handleDoubleClick() {
  ButtonManager* mgr = ButtonManager::getInstance();
  if (mgr && mgr->productionMode) {
    mgr->enterRunningMode();
  }
}

// Enter configuration mode
void ButtonManager::enterConfigMode() {
  if (xSemaphoreTake(modeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    if (currentMode != MODE_CONFIG) {
      currentMode = MODE_CONFIG;
      // v2.5.1: Use Serial.printf for production mode feedback (LOG_LED_INFO
      // suppressed in prod)
      Serial.println("[BUTTON] Entering CONFIG mode - BLE ON");

      // Turn on BLE
      if (bleManager) {
        bleManager->begin();
      }
    }
    xSemaphoreGive(modeMutex);
  }
}

// Enter running mode
void ButtonManager::enterRunningMode() {
  if (xSemaphoreTake(modeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    if (currentMode != MODE_RUNNING) {
      currentMode = MODE_RUNNING;
      // v2.5.1: Use Serial.printf for production mode feedback (LOG_LED_INFO
      // suppressed in prod)
      Serial.println("[BUTTON] Entering RUNNING mode - BLE OFF");

      // Turn off BLE
      if (bleManager) {
        bleManager->stop();
      }
    }
    xSemaphoreGive(modeMutex);
  }
}

// Get LED Status blink interval based on mode
unsigned long ButtonManager::getStatusBlinkInterval() {
  if (!productionMode) {
    // Development mode: slow blink (2000ms = 0.5Hz)
    return 2000;
  } else {
    if (currentMode == MODE_CONFIG) {
      // v2.5.1: CONFIG mode (BLE ON): Fast flicker (100ms = 5Hz) - visual
      // indicator BLE active
      return 100;
    } else {
      // v2.5.1: RUNNING mode (BLE OFF): Slow blink (1000ms = 0.5Hz) - normal
      // operation
      return 1000;
    }
  }
}

// Update LED Status
void ButtonManager::updateLEDStatus() {
  unsigned long interval = getStatusBlinkInterval();

  if (millis() - lastStatusBlinkMillis >= interval) {
    lastStatusBlinkMillis = millis();
    ledStatusState = !ledStatusState;
    digitalWrite(LED_STATUS, ledStatusState);
  }
}

// Stop button task
void ButtonManager::stop() {
  if (buttonTaskHandle) {
    vTaskDelete(buttonTaskHandle);
    buttonTaskHandle = nullptr;
    digitalWrite(LED_STATUS, LOW);
    LOG_LED_INFO("[BUTTON] Manager task stopped");
  }
}

// FreeRTOS task function
void ButtonManager::buttonTask(void* parameter) {
  ButtonManager* manager = static_cast<ButtonManager*>(parameter);
  manager->buttonLoop();
}

// Main loop for button task
void ButtonManager::buttonLoop() {
  while (true) {
    // Only process button if in production mode
    if (productionMode && button) {
      button->tick();  // Check button status
    }

    // Update LED Status (always active)
    updateLEDStatus();

    vTaskDelay(pdMS_TO_TICKS(10));  // Check every 10ms
  }
}

// Destructor - cleanup resources to prevent memory leaks
ButtonManager::~ButtonManager() {
  // Stop task first
  stop();

  // Delete mutex
  if (modeMutex) {
    vSemaphoreDelete(modeMutex);
    modeMutex = nullptr;
  }

  // Delete button instance
  if (button) {
    delete button;
    button = nullptr;
  }

  LOG_LED_INFO("[BUTTON] Manager destroyed, resources cleaned up");
}
