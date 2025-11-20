#ifndef BLE_METRICS_WINDOW_H
#define BLE_METRICS_WINDOW_H

#include <Arduino.h>
#include <deque>
#include <vector>

/*
 * @brief BLE Metrics - Rolling Window Optimization
 *
 * Implements efficient rolling window metrics collection for BLE operations.
 *
 * Features:
 * - Time-windowed statistics (1min, 5min, 15min)
 * - MTU negotiation latency tracking
 * - Connection/disconnection metrics
 * - Throughput measurements (bytes/sec)
 * - Signal strength (RSSI) trending
 * - Automatic data pruning
 * - Trend detection (improving/degrading)
 * - Low memory footprint
 */

// Metrics for individual BLE operations
struct BLEOperationMetric
{
  unsigned long timestamp = 0;   // When operation occurred
  uint32_t latencyMs = 0;        // Operation latency
  bool success = false;          // Operation result
  uint16_t mtuSize = 0;          // MTU size (for negotiation)
  int8_t rssiDbm = 0;            // Signal strength
  uint32_t bytesTransferred = 0; // Data transferred
};

// Aggregated metrics for time windows
struct WindowedMetrics
{
  // Time window info
  unsigned long windowStartTime = 0;
  unsigned long windowEndTime = 0;
  uint32_t windowDurationMs = 0;

  // Operation counts
  uint32_t totalOperations = 0;
  uint32_t successfulOperations = 0;
  uint32_t failedOperations = 0;
  float successRate = 0.0f;

  // Latency statistics (milliseconds)
  uint32_t minLatencyMs = UINT32_MAX;
  uint32_t maxLatencyMs = 0;
  uint32_t avgLatencyMs = 0;
  uint32_t p95LatencyMs = 0; // 95th percentile
  uint32_t p99LatencyMs = 0; // 99th percentile

  // Throughput (bytes/second)
  uint32_t totalBytesTransferred = 0;
  float avgThroughputBytesPerSec = 0.0f;
  float peakThroughputBytesPerSec = 0.0f;

  // Signal quality (RSSI)
  int8_t minRssiDbm = 0;
  int8_t maxRssiDbm = -127;
  int8_t avgRssiDbm = 0;
  uint32_t signalQualityPercent = 0; // 0-100%

  // MTU negotiation
  uint32_t mtuNegotiations = 0;
  uint16_t avgMtuSize = 0;
  bool mtuOptimized = false;
};

// Trend analysis
struct TrendInfo
{
  enum Trend
  {
    TREND_UNKNOWN = 0,
    TREND_STABLE = 1,
    TREND_IMPROVING = 2,
    TREND_DEGRADING = 3
  };

  Trend latencyTrend = TREND_UNKNOWN;
  Trend throughputTrend = TREND_UNKNOWN;
  Trend signalTrend = TREND_UNKNOWN;
  Trend successRateTrend = TREND_UNKNOWN;

  float latencyChangePercent = 0.0f; // Change vs previous window
  float throughputChangePercent = 0.0f;
  float signalChangeDbm = 0.0f;
  float successRateChangePercent = 0.0f;
};

// Metrics configuration
struct MetricsConfig
{
  // Window sizes (in milliseconds)
  uint32_t window1MinMs = 60000;   // 1 minute
  uint32_t window5MinMs = 300000;  // 5 minutes
  uint32_t window15MinMs = 900000; // 15 minutes

  // Data retention
  uint16_t maxMetricsPerWindow = 1000; // Max data points per window
  uint32_t maxHistoryMs = 900000;      // Keep 15 minutes of history

  // Thresholds for analysis
  uint32_t goodLatencyMs = 100;           // <100ms = good
  uint32_t warningLatencyMs = 500;        // 100-500ms = warning
  int8_t goodRssiDbm = -70;               // -70dBm or better = good
  int8_t warningRssiDbm = -85;            // -85 to -70dBm = warning
  float minAcceptableSuccessRate = 90.0f; // <90% = issue
};

// BLE Metrics Collector
class BLEMetricsCollector
{
private:
  static BLEMetricsCollector *instance;

  // Configuration
  MetricsConfig config;

  // Raw metric data (all time windows)
  std::deque<BLEOperationMetric> allMetrics;

  // Windowed aggregations
  WindowedMetrics metrics1Min;
  WindowedMetrics metrics5Min;
  WindowedMetrics metrics15Min;

  // Trend analysis
  TrendInfo latestTrend;

  // Statistics
  uint64_t totalMetricsCollected = 0;
  unsigned long lastAggregationTime = 0;
  uint32_t aggregationIntervalMs = 1000; // Recalculate every 1 second

  // Private constructor for singleton
  BLEMetricsCollector();

  // Internal methods
  void aggregateMetrics();
  void calculateWindowMetrics(WindowedMetrics &window, uint32_t windowMs);
  void analyzeTrends();
  void pruneOldMetrics();
  void calculatePercentiles(const std::vector<uint32_t> &latencies,
                            uint32_t &p95, uint32_t &p99) const;

public:
  // Singleton access
  static BLEMetricsCollector *getInstance();

  // Metrics recording
  void recordOperation(bool success, uint32_t latencyMs,
                       int8_t rssiDbm = 0, uint16_t mtuSize = 0,
                       uint32_t bytesTransferred = 0);

  void recordConnectionAttempt(bool success, uint32_t latencyMs);
  void recordDisconnection(int8_t reasonCode = 0);
  void recordMtuNegotiation(bool success, uint32_t latencyMs, uint16_t finalMtu);
  void recordDataTransfer(uint32_t bytesTransferred, uint32_t latencyMs);
  void recordSignalStrength(int8_t rssiDbm);

  // Metrics retrieval
  WindowedMetrics getMetrics1Min() const;
  WindowedMetrics getMetrics5Min() const;
  WindowedMetrics getMetrics15Min() const;
  WindowedMetrics getMetricsAllTime() const;

  // Trend analysis
  TrendInfo getTrends() const;
  TrendInfo::Trend getLatencyTrend() const;
  TrendInfo::Trend getThroughputTrend() const;
  TrendInfo::Trend getSignalTrend() const;
  TrendInfo::Trend getSuccessRateTrend() const;

  // Health assessment
  bool isHealthy() const;
  bool hasGoodSignal() const;
  bool hasGoodLatency() const;
  bool hasGoodThroughput() const;
  uint32_t getHealthScore() const; // 0-100

  // Detailed analysis
  String getHealthAssessment() const;
  String getPerformanceReport() const;
  bool isLikelyToSucceed() const; // Prediction based on recent metrics

  // Configuration
  void setConfig(const MetricsConfig &newConfig);
  void setWindowSizes(uint32_t window1, uint32_t window5, uint32_t window15);
  void setMaxMetricsPerWindow(uint16_t maxCount);
  void setLatencyThresholds(uint32_t goodMs, uint32_t warningMs);
  void setSignalThresholds(int8_t goodDbm, int8_t warningDbm);
  void setMinSuccessRate(float minPercent);

  // Utility methods
  uint32_t getMetricsCount() const;
  uint64_t getTotalMetricsCollected() const;
  void clearMetrics();
  void resetTrends();

  // Diagnostics
  void printMetrics1Min();
  void printMetrics5Min();
  void printMetrics15Min();
  void printTrendAnalysis();
  void printDetailedReport();
  void printHealthScore();
  void printWindowComparison();

  // Destructor
  ~BLEMetricsCollector();
};

#endif // BLE_METRICS_WINDOW_H
