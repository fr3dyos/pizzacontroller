# PizzaController Troubleshooting Guide

This guide helps you diagnose and resolve common issues with the PizzaController ESP32 stepper motor controller. Follow the steps below for each problem area.

## Motor Not Moving

1. **Check Power Supply:**
   - Ensure the DM556 driver receives 24-48V DC with sufficient current for your motor.
   - Verify polarity is correct (+V and GND).

2. **Verify Wiring:**
   - Check all connections between ESP32, DM556 driver, and motor.
   - Ensure stepper motor phases (A+, A-, B+, B-) are correctly connected to DM556 outputs.

3. **Check ENABLE Pin:**
   - GPIO 21 should be LOW to enable the DM556 driver.
   - Verify this in the code and wiring.

4. **Microstepping Configuration:**
   - Confirm DM556 DIP switches: SW1 (MS1)=ON, SW2 (MS2)=ON, SW3 (MS3)=ON for 1/256 microstepping.
   - Ensure ESP32 pins GPIO 22, 23, 25 are HIGH for MS1, MS2, MS3.

5. **Serial Monitor:**
   - Open Serial Monitor at 115200 baud.
   - Check for initialization messages like "Stepper initialized" or "AccelStepper initialized".

## Direct Position Buttons Not Working

1. **Check Wiring:**
   - Buttons should be connected to GPIO 12-16 and GND.
   - Ensure active low configuration (button pressed = LOW).

2. **Code Configuration:**
   - Verify internal pull-ups are enabled for GPIO 12-16.
   - Check if debouncing is implemented in the handleDirectButtons() function.

3. **Serial Output:**
   - Press buttons and look for "Direct button X pressed" messages in Serial Monitor.
   - If no messages appear, check wiring and code.

4. **Button Functionality:**
   - Ensure saved positions exist (use SAVE_POS command).
   - Test LOAD_POS command to verify position loading works.

## Serial Communication Issues

1. **Baud Rate:**
   - Set Serial Monitor to 115200 baud.

2. **COM Port:**
   - Select the correct COM port for your ESP32 in Arduino IDE.

3. **USB Connection:**
   - Verify USB cable and drivers are working.
   - Try a different USB port or cable.

4. **Code Upload:**
   - Ensure code uploads successfully before testing serial commands.
   - Check for compilation errors.

## LCD Display Not Working

1. **I2C Connections:**
   - SDA: GPIO 25
   - SCL: GPIO 26

2. **LCD Address:**
   - Verify LCD I2C address is 0x27 or 0x3F (common addresses).

3. **Power Supply:**
   - Ensure LCD receives 5V power.
   - Check VCC and GND connections.

4. **Library:**
   - Confirm LiquidCrystal_I2C library is installed.

5. **Serial Output:**
   - Check for LCD-related error messages in Serial Monitor.

## Power Supply Problems

1. **Voltage Levels:**
   - ESP32: 5V/3.3V
   - DM556 Driver: 24-48V DC
   - LCD: 5V

2. **Polarity:**
   - Double-check all power connections for correct polarity.

3. **Current Capacity:**
   - Ensure power supplies can handle the required current for all components.

4. **Common Ground:**
   - Verify all components share a common ground connection.

## Code Compilation Errors

1. **Required Libraries:**
   - Install: AccelStepper, Preferences, Wire, LiquidCrystal_I2C

2. **Board Selection:**
   - Select "ESP32 Dev Module" in Arduino IDE Tools > Board.

3. **Syntax Errors:**
   - Check for missing semicolons, brackets, or typos.
   - Ensure all variables are declared.

4. **Include Statements:**
   - Verify all necessary headers are included at the top of the sketch.

## Additional Help

If these steps don't resolve your issue:
- Review the README.md for detailed setup instructions.
- Check the serial output for specific error messages.
- Verify all hardware connections match the wiring diagrams.
- Test with a simple example sketch to isolate the problem.

For community support, please provide:
- Your hardware setup details
- Code version and modifications
- Serial output messages
- Specific symptoms of the problem
