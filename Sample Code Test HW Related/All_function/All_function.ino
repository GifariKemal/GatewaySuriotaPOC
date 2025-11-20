#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <Ethernet.h>
#include <ModbusMaster.h>
#include <RTClib.h>

// MICRO SD PIN
#define SD_MOSI 10  //11
#define SD_SCK 12
#define SD_MISO 13
#define SD_CS 11  //SS //10 11

// ETHERNET PIN
#define ETH_MOSI 14
#define ETH_SCK 47
#define ETH_MISO 21
#define ETH_CS 48          //48
#define SPI_FREQ 32000000  //32000000 //20000000

#define VSPI FSPI
// SPIClass SPI2(SPI);
SPIClass SPI3(VSPI);

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};

IPAddress ip(192, 168, 1, 177);
IPAddress myDns(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 0, 0);

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
EthernetServer server(80);

#define RXD1_RS485 15
#define TXD1_RS485 16
// #define RXD2_RS485 17
// #define TXD2_RS485 18
ModbusMaster node;
uint8_t result;
float temp, hum;

#define I2C_SDA 5
#define I2C_SCL 6
RTC_DS3231 rtc;
// DateTime now = rtc.now();

// char daysOfTheWeek[7][12] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
char daysOfTheWeek[7][12] = { "AHAD ", "SENIN ", "SELASA ", "RABU ", "KAMIS ", "JUM`AT ", "SABTU " };
char monthsOfYear[12][12] = { "JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE", "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER ", "DECEMBER" };
String date_now, time_now;
int now_hour, now_minute, now_second;

#define PIN_BUTTON 0
#define DIGITAL_READ_BTN1 digitalRead(PIN_BUTTON)

// Use only core 1
#if CONFIG_FREERTOS_UNICORE
static const BaseType_t app_cpu = 0;
#else
static const BaseType_t app_cpu = 1;
#endif


void setup() {
  Serial.begin(9600);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  Serial1.begin(9600, SERIAL_8N1, RXD1_RS485, TXD1_RS485);  //SERIAL_8E1
  node.begin(1, Serial1);

  // Serial2.begin(9600, SERIAL_8N1, RXD2_RS485, TXD2_RS485);  //SERIAL_8N1,
  // node.begin(1, Serial2);

  Wire.begin(I2C_SDA, I2C_SCL);

  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  Serial.print("\nInitializing SD card...");
  // we'll use the initialization code from the utility libraries
  // since we're just testing if the card is working!
  if (!SD.begin(SD_CS)) {
    // if (!SD.begin(SPI_HALF_SPEED, SD_CS)) {
    Serial.println("initialization failed. Things to check:");
    Serial.println("* is a card inserted?");
    Serial.println("* is your wiring correct?");
    Serial.println("* did you change the chipSelect pin to match your shield or module?");
    Serial.println("Note: press reset button on the board and reopen this Serial Monitor after fixing your issue!");
    while (1) { delay(10); }
  } else {
    Serial.println("Wiring is correct and a card is present.");
  }

  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  Serial.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
  Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));

  // Ethernet
  SPI3.begin(ETH_SCK, ETH_MISO, ETH_MOSI, ETH_CS);
  // SPI3.setFrequency(SPI_FREQ);
  Ethernet.init(ETH_CS);

  // start the Ethernet connection and the server:
  // Ethernet.begin(mac, ip);

  Serial.println("Trying to get an IP address using DHCP");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // Check for Ethernet hardware present
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
      while (true) {
        delay(1);  // do nothing, no point running without Ethernet hardware
      }
    }
    if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println("Ethernet cable is not connected.");
    }
    // initialize the Ethernet device not using DHCP:
    Ethernet.begin(mac, ip, myDns, gateway, subnet);
  }

  // start the server
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());

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

  // xTaskCreatePinnedToCore(
  //   get_modbus,
  //   "Read RS485 Task",
  //   20480,
  //   NULL,
  //   1,
  //   NULL,
  //   app_cpu);
}

void loop() {
  getDateTime();
  get_modbus();

  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    Serial.println("new client");
    // an HTTP request ends with a blank line
    bool currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        if (c == '\n' && currentLineIsBlank) {
          // send a standard HTTP response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          client.println("Refresh: 1");         // refresh the page automatically
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          
          // output the value
          client.print("SRT-MGate ESP32");
          client.println("<br/><br/>");

          client.print("DateTime: ");
          client.print((String)date_now + " " + time_now);
          client.println("<br/>");

          client.print("RS-485 (Temp, Hum): ");
          client.print((String)temp + "C," + hum + "%");
          client.println("<br/>");

          client.print("Button: ");
          if (!DIGITAL_READ_BTN1) client.print(" Pressed");
          else client.print(" Not Pressed");
          client.println("<br/>");

          client.print((String) "SD Card Size: " + SD.cardSize() / (1024 * 1024) + "MB");

          client.println("<br />");
          client.println("</html>");
          break;
        }

        if (c == '\n') currentLineIsBlank = true;
        else if (c != '\r') currentLineIsBlank = false;
      }
    }
    delay(1);
    client.stop();
    //Serial.println("client disconnected");
  }
}

void get_modbus() {
  // void get_modbus(void* parameter) {
  // while (1) {
  // Data Frame --> 01 04 00 01 00 02 20 0B
  // node.clearResponseBuffer();
  result = node.readInputRegisters(0x0001, 2);
  delay(500);
  // result = node.readInputRegisters(0x01, 2);
  if (result == node.ku8MBSuccess) {
    temp = node.getResponseBuffer(0) / 10.0f;
    hum = node.getResponseBuffer(1) / 10.0f;

    Serial.print("Temp: ");
    Serial.print(temp);
    Serial.print("\t");
    Serial.print("Hum: ");
    Serial.print(hum);
    Serial.println();

    node.clearResponseBuffer();
    node.clearTransmitBuffer();

    delay(300);
  }

  //   vTaskDelay(pdMS_TO_TICKS(10));  // Beri waktu untuk task lain
  // }
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