#include "NetworkHysteresis.h"
#include <Arduino.h>

// Singleton instance
NetworkHysteresis *NetworkHysteresis::instance = nullptr;

// Private constructor
NetworkHysteresis::NetworkHysteresis()
{
  Serial.println("[HYSTERESIS] Network Hysteresis initialized");
  currentActiveMode = "NONE";
  modeChangeTime = millis();
}

// Singleton access
NetworkHysteresis *NetworkHysteresis::getInstance()
{
  if (!instance)
  {
    instance = new NetworkHysteresis();
  }
  return instance;
}

// Configuration methods
void NetworkHysteresis::setHysteresisConfig(const HysteresisConfig &newConfig)
{
  config = newConfig;
  Serial.println("[HYSTERESIS] Configuration updated:");
  Serial.printf("  Hysteresis window: %u ms\n", config.hysteresisWindowMs);
  Serial.printf("  Stabilization delay: %u ms\n", config.stabilizationDelayMs);
  Serial.printf("  Min connection time: %u ms\n", config.minConnectionTimeMs);
}

void NetworkHysteresis::setHysteresisWindow(uint32_t windowMs)
{
  config.hysteresisWindowMs = windowMs;
  Serial.printf("[HYSTERESIS] Hysteresis window set to %u ms\n", windowMs);
}

void NetworkHysteresis::setStabilizationDelay(uint32_t delayMs)
{
  config.stabilizationDelayMs = delayMs;
  Serial.printf("[HYSTERESIS] Stabilization delay set to %u ms\n", delayMs);
}

void NetworkHysteresis::setMinConnectionTime(uint32_t timeMs)
{
  config.minConnectionTimeMs = timeMs;
  Serial.printf("[HYSTERESIS] Min connection time set to %u ms\n", timeMs);
}

void NetworkHysteresis::setPrimaryQualityThreshold(uint8_t minQuality)
{
  config.primaryQualityMin = minQuality;
  Serial.printf("[HYSTERESIS] Primary quality threshold set to %u\n", minQuality);
}

void NetworkHysteresis::setSecondaryQualityThreshold(uint8_t minQuality)
{
  config.secondaryQualityMin = minQuality;
  Serial.printf("[HYSTERESIS] Secondary quality threshold set to %u\n", minQuality);
}

void NetworkHysteresis::setSwitchToSecondaryThreshold(uint8_t quality)
{
  config.switchToSecondaryQuality = quality;
  Serial.printf("[HYSTERESIS] Switch to secondary threshold set to %u\n", quality);
}

// Signal quality mapping
SignalQualityLevel NetworkHysteresis::getRSSIQualityLevel(int8_t rssi) const
{
  if (rssi < config.signalPoorThreshold)
  {
    return SIGNAL_POOR;
  }
  else if (rssi < config.signalFairThreshold)
  {
    return SIGNAL_FAIR;
  }
  else if (rssi < config.signalGoodThreshold)
  {
    return SIGNAL_GOOD;
  }
  else
  {
    return SIGNAL_EXCELLENT;
  }
}

uint8_t NetworkHysteresis::calculateQualityPercentage(int8_t rssi) const
{
  // Map RSSI from -80 to -20 dBm to 0-100%
  // -80 dBm = 0%, -20 dBm = 100%
  const int8_t minRSSI = -80;
  const int8_t maxRSSI = -20;

  if (rssi <= minRSSI)
  {
    return 0;
  }
  else if (rssi >= maxRSSI)
  {
    return 100;
  }
  else
  {
    // Linear interpolation
    uint8_t percentage = ((rssi - minRSSI) * 100) / (maxRSSI - minRSSI);
    return percentage > 100 ? 100 : percentage;
  }
}

const char *NetworkHysteresis::getQualityLevelString(SignalQualityLevel level) const
{
  switch (level)
  {
  case SIGNAL_POOR:
    return "POOR (< -80 dBm)";
  case SIGNAL_FAIR:
    return "FAIR (-80 to -70 dBm)";
  case SIGNAL_GOOD:
    return "GOOD (-70 to -60 dBm)";
  case SIGNAL_EXCELLENT:
    return "EXCELLENT (> -60 dBm)";
  default:
    return "UNKNOWN";
  }
}

// Network state updates
void NetworkHysteresis::updatePrimaryNetworkState(bool isAvailable, int8_t rssi)
{
  unsigned long now = millis();

  // Update RSSI and quality if available
  if (isAvailable && rssi != 0)
  {
    primaryNetworkState.rssi = rssi;
    primaryNetworkState.signalQuality = calculateQualityPercentage(rssi);
  }

  // Check if state changed
  if (primaryNetworkState.isActive != isAvailable)
  {
    primaryNetworkState.isActive = isAvailable;
    primaryNetworkState.stateChangeTime = now;

    if (isAvailable)
    {
      primaryNetworkState.lastSuccessfulUseTime = now;
      primaryNetworkState.consecutiveFailureCount = 0;
      Serial.printf("[HYSTERESIS] Primary network became AVAILABLE (RSSI: %d dBm, Quality: %u%%)\n",
                    rssi, primaryNetworkState.signalQuality);
    }
    else
    {
      primaryNetworkState.consecutiveFailureCount++;
      Serial.printf("[HYSTERESIS] Primary network became UNAVAILABLE (failure count: %u)\n",
                    primaryNetworkState.consecutiveFailureCount);
    }
  }

  // Update signal quality even if state didn't change
  if (config.enableSignalQualityMonitoring && isAvailable && rssi != 0)
  {
    uint8_t newQuality = calculateQualityPercentage(rssi);
    if (newQuality != primaryNetworkState.signalQuality)
    {
      Serial.printf("[HYSTERESIS] Primary signal quality changed: %u%% -> %u%% (RSSI: %d dBm)\n",
                    primaryNetworkState.signalQuality, newQuality, rssi);
      primaryNetworkState.signalQuality = newQuality;
    }
  }
}

void NetworkHysteresis::updateSecondaryNetworkState(bool isAvailable, int8_t rssi)
{
  unsigned long now = millis();

  // Update RSSI and quality if available
  if (isAvailable && rssi != 0)
  {
    secondaryNetworkState.rssi = rssi;
    secondaryNetworkState.signalQuality = calculateQualityPercentage(rssi);
  }

  // Check if state changed
  if (secondaryNetworkState.isActive != isAvailable)
  {
    secondaryNetworkState.isActive = isAvailable;
    secondaryNetworkState.stateChangeTime = now;

    if (isAvailable)
    {
      secondaryNetworkState.lastSuccessfulUseTime = now;
      secondaryNetworkState.consecutiveFailureCount = 0;
      Serial.printf("[HYSTERESIS] Secondary network became AVAILABLE (RSSI: %d dBm, Quality: %u%%)\n",
                    rssi, secondaryNetworkState.signalQuality);
    }
    else
    {
      secondaryNetworkState.consecutiveFailureCount++;
      Serial.printf("[HYSTERESIS] Secondary network became UNAVAILABLE (failure count: %u)\n",
                    secondaryNetworkState.consecutiveFailureCount);
    }
  }

  // Update signal quality even if state didn't change
  if (config.enableSignalQualityMonitoring && isAvailable && rssi != 0)
  {
    uint8_t newQuality = calculateQualityPercentage(rssi);
    if (newQuality != secondaryNetworkState.signalQuality)
    {
      Serial.printf("[HYSTERESIS] Secondary signal quality changed: %u%% -> %u%% (RSSI: %d dBm)\n",
                    secondaryNetworkState.signalQuality, newQuality, rssi);
      secondaryNetworkState.signalQuality = newQuality;
    }
  }
}

void NetworkHysteresis::recordNetworkSwitch(const String &fromMode, const String &toMode, bool successful)
{
  unsigned long now = millis();

  if (successful)
  {
    currentActiveMode = toMode;
    modeChangeTime = now;
    Serial.printf("[HYSTERESIS] Successfully switched from %s to %s\n", fromMode.c_str(), toMode.c_str());
  }
  else
  {
    Serial.printf("[HYSTERESIS] Failed to switch from %s to %s\n", fromMode.c_str(), toMode.c_str());
  }
}

// Hysteresis decision making
bool NetworkHysteresis::shouldSwitchToPrimary() const
{
  // Don't switch if primary is not available
  if (!primaryNetworkState.isActive)
  {
    return false;
  }

  // Check if hysteresis is still blocking
  if (isInHysteresisWindow())
  {
    return false;
  }

  // Check if minimum connection time has elapsed on current network
  unsigned long timeOnCurrentNetwork = millis() - modeChangeTime;
  if (timeOnCurrentNetwork < config.minConnectionTimeMs)
  {
    return false;
  }

  // Check signal quality threshold
  if (config.enableSignalQualityMonitoring)
  {
    SignalQualityLevel primaryQuality = getRSSIQualityLevel(primaryNetworkState.rssi);
    if ((uint8_t)primaryQuality < config.primaryQualityMin)
    {
      return false;
    }
  }

  // Check if secondary is currently active and needs switching
  if (currentActiveMode == "SECONDARY" || currentActiveMode == "NONE")
  {
    // Only switch if secondary is worse or unavailable
    if (secondaryNetworkState.isActive && config.enableSignalQualityMonitoring)
    {
      SignalQualityLevel secondaryQuality = getRSSIQualityLevel(secondaryNetworkState.rssi);
      SignalQualityLevel primaryQuality = getRSSIQualityLevel(primaryNetworkState.rssi);

      // Only switch if primary is significantly better
      if ((uint8_t)primaryQuality <= (uint8_t)secondaryQuality)
      {
        return false;
      }
    }
  }

  return true;
}

bool NetworkHysteresis::shouldSwitchToSecondary() const
{
  // Don't switch if secondary is not available
  if (!secondaryNetworkState.isActive)
  {
    return false;
  }

  // Check if hysteresis is still blocking
  if (isInHysteresisWindow())
  {
    return false;
  }

  // Check if minimum connection time has elapsed on current network
  unsigned long timeOnCurrentNetwork = millis() - modeChangeTime;
  if (timeOnCurrentNetwork < config.minConnectionTimeMs)
  {
    return false;
  }

  // Check if primary is unavailable or poor quality
  if (!primaryNetworkState.isActive)
  {
    return true;
  }

  if (config.enableSignalQualityMonitoring)
  {
    SignalQualityLevel primaryQuality = getRSSIQualityLevel(primaryNetworkState.rssi);

    // Switch to secondary if primary falls below threshold
    if ((uint8_t)primaryQuality <= config.switchToSecondaryQuality)
    {
      // Verify secondary meets minimum quality
      SignalQualityLevel secondaryQuality = getRSSIQualityLevel(secondaryNetworkState.rssi);
      if ((uint8_t)secondaryQuality >= config.secondaryQualityMin)
      {
        return true;
      }
    }
  }

  return false;
}

bool NetworkHysteresis::isHysteresisActive() const
{
  return hysteresisActive;
}

bool NetworkHysteresis::isInHysteresisWindow() const
{
  if (!hysteresisActive || !config.enableHysteresis)
  {
    return false;
  }

  unsigned long elapsed = millis() - hysteresisStartTime;
  return elapsed < config.hysteresisWindowMs;
}

unsigned long NetworkHysteresis::getHysteresisTimeRemaining() const
{
  if (!isInHysteresisWindow())
  {
    return 0;
  }

  unsigned long elapsed = millis() - hysteresisStartTime;
  unsigned long remaining = config.hysteresisWindowMs - elapsed;
  return remaining > 0 ? remaining : 0;
}

// Debouncing
void NetworkHysteresis::resetDebounceCounter()
{
  stateChangeCounter = 0;
}

uint8_t NetworkHysteresis::getDebounceCounter() const
{
  return stateChangeCounter;
}

bool NetworkHysteresis::isStateChangeDebounced() const
{
  return stateChangeCounter >= DEBOUNCE_THRESHOLD;
}

// State queries
const NetworkStateWithHysteresis &NetworkHysteresis::getPrimaryNetworkState() const
{
  return primaryNetworkState;
}

const NetworkStateWithHysteresis &NetworkHysteresis::getSecondaryNetworkState() const
{
  return secondaryNetworkState;
}

String NetworkHysteresis::getCurrentActiveMode() const
{
  return currentActiveMode;
}

unsigned long NetworkHysteresis::getCurrentModeUptime() const
{
  return millis() - modeChangeTime;
}

// Transition management
NetworkTransitionState NetworkHysteresis::getPendingTransition() const
{
  return primaryNetworkState.pendingTransition;
}

bool NetworkHysteresis::hasPendingTransition() const
{
  return primaryNetworkState.pendingTransition != TRANSITION_STABLE ||
         secondaryNetworkState.pendingTransition != TRANSITION_STABLE;
}

bool NetworkHysteresis::isPendingTransitionReady() const
{
  if (!hasPendingTransition())
  {
    return false;
  }

  unsigned long now = millis();

  // Check if stabilization delay has elapsed
  if (primaryNetworkState.pendingTransition != TRANSITION_STABLE)
  {
    unsigned long elapsed = now - primaryNetworkState.pendingTransitionTime;
    if (elapsed >= config.stabilizationDelayMs)
    {
      return true;
    }
  }

  if (secondaryNetworkState.pendingTransition != TRANSITION_STABLE)
  {
    unsigned long elapsed = now - secondaryNetworkState.pendingTransitionTime;
    if (elapsed >= config.stabilizationDelayMs)
    {
      return true;
    }
  }

  return false;
}

void NetworkHysteresis::clearPendingTransition()
{
  primaryNetworkState.pendingTransition = TRANSITION_STABLE;
  primaryNetworkState.pendingTransitionTime = 0;
  secondaryNetworkState.pendingTransition = TRANSITION_STABLE;
  secondaryNetworkState.pendingTransitionTime = 0;

  hysteresisActive = true;
  hysteresisStartTime = millis();
  Serial.printf("[HYSTERESIS] Hysteresis window activated for %u ms\n", config.hysteresisWindowMs);
}

// Reporting and diagnostics
void NetworkHysteresis::printHysteresisStatus()
{
  Serial.println("\n[HYSTERESIS] ═══════════════════════════════════════════════");
  Serial.printf("[HYSTERESIS] Current Active Mode: %s\n", currentActiveMode.c_str());
  Serial.printf("[HYSTERESIS] Uptime on current mode: %lu ms\n", getCurrentModeUptime());

  if (isHysteresisActive())
  {
    unsigned long remaining = getHysteresisTimeRemaining();
    Serial.printf("[HYSTERESIS] Hysteresis ACTIVE (%.1f seconds remaining)\n",
                  remaining / 1000.0f);
  }
  else
  {
    Serial.println("[HYSTERESIS] Hysteresis inactive");
  }

  Serial.println("[HYSTERESIS] ═══════════════════════════════════════════════\n");
}

void NetworkHysteresis::printNetworkStates()
{
  Serial.println("\n[HYSTERESIS] ─── PRIMARY NETWORK ───");
  Serial.printf("  Status: %s\n", primaryNetworkState.isActive ? "ACTIVE" : "INACTIVE");
  Serial.printf("  RSSI: %d dBm\n", primaryNetworkState.rssi);
  Serial.printf("  Quality: %u%% (%s)\n", primaryNetworkState.signalQuality,
                getQualityLevelString(getRSSIQualityLevel(primaryNetworkState.rssi)));
  Serial.printf("  State changes: %lu ms ago\n", millis() - primaryNetworkState.stateChangeTime);
  Serial.printf("  Consecutive failures: %u\n", primaryNetworkState.consecutiveFailureCount);

  Serial.println("[HYSTERESIS] ─── SECONDARY NETWORK ───");
  Serial.printf("  Status: %s\n", secondaryNetworkState.isActive ? "ACTIVE" : "INACTIVE");
  Serial.printf("  RSSI: %d dBm\n", secondaryNetworkState.rssi);
  Serial.printf("  Quality: %u%% (%s)\n", secondaryNetworkState.signalQuality,
                getQualityLevelString(getRSSIQualityLevel(secondaryNetworkState.rssi)));
  Serial.printf("  State changes: %lu ms ago\n", millis() - secondaryNetworkState.stateChangeTime);
  Serial.printf("  Consecutive failures: %u\n", secondaryNetworkState.consecutiveFailureCount);
  Serial.println("[HYSTERESIS] ─────────────────────────────\n");
}

void NetworkHysteresis::printQualityThresholds()
{
  Serial.println("\n[HYSTERESIS] ─── QUALITY THRESHOLDS ───");
  Serial.printf("  RSSI Thresholds:\n");
  Serial.printf("    Poor: < %d dBm\n", config.signalPoorThreshold);
  Serial.printf("    Fair: %d to %d dBm\n", config.signalPoorThreshold, config.signalFairThreshold);
  Serial.printf("    Good: %d to %d dBm\n", config.signalFairThreshold, config.signalGoodThreshold);
  Serial.printf("    Excellent: > %d dBm\n", config.signalGoodThreshold);

  Serial.printf("  Switching Thresholds:\n");
  Serial.printf("    Primary minimum: %u (0=poor, 3=excellent)\n", config.primaryQualityMin);
  Serial.printf("    Secondary minimum: %u (0=poor, 3=excellent)\n", config.secondaryQualityMin);
  Serial.printf("    Switch to secondary at: %u (0=poor, 3=excellent)\n", config.switchToSecondaryQuality);
  Serial.println("[HYSTERESIS] ─────────────────────────────\n");
}

void NetworkHysteresis::printSwitchDecisionRationale()
{
  Serial.println("\n[HYSTERESIS] ─── SWITCH DECISION ANALYSIS ───");

  bool canSwitchToPrimary = shouldSwitchToPrimary();
  bool canSwitchToSecondary = shouldSwitchToSecondary();

  Serial.printf("  Should switch to PRIMARY: %s\n", canSwitchToPrimary ? "YES" : "NO");
  if (!canSwitchToPrimary)
  {
    if (!primaryNetworkState.isActive)
    {
      Serial.println("    to Primary network is unavailable");
    }
    else if (isInHysteresisWindow())
    {
      Serial.printf("    to Hysteresis window active (%.1f sec remaining)\n",
                    getHysteresisTimeRemaining() / 1000.0f);
    }
    else
    {
      unsigned long timeOnCurrent = millis() - modeChangeTime;
      if (timeOnCurrent < config.minConnectionTimeMs)
      {
        Serial.printf("    to Min connection time not met (%.1f / %.1f sec)\n",
                      timeOnCurrent / 1000.0f, config.minConnectionTimeMs / 1000.0f);
      }
      if (config.enableSignalQualityMonitoring)
      {
        SignalQualityLevel primaryQuality = getRSSIQualityLevel(primaryNetworkState.rssi);
        if ((uint8_t)primaryQuality < config.primaryQualityMin)
        {
          Serial.printf("    to Primary signal quality below threshold (%u < %u)\n",
                        (uint8_t)primaryQuality, config.primaryQualityMin);
        }
      }
    }
  }

  Serial.printf("  Should switch to SECONDARY: %s\n", canSwitchToSecondary ? "YES" : "NO");
  if (!canSwitchToSecondary)
  {
    if (!secondaryNetworkState.isActive)
    {
      Serial.println("    to Secondary network is unavailable");
    }
    else if (isInHysteresisWindow())
    {
      Serial.printf("    to Hysteresis window active (%.1f sec remaining)\n",
                    getHysteresisTimeRemaining() / 1000.0f);
    }
    else if (primaryNetworkState.isActive && config.enableSignalQualityMonitoring)
    {
      SignalQualityLevel primaryQuality = getRSSIQualityLevel(primaryNetworkState.rssi);
      if ((uint8_t)primaryQuality > config.switchToSecondaryQuality)
      {
        Serial.printf("    to Primary signal quality acceptable (%u > %u)\n",
                      (uint8_t)primaryQuality, config.switchToSecondaryQuality);
      }
    }
  }
  Serial.println("[HYSTERESIS] ─────────────────────────────\n");
}

// Statistics
uint32_t NetworkHysteresis::getPrimaryNetworkUptimeMs() const
{
  return millis() - primaryNetworkState.stateChangeTime;
}

uint32_t NetworkHysteresis::getSecondaryNetworkUptimeMs() const
{
  return millis() - secondaryNetworkState.stateChangeTime;
}

uint32_t NetworkHysteresis::getTimeSinceLastSwitch() const
{
  return millis() - modeChangeTime;
}

uint8_t NetworkHysteresis::getConsecutiveSwitchFailures() const
{
  return (primaryNetworkState.consecutiveFailureCount + secondaryNetworkState.consecutiveFailureCount);
}

// Destructor
NetworkHysteresis::~NetworkHysteresis()
{
  Serial.println("[HYSTERESIS] Network Hysteresis destroyed");
}
