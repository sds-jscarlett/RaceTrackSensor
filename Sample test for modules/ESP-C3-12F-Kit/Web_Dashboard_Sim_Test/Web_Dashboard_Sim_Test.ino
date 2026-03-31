/*
  Web Dashboard Simulation Test (ESP-C3-12F Kit)

  Purpose:
  - Demonstrate the race web dashboard UI without IR sensors or steppers.
  - Simulate a full 3-lane race result every 10 seconds.

  Behavior:
  - ESP starts a Wi-Fi SoftAP and hosts a dashboard.
  - Every 10 seconds, it generates a new synthetic result set.
  - Dashboard auto-refreshes every 500 ms to show changes live.
*/

#include <WiFi.h>
#include <WebServer.h>

static const uint8_t LANE_COUNT = 3;

const char* DASHBOARD_AP_SSID = "RaceDashSim-C3";
const char* DASHBOARD_AP_PASS = "racetrack123"; // >= 8 chars

const unsigned long SIM_RACE_INTERVAL_MS = 10000;

WebServer server(80);

uint8_t finishOrder[LANE_COUNT] = {0, 1, 2};
uint32_t finishDeltaUs[LANE_COUNT] = {0, 1200, 2500};
unsigned long lastSimMs = 0;
uint32_t raceNumber = 0;

void generateSimRace() {
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

  finishDeltaUs[0] = 0;
  finishDeltaUs[1] = 600 + ((raceNumber * 137) % 1800);
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
      "<style>body{font-family:Arial,sans-serif;margin:24px;}"
      ".card{max-width:620px;padding:16px;border:1px solid #ccc;border-radius:8px;}"
      "small{color:#666;}</style></head><body>"
      "<div class='card'><h2>Race Dashboard Simulation (ESP-C3)</h2>"
      "<p><strong>State:</strong> complete (simulated)</p>"
      "<p><small>New synthetic race every 10 seconds.</small></p>"
      "<p><strong>Race #</strong> <span id='race_no'>...</span></p>"
      "<ol id='results'><li>Loading...</li></ol>"
      "</div>"
      "<script>"
      "async function refresh(){"
      "const res=await fetch('/api/results');"
      "const data=await res.json();"
      "document.getElementById('race_no').textContent=data.race_number;"
      "const list=document.getElementById('results');list.innerHTML='';"
      "for(const row of data.results){"
      "const li=document.createElement('li');"
      "li.textContent = row.place===1 ? `Lane ${row.lane} (leader)` : `Lane ${row.lane} (+${row.delta_us} us)`;"
      "list.appendChild(li);"
      "}"
      "}"
      "refresh();setInterval(refresh,500);"
      "</script></body></html>";

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
  Serial.println("Web Dashboard Simulation Test (ESP-C3)");
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
