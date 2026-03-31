/*
  Race Track Finish Line System (3 lanes)
  Board: ESP32-S3 WROOM (Arduino IDE)

  Features:
  - Reads 3 IR break-beam receivers to detect finish-line crossing.
  - Uses GPIO interrupts + microsecond timestamps for near-tie accuracy.
  - Stores finish order (1st, 2nd, 3rd).
  - Starts winner flag animation on the lane that finished first.
  - Optional physical reset button.
  - Wi-Fi dashboard with live auto-refresh race status.
  - Uses one 28BYJ-48 stepper + ULN2003 driver per lane (3 total).

  Notes:
  - This sketch uses AccelStepper for smooth non-blocking movement.
  - IR sensors are wired active-LOW in this design:
      beam present  => input HIGH
      beam broken   => input LOW
*/

#include <AccelStepper.h>
#include <WiFi.h>
#include <WebServer.h>

// ------------------------- User Configuration -------------------------
static const uint8_t LANE_COUNT = 3;

// IR receiver pins (digital input): Lane 1..3
const uint8_t IR_PINS[LANE_COUNT] = {4, 5, 6};

// Optional reset button (to GND), INPUT_PULLUP enabled.
const uint8_t RESET_BUTTON_PIN = 7;
const unsigned long RESET_BUTTON_DEBOUNCE_MS = 40;

// Wi-Fi access point config for local dashboard.
const char* DASHBOARD_AP_SSID = "RaceTrackSensor";
const char* DASHBOARD_AP_PASS = "racetrack123"; // min 8 chars

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

// Debounce/lockout for each lane trigger (microseconds)
const uint32_t TRIGGER_LOCKOUT_US = 300000;

// Optional: automatically reset race after this timeout once all finished (ms)
const unsigned long AUTO_RESET_DELAY_MS = 12000;
// ---------------------------------------------------------------------

AccelStepper stepperLane1(AccelStepper::HALF4WIRE, L1_IN1, L1_IN3, L1_IN2, L1_IN4);
AccelStepper stepperLane2(AccelStepper::HALF4WIRE, L2_IN1, L2_IN3, L2_IN2, L2_IN4);
AccelStepper stepperLane3(AccelStepper::HALF4WIRE, L3_IN1, L3_IN3, L3_IN2, L3_IN4);

AccelStepper* steppers[LANE_COUNT] = {&stepperLane1, &stepperLane2, &stepperLane3};

bool laneFinished[LANE_COUNT] = {false, false, false};
uint32_t laneFinishTimestampUs[LANE_COUNT] = {0, 0, 0};

uint8_t finishOrder[LANE_COUNT] = {255, 255, 255}; // holds lane index in order
uint8_t finishCount = 0;
int winnerLane = -1;

bool raceLocked = false;
unsigned long raceCompleteTimeMs = 0;

volatile bool laneTriggered[LANE_COUNT] = {false, false, false};
volatile uint32_t laneTriggerUs[LANE_COUNT] = {0, 0, 0};
volatile uint32_t laneLastIsrUs[LANE_COUNT] = {0, 0, 0};

WebServer server(80);

bool lastResetButtonState = HIGH;
unsigned long lastResetButtonChangeMs = 0;

void IRAM_ATTR onLane1Interrupt() {
  uint32_t nowUs = micros();
  if ((uint32_t)(nowUs - laneLastIsrUs[0]) > TRIGGER_LOCKOUT_US) {
    laneLastIsrUs[0] = nowUs;
    laneTriggerUs[0] = nowUs;
    laneTriggered[0] = true;
  }
}

void IRAM_ATTR onLane2Interrupt() {
  uint32_t nowUs = micros();
  if ((uint32_t)(nowUs - laneLastIsrUs[1]) > TRIGGER_LOCKOUT_US) {
    laneLastIsrUs[1] = nowUs;
    laneTriggerUs[1] = nowUs;
    laneTriggered[1] = true;
  }
}

void IRAM_ATTR onLane3Interrupt() {
  uint32_t nowUs = micros();
  if ((uint32_t)(nowUs - laneLastIsrUs[2]) > TRIGGER_LOCKOUT_US) {
    laneLastIsrUs[2] = nowUs;
    laneTriggerUs[2] = nowUs;
    laneTriggered[2] = true;
  }
}

void setupSteppers() {
  for (uint8_t i = 0; i < LANE_COUNT; i++) {
    steppers[i]->setMaxSpeed(FLAG_MAX_SPEED);
    steppers[i]->setAcceleration(FLAG_ACCEL);
    steppers[i]->setCurrentPosition(0);
    steppers[i]->moveTo(0);
  }
}

void resetRace() {
  noInterrupts();
  for (uint8_t i = 0; i < LANE_COUNT; i++) {
    laneTriggered[i] = false;
    laneTriggerUs[i] = 0;
    laneLastIsrUs[i] = 0;
  }
  interrupts();

  for (uint8_t i = 0; i < LANE_COUNT; i++) {
    laneFinished[i] = false;
    laneFinishTimestampUs[i] = 0;
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
    uint8_t lane = finishOrder[place];
    uint32_t laneUs = laneFinishTimestampUs[lane];

    Serial.print(place + 1);
    Serial.print(place == 0 ? "st" : (place == 1 ? "nd" : "rd"));
    Serial.print(": Lane ");
    Serial.print(lane + 1);

    if (place == 0) {
      Serial.print(" (+0 us)");
    } else {
      uint8_t firstLane = finishOrder[0];
      uint32_t deltaUs = laneUs - laneFinishTimestampUs[firstLane];
      Serial.print(" (+");
      Serial.print(deltaUs);
      Serial.print(" us)");
    }

    Serial.println();
  }
  Serial.println("--------------------");
}

void recordFinish(uint8_t lane, uint32_t timestampUs) {
  if (raceLocked || laneFinished[lane]) return;

  laneFinished[lane] = true;
  laneFinishTimestampUs[lane] = timestampUs;
  finishOrder[finishCount] = lane;
  finishCount++;

  Serial.print("Lane ");
  Serial.print(lane + 1);
  Serial.print(" finished at place ");
  Serial.print(finishCount);
  Serial.print(" (timestamp: ");
  Serial.print(timestampUs);
  Serial.println(" us)");

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

void processLaneTriggers() {
  if (raceLocked) return;

  bool pending[LANE_COUNT] = {false, false, false};
  uint32_t triggerUs[LANE_COUNT] = {0, 0, 0};

  noInterrupts();
  for (uint8_t lane = 0; lane < LANE_COUNT; lane++) {
    pending[lane] = laneTriggered[lane];
    triggerUs[lane] = laneTriggerUs[lane];
    laneTriggered[lane] = false;
  }
  interrupts();

  uint8_t lanesToRecord[LANE_COUNT];
  uint32_t timesToRecord[LANE_COUNT];
  uint8_t pendingCount = 0;

  for (uint8_t lane = 0; lane < LANE_COUNT; lane++) {
    if (pending[lane] && !laneFinished[lane]) {
      lanesToRecord[pendingCount] = lane;
      timesToRecord[pendingCount] = triggerUs[lane];
      pendingCount++;
    }
  }

  // Sort pending finishes by trigger timestamp (ascending).
  for (uint8_t i = 0; i < pendingCount; i++) {
    for (uint8_t j = i + 1; j < pendingCount; j++) {
      if (timesToRecord[j] < timesToRecord[i]) {
        uint32_t tmpTs = timesToRecord[i];
        timesToRecord[i] = timesToRecord[j];
        timesToRecord[j] = tmpTs;

        uint8_t tmpLane = lanesToRecord[i];
        lanesToRecord[i] = lanesToRecord[j];
        lanesToRecord[j] = tmpLane;
      }
    }
  }

  for (uint8_t i = 0; i < pendingCount; i++) {
    recordFinish(lanesToRecord[i], timesToRecord[i]);
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

String raceStateString() {
  if (raceLocked) return "complete";
  if (finishCount > 0) return "running";
  return "ready";
}

void handleRoot() {
  String html =
      "<!doctype html><html><head><meta charset='utf-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<title>Race Dashboard</title>"
      "<style>"
      ":root{--bg:#0f172a;--panel:#111827;--text:#e5e7eb;--muted:#9ca3af;--accent:#38bdf8;--winner:#22c55e;}"
      "*{box-sizing:border-box}body{margin:0;font-family:Inter,Segoe UI,Arial,sans-serif;"
      "background:radial-gradient(circle at top,#1e293b 0%,#0b1020 45%,#050814 100%);color:var(--text);min-height:100vh;padding:24px;}"
      ".wrap{max-width:760px;margin:0 auto;}"
      ".card{background:linear-gradient(160deg,#111827,#0b1224);border:1px solid #334155;border-radius:16px;padding:20px;"
      "box-shadow:0 16px 40px rgba(0,0,0,.35);}"
      "h2{margin:0 0 10px;font-weight:700}.sub{margin:0 0 16px;color:var(--muted)}"
      ".state{display:inline-block;padding:6px 10px;border-radius:999px;background:#1e293b;border:1px solid #334155;color:#cbd5e1;font-size:13px}"
      "ol{padding-left:0;list-style:none;margin:16px 0 0;display:grid;gap:10px}"
      ".row{padding:12px 14px;border-radius:12px;background:#0f172a;border:1px solid #334155;display:flex;justify-content:space-between;align-items:center}"
      ".row .place{font-weight:700;color:#93c5fd}.row .lane{font-weight:600}.row .delta{color:var(--muted);font-size:13px}"
      ".winner{border-color:#16a34a;background:linear-gradient(90deg,rgba(34,197,94,.22),rgba(16,185,129,.12));animation:flashWin .9s ease-in-out infinite alternate}"
      "@keyframes flashWin{from{box-shadow:0 0 0 rgba(34,197,94,.15)}to{box-shadow:0 0 22px rgba(34,197,94,.65)}}"
      "button{margin-top:16px;padding:10px 14px;border-radius:10px;border:none;background:linear-gradient(90deg,#0ea5e9,#06b6d4);"
      "color:#082f49;font-weight:700;cursor:pointer}"
      "</style></head><body><div class='wrap'>"
      "<div class='card'><h2>RaceTrackSensor Live Results</h2>"
      "<p class='sub'>Live updates every 500 ms</p>"
      "<p><strong>State:</strong> <span id='state' class='state'>...</span></p>"
      "<ol id='results'><li class='row'><span class='lane'>Waiting for finishes...</span></li></ol>"
      "<button onclick='resetRace()'>Reset Race</button>"
      "</div>"
      "<script>"
      "async function refresh(){"
      "const res=await fetch('/api/results');"
      "const data=await res.json();"
      "document.getElementById('state').textContent=data.state;"
      "const list=document.getElementById('results');"
      "list.innerHTML='';"
      "if(data.results.length===0){list.innerHTML=\"<li class='row'><span class='lane'>Waiting for finishes...</span></li>\";return;}"
      "for(const row of data.results){"
      "const li=document.createElement('li');"
      "li.className='row'+(row.place===1?' winner':'');"
      "li.innerHTML=`<span><span class='place'>#${row.place}</span> <span class='lane'>Lane ${row.lane}</span></span>` +"
      "(row.place===1?\"<span class='delta'>Winner</span>\":`<span class='delta'>+${row.delta_us} us</span>`);"
      "list.appendChild(li);"
      "}"
      "}"
      "async function resetRace(){await fetch('/api/reset',{method:'POST'});setTimeout(refresh,100);}"
      "refresh();setInterval(refresh,500);"
      "</script></div></body></html>";

  server.send(200, "text/html", html);
}

void handleApiResults() {
  String json = "{\"state\":\"" + raceStateString() + "\",\"results\":[";

  for (uint8_t place = 0; place < finishCount; place++) {
    uint8_t lane = finishOrder[place];
    uint32_t deltaUs = 0;
    if (place > 0) {
      uint8_t leaderLane = finishOrder[0];
      deltaUs = laneFinishTimestampUs[lane] - laneFinishTimestampUs[leaderLane];
    }

    if (place > 0) json += ",";
    json += "{\"place\":" + String(place + 1) + ",\"lane\":" + String(lane + 1) + ",\"delta_us\":" + String(deltaUs) + "}";
  }

  json += "]}";
  server.send(200, "application/json", json);
}

void handleApiReset() {
  resetRace();
  server.send(200, "application/json", "{\"ok\":true}");
}

void setupDashboard() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(DASHBOARD_AP_SSID, DASHBOARD_AP_PASS);

  IPAddress ip = WiFi.softAPIP();
  Serial.print("Dashboard AP started: ");
  Serial.println(DASHBOARD_AP_SSID);
  Serial.print("Dashboard URL: http://");
  Serial.println(ip);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/results", HTTP_GET, handleApiResults);
  server.on("/api/reset", HTTP_POST, handleApiReset);
  server.begin();
}

void checkResetButton() {
  bool current = digitalRead(RESET_BUTTON_PIN);
  unsigned long now = millis();

  if (current != lastResetButtonState) {
    lastResetButtonChangeMs = now;
    lastResetButtonState = current;
  }

  if (current == LOW && (now - lastResetButtonChangeMs) > RESET_BUTTON_DEBOUNCE_MS) {
    resetRace();
    while (digitalRead(RESET_BUTTON_PIN) == LOW) {
      delay(1);
    }
    lastResetButtonState = HIGH;
    lastResetButtonChangeMs = millis();
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\nRace Track Finish System booting...");

  for (uint8_t i = 0; i < LANE_COUNT; i++) {
    pinMode(IR_PINS[i], INPUT_PULLUP);
  }

  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(IR_PINS[0]), onLane1Interrupt, FALLING);
  attachInterrupt(digitalPinToInterrupt(IR_PINS[1]), onLane2Interrupt, FALLING);
  attachInterrupt(digitalPinToInterrupt(IR_PINS[2]), onLane3Interrupt, FALLING);

  setupSteppers();
  resetRace();
  setupDashboard();

  Serial.println("Break beam on each lane to register finish order.");
}

void loop() {
  processLaneTriggers();
  checkResetButton();
  updateWinnerFlagAnimation();
  server.handleClient();

  // Auto-reset after all 3 lanes finish and timeout expires.
  if (raceLocked && (millis() - raceCompleteTimeMs > AUTO_RESET_DELAY_MS)) {
    resetRace();
  }
}
