#ifndef QUEUE_MANAGER_H
#define QUEUE_MANAGER_H

#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

class QueueManager
{
private:
  static QueueManager *instance;
  QueueHandle_t dataQueue;
  QueueHandle_t streamQueue;
  SemaphoreHandle_t queueMutex;
  SemaphoreHandle_t streamMutex;
  static const int MAX_QUEUE_SIZE = 100;
  static const int MAX_STREAM_QUEUE_SIZE = 50;

  // Configurable mutex timeout (milliseconds)
  uint32_t queueMutexTimeout = 100; // Default timeout for queue operations (100ms)
  uint32_t streamMutexTimeout = 10; // Default timeout for stream operations (10ms)

  QueueManager();

public:
  static QueueManager *getInstance();

  bool init();
  // Using const reference to avoid copying large JsonObject
  bool enqueue(const JsonObject &dataPoint);
  bool dequeue(JsonObject &dataPoint);
  bool peek(const JsonObject &dataPoint) const;
  bool isEmpty() const;
  bool isFull() const;
  int size() const;
  void clear();
  void getStats(JsonObject &stats) const;

  // Priority 1: Flush queue data for deleted device
  int flushDeviceData(const String &deviceId);

  // Streaming queue methods
  bool enqueueStream(const JsonObject &dataPoint);
  bool dequeueStream(JsonObject &dataPoint);
  bool isStreamEmpty() const;
  void clearStream();

  // Configurable timeout methods
  void setQueueMutexTimeout(uint32_t timeoutMs);
  void setStreamMutexTimeout(uint32_t timeoutMs);
  uint32_t getQueueMutexTimeout() const;
  uint32_t getStreamMutexTimeout() const;

  ~QueueManager();
};

#endif