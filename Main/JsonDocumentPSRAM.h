/*
 * PSRAM Allocator for ArduinoJson v7
 *
 * Custom allocator that uses PSRAM instead of DRAM for JsonDocument allocations
 *
 * BUG #31: ArduinoJson v7 doesn't allow overriding DefaultAllocator (class already defined in library).
 *          Instead, we create a custom allocator and wrapper class.
 *
 * USAGE:
 *   Instead of: JsonDocument doc;
 *   Use:        SpiRamJsonDocument doc;
 */

#ifndef JSON_DOCUMENT_PSRAM_H
#define JSON_DOCUMENT_PSRAM_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>

namespace ArduinoJson
{

  // Custom PSRAM Allocator (different name to avoid redefinition error)
  class PSRAMAllocator : public Allocator
  {
  public:
    void *allocate(size_t size) override
    {
      // Try PSRAM first (8MB available)
      void *ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

      if (ptr)
      {
        // Success - allocated in PSRAM (comment out to reduce log noise)
        // Serial.printf("[JSON] Allocated %u bytes in PSRAM\n", size);
        return ptr;
      }

      // Fallback to DRAM only if PSRAM fails
      ptr = heap_caps_malloc(size, MALLOC_CAP_8BIT);

      if (ptr)
      {
        Serial.printf("[JSON] WARNING: Allocated %u bytes in DRAM (PSRAM failed!)\n", size);
      }
      else
      {
        Serial.printf("[JSON] CRITICAL: Failed to allocate %u bytes\n", size);
      }

      return ptr;
    }

    void deallocate(void *ptr) override
    {
      if (ptr)
      {
        heap_caps_free(ptr);
      }
    }

    void *reallocate(void *ptr, size_t new_size) override
    {
      if (!ptr)
        return allocate(new_size);
      if (new_size == 0)
      {
        deallocate(ptr);
        return nullptr;
      }

      // Try realloc in PSRAM
      void *new_ptr = heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      if (new_ptr)
        return new_ptr;

      // Fallback: allocate new + copy + free old
      new_ptr = allocate(new_size);
      if (new_ptr && ptr)
      {
        size_t old_size = heap_caps_get_allocated_size(ptr);
        size_t copy_size = (old_size < new_size) ? old_size : new_size;
        memcpy(new_ptr, ptr, copy_size);
        deallocate(ptr);
      }

      return new_ptr;
    }

    static PSRAMAllocator *instance()
    {
      static PSRAMAllocator allocator;
      return &allocator;
    }

  private:
    PSRAMAllocator() = default;
  };

} // namespace ArduinoJson

// Wrapper class that uses PSRAM allocator by default
class SpiRamJsonDocument : public ArduinoJson::JsonDocument
{
public:
  SpiRamJsonDocument()
      : ArduinoJson::JsonDocument(ArduinoJson::PSRAMAllocator::instance())
  {
  }

  // Copy constructor
  SpiRamJsonDocument(const SpiRamJsonDocument &src)
      : ArduinoJson::JsonDocument(ArduinoJson::PSRAMAllocator::instance())
  {
    set(src);
  }

  // Move constructor
  SpiRamJsonDocument(SpiRamJsonDocument &&src)
      : ArduinoJson::JsonDocument(std::move(src))
  {
  }

  // Assignment operators
  SpiRamJsonDocument &operator=(const SpiRamJsonDocument &src)
  {
    ArduinoJson::JsonDocument::operator=(src);
    return *this;
  }

  SpiRamJsonDocument &operator=(SpiRamJsonDocument &&src)
  {
    ArduinoJson::JsonDocument::operator=(std::move(src));
    return *this;
  }
};

// MACRO: Transparently replace JsonDocument with SpiRamJsonDocument
// This allows existing code to use JsonDocument without changes
#define JsonDocument SpiRamJsonDocument

#endif // JSON_DOCUMENT_PSRAM_H
