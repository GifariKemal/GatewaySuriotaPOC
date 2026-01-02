# 📋 Modbus Register Editor - Documentation

> **Interactive web form for configuring Modbus registers on SRT-MGATE-1210
> Gateway**

[![Version](https://img.shields.io/badge/version-2.0.0-blue.svg)](https://github.com)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Tailwind CSS](https://img.shields.io/badge/Tailwind-3.x-38B2AC.svg)](https://tailwindcss.com)

---

## 📖 Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Screenshots](#screenshots)
- [Technical Specifications](#technical-specifications)
- [Field Descriptions](#field-descriptions)
- [JSON Payload Structure](#json-payload-structure)
- [Responsive Design](#responsive-design)
- [Animations & Interactions](#animations--interactions)
- [Installation & Usage](#installation--usage)
- [API Integration](#api-integration)
- [Browser Support](#browser-support)
- [Customization](#customization)
- [Troubleshooting](#troubleshooting)
- [Changelog](#changelog)

---

## 🎯 Overview

The **Modbus Register Editor** is a modern, interactive web-based form designed
for configuring Modbus registers on the SRT-MGATE-1210 Gateway. It provides a
user-friendly interface with real-time JSON preview, syntax highlighting, and
comprehensive validation.

### Key Highlights

- ✅ **28 Data Types** - Full support for INT, UINT, FLOAT, DOUBLE
  (16/32/64-bit) with endianness variants
- ✅ **Live JSON Preview** - Real-time syntax-highlighted JSON output
- ✅ **Calibration Support** - Built-in scale and offset configuration
- ✅ **Fully Responsive** - Optimized for mobile, tablet, and desktop
- ✅ **Modern UI/UX** - Beautiful animations, transitions, and interactive
  feedback
- ✅ **Production Ready** - Clean code, well-documented, and tested

---

## ✨ Features

### 🎨 User Interface

| Feature                  | Description                                           |
| ------------------------ | ----------------------------------------------------- |
| **Modern Design**        | Gradient backgrounds, rounded corners, soft shadows   |
| **Sticky Header**        | Always-visible navigation with backdrop blur effect   |
| **2-Column Layout**      | Form on left, live preview on right (desktop)         |
| **Icon Labels**          | SVG icons for every field for better visual hierarchy |
| **Color-Coded Sections** | Calibration section with amber/orange gradient        |
| **Dark Mode JSON**       | Syntax-highlighted JSON with dark theme               |

### 🔧 Functionality

- **Real-time Validation** - Instant feedback on address range (0-65535)
- **Live JSON Preview** - Updates as you type
- **Syntax Highlighting** - Color-coded JSON keys, strings, numbers, booleans
- **Copy to Clipboard** - One-click JSON copy with visual feedback
- **Save Confirmation** - Loading → Saved → Reset animation
- **Error States** - Visual border changes and error messages
- **Auto-complete** - Smart defaults for scale (1.0) and offset (0.0)

### 📱 Responsive Breakpoints

| Device  | Width          | Layout                              |
| ------- | -------------- | ----------------------------------- |
| Mobile  | < 640px        | Single column, full-width buttons   |
| Tablet  | 640px - 1024px | Single column, side-by-side buttons |
| Desktop | > 1024px       | 2-column grid, sticky preview       |

### 🎭 Animations

- **Page Load** - Fade-in and slide-up animations
- **Input Focus** - Transform, ring expansion, shimmer effect
- **Button Hover** - Scale-up, shadow glow
- **Button Click** - Scale-down (active state)
- **Error Display** - Slide-up animation
- **Success Feedback** - Popup notification with auto-hide

---

## 📸 Screenshots

### Desktop View (2-Column Layout)

```
┌─────────────────────────────────────────────────────────────┐
│  [🔲 Icon] Modbus Register Editor         [● Live Preview] │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌─────────────────────┐  ┌─────────────────────┐          │
│  │                     │  │                     │          │
│  │   Register Form     │  │   JSON Preview      │          │
│  │                     │  │   (Syntax Highlight)│          │
│  │   • Device ID       │  │                     │          │
│  │   • Register Name   │  │   {                 │          │
│  │   • Address         │  │     "op": "create"  │          │
│  │   • Function Code   │  │     "type": "..."   │          │
│  │   • Data Type       │  │     "config": {     │          │
│  │   • Description     │  │       ...           │          │
│  │   [Calibration]     │  │     }               │          │
│  │   • Scale/Offset    │  │   }                 │          │
│  │   • Unit            │  │                     │          │
│  │                     │  │   [API Info Box]    │          │
│  │  [Save] [Copy]      │  │                     │          │
│  └─────────────────────┘  └─────────────────────┘          │
│                                                               │
└─────────────────────────────────────────────────────────────┘
```

### Mobile View (Single Column)

```
┌────────────────────────┐
│ 🔲 Modbus Register     │
│    Editor              │
├────────────────────────┤
│                        │
│  Register Form         │
│  ┌──────────────────┐  │
│  │ Device ID        │  │
│  │ Register Name    │  │
│  │ Address          │  │
│  │ Function Code    │  │
│  │ Data Type        │  │
│  │ Description      │  │
│  │ [Calibration]    │  │
│  │ Unit             │  │
│  └──────────────────┘  │
│                        │
│  [Save Register]       │
│  [Copy JSON]           │
│                        │
│  ┌──────────────────┐  │
│  │ JSON Preview     │  │
│  │ (scrollable)     │  │
│  └──────────────────┘  │
│                        │
└────────────────────────┘
```

---

## 🛠️ Technical Specifications

### Technology Stack

| Component      | Technology        | Version   |
| -------------- | ----------------- | --------- |
| **Frontend**   | HTML5             | -         |
| **Styling**    | Tailwind CSS      | 3.x (CDN) |
| **JavaScript** | Vanilla JS (ES6+) | -         |
| **Icons**      | Heroicons (SVG)   | Inline    |
| **Fonts**      | System Fonts      | -         |

### File Information

```
File: Add Register.html
Size: ~20 KB
Lines: ~567
Dependencies: Tailwind CSS CDN only
```

### Browser Requirements

- Modern browser with ES6+ support
- JavaScript enabled
- CSS Grid and Flexbox support
- Clipboard API support (for copy function)

---

## 📋 Field Descriptions

### Required Fields (⚠️ Must be filled)

#### 1. **Device ID** (Read-only)

- **Type**: Text (6 characters, hexadecimal)
- **Format**: `D4A5F1`
- **Description**: Unique identifier for the Modbus device
- **Validation**: None (auto-generated, read-only)
- **Example**: `D4A5F1`, `A7B2C3`

#### 2. **Register Name** ⚠️

- **Type**: Text
- **Max Length**: 50 characters
- **Description**: Unique identifier for the register
- **Validation**: Required, non-empty
- **Format**: Alphanumeric, underscore, hyphen
- **Example**: `Voltage_L1`, `Temperature_Sensor`, `Power_Meter_01`

#### 3. **Modbus Address** ⚠️

- **Type**: Number
- **Range**: 0 - 65535
- **Description**: Modbus register address
- **Validation**: Required, must be 0-65535
- **Format**: Integer only
- **Example**: `40001`, `30001`, `0`, `65535`

#### 4. **Function Code** ⚠️

- **Type**: Dropdown
- **Options**:
  - `1` - Read Coil Status (FC 01)
  - `2` - Read Discrete Input (FC 02)
  - `3` - Read Holding Register (FC 03) ⭐ Default
  - `4` - Read Input Register (FC 04)
- **Description**: Modbus function code for reading data
- **Validation**: Required
- **Default**: `3` (Holding Register)

#### 5. **Data Type** ⚠️

- **Type**: Dropdown (Grouped)
- **Options**: 28 data types across 8 categories
- **Description**: Format for interpreting raw Modbus data
- **Validation**: Required
- **Default**: `FLOAT32_BE`

**Data Type Categories:**

| Category            | Types                                                    | Description                      |
| ------------------- | -------------------------------------------------------- | -------------------------------- |
| **16-bit Signed**   | INT16                                                    | Single register, -32768 to 32767 |
| **16-bit Unsigned** | UINT16                                                   | Single register, 0 to 65535      |
| **Boolean**         | BOOL                                                     | Single bit, true/false           |
| **Binary**          | BINARY                                                   | Raw 16-bit value                 |
| **32-bit Signed**   | INT32_BE, INT32_LE, INT32_BE_BS, INT32_LE_BS             | Two registers, signed integer    |
| **32-bit Unsigned** | UINT32_BE, UINT32_LE, UINT32_BE_BS, UINT32_LE_BS         | Two registers, unsigned integer  |
| **32-bit Float**    | FLOAT32_BE, FLOAT32_LE, FLOAT32_BE_BS, FLOAT32_LE_BS     | Two registers, IEEE 754 float    |
| **64-bit Signed**   | INT64_BE, INT64_LE, INT64_BE_BS, INT64_LE_BS             | Four registers, signed integer   |
| **64-bit Unsigned** | UINT64_BE, UINT64_LE, UINT64_BE_BS, UINT64_LE_BS         | Four registers, unsigned integer |
| **64-bit Double**   | DOUBLE64_BE, DOUBLE64_LE, DOUBLE64_BE_BS, DOUBLE64_LE_BS | Four registers, IEEE 754 double  |
| **Legacy**          | INT32, FLOAT32, STRING                                   | Legacy formats                   |

**Endianness Notation:**

- `BE` - Big Endian (ABCD)
- `LE` - Little Endian (DCBA)
- `_BS` - Byte Swap / Word Swap (BADC for BE, CDAB for LE)

### Optional Fields

#### 6. **Description**

- **Type**: Text
- **Max Length**: 200 characters
- **Description**: Human-readable description of the register
- **Validation**: None
- **Example**: `Phase 1 Voltage Measurement`, `Temperature from sensor 01`

#### 7. **Scale** (Calibration)

- **Type**: Number (decimal)
- **Range**: Any float
- **Description**: Multiplier for calibrating raw values
- **Validation**: None
- **Default**: `1.0`
- **Format**: Float with any precision
- **Example**: `0.1`, `0.001`, `1.8`, `100`

**Use Cases:**

- Resolution conversion (e.g., 0.1 for values in tenths)
- Unit conversion (e.g., 0.001 for mA → A)
- Celsius to Fahrenheit (scale: 1.8, offset: 32)

#### 8. **Offset** (Calibration)

- **Type**: Number (decimal)
- **Range**: Any float
- **Description**: Value added after scaling
- **Validation**: None
- **Default**: `0.0`
- **Format**: Float with any precision
- **Example**: `0`, `-5`, `32`, `-273.15`

**Formula:**

```
final_value = (raw_value × scale) + offset
```

**Use Cases:**

- Sensor bias correction
- Zero-point adjustment
- Temperature offset conversion

#### 9. **Unit**

- **Type**: Text
- **Max Length**: 20 characters
- **Description**: Measurement unit for display
- **Validation**: None
- **Example**: `°C`, `V`, `A`, `kW`, `%`, `Bar`, `PSI`, `RPM`

---

## 📦 JSON Payload Structure

### Request Format (Create Register)

```json
{
  "op": "create",
  "type": "register",
  "device_id": "D4A5F1",
  "config": {
    "address": 40001,
    "register_name": "Voltage_L1",
    "function_code": 3,
    "data_type": "FLOAT32_BE",
    "description": "Phase 1 Voltage Measurement",
    "scale": 0.1,
    "offset": 0.0,
    "unit": "V"
  }
}
```

### Field Mapping

| JSON Key               | Form Field        | Type    | Required          |
| ---------------------- | ----------------- | ------- | ----------------- |
| `op`                   | Fixed: "create"   | string  | Yes               |
| `type`                 | Fixed: "register" | string  | Yes               |
| `device_id`            | Device ID         | string  | Yes               |
| `config.address`       | Modbus Address    | integer | Yes               |
| `config.register_name` | Register Name     | string  | Yes               |
| `config.function_code` | Function Code     | integer | Yes               |
| `config.data_type`     | Data Type         | string  | Yes               |
| `config.description`   | Description       | string  | No                |
| `config.scale`         | Scale             | float   | No (default: 1.0) |
| `config.offset`        | Offset            | float   | No (default: 0.0) |
| `config.unit`          | Unit              | string  | No                |

### Example Payloads

#### Example 1: Temperature Sensor

```json
{
  "op": "create",
  "type": "register",
  "device_id": "A1B2C3",
  "config": {
    "address": 0,
    "register_name": "Temperature_Ambient",
    "function_code": 4,
    "data_type": "INT16",
    "description": "Ambient temperature sensor",
    "scale": 0.1,
    "offset": -40.0,
    "unit": "°C"
  }
}
```

#### Example 2: Power Meter

```json
{
  "op": "create",
  "type": "register",
  "device_id": "D4A5F1",
  "config": {
    "address": 30001,
    "register_name": "Active_Power_Total",
    "function_code": 3,
    "data_type": "FLOAT32_BE",
    "description": "Total active power consumption",
    "scale": 0.001,
    "offset": 0.0,
    "unit": "kW"
  }
}
```

#### Example 3: Pressure Sensor (No Calibration)

```json
{
  "op": "create",
  "type": "register",
  "device_id": "F7E8D9",
  "config": {
    "address": 100,
    "register_name": "Pressure_Line_1",
    "function_code": 3,
    "data_type": "UINT16",
    "description": "Hydraulic line pressure",
    "scale": 1.0,
    "offset": 0.0,
    "unit": "Bar"
  }
}
```

---

## 📱 Responsive Design

### Breakpoint Strategy

The form uses a **mobile-first approach** with three main breakpoints:

```css
/* Mobile: Default (< 640px) */
- Single column layout
- Full-width inputs
- Stacked buttons
- Compact padding (p-4)
- Hidden header badges

/* Tablet: sm breakpoint (≥ 640px) */
- Single column with more spacing
- Side-by-side buttons
- Show header badges
- Medium padding (p-6)

/* Desktop: lg breakpoint (≥ 1024px) */
- 2-column grid (form | preview)
- Sticky preview panel
- Large padding (p-8)
- Max-width container (6xl)
```

### Responsive Grid Layout

```html
<!-- Desktop: 2 columns | Mobile: 1 column -->
<div class="grid grid-cols-1 lg:grid-cols-2 gap-6">
  <div>Form Section</div>
  <div class="lg:sticky lg:top-24">Preview Section (sticky on desktop)</div>
</div>
```

### Touch-Friendly Elements

| Element       | Mobile Size    | Desktop Size   |
| ------------- | -------------- | -------------- |
| Input Height  | py-3 (48px+)   | py-3 (48px+)   |
| Button Height | py-3 (48px+)   | py-3 (48px+)   |
| Touch Target  | ≥ 44px         | ≥ 44px         |
| Font Size     | text-sm (14px) | text-sm (14px) |

### Mobile Optimizations

- ✅ No horizontal scroll
- ✅ Viewport meta tag configured
- ✅ Touch-friendly spacing
- ✅ Readable font sizes
- ✅ Optimized tap targets
- ✅ Fast load time (~20KB)

---

## 🎭 Animations & Interactions

### Page Load Animations

```css
/* Fade In */
@keyframes fadeIn {
  0% {
    opacity: 0;
  }
  100% {
    opacity: 1;
  }
}

/* Slide Up */
@keyframes slideUp {
  0% {
    transform: translateY(10px);
    opacity: 0;
  }
  100% {
    transform: translateY(0);
    opacity: 1;
  }
}
```

**Applied to:**

- Main container: `animate-fade-in`
- Form cards: `animate-slide-up`
- Error messages: `animate-slide-up`

### Input Focus Effects

```css
/* Transform on focus */
input:focus,
select:focus {
  transform: translateY(-1px);
}

/* Shimmer animation */
.input-shimmer:focus {
  animation: shimmer 3s infinite;
  background: linear-gradient(
    90deg,
    transparent 0%,
    rgba(47, 111, 100, 0.1) 50%,
    transparent 100%
  );
}
```

### Button Interactions

| State   | Effect                 | CSS                                 |
| ------- | ---------------------- | ----------------------------------- |
| Hover   | Scale up + shadow glow | `hover:scale-105 hover:shadow-glow` |
| Active  | Scale down             | `active:scale-95`                   |
| Loading | Spinning icon          | `animate-spin`                      |
| Success | Color change           | `bg-emerald-600`                    |

### State Transitions

**Save Button:**

```
[Normal] → [Click] → [Loading] → [Success] → [Normal]
  ↓           ↓          ↓            ↓          ↓
"Save"    spinning    "Saving..."  "Saved!"   "Save"
```

**Copy Button:**

```
[Normal] → [Click] → [Success] → [Normal]
  ↓           ↓          ↓          ↓
Dark BG   Checkmark  Green BG   Dark BG
"Copy"    "Copied!"  (2s)       "Copy"
```

### Validation Feedback

```javascript
// Visual feedback
input.classList.toggle("border-red-500", hasError); // Red border
input.classList.toggle("border-slate-200", !hasError); // Normal border

// Error message
errorMsg.classList.toggle("hidden", !hasError); // Show/hide message
```

---

## 🚀 Installation & Usage

### Quick Start

1. **Download the file**

   ```bash
   # No installation needed, just open the HTML file
   ```

2. **Open in browser**

   ```bash
   # Double-click the file or
   open Add\ Register.html
   # Or
   start Add\ Register.html  # Windows
   ```

3. **That's it!** No build process, no dependencies.

### Requirements

- ✅ Modern web browser (Chrome, Firefox, Safari, Edge)
- ✅ Internet connection (for Tailwind CSS CDN)
- ✅ JavaScript enabled

### Local Development

```bash
# Serve locally with live reload (optional)
npx live-server

# Or use Python
python -m http.server 8000

# Or use Node.js http-server
npx http-server
```

### Integration with Backend

```javascript
// Example: Send data via Fetch API
document.getElementById("btnSave").addEventListener("click", async () => {
  const payload = buildPayload();

  try {
    const response = await fetch("/api/registers", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    });

    if (response.ok) {
      alert("Register saved successfully!");
    }
  } catch (error) {
    console.error("Error:", error);
  }
});
```

### BLE Integration (for SRT-MGATE-1210)

```javascript
// Example: Send via BLE GATT Characteristic
async function sendViaBLE(payload) {
  const device = await navigator.bluetooth.requestDevice({
    filters: [{ services: ["your-service-uuid"] }],
  });

  const server = await device.gatt.connect();
  const service = await server.getPrimaryService("your-service-uuid");
  const characteristic = await service.getCharacteristic("your-char-uuid");

  const encoder = new TextEncoder();
  const data = encoder.encode(JSON.stringify(payload));

  await characteristic.writeValue(data);
}
```

---

## 🔌 API Integration

### Backend Requirements

The backend must handle the following:

#### Endpoint: `POST /api/registers`

**Request Headers:**

```
Content-Type: application/json
```

**Request Body:** (See [JSON Payload Structure](#json-payload-structure))

**Response Format:**

```json
{
  "status": "success",
  "message": "Register created successfully",
  "register_id": "R3C8D1",
  "register_index": 1
}
```

**Error Response:**

```json
{
  "status": "error",
  "message": "Duplicate address detected at 40001"
}
```

### Validation Rules (Backend)

| Field           | Validation                                   |
| --------------- | -------------------------------------------- |
| `device_id`     | Must exist in database                       |
| `address`       | 0-65535, no duplicates per device            |
| `register_name` | Unique per device, alphanumeric + underscore |
| `function_code` | 1, 2, 3, or 4 only                           |
| `data_type`     | Must be in supported types list              |
| `scale`         | Must be finite number                        |
| `offset`        | Must be finite number                        |

### Integration with Firmware

The JSON payload is compatible with:

- **SRT-MGATE-1210 Firmware** (`ConfigManager.cpp`)
- **Flutter Mobile App** (`form_modbus_config_screen.dart`)

Both systems expect the same JSON structure.

---

## 🌐 Browser Support

| Browser | Version | Support          |
| ------- | ------- | ---------------- |
| Chrome  | ≥ 90    | ✅ Full          |
| Firefox | ≥ 88    | ✅ Full          |
| Safari  | ≥ 14    | ✅ Full          |
| Edge    | ≥ 90    | ✅ Full          |
| Opera   | ≥ 76    | ✅ Full          |
| IE 11   | -       | ❌ Not supported |

### Required Features

- ✅ ES6+ JavaScript (Arrow functions, template literals, etc.)
- ✅ CSS Grid & Flexbox
- ✅ CSS Variables
- ✅ Clipboard API (`navigator.clipboard`)
- ✅ Fetch API (if using AJAX)

### Polyfills (Optional)

For older browsers, consider:

```html
<!-- Clipboard API polyfill -->
<script src="https://unpkg.com/clipboard-polyfill"></script>
```

---

## 🎨 Customization

### Color Scheme

Edit the Tailwind config in `<head>`:

```javascript
tailwind.config = {
  theme: {
    extend: {
      colors: {
        brand: {
          DEFAULT: "#2f6f64", // Primary color
          dark: "#1f4f44", // Darker variant
          light: "#3f8f74", // Lighter variant
          ink: "#e7f3f0", // Background tint
        },
      },
    },
  },
};
```

### Custom Validation

Add your own validation rules:

```javascript
function validRegisterName() {
  const name = $("#reg_name").value;
  const ok = /^[a-zA-Z0-9_-]+$/.test(name);

  if (!ok) {
    alert("Only alphanumeric, underscore, and hyphen allowed");
  }

  return ok;
}
```

### Custom Data Types

Add more data types to the dropdown:

```html
<optgroup label="Custom Types">
  <option value="CUSTOM_TYPE_1">Custom Type 1</option>
  <option value="CUSTOM_TYPE_2">Custom Type 2</option>
</optgroup>
```

### Styling Modifications

All styles use Tailwind utility classes. To modify:

```html
<!-- Change button color -->
<button class="bg-brand">
  <!-- Original -->
  <button class="bg-blue-600">
    <!-- Modified -->

    <!-- Change border radius -->
    <input class="rounded-xl" />
    <!-- Original -->
    <input class="rounded-md" />
    <!-- Modified -->
  </button>
</button>
```

---

## 🐛 Troubleshooting

### Common Issues

#### 1. **JSON Preview not updating**

**Cause:** Validation errors preventing render

**Solution:**

- Check address is between 0-65535
- Fill all required fields
- Check browser console for errors

#### 2. **Copy button not working**

**Cause:** Clipboard API not supported or permissions denied

**Solution:**

```javascript
// Fallback copy method
function fallbackCopy(text) {
  const textarea = document.createElement("textarea");
  textarea.value = text;
  document.body.appendChild(textarea);
  textarea.select();
  document.execCommand("copy");
  document.body.removeChild(textarea);
}
```

#### 3. **Styles not loading**

**Cause:** Tailwind CDN blocked or slow connection

**Solution:**

- Check internet connection
- Try local Tailwind CSS build
- Check browser console for CDN errors

#### 4. **Animations not smooth**

**Cause:** Browser performance or hardware acceleration disabled

**Solution:**

```css
/* Force hardware acceleration */
* {
  transform: translateZ(0);
  backface-visibility: hidden;
}
```

#### 5. **Mobile layout broken**

**Cause:** Viewport meta tag missing or incorrect

**Solution:**

```html
<!-- Ensure this is in <head> -->
<meta name="viewport" content="width=device-width, initial-scale=1" />
```

### Debug Mode

Enable console logging:

```javascript
// Add at top of <script>
const DEBUG = true;

function render() {
  if (DEBUG) console.log("Rendering payload:", buildPayload());
  // ... rest of function
}
```

---

## 📝 Changelog

### Version 2.0.0 (Current)

**Release Date:** 2024-01-XX

**Major Changes:**

- ✅ **Removed** `refresh_rate_ms` field (breaking change)
- ✅ **Added** 28 data types (up from 7)
- ✅ **Added** JSON syntax highlighting
- ✅ **Added** Calibration section (scale, offset)
- ✅ **Added** Unit field
- ✅ **Redesigned** UI with modern animations
- ✅ **Improved** responsive design
- ✅ **Added** interactive button states
- ✅ **Added** copy success indicator

**Breaking Changes:**

- `refresh_rate_ms` removed from JSON payload
- Minimum browser requirements updated

### Version 1.0.0

**Release Date:** 2024-01-XX

**Initial Release:**

- Basic form fields
- Simple JSON preview
- Basic validation
- Single column layout

---

## 📄 License

```
MIT License

Copyright (c) 2024 SRT-MGATE-1210 Project

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## 🤝 Contributing

Contributions are welcome! Please follow these steps:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

### Code Style

- Use Tailwind utility classes
- Keep JavaScript vanilla (no frameworks)
- Follow ES6+ conventions
- Comment complex logic
- Test on multiple browsers

---

## 📞 Support

For issues, questions, or suggestions:

- **GitHub Issues:** [Create an issue](https://github.com/your-repo/issues)
- **Email:** support@example.com
- **Documentation:** [Full docs](https://docs.example.com)

---

## 🙏 Acknowledgments

- **Tailwind CSS** - For the amazing utility-first CSS framework
- **Heroicons** - For beautiful SVG icons
- **SRT-MGATE-1210 Team** - For the gateway firmware and specifications

---

## 📚 Additional Resources

- [Modbus Protocol Specification](https://modbus.org/docs/Modbus_Application_Protocol_V1_1b3.pdf)
- [Tailwind CSS Documentation](https://tailwindcss.com/docs)
- [Web Bluetooth API](https://developer.mozilla.org/en-US/docs/Web/API/Web_Bluetooth_API)
- [SRT-MGATE-1210 Firmware Documentation](../README.md)

---

<div align="center">

**Made with ❤️ for industrial automation**

[⬆ Back to Top](#-modbus-register-editor---documentation)

</div>
