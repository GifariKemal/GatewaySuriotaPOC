# Firmware Changelog - v2.5.27 (Stable)

**Release Date:** 2025-12-02
**Version:** 2.5.27
**Build Number:** 2527

## 🚀 Major Improvements: OTA Stability

This release introduces a definitive fix for OTA download failures on slow connections, specifically addressing the "98% stuck" issue.

### 🔧 Key Fixes

1.  **HTTP Range Resume Logic**
    *   Implemented automatic download resumption if the connection is dropped.
    *   If a download fails (e.g., at 98%), the device now reconnects and requests only the missing bytes using the `Range` HTTP header.
    *   Eliminates "Connection closed unexpectedly" errors caused by server idle timeouts.

2.  **Chunked Encoding Handling**
    *   Improved handling of GitHub Contents API responses which use chunked transfer encoding.
    *   Download loop now strictly adheres to the `targetSize` defined in the manifest, ignoring misleading `Content-Length` headers from chunked streams.

3.  **Over-Download Prevention**
    *   Added logic to truncate read operations to ensure the downloaded file size exactly matches the manifest size.
    *   Prevents "Incomplete download" errors caused by reading extra metadata bytes during resume.

4.  **Watchdog Timeout (WDT) Prevention**
    *   Added `vTaskDelay(1)` and `esp_task_wdt_reset()` calls within critical loops (header parsing, SSL handshake, main download loop).
    *   Prevents system crashes/reboots during long operations or slow network conditions.

### 📦 Upgrade Path

*   **From v2.5.20+:** Direct OTA update supported.
*   **From < v2.5.0:** Requires intermediate update or factory flash.

### 🧪 Verification

*   **Tested Scenario:** Download interrupted at ~97% due to network timeout.
*   **Result:** Device successfully detected connection loss, resumed download from the last byte, and completed verification with 100% integrity.
