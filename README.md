# PizzaController - NEMA23 Stepper Motor Controller with ESP32 and FastAccelStepper

This project provides a complete Arduino sketch for controlling a NEMA23 stepper motor using an ESP32-Wroom microcontroller and a DM556 stepper driver. The program uses the FastAccelStepper library for smooth acceleration/deceleration and is configured for 1/8 microstepping to achieve both precise positioning and high speed control. It includes an LCD menu system for easy operation and has been optimized for better code maintenance.

## Hardware Requirements

- ESP32-Wroom Development Board
- NEMA23 Stepper Motor
- DM556 Stepper Driver
- 24Vdc - 5A Power Supply
- 5-Button Navigation Keypad (analog)
- 7-Position Direct Buttons panel (analog, slots 0-4 + 2 jog directions)
- 16x2 I2C LCD Display (address 0x27)
- A3144 Hall Effect Home Sensor, powered with 3.3V
- Optional: LED on GPIO 32 for home-status indication

## Wiring Connections

Connect the ESP32 to the DM556 driver as follows:

- ESP32 GPIO 18 → DM556 STEP
- ESP32 GPIO 19 → DM556 DIR
- ESP32 GPIO 21 → DM556 ENABLE (active low)
- ESP32 GPIO 27 → Home Limit Sensor

### Direct Position Buttons (Slot Selection + CW/CCW Jog)

- ESP32 GPIO 34 → 7-position Direct Buttons (analog) - selects position slots 0-4 and triggers CW/CCW jog (keys 6/7) toward the selected slot

### Home Status LED (Optional)

- ESP32 GPIO 32 → LED output indicating home-switch state (lights when the home sensor is triggered)

### Emergency Stop

- ESP32 GPIO 33 → E-Stop button (digital, active LOW) - immediate motor stop

### Navigation Keypad

- ESP32 GPIO 35 → Navigation Keypad Buttons (analog read)

### LCD Display via I2C

**LCD (main menu, 0x27):**
- ESP32 GPIO 25 → SDA
- ESP32 GPIO 26 → SCL (I2C1/Wire)

Only one LCD is used by the firmware (address 0x27 on the default Wire bus). GPIO 22/23 are not driven by this sketch.

## Connections Map

| Component      | Pin     | Connected to                          |
|----------------|---------|---------------------------------------|
| ESP32          | GPIO 18 | DM556 STEP                            |
| ESP32          | GPIO 19 | DM556 DIR                             |
| ESP32          | GPIO 21 | DM556 ENABLE (active low)             |
| ESP32          | GPIO 25 | LCD I2C SDA                           |
| ESP32          | GPIO 26 | LCD I2C SCL                           |
| ESP32          | GPIO 27 | A3144 Hall Home Sensor (3.3V)         |
| ESP32          | GPIO 32 | Home Status LED (digital output)      |
| ESP32          | GPIO 33 | E-Stop Button                         |
| ESP32          | GPIO 34 | Direct Position Buttons (7-pos analog)|
| ESP32          | GPIO 35 | Navigation Keypad (analog)            |
| ESP32          | 5v      | 5V LM2596 out+                        |
| E-Stop Button  | Signal  | ESP32 GPIO 33                         |
| E-Stop Button  | VCC     | 3.3V                                  |
| E-Stop Button  | GND     | GND                                   |
| Home LED       | Anode   | ESP32 GPIO 32 (via resistor)          |
| Home LED       | Cathode | GND                                   |
| ESP32          | GND     | GND LM2596 out-,  Power Supply GND    |
| Power Supply   | +24V    | DM556 +V , LM2596 in+                 |
| Power Supply   | GND     | DM556 GND, ESP32 GND, LM2596 in-      |
| Power Supply   | L, N    | 85-256VAC 50/60Hz                     |
| DM556          | A+      | Motor Red                             |
| DM556          | A-      | Motor Black                           |
| DM556          | B+      | Motor Green                           |
| DM556          | B-      | Motor Yellow                          |
| Hall Sensor    | Signal  | ESP32 GPIO 27                         |
| Hall Sensor    | VCC     | 3.3V                                  |
| Hall Sensor    | GND     | GND                                   |
| LCD I2C        | SDA     | ESP32 GPIO 25                         |
| LCD I2C        | SCL     | ESP32 GPIO 26                         |
| LCD I2C        | VCC     | 5V (LM2596)                           |
| LCD I2C        | GND     | GND                                   |
| Direct Buttons | Signal  | ESP32 GPIO 34                         |
| Direct Buttons | VCC     | 3.3V                                  |
| Direct Buttons | GND     | GND                                   |
| Nav Keypad     | Signal  | ESP32 GPIO 35                         |
| Nav Keypad     | VCC     | 3.3V                                  |
| Nav Keypad     | GND     | GND                                   |

### DM556 DIP Switch Configuration

The DM556 driver uses DIP switches to configure microstepping and other settings. For this project (1/8 microstepping), configure as follows:

#### Current

NEMA23 Motor, 4.01A configuration:

- **SW1 (MS1):** OFF
- **SW2 (MS2):** ON
- **SW3 (MS3):** OFF
- **SW4 (MS4):** ON (full current)

#### Steps

Configuration for pulses per revolution (default 1600 pulses/rev in hardware — firmware default `STEPS_PER_REV` is **3200**, i.e. the firmware compensates for 1/8 microstepping):

- **SW5 (MS5):** OFF
- **SW6 (MS6):** OFF
- **SW7 (MS7):** ON
- **SW8 (MS8):** ON

> Note: the firmware defaults to `STEPS_PER_REV = 3200` (8× the motor's natural 400 steps because the DM556 is set to 1/8 microstepping). Change it at runtime with `SET_STEPS <value>` if your driver is set differently; the value is persisted in NVM.

### Power Connections

- **DM556 Driver Power:** 24VDC 5A power supply connected to DM556 power input terminals (+V and GND).
- **Stepper Motor Power:** Motor phases (A+, A-, B+, B-) connected directly to DM556 outputs via Permak 4-pin connector (binocular), following color code:

| Phase | Motor Color | DM556 Color |
|-------|-------------|-------------|
| A+    | Red         | Red         |
| A-    | Black       | Black       |
| B+    | Green       | Blue        |
| B-    | Yellow      | White       |

- **ESP32 Power:** ESP32 powered from same source through LM2596 Step-Down regulator adjusted to 5V on power pin. Ensure new regulator is set to 5V before connecting to ESP32.

## Software Setup

1. Install Arduino IDE
2. Install ESP32 board support in Arduino IDE
3. Install required libraries: FastAccelStepper, Preferences, Wire, LiquidCrystal_I2C
4. Open `PizzaController_FastStepper/PizzaController_FastStepper.ino` in Arduino IDE
5. Select correct ESP32 board and port
6. Upload the sketch

## Configuration

The program is configured for:
- **Microstepping:** 1/8 (set by DM556 DIP switches)
- **Steps per Revolution (firmware default):** 3200 — change at runtime with `SET_STEPS <value>` (saved to NVS)
- **Maximum Speed:** 8000 steps/sec — change with `SET_MAX_SPEED <value>` (saved to NVS, max 50000)
- **Acceleration:** 4000 steps/sec² — change with `SET_ACCELERATION <value>` (saved to NVS, max 50000)
- **Homing Speed:** 2000 steps/sec — change with `SET_SPEED <value>` (saved to NVS, max 50000)
- **Motor Hold Time:** 300 ms after each move — change with `SET_HOLD_TIME <ms>` (saved to NVS, max 10000)
- **Homing Direction:** -1 (negative) by default — change with `SET_HOME_DIR <-1|+1>` (saved to NVS)
- **Saved Position Slots:** 5 slots persisted in NVS (`SAVE_POS <0-4>` / `LOAD_POS <0-4>`)

See the Serial command table below for full configuration options.

## Program Functionality

The program initializes the stepper motor controller and continuously monitors serial commands for motor control. It supports jogging, position management, homing, and testing. The motor can be controlled remotely via serial commands or programmatically using provided functions.

### Serial Commands
The program supports serial commands for remote control. Open Serial Monitor at 115200 baud and send commands (uppercase, ended with enter).

- `JOG F <steps>`: Jog forward (clockwise) (1-50000)
- `JOG B <steps>`: Jog backward (counterclockwise) (1-50000)
- `MOVE_TO <position>`: To absolute position (±1,000,000 max)
- `HOME`: Move to position 0 (absolute move, no sensor)
- `RESET_HOME`: Set current position as the new home (0)
- `SAVE_POS <0-4>`: Save current position to slot 0-4
- `LOAD_POS <0-4>`: Move to saved position in slot 0-4
- `GET_POS`: Print current position (refreshed from stepper)
- `FIND_HOME`: Non-blocking home search using the hall sensor (50 ms debounce, 60 s timeout)
- `TEST <steps>`: Continuous test — alternates `steps` and home (repeat until `STOP`)
- `STOP`: Stop test/movement
- `SET_STEPS <value>`: Steps per revolution (saves NVM)
- `SET_MAX_SPEED <0-50000>`: Max speed steps/sec (saves NVM)
- `SET_ACCELERATION <0-50000>`: Acceleration steps/sec² (saves NVM)
- `SET_HOLD_TIME <0-10000>`: Motor hold time ms after move (saves NVM)
- `SET_SPEED <0-50000>`: Homing speed steps/sec (saves NVM)
- `SET_HOME_DIR <-1|+1>`: Homing direction (saves NVM)
- `SET_POS <steps>`: Override current position tracking to `<steps>` without moving the motor (saves NVM)
- `GET_INFO`: Show configuration and saved positions
- `GET_SWITCH`: Read home switch status and update LED
- `HELP`: Show full command list

Others: "Unknown command".
Examples: `JOG F 1000`, `SET_MAX_SPEED 10000`, `GET_INFO`.

## Customization

Most parameters are exposed at runtime via Serial commands and persist in NVM (`Preferences` namespace `"stepper"`), so no recompile is needed to change them:

| Serial command     | Effect                                                    |
|--------------------|-----------------------------------------------------------|
| `SET_STEPS <v>`    | Steps per revolution                                      |
| `SET_MAX_SPEED <v>`| Maximum speed (steps/sec)                                 |
| `SET_ACCELERATION <v>` | Acceleration (steps/sec²)                              |
| `SET_HOLD_TIME <ms>`| Motor hold time after each move                          |
| `SET_SPEED <v>`    | Homing speed                                               |
| `SET_HOME_DIR <d>` | Homing direction (-1 or +1)                                |
| `SET_POS <steps>`  | Override tracked position without moving the motor         |

If you do want to recompile with different defaults, edit the constants at the top of the sketch:

- `STEPS_PER_REV` — default steps per revolution (default 3200)
- `MAX_SPEED` — default max speed in steps/sec (default 8000)
- `ACCELERATION` — default acceleration in steps/sec² (default 4000)
- `HOMING_SPEED` — default homing speed in steps/sec (default 2000)
- `MOTOR_HOLD_TIME` — default hold time in ms (default 300)
- `HOME_DIRECTION` — default homing direction (default -1)
- `JOG_STEPS` — default jog step count for the menu (default 50)

Use the helper functions `startMotorMovement(targetPos)`, `moveToPosition(target)`, `home()`, and `resetHome()` for custom movements in your own code.

## Serial Debugging

The program generates status messages in Serial Monitor at 115200 baud. Open Serial Monitor in Arduino IDE to view debug information.

## Safety Notes

- Ensure adequate power supply ratings for motor and driver
- Verify wiring connections before powering on
- Start with low speeds and increase gradually as needed
- Monitor motor temperature during operation

## LCD Menu System

**LCD (0x27):** Main menu system - standalone operation (GPIO 25/26 I2C, address `0x27`, 16 columns × 2 rows).

The controller includes a user-friendly LCD menu system for standalone operation without a computer. The 16x2 I2C LCD displays menu options, and a 5-button analog keypad allows navigation and input. A second analog keypad (7-position) provides direct slot selection and CW/CCW jog buttons.

The 8 menu items, in on-screen order, are:

1. **Go to Saved Pos** — load one of the 5 saved position slots
2. **Change Speed** — adjust the maximum speed
3. **Change Accel** — adjust the acceleration
4. **Home** — move to position 0 (no sensor)
5. **Reset Home** — set the current position as the new home (requires confirmation)
6. **Save Position** — save the current position to one of 5 slots
7. **Go to Pos** — move to an absolute position (can be negative)
8. **Jog** — adjust the default jog step count used by the serial `JOG F`/`JOG B` keys

### Keypad Controls

The navigation keypad (GPIO 35) is read with 5 thresholds — keys 1-5. Mapping:

- **Key 1**: Up / decrement / toggle in sub-menus
- **Key 2**: Down / increment / toggle in sub-menus
- **Key 3**: Decrement by 100 (in numeric sub-menus)
- **Key 4**: Increment by 100 / cancel sub-menu
- **Key 5**: Select — enter sub-menu or execute action

In the main menu: keys 1/2 navigate, key 5 selects.
In numeric sub-menus (Speed, Accel, Go-to, Jog): key 1 = -10, key 2 = +10, key 3 = -100, key 4 = +100, key 5 = confirm.
In the slot-selection sub-menus (Goto Saved, Save Pos): keys 1/2 cycle 0–4, key 5 = confirm.
In the confirmation sub-menu (Reset Home): keys 1/2 toggle Yes/No, key 5 = confirm.

### Direct Buttons Panel (GPIO 34)

The 7-key direct panel reads through 7 ADC thresholds:

- **Keys 1-5**: select position slot (slot = key - 1, so key 1 = slot 0 ... key 5 = slot 4). The LCD top line shows the slot and its saved position; the bottom line shows current position / Moving / Homing.
- **Key 6**: CCW jog toward the currently selected saved position (shortest path)
- **Key 7**: CW jog toward the currently selected saved position (shortest path)

After the jog completes, the firmware uses `SET_POS` internally to update the tracked position to the target slot, so the controller never loses absolute reference after a wrap-around jog.

### Menu Navigation

1. Use Up/Down buttons to select a menu item
2. Press Select to enter that item's sub-menu
3. Use the keypad to adjust the value (or toggle Yes/No)
4. Press Select to execute and return to the main menu, or press Key 4 to cancel

The firmware also runs a homing calibration on boot — `findHomeDirection(1)` is called once after the LCD splash so the controller starts each session from a known reference.

## Author

**Developed for LOEM Laboratory, Physics Department, PUC-Rio.**

Eng. Fredy Osorio  
ing.fredyosorio@gmail.com  
Rio de Janeiro - Brazil, 2026.
