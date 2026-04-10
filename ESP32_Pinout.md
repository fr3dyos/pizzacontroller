# ESP32 Pinout Reference - PizzaController

Pin assignments for PizzaController based on current hardware configuration.

## Active Pins Used

| GPIO | Function | Connected To | Type | Notes |
|------|----------|--------------|------|-------|
| 18 | STEP | DM556 STEP | Digital Out | Stepper pulses |
| 19 | DIR | DM556 DIR | Digital Out | Direction control |
| 21 | ENABLE | DM556 EN | Digital Out | Active LOW enable |
| 25 | SDA1 | LCD1 I2C SDA | I2C1 | Address 0x27 |
| 26 | SCL1 | LCD1 I2C SCL | I2C1 | Main menu |
| 22 | SDA2 | LCD2 I2C SDA | I2C2 | Address 0x3F |
| 23 | SCL2 | LCD2 I2C SCL | I2C2 | Status display |
| 27 | HOME | A3144 Hall Sensor | Analog In | Threshold <1500, 3.3V |
| 32 | ACTION_BTN | Action Buttons CW/CCW | Analog In (ADC2_CH4) | Thresholds: 300/900 |
| 33 | ESTOP | Emergency Stop Button | Digital In | Active LOW, immediate stop |
| 34 | DIRECT_BTN | 5-pos Direct Buttons | Analog In (ADC1_CH6) | Thresholds: 320/1000/1800/2800/3600, Position slots 0-4 |
| 35 | KEYPAD | Navigation Keypad | Analog In (ADC1_CH7) | Thresholds: 220/800/1400/2300/3600 |

## Power Connections

| Rail | Source | Used By |
|------|--------|---------|
| 24VDC 5A | Power Supply | DM556 +V |
| 5V | LM2596 (from 24V) | ESP32 VIN, LCD VCC |
| 3.3V | ESP32 | Hall VCC, Buttons VCC |
| GND | Common | All components |

## DM556 DIP Configuration (1/8 microstep, 1600ppr, 4.01A)

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
- **I2C:** GPIO 25/26 standard
- **No interrupts:** Hall polling with std dev filter/debounce

## Available for Expansion

- GPIO 0, 2, 4, 5, 32, 33 (ADC2)
- UART2: GPIO 16/17
- SPI: GPIO 23 (if freed)

## Verification Checklist

```
[X] GPIO 18/19/21 → DM556 STEP/DIR/EN ✓
[X] GPIO 25/26 → LCD I2C ✓
[X] GPIO 27 analog Hall 3.3V ✓
[X] GPIO 34/35 analog buttons 3.3V ✓
[X] LM2596 5V → ESP32/LCD ✓
[X] 24V 5A → DM556 ✓
[X] DIP SW1-8 per table ✓
[X] Motor colors match ✓
[X] All GND common ✓
```

**Eng. Fredy Osorio**  
*Rio de Janeiro, April 2026*
