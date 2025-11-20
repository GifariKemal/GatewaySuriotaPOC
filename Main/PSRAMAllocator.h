/*
 * PSRAM Allocator for ArduinoJson v7
 *
 * Forces JsonDocument to use PSRAM instead of DRAM heap
 * Critical for preventing DRAM exhaustion with large JSON documents
 *
 * BUG #31: DRAM exhaustion fix - redirect JsonDocument allocations to PSRAM
 */

#ifndef PSRAM_ALLOCATOR_H
#define PSRAM_ALLOCATOR_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>

namespace ArduinoJson {

// Custom allocator that uses PSRAM instead of DRAM
class PSRAMAllocator : public Allocator {
public:
  void* allocate(size_t size) override {
    // Try PSRAM first
    void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (ptr) {
      #ifdef DEBUG_PSRAM_ALLOCATOR
      Serial.printf("[PSRAM_ALLOC] Allocated %u bytes in PSRAM\n", size);
      #endif
      return ptr;
    }

    // Fallback to DRAM if PSRAM allocation fails
    ptr = heap_caps_malloc(size, MALLOC_CAP_8BIT);

    if (ptr) {
      #ifdef DEBUG_PSRAM_ALLOCATOR
      Serial.printf("[PSRAM_ALLOC] PSRAM failed, fallback to DRAM (%u bytes)\n", size);
      #endif
    } else {
      Serial.printf("[PSRAM_ALLOC] ERROR: Failed to allocate %u bytes (both PSRAM and DRAM)\n", size);
    }

    return ptr;
  }

  void deallocate(void* ptr) override {
    if (ptr) {
      heap_caps_free(ptr);
    }
  }

  void* reallocate(void* ptr, size_t new_size) override {
    // Determine if pointer is in PSRAM or DRAM
    void* new_ptr = nullptr;

    if (heap_caps_get_allocated_size(ptr) > 0) {
      // Try to reallocate in same memory type
      if (esp_ptr_in_spiram(ptr)) {
        new_ptr = heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      } else {
        new_ptr = heap_caps_realloc(ptr, new_size, MALLOC_CAP_8BIT);
      }
    }

    if (!new_ptr && new_size > 0) {
      // Realloc failed, try new allocation
      new_ptr = allocate(new_size);
      if (new_ptr && ptr) {
        // Copy old data
        size_t old_size = heap_caps_get_allocated_size(ptr);
        size_t copy_size = (old_size < new_size) ? old_size : new_size;
        memcpy(new_ptr, ptr, copy_size);
        deallocate(ptr);
      }
    }

    return new_ptr;
  }

  // Singleton instance
  static PSRAMAllocator* instance() {
    static PSRAMAllocator allocator;
    return &allocator;
  }

private:
  PSRAMAllocator() = default;
};

} // namespace ArduinoJson

// Helper macro to create JsonDocument with PSRAM allocator
#define JsonDocumentPSRAM() JsonDocument(ArduinoJson::PSRAMAllocator::instance())

#endif // PSRAM_ALLOCATOR_H
