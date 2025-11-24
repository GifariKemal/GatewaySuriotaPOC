/*
 * PSRAM Allocator for ArduinoJson v7 + STL Containers
 *
 * Comprehensive PSRAM allocation support:
 * - ArduinoJson PSRAMAllocator (from JsonDocumentPSRAM.h)
 * - PSRAMString class (from PSRAMString.h)
 * - STLPSRAMAllocator for std::deque, std::vector, etc.
 *
 * CRITICAL FIX: Prevents DRAM exhaustion in MQTTPersistentQueue
 *
 * BUG #31: DRAM exhaustion fix - redirect allocations to PSRAM
 */

#ifndef PSRAM_ALLOCATOR_H
#define PSRAM_ALLOCATOR_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>

// Include existing PSRAM allocator definitions to avoid duplication
#include "JsonDocumentPSRAM.h" // Provides ArduinoJson::PSRAMAllocator
#include "PSRAMString.h"        // Provides PSRAMString class

// =============================================================================
// STL-COMPATIBLE PSRAM ALLOCATOR (for std::deque, std::vector, etc.)
// =============================================================================

#include <memory>
#include <limits>

/**
 * @brief Custom STL allocator for using PSRAM with STL containers
 *
 * This allocator attempts to allocate memory from PSRAM first, with automatic
 * fallback to DRAM if PSRAM is exhausted. This prevents OOM crashes when
 * using large STL containers like std::deque, std::vector, etc.
 *
 * CRITICAL FIX: Prevents DRAM exhaustion in MQTTPersistentQueue
 *
 * Usage:
 *   std::deque<MyStruct, STLPSRAMAllocator<MyStruct>> myQueue;
 *   std::vector<int, STLPSRAMAllocator<int>> myVector;
 *
 * @tparam T Type of elements to allocate
 */
template <typename T>
class STLPSRAMAllocator
{
public:
  // STL allocator type definitions (required)
  using value_type = T;
  using pointer = T *;
  using const_pointer = const T *;
  using reference = T &;
  using const_reference = const T &;
  using size_type = size_t;
  using difference_type = ptrdiff_t;

  // Rebind allocator to different type (required for STL)
  template <typename U>
  struct rebind
  {
    using other = STLPSRAMAllocator<U>;
  };

  // Constructors
  STLPSRAMAllocator() noexcept = default;

  template <typename U>
  STLPSRAMAllocator(const STLPSRAMAllocator<U> &) noexcept {}

  // Destructor
  ~STLPSRAMAllocator() = default;

  /**
   * @brief Allocate memory for n objects of type T
   *
   * Attempts PSRAM allocation first, falls back to DRAM if PSRAM exhausted.
   *
   * @param n Number of objects to allocate
   * @return Pointer to allocated memory
   * @throws std::bad_alloc if both PSRAM and DRAM exhausted
   */
  T *allocate(size_type n)
  {
    if (n == 0)
    {
      return nullptr;
    }

    // Check for overflow
    if (n > std::numeric_limits<size_type>::max() / sizeof(T))
    {
      throw std::bad_alloc();
    }

    size_t bytes = n * sizeof(T);

    // Try PSRAM first (preferred)
    void *ptr = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (ptr)
    {
      // Success - allocated in PSRAM
#ifdef DEBUG_PSRAM_ALLOCATOR
      Serial.printf("[STL_PSRAM_ALLOC] Allocated %d bytes in PSRAM\n", bytes);
#endif
      return static_cast<T *>(ptr);
    }

    // PSRAM exhausted - try DRAM fallback
    ptr = heap_caps_malloc(bytes, MALLOC_CAP_8BIT);

    if (ptr)
    {
      // Fallback successful - log warning (throttled to avoid spam)
      static unsigned long lastWarning = 0;
      if (millis() - lastWarning > 30000) // Max once per 30s
      {
        Serial.printf("[STL_PSRAM_ALLOC] WARNING: PSRAM exhausted, using DRAM fallback (%d bytes)\n", bytes);
        lastWarning = millis();
      }
      return static_cast<T *>(ptr);
    }

    // Both PSRAM and DRAM exhausted - critical error
    Serial.printf("[STL_PSRAM_ALLOC] CRITICAL ERROR: Memory allocation failed (%d bytes)\n", bytes);
    throw std::bad_alloc();
  }

  /**
   * @brief Deallocate memory previously allocated with allocate()
   *
   * @param ptr Pointer to memory to deallocate
   * @param n Number of objects (ignored, but required for STL)
   */
  void deallocate(T *ptr, size_type n) noexcept
  {
    (void)n; // Suppress unused parameter warning
    if (ptr)
    {
      heap_caps_free(ptr);
    }
  }

  /**
   * @brief Get maximum allocation size
   *
   * @return Maximum number of objects that can be allocated
   */
  size_type max_size() const noexcept
  {
    return std::numeric_limits<size_type>::max() / sizeof(T);
  }

  /**
   * @brief Check if two allocators are equal (always true for stateless allocators)
   */
  template <typename U>
  bool operator==(const STLPSRAMAllocator<U> &) const noexcept
  {
    return true;
  }

  /**
   * @brief Check if two allocators are not equal (always false for stateless allocators)
   */
  template <typename U>
  bool operator!=(const STLPSRAMAllocator<U> &) const noexcept
  {
    return false;
  }
};

// PSRAMString class is now defined in PSRAMString.h (included above)
// to avoid duplicate definitions

#endif // PSRAM_ALLOCATOR_H
