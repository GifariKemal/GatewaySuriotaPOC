#ifndef MQTT_PERSISTENT_QUEUE_H
#define MQTT_PERSISTENT_QUEUE_H

#include <Arduino.h>
#include <vector>
#include <deque>
#include <ArduinoJson.h>
#include <cstdint>

/*
 * @brief MQTT Persistent Queue - Reliable message delivery with automatic retry
 *
 * Implements a persistent queue system for MQTT messages that ensures
 * no message is lost even when the broker is temporarily unavailable.
 *
 * Features:
 * - Persistent storage to LittleFS (survives power loss)
 * - Automatic retry with exponential backoff
 * - Priority levels (HIGH, NORMAL, LOW)
 * - Maximum queue size protection
 * - Compression for large messages
 * - Comprehensive statistics and diagnostics
 * - Per-message timeout tracking
 */

// Priority levels for messages
enum MessagePriority
{
  PRIORITY_LOW = 0,    // Lowest priority, queued last
  PRIORITY_NORMAL = 1, // Standard priority
  PRIORITY_HIGH = 2    // Highest priority, queued first
};

// Message status during its lifecycle
enum MessageStatus
{
  STATUS_QUEUED = 0,  // Waiting to be sent
  STATUS_SENDING = 1, // Currently being sent
  STATUS_SENT = 2,    // Successfully sent
  STATUS_FAILED = 3,  // Failed to send (max retries exceeded)
  STATUS_EXPIRED = 4  // Expired before being sent
};

// Queue operation result
enum QueueOperationResult
{
  QUEUE_SUCCESS = 0,
  QUEUE_FULL = 1,
  QUEUE_INVALID_TOPIC = 2,
  QUEUE_INVALID_PAYLOAD = 3,
  QUEUE_STORAGE_ERROR = 4,
  QUEUE_NOT_FOUND = 5
};

// Message retry state
enum RetryState
{
  RETRY_IDLE = 0,
  RETRY_WAITING = 1,
  RETRY_READY = 2
};

// Queue health status
enum QueueHealthStatus
{
  QUEUE_HEALTHY = 0,    // Queue operating normally
  QUEUE_WARNING = 1,    // > 80% full
  QUEUE_CRITICAL = 2,   // > 95% full
  QUEUE_HEALTH_FULL = 3 // At maximum capacity
};

// Queued MQTT message with metadata
struct QueuedMessage
{
  // Core message data
  String topic;       // MQTT topic
  String payload;     // Message payload
  uint16_t messageId; // Unique identifier (auto-assigned)

  // Metadata
  MessagePriority priority = PRIORITY_NORMAL;
  MessageStatus status = STATUS_QUEUED;
  unsigned long enqueuedTime = 0;  // When message was queued
  unsigned long lastRetryTime = 0; // When last retry was attempted
  uint8_t retryCount = 0;          // Number of retry attempts

  // Timeout tracking
  uint32_t timeoutMs = 0;    // 0 = no timeout (infinite retry)
  bool isCompressed = false; // Whether payload is compressed
  uint16_t originalSize = 0; // Original size if compressed

  // Retry state
  RetryState retryState = RETRY_IDLE;
  uint32_t nextRetryTimeMs = 0; // When to retry (0 = immediately)
};

// Queue statistics and monitoring
struct QueueStats
{
  // Queue size
  uint32_t totalMessages = 0;       // Total messages in queue
  uint32_t highPriorityCount = 0;   // HIGH priority messages
  uint32_t normalPriorityCount = 0; // NORMAL priority messages
  uint32_t lowPriorityCount = 0;    // LOW priority messages

  // Size metrics
  uint32_t totalPayloadSize = 0;   // Total bytes of payload
  uint32_t averagePayloadSize = 0; // Average payload size
  uint32_t largestPayloadSize = 0; // Largest single message

  // Persistence
  uint32_t persistedMessages = 0; // Messages saved to LittleFS
  uint32_t persistenceSize = 0;   // Total bytes on disk

  // Retries and failures
  uint32_t totalRetries = 0;    // Total retry attempts
  uint32_t failedMessages = 0;  // Messages that failed (max retries)
  uint32_t expiredMessages = 0; // Messages that expired

  // Success rate
  uint32_t successfulMessages = 0; // Messages sent successfully
  float successRate = 0.0f;        // Percentage of successful sends

  // Health
  QueueHealthStatus health = QUEUE_HEALTHY;
  float utilizationPercent = 0.0f; // % of max queue size
};

// Queue configuration
struct PersistenceConfig
{
  // Queue size limits
  uint32_t maxQueueSize = 1000; // Maximum number of messages

  // Retry parameters
  uint32_t initialRetryDelayMs = 5000; // 5 seconds initial delay
  uint32_t maxRetryDelayMs = 300000;   // 5 minutes max delay
  uint8_t maxRetries = 5;              // Max retry attempts per message
  float backoffMultiplier = 2.0f;      // Exponential backoff multiplier

  // Message timeouts
  uint32_t defaultTimeoutMs = 86400000; // 24 hours default timeout
  bool enableTimeout = true;            // Enable timeout checking

  // Compression
  bool enableCompression = true;       // Enable message compression
  uint16_t compressionThreshold = 256; // Min payload size to compress

  // Persistence
  bool enablePersistence = true;              // Save to LittleFS
  const char *persistenceDir = "/mqtt_queue"; // Directory for queue files
  uint32_t maxPersistenceSize = 102400;       // Max bytes on disk (100 KB)

  // Processing
  uint32_t processInterval = 5000; // How often to process queue (5 sec)
  uint8_t messagesPerCycle = 10;   // Max messages to send per cycle
};

class MQTTPersistentQueue
{
private:
  static MQTTPersistentQueue *instance;

  // Configuration
  PersistenceConfig config;

  // Message queues (separated by priority)
  std::deque<QueuedMessage> highPriorityQueue;
  std::deque<QueuedMessage> normalPriorityQueue;
  std::deque<QueuedMessage> lowPriorityQueue;

  // Statistics and tracking
  QueueStats stats;
  uint16_t nextMessageId = 1;
  unsigned long lastProcessTime = 0;

  // Callback for publish attempts
  typedef std::function<bool(const String &, const String &)> PublishCallback;
  PublishCallback publishCallback;

  // Private constructor for singleton
  MQTTPersistentQueue();

  // Internal methods
  void updateStats();
  void updateHealthStatus();
  QueueOperationResult persistMessageToDisk(const QueuedMessage &msg);
  bool loadQueueFromDisk();
  void cleanExpiredMessages();
  uint32_t calculateRetryDelay(uint8_t retryCount) const;
  std::deque<QueuedMessage> *getQueueForPriority(MessagePriority priority);

public:
  // Singleton access
  static MQTTPersistentQueue *getInstance();

  // Configuration methods
  void setConfig(const PersistenceConfig &newConfig);
  void setMaxQueueSize(uint32_t size);
  void setMaxRetries(uint8_t retries);
  void setRetryDelay(uint32_t initialMs, uint32_t maxMs);
  void setDefaultTimeout(uint32_t timeoutMs);
  void setCompressionThreshold(uint16_t bytes);

  // Message queueing
  QueueOperationResult enqueueMessage(const String &topic, const String &payload,
                                      MessagePriority priority = PRIORITY_NORMAL,
                                      uint32_t timeoutMs = 0);
  QueueOperationResult enqueueJsonMessage(const String &topic, const JsonObject &doc,
                                          MessagePriority priority = PRIORITY_NORMAL,
                                          uint32_t timeoutMs = 0);

  // Publish callback (from MQTT Manager)
  void setPublishCallback(PublishCallback callback);

  // Queue processing
  uint32_t processQueue();                     // Process pending messages for sending
  bool processReadyMessages();                 // Send messages that are ready for retry
  void retryFailedMessage(uint16_t messageId); // Manual retry of specific message

  // Message state queries
  MessageStatus getMessageStatus(uint16_t messageId) const;
  uint32_t getPendingMessageCount() const;
  uint32_t getMessagesByPriority(MessagePriority priority) const;
  QueueHealthStatus getQueueHealth() const;

  // Statistics and reporting
  QueueStats getStats() const;
  const char *getHealthStatusString(QueueHealthStatus status) const;
  const char *getMessageStatusString(MessageStatus status) const;
  const char *getPriorityString(MessagePriority priority) const;

  // Diagnostics
  void printQueueStatus();      // Print overall queue status
  void printQueueDetails();     // Print detailed queue contents
  void printStatistics();       // Print statistics
  void printRetryQueueStatus(); // Print messages waiting for retry
  void printFailedMessages();   // Print messages that failed
  void printHealthReport();     // Print health assessment

  // Queue management
  void clearQueue();                         // Clear all queued messages
  void clearFailedMessages();                // Clear messages that failed
  void clearExpiredMessages();               // Clear expired messages
  uint32_t pruneOldMessages(uint32_t ageMs); // Remove old messages

  // Persistence operations
  QueueOperationResult saveQueueToDisk();      // Manual save to disk
  QueueOperationResult loadQueueFromDiskNow(); // Manual load from disk
  void cleanupPersistenceStorage();            // Clean orphaned files
  uint32_t getPersistenceUsage() const;        // Get disk usage in bytes

  // Performance optimization
  void enableCompression(bool enable);
  void setBatchSize(uint8_t messagesPerCycle);
  void setProcessInterval(uint32_t intervalMs);

  // Recovery and robustness
  bool verifyQueueIntegrity(); // Check for corruption
  uint32_t repairQueue();      // Attempt to repair corrupted queue
  void enableAutoRecovery(bool enable);

  // Destructor
  ~MQTTPersistentQueue();
};

#endif // MQTT_PERSISTENT_QUEUE_H
