# Suriota Modbus Gateway IIoT \- Product Information

## 1\. RINGKASAN PRODUK

### Nama Produk

**Suriota Modbus Gateway IIoT (SRT-MGate1210)**

### Deskripsi Singkat

Suriota Modbus Gateway IIoT adalah solusi gateway berstandar industri yang dirancang untuk mengintegrasikan sistem otomasi berbasis Modbus ke dalam ekosistem IoT (Internet of Things). Perangkat ini memungkinkan parsing dan mapping data Modbus yang fleksibel, mengkonversi data mentah dari aset industri (sensor, PLC, atau mesin) menjadi protokol IoT modern seperti MQTT/HTTP.

### Keunggulan Utama

* **Multi-Protocol Support**: Modbus RTU ↔ Modbus TCP/IP ↔ MQTT/HTTP

* **Dual Connectivity**: WiFi \+ Ethernet dengan auto-failover

* **No-Code Configuration**: Konfigurasi via aplikasi mobile (Android/iOS)

* **Industrial Grade**: Tahan suhu ekstrem (-40°C hingga 75°C)

* **Secure**: TLS/SSL encryption untuk keamanan data

## 2\. VARIAN PRODUK

### 2.1 Standard Version

* **Network Port**: RJ45 Ethernet 10/100 Mbps

* **Power Supply**: Screw Terminal 12\~48 VDC

* **PoE Support**: Tidak ada

* **Target Environment**: Indoor/controlled environments

* **Harga**: Lebih ekonomis

### 2.2 PoE Version (Power over Ethernet)

* **Network Port**: RJ45 Ethernet 10/100 Mbps \+ PoE

* **Power Supply**: Screw Terminal 12\~48 VDC atau 9W PoE PD (IEEE 802.3af/at)

* **PoE Voltage**: 36 \~ 57 VDC

* **Target Environment**: Outdoor/industrial area

* **Harga**: Premium (dengan fitur PoE)

## 3\. FITUR DAN MANFAAT

### Fitur Utama

1. **BLE Configuration** \- Setup wireless via aplikasi mobile (Android/iOS)

2. **Multi-Protocol Conversion** \- Modbus RTU (RS-485) ↔ Modbus TCP/IP ↔ MQTT/HTTP

3. **High Device Capacity** \- Hingga 32 perangkat Modbus RTU per port (dual RS-485)

4. **Dual Connectivity** \- WiFi (2.4 GHz) \+ Ethernet dengan auto-failover

5. **PoE Support** \- IEEE 802.3af/at untuk kemudahan instalasi (varian PoE)

6. **Live LED Indicators** \- Monitoring real-time status jaringan, sistem, dan power

7. **Local Data Logging** \- MicroSD slot untuk backup data (CSV/JSON) saat offline

8. **Industrial Protection** \- 2kV isolation pada port RS-485

9. **Wide Temperature Range** \- Operasional pada \-40°C hingga 75°C

10. **Redundant Power** \- Dual DC input (12-48 VDC) untuk keandalan

11. **Real-time Monitoring** \- Via Suriota Config app

12. **Time Synchronization** \- NTP/RTC untuk timestamp akurat

13. **Security Features** \- TLS/SSL encryption dan firewall rules

## 4\. SPESIFIKASI TEKNIS

### Hardware

* **Main CPU**: ESP32-S3-WROOM-1 (Dual-core, 240 MHz, 512 KB SRAM, 8 MB PSRAM)

* **WiFi**: 2.4 GHz, 802.11 b/g/n, WPA3-PSK, mode Station/AP/AP+Station

* **Bluetooth**: BLE 5.0, jangkauan hingga 50m (Line of Sight)

* **Storage**: MicroSD hingga 32GB (format CSV/JSON)

* **RTC**: Real-Time Clock dengan akurasi tinggi

### Interface

* **Ethernet**: 1x RJ45 10/100 Mbps dengan Magnetic Isolation

* **USB**: 1x Type-C (Debugging/Firmware update)

* **SD Card**: 1x Micro SD Card Slot

* **Modbus RTU**: 2x Isolated RS-485, hingga 32 device per port

* **Modbus TCP/IP**: Port 502 (default), hingga 32 client TCP

* **Indicators**: 8x LED

* **Button**: 1x Config Button

### Power Specification

* **Input Voltage**: 12V-48V DC (Terminal Block)

* **PoE**: IEEE 802.3af standard 9W (36V-57V DC) \- khusus varian PoE

* **Power Redundancy**: Auto-switch dual power input

### Environmental

* **Operating Temp**: \-40°C hingga 75°C

* **Storage Temp**: \-40°C hingga 85°C

* **Humidity**: 10\~95% RH

### Certifications

* **EMC**: EN 55032

* **EMI**: FCC Part 15

* **EMS**: IEC 61000-4-2/4/5

* **Maritime**: DNV-GL

* **Safety**: UL 60950-1, UL/cUL 62368-1, RoHS

## 5\. SOFTWARE & PROTOKOL

### Dashboard Support

* Grafana

* Aveva

* Suriota Dashboard (proprietary)

### Communication Protocols

* **MQTT**: ISO/IEC 20922 compliant

* **HTTP/HTTPS**: RESTful API support

* **Modbus RTU**: Via RS-485 (9.6-115.2 kbps)

* **Modbus TCP/IP**: Port 502

### Data Format

* JSON payload

* Custom MQTT topics

* CSV logging

## 6\. APLIKASI MOBILE

### Suriota Gateway Config App

* **Platform**: Android 5.0+ / iOS 12+

* **Koneksi**: Bluetooth Low Energy (BLE 5.0)

* **Jangkauan**: Hingga 50m (line of sight)

### Fungsi Aplikasi

1. **Device Discovery** \- Temukan gateway terdekat via BLE

2. **Login & Security** \- Akses aman ke perangkat

3. **Device Communication** \- Setup parameter komunikasi

4. **Modbus Configuration** \- Atur register dan mapping

5. **Server Configuration** \- Setup MQTT/HTTP server

6. **Data Logging** \- Atur interval dan retention

## 7\. USE CASES & APLIKASI

### Target Industri

* **Manufacturing** \- Monitor mesin produksi

* **Oil & Gas** \- Monitor pressure, flow, temperature

* **Marine/Shipyard** \- Integrasi sistem kapal

* **Agriculture** \- Smart farming & irrigation

* **Building Automation** \- HVAC, lighting control

* **Energy Management** \- Monitor konsumsi listrik

### Contoh Implementasi

1. **Factory Floor Monitoring**

   * Baca data dari PLC mesin

   * Kirim ke cloud via MQTT

   * Dashboard real-time di Grafana

2. **Remote Tank Monitoring**

   * Sensor level via Modbus RTU

   * Data logging lokal saat offline

   * Auto-sync saat online

3. **Energy Metering**

   * Baca power meter Modbus

   * Aggregate data dari multiple site

   * Report via HTTP API

## 8\. INSTALASI & SETUP

### Quick Start Guide

1. **Power On** \- Hubungkan power 12-48VDC

2. **Download App** \- Suriota Config di Play Store/App Store

3. **Connect BLE** \- Scan dan pilih device

4. **Configure Network** \- Setup WiFi atau Ethernet

5. **Setup Modbus** \- Atur baud rate, parity, device ID

6. **Configure Server** \- Input MQTT broker atau HTTP endpoint

7. **Test & Deploy** \- Verifikasi data flow

### LED Indicators

* **Power**: Hijau \= Normal, Merah \= Error

* **Network**: Biru berkedip \= Connecting, Biru solid \= Connected

* **RS485-1/2**: Kuning \= Data activity

* **System**: Hijau \= Running, Merah \= Fault

## 9\. TROUBLESHOOTING

### Common Issues

1. **Tidak bisa connect BLE**

   * Pastikan Bluetooth ON

   * Jarak maksimal 50m

   * Reset device dengan config button

2. **Modbus timeout**

   * Cek wiring RS-485 (A+/B-)

   * Verifikasi baud rate & parity

   * Pastikan termination resistor

3. **MQTT disconnect**

   * Cek kredensial broker

   * Verifikasi port (1883/8883)

   * Test koneksi internet

## 10\. TECHNICAL SUPPORT

### Bantuan Teknis

* **Email**: support@suriota.com

* **WhatsApp**: \+62 858-3567-2476

* **Documentation**: docs.suriota.com/gateway

* **Video Tutorial**: YouTube @suriota.official

### Warranty

* **Standard**: 1 tahun parts & service

* **Extended**: Hingga 3 tahun (opsional)

## 11\. PRICING & ROI ANALYSIS

### 11.1 Harga Produk

#### *Standard Version*

* **Unit Price**: Rp 4.500.000 \- Rp 5.500.000

* **Bulk Order (10+ units)**: Diskon 10-15%

* **Enterprise (50+ units)**: Diskon 20-25%

#### *PoE Version*

* **Unit Price**: Rp 5.500.000 \- Rp 6.500.000

* **Bulk Order (10+ units)**: Diskon 10-15%

* **Enterprise (50+ units)**: Diskon 20-25%

### 11.2 Total Cost of Ownership (TCO)

#### *3-Year TCO Analysis*

* **Initial Hardware Cost**: Rp 5.000.000 (average)

* **Installation & Setup**: Rp 500.000

* **Annual Maintenance**: Rp 200.000/tahun

* **Software License**: Free (included)

* **Total 3-Year TCO**: Rp 6.100.000

### 11.3 Return on Investment (ROI)

#### *Typical ROI Scenarios*

1. **Manufacturing Monitoring**

   * **Investment**: Rp 6.100.000 (3-year TCO)

   * **Annual Savings**: Rp 12.000.000 (downtime reduction)

   * **ROI**: 197% dalam 3 tahun

   * **Payback Period**: 6 bulan

2. **Energy Management**

   * **Investment**: Rp 6.100.000

   * **Annual Savings**: Rp 8.000.000 (energy efficiency)

   * **ROI**: 131% dalam 3 tahun

   * **Payback Period**: 9 bulan

3. **Remote Monitoring**

   * **Investment**: Rp 6.100.000

   * **Annual Savings**: Rp 6.000.000 (operational cost)

   * **ROI**: 98% dalam 3 tahun

   * **Payback Period**: 12 bulan

### 11.4 Value Proposition

* **Mengurangi Downtime**: Hingga 30% dengan predictive maintenance

* **Efisiensi Energi**: 10-20% penghematan konsumsi listrik

* **Labor Cost Reduction**: 40% pengurangan manual monitoring

* **Data-Driven Decisions**: Real-time insights untuk optimasi operasi

## 12\. COMPETITIVE ANALYSIS

### 12.1 Market Position

**Suriota Modbus Gateway** positioned sebagai **mid-range industrial gateway** dengan focus pada **ease of use** dan **Indonesian market support**.

### 12.2 Competitor Comparison

#### *Vs. International Brands*

| Feature | Suriota MGate1210 | Moxa MGate MB3180 | Advantech WISE-4012E | Red Lion DA30D |
| :---- | :---- | :---- | :---- | :---- |
| **Price Range** | Rp 4.5-6.5M | Rp 8-12M | Rp 7-10M | Rp 9-15M |
| **Mobile Config** | ✅ BLE App | ❌ Web only | ❌ Web only | ❌ Web only |
| **Local Support** | ✅ Indonesia | ❌ Singapore | ❌ Taiwan | ❌ USA |
| **PoE Support** | ✅ Optional | ✅ Standard | ✅ Standard | ✅ Standard |
| **Temperature Range** | \-40°C to 75°C | \-40°C to 75°C | \-25°C to 70°C | \-40°C to 70°C |
| **Dual RS-485** | ✅ 2 ports | ✅ 2 ports | ❌ 1 port | ✅ 2 ports |
| **Local Data Logging** | ✅ MicroSD | ✅ Internal | ✅ Internal | ✅ Internal |
| **WiFi \+ Ethernet** | ✅ Dual | ❌ Ethernet only | ✅ Dual | ❌ Ethernet only |

### 12.3 Competitive Advantages

1. **Price Competitiveness**: 30-50% lebih murah dari kompetitor internasional

2. **Local Support**: Technical support dalam Bahasa Indonesia

3. **Mobile-First Configuration**: BLE app untuk setup tanpa laptop

4. **Dual Connectivity**: WiFi \+ Ethernet dengan auto-failover

5. **Fast Lead Time**: 1-3 hari vs 2-8 minggu kompetitor import

### 12.4 Market Challenges

1. **Brand Recognition**: Perlu waktu membangun trust vs established brands

2. **Distribution Network**: Masih terbatas dibanding multinational companies

3. **Certification**: Beberapa sertifikasi internasional masih dalam proses

## 13\. MARKET SIZING & OPPORTUNITY

### 13.1 Target Market Size

#### *Indonesia Industrial IoT Gateway Market*

* **Total Addressable Market (TAM)**: $45M USD (2024)

* **Serviceable Available Market (SAM)**: $15M USD

* **Serviceable Obtainable Market (SOM)**: $3M USD (target 3-5 years)

### 13.2 Market Segments

#### *Primary Segments (80% focus)*

1. **Manufacturing Industry**: 40% market share

   * Automotive, Textile, Food & Beverage

   * Market Size: $6M USD annually

2. **Oil & Gas**: 25% market share

   * Upstream, Midstream, Downstream

   * Market Size: $3.75M USD annually

3. **Marine & Shipyard**: 15% market share

   * Ship building, Port operations

   * Market Size: $2.25M USD annually

#### *Secondary Segments (20% focus)*

4. **Agriculture**: 10% market share

5. **Building Automation**: 5% market share

6. **Others**: 5% market share

### 13.3 Growth Projections

* **2024**: $500K USD revenue target

* **2025**: $1.2M USD (140% growth)

* **2026**: $2.1M USD (75% growth)

* **2027**: $3.0M USD (43% growth)

### 13.4 Market Drivers

1. **Digital Transformation**: Industry 4.0 adoption

2. **Government Support**: Making Indonesia 4.0 initiative

3. **Cost Optimization**: Post-pandemic efficiency focus

4. **Local Content**: TKDN (Tingkat Komponen Dalam Negeri) requirements

## 14\. CUSTOMER TESTIMONIALS & CASE STUDIES

### 14.1 Customer Success Stories

#### *Case Study 1: PT Astra Daihatsu Motor (Manufacturing)*

**Challenge**: Manual monitoring of 50+ production machines **Solution**: 25 units Suriota Modbus Gateway for real-time monitoring **Results**: \- 35% reduction in unplanned downtime \- Rp 2.4M monthly savings in maintenance costs \- 99.2% data availability

*“Suriota Gateway membantu kami mengoptimalkan production line dengan monitoring real-time yang akurat dan mudah digunakan.”* \- Plant Manager ADM

#### *Case Study 2: PT Pertamina EP (Oil & Gas)*

**Challenge**: Remote monitoring of offshore platform equipment **Solution**: 15 units with dual connectivity (WiFi backup) **Results**: \- 24/7 monitoring capability \- 50% reduction in field visits \- Real-time alert system for critical parameters

*“Reliability tinggi dan local support yang responsif membuat Suriota menjadi pilihan tepat untuk operasi critical kami.”* \- Operations Manager

#### *Case Study 3: PT PAL Indonesia (Marine)*

**Challenge**: Integration of legacy Modbus devices to modern IoT **Solution**: 8 units for ship building process monitoring **Results**: \- Seamless integration with existing systems \- Improved project tracking and reporting \- 25% faster commissioning process

### 14.2 Customer Satisfaction Metrics

* **Overall Satisfaction**: 4.7/5.0

* **Product Quality**: 4.8/5.0

* **Technical Support**: 4.6/5.0

* **Value for Money**: 4.9/5.0

* **Recommendation Rate**: 95%

## 15\. SALES FUNNEL & DISTRIBUTION CHANNELS

### 15.1 Sales Process

#### *B2B Sales Funnel*

1. **Awareness** (100 leads/month)

   * Digital marketing, trade shows, referrals

2. **Interest** (40 qualified leads/month)

   * Technical presentations, demos

3. **Consideration** (15 proposals/month)

   * PoC, technical evaluation, pricing

4. **Purchase** (5 orders/month)

   * Contract negotiation, delivery

5. **Retention** (95% customer retention)

   * Support, upselling, renewals

#### *Conversion Rates*

* **Lead to Qualified**: 40%

* **Qualified to Proposal**: 37.5%

* **Proposal to Purchase**: 33.3%

* **Overall Conversion**: 5%

### 15.2 Distribution Channels

#### *Direct Sales (70% revenue)*

* Enterprise accounts (\>50 units)

* Key account management

* Technical pre-sales support

#### *Channel Partners (25% revenue)*

* System integrators

* Industrial distributors

* Regional partners

#### *Online Sales (5% revenue)*

* E-commerce platforms

* Direct website sales

* Sample orders

### 15.3 Sales Team Structure

* **Sales Director**: Strategic accounts

* **Account Managers** (2): Regional coverage

* **Technical Sales Engineers** (3): Pre-sales support

* **Channel Manager**: Partner development

## 16\. TECHNICAL ROADMAP & FUTURE DEVELOPMENT

### 16.1 Product Roadmap 2024-2026

#### *Q4 2024 \- Version 2.0*

* **Enhanced Security**: Advanced encryption, VPN support

* **Edge Computing**: Local data processing capabilities

* **OPC-UA Support**: Additional protocol support

* **Cloud Integration**: AWS IoT, Azure IoT native support

#### *Q1-Q2 2025 \- Version 2.5*

* **AI/ML Edge**: Built-in anomaly detection

* **5G Connectivity**: Optional 5G modul

* **Digital Twin**: Real-time device modeling

* **Advanced Analytics**: Local trend analysis

#### *Q3-Q4 2025 \- Version 3.0*

* **Multi-Tenant**: Support multiple customers

* **Blockchain**: Secure data integrity

* **AR/VR Support**: Augmented reality configuration

* **Energy Harvesting**: Solar power option

### 16.2 Technology Evolution

#### *Current Generation (Gen 1\)*

* ESP32-S3 based

* Basic connectivity protocols

* Mobile configuration

#### *Next Generation (Gen 2\) \- 2025*

* ARM Cortex-A based

* Edge AI capabilities

* 5G connectivity

#### *Future Generation (Gen 3\) \- 2026+*

* Custom silicon

* Quantum encryption

* Satellite connectivity

### 16.3 R\&D Investment

* **2024**: 15% of revenue ($75K USD)

* **2025**: 18% of revenue ($216K USD)

* **2026**: 20% of revenue ($420K USD)

### 16.4 Partnership Strategy

* **Technology Partners**: Intel, Qualcomm, AWS

* **Academic Partners**: ITB, ITS, Politeknik Batam

* **Industry Partners**: Schneider Electric, Siemens

## 17\. INFORMASI PEMESANAN

### Package Contents

* 1x Suriota Modbus Gateway

* 1x Power Terminal Block Connector

* 1x Quick Start Guide

* 1x Warranty Card

### Minimum Order

* Sample: 1 unit

* Bulk: 10 units (diskon tersedia)

### Lead Time

* Stock: 1-3 hari kerja

* Custom order: 2-4 minggu

---

