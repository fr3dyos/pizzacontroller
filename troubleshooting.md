# PizzaController Troubleshooting Guide

This guide helps diagnose and resolve common issues with PizzaController (ESP32, DM556 driver, NEMA23 stepper). Follow steps by problem category, from simplest to most likely.

---

## Motor Not Moving

1. **Power and Ground**
   - Verify 24VDC between DM556 +V/GND (within driver range).
   - Check LM2596 adjusted to stable 5V for ESP32/LCD (test multimeter under load).
   - All GNDs common (supply, DM556, ESP32, LM2596).

2. **Motor Wiring**
   ```
   | Phase | Motor | DM556 |
   |-------|-------|-------|
   | A+    | Red   | Red   |
   | A-    | Black | Black |
   | B+    | Green | Blue  |
   | B-    | Yel   | White |
   ```
   - If motor vibrates but doesn't rotate, coils likely swapped. Check continuity or motor docs.

3. **Command Signals (STEP/DIR/ENABLE)**
   - GPIO 21 (ENABLE) must be LOW to enable DM556 (active low).
   - Confirm firmware uses correct pins (18=STEP, 19=DIR) and FastAccelStepper initialized.
   - Quick test: LED + resistor STEP to GND to see pulses on `JOG F 100`.

4. **DM556 DIP Switches (1/8 microstep, 4.01A, 1600ppr)**
   ```
   Current: SW1 OFF, SW2 ON, SW3 OFF, SW4 ON
   Steps:   SW5 OFF, SW6 OFF, SW7 ON, SW8 ON
   ```
   - Current too low → low torque, motor "sings" without rotating.
   - Current too high → excessive heat (see Overheating).

5. **Firmware Running**
   - Serial Monitor (115200): After reset, expect:
     - `========================================`
     - `Pizza Controller - LOEM PUC-Rio`
     - `========================================`
     - Motor configuration block (steps, speed, accel, hold, home direction, saved positions)
     - `Serial Commands` help block
     - `System ready!`
   - The sketch also runs a homing calibration on boot (`findHomeDirection(1)` after the splash) — if homing doesn't complete, check the home sensor and `HOME_DIRECTION`.
   - No messages → check USB data cable, COM port, board selection.

6. **Brown-out/Reset During Movement**
   - ESP32 reboots on start → voltage drop (undersized PSU) or poor GND.
   - Test `SET_MAX_SPEED`/`SET_ACCELERATION` lower, verify 24V/5A supply.

---

## Direct/Keypad/E-Stop Buttons Not Responding

1. **Pins and Power**
   - GPIO 34: Direct 7-pos buttons (analog, 3.3V VCC, common GND). Keys 1-5 = position slots 0-4; keys 6/7 = CCW/CW jog toward the selected slot.
   - GPIO 35: Navigation keypad (analog, 3.3V, 5 keys).
   - GPIO 33: E-Stop (digital INPUT_PULLUP, active LOW).
   - Measure voltage on each GPIO to GND per button; levels must differ (resistor ladder).
   - **GPIO 32 is NOT a button input** — it's a digital output for the home-status LED (mirrors `GET_SWITCH`).

2. **Code Analog Thresholds**
   ```
   KEYPAD:  220/800/1400/2300/3600          (5 keys, GPIO 35)
   DIRECT:  130/570/1170/1740/2370/3100/3700 (7 keys, GPIO 34)
   HOME:    <1500                           (GPIO 27, A3144 hall)
   ```
   - If ADC readings (Serial) don't match these intervals, adjust the threshold constants at the top of the sketch.

3. **Serial Test**
   - Press a direct-button → Serial: `Position slot N` (for keys 1-5) or `Going to saved position ... D: CW/CCW` (for keys 6/7).
   - Press a navigation key → Serial: navigates the menu / changes `inputValue` (no debug line unless you add one).
   - Confirm slots 0-4 load saved positions on press.

---

## LCD Not Displaying

1. **I2C Wiring (single LCD)**
   - **LCD (main, 0x27):** SDA GPIO 25, SCL GPIO 26 (Wire/I2C1)
   - VCC → 5V LM2596, GND common
   - Add 4.7k pullups to 3.3V on SDA/SCL if the display is unstable
   - Check no swaps, common GND
   - The current firmware initializes **only one** LCD on the default Wire bus (address 0x27). GPIO 22/23 are unused.

2. **I2C Address**
   - Code default: `0x27`.
   - Some modules use `0x3F`; run an ESP32 I2C scanner to find the actual address and edit `#define LCD_ADDR` at the top of the sketch.

3. **Initialization**
   - Serial shows the boot banner but LCD is blank → likely hardware (power/I2C) or wrong address.
   - Adjust the `LiquidCrystal_I2C` constructor address/init per module.

---

## Home Sensor Not Working

1. **A3144 Hall Connections**
   - Signal → GPIO 27.
   - VCC → 3.3V, GND common.
   - Check polarity (some modules have hard-to-read silkscreen).

2. **Reading and Threshold**
   - HOME threshold: ADC `<1500` = triggered (per code).
   - Test `GET_SWITCH` Serial: Observe value/state with/without magnet near sensor.

3. **FIND_HOME Routine**
   - `FIND_HOME` moves until triggered, 50ms debounce.
   - Motor passes sensor:
     - Check homing direction (`SET_HOME_DIR`).
     - Check magnet polarity.
     - Lower homing speed (`SET_SPEED`) for precision.

---

## Serial Commands Not Working

1. **Serial Monitor Setup**
   - 115200 baud, 8N1, no flow control.
   - Commands **UPPERCASE**, ended with Enter (CR/LF per settings).

2. **Basic Test Commands**
   - `HELP` → full command list.
   - `JOG F 1000`, `JOG B 1000` → quick movement test.
   - `SET_MAX_SPEED 10000`, `SET_ACCELERATION 5000` → adjust dynamics.
   - `GET_INFO` → shows steps, speed, accel, hold, saved positions.

3. **No Terminal Echo**
   - Nothing on typing:
     - Verify correct COM port.
     - Use USB data cable (not charge-only).
     - Close other apps using port.

---

## Upload/Compilation Failure

1. **Required Libraries**
   - Installed: `FastAccelStepper`, `Preferences`, `Wire`, `LiquidCrystal_I2C`.
   - Avoid multiple versions in Arduino folders.

2. **Board/Port Selection**
   - Board: "ESP32 Dev Module" (or compatible).
   - Port: **Tools > Port**.

3. **Common Upload Issues**
   - Port missing: USB driver or charge-only cable.
   - Connection errors:
     - Try different USB cable/port/PC hub.
     - Hold BOOT/EN during upload start.

---

## Overheating

1. **Motor/Driver**
   - DM556 4.01A may be high for some NEMA23; check motor rated current.
   - Lower DIP current if too hot to touch (shouldn't burn hand seconds).

2. **Operating Conditions**
   - Holding position generates heat.
   - `SET_HOLD_TIME` reduces energized time post-move.
   - Ensure DM556/motor ventilation (heatsinks/fan if needed).

3. **Power Supply**
   - Undersized PSU overheats, causes voltage sag.
   - Use quality 24V ≥5A supply.

---

## Position Not Persisting

1. **NVM Storage (Preferences)**
   - After `SAVE_POS <slot>`, reboot ESP32, `GET_INFO` to verify reload.
   - Check `Preferences` namespace unchanged (resets old data).

2. **Recommended Procedure**
   - Always home (`HOME`/`FIND_HOME`) after power-on before trusting saved positions.
   - Home first, move to desired, then `SAVE_POS`.

---

## LCD Menu Freezing

1. **Debounce and ADC Noise**
   - Menu 300ms debounce (`lastKeyTime`) prevents double-clicks.
   - Noisy ADC causes false clicks or stuck states.

---

## Recommended Quick Tests

```
Serial:
GET_POS
GET_INFO
GET_SWITCH
JOG F 50        (small movement test)
FIND_HOME       (verify home sensor)
TEST 100        (forward/home loop stress-test)
SET_POS 0       (reset tracked position without moving, useful after manual moves)

Menu:
Navigate Up/Down → Red Select (key 5) to execute
```

---

## General Checks

- All GNDs common (supply, DM556, ESP32, LM2596).
- LM2596 regulated/stable 5V under load.
- 24V supply ≥5A current.
- Connectors/screws tight, no loose/mis-crimped wires.
- ESP32 reset after wiring/params changes.

If persists, capture:
- Sharp wiring photos (supply, DM556, ESP32, motor, LCD, sensors).
- Full Serial log (boot + test commands).

Open GitHub repo issue for detailed analysis.

**Eng. Fredy Osorio**  
ing.fredyosorio@gmail.com  
Rio de Janeiro, 2026
