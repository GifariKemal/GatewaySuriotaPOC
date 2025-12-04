# BLE Gateway Identity API

**SRT-MGATE-1210 Modbus IIoT Gateway**
Multi-Gateway Support & Device Identification

[Home](../../README.md) > [Documentation](../README.md) > [API Reference](API.md) > Gateway Identity

**Version:** 2.5.31
**Release Date:** December 04, 2025
**Developer:** Kemal

---

## Table of Contents

- [Overview](#overview)
- [Multi-Gateway Architecture](#multi-gateway-architecture)
- [BLE Name Format](#ble-name-format)
- [API Commands](#api-commands)
  - [get_gateway_info](#1-get_gateway_info)
  - [set_friendly_name](#2-set_friendly_name)
  - [set_gateway_location](#3-set_gateway_location)
- [Mobile App Integration Guide](#mobile-app-integration-guide)
  - [Recommended Flow](#recommended-flow)
  - [Gateway Registry Database](#gateway-registry-database)
  - [UI/UX Recommendations](#uiux-recommendations)
- [Android Implementation Example](#android-implementation-example)
- [iOS Implementation Example](#ios-implementation-example)
- [Error Handling](#error-handling)
- [FAQ](#faq)

---

## Overview

Starting from firmware **v2.5.31**, the SRT-MGATE-1210 gateway supports **Multi-Gateway deployment**. Each gateway automatically generates a **unique BLE name** from its MAC address, allowing mobile apps to distinguish between multiple devices during Bluetooth scanning.

### Key Features

| Feature                | Description                                        |
| ---------------------- | -------------------------------------------------- |
| **Unique BLE Name**    | Auto-generated from MAC address: `SURIOTA-XXXXXX`  |
| **Friendly Name**      | User-configurable custom name (max 32 chars)       |
| **Location**           | Optional location info (max 64 chars)              |
| **Persistent Storage** | Config saved to `/gateway_config.json` on LittleFS |
| **Zero Configuration** | Works out-of-box, no manual setup required         |

---

## Multi-Gateway Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                          MOBILE APP                                  │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│   ┌─────────────────┐                                               │
│   │   BLE SCANNER   │  ← Scans for "SURIOTA-*" devices              │
│   └────────┬────────┘                                               │
│            │                                                         │
│            ▼                                                         │
│   ┌─────────────────────────────────────────────────────────────┐   │
│   │                    SCAN RESULTS                              │   │
│   │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐          │   │
│   │  │ SURIOTA-    │  │ SURIOTA-    │  │ SURIOTA-    │          │   │
│   │  │ A3B2C1      │  │ D4E5F6      │  │ 7890AB      │          │   │
│   │  │ RSSI: -45   │  │ RSSI: -62   │  │ RSSI: -78   │          │   │
│   │  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘          │   │
│   └─────────┼────────────────┼────────────────┼─────────────────┘   │
│             │                │                │                      │
│             ▼                ▼                ▼                      │
│   ┌─────────────────────────────────────────────────────────────┐   │
│   │              GATEWAY REGISTRY (Local SQLite)                 │   │
│   │  ┌────────────────────────────────────────────────────────┐ │   │
│   │  │ MAC: A3B2C1 → "Panel Listrik Gedung A" (Lt.1 R.Panel)  │ │   │
│   │  │ MAC: D4E5F6 → "Chiller Gedung B" (Basement R.Mesin)    │ │   │
│   │  │ MAC: 7890AB → "(New Device)" ← Not registered yet      │ │   │
│   │  └────────────────────────────────────────────────────────┘ │   │
│   └─────────────────────────────────────────────────────────────┘   │
│                                                                      │
│   ┌─────────────────────────────────────────────────────────────┐   │
│   │                    USER-FRIENDLY LIST                        │   │
│   │  ┌─────────────────────────────────────────────────────┐    │   │
│   │  │ 🟢 Panel Listrik Gedung A                           │    │   │
│   │  │    Lt.1 Ruang Panel | Signal: Excellent             │    │   │
│   │  ├─────────────────────────────────────────────────────┤    │   │
│   │  │ 🟢 Chiller Gedung B                                 │    │   │
│   │  │    Basement Ruang Mesin | Signal: Good              │    │   │
│   │  ├─────────────────────────────────────────────────────┤    │   │
│   │  │ 🔵 New Gateway (SURIOTA-7890AB)                     │    │   │
│   │  │    Tap to configure | Signal: Fair                  │    │   │
│   │  └─────────────────────────────────────────────────────┘    │   │
│   └─────────────────────────────────────────────────────────────┘   │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                         GATEWAYS (Hardware)                          │
│                                                                      │
│  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐   │
│  │  SRT-MGATE-1210  │  │  SRT-MGATE-1210  │  │  SRT-MGATE-1210  │   │
│  │  ════════════════│  │  ════════════════│  │  ════════════════│   │
│  │  BLE: SURIOTA-   │  │  BLE: SURIOTA-   │  │  BLE: SURIOTA-   │   │
│  │       A3B2C1     │  │       D4E5F6     │  │       7890AB     │   │
│  │  MAC: AA:BB:CC:  │  │  MAC: DD:EE:FF:  │  │  MAC: 11:22:33:  │   │
│  │       A3:B2:C1   │  │       D4:E5:F6   │  │       78:90:AB   │   │
│  │  ────────────────│  │  ────────────────│  │  ────────────────│   │
│  │  Modbus RTU x2   │  │  Modbus RTU x2   │  │  Modbus RTU x2   │   │
│  │  Modbus TCP      │  │  Modbus TCP      │  │  Modbus TCP      │   │
│  │  WiFi/Ethernet   │  │  WiFi/Ethernet   │  │  WiFi/Ethernet   │   │
│  └──────────────────┘  └──────────────────┘  └──────────────────┘   │
│         │                      │                      │              │
│         └──────────────────────┼──────────────────────┘              │
│                                ▼                                     │
│                    ┌─────────────────────┐                          │
│                    │   MQTT BROKER       │                          │
│                    │   (Cloud/Local)     │                          │
│                    └─────────────────────┘                          │
└─────────────────────────────────────────────────────────────────────┘
```

---

## BLE Name Format

Each gateway automatically generates a unique BLE advertising name:

```
SURIOTA-XXXXXX
        └─────── Last 3 bytes of Bluetooth MAC address (hex uppercase)
```

### Examples

| Full MAC Address    | BLE Name         |
| ------------------- | ---------------- |
| `AA:BB:CC:A3:B2:C1` | `SURIOTA-A3B2C1` |
| `11:22:33:D4:E5:F6` | `SURIOTA-D4E5F6` |
| `FF:EE:DD:78:90:AB` | `SURIOTA-7890AB` |

### Why Last 3 Bytes?

- **Uniqueness**: 16.7 million combinations (2^24)
- **Readability**: 6 characters is easy to read and remember
- **BLE Limit**: BLE device names have ~29 byte limit

---

## API Commands

All commands use the standard BLE CRUD format with `op: "control"`.

### 1. get_gateway_info

Retrieve complete gateway identification information.

#### Request

```json
{
  "op": "control",
  "type": "get_gateway_info"
}
```

#### Response

```json
{
  "status": "ok",
  "command": "get_gateway_info",
  "data": {
    "ble_name": "SURIOTA-A3B2C1",
    "mac": "AA:BB:CC:A3:B2:C1",
    "short_mac": "A3B2C1",
    "friendly_name": "Panel Listrik Gedung A",
    "location": "Lt.1 Ruang Panel",
    "firmware": "2.5.31",
    "model": "SRT-MGATE-1210",
    "free_heap": 150000,
    "free_psram": 7500000
  }
}
```

#### Response Fields

| Field           | Type   | Description                             |
| --------------- | ------ | --------------------------------------- |
| `ble_name`      | string | Auto-generated BLE advertising name     |
| `mac`           | string | Full Bluetooth MAC address              |
| `short_mac`     | string | Last 6 hex chars (for quick reference)  |
| `friendly_name` | string | User-set custom name (empty if not set) |
| `location`      | string | User-set location (empty if not set)    |
| `firmware`      | string | Current firmware version                |
| `model`         | string | Device model identifier                 |
| `free_heap`     | number | Free DRAM in bytes                      |
| `free_psram`    | number | Free PSRAM in bytes                     |

---

### 2. set_friendly_name

Set a user-friendly name for this gateway.

#### Request

```json
{
  "op": "control",
  "type": "set_friendly_name",
  "name": "Panel Listrik Gedung A"
}
```

#### Parameters

| Parameter | Type   | Required | Constraints | Description                 |
| --------- | ------ | -------- | ----------- | --------------------------- |
| `name`    | string | Yes      | 1-32 chars  | Custom name for the gateway |

#### Response (Success)

```json
{
  "status": "ok",
  "command": "set_friendly_name",
  "friendly_name": "Panel Listrik Gedung A",
  "ble_name": "SURIOTA-A3B2C1",
  "message": "Friendly name updated successfully"
}
```

#### Response (Error)

```json
{
  "status": "error",
  "message": "name too long (max 32 chars)"
}
```

---

### 3. set_gateway_location

Set location information for this gateway.

#### Request

```json
{
  "op": "control",
  "type": "set_gateway_location",
  "location": "Lt.1 Ruang Panel"
}
```

#### Parameters

| Parameter  | Type   | Required | Constraints | Description                           |
| ---------- | ------ | -------- | ----------- | ------------------------------------- |
| `location` | string | Yes      | 0-64 chars  | Location info (empty string to clear) |

#### Response (Success)

```json
{
  "status": "ok",
  "command": "set_gateway_location",
  "location": "Lt.1 Ruang Panel",
  "ble_name": "SURIOTA-A3B2C1",
  "message": "Location updated successfully"
}
```

---

## Mobile App Integration Guide

### Recommended Flow

```
┌─────────────────────────────────────────────────────────────────────┐
│                    MOBILE APP INTEGRATION FLOW                       │
└─────────────────────────────────────────────────────────────────────┘

┌─────────┐     ┌─────────┐     ┌─────────┐     ┌─────────┐     ┌─────────┐
│  START  │────▶│  SCAN   │────▶│ CONNECT │────▶│GET INFO │────▶│REGISTER │
└─────────┘     └─────────┘     └─────────┘     └─────────┘     └─────────┘
                    │                               │                │
                    ▼                               ▼                ▼
            ┌───────────────┐            ┌───────────────┐   ┌───────────────┐
            │ Filter by     │            │ Parse JSON    │   │ Save to       │
            │ "SURIOTA-*"   │            │ response      │   │ local DB      │
            └───────────────┘            └───────────────┘   └───────────────┘
```

#### Step-by-Step Flow

```
┌──────────────────────────────────────────────────────────────────────────┐
│ STEP 1: BLE SCAN                                                          │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  // Start BLE scan with name filter                                      │
│  scanFilter = "SURIOTA-"                                                 │
│                                                                          │
│  // Results:                                                             │
│  [                                                                       │
│    { name: "SURIOTA-A3B2C1", rssi: -45, address: "AA:BB:CC:A3:B2:C1" }, │
│    { name: "SURIOTA-D4E5F6", rssi: -62, address: "DD:EE:FF:D4:E5:F6" }, │
│    { name: "SURIOTA-7890AB", rssi: -78, address: "11:22:33:78:90:AB" }  │
│  ]                                                                       │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ STEP 2: CHECK LOCAL REGISTRY                                              │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  // For each scanned device, check if we have a friendly name            │
│  for (device in scanResults) {                                           │
│    shortMac = device.name.replace("SURIOTA-", "")  // "A3B2C1"          │
│    cachedInfo = database.getGateway(shortMac)                            │
│                                                                          │
│    if (cachedInfo != null) {                                             │
│      device.friendlyName = cachedInfo.friendly_name                      │
│      device.location = cachedInfo.location                               │
│      device.isRegistered = true                                          │
│    } else {                                                              │
│      device.friendlyName = "(New Device)"                                │
│      device.isRegistered = false                                         │
│    }                                                                     │
│  }                                                                       │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ STEP 3: USER SELECTS DEVICE                                               │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  // Connect to selected gateway                                          │
│  ble.connect(device.address)                                             │
│                                                                          │
│  // Discover services and characteristics                                │
│  service = ble.getService("4fafc201-1fb5-459e-8fcc-c5c9c331914b")       │
│  cmdChar = service.getCharacteristic(COMMAND_UUID)                       │
│  respChar = service.getCharacteristic(RESPONSE_UUID)                     │
│                                                                          │
│  // Enable notifications                                                 │
│  respChar.enableNotifications()                                          │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ STEP 4: GET GATEWAY INFO                                                  │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  // Send get_gateway_info command                                        │
│  request = { "op": "control", "type": "get_gateway_info" }              │
│  cmdChar.write(JSON.stringify(request))                                  │
│                                                                          │
│  // Wait for response (via notification)                                 │
│  response = await waitForNotification()                                  │
│                                                                          │
│  // Parse response                                                       │
│  gatewayInfo = JSON.parse(response).data                                 │
│  // {                                                                    │
│  //   ble_name: "SURIOTA-A3B2C1",                                       │
│  //   mac: "AA:BB:CC:A3:B2:C1",                                         │
│  //   short_mac: "A3B2C1",                                              │
│  //   friendly_name: "Panel Listrik Gedung A",                          │
│  //   location: "Lt.1 Ruang Panel",                                     │
│  //   firmware: "2.5.31",                                               │
│  //   model: "SRT-MGATE-1210"                                           │
│  // }                                                                    │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ STEP 5: REGISTER/UPDATE IN LOCAL DATABASE                                 │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  // Save or update gateway info in local database                        │
│  database.upsertGateway({                                                │
│    short_mac: gatewayInfo.short_mac,      // Primary key                │
│    ble_name: gatewayInfo.ble_name,                                       │
│    mac: gatewayInfo.mac,                                                 │
│    friendly_name: gatewayInfo.friendly_name,                             │
│    location: gatewayInfo.location,                                       │
│    firmware: gatewayInfo.firmware,                                       │
│    model: gatewayInfo.model,                                             │
│    last_connected: DateTime.now()                                        │
│  })                                                                      │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ STEP 6: FIRST-TIME SETUP (If friendly_name is empty)                      │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  if (gatewayInfo.friendly_name == "") {                                  │
│    // Show setup dialog to user                                          │
│    showSetupDialog()                                                     │
│                                                                          │
│    // User enters name and location                                      │
│    userInput = {                                                         │
│      name: "Panel Listrik Gedung A",                                     │
│      location: "Lt.1 Ruang Panel"                                        │
│    }                                                                     │
│                                                                          │
│    // Send set_friendly_name command                                     │
│    cmdChar.write(JSON.stringify({                                        │
│      "op": "control",                                                    │
│      "type": "set_friendly_name",                                        │
│      "name": userInput.name                                              │
│    }))                                                                   │
│                                                                          │
│    // Send set_gateway_location command                                  │
│    cmdChar.write(JSON.stringify({                                        │
│      "op": "control",                                                    │
│      "type": "set_gateway_location",                                     │
│      "location": userInput.location                                      │
│    }))                                                                   │
│                                                                          │
│    // Update local database                                              │
│    database.updateGateway(shortMac, userInput)                           │
│  }                                                                       │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

### Gateway Registry Database

#### SQLite Schema (Recommended)

```sql
-- Gateway registry table
CREATE TABLE gateways (
    short_mac TEXT PRIMARY KEY,        -- "A3B2C1" (unique identifier)
    ble_name TEXT NOT NULL,            -- "SURIOTA-A3B2C1"
    mac TEXT NOT NULL,                 -- "AA:BB:CC:A3:B2:C1"
    friendly_name TEXT DEFAULT '',     -- "Panel Listrik Gedung A"
    location TEXT DEFAULT '',          -- "Lt.1 Ruang Panel"
    firmware TEXT DEFAULT '',          -- "2.5.31"
    model TEXT DEFAULT '',             -- "SRT-MGATE-1210"
    last_connected TIMESTAMP,          -- Last connection time
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Index for faster lookups
CREATE INDEX idx_gateways_ble_name ON gateways(ble_name);

-- Example queries
-- Get gateway by short_mac
SELECT * FROM gateways WHERE short_mac = 'A3B2C1';

-- Get all registered gateways
SELECT * FROM gateways ORDER BY last_connected DESC;

-- Update last connected time
UPDATE gateways SET last_connected = CURRENT_TIMESTAMP WHERE short_mac = 'A3B2C1';

-- Check if gateway exists
SELECT COUNT(*) FROM gateways WHERE short_mac = 'A3B2C1';
```

#### Room Entity (Android)

```kotlin
@Entity(tableName = "gateways")
data class Gateway(
    @PrimaryKey
    @ColumnInfo(name = "short_mac")
    val shortMac: String,              // "A3B2C1"

    @ColumnInfo(name = "ble_name")
    val bleName: String,               // "SURIOTA-A3B2C1"

    @ColumnInfo(name = "mac")
    val mac: String,                   // "AA:BB:CC:A3:B2:C1"

    @ColumnInfo(name = "friendly_name")
    val friendlyName: String = "",     // "Panel Listrik Gedung A"

    @ColumnInfo(name = "location")
    val location: String = "",         // "Lt.1 Ruang Panel"

    @ColumnInfo(name = "firmware")
    val firmware: String = "",         // "2.5.31"

    @ColumnInfo(name = "model")
    val model: String = "",            // "SRT-MGATE-1210"

    @ColumnInfo(name = "last_connected")
    val lastConnected: Long? = null,

    @ColumnInfo(name = "created_at")
    val createdAt: Long = System.currentTimeMillis()
)

@Dao
interface GatewayDao {
    @Query("SELECT * FROM gateways WHERE short_mac = :shortMac")
    suspend fun getByShortMac(shortMac: String): Gateway?

    @Query("SELECT * FROM gateways ORDER BY last_connected DESC")
    fun getAllGateways(): Flow<List<Gateway>>

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun upsert(gateway: Gateway)

    @Query("UPDATE gateways SET last_connected = :timestamp WHERE short_mac = :shortMac")
    suspend fun updateLastConnected(shortMac: String, timestamp: Long)
}
```

---

### UI/UX Recommendations

#### Gateway List Screen

```
┌─────────────────────────────────────────┐
│ ← Gateways                         ⟳   │
├─────────────────────────────────────────┤
│                                         │
│  ┌─────────────────────────────────┐   │
│  │ 🟢 Panel Listrik Gedung A       │   │
│  │    📍 Lt.1 Ruang Panel          │   │
│  │    📶 Excellent (-45 dBm)       │   │
│  │    v2.5.31 • Connected 2m ago   │   │
│  └─────────────────────────────────┘   │
│                                         │
│  ┌─────────────────────────────────┐   │
│  │ 🟢 Chiller Gedung B             │   │
│  │    📍 Basement Ruang Mesin      │   │
│  │    📶 Good (-62 dBm)            │   │
│  │    v2.5.31 • Connected 5m ago   │   │
│  └─────────────────────────────────┘   │
│                                         │
│  ┌─────────────────────────────────┐   │
│  │ 🔵 New Gateway                  │   │
│  │    SURIOTA-7890AB               │   │
│  │    📶 Fair (-78 dBm)            │   │
│  │    Tap to configure             │   │
│  └─────────────────────────────────┘   │
│                                         │
├─────────────────────────────────────────┤
│  [  🔍 Scan for Gateways  ]            │
└─────────────────────────────────────────┘
```

#### First-Time Setup Dialog

```
┌─────────────────────────────────────────┐
│           Configure Gateway              │
├─────────────────────────────────────────┤
│                                         │
│  Device: SURIOTA-7890AB                 │
│  MAC: 11:22:33:78:90:AB                 │
│  Firmware: v2.5.31                      │
│                                         │
│  ┌─────────────────────────────────┐   │
│  │ Name *                          │   │
│  │ ┌─────────────────────────────┐ │   │
│  │ │ Sensor Gudang Utama         │ │   │
│  │ └─────────────────────────────┘ │   │
│  └─────────────────────────────────┘   │
│                                         │
│  ┌─────────────────────────────────┐   │
│  │ Location (optional)             │   │
│  │ ┌─────────────────────────────┐ │   │
│  │ │ Gedung C Lt.2               │ │   │
│  │ └─────────────────────────────┘ │   │
│  └─────────────────────────────────┘   │
│                                         │
│  ┌─────────────┐  ┌─────────────────┐  │
│  │   Cancel    │  │      Save       │  │
│  └─────────────┘  └─────────────────┘  │
└─────────────────────────────────────────┘
```

---

## Android Implementation Example

### BLE Scanner with Gateway Filter

```kotlin
class GatewayScannerViewModel : ViewModel() {

    private val bluetoothAdapter: BluetoothAdapter? =
        BluetoothAdapter.getDefaultAdapter()

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val deviceName = result.device.name ?: return

            // Filter for SURIOTA gateways only
            if (deviceName.startsWith("SURIOTA-")) {
                val shortMac = deviceName.removePrefix("SURIOTA-")
                val gateway = ScannedGateway(
                    bleName = deviceName,
                    shortMac = shortMac,
                    address = result.device.address,
                    rssi = result.rssi
                )
                _scannedGateways.value += gateway
            }
        }
    }

    fun startScan() {
        val scanner = bluetoothAdapter?.bluetoothLeScanner
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()

        // Optional: Filter by name prefix
        val filter = ScanFilter.Builder()
            .setDeviceName("SURIOTA-") // Note: Partial match may not work on all devices
            .build()

        scanner?.startScan(listOf(filter), settings, scanCallback)

        // Stop scan after 10 seconds
        viewModelScope.launch {
            delay(10_000)
            stopScan()
        }
    }
}
```

### Gateway Info Parser

```kotlin
data class GatewayInfo(
    val bleName: String,
    val mac: String,
    val shortMac: String,
    val friendlyName: String,
    val location: String,
    val firmware: String,
    val model: String,
    val freeHeap: Long,
    val freePsram: Long
)

fun parseGatewayInfo(jsonString: String): GatewayInfo? {
    return try {
        val json = JSONObject(jsonString)
        if (json.getString("status") != "ok") return null

        val data = json.getJSONObject("data")
        GatewayInfo(
            bleName = data.getString("ble_name"),
            mac = data.getString("mac"),
            shortMac = data.getString("short_mac"),
            friendlyName = data.optString("friendly_name", ""),
            location = data.optString("location", ""),
            firmware = data.getString("firmware"),
            model = data.getString("model"),
            freeHeap = data.getLong("free_heap"),
            freePsram = data.getLong("free_psram")
        )
    } catch (e: Exception) {
        null
    }
}
```

### Send BLE Command

```kotlin
class GatewayBleService(private val gatt: BluetoothGatt) {

    companion object {
        val SERVICE_UUID = UUID.fromString("4fafc201-1fb5-459e-8fcc-c5c9c331914b")
        val COMMAND_UUID = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26a8")
        val RESPONSE_UUID = UUID.fromString("d5f78320-e1b1-4f9f-b5b5-e6e9e6e7e8e9")
    }

    suspend fun getGatewayInfo(): GatewayInfo? {
        val command = """{"op":"control","type":"get_gateway_info"}"""
        val response = sendCommand(command)
        return parseGatewayInfo(response)
    }

    suspend fun setFriendlyName(name: String): Boolean {
        val command = """{"op":"control","type":"set_friendly_name","name":"$name"}"""
        val response = sendCommand(command)
        return JSONObject(response).getString("status") == "ok"
    }

    suspend fun setLocation(location: String): Boolean {
        val command = """{"op":"control","type":"set_gateway_location","location":"$location"}"""
        val response = sendCommand(command)
        return JSONObject(response).getString("status") == "ok"
    }

    private suspend fun sendCommand(json: String): String {
        val service = gatt.getService(SERVICE_UUID)
        val cmdChar = service.getCharacteristic(COMMAND_UUID)

        cmdChar.value = json.toByteArray(Charsets.UTF_8)
        gatt.writeCharacteristic(cmdChar)

        // Wait for response notification
        return waitForResponse()
    }
}
```

---

## iOS Implementation Example

### Gateway Scanner (Swift)

```swift
import CoreBluetooth

class GatewayScannerManager: NSObject, CBCentralManagerDelegate {

    private var centralManager: CBCentralManager!
    @Published var scannedGateways: [ScannedGateway] = []

    struct ScannedGateway: Identifiable {
        let id = UUID()
        let bleName: String
        let shortMac: String
        let peripheral: CBPeripheral
        let rssi: Int
    }

    override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: nil)
    }

    func startScan() {
        scannedGateways.removeAll()
        centralManager.scanForPeripherals(withServices: nil, options: [
            CBCentralManagerScanOptionAllowDuplicatesKey: false
        ])

        // Stop after 10 seconds
        DispatchQueue.main.asyncAfter(deadline: .now() + 10) {
            self.stopScan()
        }
    }

    func centralManager(_ central: CBCentralManager,
                       didDiscover peripheral: CBPeripheral,
                       advertisementData: [String : Any],
                       rssi RSSI: NSNumber) {

        guard let name = peripheral.name, name.hasPrefix("SURIOTA-") else { return }

        let shortMac = String(name.dropFirst("SURIOTA-".count))
        let gateway = ScannedGateway(
            bleName: name,
            shortMac: shortMac,
            peripheral: peripheral,
            rssi: RSSI.intValue
        )

        if !scannedGateways.contains(where: { $0.shortMac == shortMac }) {
            scannedGateways.append(gateway)
        }
    }
}
```

### Gateway Info Model (Swift)

```swift
struct GatewayInfo: Codable {
    let bleName: String
    let mac: String
    let shortMac: String
    let friendlyName: String
    let location: String
    let firmware: String
    let model: String
    let freeHeap: Int
    let freePsram: Int

    enum CodingKeys: String, CodingKey {
        case bleName = "ble_name"
        case mac
        case shortMac = "short_mac"
        case friendlyName = "friendly_name"
        case location
        case firmware
        case model
        case freeHeap = "free_heap"
        case freePsram = "free_psram"
    }
}

struct GatewayInfoResponse: Codable {
    let status: String
    let command: String
    let data: GatewayInfo
}

// Parse response
func parseGatewayInfo(jsonData: Data) -> GatewayInfo? {
    let decoder = JSONDecoder()
    guard let response = try? decoder.decode(GatewayInfoResponse.self, from: jsonData) else {
        return nil
    }
    return response.data
}
```

---

## Error Handling

### Possible Errors

| Error Message                      | Cause                                      | Solution                         |
| ---------------------------------- | ------------------------------------------ | -------------------------------- |
| `Gateway config not initialized`   | GatewayConfig singleton failed             | Check ESP32 logs, restart device |
| `name parameter required`          | Missing `name` in set_friendly_name        | Include `name` field in request  |
| `name cannot be empty`             | Empty string for name                      | Provide non-empty name           |
| `name too long (max 32 chars)`     | Name exceeds limit                         | Shorten the name                 |
| `location parameter required`      | Missing `location` in set_gateway_location | Include `location` field         |
| `location too long (max 64 chars)` | Location exceeds limit                     | Shorten the location             |
| `Failed to save friendly name`     | LittleFS write error                       | Check flash storage              |

### Error Response Format

```json
{
  "status": "error",
  "message": "name too long (max 32 chars)"
}
```

---

## FAQ

### Q: Can I change the BLE name prefix from "SURIOTA-"?

**A:** Currently, the prefix is hardcoded. To change it, modify `GatewayConfig::generateBLEName()` in the firmware and recompile.

### Q: What happens if two gateways have the same last 3 MAC bytes?

**A:** This is extremely unlikely (1 in 16.7 million). If it happens, both devices will have the same BLE name but different full MAC addresses. Use the full MAC for identification.

### Q: Is the friendly_name synced to the cloud?

**A:** No, friendly_name is stored locally on the gateway's LittleFS. The mobile app should maintain its own gateway registry for offline access.

### Q: Can I set friendly_name via MQTT?

**A:** Currently, gateway identity commands are only available via BLE. MQTT support can be added in future versions.

### Q: What if the gateway is factory reset?

**A:** Factory reset clears `/gateway_config.json`, so friendly_name and location will be empty. The BLE name (MAC-based) remains unchanged.

---

## See Also

- [API.md](API.md) - Main BLE CRUD API Reference
- [BLE_BACKUP_RESTORE.md](BLE_BACKUP_RESTORE.md) - Configuration Backup/Restore
- [BLE_FACTORY_RESET.md](BLE_FACTORY_RESET.md) - Factory Reset Command
- [VERSION_HISTORY.md](../Changelog/VERSION_HISTORY.md) - Firmware Changelog

---

**Document Version:** 1.0
**Last Updated:** December 04, 2025
**Author:** Kemal (with Claude Code)
