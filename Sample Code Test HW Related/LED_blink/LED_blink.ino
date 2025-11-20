#define LED_STATUS 7
#define LED_NET 8

#define DELAY 0.5  //second

void setup() {
  Serial.begin(9600);
  pinMode(LED_STATUS, OUTPUT);
  pinMode(LED_NET, OUTPUT);
  digitalWrite(LED_STATUS, LOW);
  digitalWrite(LED_NET, LOW);
}

void loop() {
  digitalWrite(LED_STATUS, HIGH);
  digitalWrite(LED_NET, LOW);
  delay(DELAY * 1000);
  digitalWrite(LED_STATUS, LOW);
  digitalWrite(LED_NET, HIGH);
  delay(DELAY * 1000);
}
