/*
 * Global PSRAM Allocator for ArduinoJson v7
 *
 * Overrides default allocator so ALL JsonDocument instances use PSRAM automatically
 *
 * BUG #31: Comprehensive fix - ALL JsonDocument → PSRAM (not just 3 instances)
 *
 * CRITICAL FIX: ArduinoJson v7 uses detail::DefaultAllocator, not DefaultAllocator!
 */

#ifndef JSON_DOCUMENT_PSRAM_H
#define JSON_DOCUMENT_PSRAM_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>

namespace ArduinoJson {
namespace detail {  // CRITICAL: Must be in detail namespace for v7!

// Override default allocator to use PSRAM
class DefaultAllocator : public Allocator {
public:
  void* allocate(size_t size) override {
    // Try PSRAM first (8MB available, plenty of space!)
    void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (ptr) {
      Serial.printf("[JSON] Allocated %u bytes in PSRAM\n", size);
      return ptr;
    }

    // Fallback to DRAM only if PSRAM fails
    ptr = heap_caps_malloc(size, MALLOC_CAP_8BIT);

    if (ptr) {
      Serial.printf("[JSON] WARNING: Allocated %u bytes in DRAM (PSRAM failed!)\n", size);
    } else {
      Serial.printf("[JSON] CRITICAL: Failed to allocate %u bytes (PSRAM and DRAM exhausted!)\n", size);
    }

    return ptr;
  }

  void deallocate(void* ptr) override {
    if (ptr) {
      heap_caps_free(ptr);
    }
  }

  void* reallocate(void* ptr, size_t new_size) override {
    if (!ptr) return allocate(new_size);
    if (new_size == 0) {
      deallocate(ptr);
      return nullptr;
    }

    // Try realloc in PSRAM
    void* new_ptr = heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (new_ptr) return new_ptr;

    // Fallback: allocate new + copy + free old
    new_ptr = allocate(new_size);
    if (new_ptr && ptr) {
      size_t old_size = heap_caps_get_allocated_size(ptr);
      size_t copy_size = (old_size < new_size) ? old_size : new_size;
      memcpy(new_ptr, ptr, copy_size);
      deallocate(ptr);
    }

    return new_ptr;
  }

  static DefaultAllocator* instance() {
    static DefaultAllocator allocator;
    return &allocator;
  }

private:
  DefaultAllocator() = default;
};

} // namespace detail
} // namespace ArduinoJson

#endif // JSON_DOCUMENT_PSRAM_H
