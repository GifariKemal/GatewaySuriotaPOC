# JSON Preview Component

## Overview

Komponen JSON Preview untuk menampilkan BLE command dan response secara
real-time di semua Mockup UI files.

## Features

- **Dual Panel**: Command (Request) dan Response side-by-side
- **Minimize/Maximize**: Toggle visibility
- **Copy to Clipboard**: Copy JSON dengan satu klik
- **Auto-scroll**: Scroll ke command/response terbaru
- **Syntax Highlighting**: Dark background dengan text hijau
- **Timestamp**: Setiap command/response memiliki timestamp
- **Fixed Position**: Sticky di bagian bawah halaman

## UI Structure

```html
<!-- JSON Preview Panel (Fixed at bottom) -->
<div
  id="jsonPreviewPanel"
  class="fixed bottom-0 left-0 right-0 bg-white border-t-2 border-gray-300 shadow-2xl z-50 transition-all duration-300"
>
  <!-- Header dengan Minimize/Maximize button -->
  <div
    class="bg-gradient-to-r from-gray-800 to-gray-900 text-white px-4 py-2 flex justify-between items-center cursor-pointer"
    onclick="toggleJSONPreview()"
  >
    <h3>JSON Preview - BLE Commands & Responses</h3>
    <svg id="toggleIcon">...</svg>
  </div>

  <!-- Content (collapsible) -->
  <div
    id="jsonPreviewContent"
    class="grid grid-cols-1 md:grid-cols-2 gap-4 p-4 max-h-80 overflow-hidden"
  >
    <!-- Command Panel -->
    <div>
      <h4>Last Command (Request)</h4>
      <pre id="jsonCommand">...</pre>
      <button onclick="copyJSON('command')">Copy</button>
    </div>

    <!-- Response Panel -->
    <div>
      <h4>Last Response</h4>
      <pre id="jsonResponse">...</pre>
      <button onclick="copyJSON('response')">Copy</button>
    </div>
  </div>
</div>
```

## JavaScript Functions

```javascript
// Toggle JSON Preview Panel
function toggleJSONPreview() {
  const content = document.getElementById("jsonPreviewContent");
  const icon = document.getElementById("toggleIcon");

  if (content.classList.contains("hidden")) {
    content.classList.remove("hidden");
    icon.innerHTML = "▼"; // Down arrow
  } else {
    content.classList.add("hidden");
    icon.innerHTML = "▲"; // Up arrow
  }
}

// Update Command Preview
function updateJSONCommand(command) {
  const el = document.getElementById("jsonCommand");
  const timestamp = new Date().toLocaleTimeString();
  el.textContent = JSON.stringify(command, null, 2);
  // Add timestamp
}

// Update Response Preview
function updateJSONResponse(response) {
  const el = document.getElementById("jsonResponse");
  const timestamp = new Date().toLocaleTimeString();
  el.textContent = JSON.stringify(response, null, 2);
  // Add timestamp
}

// Copy to Clipboard
function copyJSON(type) {
  const el = document.getElementById(
    type === "command" ? "jsonCommand" : "jsonResponse",
  );
  navigator.clipboard.writeText(el.textContent);
  showToast("JSON copied to clipboard!", "success");
}
```

## Integration Steps

1. Add HTML panel before `</body>`
2. Add CSS styles for syntax highlighting
3. Update `MockBLEManager.sendCommand()` to call `updateJSONCommand(command)`
4. Update response handling to call `updateJSONResponse(response)`

## Files to Update

- [x] Settings.html
- [ ] Device List (Home Screen).html
- [ ] Device Details.html
- [ ] Create New Device.html
- [ ] Add Register.html
- [ ] Form Server_Configuration.html
- [ ] Logging Config dan Delete Dialog.html
- [ ] Streaming.html
