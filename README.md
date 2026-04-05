# PizzaController - ESP32 NEMA23 Stepper Motor Controller with FastAccelStepper

This project provides a complete Arduino sketch for controlling a NEMA23 stepper motor using an ESP32-Wroom microcontroller and a DM556 stepper driver. The program uses the FastAccelStepper library for high-performance, non-blocking motor control with smooth acceleration/deceleration and is configured for 1/256 microstepping to achieve precise positioning control. It includes an LCD menu system for easy operation, 5-button keypads for navigation and direct positions, home sensor, and has been optimized for low latency and better performance.

## Hardware Requirements

- ESP32-Wroom Development Board
- NEMA23 Stepper Motor (4.01A)
- DM556 Stepper Driver
- Power Supply 24Vdc - 5A
- 5-Button Keypad (navigation)
- 16x2 LCD Display (I2C)
- A3144 Hall Effect Home Sensor
- Custom direct position buttons

## Wiring Connections

Connect the ESP32 to the DM556 driver as follows:

- ESP32 GPIO 18 → DM556 STEP
- ESP32 GPIO 19 → DM556 DIR
- ESP32 GPIO 21 → DM556 ENABLE (active low)
- ESP32 GPIO 22 → DM556 MS1
- ESP32 GPIO 23 → DM556 MS2
- ESP32 GPIO 25 → DM556 MS3
- ESP32 GPIO 26 → Home Limit Sensor (active low, one terminal to GPIO 26 and the other to GND)

### Direct Position Buttons

- ESP32 GPIO 34 → Direct Position Buttons

### Navigation Keypad

- ESP32 GPIO 35 → Navigation Keypad

### LCD 16x2 I2C Display

- ESP32 GPIO 25 → SDA (Serial Data)
- ESP32 GPIO 26 → SCL (Serial Clock)
**Note:** GPIO 25/26 used for both MS3/home and LCD I2C – verify code configuration.

### DM556 DIP Switch Configuration

#### Current (NEMA23, 4.01A)

- **SW1:** OFF
- **SW2:** ON
- **SW3:** OFF
- **SW4:** ON (full current)

#### Steps (1600 pulses/revolution)

- **SW5:** OFF
- **SW6:** OFF
- **SW7:** ON
- **SW8:** ON

### Power Connections

- **DM556 Driver Power:** 24VDC, 5A power supply connected to DM556 power input terminals (+V and GND).
- **Stepper Motor Phases:** Connected to DM556 outputs via Permak 4-pin connector (binocular), following color code:

| Phase | Motor Color | DM556 Color |
|-------|-------------|-------------|
| A+    | Red         | Red         |
| A-    | Black       | Black       |
| B+    | Green       | Blue        |
| B-    | Yellow      | White       |

- **ESP32 Power:** Powered from same source through LM2596 Step-Down regulator adjusted to 5V on power pin. Ensure new regulator is set to 5V before connecting to ESP32.

**Note:** Adjust GPIO pins in code if wiring differs. Verify power ratings to avoid damage.

## Software Setup

1. Install Arduino IDE
2. Install ESP32 board support in Arduino IDE
3. Install libraries: FastAccelStepper, Preferences, Wire, LiquidCrystal_I2C
4. Open `PizzaController_FastStepper/PizzaController_FastStepper.ino` in Arduino IDE
5. Select correct ESP32 board and port
6. Upload the sketch

## Configuration

The program is configured for:
- **Microstepping:** 1/256 (maximum precision)
- **Pulses per Revolution:** 1600
- **Speed:** Adjustable via `SPEED_DELAY` constant (currently 500 microseconds between steps)

## Program Functionality

The program initializes the stepper motor controller and continuously monitors serial commands for motor control. It supports jogging, position management, homing, and testing. Motor controlled remotely via serial or programmatically.

## New Features Added

### Jog Function
- `jog(steps, direction)`: Move specific number of steps in specified direction
- Direction: `true` clockwise, `false` counterclockwise

### Position Saving
- `savePosition()`: Save current position to non-volatile memory (persists across power cycles)
- Positions loaded automatically on startup

### Homing Function
- `home()`: Move to home position (0)
- Calculates movement based on current position

### Reset Home Function
- `resetHome()`: Set current position as new home (0)
- Useful for recalibrating reference

### Position Tracking
- Real-time position relative to home
- Persistent storage via ESP32 Preferences library

## Usage Examples

### Basic Jogging
```cpp
// Jog 1000 steps clockwise
jog(1000, true);

// Jog 500 steps counterclockwise
jog(500, false);
```

### Position Management
```cpp
// Move to specific position
moveSteps(2500, true);

// Save current position
savePosition();

// Return to home
home();

// Set current as new home
resetHome();
```

### Serial Command Examples
Open Serial Monitor at 115200 baud, send commands (case-sensitive + newline):

- `JOG F <steps>`: Jog forward (e.g., `JOG F 1000`)
- `JOG B <steps>`: Jog backward (e.g., `JOG B 500`)
- `MOVE_TO <position>`: Move absolute (e.g., `MOVE_TO 2500`)
- `HOME`: To home (0)
- `FIND_HOME`: Find home via GPIO 26 switch
- `RESET_HOME`: Current as new home
- `SAVE_POS <num>`: Save to slot 0-4 (e.g., `SAVE_POS 1`)
- `LOAD_POS <num>`: Load from slot 0-4
- `GET_POS`: Current position
- `TEST <steps>` / `RUN_TEST <steps>`: Test mode
- `STOP`: Stop test

Other input: "Unknown command".

### Demo Functions
```cpp
demoJog();              // Jog back and forth
delay(2000);
demoSaveAndHome();      // Save, home, reset demo
```

## Customization

- Change `SPEED_DELAY` (lower = faster)
- Adjust loop steps for rotation angles
- `moveSteps()` for custom moves

## Serial Debugging

Status messages at 115200 baud in Serial Monitor.

## Safety Notes

- Proper PSU ratings for motor/driver
- Verify wiring before power-on
- Start low speeds, increase gradually
- Monitor motor temperature

## Troubleshooting

- Motor not moving: Check ENABLE LOW
- Microstepping pins for 1/256
- Adequate PSU voltage/current
- Serial Monitor init messages

## Code Optimizations

- **Enums:** Descriptive states replace magic numbers
- **FastAccelStepper:** Non-blocking high-perf control
- **Analog Keypads:** Better integration
- **Interrupt Homing:** Efficient limit detection
- **Serial Buffering:** No motor blocking
- **Persistent Params:** Steps/speed/accel saved NVM
- **Modular Code:** Logical functions

## LCD Menu System

16x2 I2C LCD with 5-button keypad for standalone operation.

### Menu Options
1. Jog (manual steps)
2. Change Speed
3. Change Acceleration
4. Home
5. Reset Home (confirm)
6. Save Position (slot, confirm)
7. Go to Position (absolute)
8. Go to Saved Position

### Keypad Controls
- Up/Down: Navigate
- Left/Right: ±10
- Up/Down sub: ±100
- Select: Enter/execute

### Note
LCD menu not implemented in current `PizzaController_FastStepper.ino` – may be in development.

## Advanced Features

- Encoder position feedback
- Accel/decel profiles
- Limit switches
- Serial integration

## Author

Eng. Fredy Osorio <ing.fredyosorio@gmail.com>
