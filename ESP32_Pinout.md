# ESP32 Pinout Reference - PizzaController

Pin assignments for PizzaController based on current hardware configuration.

## Active Pins Used

| GPIO | Function | Connected To | Type | Notes |
|------|----------|--------------|------|-------|
| 18 | STEP | DM556 STEP | Digital Out | Stepper pulses |
| 19 | DIR | DM556 DIR | Digital Out | Direction control |
| 21 | ENABLE | DM556 EN | Digital Out | Active LOW enable |
| 25 | SDA | LCD I2C SDA | I2C1 | Address 0x27 |
| 26 | SCL | LCD I2C SCL | I2C1 | Main menu |
| 27 | HOME | A3144 Hall Sensor | Analog In | Threshold <1500, 3.3V |
| 32 | LED_HOME | Home Status LED | Digital Out | HIGH when home sensor is triggered (mirrors `GET_SWITCH`) |
| 33 | ESTOP | Emergency Stop Button | Digital In | INPUT_PULLUP, active LOW, immediate stop |
| 34 | DIRECT_BTN | 7-pos Direct Buttons | Analog In (ADC1_CH6) | Thresholds: 130/570/1170/1740/2370/3100/3700, Keys 1-5 = slots 0-4, keys 6/7 = CCW/CW jog |
| 35 | KEYPAD | Navigation Keypad | Analog In (ADC1_CH7) | Thresholds: 220/800/1400/2300/3600 |

> GPIO 22 and GPIO 23 are **not driven** by the current sketch. Earlier drafts referenced a second I2C LCD (address 0x3F on Wire2) — that hardware is not part of this firmware build.

## Power Connections

| Rail | Source | Used By |
|------|--------|---------|
| 24VDC 5A | Power Supply | DM556 +V |
| 5V | LM2596 (from 24V) | ESP32 VIN, LCD VCC |
| 3.3V | ESP32 | Hall VCC, Buttons VCC |
| GND | Common | All components |

## DM556 DIP Configuration (1/8 microstep, 1600ppr hardware / 3200 STEPS_PER_REV firmware, 4.01A)

| SW | Setting | Purpose |
|----|---------|---------|
| 1  | OFF | Current (MS1) |
| 2  | ON  | Current (MS2) |
| 3  | OFF | Current (MS3) |
| 4  | ON  | Current full |
| 5  | OFF | Steps |
| 6  | OFF | Steps |
| 7  | ON  | Steps |
| 8  | ON  | Steps |

The firmware defaults `STEPS_PER_REV = 3200` (8× the motor's natural 400 steps). If you change the DIP switches, run `SET_STEPS <value>` to keep the firmware in sync — the value is saved to NVM.

## Motor Wiring (Permak 4-pin)

| Phase | Motor Wire | DM556 Terminal |
|-------|------------|----------------|
| A+    | Red        | Red            |
| A-    | Black      | Black          |
| B+    | Green      | Blue           |
| B-    | Yellow     | White          |

## Capabilities & Restrictions

- **Analog Inputs:** GPIO 34/35/27 only (ADC1)
- **No MS1/MS2/MS3 control:** DM556 DIP switches only
- **GPIO 34/35:** Input-only (no pullup/down), perfect for analog buttons
- **GPIO 27:** Analog for Hall threshold detection (<1500 = triggered)
- **GPIO 32:** Digital output for home-status LED (not an input — GPIO 32 is on ADC2 and is used here as a digital output)
- **I2C:** GPIO 25/26 standard (Wire); one LCD at address 0x27
- **No interrupts:** Hall polling with std-dev filter and 50 ms debounce

## Available for Expansion

- GPIO 0, 2, 4, 5 (general-purpose)
- GPIO 22, 23 (available I2C pair / SPI / UART)
- GPIO 16, 17 (UART2)
- GPIO 33 (currently E-Stop; could be reassigned if E-Stop is moved)

## Verification Checklist

```
[X] GPIO 18/19/21 → DM556 STEP/DIR/EN ✓
[X] GPIO 25/26 → LCD I2C (single LCD, address 0x27) ✓
[X] GPIO 27 analog Hall 3.3V ✓
[X] GPIO 32 → Home Status LED (digital output) ✓
[X] GPIO 33 → E-Stop button (active LOW, INPUT_PULLUP) ✓
[X] GPIO 34/35 analog buttons 3.3V ✓
[X] LM2596 5V → ESP32/LCD ✓
[X] 24V 5A → DM556 ✓
[X] DIP SW1-8 per table ✓
[X] Motor colors match ✓
[X] All GND common ✓
```

**Eng. Fredy Osorio**  
*Rio de Janeiro, 2026*
