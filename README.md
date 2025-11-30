# PizzaController - ESP32 NEMA23 Stepper Motor Controller with AccelStepper

This project provides a complete Arduino sketch for controlling a NEMA23 stepper motor using an ESP32 microcontroller and a DM556 stepper driver. The program uses the AccelStepper library for smooth acceleration/deceleration and is configured for maximum microstepping (1/256) to achieve precise positioning control. It includes an LCD menu system for easy operation and has been optimized for better code maintainability using enums instead of magic numbers.

## Hardware Requirements

- ESP32 Development Board
- NEMA23 Stepper Motor
- DM556 Stepper Driver
- Power Supply (appropriate for your motor and driver)
- Connecting Wires
- Optional: Home Limit Switch (active low)

## Wiring Connections

Connect the ESP32 to the DM556 driver as follows:

- ESP32 GPIO 18 → DM556 STEP
- ESP32 GPIO 19 → DM556 DIR
- ESP32 GPIO 21 → DM556 ENABLE (active low)
- ESP32 GPIO 22 → DM556 MS1
- ESP32 GPIO 23 → DM556 MS2
- ESP32 GPIO 25 → DM556 MS3
- ESP32 GPIO 26 → Home Limit Switch (active low, connect one terminal to GPIO 26 and the other to GND)

### DM556 DIP Switch Configuration

The DM556 driver uses DIP switches to configure microstepping and other settings. For this project (1/256 microstepping), set the switches as follows:

- **SW1 (MS1):** ON
- **SW2 (MS2):** ON
- **SW3 (MS3):** ON
- **SW4-SW8:** Refer to your DM556 manual for current and other settings (typically OFF for default current)

**Note:** Ensure the DIP switches match the microstepping pins set in the ESP32 code (MS1, MS2, MS3 all HIGH for 1/256).

### Power Connections

- **DM556 Driver Power:** Connect a suitable DC power supply (typically 24V-48V DC, check your motor specifications) to the DM556 power input terminals (+V and GND).
- **Stepper Motor Power:** The motor phases (A+, A-, B+, B-) are connected directly to the DM556 driver outputs.
- **ESP32 Power:** Power the ESP32 via USB or a separate 5V/3.3V supply. Ensure the power supply can handle the current requirements.

**Note:** Adjust the GPIO pins in the code if your wiring differs. Ensure proper power ratings to avoid damage.

## Software Setup

1. Install the Arduino IDE
2. Install the ESP32 board support in Arduino IDE
3. Install the required libraries: AccelStepper, Preferences, Wire, LiquidCrystal_I2C
4. Open `PizzaController_AccelStepper/PizzaController_AccelStepper.ino` in Arduino IDE
5. Select the correct ESP32 board and port
6. Upload the sketch

## Configuration

The program is configured for:
- **Microstepping:** 1/256 (maximum precision)
- **Steps per Revolution:** 51200 (200 full steps × 256 microsteps)
- **Speed:** Adjustable via `SPEED_DELAY` constant (currently 500 microseconds between steps)

## Program Functionality

The program initializes the stepper motor controller and continuously monitors for serial commands to control the motor. It supports jogging, position management, homing, and testing functions. The motor can be controlled remotely via serial commands or programmatically using the provided functions.

## New Features Added

### Jog Function
- `jog(steps, direction)`: Move the motor a specific number of steps in the specified direction
- Direction: `true` for clockwise, `false` for counterclockwise

### Position Saving
- `savePosition()`: Save the current position to non-volatile memory (survives power cycles)
- Positions are automatically loaded on startup

### Homing Function
- `home()`: Move the motor back to the home position (position 0)
- Calculates the required movement based on current position

### Reset Home Function
- `resetHome()`: Set the current position as the new home position (0)
- Useful for recalibrating the home reference point

### Position Tracking
- Real-time position tracking relative to home
- Persistent storage using ESP32 Preferences library

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
// Move to a specific position
moveSteps(2500, true);

// Save the current position
savePosition();

// Return to home
home();

// Set current position as new home
resetHome();
```

### Serial Command Examples
The program supports serial commands for remote control. Open the Serial Monitor at 115200 baud and send commands (case-sensitive, followed by newline):

- `JOG F <steps>`: Jog forward (clockwise) by the specified number of steps (e.g., `JOG F 1000`)
- `JOG B <steps>`: Jog backward (counterclockwise) by the specified number of steps (e.g., `JOG B 500`)
- `MOVE_TO <position>`: Move to an absolute position (e.g., `MOVE_TO 2500`)
- `HOME`: Move to the home position (position 0)
- `FIND_HOME`: Find home using the limit switch connected to GPIO 26
- `RESET_HOME`: Set the current position as the new home position
- `SAVE_POS <num>`: Save the current position to slot 0-4 (e.g., `SAVE_POS 1`)
- `LOAD_POS <num>`: Load position from slot 0-4 (e.g., `LOAD_POS 1`)
- `GET_POS`: Get the current position
- `TEST <steps>`: Start the test function with the specified number of steps (e.g., `TEST 1000`)
- `RUN_TEST <steps>`: Start the test function with the specified number of steps (e.g., `RUN_TEST 1000`)
- `STOP`: Stop the test function

Any other input will respond with "Unknown command".

### Demo Function Examples
Add these function calls to the `loop()` function or call them from serial commands for demonstration:

```cpp
// In loop() or setup() for automatic demo
demoJog();        // Performs jogging back and forth
delay(2000);      // Wait 2 seconds
demoSaveAndHome(); // Demonstrates saving position, homing, and resetting home
```

## Customization

- Modify `SPEED_DELAY` to change motor speed (lower values = faster)
- Adjust the number of steps in the loops for different rotation angles
- Uncomment the `AccelStepper` library include for advanced features
- Use the `moveSteps()` function for custom movements

## Serial Debugging

The program outputs status messages to the Serial Monitor at 115200 baud. Open the Serial Monitor in Arduino IDE to view debug information.

## Safety Notes

- Ensure proper power supply ratings for your motor and driver
- Verify wiring connections before powering on
- Start with low speeds and gradually increase as needed
- Monitor motor temperature during operation

## Troubleshooting

- If the motor doesn't move, check ENABLE pin (should be LOW to enable)
- Verify microstepping pins (MS1, MS2, MS3) are set correctly for 1/256 mode
- Ensure adequate power supply voltage and current
- Check Serial Monitor for initialization messages

## Code Optimizations

The code has been optimized for better maintainability and readability:

- **Enum Usage:** Replaced magic numbers with descriptive enum values (`NONE`, `JOG`, `SPEED`, `ACCEL`, `SAVE_POS`, `GOTO`, `GOTO_SAVED`) for menu states
- **AccelStepper Library:** Uses the AccelStepper library for smooth acceleration/deceleration instead of manual step timing
- **Persistent Settings:** Motor parameters (steps per revolution, max speed, acceleration) are saved to non-volatile memory
- **Modular Functions:** Code is organized into logical functions for better readability and maintenance

## LCD Menu System

The controller includes a user-friendly LCD menu system for easy operation without a computer. The 16x2 I2C LCD displays menu options, and a 5-button analog keypad allows navigation and input.

### Menu Options

1. **Jog**: Manually move the motor by entering the number of steps
2. **Change Speed**: Adjust the maximum speed setting
3. **Change Accel**: Adjust the acceleration setting
4. **Home**: Move the motor to the home position (0)
5. **Reset Home**: Set the current position as the new home (requires confirmation)
6. **Save Position**: Save the current position to one of 5 slots (requires confirmation)
7. **Go to Position**: Move to an absolute position (can be negative)
8. **Go to Saved Position**: Load a saved position from one of 5 slots

### Keypad Controls

- **Up/Down**: Navigate through menu items
- **Left/Right**: Adjust values in sub-menus (increment/decrement by 10)
- **Up/Down in sub-menu**: Adjust values by 100
- **Select**: Enter sub-menu or execute action

### Menu Navigation

1. Use Up/Down buttons to select a menu item
2. Press Select to enter the sub-menu for that item
3. Use Left/Right/Up/Down to adjust the value
4. Press Select again to execute the action and return to the main menu

## Advanced Features

For more advanced control, consider:
- Adding position feedback with encoders
- Implementing acceleration/deceleration profiles
- Adding limit switches for homing
- Integrating with other control systems via serial communication

## Author

Eng. Fredy Osorio <ing.fredyosorio@gmail.com>
