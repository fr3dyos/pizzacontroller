# README.md Update Plan

**Goal:** Update README.md by merging/incorporating accurate details from README-pt-BR.md reference (hardware specs, wiring incl. GPIO 35, DIP SW config with current/steps, power table, 1600 steps/rev, FastStepper folder).

**Changes:**
- Hardware: Add ESP32-Wroom, 24Vdc-5A PSU, 5-button keypad, 16x2 LCD, A3144 Hall sensor, custom direct position button.
- Wiring: Add GPIO 35 navigation keypad, LCD I2C GPIO 25 SDA / 26 SCL (note conflict with MS3/home, resolve).
- DIP: Update to PT-BR SW1 OFF, SW2 ON, SW3 OFF for current; SW5-8 for 1600 pulses/rev.
- Power: Add table for motor/DM556 colors, LM2596 5V step-down for ESP32.
- Software: Change folder to PizzaController_FastStepper.ino.
- Config: Update steps/rev to 1600.
- Translate/merge rest, fix inconsistencies (FastAccelStepper vs AccelStepper mention).

**Steps:**
- [x] 1. Backup current README.md (README.md.backup)
- [x] 2. Edit README.md with merged content (from README.md.new)
- [x] 3. Commit and push to main

