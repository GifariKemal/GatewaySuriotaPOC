#include "BLEMetricsWindow.h"

// Singleton instance
BLEMetricsCollector *BLEMetricsCollector::instance = nullptr;

// Constructor
BLEMetricsCollector::BLEMetricsCollector()
{
  lastAggregationTime = millis();
  Serial.println("[BLE_METRICS] Initialized - Rolling window metrics system ready");
}

// Singleton access
BLEMetricsCollector *BLEMetricsCollector::getInstance()
{
  // Thread-safe Meyers Singleton (C++11 guarantees thread-safe static init)
  static BLEMetricsCollector instance;
  static BLEMetricsCollector *ptr = &instance;
  return ptr;
}

// Metrics recording
void BLEMetricsCollector::recordOperation(bool success, uint32_t latencyMs,
                                          int8_t rssiDbm, uint16_t mtuSize,
                                          uint32_t bytesTransferred)
{
  BLEOperationMetric metric;
  metric.timestamp = millis();
  metric.latencyMs = latencyMs;
  metric.success = success;
  metric.rssiDbm = rssiDbm;
  metric.mtuSize = mtuSize;
  metric.bytesTransferred = bytesTransferred;

  allMetrics.push_back(metric);
  totalMetricsCollected++;

  // Trigger aggregation if interval elapsed
  if ((millis() - lastAggregationTime) >= aggregationIntervalMs)
  {
    aggregateMetrics();
    lastAggregationTime = millis();
  }

  // Prune old metrics
  pruneOldMetrics();
}

void BLEMetricsCollector::recordConnectionAttempt(bool success, uint32_t latencyMs)
{
  recordOperation(success, latencyMs);
}

void BLEMetricsCollector::recordDisconnection(int8_t reasonCode)
{
  (void)reasonCode; // Unused parameter
  recordOperation(true, 0);
}

void BLEMetricsCollector::recordMtuNegotiation(bool success, uint32_t latencyMs, uint16_t finalMtu)
{
  recordOperation(success, latencyMs, 0, finalMtu);
}

void BLEMetricsCollector::recordDataTransfer(uint32_t bytesTransferred, uint32_t latencyMs)
{
  recordOperation(true, latencyMs, 0, 0, bytesTransferred);
}

void BLEMetricsCollector::recordSignalStrength(int8_t rssiDbm)
{
  recordOperation(true, 0, rssiDbm);
}

// Metrics retrieval
WindowedMetrics BLEMetricsCollector::getMetrics1Min() const
{
  return metrics1Min;
}

WindowedMetrics BLEMetricsCollector::getMetrics5Min() const
{
  return metrics5Min;
}

WindowedMetrics BLEMetricsCollector::getMetrics15Min() const
{
  return metrics15Min;
}

WindowedMetrics BLEMetricsCollector::getMetricsAllTime() const
{
  WindowedMetrics allTime;
  allTime.windowDurationMs = 0xFFFFFFFF; // Infinite

  if (allMetrics.empty())
  {
    return allTime;
  }

  allTime.totalOperations = allMetrics.size();

  uint32_t successCount = 0;
  uint32_t totalLatency = 0;
  uint32_t latencyCount = 0;
  uint32_t totalBytes = 0;
  int32_t totalRssi = 0;
  uint32_t rssiCount = 0;

  std::vector<uint32_t> latencies;

  for (const auto &metric : allMetrics)
  {
    if (metric.success)
      successCount++;

    if (metric.latencyMs > 0)
    {
      totalLatency += metric.latencyMs;
      latencyCount++;
      latencies.push_back(metric.latencyMs);

      if (metric.latencyMs < allTime.minLatencyMs)
      {
        allTime.minLatencyMs = metric.latencyMs;
      }
      if (metric.latencyMs > allTime.maxLatencyMs)
      {
        allTime.maxLatencyMs = metric.latencyMs;
      }
    }

    if (metric.rssiDbm != 0)
    {
      totalRssi += metric.rssiDbm;
      rssiCount++;
    }

    totalBytes += metric.bytesTransferred;
  }

  if (latencyCount > 0)
  {
    allTime.avgLatencyMs = totalLatency / latencyCount;
    calculatePercentiles(latencies, allTime.p95LatencyMs, allTime.p99LatencyMs);
  }

  if (rssiCount > 0)
  {
    allTime.avgRssiDbm = totalRssi / rssiCount;
  }

  allTime.successfulOperations = successCount;
  allTime.failedOperations = allTime.totalOperations - successCount;
  allTime.successRate = (successCount * 100.0f) / allTime.totalOperations;
  allTime.totalBytesTransferred = totalBytes;

  return allTime;
}

// Trend analysis
TrendInfo BLEMetricsCollector::getTrends() const
{
  return latestTrend;
}

TrendInfo::Trend BLEMetricsCollector::getLatencyTrend() const
{
  return latestTrend.latencyTrend;
}

TrendInfo::Trend BLEMetricsCollector::getThroughputTrend() const
{
  return latestTrend.throughputTrend;
}

TrendInfo::Trend BLEMetricsCollector::getSignalTrend() const
{
  return latestTrend.signalTrend;
}

TrendInfo::Trend BLEMetricsCollector::getSuccessRateTrend() const
{
  return latestTrend.successRateTrend;
}

// Health assessment
bool BLEMetricsCollector::isHealthy() const
{
  return hasGoodLatency() && hasGoodSignal() && getHealthScore() >= 70;
}

bool BLEMetricsCollector::hasGoodSignal() const
{
  return metrics1Min.avgRssiDbm >= config.goodRssiDbm ||
         metrics5Min.avgRssiDbm >= config.goodRssiDbm;
}

bool BLEMetricsCollector::hasGoodLatency() const
{
  return metrics1Min.avgLatencyMs <= config.goodLatencyMs ||
         metrics5Min.avgLatencyMs <= config.goodLatencyMs;
}

bool BLEMetricsCollector::hasGoodThroughput() const
{
  return metrics1Min.avgThroughputBytesPerSec > 100.0f ||
         metrics5Min.avgThroughputBytesPerSec > 100.0f;
}

uint32_t BLEMetricsCollector::getHealthScore() const
{
  uint32_t score = 100;

  // Latency impact
  if (metrics1Min.avgLatencyMs > config.warningLatencyMs)
  {
    score -= 20;
  }
  else if (metrics1Min.avgLatencyMs > config.goodLatencyMs)
  {
    score -= 10;
  }

  // Signal impact
  if (metrics1Min.avgRssiDbm < config.warningRssiDbm)
  {
    score -= 20;
  }
  else if (metrics1Min.avgRssiDbm < config.goodRssiDbm)
  {
    score -= 10;
  }

  // Success rate impact
  if (metrics1Min.successRate < config.minAcceptableSuccessRate)
  {
    score -= 30;
  }

  return (score > 0) ? score : 0;
}

String BLEMetricsCollector::getHealthAssessment() const
{
  uint32_t score = getHealthScore();

  if (score >= 90)
    return "EXCELLENT";
  if (score >= 80)
    return "GOOD";
  if (score >= 70)
    return "FAIR";
  if (score >= 50)
    return "POOR";
  return "CRITICAL";
}

String BLEMetricsCollector::getPerformanceReport() const
{
  String report = "BLE PERFORMANCE REPORT\n";
  report += "1-Min: Latency=" + String(metrics1Min.avgLatencyMs) + "ms, ";
  report += "Success=" + String(metrics1Min.successRate, 1) + "%, ";
  report += "RSSI=" + String(metrics1Min.avgRssiDbm) + "dBm\n";

  report += "5-Min: Latency=" + String(metrics5Min.avgLatencyMs) + "ms, ";
  report += "Success=" + String(metrics5Min.successRate, 1) + "%, ";
  report += "RSSI=" + String(metrics5Min.avgRssiDbm) + "dBm\n";

  report += "Status: " + getHealthAssessment();

  return report;
}

bool BLEMetricsCollector::isLikelyToSucceed() const
{
  // Based on recent success rate and signal quality
  bool goodSuccessRate = metrics1Min.successRate >= config.minAcceptableSuccessRate;
  bool goodSignal = metrics1Min.avgRssiDbm >= config.warningRssiDbm;
  bool goodLatency = metrics1Min.avgLatencyMs <= config.warningLatencyMs;

  return goodSuccessRate && goodSignal && goodLatency;
}

// Configuration
void BLEMetricsCollector::setConfig(const MetricsConfig &newConfig)
{
  config = newConfig;
}

void BLEMetricsCollector::setWindowSizes(uint32_t window1, uint32_t window5, uint32_t window15)
{
  config.window1MinMs = window1;
  config.window5MinMs = window5;
  config.window15MinMs = window15;
}

void BLEMetricsCollector::setMaxMetricsPerWindow(uint16_t maxCount)
{
  config.maxMetricsPerWindow = maxCount;
}

void BLEMetricsCollector::setLatencyThresholds(uint32_t goodMs, uint32_t warningMs)
{
  config.goodLatencyMs = goodMs;
  config.warningLatencyMs = warningMs;
}

void BLEMetricsCollector::setSignalThresholds(int8_t goodDbm, int8_t warningDbm)
{
  config.goodRssiDbm = goodDbm;
  config.warningRssiDbm = warningDbm;
}

void BLEMetricsCollector::setMinSuccessRate(float minPercent)
{
  config.minAcceptableSuccessRate = minPercent;
}

// Utility methods
uint32_t BLEMetricsCollector::getMetricsCount() const
{
  return allMetrics.size();
}

uint64_t BLEMetricsCollector::getTotalMetricsCollected() const
{
  return totalMetricsCollected;
}

void BLEMetricsCollector::clearMetrics()
{
  allMetrics.clear();
  metrics1Min = WindowedMetrics();
  metrics5Min = WindowedMetrics();
  metrics15Min = WindowedMetrics();
  Serial.println("[BLE_METRICS] Metrics cleared");
}

void BLEMetricsCollector::resetTrends()
{
  latestTrend = TrendInfo();
}

// Diagnostics
void BLEMetricsCollector::printMetrics1Min()
{
  Serial.println("\n[BLE METRICS] 1 Minute Window");
  Serial.printf("  Operations: %ld successful, %ld failed (%.1f%% success rate)\n",
                metrics1Min.successfulOperations,
                metrics1Min.failedOperations,
                metrics1Min.successRate);
  Serial.printf("  Latency: Min=%ldms, Avg=%ldms, Max=%ldms, P95=%ldms, P99=%ldms\n",
                metrics1Min.minLatencyMs,
                metrics1Min.avgLatencyMs,
                metrics1Min.maxLatencyMs,
                metrics1Min.p95LatencyMs,
                metrics1Min.p99LatencyMs);
  Serial.printf("  Signal: Avg=%ddBm, Min=%ddBm, Max=%ddBm (%ld%% quality)\n",
                metrics1Min.avgRssiDbm,
                metrics1Min.minRssiDbm,
                metrics1Min.maxRssiDbm,
                metrics1Min.signalQualityPercent);
  Serial.printf("  Throughput: Avg=%.2f B/s, Peak=%.2f B/s\n",
                metrics1Min.avgThroughputBytesPerSec,
                metrics1Min.peakThroughputBytesPerSec);
  Serial.printf("  MTU: %d bytes (optimized: %s)\n\n",
                metrics1Min.avgMtuSize,
                metrics1Min.mtuOptimized ? "YES" : "NO");
}

void BLEMetricsCollector::printMetrics5Min()
{
  Serial.println("\n[BLE METRICS] 5 Minutes Window");
  Serial.printf("  Operations: %ld successful, %ld failed (%.1f%% success rate)\n",
                metrics5Min.successfulOperations,
                metrics5Min.failedOperations,
                metrics5Min.successRate);
  Serial.printf("  Latency: Avg=%ldms, P95=%ldms, P99=%ldms\n",
                metrics5Min.avgLatencyMs,
                metrics5Min.p95LatencyMs,
                metrics5Min.p99LatencyMs);
  Serial.printf("  Signal: Avg=%ddBm (%ld%% quality)\n\n",
                metrics5Min.avgRssiDbm,
                metrics5Min.signalQualityPercent);
}

void BLEMetricsCollector::printMetrics15Min()
{
  Serial.println("\n[BLE METRICS] 15 Minutes Window");
  Serial.printf("  Operations: %ld successful, %ld failed (%.1f%% success rate)\n",
                metrics15Min.successfulOperations,
                metrics15Min.failedOperations,
                metrics15Min.successRate);
  Serial.printf("  Latency: Avg=%ldms, P95=%ldms\n\n",
                metrics15Min.avgLatencyMs,
                metrics15Min.p95LatencyMs);
}

void BLEMetricsCollector::printTrendAnalysis()
{
  Serial.println("\n[BLE METRICS] TREND ANALYSIS");

  Serial.printf("  Latency Trend: ");
  switch (latestTrend.latencyTrend)
  {
  case TrendInfo::TREND_IMPROVING:
    Serial.printf("IMPROVING (%.1f%% change)\n", latestTrend.latencyChangePercent);
    break;
  case TrendInfo::TREND_DEGRADING:
    Serial.printf("DEGRADING (%.1f%% change)\n", latestTrend.latencyChangePercent);
    break;
  case TrendInfo::TREND_STABLE:
    Serial.println("STABLE");
    break;
  default:
    Serial.println("? UNKNOWN");
    break;
  }

  Serial.printf("  Throughput Trend: ");
  switch (latestTrend.throughputTrend)
  {
  case TrendInfo::TREND_IMPROVING:
    Serial.printf("IMPROVING (%.1f%% change)\n", latestTrend.throughputChangePercent);
    break;
  case TrendInfo::TREND_DEGRADING:
    Serial.printf("DEGRADING (%.1f%% change)\n", latestTrend.throughputChangePercent);
    break;
  case TrendInfo::TREND_STABLE:
    Serial.println("STABLE");
    break;
  default:
    Serial.println("? UNKNOWN");
    break;
  }

  Serial.printf("  Signal Trend: ");
  switch (latestTrend.signalTrend)
  {
  case TrendInfo::TREND_IMPROVING:
    Serial.printf("IMPROVING (%.1f dBm change)\n", latestTrend.signalChangeDbm);
    break;
  case TrendInfo::TREND_DEGRADING:
    Serial.printf("DEGRADING (%.1f dBm change)\n", latestTrend.signalChangeDbm);
    break;
  case TrendInfo::TREND_STABLE:
    Serial.println("STABLE");
    break;
  default:
    Serial.println("? UNKNOWN");
    break;
  }

  Serial.printf("  Success Rate Trend: ");
  switch (latestTrend.successRateTrend)
  {
  case TrendInfo::TREND_IMPROVING:
    Serial.printf("IMPROVING (%.1f%% change)\n", latestTrend.successRateChangePercent);
    break;
  case TrendInfo::TREND_DEGRADING:
    Serial.printf("DEGRADING (%.1f%% change)\n", latestTrend.successRateChangePercent);
    break;
  case TrendInfo::TREND_STABLE:
    Serial.println("STABLE");
    break;
  default:
    Serial.println("? UNKNOWN");
    break;
  }
  Serial.println();
}

void BLEMetricsCollector::printDetailedReport()
{
  Serial.println("\n[BLE METRICS] DETAILED REPORT\n");

  printMetrics1Min();
  printMetrics5Min();
  printMetrics15Min();
  printTrendAnalysis();
  printHealthScore();
}

void BLEMetricsCollector::printHealthScore()
{
  uint32_t score = getHealthScore();
  Serial.printf("\n[BLE HEALTH] Score: %ld/100 (%s)\n", score, getHealthAssessment().c_str());

  // Draw simple progress bar
  Serial.print("  Progress: [");
  for (int i = 0; i < 20; i++)
  {
    if (i < (score / 5))
    {
      Serial.print("#");
    }
    else
    {
      Serial.print(" ");
    }
  }
  Serial.println("]");

  if (isHealthy())
  {
    Serial.println("  Status: HEALTHY\n");
  }
  else
  {
    Serial.println("  Status: NEEDS ATTENTION\n");
  }
}

void BLEMetricsCollector::printWindowComparison()
{
  Serial.println("\n[BLE METRICS] WINDOW COMPARISON");
  Serial.printf("           1-Min    5-Min    15-Min\n");
  Serial.printf("  Success: %.1f%%    %.1f%%    %.1f%%\n",
                metrics1Min.successRate,
                metrics5Min.successRate,
                metrics15Min.successRate);
  Serial.printf("  Latency: %ldms    %ldms    %ldms\n",
                metrics1Min.avgLatencyMs,
                metrics5Min.avgLatencyMs,
                metrics15Min.avgLatencyMs);
  Serial.printf("  Signal:  %ddBm    %ddBm    %ddBm\n\n",
                metrics1Min.avgRssiDbm,
                metrics5Min.avgRssiDbm,
                metrics15Min.avgRssiDbm);
}

// Private helper methods
void BLEMetricsCollector::aggregateMetrics()
{
  unsigned long now = millis();

  // Recalculate all windows
  calculateWindowMetrics(metrics1Min, config.window1MinMs);
  calculateWindowMetrics(metrics5Min, config.window5MinMs);
  calculateWindowMetrics(metrics15Min, config.window15MinMs);

  // Analyze trends
  analyzeTrends();
}

void BLEMetricsCollector::calculateWindowMetrics(WindowedMetrics &window, uint32_t windowMs)
{
  unsigned long now = millis();
  window.windowStartTime = now - windowMs;
  window.windowEndTime = now;
  window.windowDurationMs = windowMs;

  uint32_t successCount = 0;
  uint32_t totalLatency = 0;
  uint32_t latencyCount = 0;
  uint32_t totalBytes = 0;
  int32_t totalRssi = 0;
  uint32_t rssiCount = 0;
  uint32_t mtuCount = 0;
  uint32_t totalMtu = 0;

  std::vector<uint32_t> latencies;
  window.totalOperations = 0;
  window.minLatencyMs = UINT32_MAX;
  window.maxLatencyMs = 0;

  for (const auto &metric : allMetrics)
  {
    if (metric.timestamp >= window.windowStartTime)
    {
      window.totalOperations++;

      if (metric.success)
        successCount++;

      if (metric.latencyMs > 0)
      {
        totalLatency += metric.latencyMs;
        latencyCount++;
        latencies.push_back(metric.latencyMs);

        if (metric.latencyMs < window.minLatencyMs)
        {
          window.minLatencyMs = metric.latencyMs;
        }
        if (metric.latencyMs > window.maxLatencyMs)
        {
          window.maxLatencyMs = metric.latencyMs;
        }
      }

      if (metric.rssiDbm != 0)
      {
        totalRssi += metric.rssiDbm;
        rssiCount++;
      }

      if (metric.mtuSize > 0)
      {
        totalMtu += metric.mtuSize;
        mtuCount++;
      }

      totalBytes += metric.bytesTransferred;
    }
  }

  if (latencyCount > 0)
  {
    window.avgLatencyMs = totalLatency / latencyCount;
    calculatePercentiles(latencies, window.p95LatencyMs, window.p99LatencyMs);
  }

  if (rssiCount > 0)
  {
    window.avgRssiDbm = totalRssi / rssiCount;
    // Calculate signal quality: -30dBm=100%, -100dBm=0%
    int8_t qualityDbm = window.avgRssiDbm + 100;
    if (qualityDbm > 70)
      qualityDbm = 70;
    window.signalQualityPercent = (qualityDbm * 100) / 70;
  }

  if (mtuCount > 0)
  {
    window.avgMtuSize = totalMtu / mtuCount;
    window.mtuOptimized = window.avgMtuSize > 100;
  }

  window.successfulOperations = successCount;
  window.failedOperations = window.totalOperations - successCount;

  if (window.totalOperations > 0)
  {
    window.successRate = (successCount * 100.0f) / window.totalOperations;
  }

  window.totalBytesTransferred = totalBytes;

  if (windowMs > 0 && latencyCount > 0)
  {
    window.avgThroughputBytesPerSec = (totalBytes * 1000.0f) / windowMs;
    window.peakThroughputBytesPerSec = (totalBytes * 1000.0f) / (windowMs / latencyCount);
  }
}

void BLEMetricsCollector::analyzeTrends()
{
  // Compare 5-min vs 15-min to determine trends

  // Latency trend
  if (metrics5Min.avgLatencyMs < metrics15Min.avgLatencyMs)
  {
    latestTrend.latencyTrend = TrendInfo::TREND_IMPROVING;
    latestTrend.latencyChangePercent = -((metrics15Min.avgLatencyMs - metrics5Min.avgLatencyMs) * 100.0f / metrics15Min.avgLatencyMs);
  }
  else if (metrics5Min.avgLatencyMs > metrics15Min.avgLatencyMs)
  {
    latestTrend.latencyTrend = TrendInfo::TREND_DEGRADING;
    latestTrend.latencyChangePercent = ((metrics5Min.avgLatencyMs - metrics15Min.avgLatencyMs) * 100.0f / metrics15Min.avgLatencyMs);
  }
  else
  {
    latestTrend.latencyTrend = TrendInfo::TREND_STABLE;
  }

  // Throughput trend
  if (metrics5Min.avgThroughputBytesPerSec > metrics15Min.avgThroughputBytesPerSec)
  {
    latestTrend.throughputTrend = TrendInfo::TREND_IMPROVING;
  }
  else if (metrics5Min.avgThroughputBytesPerSec < metrics15Min.avgThroughputBytesPerSec)
  {
    latestTrend.throughputTrend = TrendInfo::TREND_DEGRADING;
  }
  else
  {
    latestTrend.throughputTrend = TrendInfo::TREND_STABLE;
  }

  // Signal trend
  if (metrics5Min.avgRssiDbm > metrics15Min.avgRssiDbm)
  {
    latestTrend.signalTrend = TrendInfo::TREND_IMPROVING;
  }
  else if (metrics5Min.avgRssiDbm < metrics15Min.avgRssiDbm)
  {
    latestTrend.signalTrend = TrendInfo::TREND_DEGRADING;
  }
  else
  {
    latestTrend.signalTrend = TrendInfo::TREND_STABLE;
  }

  // Success rate trend
  if (metrics5Min.successRate > metrics15Min.successRate)
  {
    latestTrend.successRateTrend = TrendInfo::TREND_IMPROVING;
  }
  else if (metrics5Min.successRate < metrics15Min.successRate)
  {
    latestTrend.successRateTrend = TrendInfo::TREND_DEGRADING;
  }
  else
  {
    latestTrend.successRateTrend = TrendInfo::TREND_STABLE;
  }
}

void BLEMetricsCollector::pruneOldMetrics()
{
  unsigned long now = millis();
  unsigned long cutoffTime = now - config.maxHistoryMs;

  while (!allMetrics.empty() && allMetrics.back().timestamp < cutoffTime)
  {
    allMetrics.pop_back();
  }

  // Also limit per-window size
  if (allMetrics.size() > config.maxMetricsPerWindow)
  {
    while (allMetrics.size() > config.maxMetricsPerWindow)
    {
      allMetrics.pop_back();
    }
  }
}

void BLEMetricsCollector::calculatePercentiles(const std::vector<uint32_t> &latencies,
                                               uint32_t &p95, uint32_t &p99) const
{
  if (latencies.empty())
  {
    p95 = 0;
    p99 = 0;
    return;
  }

  // Simple percentile calculation
  uint32_t index95 = (latencies.size() * 95) / 100;
  uint32_t index99 = (latencies.size() * 99) / 100;

  if (index95 < latencies.size())
  {
    // Find the 95th percentile value
    uint32_t minVal = UINT32_MAX;
    for (size_t i = 0; i < index95; i++)
    {
      if (latencies[i] < minVal)
        minVal = latencies[i];
    }
    p95 = minVal;
  }

  if (index99 < latencies.size())
  {
    uint32_t minVal = UINT32_MAX;
    for (size_t i = 0; i < index99; i++)
    {
      if (latencies[i] < minVal)
        minVal = latencies[i];
    }
    p99 = minVal;
  }
}

// Destructor
BLEMetricsCollector::~BLEMetricsCollector()
{
  Serial.println("[BLE_METRICS] Destroyed");
}
