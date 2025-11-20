#ifndef PSRAM_STRING_H
#define PSRAM_STRING_H

#include <Arduino.h>
#include <cstring>
#include <esp_heap_caps.h>

/**
 * @brief PSRAMString - Drop-in replacement for Arduino String using PSRAM
 *
 * BUG #31 ROOT CAUSE: Arduino String class ALWAYS allocates in DRAM.
 * This class provides identical API but allocates string data in PSRAM.
 *
 * Benefits:
 * - Saves DRAM for critical operations (WiFi, BLE, ISR, stacks)
 * - Reduces memory fragmentation
 * - Compatible with existing String-based code
 *
 * Usage:
 *   PSRAMString deviceId = "D7A3F2";  // Allocated in PSRAM
 *   const char* cstr = deviceId.c_str();
 *   if (deviceId == "D7A3F2") { ... }
 */
class PSRAMString {
private:
    char* buffer;
    size_t len;
    size_t capacity;

    // Allocate buffer in PSRAM with DRAM fallback
    char* allocate(size_t size) {
        // Try PSRAM first (8MB available)
        void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (ptr) {
            return (char*)ptr;
        }

        // Fallback to DRAM if PSRAM fails
        ptr = heap_caps_malloc(size, MALLOC_CAP_8BIT);
        if (!ptr) {
            Serial.printf("[PSRAMString] CRITICAL: Failed to allocate %u bytes\n", size);
        }
        return (char*)ptr;
    }

    // Ensure capacity for new length
    void reserve(size_t newCapacity) {
        if (newCapacity <= capacity) {
            return;
        }

        // Allocate new buffer (round up to next power of 2 for efficiency)
        size_t allocSize = 16;
        while (allocSize < newCapacity + 1) {
            allocSize *= 2;
        }

        char* newBuffer = allocate(allocSize);
        if (!newBuffer) {
            return;  // Keep old buffer on allocation failure
        }

        // Copy existing data
        if (buffer && len > 0) {
            memcpy(newBuffer, buffer, len + 1);
        } else {
            newBuffer[0] = '\0';
        }

        // Free old buffer
        if (buffer) {
            heap_caps_free(buffer);
        }

        buffer = newBuffer;
        capacity = allocSize - 1;
    }

public:
    // Constructor - empty string
    PSRAMString() : buffer(nullptr), len(0), capacity(0) {
        reserve(16);  // Default 16 bytes
    }

    // Constructor - from C string
    PSRAMString(const char* str) : buffer(nullptr), len(0), capacity(0) {
        if (str) {
            len = strlen(str);
            reserve(len);
            if (buffer) {
                strcpy(buffer, str);
            }
        } else {
            reserve(16);
        }
    }

    // Constructor - from Arduino String
    PSRAMString(const String& str) : buffer(nullptr), len(0), capacity(0) {
        len = str.length();
        reserve(len);
        if (buffer) {
            strcpy(buffer, str.c_str());
        }
    }

    // Copy constructor
    PSRAMString(const PSRAMString& other) : buffer(nullptr), len(0), capacity(0) {
        len = other.len;
        reserve(len);
        if (buffer && other.buffer) {
            strcpy(buffer, other.buffer);
        }
    }

    // Move constructor
    PSRAMString(PSRAMString&& other) noexcept
        : buffer(other.buffer), len(other.len), capacity(other.capacity) {
        other.buffer = nullptr;
        other.len = 0;
        other.capacity = 0;
    }

    // Destructor
    ~PSRAMString() {
        if (buffer) {
            heap_caps_free(buffer);
        }
    }

    // Assignment operators
    PSRAMString& operator=(const char* str) {
        if (str) {
            len = strlen(str);
            reserve(len);
            if (buffer) {
                strcpy(buffer, str);
            }
        } else {
            clear();
        }
        return *this;
    }

    PSRAMString& operator=(const String& str) {
        len = str.length();
        reserve(len);
        if (buffer) {
            strcpy(buffer, str.c_str());
        }
        return *this;
    }

    PSRAMString& operator=(const PSRAMString& other) {
        if (this != &other) {
            len = other.len;
            reserve(len);
            if (buffer && other.buffer) {
                strcpy(buffer, other.buffer);
            }
        }
        return *this;
    }

    PSRAMString& operator=(PSRAMString&& other) noexcept {
        if (this != &other) {
            if (buffer) {
                heap_caps_free(buffer);
            }
            buffer = other.buffer;
            len = other.len;
            capacity = other.capacity;
            other.buffer = nullptr;
            other.len = 0;
            other.capacity = 0;
        }
        return *this;
    }

    // Comparison operators
    bool operator==(const char* str) const {
        if (!buffer || !str) return false;
        return strcmp(buffer, str) == 0;
    }

    bool operator==(const String& str) const {
        return operator==(str.c_str());
    }

    bool operator==(const PSRAMString& other) const {
        if (!buffer || !other.buffer) return false;
        return strcmp(buffer, other.buffer) == 0;
    }

    bool operator!=(const char* str) const {
        return !operator==(str);
    }

    bool operator!=(const String& str) const {
        return !operator==(str);
    }

    bool operator!=(const PSRAMString& other) const {
        return !operator==(other);
    }

    // Concatenation operators
    PSRAMString& operator+=(const char* str) {
        if (str) {
            size_t addLen = strlen(str);
            reserve(len + addLen);
            if (buffer) {
                strcat(buffer, str);
                len += addLen;
            }
        }
        return *this;
    }

    PSRAMString& operator+=(const String& str) {
        return operator+=(str.c_str());
    }

    PSRAMString& operator+=(const PSRAMString& other) {
        if (other.buffer) {
            return operator+=(other.buffer);
        }
        return *this;
    }

    PSRAMString operator+(const char* str) const {
        PSRAMString result(*this);
        result += str;
        return result;
    }

    PSRAMString operator+(const String& str) const {
        return operator+(str.c_str());
    }

    PSRAMString operator+(const PSRAMString& other) const {
        PSRAMString result(*this);
        result += other;
        return result;
    }

    // Arduino String compatibility methods
    const char* c_str() const {
        return buffer ? buffer : "";
    }

    size_t length() const {
        return len;
    }

    bool isEmpty() const {
        return len == 0;
    }

    void clear() {
        if (buffer) {
            buffer[0] = '\0';
        }
        len = 0;
    }

    // Convert to Arduino String (for legacy code compatibility)
    String toString() const {
        return String(buffer ? buffer : "");
    }

    // Implicit conversion to const char* (for printf, Serial.print, etc.)
    operator const char*() const {
        return c_str();
    }

    // Indexing operator
    char operator[](size_t index) const {
        if (buffer && index < len) {
            return buffer[index];
        }
        return '\0';
    }

    // Substring
    PSRAMString substring(size_t start, size_t end = 0) const {
        if (!buffer || start >= len) {
            return PSRAMString();
        }
        if (end == 0 || end > len) {
            end = len;
        }

        size_t subLen = end - start;
        char* temp = (char*)alloca(subLen + 1);
        strncpy(temp, buffer + start, subLen);
        temp[subLen] = '\0';
        return PSRAMString(temp);
    }

    // Find
    int indexOf(const char* str) const {
        if (!buffer || !str) return -1;
        char* pos = strstr(buffer, str);
        return pos ? (pos - buffer) : -1;
    }

    // Memory diagnostics
    bool isInPSRAM() const {
        if (!buffer) return false;
        // Check if pointer is in PSRAM address range (ESP32-S3 specific)
        // PSRAM starts at 0x3C000000 for ESP32-S3
        uintptr_t addr = (uintptr_t)buffer;
        return (addr >= 0x3C000000 && addr < 0x3E000000);
    }

    size_t getCapacity() const {
        return capacity;
    }

    void printMemoryInfo() const {
        Serial.printf("[PSRAMString] Length: %u, Capacity: %u, In PSRAM: %s\n",
                      len, capacity, isInPSRAM() ? "YES" : "NO (DRAM)");
    }
};

#endif // PSRAM_STRING_H
