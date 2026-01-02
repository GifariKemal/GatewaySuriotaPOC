# Mockup UI Update Plan - v2.3.0 Features Integration

**Created:** November 21, 2025 **Firmware Version:** 2.3.0 **Goal:** Update all
mockup UI files to reflect firmware features with responsive Tailwind CSS,
interactive animations, and live preview capability

---

## 📋 Current Mockup Files

1. **Device List (Home Screen).html** - Main dashboard showing all devices
2. **Device Details.html** - Individual device view with registers and live
   streaming
3. **Create New Device.html** - Form for creating new devices
4. **Add Register.html** - Form for adding registers to devices
5. **Form Server_Configuration.html** - Server configuration (MQTT, HTTP, WiFi,
   Ethernet)
6. **Logging Config dan Delete Dialog.html** - Logging settings
7. **Streaming.html** - Real-time data streaming view

---

## 🆕 New Features to Integrate (v2.3.0)

### 1. Backup & Restore System

- **API:** `read` → `full_config`, `system` → `restore_config`
- **Features:**
  - Complete configuration export (200KB response support)
  - Save backup to file (JSON download)
  - Restore from backup file
  - Backup metadata (timestamp, firmware version, statistics)
  - PSRAM optimized for large configs

### 2. Factory Reset

- **API:** `system` → `factory_reset`
- **Features:**
  - One-command reset to factory defaults
  - Clears all devices, server config, logging config
  - Automatic device restart
  - Optional reason field for audit trail

### 3. Device Control & Health Metrics

- **API:** `control` → `enable_device`, `disable_device`, `get_device_status`,
  `get_all_device_status`
- **Features:**
  - Manual enable/disable devices
  - Health metrics: success rate, response times (avg/min/max)
  - Disable reasons: NONE, MANUAL, AUTO_RETRY, AUTO_TIMEOUT
  - Auto-recovery every 5 minutes for auto-disabled devices
  - Protocol-agnostic (RTU & TCP)

### 4. Advanced BLE Metrics

- **Features:**
  - MTU metrics (current, max, timeout count)
  - Connection metrics (uptime, fragments, bytes)
  - Queue metrics (depth, peak, utilization)

---

## 🎯 Update Plan by File

### File 1: Device List (Home Screen).html

**Current Features:**

- ✅ List of devices with filter (ALL/RTU/TCP)
- ✅ Search functionality
- ✅ Add device modal
- ✅ Device count display

**Features to Add:**

1. **Device Status Indicators**
   - Green dot: Enabled, healthy (success rate > 95%)
   - Yellow dot: Enabled, issues (success rate 90-95%)
   - Red dot: Enabled, poor performance (success rate < 90%)
   - Gray dot: Disabled

2. **Health Metrics Display**
   - Success rate percentage
   - Average response time
   - Disable reason (if disabled)

3. **Quick Actions**
   - Enable/disable device toggle
   - Quick view metrics

4. **Settings Menu (new)**
   - Backup configuration button
   - Restore configuration button
   - Factory reset button
   - BLE metrics view

**UI Enhancements:**

- Animated status indicators (pulsing dots)
- Skeleton loading states
- Toast notifications for actions
- Responsive grid layout (1 col mobile, 2 col tablet, 3 col desktop)

---

### File 2: Device Details.html

**Current Features:**

- ✅ Device information display
- ✅ Register list with search/sort
- ✅ Live data streaming toggle
- ✅ Register detail modal

**Features to Add:**

1. **Device Control Section**
   - Enable/disable button with confirmation
   - Clear metrics button
   - Status badge (enabled/disabled with reason)

2. **Health Metrics Dashboard**
   - Success rate gauge chart
   - Response time chart (avg/min/max)
   - Total reads, successful reads, failed reads
   - Consecutive failures counter
   - Timeout statistics

3. **Disable Reason Display**
   - Reason type badge (MANUAL, AUTO_RETRY, AUTO_TIMEOUT)
   - Reason detail text
   - Disabled duration timer

4. **Enhanced Register View**
   - Refresh rate override indicator
   - Calibration indicator (scale/offset)
   - Last read value with timestamp

**UI Enhancements:**

- Animated charts (Chart.js or pure CSS)
- Progress bars for metrics
- Color-coded status indicators
- Smooth expand/collapse animations

---

### File 3: Create New Device.html

**Current Features:**

- ✅ Basic device creation form
- ✅ Protocol selection (RTU/TCP)

**Features to Add:**

1. **Complete Form Fields**
   - **Common:** device_name, device_id (auto-generate option), protocol
   - **RTU Specific:** slave_id (1-247), serial_port (1/2), baud_rate (dropdown:
     1200-115200)
   - **TCP Specific:** ip_address, port (502 default)
   - **Common:** timeout_ms, retry_count, refresh_rate_ms
   - **Control:** enabled (toggle, default true)

2. **Form Validation**
   - Device ID uniqueness check
   - Slave ID range validation (1-247)
   - IP address format validation (TCP)
   - Baud rate dropdown (1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200)

3. **Conditional Fields**
   - Show RTU fields only when protocol=RTU
   - Show TCP fields only when protocol=TCP

4. **Preview Panel**
   - Live JSON preview of device configuration
   - Copy JSON button

**UI Enhancements:**

- Multi-step wizard (Step 1: Basic Info, Step 2: Protocol Settings, Step 3:
  Review)
- Animated transitions between steps
- Inline validation errors
- Success animation on save

---

### File 4: Add Register.html

**Current Features:**

- ✅ Basic register form
- ✅ Some data types

**Features to Add:**

1. **Complete Data Type List (40+ types)**
   - INT16, UINT16, INT32_ABCD, INT32_CDAB, etc.
   - FLOAT32_ABCD, FLOAT32_CDAB, FLOAT32_BADC, FLOAT32_DCBA
   - All variants from MODBUS_DATATYPES.md

2. **Additional Fields**
   - register_name, register_id (auto-generate)
   - address (0-65535)
   - function_code (dropdown: 1=Coil, 2=DI, 3=HR, 4=IR)
   - data_type (searchable dropdown with descriptions)
   - quantity (auto-calculate based on data type)
   - refresh_rate_ms (optional override)
   - calibration: scale (default 1.0), offset (default 0.0)

3. **Smart Features**
   - Auto-calculate quantity based on data type
   - Data type search/filter
   - Calibration formula preview: `result = (raw_value * scale) + offset`

4. **Validation**
   - Address range check
   - Quantity validation per data type
   - Calibration value validation

**UI Enhancements:**

- Data type selector with icons
- Quantity auto-calculation indicator
- Calibration calculator
- JSON preview panel

---

### File 5: Form Server_Configuration.html

**Current Features:**

- ✅ Communication mode (WiFi/Ethernet)
- ✅ MQTT configuration
- ✅ HTTP configuration
- ✅ WiFi settings
- ✅ Ethernet settings

**Features to Add:**

1. **MQTT Publish Modes (v2.2.0)**
   - **Default Mode:** Single topic with 5s interval
     - topic_publish, topic_subscribe, interval, interval_unit (s/m/h)
   - **Customize Mode:** Per-device custom topics
     - Array of custom_topics with device_id mapping

2. **HTTP Interval Configuration**
   - Move interval into http_config structure
   - interval, interval_unit (s/m/h)

3. **Network Failover Settings**
   - Priority settings (Ethernet=1, WiFi=2)
   - Failover enabled toggle
   - Check interval (ms)
   - Hysteresis delay (ms) to prevent oscillation

4. **Enhanced MQTT Settings**
   - keep_alive, clean_session, use_tls
   - QoS levels (0, 1, 2)

5. **Enhanced HTTP Settings**
   - Custom headers (key-value pairs)
   - Method (POST, PUT, PATCH)
   - Timeout, retry count

**UI Enhancements:**

- Tabbed interface (Network, MQTT, HTTP)
- Mode toggle with smooth animations
- Real-time JSON preview
- Validation indicators

---

### File 6: Logging Config dan Delete Dialog.html

**Current Features:**

- ✅ Basic logging configuration
- ✅ Delete confirmation dialog

**Features to Add:**

1. **Logging Configuration**
   - logging_ret (retention): dropdown (1h, 6h, 12h, 1d, 1w, 1m)
   - logging_interval: dropdown (1m, 5m, 10m, 30m, 1h)

2. **Runtime Log Control**
   - Log level selector (NONE, ERROR, WARN, INFO, DEBUG, VERBOSE)
   - Module-specific logging toggles
   - RTC timestamp toggle

3. **Enhanced Delete Dialog**
   - Warning level indicators
   - Impact description
   - Confirmation checkbox ("I understand this action cannot be undone")

**UI Enhancements:**

- Visual log level indicator
- Preview of log output
- Animated modal transitions

---

### File 7: Streaming.html

**Current Features:**

- ✅ Live data streaming view

**Features to Add:**

1. **Enhanced Streaming**
   - Multiple device streaming support
   - Register-level filtering
   - Chart visualization (line charts)
   - Data export (CSV download)

2. **Metrics Display**
   - Packets received counter
   - Data rate (bytes/sec)
   - Latency indicator

3. **Controls**
   - Pause/resume streaming
   - Clear data buffer
   - Auto-scroll toggle

**UI Enhancements:**

- Real-time animated charts
- Smooth data transitions
- Performance optimized rendering

---

### File 8: Settings.html (NEW)

**Purpose:** Central settings page for backup, restore, factory reset, and BLE
metrics

**Features:**

1. **Backup Section**
   - "Create Backup" button
   - Backup list (from localStorage)
   - Each backup shows: timestamp, firmware version, device count, file size
   - Download backup button
   - Delete backup button

2. **Restore Section**
   - File upload input
   - Backup file validation
   - Preview backup contents
   - Restore confirmation dialog
   - Progress indicator during restore

3. **Factory Reset Section**
   - Warning banner
   - "Create Backup First" suggestion
   - Reason input field
   - Factory reset button (requires confirmation)
   - Countdown timer before restart

4. **BLE Metrics Section**
   - MTU metrics display
   - Connection metrics display
   - Queue metrics display
   - Refresh button

**UI Design:**

- Card-based layout
- Warning colors for destructive actions
- Confirmation modals with clear messaging
- Success/error toast notifications

---

## 🎨 Design System

### Color Palette

```css
Primary (Brand): #2f6f64
Success: #10b981 (emerald-500)
Warning: #f59e0b (amber-500)
Error: #ef4444 (red-500)
Info: #3b82f6 (blue-500)
Gray: #64748b (slate-500)
```

### Status Colors

```css
Enabled + Healthy: #10b981 (green)
Enabled + Issues: #f59e0b (yellow)
Enabled + Poor: #ef4444 (red)
Disabled: #9ca3af (gray)
```

### Typography

- **Headings:** font-semibold to font-bold
- **Body:** text-sm to text-base
- **Captions:** text-xs
- **Mono:** font-mono for IDs, values

### Spacing

- **Mobile:** p-3, gap-3, space-y-3
- **Desktop:** p-6, gap-6, space-y-6

### Animations

```css
Fade In: animate-fade-in (0.3s ease-in-out)
Slide Down: animate-slide-down (0.3s ease-out)
Scale In: animate-scale-in (0.2s ease-out)
Pulse: animate-pulse (2s infinite)
```

---

## 🔧 Technical Implementation

### Tailwind CSS

- Use CDN for quick setup: `https://cdn.tailwindcss.com`
- Custom config for brand colors and animations
- Responsive breakpoints: sm (640px), md (768px), lg (1024px)

### JavaScript Pattern

```javascript
// Singleton pattern for managers
class BackupManager {
  static instance = null;
  static getInstance() {
    if (!this.instance) this.instance = new BackupManager();
    return this.instance;
  }
}

// Event-driven architecture
document.addEventListener("deviceUpdated", (e) => {
  console.log("Device updated:", e.detail);
  refreshDeviceList();
});

// LocalStorage for mock data persistence
localStorage.setItem("devices", JSON.stringify(devices));
localStorage.setItem("backups", JSON.stringify(backups));
```

### Mock BLE Communication

```javascript
class MockBLEManager {
  async sendCommand(command) {
    // Simulate BLE delay
    await new Promise((resolve) => setTimeout(resolve, 500));

    // Mock response based on command
    if (command.op === "read" && command.type === "full_config") {
      return this.mockFullConfigResponse();
    }
    // ... other commands
  }

  mockFullConfigResponse() {
    const devices = JSON.parse(localStorage.getItem("devices") || "[]");
    const serverConfig = JSON.parse(
      localStorage.getItem("serverConfig") || "{}",
    );

    return {
      status: "ok",
      backup_info: {
        timestamp: Date.now(),
        firmware_version: "2.3.0",
        total_devices: devices.length,
        total_registers: devices.reduce(
          (sum, d) => sum + (d.registers?.length || 0),
          0,
        ),
        processing_time_ms: 350,
        backup_size_bytes: JSON.stringify({
          devices,
          server_config: serverConfig,
        }).length,
      },
      config: {
        devices,
        server_config: serverConfig,
        logging_config: { logging_ret: "1w", logging_interval: "5m" },
      },
    };
  }
}
```

### Data Persistence

- Use `localStorage` for mock data
- Structure:
  ```javascript
  localStorage.devices = JSON.stringify([...]);
  localStorage.serverConfig = JSON.stringify({...});
  localStorage.loggingConfig = JSON.stringify({...});
  localStorage.backups = JSON.stringify([...]);
  localStorage.deviceMetrics = JSON.stringify({...});
  ```

---

## 📦 File Update Priority

1. **HIGH PRIORITY:**
   - Settings.html (NEW) - Backup/Restore/Factory Reset
   - Device List (Home Screen).html - Health metrics, status indicators
   - Device Details.html - Device control, health dashboard

2. **MEDIUM PRIORITY:**
   - Form Server_Configuration.html - MQTT modes, network failover
   - Create New Device.html - Complete form fields
   - Add Register.html - All data types, calibration

3. **LOW PRIORITY:**
   - Logging Config.html - Enhanced UI
   - Streaming.html - Charts and metrics

---

## ✅ Success Criteria

Each updated file must have:

- ✅ Responsive design (mobile-first)
- ✅ Tailwind CSS styling
- ✅ Interactive animations
- ✅ Live preview capability (mock BLE)
- ✅ All v2.3.0 firmware features represented
- ✅ Form validation where applicable
- ✅ Error handling
- ✅ Toast notifications for user feedback
- ✅ Accessibility (ARIA labels, keyboard navigation)

---

## 🚀 Next Steps

1. ✅ Create Settings.html with backup/restore/factory reset
2. Update Device List (Home Screen).html
3. Update Device Details.html
4. Update Create New Device.html
5. Update Add Register.html
6. Update Form Server_Configuration.html
7. Update Logging Config.html
8. Update Streaming.html
9. Create comprehensive README for mockup usage
10. Test all files for responsiveness and interactivity

---

**Created by:** Claude Code **Date:** November 21, 2025 **Version:** 1.0.0
