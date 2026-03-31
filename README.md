# RaceTrackSensor

A complete 3-lane finish-line system for toy-car racing using:
- **ESP32-S3 WROOM**
- **3x IR break-beam sensor pairs** (one pair per lane)
- **3x ULN2003 + 28BYJ-48 stepper modules** (one flag per lane)

When the first car crosses the finish line, the code identifies the winning lane and waves that lane's flag.

---

## 1) Project Structure

- `RaceTrackFinishSystem/RaceTrackFinishSystem.ino`  
  Main race system sketch (3 lanes + finish order + winner flag wave).
- `Sample test for modules/ESP32-S3-WROOM/IR_BreakBeam_Test/IR_BreakBeam_Test.ino`  
  Standalone IR sensor test sketch for ESP32-S3 WROOM.
- `Sample test for modules/ESP-C3-12F-Kit/IR_BreakBeam_Test/IR_BreakBeam_Test.ino`  
  Standalone IR sensor test sketch for ESP-C3-12F-Kit.
- `Sample test for modules/ESP32-S3-WROOM/Stepper_28BYJ48_Test/Stepper_28BYJ48_Test.ino`  
  Standalone stepper + ULN2003 test sketch for ESP32-S3 WROOM.
- `Sample test for modules/ESP-C3-12F-Kit/Stepper_28BYJ48_Test/Stepper_28BYJ48_Test.ino`  
  Standalone stepper + ULN2003 test sketch for ESP-C3-12F-Kit.

---

## 2) Bill of Materials (BOM)

1. ESP32-S3 WROOM development board (USB programmable)
2. 3x IR break-beam emitter/receiver pairs
3. 3x 28BYJ-48 5V stepper motors
4. 3x ULN2003 stepper driver boards
5. External **5V power supply** sized for all steppers (recommended >= 2A)
6. Jumper wires / breadboard / terminal blocks
7. 3x mechanical flags and linkages

---

## 3) Wiring (Main 3-Lane System)

> Important: Do **not** power stepper motors from ESP32 5V/VBUS if current is high; use a dedicated 5V supply.

## 3.1 Common wiring rules

- Connect all grounds together:
  - ESP32 GND
  - External 5V power supply GND
  - All ULN2003 GND pins
  - IR receiver GND pins
- IR receivers are read as **active-LOW** in code:
  - Beam intact = HIGH
  - Beam broken by car = LOW

## 3.2 IR break-beam wiring (3 lanes)

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

## 3.3 Stepper + ULN2003 wiring (one set per lane)

### Lane 1 stepper driver pins
- IN1 -> GPIO **8**
- IN2 -> GPIO **9**
- IN3 -> GPIO **10**
- IN4 -> GPIO **11**

### Lane 2 stepper driver pins
- IN1 -> GPIO **12**
- IN2 -> GPIO **13**
- IN3 -> GPIO **14**
- IN4 -> GPIO **15**

### Lane 3 stepper driver pins
- IN1 -> GPIO **16**
- IN2 -> GPIO **17**
- IN3 -> GPIO **18**
- IN4 -> GPIO **21**

Driver power:
- ULN2003 VCC -> external **5V**
- ULN2003 GND -> shared GND
- 28BYJ-48 motor cable -> ULN2003 motor socket

---

## 4) Design Risks / Challenges (and Mitigation)

## 4.1 Power brownouts / random resets
**Risk:** 3 steppers can draw enough current to brown out the ESP32 if improperly powered.  
**Mitigation:** Use separate 5V rail for ULN2003 boards, and keep common GND.

## 4.2 Sensor false triggers (ambient light, alignment)
**Risk:** IR receivers may trigger from sunlight or poor emitter alignment.  
**Mitigation:**
- Physically shield sensor path.
- Use lane-specific lockout/debounce (already in code).
- Keep emitter/receiver fixed and rigid.

## 4.3 28BYJ-48 speed/torque limitations
**Risk:** Small steppers can skip if flag mechanism is heavy or binds.  
**Mitigation:**
- Use lightweight flags.
- Lower max speed/acceleration in code if stalling.
- Ensure smooth mechanical linkage.

## 4.4 Wrong logic level from sensor module
**Risk:** Some break-beam modules output 5V digital signals. ESP32 GPIO is **not 5V tolerant**.  
**Mitigation:**
- Verify sensor OUT voltage.
- Use voltage divider or level shifter if OUT can exceed 3.3V.

## 4.5 Simultaneous finish edge cases
**Risk:** Two cars crossing near-simultaneously might be very close in timing.  
**Mitigation:**
- Current code polls quickly and stores order based on detection sequence.
- For ultra-high precision, move to interrupts and timestamping.

---

## 5) Arduino IDE Setup

## 5.1 Install Arduino IDE
Install Arduino IDE 2.x from Arduino official website.

## 5.2 Add ESP32 board support
1. Open **Arduino IDE**.
2. Go to **File > Preferences**.
3. In **Additional Boards Manager URLs**, add:
   - `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
4. Go to **Tools > Board > Boards Manager**.
5. Search `esp32` and install **esp32 by Espressif Systems**.

## 5.3 Install required third-party library
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
3. Open: `RaceTrackFinishSystem/RaceTrackFinishSystem.ino`
4. Set board:
   - **Tools > Board > ESP32 Arduino > ESP32S3 Dev Module** (or exact board variant)
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

## 7) How Main Logic Works

1. Waits for all lanes in ready state.
2. Monitors IR sensors continuously.
3. First beam break:
   - records winner lane
   - starts that lane’s stepper flag oscillation
4. Next beam breaks:
   - records 2nd and 3rd place
5. Prints race results in Serial Monitor.
6. Auto-resets after timeout (`AUTO_RESET_DELAY_MS`).

Tunables in code:
- `FLAG_WAVE_AMPLITUDE_STEPS`
- `FLAG_MAX_SPEED`
- `FLAG_ACCEL`
- `TRIGGER_LOCKOUT_MS`
- `AUTO_RESET_DELAY_MS`

---

## 8) Module-by-Module Test Sketches

Before running full system, test each module independently.

## 8.1 IR sensor test
Open and upload one of:
- `Sample test for modules/ESP32-S3-WROOM/IR_BreakBeam_Test/IR_BreakBeam_Test.ino`
- `Sample test for modules/ESP-C3-12F-Kit/IR_BreakBeam_Test/IR_BreakBeam_Test.ino`

Expected Serial output:
- `BEAM OK` when beam is uninterrupted.
- `BEAM BROKEN` when object blocks beam.

## 8.2 Stepper + ULN2003 test
Open and upload one of:
- `Sample test for modules/ESP32-S3-WROOM/Stepper_28BYJ48_Test/Stepper_28BYJ48_Test.ino`
- `Sample test for modules/ESP-C3-12F-Kit/Stepper_28BYJ48_Test/Stepper_28BYJ48_Test.ino`

Expected behavior:
- Motor sweeps clockwise/counterclockwise continuously.
- Serial monitor prints new target positions.

---

## 9) Calibration Procedure (Recommended)

1. Upload stepper test sketch.
2. Confirm each lane motor wiring order (IN1..IN4) is correct.
3. If rotation is rough/jittery, reduce speed/acceleration.
4. Install flags and check movement range.
5. Upload IR test sketch and align emitter/receiver physically.
6. Upload main race sketch and run multi-car test.

---

## 10) Future Improvements

- Add start gate sensor + synchronized race start state.
- Save race history in NVS/EEPROM.
- Add OLED/LCD scoreboard.
- Add Wi-Fi web dashboard for live race results.
- Use interrupts + microsecond timestamps for near-tie accuracy.

