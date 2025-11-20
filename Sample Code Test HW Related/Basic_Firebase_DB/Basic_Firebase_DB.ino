#include <WiFi.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <Firebase_ESP_Client.h>
#include <ModbusMaster.h>
#include <RTClib.h>

#include <addons/TokenHelper.h>  // Provide the token generation process info.
#include <addons/RTDBHelper.h>   // Provide the RTDB payload printing info and other helper functions.

#define WIFI_SSID "EMBEDDETRONICS_LAB"
#define WIFI_PASSWORD "Lab12345678!@"

// For using other Ethernet library that works with other Ethernet module,
// the following build flags or macros should be assigned in src/FirebaseFS.h or your CustomFirebaseFS.h.
// FIREBASE_ETHERNET_MODULE_LIB and FIREBASE_ETHERNET_MODULE_CLASS
// See src/FirebaseFS.h for detail.

// For the following credentials, see examples/Authentications/SignInAsUser/EmailPassword/EmailPassword.ino

/* Define the API Key */
#define API_KEY "yf0uxVzPfSuqF7leRyRwI0Xjo6idSmCd8Lig8gdA"
/* Define the RTDB URL */
#define DATABASE_URL "test-84f03-default-rtdb.firebaseio.com"  //<databaseName>.firebaseio.com or <databaseName>.<region>.firebasedatabase.app

/* Define the user Email and password that alreadey registerd or added in your project */
#define USER_EMAIL "muhamad.ilham1969@gmail.com"
#define USER_PASSWORD "1234567890-"

/*Defined the Ethernet module connection */
#define ETH_RST 3  // Connect W5500 Reset pin to GPIO 26 of ESP32 (-1 for no reset pin assigned)
#define ETH_MOSI 14
#define ETH_SCK 47
#define ETH_MISO 21
#define ETH_CS 48          //48
#define SPI_FREQ 32000000  //32000000 //20000000

#define I2C_SDA 5
#define I2C_SCL 6
RTC_DS3231 rtc;

char daysOfTheWeek[7][12] = { "AHAD ", "SENIN ", "SELASA ", "RABU ", "KAMIS ", "JUM`AT ", "SABTU " };
char monthsOfYear[12][12] = { "JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE", "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER ", "DECEMBER" };
String date_now, time_now;
int now_hour, now_minute, now_second;

#define PIN_BUTTON 0
#define DIGITAL_READ_BTN1 digitalRead(PIN_BUTTON)

#define RXD1_RS485 15
#define TXD1_RS485 16
// #define RXD2_RS485 17
// #define TXD2_RS485 18
ModbusMaster node;
uint8_t result;
float temp, hum;

// MICRO SD PIN
#define SD_MOSI 10  //11
#define SD_SCK 12
#define SD_MISO 13
#define SD_CS 11  //SS //10 11

#define VSPI FSPI
// SPIClass SPI2(SPI);
SPIClass SPI3(VSPI);

/* Define MAC */
// uint8_t Eth_MAC[] = { 0x02, 0xF0, 0x0D, 0xBE, 0xEF, 0x01 };
byte Eth_MAC[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

/* Define the static IP(Optional) */
IPAddress localIP(192, 168, 0, 177);
IPAddress subnet(255, 255, 0, 0);
IPAddress gateway(192, 168, 0, 1);
// IPAddress dnsServer(8, 8, 8, 8);
IPAddress dnsServer(192, 168, 0, 1);
bool optional = true;  // Use this static IP only no DHCP
Firebase_StaticIP staIP(localIP, subnet, gateway, dnsServer, optional);


// Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

EthernetClient eth;

bool eth_mode = false;
bool internet = false;
void setup() {
  Serial.begin(9600);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  Serial1.begin(9600, SERIAL_8N1, RXD1_RS485, TXD1_RS485);  //SERIAL_8E1
  node.begin(1, Serial1);

  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  SPI3.begin(ETH_SCK, ETH_MISO, ETH_MOSI, ETH_CS);
  Ethernet.init(ETH_CS);

  Serial.println();
  Serial.println("Trying connect with ETHERNET...");

  if (Ethernet.begin(Eth_MAC) == 0) {
    eth_mode = false;
    internet = false;
    Serial.println("Failed to configure Ethernet using DHCP");
    // Check for Ethernet hardware present
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println("Ethernet shield was not found. Sorry, can't run without hardware. :(");
      // while (true) {
      //   delay(1);  // do nothing, no point running without Ethernet hardware
      // }
    }
    if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println("Ethernet cable is not connected.");
    }
    // initialize the Ethernet device not using DHCP:
    //Ethernet.begin(Eth_MAC, localIP, dnsServer, gateway, subnet);
  } else {
    Serial.println(Ethernet.localIP());
    eth_mode = true;
    internet = true;
  }

  // if Ethernet failed, trying connect with Wi-Fi.
  if (!eth_mode) {
    Serial.println("Trying connect with Wi-Fi...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    for (int i = 0; i <= 15; i++) {
      Serial.print(".");
      delay(500);
      // if (WiFi.status() != WL_CONNECTED && i == 15) {
      //   delay(300);
      // }

      if (WiFi.status() == WL_CONNECTED) i = 1000;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n\nWiFi Connected");
      Serial.println(WiFi.localIP());
      internet = true;
    } else {
      Serial.println("\n\nWiFi Not Connected");
      internet = false;
    }
  }

  if (!internet) {
    Serial.println("Can't connect to internet. Sorry, can't run without internet");
    while (true) {
      delay(1);  // do nothing, no point running without Ethernet hardware
    }
  }


  Serial_Printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the user sign in credentials */
  // auth.user.email = USER_EMAIL;
  // auth.user.password = USER_PASSWORD;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  // legacy authenticate method
  config.signer.tokens.legacy_token = API_KEY;

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback;  // see addons/TokenHelper.h

  /* Assign the pointer to global defined Ethernet Client object */
  if (eth_mode) fbdo.setEthernetClient(&eth, Eth_MAC, ETH_CS, ETH_RST);  // The staIP can be assigned to the fifth param
  else Firebase.begin(&config, &auth);

  // Comment or pass false value when WiFi reconnection will control by your code or third party library e.g. WiFiManager
  Firebase.reconnectNetwork(true);

  // Since v4.4.x, BearSSL engine was used, the SSL buffer need to be set.
  // Large data transmission may require larger RX buffer, otherwise connection issue or data read time out can be occurred.
  fbdo.setBSSLBufferSize(4096 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);
  Firebase.setDoubleDigits(5);
  Firebase.begin(&config, &auth);

  Wire.begin(I2C_SDA, I2C_SCL);
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }

  // When time needs to be re-set on a previously configured device, the
  // following line sets the RTC to the date & time this sketch was compiled
  //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
}

void loop() {
  getDateTime();
  get_modbus();

  Serial.printf("RTC... %s\n", Firebase.RTDB.setString(&fbdo, F("/DateTime"), (String)date_now + " " + time_now) ? "ok" : fbdo.errorReason().c_str());
  Serial.printf("RS485... %s\n", Firebase.RTDB.setString(&fbdo, F("/RS485"), (String)temp + "C," + hum + "%") ? "ok" : fbdo.errorReason().c_str());

  if (!DIGITAL_READ_BTN1) Serial.printf("Button Pressed... %s\n", Firebase.RTDB.setString(&fbdo, F("/ButtonState"), "Pressed") ? "ok" : fbdo.errorReason().c_str());
  else Serial.printf("Button Not Pressed... %s\n", Firebase.RTDB.setString(&fbdo, F("/ButtonState"), "Not Pressed") ? "ok" : fbdo.errorReason().c_str());



  // Firebase.ready() should be called repeatedly to handle authentication tasks.

  // if (Firebase.ready() && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0)) {
  //   sendDataPrevMillis = millis();
  //   Serial_Printf("Set bool... %s\n", Firebase.RTDB.setBool(&fbdo, F("/test/bool"), count % 2 == 0) ? "ok" : fbdo.errorReason().c_str());
  //   count++;
  // }
}

void get_modbus() {
  // Data Frame --> 01 04 00 01 00 02 20 0B
  // node.clearResponseBuffer();
  result = node.readInputRegisters(0x0001, 2);
  delay(500);
  // result = node.readInputRegisters(0x01, 2);
  if (result == node.ku8MBSuccess) {
    temp = node.getResponseBuffer(0) / 10.0f;
    hum = node.getResponseBuffer(1) / 10.0f;

    // Serial.print("Temp: ");
    // Serial.print(temp);
    // Serial.print("\t");
    // Serial.print("Hum: ");
    // Serial.print(hum);
    // Serial.println();

    node.clearResponseBuffer();
    node.clearTransmitBuffer();

    delay(300);
  }
}

void getDateTime() {
  DateTime now = rtc.now();

  now_hour = now.hour();
  now_minute = now.minute();
  now_second = now.second();

  //daysOfTheWeek[now.dayOfTheWeek()] + "," +
  date_now = (String)now.day() + "-" + monthsOfYear[now.month() - 1] + "-" + now.year();
  time_now = (String)now.hour() + ":" + now.minute() + ":" + now.second();
}