/*
  28BYJ-48 + ULN2003 Test
  Board: ESP32-S3 WROOM

  ESP32 -> ULN2003
  IN1 -> GPIO 8
  IN2 -> GPIO 9
  IN3 -> GPIO 10
  IN4 -> GPIO 11

  Power:
  - ULN2003 VCC -> external 5V supply
  - ULN2003 GND -> external supply GND
  - ESP32 GND must be connected to ULN2003 GND (common ground)
*/

#include <AccelStepper.h>

const uint8_t IN1 = 8;
const uint8_t IN2 = 9;
const uint8_t IN3 = 10;
const uint8_t IN4 = 11;

AccelStepper motor(AccelStepper::HALF4WIRE, IN1, IN3, IN2, IN4);

const long SWEEP = 700;

void setup() {
  Serial.begin(115200);
  delay(300);

  motor.setMaxSpeed(900);
  motor.setAcceleration(650);
  motor.setCurrentPosition(0);
  motor.moveTo(SWEEP);

  Serial.println("Stepper test started.");
}

void loop() {
  motor.run();

  if (motor.distanceToGo() == 0) {
    long nextTarget = (motor.targetPosition() > 0) ? -SWEEP : SWEEP;
    motor.moveTo(nextTarget);

    Serial.print("New target: ");
    Serial.println(nextTarget);
  }
}
