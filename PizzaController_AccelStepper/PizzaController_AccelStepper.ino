// PizzaController_AccelStepper.ino - ESP32 Program to Control NEMA23 Stepper Motor with DM556 Driver using AccelStepper Library
// This program demonstrates basic stepper motor control with maximum microstepping (1/256) for precise positioning.
// It includes functions for jogging, saving positions, homing, and resetting home position.
// Uses AccelStepper library for smooth acceleration/deceleration.
// Author: Eng. Fredy Osorio <ing.fredyosorio@gmail.com>

// Serial Commands:
// - JOG F <steps>: Jog forward (clockwise) by <steps> steps
// - JOG B <steps>: Jog backward (counterclockwise) by <steps> steps
// - MOVE_TO <position>: Move to absolute position <position>
// - HOME: Move to home position (0)
// - RESET_HOME: Set current position as new home
// - SAVE_POS <num>: Save current position to slot <num> (0-4)
// - LOAD_POS <num>: Load position from slot <num>
// - GET_POS: Print current position
// - FIND_HOME: Find home using limit switch
// - TEST <steps>: Start continuous test with <steps> steps
// - STOP: Stop the test
// - SET_STEPS <value>: Set steps per revolution and save to memory
// - SET_MAX_SPEED <value>: Set maximum speed and save to memory
// - SET_ACCELERATION <value>: Set acceleration and save to memory

// Step 1: Include necessary libraries
#include <Preferences.h>  // For saving positions to non-volatile memory
#include <AccelStepper.h> // For advanced stepper control with acceleration
#include <Wire.h>         // For I2C communication
#include <LiquidCrystal_I2C.h> // For LCD display

// Step 2: Define pin connections for ESP32 to DM556 driver
// These pins are GPIO pins on ESP32. Adjust based on your wiring.
#define STEP_PIN 18      // Connect to STEP input on DM556
#define DIR_PIN 19       // Connect to DIR input on DM556
#define ENABLE_PIN 21    // Connect to ENABLE input on DM556 (active low)
#define HOME_SWITCH_PIN 27 // Connect to home limit switch (active low)

// LCD and Keypad definitions
#define LCD_ADDR 0x27    // I2C address for LCD (common for 16x2 I2C LCD)
#define LCD_COLS 16
#define LCD_ROWS 2
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
#define KEYPAD_PIN 34    // Analog pin for 5-button Adkeypad

// Enum for submenu types
enum SubMenu {
  NONE,
  JOG,
  SPEED,
  ACCEL,
  HOME,       // Immediate action
  RESET_HOME, // Immediate action
  SAVE_POS,
  GOTO,
  GOTO_SAVED,
  CONFIRM_RESET_HOME,
  CONFIRM_SAVE_POS
};

// Step 3: Define motor parameters
// NEMA23 typically has 200 steps per revolution (full step).
// DM556 driver microstepping is set via DIP switches, not GPIO pins.
// STEPS_PER_REV, MAX_SPEED, ACCELERATION are configurable via serial commands and saved to memory.
int STEPS_PER_REV = 10000;  // Default steps per revolution (full step)
float MAX_SPEED = 3000.0;   // Maximum speed in steps per second (typical range: 100-10000 steps/sec, depending on motor/driver)
float ACCELERATION = 1000.0; // Acceleration in steps per second squared (typical range: 100-5000 steps/sec² for smooth acceleration)
int JOG_STEPS = 20; // Steps per button press in jog submenu

// Step 3.1: Global variables for position tracking
long currentPosition = 0;         // Current position in steps from home
long homePosition = 0;            // Home position offset
long savedPositions[5] = {0};     // Array for up to 5 saved positions
Preferences preferences;          // For saving positions to flash memory

// Step 3.2: Global variables for test function
bool isTesting = false;           // Flag to indicate if test is running
long testSteps = 0;               // Number of steps for test

// Step 3.4: Menu variables
int menuIndex = 0;                // Current menu item index
int menuSize = 8;                 // Number of menu items
String menuItems[8] = {"Jog", "Change Speed", "Change Accel", "Home", "Reset Home", "Save Position", "Go to Pos", "Go to Saved Pos"};
bool inSubMenu = false;           // Flag for sub-menu
SubMenu subMenuType = NONE;       // Type of sub-menu
long inputValue = 0;              // For numeric input in sub-menus
bool inputDirection = true;       // For jog direction
int lastKey = -1;                 // Last key pressed
unsigned long lastKeyTime = 0;    // Time of last key press
const unsigned long debounceDelay = 200; // Debounce delay in ms

// Step 3.3: AccelStepper object
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

// Forward declarations
void enterSubMenu();
void executeMenuAction();

// Step 4: Setup function - runs once at startup
void setup() {
  // Step 4.1: Set pin modes
  pinMode(ENABLE_PIN, OUTPUT);
  pinMode(HOME_SWITCH_PIN, INPUT_PULLUP);  // Home switch with internal pull-up

  // Step 4.2: Configure microstepping to 1/128 (as per DM556 driver setup)
  // DM556 microstep settings: MS1=0, MS2=1, MS3=1 for 1/128
  //  digitalWrite(MS1_PIN, LOW);
  //  digitalWrite(MS2_PIN, HIGH);
  //  digitalWrite(MS3_PIN, HIGH);

  // Step 4.3: Enable the driver (ENABLE is active low, so set to LOW)
  digitalWrite(ENABLE_PIN, HIGH);

  // Step 4.4: Configure AccelStepper
  stepper.setMaxSpeed(MAX_SPEED);
  stepper.setAcceleration(ACCELERATION);
  stepper.setCurrentPosition(0);  // Start at position 0

  // Optional: Initialize serial for debugging
  Serial.begin(115200);
  Serial.println("Stepper Motor Controller with AccelStepper Initialized");
  Serial.println("Available Serial Commands:");
  Serial.println("- JOG F <steps>: Jog forward (clockwise) by <steps> steps");
  Serial.println("- JOG B <steps>: Jog backward (counterclockwise) by <steps> steps");
  Serial.println("- MOVE_TO <position>: Move to absolute position <position>");
  Serial.println("- HOME: Move to home position (0)");
  Serial.println("- RESET_HOME: Set current position as new home");
  Serial.println("- SAVE_POS <num>: Save current position to slot <num> (0-4)");
  Serial.println("- LOAD_POS <num>: Load position from slot <num>");
  Serial.println("- GET_POS: Print current position");
  Serial.println("- FIND_HOME: Find home using limit switch");
  Serial.println("- TEST <steps>: Start continuous test with <steps> steps");
  Serial.println("- STOP: Stop the test");
  Serial.println("- SET_STEPS <value>: Set steps per revolution and save to memory");
  Serial.println("- SET_MAX_SPEED <value>: Set maximum speed and save to memory");
  Serial.println("- SET_ACCELERATION <value>: Set acceleration and save to memory");

  // Step 4.5: Initialize preferences for position storage
  preferences.begin("stepper", false);  // Namespace "stepper", read-write mode
  homePosition = preferences.getLong("homePos", 0);
  currentPosition = preferences.getLong("currPos", 0);
  STEPS_PER_REV = preferences.getInt("stepsPerRev", 200);  // Load steps per revolution
  MAX_SPEED = preferences.getFloat("maxSpeed", 3000.0);  // Load max speed
  ACCELERATION = preferences.getFloat("acceleration", 1000.0);  // Load acceleration
  stepper.setMaxSpeed(MAX_SPEED);
  stepper.setAcceleration(ACCELERATION);
  stepper.setCurrentPosition(currentPosition);
  for (int i = 0; i < 5; i++) {
    savedPositions[i] = preferences.getLong(("pos" + String(i)).c_str(), 0);
  }
  Serial.print("Home position loaded: ");
  Serial.println(homePosition);
  Serial.print("Current position loaded: ");
  Serial.println(currentPosition);
  Serial.print("Steps per revolution loaded: ");
  Serial.println(STEPS_PER_REV);
  Serial.println("Saved positions loaded");

  // Step 4.6: Initialize LCD
  Wire.begin(25, 26);  // SDA=25, SCL=26 for ESP32
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("PizzaController");
  lcd.setCursor(0, 1);
  lcd.print("Ready");
  delay(2000);
  updateMenuDisplay();
}

// Step 5: Loop function - runs repeatedly
void loop() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    processCommand(command);
  }

  // Handle menu input
  handleMenu();

  // Run stepper
  stepper.run();

  // Run test function if active
  if (isTesting) {
    runTestAccel();
  }
}

// Process serial commands
void processCommand(String command) {
  if (command.startsWith("JOG F ")) {
    long steps = command.substring(6).toInt();
    jog(steps, true);  // Forward (clockwise)
  } else if (command.startsWith("JOG B ")) {
    long steps = command.substring(6).toInt();
    jog(steps, false);  // Backward (counterclockwise)
  } else if (command.startsWith("MOVE_TO ")) {
    long position = command.substring(8).toInt();
    moveToPosition(position);
  } else if (command == "HOME") {
    home();
  } else if (command == "RESET_HOME") {
    resetHome();
  } else if (command.startsWith("SAVE_POS ")) {
    int num = command.substring(9).toInt();
    savePosition(num);
  } else if (command.startsWith("LOAD_POS ")) {
    int num = command.substring(9).toInt();
    loadPosition(num);
  } else if (command == "GET_POS") {
    Serial.println(currentPosition);
  } else if (command == "FIND_HOME") {
    findHome();
  } else if (command.startsWith("TEST ")) {
    long steps = command.substring(5).toInt();
    startTestAccel(steps);
  } else if (command == "STOP") {
    stopTestAccel();
  } else if (command.startsWith("RUN_TEST ")) {
    long steps = command.substring(9).toInt();
    startTestAccel(steps);
  } else if (command.startsWith("SET_STEPS ")) {
    int value = command.substring(10).toInt();
    setStepsPerRev(value);
  } else if (command.startsWith("SET_MAX_SPEED ")) {
    float value = command.substring(14).toFloat();
    setMaxSpeed(value);
  } else if (command.startsWith("SET_ACCELERATION ")) {
    float value = command.substring(17).toFloat();
    setAcceleration(value);
  } else {
    Serial.println("Unknown command");
  }
}

// Step 6: Advanced control functions

// Function to move a specific number of steps in a given direction
// direction: true for clockwise, false for counterclockwise
void moveSteps(long steps, bool direction) {
  digitalWrite(ENABLE_PIN, LOW);  // Enable driver before moving
  long targetPosition = currentPosition + (direction ? steps : -steps);
  stepper.moveTo(targetPosition);
  // Wait for movement to complete
  while (stepper.isRunning()) {
    stepper.run();
  }
  currentPosition = stepper.currentPosition();
  preferences.putLong("currPos", currentPosition);  // Save current position after every move
  Serial.print("Moved to position: ");
  Serial.println(currentPosition);
  digitalWrite(ENABLE_PIN, HIGH);  // Disable driver after moving
}

// Jog function: Move N steps in specified direction
// direction: true for clockwise, false for counterclockwise
void jog(long steps, bool direction) {
  Serial.print("Jogging ");
  Serial.print(steps);
  Serial.println(direction ? " steps clockwise" : " steps counterclockwise");
  moveSteps(steps, direction);
  Serial.println(currentPosition);
}

// Save current position to non-volatile memory
void savePosition() {
  preferences.putLong("currPos", currentPosition);
  preferences.putLong("homePos", homePosition);
  Serial.print("Position saved: ");
  Serial.println(currentPosition);
}

// Save position to a numbered slot
void savePosition(int num) {
  if (num >= 0 && num < 5) {
    savedPositions[num] = currentPosition;
    preferences.putLong(("pos" + String(num)).c_str(), currentPosition);
    Serial.print("position ");
    Serial.print(num);
    Serial.print(" saved, ");
    Serial.println(currentPosition);
  }
}

// Load position from a numbered slot
void loadPosition(int num) {
  if (num >= 0 && num < 5) {
    long targetPos = savedPositions[num];
    moveToPosition(targetPos);
  }
}

// Move to absolute position
void moveToPosition(long targetPosition) {
  Serial.print("Moving to absolute position ");
  Serial.println(targetPosition);
  digitalWrite(ENABLE_PIN, LOW);  // Enable driver before moving
  stepper.moveTo(targetPosition);
  while (stepper.isRunning()) {
    stepper.run();
  }
  digitalWrite(ENABLE_PIN, HIGH);  // Disable driver after moving
  currentPosition = stepper.currentPosition();
  preferences.putLong("currPos", currentPosition);
  Serial.println(currentPosition);
}

// Home function: Move to home position (position 0)
// Assumes home is at current position when called, or implement limit switch logic
void home() {
  long stepsToHome = -currentPosition;  // Steps needed to reach home (0)
  Serial.print("Homing: moving ");
  Serial.print(abs(stepsToHome));
  Serial.println(stepsToHome > 0 ? " steps clockwise" : " steps counterclockwise");
  digitalWrite(ENABLE_PIN, LOW);  // Enable driver before moving
  stepper.moveTo(0);
  while (stepper.isRunning()) {
    stepper.run();
  }
  digitalWrite(ENABLE_PIN, HIGH);  // Disable driver after moving
  currentPosition = 0;  // Set current position to home
  stepper.setCurrentPosition(0);
  savePosition();
  Serial.println("Homed to position 0");
  Serial.println(currentPosition);
}

// Reset home function: Set current position as new home (position 0)
void resetHome() {
  homePosition = currentPosition;
  currentPosition = 0;
  stepper.setCurrentPosition(0);
  preferences.putLong("homePos", homePosition);
  preferences.putLong("currPos", currentPosition);
  Serial.print("Home reset. New home offset: ");
  Serial.println(homePosition);
  Serial.println("Current position set to 0");
  Serial.println(currentPosition);
}

// Find home using limit switch
void findHome() {
  Serial.println("Finding home using limit switch...");
  // Move in the home direction (assuming counterclockwise is towards home)
  // Adjust direction based on your setup: true for clockwise, false for counterclockwise
  bool homeDirection = false;  // Change to true if clockwise is towards home

  digitalWrite(ENABLE_PIN, LOW);  // Enable driver before moving
  stepper.setSpeed(homeDirection ? MAX_SPEED : -MAX_SPEED);

  // Move until limit switch is triggered
  while (digitalRead(HOME_SWITCH_PIN) == HIGH) {  // Assuming active low switch
    stepper.runSpeed();
  }

  digitalWrite(ENABLE_PIN, HIGH);  // Disable driver after moving to prevent overheating

  // Stop and set position to 0
  stepper.stop();
  stepper.setCurrentPosition(0);
  currentPosition = 0;
  preferences.putLong("currPos", currentPosition);
  Serial.println("Home found and set to position 0");
  Serial.println(currentPosition);
}

// Get current position relative to home
long getCurrentPosition() {
  return currentPosition;
}

// Set steps per revolution and save to memory
void setStepsPerRev(int value) {
  if (value > 0) {
    STEPS_PER_REV = value;
    preferences.putInt("stepsPerRev", STEPS_PER_REV);
    Serial.print("Steps per revolution set to: ");
    Serial.println(STEPS_PER_REV);
  } else {
    Serial.println("Invalid value for steps per revolution");
  }
}

// Set maximum speed and save to memory
void setMaxSpeed(float value) {
  if (value > 0) {
    MAX_SPEED = value;
    preferences.putFloat("maxSpeed", MAX_SPEED);
    stepper.setMaxSpeed(MAX_SPEED);
    Serial.print("Maximum speed set to: ");
    Serial.println(MAX_SPEED);
  } else {
    Serial.println("Invalid value for maximum speed");
  }
}

// Set acceleration and save to memory
void setAcceleration(float value) {
  if (value > 0) {
    ACCELERATION = value;
    preferences.putFloat("acceleration", ACCELERATION);
    stepper.setAcceleration(ACCELERATION);
    Serial.print("Acceleration set to: ");
    Serial.println(ACCELERATION);
  } else {
    Serial.println("Invalid value for acceleration");
  }
}

// Example usage functions (call these from loop() or serial commands)
void demoJog() {
  // Jog 1000 steps clockwise
  jog(1000, true);
  delay(1000);

  // Jog 1000 steps counterclockwise
  jog(1000, false);
  delay(1000);
}

void demoSaveAndHome() {
  // Move to a position
  moveSteps(5000, true);
  delay(1000);

  // Save position
  savePosition();
  delay(1000);

  // Home
  home();
  delay(1000);

  // Reset home at current position
  resetHome();
}

// Test function: Goes forward N steps and then backwards N steps, stops only when STOP command is received
void startTestAccel(long steps) {
  if (steps > 0) {
    testSteps = steps;
    isTesting = true;
    Serial.print("Starting test with ");
    Serial.print(testSteps);
    Serial.println(" steps");
  } else {
    Serial.println("Invalid number of steps for test");
  }
}

void stopTestAccel() {
  isTesting = false;
  stepper.stop();
  Serial.println("Test stopped");
}

void runTestAccel() {
  static bool direction = true;  // true = forward (clockwise), false = backward (counterclockwise)

  if (!stepper.isRunning()) {
    // Calculate target position
    // long targetPosition = direction ? testSteps : -testSteps;
    long targetPosition = testSteps;
    moveSteps(targetPosition,direction);
    // Toggle direction for next cycle
    direction = !direction;
    Serial.print("Changing direction to ");
    Serial.println(direction ? "forward" : "backward");
    delay(500);
    Serial.println(currentPosition);
    delay(500);
  }
}

// Step 7: LCD and Keypad functions

// Function to read keypad

int readKeypad() {
  int adcValue = analogRead(KEYPAD_PIN);
  if (adcValue < 100) return 4; // Left
  if (adcValue < 800) return 2; // Up
  if (adcValue < 1800) return 3; // Down
  if (adcValue < 2700) return 1; // Right
  if (adcValue < 4000) return 5; // Select
  if (adcValue > 4000) return 0; // No button
  return 0; // No button
}
// Function to update LCD menu display
void updateMenuDisplay() {
  lcd.clear();
  if (!inSubMenu) {
    lcd.setCursor(0, 0);
    lcd.print("Pos:");
    lcd.print(currentPosition);
    lcd.setCursor(0, 1);
    lcd.print(menuItems[menuIndex]);
  } else {
    switch (subMenuType) {
      case JOG: // Jog
        lcd.setCursor(0, 0);
        lcd.print("Jog: ");
        lcd.print(inputValue);
        lcd.setCursor(0, 1);
        lcd.print("L:< R:> Sel:EXIT");
        break;
      case SPEED: // Speed
        lcd.setCursor(0, 0);
        lcd.print("Max Speed:");
        lcd.setCursor(0, 1);
        lcd.print(inputValue);
        break;
      case ACCEL: // Accel
        lcd.setCursor(0, 0);
        lcd.print("Acceleration:");
        lcd.setCursor(0, 1);
        lcd.print(inputValue);
        break;
      case SAVE_POS: // Save
        lcd.setCursor(0, 0);
        lcd.print("Slot ");
        lcd.print(inputValue+1);
        lcd.print(": ");
        lcd.print(savedPositions[inputValue]);
        lcd.setCursor(0, 1);
        lcd.print("Select to Save");
        break;
      case GOTO: // Goto
        lcd.setCursor(0, 0);
        lcd.print("Go to Pos:");
        lcd.setCursor(0, 1);
        lcd.print(inputValue);
        break;
      case GOTO_SAVED: // Goto Saved
        lcd.setCursor(0, 0);
        lcd.print("Slot ");
        lcd.print(inputValue+1);
        lcd.print(": ");
        lcd.print(savedPositions[inputValue]);
        lcd.setCursor(0, 1);
        lcd.print("Select to Load");
        break;
      case CONFIRM_RESET_HOME: // Confirm Reset Home
        lcd.setCursor(0, 0);
        lcd.print("Reset Home?");
        lcd.setCursor(0, 1);
        lcd.print("Sel:Yes Any:No");
        delay(1000);
        break;
      case CONFIRM_SAVE_POS: // Confirm Save Position
        lcd.setCursor(0, 0);
        lcd.print("Save to Slot ");
        lcd.print(inputValue+1);
        lcd.print("?");
        lcd.setCursor(0, 1);
        lcd.print("Sel:Yes Any:No");
        delay(1000);
        break;
    }
  }
}

// Function to handle menu navigation and input
void handleMenu() {
  int key = readKeypad();
  unsigned long currentTime = millis();

  if (key != lastKey && currentTime - lastKeyTime > debounceDelay) {
    lastKey = key;
    lastKeyTime = currentTime;

    // Print button press to serial
    String keyName = "";
    switch(key) {
      case 1: keyName = "Right"; break;
      case 2: keyName = "Up"; break;
      case 3: keyName = "Down"; break;
      case 4: keyName = "Left"; break;
      case 5: keyName = "Select"; break;
    }
    Serial.print("Key pressed: ");
    Serial.println(keyName);

    if (!inSubMenu) {
      switch (key) {
        case 2: // Up
          menuIndex = (menuIndex - 1 + menuSize) % menuSize;
          updateMenuDisplay();
          break;
        case 3: // Down
          menuIndex = (menuIndex + 1) % menuSize;
          updateMenuDisplay();
          break;
        case 5: // Select
          if (menuIndex == 3) { // Home
            home();
            updateMenuDisplay();
          } else if (menuIndex == 4) { // Reset Home
            inSubMenu = true;
            subMenuType = CONFIRM_RESET_HOME;
            updateMenuDisplay();
          } else {
            enterSubMenu();
          }
          break;
      }
    } else {
      if (subMenuType == JOG) { // Special handling for Jog submenu
        switch (key) {
          case 1: // Right - Jog forward
            jog(inputValue, true);
            break;
          case 4: // Left - Jog backward
            jog(inputValue, false);
            break;
          case 2: // Up
            inputValue += 20;
            updateMenuDisplay();
            break;
          case 3: // Down
            inputValue -= 20;
            if (inputValue < 0 ) inputValue = 0;
            updateMenuDisplay();
            break;
          case 5: // Select - Exit to main menu
            inSubMenu = false;
            subMenuType = NONE;
            updateMenuDisplay();
            break;
          // Up and Down do nothing
        }
      } else if (subMenuType == GOTO_SAVED || subMenuType == SAVE_POS){
        switch (key) {
          case 1: // Right
            inputValue += 1;
            if (inputValue > 4) inputValue = 4;
            updateMenuDisplay();
            break;
          case 4: // Left
            inputValue -= 1;
            if (inputValue < 0) inputValue = 0;
            updateMenuDisplay();
            break;
          case 5: // Select - Exit to main menu
            executeMenuAction();
            break;
          // Up and Down do nothing
        }

        } else if (subMenuType == CONFIRM_RESET_HOME || subMenuType == CONFIRM_SAVE_POS) {
        switch (key) {
          case 5: // Select - Confirm action
            if (subMenuType == CONFIRM_RESET_HOME) {
              resetHome();
            } else if (subMenuType == CONFIRM_SAVE_POS) {
              savePosition(inputValue);
            }
            inSubMenu = false;
            subMenuType = NONE;
            updateMenuDisplay();
            break;
          default: // Any other key - Cancel
            inSubMenu = false;
            subMenuType = NONE;
            updateMenuDisplay();
            break;
        }
      } else {
        switch (key) {
          case 1: // Right           
            inputValue += 10;   
            updateMenuDisplay();
            break;
          case 4: // Left
            inputValue -= 10;
            updateMenuDisplay();
            break;
          case 2: // Up         
            inputValue += 100;
            updateMenuDisplay();
            break;
          case 3: // Down
            inputValue -= 100;           
            updateMenuDisplay();
            break;
          case 5: // Select
            executeMenuAction();
            break;
        }
      }
    }
  } else if (key == 0) {
    lastKey = -1;
  }
}

// Function to enter sub-menu
void enterSubMenu() {
  inSubMenu = true;
  subMenuType = (SubMenu)(menuIndex + 1);
  if (subMenuType == SPEED) { // Speed
    inputValue = (long)MAX_SPEED;
  } else if (subMenuType == ACCEL) { // Accel
    inputValue = (long)ACCELERATION;
  } else if (subMenuType == JOG) { // Jog
    inputValue = 20;
  } else {
    inputValue = 0;
  }
  updateMenuDisplay();
}

// Function to execute menu action
void executeMenuAction() {
  switch (subMenuType) {
    case JOG: // Jog
      jog(inputValue, inputDirection);
      break;
    case SPEED: // Speed
      setMaxSpeed(inputValue);
      break;
    case ACCEL: // Accel
      setAcceleration(inputValue);
      break;
    case SAVE_POS: // Save
      inSubMenu = true;
      subMenuType = CONFIRM_SAVE_POS;
      updateMenuDisplay();
      break;
    case GOTO: // Goto Position
      moveToPosition(inputValue);
      break;
    case GOTO_SAVED: // Goto Saved
      loadPosition(inputValue);
      break;
  }
  inSubMenu = false;
  subMenuType = NONE;
  updateMenuDisplay();
}
