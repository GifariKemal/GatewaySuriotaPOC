#ifndef NETWORK_HYSTERESIS_H
#define NETWORK_HYSTERESIS_H

#include <Arduino.h>
#include <cstdint>

/*
 * @brief Network Failover Hysteresis and Threshold Management
 *
 * Implements hysteresis logic to prevent rapid switching between networks
 * and maintains configurable signal quality thresholds for WiFi.
 *
 * Features:
 * - Hysteresis window to prevent flip-flop switching
 * - Signal quality thresholds (poor, fair, good, excellent)
 * - Debouncing of network state changes
 * - Threshold-based switching decisions
 * - Network state tracking with timestamps
 */

// WiFi signal quality levels
enum SignalQualityLevel
{
  SIGNAL_POOR = 0,     // < -80 dBm (very weak)
  SIGNAL_FAIR = 1,     // -80 to -70 dBm (weak)
  SIGNAL_GOOD = 2,     // -70 to -60 dBm (moderate)
  SIGNAL_EXCELLENT = 3 // > -60 dBm (strong)
};

// Network state with transitions
enum NetworkTransitionState
{
  TRANSITION_STABLE = 0,           // No pending transition
  TRANSITION_PRIMARY_PENDING = 1,  // Waiting to switch to primary
  TRANSITION_SECONDARY_PENDING = 2 // Waiting to switch to secondary
};

// Hysteresis configuration structure
struct HysteresisConfig
{
  // Hysteresis timing
  uint32_t hysteresisWindowMs = 10000;  // 10-second hysteresis window
  uint32_t stabilizationDelayMs = 3000; // 3-second stabilization before switch
  uint32_t minConnectionTimeMs = 5000;  // Minimum time on current network

  // Signal quality thresholds (RSSI in dBm, negative values)
  int8_t signalPoorThreshold = -80;      // Below -80 dBm is poor
  int8_t signalFairThreshold = -70;      // Below -70 dBm is fair
  int8_t signalGoodThreshold = -60;      // Below -60 dBm is good
  int8_t signalExcellentThreshold = -50; // -50 and above is excellent

  // Switching decision thresholds
  uint8_t primaryQualityMin = 2;        // Minimum signal quality to stay on primary (0=poor, 3=excellent)
  uint8_t secondaryQualityMin = 1;      // Minimum signal quality to use secondary
  uint8_t switchToSecondaryQuality = 1; // Switch if primary falls to this level

  // Enable/disable features
  bool enableHysteresis = true;              // Enable hysteresis window
  bool enableSignalQualityMonitoring = true; // Monitor signal quality
  bool enableDebouncing = true;              // Debounce state changes
};

// Network state with hysteresis tracking
struct NetworkStateWithHysteresis
{
  bool isActive;                           // Currently active/available
  unsigned long stateChangeTime = 0;       // When state last changed
  unsigned long lastSuccessfulUseTime = 0; // Last time successfully used
  uint8_t consecutiveFailureCount = 0;     // Consecutive failures
  uint8_t signalQuality = 0;               // WiFi signal quality (0-100%)
  int8_t rssi = 0;                         // WiFi RSSI in dBm
  NetworkTransitionState pendingTransition = TRANSITION_STABLE;
  unsigned long pendingTransitionTime = 0; // When transition was requested
};

class NetworkHysteresis
{
private:
  static NetworkHysteresis *instance;

  // Configuration
  HysteresisConfig config;

  // Primary network state
  NetworkStateWithHysteresis primaryNetworkState;

  // Secondary network state
  NetworkStateWithHysteresis secondaryNetworkState;

  // Current active network (for hysteresis checks)
  String currentActiveMode = "NONE";
  unsigned long modeChangeTime = 0;

  // Hysteresis tracking
  bool hysteresisActive = false;
  unsigned long hysteresisStartTime = 0;
  String hysteresisBlocking = ""; // Which mode is blocked by hysteresis

  // Debouncing
  uint8_t stateChangeCounter = 0;
  static constexpr uint8_t DEBOUNCE_THRESHOLD = 3; // Require 3 consecutive reads

  // Failure tracking
  uint8_t consecutiveSwitchFailures = 0;
  unsigned long lastSuccessfulSwitch = 0;

  // Private constructor for singleton
  NetworkHysteresis();

public:
  // Singleton access
  static NetworkHysteresis *getInstance();

  // Configuration methods
  void setHysteresisConfig(const HysteresisConfig &newConfig);
  void setHysteresisWindow(uint32_t windowMs);
  void setStabilizationDelay(uint32_t delayMs);
  void setMinConnectionTime(uint32_t timeMs);
  void setPrimaryQualityThreshold(uint8_t minQuality);
  void setSecondaryQualityThreshold(uint8_t minQuality);
  void setSwitchToSecondaryThreshold(uint8_t quality);

  // Signal quality mapping
  SignalQualityLevel getRSSIQualityLevel(int8_t rssi) const;
  uint8_t calculateQualityPercentage(int8_t rssi) const;
  const char *getQualityLevelString(SignalQualityLevel level) const;

  // Network state updates
  void updatePrimaryNetworkState(bool isAvailable, int8_t rssi = 0);
  void updateSecondaryNetworkState(bool isAvailable, int8_t rssi = 0);
  void recordNetworkSwitch(const String &fromMode, const String &toMode, bool successful);

  // Hysteresis decision making
  bool shouldSwitchToPrimary() const;
  bool shouldSwitchToSecondary() const;
  bool isHysteresisActive() const;
  bool isInHysteresisWindow() const;
  unsigned long getHysteresisTimeRemaining() const;

  // Debouncing
  void resetDebounceCounter();
  uint8_t getDebounceCounter() const;
  bool isStateChangeDebounced() const;

  // State queries
  const NetworkStateWithHysteresis &getPrimaryNetworkState() const;
  const NetworkStateWithHysteresis &getSecondaryNetworkState() const;
  String getCurrentActiveMode() const;
  unsigned long getCurrentModeUptime() const;

  // Transition management
  NetworkTransitionState getPendingTransition() const;
  bool hasPendingTransition() const;
  bool isPendingTransitionReady() const;
  void clearPendingTransition();

  // Reporting and diagnostics
  void printHysteresisStatus();
  void printNetworkStates();
  void printQualityThresholds();
  void printSwitchDecisionRationale();

  // Statistics
  uint32_t getPrimaryNetworkUptimeMs() const;
  uint32_t getSecondaryNetworkUptimeMs() const;
  uint32_t getTimeSinceLastSwitch() const;
  uint8_t getConsecutiveSwitchFailures() const;

  // Destructor
  ~NetworkHysteresis();
};

#endif // NETWORK_HYSTERESIS_H
