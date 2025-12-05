# BLE Gateway Identity API

**SRT-MGATE-1210 Modbus IIoT Gateway**
Multi-Gateway Support & Device Identification

[Home](../../README.md) > [Documentation](../README.md) > [API Reference](API.md) > Gateway Identity

**Version:** 2.5.32
**Release Date:** December 05, 2025
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

Starting from firmware **v2.5.32**, the SRT-MGATE-1210 gateway uses a **product-based BLE naming convention**. Each gateway automatically generates a **unique BLE name** with product model and variant information, plus a unique identifier from its MAC address.

### Key Features

| Feature                | Description                                                              |
| ---------------------- | ------------------------------------------------------------------------ |
| **Product BLE Name**   | Auto-generated: `MGate-1210(P)-XXXX` (POE) or `MGate-1210-XXXX` (Non-POE)|
| **Serial Number**      | Auto-generated: `SRT-MGATE1210P-YYYYMMDD-XXXXXX` (18+ digits)            |
| **Friendly Name**      | User-configurable custom name (max 32 chars)                             |
| **Location**           | Optional location info (max 64 chars)                                    |
| **Persistent Storage** | Config saved to `/gateway_config.json` on LittleFS                       |
| **Zero Configuration** | Works out-of-box, no manual setup required                               |
| **Variant Support**    | POE (P) and Non-POE variants configurable in `ProductConfig.h`           |

---

## Multi-Gateway Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                          MOBILE APP                                  │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│   ┌─────────────────┐                                               │
│   │   BLE SCANNER   │  ← Scans for "MGate-1210*" devices            │
│   └────────┬────────┘                                               │
│            │                                                         │
│            ▼                                                         │
│   ┌─────────────────────────────────────────────────────────────┐   │
│   │                    SCAN RESULTS                              │   │
│   │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐          │   │
│   │  │ MGate-1210  │  │ MGate-1210  │  │ MGate-1210  │          │   │
│   │  │ (P)-A716    │  │ (P)-B213    │  │ (P)-C726    │          │   │
│   │  │ RSSI: -45   │  │ RSSI: -62   │  │ RSSI: -78   │          │   │
│   │  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘          │   │
│   └─────────┼────────────────┼────────────────┼─────────────────┘   │
│             │                │                │                      │
│             ▼                ▼                ▼                      │
│   ┌─────────────────────────────────────────────────────────────┐   │
│   │              GATEWAY REGISTRY (Local SQLite)                 │   │
│   │  ┌────────────────────────────────────────────────────────┐ │   │
│   │  │ UID: A716 → "Panel Listrik Gedung A" (Lt.1 R.Panel)    │ │   │
│   │  │ UID: B213 → "Chiller Gedung B" (Basement R.Mesin)      │ │   │
│   │  │ UID: C726 → "(New Device)" ← Not registered yet        │ │   │
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
│   │  │ 🔵 New Gateway (MGate-1210(P)-C726)                 │    │   │
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
│  │  BLE: MGate-1210 │  │  BLE: MGate-1210 │  │  BLE: MGate-1210 │   │
│  │       (P)-A716   │  │       (P)-B213   │  │       (P)-C726   │   │
│  │  MAC: AA:BB:CC:  │  │  MAC: DD:EE:FF:  │  │  MAC: 11:22:33:  │   │
│  │       DD:A7:16   │  │       44:B2:13   │  │       CC:C7:26   │   │
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

Each gateway automatically generates a unique BLE advertising name based on product model and variant:

### v2.5.32+ Format (Current)

```
MGate-1210(P)-XXXX    ← POE Variant
MGate-1210-XXXX       ← Non-POE Variant
           └───────── Last 2 bytes of Bluetooth MAC address (4 hex chars)
```

### Examples (v2.5.32+)

| Variant  | Full MAC Address    | BLE Name              |
| -------- | ------------------- | --------------------- |
| POE      | `AA:BB:CC:DD:A7:16` | `MGate-1210(P)-A716`  |
| POE      | `11:22:33:44:B2:13` | `MGate-1210(P)-B213`  |
| Non-POE  | `FF:EE:DD:CC:C7:26` | `MGate-1210-C726`     |

### Legacy Format (v2.5.31)

```
SURIOTA-XXXXXX
        └─────── Last 3 bytes of Bluetooth MAC address (6 hex chars)
```

| Full MAC Address    | BLE Name (Legacy) |
| ------------------- | ----------------- |
| `AA:BB:CC:A3:B2:C1` | `SURIOTA-A3B2C1`  |

### Why 4 Hex Chars (2 Bytes)?

- **Uniqueness**: 65,536 combinations (2^16) - sufficient for most deployments
- **Readability**: 4 characters is very easy to read and remember
- **BLE Limit**: Shorter name allows more space for product model info
- **Product Branding**: Includes "MGate-1210" product name + variant indicator

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
    "ble_name": "MGate-1210(P)-A716",
    "mac": "AA:BB:CC:DD:A7:16",
    "uid": "A716",
    "short_mac": "DDA716",
    "serial_number": "SRT-MGATE1210P-20251205-DDA716",
    "friendly_name": "Panel Listrik Gedung A",
    "location": "Lt.1 Ruang Panel",
    "firmware": "2.5.32",
    "build_number": 2532,
    "model": "MGate-1210(P)",
    "variant": "P",
    "is_poe": true,
    "manufacturer": "SURIOTA"
  }
}
```

#### Response Fields

| Field            | Type    | Description                                    |
| ---------------- | ------- | ---------------------------------------------- |
| `ble_name`       | string  | Auto-generated BLE advertising name            |
| `mac`            | string  | Full Bluetooth MAC address                     |
| `uid`            | string  | Unique ID (last 4 hex chars of MAC)            |
| `short_mac`      | string  | Last 6 hex chars (for compatibility)           |
| `serial_number`  | string  | Full serial number                             |
| `friendly_name`  | string  | User-set custom name (empty if not set)        |
| `location`       | string  | User-set location (empty if not set)           |
| `firmware`       | string  | Current firmware version                       |
| `build_number`   | number  | Firmware build number for OTA comparison       |
| `model`          | string  | Full product model (e.g., "MGate-1210(P)")     |
| `variant`        | string  | Product variant ("P" for POE, "" for Non-POE)  |
| `is_poe`         | boolean | Whether this is POE variant                    |
| `manufacturer`   | string  | Manufacturer name                              |

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
  "ble_name": "MGate-1210(P)-A716",
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
  "ble_name": "MGate-1210(P)-A716",
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
            │ "MGate-1210*" │            │ response      │   │ local DB      │
            └───────────────┘            └───────────────┘   └───────────────┘
```

#### Step-by-Step Flow

```
┌──────────────────────────────────────────────────────────────────────────┐
│ STEP 1: BLE SCAN                                                          │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  // Start BLE scan with name filter (supports both formats)              │
│  scanFilters = ["MGate-1210", "SURIOTA-"]  // v2.5.32+ and legacy       │
│                                                                          │
│  // Results:                                                             │
│  [                                                                       │
│    { name: "MGate-1210(P)-A716", rssi: -45, address: "AA:BB:CC:DD:A7:16"},│
│    { name: "MGate-1210(P)-B213", rssi: -62, address: "DD:EE:FF:44:B2:13"},│
│    { name: "MGate-1210(P)-C726", rssi: -78, address: "11:22:33:CC:C7:26"} │
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
│    uid = extractUID(device.name)  // "A716" from "MGate-1210(P)-A716"   │
│    cachedInfo = database.getGateway(uid)                                 │
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
│  //   ble_name: "MGate-1210(P)-A716",                                   │
│  //   mac: "AA:BB:CC:DD:A7:16",                                         │
│  //   uid: "A716",                                                      │
│  //   friendly_name: "Panel Listrik Gedung A",                          │
│  //   location: "Lt.1 Ruang Panel",                                     │
│  //   firmware: "2.5.32",                                               │
│  //   model: "MGate-1210(P)"                                            │
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
│    uid: gatewayInfo.uid,                // Primary key (e.g., "A716")   │
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
│    database.updateGateway(uid, userInput)                                │
│  }                                                                       │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

### Gateway Registry Database

#### SQLite Schema (Recommended)

```sql
-- Gateway registry table (v2.5.32+)
CREATE TABLE gateways (
    uid TEXT PRIMARY KEY,              -- "A716" (unique identifier, 4 hex chars)
    ble_name TEXT NOT NULL,            -- "MGate-1210(P)-A716"
    mac TEXT NOT NULL,                 -- "AA:BB:CC:DD:A7:16"
    friendly_name TEXT DEFAULT '',     -- "Panel Listrik Gedung A"
    location TEXT DEFAULT '',          -- "Lt.1 Ruang Panel"
    firmware TEXT DEFAULT '',          -- "2.5.32"
    model TEXT DEFAULT '',             -- "MGate-1210(P)"
    last_connected TIMESTAMP,          -- Last connection time
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Index for faster lookups
CREATE INDEX idx_gateways_ble_name ON gateways(ble_name);

-- Example queries
-- Get gateway by uid
SELECT * FROM gateways WHERE uid = 'A716';

-- Get all registered gateways
SELECT * FROM gateways ORDER BY last_connected DESC;

-- Update last connected time
UPDATE gateways SET last_connected = CURRENT_TIMESTAMP WHERE uid = 'A716';

-- Check if gateway exists
SELECT COUNT(*) FROM gateways WHERE uid = 'A716';
```

#### Room Entity (Android)

```kotlin
@Entity(tableName = "gateways")
data class Gateway(
    @PrimaryKey
    @ColumnInfo(name = "uid")
    val uid: String,                   // "A716" (4 hex chars)

    @ColumnInfo(name = "ble_name")
    val bleName: String,               // "MGate-1210(P)-A716"

    @ColumnInfo(name = "mac")
    val mac: String,                   // "AA:BB:CC:DD:A7:16"

    @ColumnInfo(name = "friendly_name")
    val friendlyName: String = "",     // "Panel Listrik Gedung A"

    @ColumnInfo(name = "location")
    val location: String = "",         // "Lt.1 Ruang Panel"

    @ColumnInfo(name = "firmware")
    val firmware: String = "",         // "2.5.32"

    @ColumnInfo(name = "model")
    val model: String = "",            // "MGate-1210(P)"

    @ColumnInfo(name = "last_connected")
    val lastConnected: Long? = null,

    @ColumnInfo(name = "created_at")
    val createdAt: Long = System.currentTimeMillis()
)

@Dao
interface GatewayDao {
    @Query("SELECT * FROM gateways WHERE uid = :uid")
    suspend fun getByUID(uid: String): Gateway?

    @Query("SELECT * FROM gateways ORDER BY last_connected DESC")
    fun getAllGateways(): Flow<List<Gateway>>

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun upsert(gateway: Gateway)

    @Query("UPDATE gateways SET last_connected = :timestamp WHERE uid = :uid")
    suspend fun updateLastConnected(uid: String, timestamp: Long)
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
│  │    v2.5.32 • Connected 2m ago   │   │
│  └─────────────────────────────────┘   │
│                                         │
│  ┌─────────────────────────────────┐   │
│  │ 🟢 Chiller Gedung B             │   │
│  │    📍 Basement Ruang Mesin      │   │
│  │    📶 Good (-62 dBm)            │   │
│  │    v2.5.32 • Connected 5m ago   │   │
│  └─────────────────────────────────┘   │
│                                         │
│  ┌─────────────────────────────────┐   │
│  │ 🔵 New Gateway                  │   │
│  │    MGate-1210(P)-C726           │   │
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
│  Device: MGate-1210(P)-C726             │
│  MAC: 11:22:33:CC:C7:26                 │
│  Firmware: v2.5.32                      │
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

    // Support both v2.5.32+ and legacy formats
    companion object {
        const val NAME_PREFIX_NEW = "MGate-1210"      // v2.5.32+
        const val NAME_PREFIX_LEGACY = "SURIOTA-"    // v2.5.31 and older
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val deviceName = result.device.name ?: return

            // Filter for MGate gateways (v2.5.32+) or legacy SURIOTA gateways
            when {
                deviceName.startsWith(NAME_PREFIX_NEW) -> {
                    // v2.5.32+: "MGate-1210(P)-A716" or "MGate-1210-A716"
                    val uid = deviceName.takeLast(4)  // Last 4 chars = UID
                    addGateway(deviceName, uid, result)
                }
                deviceName.startsWith(NAME_PREFIX_LEGACY) -> {
                    // Legacy: "SURIOTA-A3B2C1"
                    val shortMac = deviceName.removePrefix(NAME_PREFIX_LEGACY)
                    addGateway(deviceName, shortMac, result)
                }
            }
        }

        private fun addGateway(name: String, uid: String, result: ScanResult) {
            val gateway = ScannedGateway(
                bleName = name,
                uid = uid,
                address = result.device.address,
                rssi = result.rssi
            )
            _scannedGateways.value += gateway
        }
    }

    fun startScan() {
        val scanner = bluetoothAdapter?.bluetoothLeScanner
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()

        // Scan without filter - we'll filter in callback
        scanner?.startScan(null, settings, scanCallback)

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
    val uid: String,
    val shortMac: String,
    val serialNumber: String,
    val friendlyName: String,
    val location: String,
    val firmware: String,
    val buildNumber: Int,
    val model: String,
    val variant: String,
    val isPoe: Boolean,
    val manufacturer: String
)

fun parseGatewayInfo(jsonString: String): GatewayInfo? {
    return try {
        val json = JSONObject(jsonString)
        if (json.getString("status") != "ok") return null

        val data = json.getJSONObject("data")
        GatewayInfo(
            bleName = data.getString("ble_name"),
            mac = data.getString("mac"),
            uid = data.optString("uid", data.optString("short_mac", "")),
            shortMac = data.optString("short_mac", ""),
            serialNumber = data.optString("serial_number", ""),
            friendlyName = data.optString("friendly_name", ""),
            location = data.optString("location", ""),
            firmware = data.getString("firmware"),
            buildNumber = data.optInt("build_number", 0),
            model = data.getString("model"),
            variant = data.optString("variant", ""),
            isPoe = data.optBoolean("is_poe", false),
            manufacturer = data.optString("manufacturer", "SURIOTA")
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

    // Support both v2.5.32+ and legacy formats
    static let namePrefixNew = "MGate-1210"      // v2.5.32+
    static let namePrefixLegacy = "SURIOTA-"     // v2.5.31 and older

    struct ScannedGateway: Identifiable {
        let id = UUID()
        let bleName: String
        let uid: String
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

        guard let name = peripheral.name else { return }

        var uid: String?

        if name.hasPrefix(Self.namePrefixNew) {
            // v2.5.32+: "MGate-1210(P)-A716" or "MGate-1210-A716"
            uid = String(name.suffix(4))  // Last 4 chars = UID
        } else if name.hasPrefix(Self.namePrefixLegacy) {
            // Legacy: "SURIOTA-A3B2C1"
            uid = String(name.dropFirst(Self.namePrefixLegacy.count))
        }

        guard let extractedUID = uid else { return }

        let gateway = ScannedGateway(
            bleName: name,
            uid: extractedUID,
            peripheral: peripheral,
            rssi: RSSI.intValue
        )

        if !scannedGateways.contains(where: { $0.uid == extractedUID }) {
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
    let uid: String
    let shortMac: String
    let serialNumber: String
    let friendlyName: String
    let location: String
    let firmware: String
    let buildNumber: Int
    let model: String
    let variant: String
    let isPoe: Bool
    let manufacturer: String

    enum CodingKeys: String, CodingKey {
        case bleName = "ble_name"
        case mac
        case uid
        case shortMac = "short_mac"
        case serialNumber = "serial_number"
        case friendlyName = "friendly_name"
        case location
        case firmware
        case buildNumber = "build_number"
        case model
        case variant
        case isPoe = "is_poe"
        case manufacturer
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

### Q: Can I change the BLE name prefix from "MGate-1210"?

**A:** Starting from v2.5.32, the BLE name format is defined in `ProductConfig.h`. To change it, modify `BLE_NAME_PREFIX` and recompile the firmware.

### Q: What happens if two gateways have the same last 2 MAC bytes?

**A:** This is unlikely (1 in 65,536). If it happens, both devices will have the same BLE name suffix but different full MAC addresses. Use the full MAC for identification.

### Q: How do I support both old and new BLE name formats?

**A:** Scan for devices starting with "MGate-1210" (v2.5.32+) or "SURIOTA-" (legacy). The Python test scripts and mobile app examples in this document show how to handle both formats.

### Q: Is the friendly_name synced to the cloud?

**A:** No, friendly_name is stored locally on the gateway's LittleFS. The mobile app should maintain its own gateway registry for offline access.

### Q: Can I set friendly_name via MQTT?

**A:** Currently, gateway identity commands are only available via BLE. MQTT support can be added in future versions.

### Q: What if the gateway is factory reset?

**A:** Factory reset clears `/gateway_config.json`, so friendly_name and location will be empty. The BLE name (MAC-based) remains unchanged.

### Q: What's the difference between `uid` and `short_mac`?

**A:** In v2.5.32+, `uid` is the last 4 hex chars of MAC (2 bytes), used in the BLE name. `short_mac` is the last 6 hex chars (3 bytes), kept for backward compatibility with legacy systems.

---

## See Also

- [API.md](API.md) - Main BLE CRUD API Reference
- [BLE_BACKUP_RESTORE.md](BLE_BACKUP_RESTORE.md) - Configuration Backup/Restore
- [BLE_FACTORY_RESET.md](BLE_FACTORY_RESET.md) - Factory Reset Command
- [VERSION_HISTORY.md](../Changelog/VERSION_HISTORY.md) - Firmware Changelog

---

**Document Version:** 2.0
**Last Updated:** December 05, 2025
**Author:** Kemal (with Claude Code)
