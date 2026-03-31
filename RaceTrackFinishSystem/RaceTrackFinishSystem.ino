/*
  Race Track Finish Line System (3 lanes)
  Board: ESP32-S3 WROOM (Arduino IDE)

  Features:
  - Reads 3 IR break-beam receivers to detect finish-line crossing.
  - Stores finish order (1st, 2nd, 3rd).
  - Starts winner flag animation on the lane that finished first.
  - Uses one 28BYJ-48 stepper + ULN2003 driver per lane (3 total).

  Notes:
  - This sketch uses AccelStepper for smooth non-blocking movement.
  - IR sensors are wired active-LOW in this design:
      beam present  => input HIGH
      beam broken   => input LOW
*/

#include <AccelStepper.h>

// ------------------------- User Configuration -------------------------
static const uint8_t LANE_COUNT = 3;

// IR receiver pins (digital input): Lane 1..3
const uint8_t IR_PINS[LANE_COUNT] = {4, 5, 6};

// ULN2003 IN1..IN4 pins for each lane's stepper
// Lane 1 stepper pins
const uint8_t L1_IN1 = 8;
const uint8_t L1_IN2 = 9;
const uint8_t L1_IN3 = 10;
const uint8_t L1_IN4 = 11;

// Lane 2 stepper pins
const uint8_t L2_IN1 = 12;
const uint8_t L2_IN2 = 13;
const uint8_t L2_IN3 = 14;
const uint8_t L2_IN4 = 15;

// Lane 3 stepper pins
const uint8_t L3_IN1 = 16;
const uint8_t L3_IN2 = 17;
const uint8_t L3_IN3 = 18;
const uint8_t L3_IN4 = 21;

// 28BYJ-48 full revolution (typical): ~2048 steps for full-step mode.
// We use HALF4WIRE mode, so values are often handled as ~4096 micro-steps equivalent.
// For a visible flag wave, a smaller travel works better.
const long FLAG_WAVE_AMPLITUDE_STEPS = 450; // adjust mechanically
const float FLAG_MAX_SPEED = 850.0;         // steps/s
const float FLAG_ACCEL = 600.0;             // steps/s^2

// Debounce/lockout for each lane trigger (ms)
const unsigned long TRIGGER_LOCKOUT_MS = 300;

// Optional: automatically reset race after this timeout once all finished (ms)
const unsigned long AUTO_RESET_DELAY_MS = 12000;
// ---------------------------------------------------------------------

AccelStepper stepperLane1(AccelStepper::HALF4WIRE, L1_IN1, L1_IN3, L1_IN2, L1_IN4);
AccelStepper stepperLane2(AccelStepper::HALF4WIRE, L2_IN1, L2_IN3, L2_IN2, L2_IN4);
AccelStepper stepperLane3(AccelStepper::HALF4WIRE, L3_IN1, L3_IN3, L3_IN2, L3_IN4);

AccelStepper* steppers[LANE_COUNT] = {&stepperLane1, &stepperLane2, &stepperLane3};

bool laneFinished[LANE_COUNT] = {false, false, false};
unsigned long laneLastTriggerMs[LANE_COUNT] = {0, 0, 0};

uint8_t finishOrder[LANE_COUNT] = {255, 255, 255}; // holds lane index in order
uint8_t finishCount = 0;
int winnerLane = -1;

bool raceLocked = false;
unsigned long raceCompleteTimeMs = 0;

void setupSteppers() {
  for (uint8_t i = 0; i < LANE_COUNT; i++) {
    steppers[i]->setMaxSpeed(FLAG_MAX_SPEED);
    steppers[i]->setAcceleration(FLAG_ACCEL);
    steppers[i]->setCurrentPosition(0);
    steppers[i]->moveTo(0);
  }
}

void resetRace() {
  for (uint8_t i = 0; i < LANE_COUNT; i++) {
    laneFinished[i] = false;
    laneLastTriggerMs[i] = 0;
    finishOrder[i] = 255;
  }

  finishCount = 0;
  winnerLane = -1;
  raceLocked = false;
  raceCompleteTimeMs = 0;

  // Return all flags to neutral home
  for (uint8_t i = 0; i < LANE_COUNT; i++) {
    steppers[i]->moveTo(0);
  }

  Serial.println("\n=== Race Reset. Ready for next race. ===");
}

void announceResults() {
  Serial.println("\n--- Race Results ---");
  for (uint8_t place = 0; place < finishCount; place++) {
    Serial.print(place + 1);
    Serial.print(place == 0 ? "st" : (place == 1 ? "nd" : "rd"));
    Serial.print(": Lane ");
    Serial.println(finishOrder[place] + 1);
  }
  Serial.println("--------------------");
}

void recordFinish(uint8_t lane) {
  laneFinished[lane] = true;
  finishOrder[finishCount] = lane;
  finishCount++;

  Serial.print("Lane ");
  Serial.print(lane + 1);
  Serial.print(" finished at place ");
  Serial.println(finishCount);

  if (finishCount == 1) {
    winnerLane = lane;
    Serial.print("Winner detected: Lane ");
    Serial.println(winnerLane + 1);

    // Start winner flag wave from center to one side.
    steppers[winnerLane]->moveTo(FLAG_WAVE_AMPLITUDE_STEPS);
  }

  if (finishCount >= LANE_COUNT) {
    raceLocked = true;
    raceCompleteTimeMs = millis();
    announceResults();
    Serial.println("Race complete. Auto-reset timer started.");
  }
}

void checkFinishSensors() {
  if (raceLocked) return;

  unsigned long now = millis();

  for (uint8_t lane = 0; lane < LANE_COUNT; lane++) {
    if (laneFinished[lane]) continue;

    int sensorState = digitalRead(IR_PINS[lane]);
    bool beamBroken = (sensorState == LOW); // active-LOW convention

    if (beamBroken) {
      if (now - laneLastTriggerMs[lane] > TRIGGER_LOCKOUT_MS) {
        laneLastTriggerMs[lane] = now;
        recordFinish(lane);
      }
    }
  }
}

void updateWinnerFlagAnimation() {
  // Run all motors so any pending move-to-home movement is also executed.
  for (uint8_t i = 0; i < LANE_COUNT; i++) {
    steppers[i]->run();
  }

  // Oscillate winner flag continuously after first finish.
  if (winnerLane >= 0) {
    AccelStepper* winner = steppers[winnerLane];
    if (winner->distanceToGo() == 0) {
      long currentTarget = winner->targetPosition();
      if (currentTarget > 0) {
        winner->moveTo(-FLAG_WAVE_AMPLITUDE_STEPS);
      } else {
        winner->moveTo(FLAG_WAVE_AMPLITUDE_STEPS);
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\nRace Track Finish System booting...");

  for (uint8_t i = 0; i < LANE_COUNT; i++) {
    pinMode(IR_PINS[i], INPUT_PULLUP);
  }

  setupSteppers();
  resetRace();

  Serial.println("Break beam on each lane to register finish order.");
}

void loop() {
  checkFinishSensors();
  updateWinnerFlagAnimation();

  // Auto-reset after all 3 lanes finish and timeout expires.
  if (raceLocked && (millis() - raceCompleteTimeMs > AUTO_RESET_DELAY_MS)) {
    resetRace();
  }
}
