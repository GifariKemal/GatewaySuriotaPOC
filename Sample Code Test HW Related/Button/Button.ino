#include "OneButton.h"  //we need the OneButton library

#define PIN_BUTTON 0
#define DIGITAL_READ_BTN1 digitalRead(PIN_BUTTON)

OneButton button(PIN_BUTTON, true);  //attach a button on pin to the library

void setup() {
  Serial.begin(9600);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  button.attachClick(singleclick);        // link the function to be called on a singleclick event.
  button.attachDoubleClick(doubleclick);  // link the function to be called on a doubleclick event.
  button.attachLongPressStop(longclick);  // link the function to be called on a longpress event.
}

bool _singleclick, _doubleclick, _longclick;
void loop() {
  button.tick();  // check the status of the button

  //Serial.println(DIGITAL_READ_BTN1);

  if (_singleclick) Serial.println("Single click");
  else if (_doubleclick) Serial.println("Double click");
  else if (_longclick) Serial.println("Long click");

  if (DIGITAL_READ_BTN1) {
    _singleclick = false;
    _doubleclick = false;
    _longclick = false;
  }
  delay(10);  // a short wait between checking the button
}

void singleclick() {  // what happens when the button is clicked
  //  Serial.println("Single click");
  _singleclick = true;
}

void doubleclick() {  // what happens when button is double-clicked
  //  Serial.println("Double click");
  _doubleclick = true;
}


void longclick() {  // what happens when buton is long-pressed
  //  Serial.println("Long click");
  _longclick = true;
}
