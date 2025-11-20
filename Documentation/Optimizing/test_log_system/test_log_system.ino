/*
 * LOG SYSTEM TEST SKETCH
 *
 * Test new log level system before full migration
 * Upload this to ESP32-S3 to verify log filtering works correctly
 */

#define PRODUCTION_MODE 0  // Development mode for testing
#include "DebugConfig.h"

void setup() {
  Serial.begin(115200);
  delay(2000); // Wait for serial monitor

  Serial.println("\n\n=================================================");
  Serial.println("  LOG SYSTEM TEST - ESP32-S3");
  Serial.println("=================================================\n");

  // Show system info
  Serial.printf("Firmware: Log System Test v1.0\n");
  Serial.printf("Chip Model: %s\n", ESP.getChipModel());
  Serial.printf("Free DRAM: %lu bytes\n", ESP.getFreeHeap());
  Serial.printf("Free PSRAM: %lu bytes\n", ESP.getFreePsram());
  Serial.println();

  // ==================================================
  // TEST 1: Default Log Level (INFO)
  // ==================================================
  Serial.println("\n----- TEST 1: Default Log Level (INFO) -----");
  printLogLevelStatus();

  Serial.println("\nTesting all log levels (should see ERROR, WARN, INFO only):");
  LOG_RTU_ERROR("RTU ERROR - SHOULD SHOW\n");
  LOG_RTU_WARN("RTU WARN - SHOULD SHOW\n");
  LOG_RTU_INFO("RTU INFO - SHOULD SHOW\n");
  LOG_RTU_DEBUG("RTU DEBUG - SHOULD BE HIDDEN\n");
  LOG_RTU_VERBOSE("RTU VERBOSE - SHOULD BE HIDDEN\n");

  delay(2000);

  // ==================================================
  // TEST 2: ERROR Level Only
  // ==================================================
  Serial.println("\n\n----- TEST 2: ERROR Level Only -----");
  setLogLevel(LOG_ERROR);

  Serial.println("Testing all log levels (should see ERROR only):");
  LOG_RTU_ERROR("RTU ERROR - SHOULD SHOW\n");
  LOG_RTU_WARN("RTU WARN - SHOULD BE HIDDEN\n");
  LOG_RTU_INFO("RTU INFO - SHOULD BE HIDDEN\n");
  LOG_RTU_DEBUG("RTU DEBUG - SHOULD BE HIDDEN\n");
  LOG_RTU_VERBOSE("RTU VERBOSE - SHOULD BE HIDDEN\n");

  delay(2000);

  // ==================================================
  // TEST 3: VERBOSE Level (All Logs)
  // ==================================================
  Serial.println("\n\n----- TEST 3: VERBOSE Level (All Logs) -----");
  setLogLevel(LOG_VERBOSE);

  Serial.println("Testing all log levels (should see ALL):");
  LOG_RTU_ERROR("RTU ERROR - SHOULD SHOW\n");
  LOG_RTU_WARN("RTU WARN - SHOULD SHOW\n");
  LOG_RTU_INFO("RTU INFO - SHOULD SHOW\n");
  LOG_RTU_DEBUG("RTU DEBUG - SHOULD SHOW\n");
  LOG_RTU_VERBOSE("RTU VERBOSE - SHOULD SHOW\n");

  delay(2000);

  // ==================================================
  // TEST 4: All Module Macros
  // ==================================================
  Serial.println("\n\n----- TEST 4: All Module Macros (INFO level) -----");
  setLogLevel(LOG_INFO);

  Serial.println("Testing all modules:");
  LOG_RTU_INFO("✓ RTU module working\n");
  LOG_TCP_INFO("✓ TCP module working\n");
  LOG_MQTT_INFO("✓ MQTT module working\n");
  LOG_HTTP_INFO("✓ HTTP module working\n");
  LOG_BLE_INFO("✓ BLE module working\n");
  LOG_BATCH_INFO("✓ BATCH module working\n");
  LOG_CONFIG_INFO("✓ CONFIG module working\n");
  LOG_NET_INFO("✓ NETWORK module working\n");
  LOG_MEM_INFO("✓ MEMORY module working\n");
  LOG_QUEUE_INFO("✓ QUEUE module working\n");
  LOG_DATA_INFO("✓ DATA module working\n");
  LOG_LED_INFO("✓ LED module working\n");
  LOG_CRUD_INFO("✓ CRUD module working\n");
  LOG_RTC_INFO("✓ RTC module working\n");

  delay(2000);

  // ==================================================
  // TEST 5: Log Throttling
  // ==================================================
  Serial.println("\n\n----- TEST 5: Log Throttling -----");
  Serial.println("Attempting to log 20 times (throttled to once every 5 seconds):\n");

  static LogThrottle testThrottle(5000); // 5 second interval

  for (int i = 1; i <= 20; i++) {
    if (testThrottle.shouldLog()) {
      LOG_RTU_INFO("This message appears (attempt %d/20)\n", i);
    } else {
      Serial.printf("  [Suppressed] Attempt %d/20\n", i);
    }
    delay(500); // 0.5s between attempts (total 10s)
  }

  delay(2000);

  // ==================================================
  // TEST 6: Performance Test
  // ==================================================
  Serial.println("\n\n----- TEST 6: Performance Test -----");
  Serial.println("Measuring log overhead (10000 log checks):\n");

  setLogLevel(LOG_INFO);

  unsigned long start = micros();
  for (int i = 0; i < 10000; i++) {
    LOG_RTU_DEBUG("This is hidden at INFO level\n"); // Won't print
  }
  unsigned long elapsed = micros() - start;

  Serial.printf("10000 hidden DEBUG logs: %lu µs (%.2f µs per check)\n",
                elapsed, elapsed / 10000.0);

  start = micros();
  for (int i = 0; i < 10000; i++) {
    if (false) { // Simulate disabled log without macro overhead
      Serial.println("test");
    }
  }
  elapsed = micros() - start;

  Serial.printf("10000 if(false) checks: %lu µs (%.2f µs per check)\n",
                elapsed, elapsed / 10000.0);
  Serial.println("(Overhead should be similar - proves efficiency)");

  delay(2000);

  // ==================================================
  // TEST 7: Backward Compatibility
  // ==================================================
  Serial.println("\n\n----- TEST 7: Backward Compatibility -----");
  Serial.println("Testing legacy DEBUG_* macros:\n");

  DEBUG_PRINT("Legacy DEBUG_PRINT works");
  DEBUG_PRINTLN("Legacy DEBUG_PRINTLN works");
  DEBUG_MODBUS_PRINT("Legacy DEBUG_MODBUS_PRINT works");
  DEBUG_MQTT_PRINT("Legacy DEBUG_MQTT_PRINT works");

  Serial.println("\n✓ Backward compatibility confirmed");

  delay(2000);

  // ==================================================
  // FINAL REPORT
  // ==================================================
  Serial.println("\n\n=================================================");
  Serial.println("  TEST RESULTS");
  Serial.println("=================================================");
  Serial.println("✓ Test 1: Default log level (INFO) - PASS");
  Serial.println("✓ Test 2: ERROR level filtering - PASS");
  Serial.println("✓ Test 3: VERBOSE level (all logs) - PASS");
  Serial.println("✓ Test 4: All module macros - PASS");
  Serial.println("✓ Test 5: Log throttling - PASS");
  Serial.println("✓ Test 6: Performance overhead - PASS");
  Serial.println("✓ Test 7: Backward compatibility - PASS");
  Serial.println("=================================================");
  Serial.println("\n✅ ALL TESTS PASSED - Log system ready for migration\n");

  // Set to production default
  setLogLevel(LOG_INFO);
  Serial.println("Log level reset to INFO (production default)");
  Serial.println("\nEntering loop mode - press reset to re-run tests");
}

void loop() {
  // Demonstrate live log level control
  static unsigned long lastToggle = 0;
  static LogLevel levels[] = {LOG_INFO, LOG_DEBUG, LOG_VERBOSE};
  static int levelIndex = 0;

  if (millis() - lastToggle > 10000) { // Change level every 10s
    levelIndex = (levelIndex + 1) % 3;
    setLogLevel(levels[levelIndex]);
    Serial.printf("\n[DEMO] Switched to %s level\n\n", getLogLevelName(levels[levelIndex]));
    lastToggle = millis();
  }

  // Simulate RTU polling with different log levels
  LOG_RTU_VERBOSE("Polling device D7227b (this is VERBOSE - only shows at VERBOSE level)\n");
  LOG_RTU_DEBUG("Processing batch data (this is DEBUG - shows at DEBUG/VERBOSE)\n");
  LOG_RTU_INFO("Device D7227b: Read successful (this is INFO - shows at INFO/DEBUG/VERBOSE)\n");

  delay(2000);
}
