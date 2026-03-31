/*
  Web Dashboard Simulation Test (ESP32-S3 WROOM)

  Purpose:
  - Demonstrate the race web dashboard UI without IR sensors or steppers.
  - Simulate ready/running/complete race states with 5-second steps.

  Behavior:
  - ESP32 starts a Wi-Fi SoftAP and hosts a dashboard.
  - Race starts ONLY when Reset Race is clicked.
  - While running, places are simulated every 5 seconds.
*/

#include <WiFi.h>
#include <WebServer.h>

static const uint8_t LANE_COUNT = 3;

const char* DASHBOARD_AP_SSID = "RaceDashSim-S3";
const char* DASHBOARD_AP_PASS = "racetrack123"; // >= 8 chars

const unsigned long SIM_STATE_STEP_MS = 5000;

WebServer server(80);

enum SimRaceState {
  SIM_READY,
  SIM_RUNNING,
  SIM_COMPLETE
};

SimRaceState simState = SIM_READY;

uint8_t finishOrder[LANE_COUNT] = {0, 1, 2};
float finishDeltaMs[LANE_COUNT] = {0.0f, 1.2f, 2.5f};
uint8_t finishCount = 0;
unsigned long stateStartMs = 0;
uint32_t raceNumber = 0;

void generateRacePattern() {
  uint8_t pattern = raceNumber % 6;
  switch (pattern) {
    case 0:
      finishOrder[0] = 0; finishOrder[1] = 1; finishOrder[2] = 2;
      break;
    case 1:
      finishOrder[0] = 0; finishOrder[1] = 2; finishOrder[2] = 1;
      break;
    case 2:
      finishOrder[0] = 1; finishOrder[1] = 0; finishOrder[2] = 2;
      break;
    case 3:
      finishOrder[0] = 1; finishOrder[1] = 2; finishOrder[2] = 0;
      break;
    case 4:
      finishOrder[0] = 2; finishOrder[1] = 0; finishOrder[2] = 1;
      break;
    default:
      finishOrder[0] = 2; finishOrder[1] = 1; finishOrder[2] = 0;
      break;
  }

  finishDeltaMs[0] = 0.0f;
  finishDeltaMs[1] = 0.6f + ((raceNumber * 137) % 1800) / 1000.0f;
  finishDeltaMs[2] = finishDeltaMs[1] + 0.4f + ((raceNumber * 97) % 2200) / 1000.0f;
}

const char* stateText() {
  if (simState == SIM_READY) return "ready";
  if (simState == SIM_RUNNING) return "running";
  return "complete";
}

void startNextRace() {
  generateRacePattern();
  raceNumber++;
  finishCount = 0;
  simState = SIM_RUNNING;
  stateStartMs = millis();

  Serial.print("Simulation started for race #");
  Serial.println(raceNumber);
}

void updateSimulation() {
  if (simState != SIM_RUNNING) return;

  unsigned long elapsed = millis() - stateStartMs;
  if (elapsed >= 3 * SIM_STATE_STEP_MS) {
    finishCount = 3;
    simState = SIM_COMPLETE;
    Serial.println("Simulation reached COMPLETE state");
    return;
  }

  if (elapsed >= 2 * SIM_STATE_STEP_MS) {
    finishCount = 2;
  } else if (elapsed >= SIM_STATE_STEP_MS) {
    finishCount = 1;
  }
}

void handleRoot() {
  String html =
      "<!doctype html><html><head><meta charset='utf-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<title>Dashboard Simulation</title>"
      "<style>"
      ":root{--text:#e5e7eb;--muted:#9ca3af;}"
      "*{box-sizing:border-box}body{margin:0;font-family:Inter,Segoe UI,Arial,sans-serif;"
      "background:radial-gradient(circle at top,#1e293b 0%,#0b1020 45%,#050814 100%);color:var(--text);min-height:100vh;padding:24px;}"
      ".wrap{max-width:760px;margin:0 auto}.card{background:linear-gradient(160deg,#111827,#0b1224);border:1px solid #334155;"
      "border-radius:16px;padding:20px;box-shadow:0 16px 40px rgba(0,0,0,.35)}"
      "h2{margin:0 0 10px}.sub{margin:0 0 14px;color:var(--muted)}"
      ".meta{display:flex;gap:14px;flex-wrap:wrap;color:#cbd5e1;font-size:14px}"
      "ol{padding-left:0;list-style:none;margin:16px 0 0;display:grid;gap:10px}"
      ".row{padding:12px 14px;border-radius:12px;background:#0f172a;border:1px solid #334155;display:flex;justify-content:space-between;align-items:center}"
      ".place{font-weight:700;color:#93c5fd}.track{font-weight:600}.delta{color:var(--muted);font-size:13px}"
      ".winner{border-color:#16a34a;background:linear-gradient(90deg,rgba(34,197,94,.22),rgba(16,185,129,.12));animation:flashWin .9s ease-in-out infinite alternate}"
      "@keyframes flashWin{from{box-shadow:0 0 0 rgba(34,197,94,.15)}to{box-shadow:0 0 22px rgba(34,197,94,.65)}}"
      "button{margin-top:16px;padding:10px 14px;border-radius:10px;border:none;background:linear-gradient(90deg,#0ea5e9,#06b6d4);color:#082f49;font-weight:700;cursor:pointer}"
      "</style></head><body><div class='wrap'>"
      "<div class='card'><h2>Race Dashboard Simulation (ESP32-S3)</h2>"
      "<p class='sub'>Each state advances every 5 seconds after you press Reset Race.</p>"
      "<div class='meta'><span><strong>State:</strong> <span id='state'>ready</span></span>"
      "<span><strong>Race #</strong> <span id='race_no'>0</span></span></div>"
      "<ol id='results'><li class='row'><span class='track'>Press Reset Race to start</span></li></ol>"
      "<button onclick='resetRace()'>Reset Race</button>"
      "</div>"
      "<script>"
      "async function refresh(){"
      "const res=await fetch('/api/results');"
      "const data=await res.json();"
      "document.getElementById('race_no').textContent=data.race_number;"
      "document.getElementById('state').textContent=data.state;"
      "const list=document.getElementById('results');list.innerHTML='';"
      "if(data.results.length===0){list.innerHTML=\"<li class='row'><span class='track'>Press Reset Race to start</span></li>\";return;}"
      "for(const row of data.results){"
      "const li=document.createElement('li');"
      "li.className='row'+(row.place===1?' winner':'');"
      "li.innerHTML=`<span><span class='place'>#${row.place}</span> <span class='track'>🏎️ Track ${row.track}</span></span>` +"
      "(row.place===1?\"<span class='delta'>Winner</span>\":`<span class='delta'>+${row.delta_ms} ms</span>`);"
      "list.appendChild(li);"
      "}"
      "}"
      "async function resetRace(){await fetch('/api/reset',{method:'POST'});setTimeout(refresh,120);}"
      "refresh();setInterval(refresh,500);"
      "</script></div></body></html>";

  server.send(200, "text/html", html);
}

void handleApiResults() {
  String json = "{\"race_number\":" + String(raceNumber) + ",\"state\":\"" + String(stateText()) + "\",\"results\":[";
  for (uint8_t i = 0; i < finishCount; i++) {
    if (i > 0) json += ",";
    char trackLabel = 'A' + finishOrder[i];
    json += "{\"place\":" + String(i + 1) + ",\"track\":\"" + String(trackLabel) + "\",\"delta_ms\":\"" + String(finishDeltaMs[i], 3) + "\"}";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void handleApiReset() {
  startNextRace();
  server.send(200, "application/json", "{\"ok\":true}");
}

void setup() {
  Serial.begin(115200);
  delay(400);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(DASHBOARD_AP_SSID, DASHBOARD_AP_PASS);

  IPAddress ip = WiFi.softAPIP();
  Serial.println("Web Dashboard Simulation Test (ESP32-S3)");
  Serial.print("SSID: ");
  Serial.println(DASHBOARD_AP_SSID);
  Serial.print("URL: http://");
  Serial.println(ip);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/results", HTTP_GET, handleApiResults);
  server.on("/api/reset", HTTP_POST, handleApiReset);
  server.begin();

  finishCount = 0;
  simState = SIM_READY;
  raceNumber = 0;
}

void loop() {
  server.handleClient();
  updateSimulation();
}
