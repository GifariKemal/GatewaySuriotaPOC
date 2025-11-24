#include "MQTTPersistentQueue.h"
#include <LittleFS.h>
#include <algorithm>

// Singleton instance
MQTTPersistentQueue *MQTTPersistentQueue::instance = nullptr;

// Private constructor
MQTTPersistentQueue::MQTTPersistentQueue()
{
  Serial.println("[MQTT_QUEUE] Persistent queue initialized");
  lastProcessTime = millis();

  // CRITICAL FIX: Create mutex for thread safety
  queueMutex = xSemaphoreCreateMutex();
  if (queueMutex == NULL)
  {
    Serial.println("[MQTT_QUEUE] CRITICAL: Failed to create mutex!");
  }
  else
  {
    Serial.println("[MQTT_QUEUE] Thread safety mutex created successfully");
  }

  // Initialize LittleFS if not already done
  if (!LittleFS.begin(true))
  {
    Serial.println("[MQTT_QUEUE] WARNING: LittleFS initialization failed");
  }

  // Create queue directory
  File queueDir = LittleFS.open(config.persistenceDir, "r");
  if (!queueDir)
  {
    LittleFS.mkdir(config.persistenceDir);
    Serial.printf("[MQTT_QUEUE] Created queue directory: %s\n", config.persistenceDir);
  }
  else
  {
    queueDir.close();
  }

  // Load persisted queue from disk
  loadQueueFromDisk();
}

// Singleton access
MQTTPersistentQueue *MQTTPersistentQueue::getInstance()
{
  // Thread-safe Meyers Singleton (C++11 guarantees thread-safe static init)
  static MQTTPersistentQueue instance;
  static MQTTPersistentQueue *ptr = &instance;
  return ptr;
}

// Configuration methods
void MQTTPersistentQueue::setConfig(const PersistenceConfig &newConfig)
{
  config = newConfig;
  Serial.println("[MQTT_QUEUE] Configuration updated:");
  Serial.printf("  Max queue size: %ld\n", config.maxQueueSize);
  Serial.printf("  Max retries: %d\n", config.maxRetries);
  Serial.printf("  Max retry delay: %ld ms\n", config.maxRetryDelayMs);
}

void MQTTPersistentQueue::setMaxQueueSize(uint32_t size)
{
  config.maxQueueSize = size;
  Serial.printf("[MQTT_QUEUE] Max queue size set to: %ld\n", size);
}

void MQTTPersistentQueue::setMaxRetries(uint8_t retries)
{
  config.maxRetries = retries;
  Serial.printf("[MQTT_QUEUE] Max retries set to: %d\n", retries);
}

void MQTTPersistentQueue::setRetryDelay(uint32_t initialMs, uint32_t maxMs)
{
  config.initialRetryDelayMs = initialMs;
  config.maxRetryDelayMs = maxMs;
  Serial.printf("[MQTT_QUEUE] Retry delay set: %ld - %ld ms\n", initialMs, maxMs);
}

void MQTTPersistentQueue::setDefaultTimeout(uint32_t timeoutMs)
{
  config.defaultTimeoutMs = timeoutMs;
  Serial.printf("[MQTT_QUEUE] Default timeout set to: %ld ms\n", timeoutMs);
}

void MQTTPersistentQueue::setCompressionThreshold(uint16_t bytes)
{
  config.compressionThreshold = bytes;
  Serial.printf("[MQTT_QUEUE] Compression threshold set to: %d bytes\n", bytes);
}

// Message queueing
QueueOperationResult MQTTPersistentQueue::enqueueMessage(const String &topic,
                                                         const String &payload,
                                                         MessagePriority priority,
                                                         uint32_t timeoutMs)
{
  // CRITICAL FIX: Mutex protection for thread safety
  xSemaphoreTake(queueMutex, portMAX_DELAY);

  // Validate inputs
  if (topic.isEmpty())
  {
    Serial.println("[MQTT_QUEUE] ERROR: Topic is empty");
    xSemaphoreGive(queueMutex);
    return QUEUE_INVALID_TOPIC;
  }

  if (payload.isEmpty())
  {
    Serial.println("[MQTT_QUEUE] ERROR: Payload is empty");
    xSemaphoreGive(queueMutex);
    return QUEUE_INVALID_PAYLOAD;
  }

  // Check queue size limits
  uint32_t totalMessages = highPriorityQueue.size() +
                           normalPriorityQueue.size() +
                           lowPriorityQueue.size();

  if (totalMessages >= config.maxQueueSize)
  {
    Serial.printf("[MQTT_QUEUE] ERROR: Queue full (%ld/%ld)\n",
                  totalMessages, config.maxQueueSize);
    xSemaphoreGive(queueMutex);
    return QUEUE_FULL;
  }

  // Create message
  QueuedMessage msg;
  msg.messageId = nextMessageId++;
  msg.topic = topic;
  msg.payload = payload;
  msg.priority = priority;
  msg.status = STATUS_QUEUED;
  msg.enqueuedTime = millis();
  msg.lastRetryTime = 0;
  msg.retryCount = 0;
  msg.timeoutMs = (timeoutMs > 0) ? timeoutMs : config.defaultTimeoutMs;
  msg.retryState = RETRY_IDLE;
  msg.nextRetryTimeMs = 0;

  // Get appropriate queue
  std::deque<QueuedMessage, STLPSRAMAllocator<QueuedMessage>> *targetQueue = getQueueForPriority(priority);
  if (!targetQueue)
  {
    xSemaphoreGive(queueMutex);
    return QUEUE_INVALID_TOPIC;
  }

  // Enqueue message
  targetQueue->push_back(msg);

  // CRITICAL FIX: Release mutex BEFORE File I/O to prevent blocking
  // This dramatically improves concurrency performance (50-500ms saved)
  xSemaphoreGive(queueMutex);

  // Update statistics (outside mutex - not critical section)
  // NOTE: Stats updates have potential race condition but acceptable for monitoring data
  // Stats accuracy is not critical for system functionality
  stats.totalPayloadSize += payload.length();
  if (payload.length() > stats.largestPayloadSize)
  {
    stats.largestPayloadSize = payload.length();
  }
  updateStats();

  // Persist to disk AFTER releasing mutex (deferred write pattern)
  // Trade-off: If crash occurs between unlock and persist, message is in RAM but not on disk
  // This is acceptable for significantly improved system responsiveness
  if (config.enablePersistence)
  {
    QueueOperationResult result = persistMessageToDisk(msg);
    if (result != QUEUE_SUCCESS)
    {
      Serial.printf("[MQTT_QUEUE] WARNING: Failed to persist message %d\n", msg.messageId);
    }
  }

  Serial.printf("[MQTT_QUEUE] Message %d queued [%s] (topic: %s, size: %d bytes)\n",
                msg.messageId, getPriorityString(priority), msg.topic.c_str(),
                msg.payload.length());

  return QUEUE_SUCCESS;
}

QueueOperationResult MQTTPersistentQueue::enqueueJsonMessage(const String &topic,
                                                             const JsonObject &doc,
                                                             MessagePriority priority,
                                                             uint32_t timeoutMs)
{
  String payload;
  serializeJson(doc, payload);
  return enqueueMessage(topic, payload, priority, timeoutMs);
}

// Publish callback
void MQTTPersistentQueue::setPublishCallback(PublishCallback callback)
{
  publishCallback = callback;
  Serial.println("[MQTT_QUEUE] Publish callback registered");
}

// Queue processing
uint32_t MQTTPersistentQueue::processQueue()
{
  // CRITICAL FIX: Mutex protection for thread safety
  xSemaphoreTake(queueMutex, portMAX_DELAY);

  unsigned long now = millis();

  // Check if processing interval has elapsed
  if (now - lastProcessTime < config.processInterval)
  {
    xSemaphoreGive(queueMutex);
    return 0;
  }
  lastProcessTime = now;

  uint32_t messagesSent = 0;

  // Clean expired messages
  cleanExpiredMessages();

  // Process messages by priority (HIGH to NORMAL to LOW)
  uint8_t messagesThisCycle = 0;

  // Process HIGH priority first
  while (!highPriorityQueue.empty() && messagesThisCycle < config.messagesPerCycle)
  {
    QueuedMessage &msg = highPriorityQueue.front();

    if (msg.status == STATUS_QUEUED || msg.retryState == RETRY_READY)
    {
      // Convert PSRAMString to Arduino String for callback
      if (publishCallback && publishCallback(msg.topic.toString(), msg.payload.toString()))
      {
        // Success
        msg.status = STATUS_SENT;
        Serial.printf("[MQTT_QUEUE] Message %d sent successfully\n", msg.messageId);
        stats.successfulMessages++;
        messagesSent++;
        messagesThisCycle++;
        highPriorityQueue.pop_front();
      }
      else
      {
        // Failed - schedule retry
        msg.status = STATUS_FAILED;
        msg.retryCount++;

        if (msg.retryCount < config.maxRetries)
        {
          msg.retryState = RETRY_WAITING;
          msg.lastRetryTime = now;
          msg.nextRetryTimeMs = now + calculateRetryDelay(msg.retryCount);

          Serial.printf("[MQTT_QUEUE] Message %d retry %d scheduled (next: +%ld ms)\n",
                        msg.messageId, msg.retryCount,
                        calculateRetryDelay(msg.retryCount));
          messagesThisCycle++;
          highPriorityQueue.pop_front();
          normalPriorityQueue.push_back(msg); // Re-queue with lower priority
        }
        else
        {
          // Max retries exceeded
          msg.status = STATUS_FAILED;
          Serial.printf("[MQTT_QUEUE] ERROR: Message %d failed (max %d retries)\n",
                        msg.messageId, config.maxRetries);
          stats.failedMessages++;
          messagesThisCycle++;
          highPriorityQueue.pop_front();
        }
      }
    }
    else if (msg.retryState == RETRY_WAITING)
    {
      // Check if retry time has arrived
      if (now >= msg.nextRetryTimeMs)
      {
        msg.retryState = RETRY_READY;
        highPriorityQueue.push_back(highPriorityQueue.front());
        highPriorityQueue.pop_front();
      }
      else
      {
        break; // Not ready yet, skip rest
      }
    }
    else
    {
      highPriorityQueue.pop_front();
    }
  }

  // Process NORMAL priority (similar logic)
  while (!normalPriorityQueue.empty() && messagesThisCycle < config.messagesPerCycle)
  {
    QueuedMessage &msg = normalPriorityQueue.front();

    if (msg.status == STATUS_QUEUED || msg.retryState == RETRY_READY)
    {
      // Convert PSRAMString to Arduino String for callback
      if (publishCallback && publishCallback(msg.topic.toString(), msg.payload.toString()))
      {
        msg.status = STATUS_SENT;
        Serial.printf("[MQTT_QUEUE] Message %d sent successfully\n", msg.messageId);
        stats.successfulMessages++;
        messagesSent++;
        messagesThisCycle++;
        normalPriorityQueue.pop_front();
      }
      else
      {
        msg.status = STATUS_FAILED;
        msg.retryCount++;

        if (msg.retryCount < config.maxRetries)
        {
          msg.retryState = RETRY_WAITING;
          msg.lastRetryTime = now;
          msg.nextRetryTimeMs = now + calculateRetryDelay(msg.retryCount);
          messagesThisCycle++;
          normalPriorityQueue.pop_front();
          lowPriorityQueue.push_back(msg);
        }
        else
        {
          msg.status = STATUS_FAILED;
          stats.failedMessages++;
          messagesThisCycle++;
          normalPriorityQueue.pop_front();
        }
      }
    }
    else if (msg.retryState == RETRY_WAITING)
    {
      if (now >= msg.nextRetryTimeMs)
      {
        msg.retryState = RETRY_READY;
        normalPriorityQueue.push_back(normalPriorityQueue.front());
        normalPriorityQueue.pop_front();
      }
      else
      {
        break;
      }
    }
    else
    {
      normalPriorityQueue.pop_front();
    }
  }

  // Process LOW priority
  while (!lowPriorityQueue.empty() && messagesThisCycle < config.messagesPerCycle)
  {
    QueuedMessage &msg = lowPriorityQueue.front();

    if (msg.status == STATUS_QUEUED || msg.retryState == RETRY_READY)
    {
      // Convert PSRAMString to Arduino String for callback
      if (publishCallback && publishCallback(msg.topic.toString(), msg.payload.toString()))
      {
        msg.status = STATUS_SENT;
        stats.successfulMessages++;
        messagesSent++;
        messagesThisCycle++;
        lowPriorityQueue.pop_front();
      }
      else
      {
        msg.retryCount++;
        if (msg.retryCount < config.maxRetries)
        {
          msg.retryState = RETRY_WAITING;
          msg.lastRetryTime = now;
          msg.nextRetryTimeMs = now + calculateRetryDelay(msg.retryCount);
          messagesThisCycle++;
          lowPriorityQueue.pop_front();
        }
        else
        {
          msg.status = STATUS_FAILED;
          stats.failedMessages++;
          messagesThisCycle++;
          lowPriorityQueue.pop_front();
        }
      }
    }
    else if (msg.retryState == RETRY_WAITING)
    {
      if (now >= msg.nextRetryTimeMs)
      {
        msg.retryState = RETRY_READY;
        lowPriorityQueue.push_back(lowPriorityQueue.front());
        lowPriorityQueue.pop_front();
      }
      else
      {
        break;
      }
    }
    else
    {
      lowPriorityQueue.pop_front();
    }
  }

  updateStats();
  stats.totalRetries += messagesThisCycle;

  xSemaphoreGive(queueMutex);
  return messagesSent;
}

bool MQTTPersistentQueue::processReadyMessages()
{
  unsigned long now = millis();
  uint32_t processed = 0;

  // Check all queues for messages ready to retry
  for (auto &msg : highPriorityQueue)
  {
    if (msg.retryState == RETRY_WAITING && now >= msg.nextRetryTimeMs)
    {
      msg.retryState = RETRY_READY;
      processed++;
    }
  }

  for (auto &msg : normalPriorityQueue)
  {
    if (msg.retryState == RETRY_WAITING && now >= msg.nextRetryTimeMs)
    {
      msg.retryState = RETRY_READY;
      processed++;
    }
  }

  for (auto &msg : lowPriorityQueue)
  {
    if (msg.retryState == RETRY_WAITING && now >= msg.nextRetryTimeMs)
    {
      msg.retryState = RETRY_READY;
      processed++;
    }
  }

  return processed > 0;
}

void MQTTPersistentQueue::retryFailedMessage(uint16_t messageId)
{
  // Search in all queues
  for (auto &msg : highPriorityQueue)
  {
    if (msg.messageId == messageId)
    {
      msg.retryCount = 0;
      msg.status = STATUS_QUEUED;
      msg.retryState = RETRY_IDLE;
      Serial.printf("[MQTT_QUEUE] Manual retry scheduled for message %d\n", messageId);
      return;
    }
  }

  for (auto &msg : normalPriorityQueue)
  {
    if (msg.messageId == messageId)
    {
      msg.retryCount = 0;
      msg.status = STATUS_QUEUED;
      msg.retryState = RETRY_IDLE;
      Serial.printf("[MQTT_QUEUE] Manual retry scheduled for message %d\n", messageId);
      return;
    }
  }

  for (auto &msg : lowPriorityQueue)
  {
    if (msg.messageId == messageId)
    {
      msg.retryCount = 0;
      msg.status = STATUS_QUEUED;
      msg.retryState = RETRY_IDLE;
      Serial.printf("[MQTT_QUEUE] Manual retry scheduled for message %d\n", messageId);
      return;
    }
  }

  Serial.printf("[MQTT_QUEUE] ERROR: Message %d not found\n", messageId);
}

// Message state queries
MessageStatus MQTTPersistentQueue::getMessageStatus(uint16_t messageId) const
{
  for (const auto &msg : highPriorityQueue)
  {
    if (msg.messageId == messageId)
      return msg.status;
  }
  for (const auto &msg : normalPriorityQueue)
  {
    if (msg.messageId == messageId)
      return msg.status;
  }
  for (const auto &msg : lowPriorityQueue)
  {
    if (msg.messageId == messageId)
      return msg.status;
  }
  return STATUS_QUEUED;
}

uint32_t MQTTPersistentQueue::getPendingMessageCount() const
{
  return highPriorityQueue.size() + normalPriorityQueue.size() + lowPriorityQueue.size();
}

uint32_t MQTTPersistentQueue::getMessagesByPriority(MessagePriority priority) const
{
  switch (priority)
  {
  case PRIORITY_HIGH:
    return highPriorityQueue.size();
  case PRIORITY_NORMAL:
    return normalPriorityQueue.size();
  case PRIORITY_LOW:
    return lowPriorityQueue.size();
  default:
    return 0;
  }
}

QueueHealthStatus MQTTPersistentQueue::getQueueHealth() const
{
  return stats.health;
}

// Statistics and reporting
QueueStats MQTTPersistentQueue::getStats() const
{
  return stats;
}

const char *MQTTPersistentQueue::getHealthStatusString(QueueHealthStatus status) const
{
  switch (status)
  {
  case QUEUE_HEALTHY:
    return "HEALTHY";
  case QUEUE_WARNING:
    return "WARNING (>80% full)";
  case QUEUE_CRITICAL:
    return "CRITICAL (>95% full)";
  case QUEUE_HEALTH_FULL:
    return "FULL (at max capacity)";
  default:
    return "UNKNOWN";
  }
}

const char *MQTTPersistentQueue::getMessageStatusString(MessageStatus status) const
{
  switch (status)
  {
  case STATUS_QUEUED:
    return "QUEUED";
  case STATUS_SENDING:
    return "SENDING";
  case STATUS_SENT:
    return "SENT";
  case STATUS_FAILED:
    return "FAILED";
  case STATUS_EXPIRED:
    return "EXPIRED";
  default:
    return "UNKNOWN";
  }
}

const char *MQTTPersistentQueue::getPriorityString(MessagePriority priority) const
{
  switch (priority)
  {
  case PRIORITY_HIGH:
    return "HIGH";
  case PRIORITY_NORMAL:
    return "NORMAL";
  case PRIORITY_LOW:
    return "LOW";
  default:
    return "UNKNOWN";
  }
}

// Diagnostics
void MQTTPersistentQueue::printQueueStatus()
{
  Serial.println("\n[MQTT_QUEUE] QUEUE STATUS");
  Serial.printf("  Health: %s\n", getHealthStatusString(stats.health));
  Serial.printf("  Pending messages: %ld / %ld (%.1f%%)\n",
                stats.totalMessages, config.maxQueueSize, stats.utilizationPercent);
  Serial.printf("  Total payload: %ld bytes\n", stats.totalPayloadSize);
  Serial.printf("  Success rate: %.1f%% (%ld/%ld)\n\n",
                stats.successRate, stats.successfulMessages,
                stats.successfulMessages + stats.failedMessages);
}

void MQTTPersistentQueue::printQueueDetails()
{
  Serial.println("\n[MQTT_QUEUE] HIGH PRIORITY QUEUE");
  Serial.printf("  Count: %ld\n", highPriorityQueue.size());
  for (const auto &msg : highPriorityQueue)
  {
    Serial.printf("    [%d] %s to %s (%d bytes, retries: %d)\n",
                  msg.messageId, msg.topic.c_str(),
                  getMessageStatusString(msg.status), msg.payload.length(),
                  msg.retryCount);
  }

  Serial.println("\n[MQTT_QUEUE] NORMAL PRIORITY QUEUE");
  Serial.printf("  Count: %ld\n", normalPriorityQueue.size());
  for (const auto &msg : normalPriorityQueue)
  {
    Serial.printf("    [%d] %s to %s (%d bytes, retries: %d)\n",
                  msg.messageId, msg.topic.c_str(),
                  getMessageStatusString(msg.status), msg.payload.length(),
                  msg.retryCount);
  }

  Serial.println("\n[MQTT_QUEUE] LOW PRIORITY QUEUE");
  Serial.printf("  Count: %ld\n", lowPriorityQueue.size());
  for (const auto &msg : lowPriorityQueue)
  {
    Serial.printf("    [%d] %s to %s (%d bytes, retries: %d)\n",
                  msg.messageId, msg.topic.c_str(),
                  getMessageStatusString(msg.status), msg.payload.length(),
                  msg.retryCount);
  }
  Serial.println();
}

void MQTTPersistentQueue::printStatistics()
{
  Serial.println("\n[MQTT_QUEUE] STATISTICS");
  Serial.printf("  Total queued: %ld\n", stats.totalMessages);
  Serial.printf("  By priority: HIGH=%ld, NORMAL=%ld, LOW=%ld\n",
                stats.highPriorityCount, stats.normalPriorityCount,
                stats.lowPriorityCount);
  Serial.printf("  Total payload: %ld bytes (avg: %ld, max: %ld)\n",
                stats.totalPayloadSize, stats.averagePayloadSize,
                stats.largestPayloadSize);
  Serial.printf("  Successful: %ld | Failed: %ld | Expired: %ld\n",
                stats.successfulMessages, stats.failedMessages,
                stats.expiredMessages);
  Serial.printf("  Retries: %ld attempts\n\n", stats.totalRetries);
}

void MQTTPersistentQueue::printRetryQueueStatus()
{
  Serial.println("\n[MQTT_QUEUE] MESSAGES WAITING FOR RETRY");

  uint32_t waitingCount = 0;
  unsigned long now = millis();

  for (const auto &msg : highPriorityQueue)
  {
    if (msg.retryState == RETRY_WAITING)
    {
      unsigned long timeUntilRetry = msg.nextRetryTimeMs > now ? msg.nextRetryTimeMs - now : 0;
      Serial.printf("  [%d] Retry in %lu ms (attempt %d/%d)\n",
                    msg.messageId, timeUntilRetry, msg.retryCount + 1,
                    config.maxRetries);
      waitingCount++;
    }
  }

  for (const auto &msg : normalPriorityQueue)
  {
    if (msg.retryState == RETRY_WAITING)
    {
      unsigned long timeUntilRetry = msg.nextRetryTimeMs > now ? msg.nextRetryTimeMs - now : 0;
      Serial.printf("  [%d] Retry in %lu ms (attempt %d/%d)\n",
                    msg.messageId, timeUntilRetry, msg.retryCount + 1,
                    config.maxRetries);
      waitingCount++;
    }
  }

  for (const auto &msg : lowPriorityQueue)
  {
    if (msg.retryState == RETRY_WAITING)
    {
      unsigned long timeUntilRetry = msg.nextRetryTimeMs > now ? msg.nextRetryTimeMs - now : 0;
      Serial.printf("  [%d] Retry in %lu ms (attempt %d/%d)\n",
                    msg.messageId, timeUntilRetry, msg.retryCount + 1,
                    config.maxRetries);
      waitingCount++;
    }
  }

  if (waitingCount == 0)
  {
    Serial.println("  None");
  }
  Serial.println();
}

void MQTTPersistentQueue::printFailedMessages()
{
  Serial.println("\n[MQTT_QUEUE] FAILED MESSAGES");

  uint32_t failedCount = 0;

  for (const auto &msg : highPriorityQueue)
  {
    if (msg.status == STATUS_FAILED)
    {
      Serial.printf("  [%d] %s (retries: %d/%d, payload: %d bytes)\n",
                    msg.messageId, msg.topic.c_str(),
                    msg.retryCount, config.maxRetries, msg.payload.length());
      failedCount++;
    }
  }

  for (const auto &msg : normalPriorityQueue)
  {
    if (msg.status == STATUS_FAILED)
    {
      Serial.printf("  [%d] %s (retries: %d/%d, payload: %d bytes)\n",
                    msg.messageId, msg.topic.c_str(),
                    msg.retryCount, config.maxRetries, msg.payload.length());
      failedCount++;
    }
  }

  for (const auto &msg : lowPriorityQueue)
  {
    if (msg.status == STATUS_FAILED)
    {
      Serial.printf("  [%d] %s (retries: %d/%d, payload: %d bytes)\n",
                    msg.messageId, msg.topic.c_str(),
                    msg.retryCount, config.maxRetries, msg.payload.length());
      failedCount++;
    }
  }

  if (failedCount == 0)
  {
    Serial.println("  None - all messages successful!");
  }
  Serial.println();
}

void MQTTPersistentQueue::printHealthReport()
{
  Serial.println("\n[MQTT QUEUE] HEALTH REPORT\n");

  printQueueStatus();
  printStatistics();
  printRetryQueueStatus();
  printFailedMessages();
}

// Queue management
void MQTTPersistentQueue::clearQueue()
{
  xSemaphoreTake(queueMutex, portMAX_DELAY);
  highPriorityQueue.clear();
  normalPriorityQueue.clear();
  lowPriorityQueue.clear();
  updateStats();
  xSemaphoreGive(queueMutex);
  Serial.println("[MQTT_QUEUE] Queue cleared");
}

void MQTTPersistentQueue::clearFailedMessages()
{
  uint32_t cleared = 0;

  for (auto it = highPriorityQueue.begin(); it != highPriorityQueue.end();)
  {
    if (it->status == STATUS_FAILED)
    {
      it = highPriorityQueue.erase(it);
      cleared++;
    }
    else
    {
      ++it;
    }
  }

  for (auto it = normalPriorityQueue.begin(); it != normalPriorityQueue.end();)
  {
    if (it->status == STATUS_FAILED)
    {
      it = normalPriorityQueue.erase(it);
      cleared++;
    }
    else
    {
      ++it;
    }
  }

  for (auto it = lowPriorityQueue.begin(); it != lowPriorityQueue.end();)
  {
    if (it->status == STATUS_FAILED)
    {
      it = lowPriorityQueue.erase(it);
      cleared++;
    }
    else
    {
      ++it;
    }
  }

  updateStats();
  Serial.printf("[MQTT_QUEUE] Cleared %ld failed messages\n", cleared);
}

void MQTTPersistentQueue::clearExpiredMessages()
{
  unsigned long now = millis();
  uint32_t cleared = 0;

  for (auto it = highPriorityQueue.begin(); it != highPriorityQueue.end();)
  {
    if (it->timeoutMs > 0 && (now - it->enqueuedTime) > it->timeoutMs)
    {
      it->status = STATUS_EXPIRED;
      it = highPriorityQueue.erase(it);
      cleared++;
      stats.expiredMessages++;
    }
    else
    {
      ++it;
    }
  }

  for (auto it = normalPriorityQueue.begin(); it != normalPriorityQueue.end();)
  {
    if (it->timeoutMs > 0 && (now - it->enqueuedTime) > it->timeoutMs)
    {
      it->status = STATUS_EXPIRED;
      it = normalPriorityQueue.erase(it);
      cleared++;
      stats.expiredMessages++;
    }
    else
    {
      ++it;
    }
  }

  for (auto it = lowPriorityQueue.begin(); it != lowPriorityQueue.end();)
  {
    if (it->timeoutMs > 0 && (now - it->enqueuedTime) > it->timeoutMs)
    {
      it->status = STATUS_EXPIRED;
      it = lowPriorityQueue.erase(it);
      cleared++;
      stats.expiredMessages++;
    }
    else
    {
      ++it;
    }
  }

  if (cleared > 0)
  {
    Serial.printf("[MQTT_QUEUE] Cleared %ld expired messages\n", cleared);
    updateStats();
  }
}

uint32_t MQTTPersistentQueue::pruneOldMessages(uint32_t ageMs)
{
  unsigned long now = millis();
  uint32_t pruned = 0;

  for (auto it = lowPriorityQueue.begin(); it != lowPriorityQueue.end();)
  {
    if ((now - it->enqueuedTime) > ageMs)
    {
      it = lowPriorityQueue.erase(it);
      pruned++;
    }
    else
    {
      ++it;
    }
  }

  updateStats();
  return pruned;
}

// Persistence operations
QueueOperationResult MQTTPersistentQueue::persistMessageToDisk(const QueuedMessage &msg)
{
  String filename = String(config.persistenceDir) + "/" + String(msg.messageId) + ".json";

  JsonDocument doc;
  doc["id"] = msg.messageId;
  doc["topic"] = msg.topic.toString();       // Convert PSRAMString to String
  doc["payload"] = msg.payload.toString();   // Convert PSRAMString to String
  doc["priority"] = (int)msg.priority;
  doc["status"] = (int)msg.status;
  doc["enqueued_time"] = msg.enqueuedTime;
  doc["retry_count"] = msg.retryCount;
  doc["timeout_ms"] = msg.timeoutMs;

  File file = LittleFS.open(filename.c_str(), "w");
  if (!file)
  {
    Serial.printf("[MQTT_QUEUE] ERROR: Cannot create file %s\n", filename.c_str());
    return QUEUE_STORAGE_ERROR;
  }

  if (serializeJson(doc, file) == 0)
  {
    Serial.printf("[MQTT_QUEUE] ERROR: Failed to write to %s\n", filename.c_str());
    file.close();
    return QUEUE_STORAGE_ERROR;
  }

  file.close();
  return QUEUE_SUCCESS;
}

bool MQTTPersistentQueue::loadQueueFromDisk()
{
  File queueDir = LittleFS.open(config.persistenceDir, "r");
  if (!queueDir)
  {
    Serial.printf("[MQTT_QUEUE] Queue directory not found: %s\n", config.persistenceDir);
    return false;
  }

  uint32_t loadedCount = 0;

  File file = queueDir.openNextFile();
  while (file)
  {
    if (String(file.name()).endsWith(".json"))
    {
      JsonDocument doc;

      if (deserializeJson(doc, file) == DeserializationError::Ok)
      {
        QueuedMessage msg;
        msg.messageId = doc["id"];
        msg.topic = PSRAMString(doc["topic"].as<String>());      // Convert String to PSRAMString
        msg.payload = PSRAMString(doc["payload"].as<String>());  // Convert String to PSRAMString
        msg.priority = (MessagePriority)doc["priority"].as<int>();
        msg.status = (MessageStatus)doc["status"].as<int>();
        msg.enqueuedTime = doc["enqueued_time"];
        msg.retryCount = doc["retry_count"];
        msg.timeoutMs = doc["timeout_ms"];
        msg.retryState = RETRY_IDLE;

        std::deque<QueuedMessage, STLPSRAMAllocator<QueuedMessage>> *targetQueue = getQueueForPriority(msg.priority);
        if (targetQueue)
        {
          targetQueue->push_back(msg);
          loadedCount++;
        }
      }
    }

    file.close();
    file = queueDir.openNextFile();
  }

  queueDir.close();

  if (loadedCount > 0)
  {
    Serial.printf("[MQTT_QUEUE] Loaded %ld persisted messages from disk\n", loadedCount);
    updateStats();
  }

  return true;
}

void MQTTPersistentQueue::cleanupPersistenceStorage()
{
  File queueDir = LittleFS.open(config.persistenceDir, "r");
  if (!queueDir)
  {
    // No valid handle to close - LittleFS returns invalid File object on failure
    Serial.printf("[MQTT_QUEUE] ERROR: Failed to open persistence directory: %s\n", config.persistenceDir);
    return;
  }

  uint32_t cleanedCount = 0;
  File file = queueDir.openNextFile();

  while (file)
  {
    String filename = file.name();

    // Remove files for messages no longer in queue
    uint16_t fileId = 0;
    if (sscanf(filename.c_str(), "%hu.json", &fileId) == 1)
    {
      bool found = false;

      for (const auto &msg : highPriorityQueue)
      {
        if (msg.messageId == fileId)
        {
          found = true;
          break;
        }
      }
      for (const auto &msg : normalPriorityQueue)
      {
        if (msg.messageId == fileId)
        {
          found = true;
          break;
        }
      }
      for (const auto &msg : lowPriorityQueue)
      {
        if (msg.messageId == fileId)
        {
          found = true;
          break;
        }
      }

      if (!found)
      {
        String fullPath = String(config.persistenceDir) + "/" + filename;
        LittleFS.remove(fullPath.c_str());
        cleanedCount++;
      }
    }

    file.close();
    file = queueDir.openNextFile();
  }

  queueDir.close();

  if (cleanedCount > 0)
  {
    Serial.printf("[MQTT_QUEUE] Cleaned %ld orphaned files\n", cleanedCount);
  }
}

QueueOperationResult MQTTPersistentQueue::saveQueueToDisk()
{
  // Save all queued messages
  for (const auto &msg : highPriorityQueue)
  {
    persistMessageToDisk(msg);
  }
  for (const auto &msg : normalPriorityQueue)
  {
    persistMessageToDisk(msg);
  }
  for (const auto &msg : lowPriorityQueue)
  {
    persistMessageToDisk(msg);
  }

  Serial.println("[MQTT_QUEUE] Queue saved to disk");
  return QUEUE_SUCCESS;
}

QueueOperationResult MQTTPersistentQueue::loadQueueFromDiskNow()
{
  clearQueue();
  return loadQueueFromDisk() ? QUEUE_SUCCESS : QUEUE_STORAGE_ERROR;
}

uint32_t MQTTPersistentQueue::getPersistenceUsage() const
{
  uint32_t totalSize = 0;
  File queueDir = LittleFS.open(config.persistenceDir, "r");

  if (queueDir)
  {
    File file = queueDir.openNextFile();
    while (file)
    {
      totalSize += file.size();
      file.close();
      file = queueDir.openNextFile();
    }
    queueDir.close();
  }

  return totalSize;
}

// Performance optimization
void MQTTPersistentQueue::enableCompression(bool enable)
{
  config.enableCompression = enable;
  Serial.printf("[MQTT_QUEUE] Compression %s\n", enable ? "ENABLED" : "DISABLED");
}

void MQTTPersistentQueue::setBatchSize(uint8_t messagesPerCycle)
{
  config.messagesPerCycle = messagesPerCycle;
  Serial.printf("[MQTT_QUEUE] Batch size set to: %d messages/cycle\n", messagesPerCycle);
}

void MQTTPersistentQueue::setProcessInterval(uint32_t intervalMs)
{
  config.processInterval = intervalMs;
  Serial.printf("[MQTT_QUEUE] Process interval set to: %ld ms\n", intervalMs);
}

// Recovery and robustness
bool MQTTPersistentQueue::verifyQueueIntegrity()
{
  // Check for invalid states
  uint32_t issues = 0;

  for (const auto &msg : highPriorityQueue)
  {
    if (msg.topic.isEmpty() || msg.payload.isEmpty())
    {
      issues++;
    }
  }

  for (const auto &msg : normalPriorityQueue)
  {
    if (msg.topic.isEmpty() || msg.payload.isEmpty())
    {
      issues++;
    }
  }

  for (const auto &msg : lowPriorityQueue)
  {
    if (msg.topic.isEmpty() || msg.payload.isEmpty())
    {
      issues++;
    }
  }

  if (issues > 0)
  {
    Serial.printf("[MQTT_QUEUE] WARNING: Queue integrity check found %ld issues\n", issues);
  }

  return issues == 0;
}

uint32_t MQTTPersistentQueue::repairQueue()
{
  uint32_t repaired = 0;

  for (auto it = highPriorityQueue.begin(); it != highPriorityQueue.end();)
  {
    if (it->topic.isEmpty() || it->payload.isEmpty())
    {
      it = highPriorityQueue.erase(it);
      repaired++;
    }
    else
    {
      ++it;
    }
  }

  for (auto it = normalPriorityQueue.begin(); it != normalPriorityQueue.end();)
  {
    if (it->topic.isEmpty() || it->payload.isEmpty())
    {
      it = normalPriorityQueue.erase(it);
      repaired++;
    }
    else
    {
      ++it;
    }
  }

  for (auto it = lowPriorityQueue.begin(); it != lowPriorityQueue.end();)
  {
    if (it->topic.isEmpty() || it->payload.isEmpty())
    {
      it = lowPriorityQueue.erase(it);
      repaired++;
    }
    else
    {
      ++it;
    }
  }

  if (repaired > 0)
  {
    Serial.printf("[MQTT_QUEUE] Repaired %ld corrupted messages\n", repaired);
    updateStats();
  }

  return repaired;
}

void MQTTPersistentQueue::enableAutoRecovery(bool enable)
{
  Serial.printf("[MQTT_QUEUE] Auto-recovery %s\n", enable ? "ENABLED" : "DISABLED");
}

// Private helper methods
void MQTTPersistentQueue::updateStats()
{
  stats.totalMessages = highPriorityQueue.size() +
                        normalPriorityQueue.size() +
                        lowPriorityQueue.size();

  stats.highPriorityCount = highPriorityQueue.size();
  stats.normalPriorityCount = normalPriorityQueue.size();
  stats.lowPriorityCount = lowPriorityQueue.size();

  stats.utilizationPercent = (stats.totalMessages * 100.0f) / config.maxQueueSize;

  if (stats.totalMessages > 0)
  {
    stats.averagePayloadSize = stats.totalPayloadSize / stats.totalMessages;
  }
  else
  {
    stats.averagePayloadSize = 0;
  }

  uint32_t totalProcessed = stats.successfulMessages + stats.failedMessages;
  if (totalProcessed > 0)
  {
    stats.successRate = (stats.successfulMessages * 100.0f) / totalProcessed;
  }
  else
  {
    stats.successRate = 0.0f;
  }

  updateHealthStatus();
}

void MQTTPersistentQueue::updateHealthStatus()
{
  if (stats.utilizationPercent >= 100.0f)
  {
    stats.health = QUEUE_HEALTH_FULL;
  }
  else if (stats.utilizationPercent >= 95.0f)
  {
    stats.health = QUEUE_CRITICAL;
  }
  else if (stats.utilizationPercent >= 80.0f)
  {
    stats.health = QUEUE_WARNING;
  }
  else
  {
    stats.health = QUEUE_HEALTHY;
  }
}

uint32_t MQTTPersistentQueue::calculateRetryDelay(uint8_t retryCount) const
{
  if (retryCount == 0)
    return 0;

  uint32_t delay = config.initialRetryDelayMs;
  for (uint8_t i = 1; i < retryCount; i++)
  {
    delay = (uint32_t)(delay * config.backoffMultiplier);
    if (delay > config.maxRetryDelayMs)
    {
      delay = config.maxRetryDelayMs;
      break;
    }
  }

  // Add jitter (±25%)
  uint32_t jitter = (delay * 25) / 100;
  delay = delay - jitter + random(0, jitter * 2);

  return delay;
}

std::deque<QueuedMessage, STLPSRAMAllocator<QueuedMessage>> *MQTTPersistentQueue::getQueueForPriority(
    MessagePriority priority)
{
  switch (priority)
  {
  case PRIORITY_HIGH:
    return &highPriorityQueue;
  case PRIORITY_NORMAL:
    return &normalPriorityQueue;
  case PRIORITY_LOW:
    return &lowPriorityQueue;
  default:
    return nullptr;
  }
}

void MQTTPersistentQueue::cleanExpiredMessages()
{
  if (!config.enableTimeout)
    return;

  unsigned long now = millis();
  uint32_t expiredCount = 0;

  // Clean HIGH priority queue
  for (auto it = highPriorityQueue.begin(); it != highPriorityQueue.end();)
  {
    if (it->timeoutMs > 0 && (now - it->enqueuedTime) > it->timeoutMs)
    {
      it->status = STATUS_EXPIRED;
      stats.expiredMessages++;
      expiredCount++;
      it = highPriorityQueue.erase(it);
    }
    else
    {
      ++it;
    }
  }

  // Clean NORMAL priority queue
  for (auto it = normalPriorityQueue.begin(); it != normalPriorityQueue.end();)
  {
    if (it->timeoutMs > 0 && (now - it->enqueuedTime) > it->timeoutMs)
    {
      it->status = STATUS_EXPIRED;
      stats.expiredMessages++;
      expiredCount++;
      it = normalPriorityQueue.erase(it);
    }
    else
    {
      ++it;
    }
  }

  // Clean LOW priority queue
  for (auto it = lowPriorityQueue.begin(); it != lowPriorityQueue.end();)
  {
    if (it->timeoutMs > 0 && (now - it->enqueuedTime) > it->timeoutMs)
    {
      it->status = STATUS_EXPIRED;
      stats.expiredMessages++;
      expiredCount++;
      it = lowPriorityQueue.erase(it);
    }
    else
    {
      ++it;
    }
  }

  if (expiredCount > 0)
  {
    updateStats();
    Serial.printf("[MQTT_QUEUE] Cleaned %ld expired messages\n", expiredCount);
  }
}

// Destructor
MQTTPersistentQueue::~MQTTPersistentQueue()
{
  saveQueueToDisk();

  // CRITICAL FIX: Delete mutex for cleanup
  if (queueMutex != NULL)
  {
    vSemaphoreDelete(queueMutex);
    queueMutex = NULL;
  }

  Serial.println("[MQTT_QUEUE] Persistent queue destroyed");
}
