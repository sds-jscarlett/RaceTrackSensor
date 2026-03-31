# RaceTrackSensor

A complete 3-lane finish-line system for toy-car racing using:
- **ESP32-S3 WROOM**
- **3x IR break-beam sensor pairs** (one pair per lane)
- **3x ULN2003 + 28BYJ-48 stepper modules** (one flag per lane)
- **1x reset push button** (optional)

When the first car crosses the finish line, the code identifies the winning lane and waves that lane's flag. The system now also hosts a built-in Wi-Fi dashboard for live race results, auto-refreshing every 500 ms.

---

## 1) Project Structure

- `RaceTrackFinishSystem/RaceTrackFinishSystem.ino`  
  Main race system sketch (3 lanes + interrupt timing + winner flag wave + reset button + Wi-Fi dashboard).
- `RaceTrackFinishSystem/RaceTrackFinishSystem_ESP-C3-12F-Kit.ino`  
  Main race system sketch variant for ESP-C3-12F-Kit (3 lanes + interrupt timing + winner flag wave + Wi-Fi dashboard, reset via web/auto).
- `Sample test for modules/ESP32-S3-WROOM/IR_BreakBeam_Test/IR_BreakBeam_Test.ino`  
  Standalone IR sensor test sketch for ESP32-S3 WROOM.
- `Sample test for modules/ESP-C3-12F-Kit/IR_BreakBeam_Test/IR_BreakBeam_Test.ino`  
  Standalone IR sensor test sketch for ESP-C3-12F-Kit.
- `Sample test for modules/ESP32-S3-WROOM/Stepper_28BYJ48_Test/Stepper_28BYJ48_Test.ino`  
  Standalone stepper + ULN2003 test sketch for ESP32-S3 WROOM.
- `Sample test for modules/ESP-C3-12F-Kit/Stepper_28BYJ48_Test/Stepper_28BYJ48_Test.ino`  
  Standalone stepper + ULN2003 test sketch for ESP-C3-12F-Kit.
- `Sample test for modules/ESP32-S3-WROOM/Web_Dashboard_Sim_Test/Web_Dashboard_Sim_Test.ino`  
  Dashboard simulation sketch for ESP32-S3 (new simulated results every 10 seconds).
- `Sample test for modules/ESP-C3-12F-Kit/Web_Dashboard_Sim_Test/Web_Dashboard_Sim_Test.ino`  
  Dashboard simulation sketch for ESP-C3 (new simulated results every 10 seconds).

---

## 2) Bill of Materials (BOM)

1. ESP32-S3 WROOM **or** ESP-C3-12F-Kit development board (USB programmable)
2. 3x IR break-beam emitter/receiver pairs
3. 3x 28BYJ-48 5V stepper motors
4. 3x ULN2003 stepper driver boards
5. External **5V power supply** sized for all steppers (recommended >= 2A)
6. 1x momentary push button (reset race)
7. Jumper wires / breadboard / terminal blocks
8. 3x mechanical flags and linkages

---

## 3) Wiring (Main 3-Lane System)

> Important: Do **not** power stepper motors from ESP32 5V/VBUS if current is high; use a dedicated 5V supply.

### 3.1 Common wiring rules

- Connect all grounds together:
  - ESP32 GND
  - External 5V power supply GND
  - All ULN2003 GND pins
  - IR receiver GND pins
- IR receivers are read as **active-LOW** in code:
  - Beam intact = HIGH
  - Beam broken by car = LOW

### 3.2 IR break-beam wiring (3 lanes)

Each lane uses one receiver output to one GPIO:

- Lane 1 IR OUT -> GPIO **4**
- Lane 2 IR OUT -> GPIO **5**
- Lane 3 IR OUT -> GPIO **6**

Receiver power:
- Receiver VCC -> ESP32 **3.3V** (preferred if module supports it)
- Receiver GND -> GND

Emitter side:
- Usually powered by 3.3V or 5V depending on sensor specs.
- If your emitter requires 5V, keep grounds common and ensure receiver output to ESP32 is 3.3V-safe.

### 3.3 Reset button wiring (optional)

- One side of push button -> GPIO **7**
- Other side -> GND
- Code uses `INPUT_PULLUP`, so button press reads LOW and triggers immediate reset.

### 3.4 Stepper + ULN2003 wiring (one set per lane)

#### Lane 1 stepper driver pins
- IN1 -> GPIO **8**
- IN2 -> GPIO **9**
- IN3 -> GPIO **10**
- IN4 -> GPIO **11**

#### Lane 2 stepper driver pins
- IN1 -> GPIO **12**
- IN2 -> GPIO **13**
- IN3 -> GPIO **14**
- IN4 -> GPIO **15**

#### Lane 3 stepper driver pins
- IN1 -> GPIO **16**
- IN2 -> GPIO **17**
- IN3 -> GPIO **18**
- IN4 -> GPIO **21**

Driver power:
- ULN2003 VCC -> external **5V**
- ULN2003 GND -> shared GND
- 28BYJ-48 motor cable -> ULN2003 motor socket

### 3.5 ESP-C3-12F-Kit pin map (main race sketch variant)

For `RaceTrackFinishSystem_ESP-C3-12F-Kit.ino`, use this mapping:

- IR receivers:
  - Lane 1 IR OUT -> GPIO **0**
  - Lane 2 IR OUT -> GPIO **1**
  - Lane 3 IR OUT -> GPIO **2**

- Lane 1 ULN2003:
  - IN1 -> GPIO **3**
  - IN2 -> GPIO **4**
  - IN3 -> GPIO **5**
  - IN4 -> GPIO **6**

- Lane 2 ULN2003:
  - IN1 -> GPIO **7**
  - IN2 -> GPIO **8**
  - IN3 -> GPIO **9**
  - IN4 -> GPIO **10**

- Lane 3 ULN2003:
  - IN1 -> GPIO **18**
  - IN2 -> GPIO **19**
  - IN3 -> GPIO **20**
  - IN4 -> GPIO **21**

Note for ESP-C3 variant:
- The physical reset button is disabled in this mapping to preserve GPIOs for all 3 stepper lanes.
- Use the web dashboard reset button or automatic reset timeout.

---

## 4) Design Risks / Challenges (and Mitigation)

### 4.1 Power brownouts / random resets
**Risk:** 3 steppers can draw enough current to brown out the ESP32 if improperly powered.  
**Mitigation:** Use separate 5V rail for ULN2003 boards, and keep common GND.

### 4.2 Sensor false triggers (ambient light, alignment)
**Risk:** IR receivers may trigger from sunlight or poor emitter alignment.  
**Mitigation:**
- Physically shield sensor path.
- ISR lockout/debounce is already in code (`TRIGGER_LOCKOUT_US`).
- Keep emitter/receiver fixed and rigid.

### 4.3 28BYJ-48 speed/torque limitations
**Risk:** Small steppers can skip if flag mechanism is heavy or binds.  
**Mitigation:**
- Use lightweight flags.
- Lower max speed/acceleration in code if stalling.
- Ensure smooth mechanical linkage.

### 4.4 Wrong logic level from sensor module
**Risk:** Some break-beam modules output 5V digital signals. ESP32 GPIO is **not 5V tolerant**.  
**Mitigation:**
- Verify sensor OUT voltage.
- Use voltage divider or level shifter if OUT can exceed 3.3V.

### 4.5 Simultaneous finish edge cases
**Risk:** Two cars crossing near-simultaneously might be very close in timing.  
**Mitigation:**
- Race lanes are timestamped with `micros()` inside GPIO interrupts.
- Pending finishes are sorted by timestamp before assigning places.

---

## 5) Arduino IDE Setup

### 5.1 Install Arduino IDE
Install Arduino IDE 2.x from Arduino official website.

### 5.2 Add ESP32 board support
1. Open **Arduino IDE**.
2. Go to **File > Preferences**.
3. In **Additional Boards Manager URLs**, add:
   - `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
4. Go to **Tools > Board > Boards Manager**.
5. Search `esp32` and install **esp32 by Espressif Systems**.

### 5.3 Install required third-party library
This project requires:
- **AccelStepper** by Mike McCauley

Install steps:
1. Go to **Sketch > Include Library > Manage Libraries...**
2. Search `AccelStepper`.
3. Install latest stable version.

---

## 6) Upload Main Sketch (Race System)

1. Connect ESP32-S3 board via USB.
2. Open Arduino IDE.
3. Open one of:
   - `RaceTrackFinishSystem/RaceTrackFinishSystem.ino` (ESP32-S3)
   - `RaceTrackFinishSystem/RaceTrackFinishSystem_ESP-C3-12F-Kit.ino` (ESP-C3)
4. Set board:
   - For S3 sketch: **Tools > Board > ESP32 Arduino > ESP32S3 Dev Module**
   - For C3 sketch: **Tools > Board > ESP32 Arduino > ESP32C3 Dev Module** (or your specific ESP-C3-12F entry)
5. Set port:
   - **Tools > Port > [your ESP32 COM/tty port]**
6. (Optional recommended settings)
   - USB CDC On Boot: Enabled
   - Upload Speed: 921600 (or lower if unstable)
7. Click **Verify**.
8. Click **Upload**.
9. Open **Serial Monitor** at **115200 baud**.

If upload fails:
- Hold **BOOT** while upload starts, then release when “Connecting...” appears.
- Ensure a data-capable USB cable.

---

## 7) Live Wi-Fi Dashboard

The main sketch starts an ESP32 SoftAP for local live results.

- SSID: `RaceTrackSensor`
- Password: `racetrack123`
- URL: `http://192.168.4.1` (default SoftAP address)

Dashboard features:
- Auto-refreshes every 500 ms.
- Shows current race state (`ready`, `running`, `complete`).
- Displays ordered finish results and millisecond deltas from the leader.
- Highlights the winner row with a flashing visual style for quick readability.
- Includes a **Reset Race** button (web-triggered).

You can change SSID/password in:
- `DASHBOARD_AP_SSID`
- `DASHBOARD_AP_PASS`

---

## 8) How Main Logic Works

1. Waits for all lanes in ready state.
2. Listens for each lane with GPIO **interrupts** on falling edge (beam break).
3. ISR captures lane timestamp with `micros()` and stores pending trigger.
4. Main loop sorts pending triggers by timestamp for precise place ordering.
5. First finish:
   - records winner lane
   - starts winner lane stepper flag oscillation
6. Next finishes:
   - records 2nd and 3rd places with millisecond deltas shown in UI
7. Race can be reset in three ways:
   - hardware button press
   - web dashboard reset button
   - automatic timeout (`AUTO_RESET_DELAY_MS`)

Tunables in code:
- `FLAG_WAVE_AMPLITUDE_STEPS`
- `FLAG_MAX_SPEED`
- `FLAG_ACCEL`
- `TRIGGER_LOCKOUT_US`
- `AUTO_RESET_DELAY_MS`

---

## 9) Module-by-Module Test Sketches

Before running full system, test each module independently.

### 9.1 IR sensor test
Open and upload one of:
- `Sample test for modules/ESP32-S3-WROOM/IR_BreakBeam_Test/IR_BreakBeam_Test.ino`
- `Sample test for modules/ESP-C3-12F-Kit/IR_BreakBeam_Test/IR_BreakBeam_Test.ino`

Expected Serial output:
- `BEAM OK` when beam is uninterrupted.
- `BEAM BROKEN` when object blocks beam.

### 9.2 Stepper + ULN2003 test
Open and upload one of:
- `Sample test for modules/ESP32-S3-WROOM/Stepper_28BYJ48_Test/Stepper_28BYJ48_Test.ino`
- `Sample test for modules/ESP-C3-12F-Kit/Stepper_28BYJ48_Test/Stepper_28BYJ48_Test.ino`

Expected behavior:
- Motor sweeps clockwise/counterclockwise continuously.
- Serial monitor prints new target positions.

### 9.3 Web dashboard simulation test
Open and upload one of:
- `Sample test for modules/ESP32-S3-WROOM/Web_Dashboard_Sim_Test/Web_Dashboard_Sim_Test.ino`
- `Sample test for modules/ESP-C3-12F-Kit/Web_Dashboard_Sim_Test/Web_Dashboard_Sim_Test.ino`

Expected behavior:
- Board creates a Wi-Fi AP and prints dashboard URL in Serial Monitor.
- Browser dashboard auto-refreshes every 500 ms.
- Press **Reset Race** to start a new simulated race.
- Simulation walks through states every 5 seconds (`ready` -> `running` -> `complete`).

---

## 10) Calibration Procedure (Recommended)

1. Upload stepper test sketch.
2. Confirm each lane motor wiring order (IN1..IN4) is correct.
3. If rotation is rough/jittery, reduce speed/acceleration.
4. Install flags and check movement range.
5. Upload IR test sketch and align emitter/receiver physically.
6. Upload main race sketch and run multi-car test.
