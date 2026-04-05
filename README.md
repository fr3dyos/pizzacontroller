# PizzaController - NEMA23 Stepper Motor Controller with ESP32 and FastAccelStepper

This project provides a complete Arduino sketch for controlling a NEMA23 stepper motor using an ESP32-Wroom microcontroller and a DM556 stepper driver. The program uses the FastAccelStepper library for smooth acceleration/deceleration and is configured for 1/8 microstepping to achieve both precise positioning and high speed control. It includes an LCD menu system for easy operation and has been optimized for better code maintenance.

## Hardware Requirements

- ESP32-Wroom Development Board
- NEMA23 Stepper Motor
- DM556 Stepper Driver
- 24Vdc - 5A Power Supply
- 5-Button Keypad
- 16x2 LCD Display
- A3144 Hall Effect Home Sensor, powered with 3.3V
- Custom direct position buttons

## Wiring Connections

Connect the ESP32 to the DM556 driver as follows:

- ESP32 GPIO 18 → DM556 STEP
- ESP32 GPIO 19 → DM556 DIR
- ESP32 GPIO 21 → DM556 ENABLE (active low)
- ESP32 GPIO 27 → Home Limit Sensor

### Direct Position Buttons

- ESP32 GPIO 34 → 5-position Direct Position Buttons (analog read)

### Navigation Keypad

- ESP32 GPIO 35 → Navigation Keypad Buttons (analog read)

### 16x2 LCD Display via I2C

- ESP32 GPIO 25 → SDA (Serial Data)
- ESP32 GPIO 26 → SCL (Serial Clock)

## Connections Map

| Component      | Pin     | Connected to                          |
|----------------|---------|---------------------------------------|
| ESP32          | GPIO 18 | DM556 STEP                            |
| ESP32          | GPIO 19 | DM556 DIR                             |
| ESP32          | GPIO 21 | DM556 ENABLE (active low)             |
| ESP32          | GPIO 25 | LCD I2C SDA                           |
| ESP32          | GPIO 26 | LCD I2C SCL                           |
| ESP32          | GPIO 27 | A3144 Hall Home Sensor (3.3V)         |
| ESP32          | GPIO 34 | Direct Position Buttons (5-pos analog)|
| ESP32          | GPIO 35 | Navigation Keypad (analog)            |
| ESP32          | 5v      | 5V LM2596 out+                        |
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

Configuration for pulses per revolution (default 1600 pulses/rev):

- **SW5 (MS5):** OFF
- **SW6 (MS6):** OFF
- **SW7 (MS7):** ON
- **SW8 (MS8):** ON

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
- **Microstepping:** 1/8
- **Steps per Revolution:** 1600
- **Speed:** Adjustable via `SPEED_DELAY` constant (currently 500 microseconds between steps)

## Program Functionality

The program initializes the stepper motor controller and continuously monitors serial commands for motor control. It supports jogging, position management, homing, and testing. The motor can be controlled remotely via serial commands or programmatically using provided functions.

### Serial Commands
The program supports serial commands for remote control. Open Serial Monitor at 115200 baud and send commands (uppercase, ended with enter).

- `JOG F <steps>`: Jog forward (clockwise) (1-50000)
- `JOG B <steps>`: Jog backward (counterclockwise) (1-50000)
- `MOVE_TO <position>`: To absolute position (±1M max)
- `HOME`: To position 0
- `RESET_HOME`: Current position as home (0)
- `SAVE_POS <0-4>`: Save to slot 0-4
- `LOAD_POS <0-4>`: Go to slot 0-4
- `GET_POS`: Current position
- `FIND_HOME`: Non-blocking home search with limit sensor
- `TEST <steps>`: Continuous test (forward/home repeat)
- `STOP`: Stop test/movement
- `SET_STEPS <value>`: Steps/rev (saves NVM)
- `SET_MAX_SPEED <0-50000>`: Max speed steps/sec (saves)
- `SET_ACCELERATION <0-50000>`: Acceleration steps/sec² (saves)
- `SET_HOLD_TIME <0-10000>`: Hold time ms after move (saves)
- `SET_SPEED <0-50000>`: Home speed (saves)
- `SET_HOME_DIR <-1/+1>`: Home direction (saves)
- `GET_INFO`: Show config (steps, speed, accel, hold, positions)
- `GET_SWITCH`: Home sensor status
- `HELP`: Show command list

Others: "Unknown command".
Examples: `JOG F 1000`, `SET_MAX_SPEED 10000`, `GET_INFO`.

## Customization

- Modify `SPEED_DELAY` to change motor speed (lower values = faster)
- Adjust step counts in loops for different rotation angles
- Use `moveSteps()` function for custom movements

## Serial Debugging

The program generates status messages in Serial Monitor at 115200 baud. Open Serial Monitor in Arduino IDE to view debug information.

## Safety Notes

- Ensure adequate power supply ratings for motor and driver
- Verify wiring connections before powering on
- Start with low speeds and increase gradually as needed
- Monitor motor temperature during operation

## LCD Menu System

The controller includes a user-friendly LCD menu system for standalone operation without a computer. The 16x2 I2C LCD displays menu options, and a 5-button analog keypad allows navigation and input.

### Menu Options

1. **Jog**: Manually move motor by entering number of steps
2. **Change Speed**: Adjust maximum speed setting
3. **Change Acceleration**: Adjust acceleration setting
4. **Home**: Move motor to initial position (0)
5. **Reset Home**: Set current position as new home (requires confirmation)
6. **Save Position**: Save current position to one of 5 slots (requires confirmation)
7. **Go to Position**: Move to absolute position (can be negative)
8. **Go to Saved Position**: Load saved position from one of 5 slots

### Keypad Controls

- **Up/Down**: Navigate menu items
- **Left/Right**: Adjust values in sub-menus (increment/decrement by 10)
- **Up/Down in sub-menu**: Adjust values by 100
- **Select (Red)**: Enter sub-menu or execute action

### Menu Navigation

1. Use Up/Down buttons to select menu item
2. Press Select to enter that item's sub-menu
3. Use Left/Right/Up/Down to adjust value
4. Press Select again to execute action and return to main menu

## Author

Eng. Fredy Osorio
ing.fredyosorio@gmai.com
Rio de Janeiro - Brazil, April 2026.
