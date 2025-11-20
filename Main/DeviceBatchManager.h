#ifndef DEVICE_BATCH_MANAGER_H
#define DEVICE_BATCH_MANAGER_H

#include <Arduino.h>
#include <map>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "DebugConfig.h"

/*
 * Device Batch Manager
 *
 * Purpose: Track register batch completion for MQTT publishing
 *
 * Issue: MQTT was dequeuing registers before all were read/enqueued
 * - Device has 50 registers
 * - RTU reads and enqueues them one by one (takes ~50 seconds)
 * - MQTT publishes after 20 seconds (only gets 18-20 registers)
 * - Result: Incomplete data published
 *
 * Solution: Batch tracking
 * - RTU calls startBatch(deviceId, 50) before reading
 * - RTU calls incrementEnqueued(deviceId) after each register enqueued
 * - MQTT calls hasCompleteBatch() before dequeuing
 * - Only dequeue when batchEnqueued == batchExpected
 * - MQTT calls clearBatch(deviceId) after publishing
 */

class DeviceBatchManager
{
private:
    static DeviceBatchManager *instance;
    SemaphoreHandle_t batchMutex;

    struct BatchInfo
    {
        int expected;   // How many registers expected
        int attempted;  // How many registers attempted (success + failure)
        int enqueued;   // How many registers successfully enqueued
        int failed;     // How many registers failed to read
        unsigned long startTime; // When batch started
        bool complete;  // Is batch complete?

        BatchInfo() : expected(0), attempted(0), enqueued(0), failed(0), startTime(0), complete(false) {}
    };

    std::map<String, BatchInfo> deviceBatches;

    // Private constructor (singleton)
    DeviceBatchManager()
    {
        batchMutex = xSemaphoreCreateMutex();
        if (!batchMutex)
        {
            Serial.println("[BATCH] ERROR: Failed to create mutex");
        }
    }

public:
    // Singleton access
    static DeviceBatchManager *getInstance()
    {
        if (!instance)
        {
            instance = new DeviceBatchManager();
        }
        return instance;
    }

    // Start batch tracking for a device
    void startBatch(const String &deviceId, int registerCount)
    {
        if (xSemaphoreTake(batchMutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        {
            Serial.println("[BATCH] ERROR: Mutex timeout in startBatch");
            return;
        }

        BatchInfo &batch = deviceBatches[deviceId];
        batch.expected = registerCount;
        batch.attempted = 0;
        batch.enqueued = 0;
        batch.failed = 0;
        batch.startTime = millis();
        batch.complete = false;

        LOG_BATCH_INFO("Started tracking %s: expecting %d registers\n",
                       deviceId.c_str(), registerCount);

        xSemaphoreGive(batchMutex);
    }

    // Increment enqueued count for a device (successful read)
    void incrementEnqueued(const String &deviceId)
    {
        if (xSemaphoreTake(batchMutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        {
            LOG_BATCH_ERROR("Mutex timeout in incrementEnqueued\n");
            return;
        }

        auto it = deviceBatches.find(deviceId);
        if (it != deviceBatches.end())
        {
            BatchInfo &batch = it->second;
            batch.attempted++;  // Track attempt
            batch.enqueued++;   // Track success

            // Check if batch is complete (all registers attempted)
            if (batch.attempted >= batch.expected && !batch.complete)
            {
                batch.complete = true;
                unsigned long duration = millis() - batch.startTime;
                LOG_BATCH_INFO("Device %s COMPLETE (%d success, %d failed, %d/%d total, took %lu ms)\n",
                               deviceId.c_str(), batch.enqueued, batch.failed,
                               batch.attempted, batch.expected, duration);
            }
        }

        xSemaphoreGive(batchMutex);
    }

    // Increment failed count for a device (failed read)
    void incrementFailed(const String &deviceId)
    {
        if (xSemaphoreTake(batchMutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        {
            Serial.println("[BATCH] ERROR: Mutex timeout in incrementFailed");
            return;
        }

        auto it = deviceBatches.find(deviceId);
        if (it != deviceBatches.end())
        {
            BatchInfo &batch = it->second;
            batch.attempted++;  // Track attempt
            batch.failed++;     // Track failure

            // Check if batch is complete (all registers attempted)
            if (batch.attempted >= batch.expected && !batch.complete)
            {
                batch.complete = true;
                unsigned long duration = millis() - batch.startTime;
                LOG_BATCH_INFO("Device %s COMPLETE (%d success, %d failed, %d/%d total, took %lu ms)\n",
                               deviceId.c_str(), batch.enqueued, batch.failed,
                               batch.attempted, batch.expected, duration);
            }
        }

        xSemaphoreGive(batchMutex);
    }

    // Check if any device has a complete batch ready for publishing
    bool hasCompleteBatch()
    {
        if (xSemaphoreTake(batchMutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        {
            Serial.println("[BATCH] ERROR: Mutex timeout in hasCompleteBatch");
            return false;
        }

        bool result = false;
        for (const auto &entry : deviceBatches)
        {
            if (entry.second.complete)
            {
                result = true;
                break;
            }
        }

        xSemaphoreGive(batchMutex);
        return result;
    }

    // Check if there are ANY batches being tracked (devices configured)
    bool hasAnyBatches()
    {
        if (xSemaphoreTake(batchMutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        {
            Serial.println("[BATCH] ERROR: Mutex timeout in hasAnyBatches");
            return false;
        }

        bool result = !deviceBatches.empty();

        xSemaphoreGive(batchMutex);
        return result;
    }

    // Clear batch status after publishing (called once per device, not per register)
    void clearBatch(const String &deviceId)
    {
        if (xSemaphoreTake(batchMutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        {
            Serial.println("[BATCH] ERROR: Mutex timeout in clearBatch");
            return;
        }

        auto it = deviceBatches.find(deviceId);
        if (it != deviceBatches.end() && it->second.complete)
        {
            deviceBatches.erase(it);
            LOG_BATCH_INFO("Cleared batch status for device %s\n", deviceId.c_str());
        }

        xSemaphoreGive(batchMutex);
    }

    // Debug: Get batch status
    void printBatchStatus()
    {
        if (xSemaphoreTake(batchMutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        {
            Serial.println("[BATCH] ERROR: Mutex timeout in printBatchStatus");
            return;
        }

        Serial.printf("[BATCH] Active batches: %d\n", deviceBatches.size());
        for (const auto &entry : deviceBatches)
        {
            const BatchInfo &batch = entry.second;
            Serial.printf("  %s: attempted=%d/%d, success=%d, failed=%d (complete=%s)\n",
                          entry.first.c_str(), batch.attempted, batch.expected,
                          batch.enqueued, batch.failed,
                          batch.complete ? "YES" : "NO");
        }

        xSemaphoreGive(batchMutex);
    }

    ~DeviceBatchManager()
    {
        if (batchMutex)
        {
            vSemaphoreDelete(batchMutex);
        }
    }
};

#endif // DEVICE_BATCH_MANAGER_H
