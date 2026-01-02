# Firmware v2.5.20 - OTA Stability Improvements

**Release Date:** 2025-12-02  
**Build Number:** 2520

## 🔧 Bug Fixes

### OTA Download Stability

- **Fixed:** Connection timeout during final download chunks (97%+ completion)
  - Increased "no data" timeout from 1s to 5s for slow connections
  - Prevents premature connection closure when download is nearly complete
- **Optimized:** SSL buffer size for better stability
  - Reduced `ESP_SSLCLIENT_BUFFER_SIZE` from 16KB to 8KB
  - Improves connection stability with slower network speeds (~17 KB/s)
  - Better memory management for long-running downloads

## 📊 Technical Details

### Modified Files

- `Main/OTAHttps.cpp` - Connection timeout logic
- `Main/OTAHttps.h` - SSL buffer configuration
- `Main/Main.ino` - Version bump to 2.5.20
- `Main/CRUDHandler.cpp` - Version strings updated

### Issue Addressed

- **Problem:** OTA downloads failing at 97% completion (1.96 MB / 2.01 MB)
- **Root Cause:** SSL connection closed by server during idle periods
- **Solution:** Extended timeout window for final data chunks

### Testing Notes

- Download speed: ~17.7 KB/s (tested)
- Firmware size: 2,010,064 bytes (1.92 MB)
- Expected download time: ~115 seconds
- Timeout buffer: 5 seconds (sufficient for 48 KB at 17 KB/s)

## 🔄 Upgrade Path

From v2.5.19:

1. Flash v2.5.20 manually via USB/Serial
2. Test OTA update mechanism
3. Deploy to production devices via OTA

## ⚠️ Known Issues

None

## 📝 Changelog Summary

```
v2.5.20: OTA timeout fixes & SSL buffer optimization
v2.5.19: Critical fixes (RTU/TCP/HTTP tasks) & MQTT Optimization
```
