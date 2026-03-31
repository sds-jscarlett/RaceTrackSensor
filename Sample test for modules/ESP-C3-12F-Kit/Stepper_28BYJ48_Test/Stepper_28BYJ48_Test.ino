/*
  28BYJ-48 + ULN2003 Test
  Board: ESP-C3-12F-Kit

  ESP32-C3 -> ULN2003
  IN1 -> GPIO 5
  IN2 -> GPIO 6
  IN3 -> GPIO 7
  IN4 -> GPIO 8

  Power:
  - ULN2003 VCC -> external 5V supply
  - ULN2003 GND -> external supply GND
  - ESP32-C3 GND must be connected to ULN2003 GND (common ground)
*/

#include <AccelStepper.h>

const uint8_t IN1 = 5;
const uint8_t IN2 = 6;
const uint8_t IN3 = 7;
const uint8_t IN4 = 8;

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
