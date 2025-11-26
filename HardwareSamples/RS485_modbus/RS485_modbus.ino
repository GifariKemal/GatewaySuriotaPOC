
#include <ModbusMaster.h>

#define RXD1_RS485 15
#define TXD1_RS485 16
// #define RXD2_RS485 17
// #define TXD2_RS485 18

ModbusMaster node;

uint8_t result;
float temp, hum;

void setup() {
  Serial.begin(9600);

  Serial1.begin(9600, SERIAL_8N1, RXD1_RS485, TXD1_RS485);  //SERIAL_8E1
  node.begin(1, Serial1);

  // Serial2.begin(9600, SERIAL_8N1, RXD2_RS485, TXD2_RS485);  //SERIAL_8N1,
  // node.begin(1, Serial2);
}

void loop() {

  // Data Frame --> 01 04 00 01 00 02 20 0B
  // node.clearResponseBuffer();
  result = node.readInputRegisters(0x0001, 2);
  delay(1);
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

    delay(500);
  }
}
