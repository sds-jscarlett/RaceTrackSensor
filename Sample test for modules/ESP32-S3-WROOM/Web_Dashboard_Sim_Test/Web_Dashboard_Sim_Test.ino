/*
  Web Dashboard Simulation Test (ESP32-S3 WROOM)

  Purpose:
  - Demonstrate the race web dashboard UI without IR sensors or steppers.
  - Simulate a full 3-lane race result every 10 seconds.

  Behavior:
  - ESP32 starts a Wi-Fi SoftAP and hosts a dashboard.
  - Every 10 seconds, it generates a new synthetic result set.
  - Dashboard auto-refreshes every 500 ms to show changes live.
*/

#include <WiFi.h>
#include <WebServer.h>

static const uint8_t LANE_COUNT = 3;

const char* DASHBOARD_AP_SSID = "RaceDashSim-S3";
const char* DASHBOARD_AP_PASS = "racetrack123"; // >= 8 chars

const unsigned long SIM_RACE_INTERVAL_MS = 10000;

WebServer server(80);

uint8_t finishOrder[LANE_COUNT] = {0, 1, 2};
uint32_t finishDeltaUs[LANE_COUNT] = {0, 1200, 2500};
unsigned long lastSimMs = 0;
uint32_t raceNumber = 0;

void generateSimRace() {
  // Simple permutation over 3 lanes based on race number.
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

  // Deterministic synthetic near-tie-ish deltas.
  finishDeltaUs[0] = 0;
  finishDeltaUs[1] = 600 + ((raceNumber * 137) % 1800);  // 600..2399 us
  finishDeltaUs[2] = finishDeltaUs[1] + 400 + ((raceNumber * 97) % 2200);

  raceNumber++;

  Serial.print("Sim race #");
  Serial.print(raceNumber);
  Serial.print(" => 1st lane ");
  Serial.print(finishOrder[0] + 1);
  Serial.print(", 2nd lane ");
  Serial.print(finishOrder[1] + 1);
  Serial.print(", 3rd lane ");
  Serial.println(finishOrder[2] + 1);
}

void handleRoot() {
  String html =
      "<!doctype html><html><head><meta charset='utf-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<title>Dashboard Simulation</title>"
      "<style>"
      ":root{--bg:#0f172a;--panel:#111827;--text:#e5e7eb;--muted:#9ca3af;--winner:#22c55e;}"
      "*{box-sizing:border-box}body{margin:0;font-family:Inter,Segoe UI,Arial,sans-serif;"
      "background:radial-gradient(circle at top,#1e293b 0%,#0b1020 45%,#050814 100%);color:var(--text);min-height:100vh;padding:24px;}"
      ".wrap{max-width:760px;margin:0 auto}.card{background:linear-gradient(160deg,#111827,#0b1224);border:1px solid #334155;"
      "border-radius:16px;padding:20px;box-shadow:0 16px 40px rgba(0,0,0,.35)}"
      "h2{margin:0 0 10px}.sub{margin:0 0 14px;color:var(--muted)}"
      ".meta{display:flex;gap:14px;flex-wrap:wrap;color:#cbd5e1;font-size:14px}"
      "ol{padding-left:0;list-style:none;margin:16px 0 0;display:grid;gap:10px}"
      ".row{padding:12px 14px;border-radius:12px;background:#0f172a;border:1px solid #334155;display:flex;justify-content:space-between;align-items:center}"
      ".place{font-weight:700;color:#93c5fd}.lane{font-weight:600}.delta{color:var(--muted);font-size:13px}"
      ".winner{border-color:#16a34a;background:linear-gradient(90deg,rgba(34,197,94,.22),rgba(16,185,129,.12));animation:flashWin .9s ease-in-out infinite alternate}"
      "@keyframes flashWin{from{box-shadow:0 0 0 rgba(34,197,94,.15)}to{box-shadow:0 0 22px rgba(34,197,94,.65)}}"
      "</style></head><body><div class='wrap'>"
      "<div class='card'><h2>Race Dashboard Simulation (ESP32-S3)</h2>"
      "<p class='sub'>Synthetic race reruns every 10 seconds.</p>"
      "<div class='meta'><span><strong>State:</strong> complete (simulated)</span>"
      "<span><strong>Race #</strong> <span id='race_no'>...</span></span></div>"
      "<ol id='results'><li class='row'><span class='lane'>Loading...</span></li></ol>"
      "</div>"
      "<script>"
      "async function refresh(){"
      "const res=await fetch('/api/results');"
      "const data=await res.json();"
      "document.getElementById('race_no').textContent=data.race_number;"
      "const list=document.getElementById('results');list.innerHTML='';"
      "for(const row of data.results){"
      "const li=document.createElement('li');"
      "li.className='row'+(row.place===1?' winner':'');"
      "li.innerHTML=`<span><span class='place'>#${row.place}</span> <span class='lane'>Lane ${row.lane}</span></span>` +"
      "(row.place===1?\"<span class='delta'>Winner</span>\":`<span class='delta'>+${row.delta_us} us</span>`);"
      "list.appendChild(li);"
      "}"
      "}"
      "refresh();setInterval(refresh,500);"
      "</script></div></body></html>";

  server.send(200, "text/html", html);
}

void handleApiResults() {
  String json = "{\"race_number\":" + String(raceNumber) + ",\"results\":[";
  for (uint8_t i = 0; i < LANE_COUNT; i++) {
    if (i > 0) json += ",";
    json += "{\"place\":" + String(i + 1) + ",\"lane\":" + String(finishOrder[i] + 1) + ",\"delta_us\":" + String(finishDeltaUs[i]) + "}";
  }
  json += "]}";
  server.send(200, "application/json", json);
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
  server.begin();

  generateSimRace();
  lastSimMs = millis();
}

void loop() {
  server.handleClient();

  unsigned long now = millis();
  if (now - lastSimMs >= SIM_RACE_INTERVAL_MS) {
    generateSimRace();
    lastSimMs = now;
  }
}
