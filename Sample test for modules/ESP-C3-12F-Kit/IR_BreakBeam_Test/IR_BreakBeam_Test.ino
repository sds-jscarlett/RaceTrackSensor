/*
  IR Break Beam Test (single channel)
  Board: ESP-C3-12F-Kit

  Wiring (receiver side):
  - VCC -> 3.3V
  - GND -> GND
  - OUT -> GPIO 4 (or update IR_PIN)

  Expected behavior with INPUT_PULLUP:
  - Beam present  => HIGH
  - Beam broken   => LOW
*/

const uint8_t IR_PIN = 4;
unsigned long lastPrint = 0;

void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(IR_PIN, INPUT_PULLUP);
  Serial.println("IR Break Beam test started.");
}

void loop() {
  int state = digitalRead(IR_PIN);

  if (millis() - lastPrint > 200) {
    lastPrint = millis();
    Serial.print("IR state: ");
    Serial.print(state == LOW ? "BEAM BROKEN" : "BEAM OK");
    Serial.print("  (raw=");
    Serial.print(state);
    Serial.println(")");
  }
}
