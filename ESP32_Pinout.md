# ESP32 Pinout for PizzaController

This document lists the GPIO pins used by the PizzaController project on the ESP32 microcontroller.

## Pin Assignments

| GPIO Pin | Function | Connected To | Notes |
|----------|----------|--------------|-------|
| GPIO 18 | STEP | DM556 Driver STEP input | Pulse signal for stepper motor steps |
| GPIO 19 | DIR | DM556 Driver DIR input | Direction control (HIGH = clockwise, LOW = counterclockwise) |
| GPIO 21 | ENABLE | DM556 Driver ENABLE input | Active low enable signal |
| GPIO 22 | MS1 | DM556 Driver MS1 input | Microstepping control |
| GPIO 23 | MS2 | DM556 Driver MS2 input | Microstepping control |
| GPIO 25 | LCD_SDA | LCD I2C SDA | I2C data line for LCD display |
| GPIO 26 | LCD_SCL | LCD I2C SCL | I2C clock line for LCD display |
| GPIO 27 | HOME_SWITCH | Home Limit Switch | Active low input with internal pull-up |
| GPIO 34 | KEYPAD_PIN | 5-button Analog Keypad | ADC input for keypad button detection |

## I2C Pins (for LCD)

- SDA: GPIO 25
- SCL: GPIO 26

## Notes

- GPIO 16 and GPIO 17 are dedicated to LCD I2C communication
- GPIO 34 is an ADC-capable pin used for analog keypad input
- GPIO 26 has internal pull-up resistor enabled for the limit switch
- All pins are configured as digital I/O except GPIO 34 which is analog input

## ESP32 Pin Capabilities

- **GPIO 18, 19, 21-23, 25-27**: Digital I/O, PWM capable
- **GPIO 34**: ADC1_CH6, input only (no pull-up/down)

## Power Pins

- 3.3V: Power for ESP32 logic
- 5V: Power for LCD and keypad (if needed)
- GND: Common ground

## Unused Pins

The following pins are available for future expansion:
- GPIO 0, 2, 4, 5, 12-17, 28, 32, 33, 35
- ADC pins: GPIO 32, 33, 35 (GPIO 34 is used for keypad)
- DAC pins: GPIO 25, 26 (but GPIO 25 and 26 are used)
- I2C: GPIO 16 (SDA), GPIO 17 (SCL) - available
- SPI: GPIO 18, 19, 21, 22, 23 - partially used
