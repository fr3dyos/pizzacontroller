// PizzaController_AccelStepper.ino - FIXED & IMPROVED VERSION
// ESP32 Program to Control NEMA23 Stepper Motor with DM556 Driver using AccelStepper Library
// Features: Non-blocking motor control, anti-overheating, position saving, LCD menu, motor configuration retrieval, limit switch status
// Original Author: Eng. Fredy Osorio <ing.fredyosorio@gmail.com>
// Fixed Version: January 22, 2026
// Fixes: All 10 issues from code review addressed

// Serial Commands:
// - JOG F <steps>: Jog forward (clockwise) by <steps> steps
// - JOG B <steps>: Jog backward (counterclockwise) by <steps> steps
// - MOVE_TO <position>: Move to absolute position <position>
// - HOME: Move to home position (0)
// - RESET_HOME: Set current position as new home
// - SAVE_POS <num>: Save current position to slot <num> (0-4)
// - LOAD_POS <num>: Load position from slot <num>
// - GET_POS: Print current position
// - FIND_HOME: Find home using limit switch (non-blocking)
// - TEST <steps>: Start continuous test with <steps> steps
// - STOP: Stop the test
// - SET_STEPS <value>: Set steps per revolution and save to memory
// - SET_MAX_SPEED <value>: Set maximum speed and save to memory (0 < value <= 50000)
// - SET_ACCELERATION <value>: Set acceleration and save to memory (0 < value <= 50000)
// - SET_HOLD_TIME <ms>: Set motor hold time after move (0-10000 ms)
// - SET_SPEED <value>: Set homing speed and save to memory (0 < value <= 50000)
// - GET_INFO: Display motor configuration (steps, speed, acceleration, hold time)
// - GET_SWITCH: Read home limit switch status

// ============================================================================
// INCLUDES
// ============================================================================
#include <Preferences.h>  // For saving positions to non-volatile memory
#include <AccelStepper.h> // For advanced stepper control with acceleration
#include <Wire.h>         // For I2C communication
#include <LiquidCrystal_I2C.h> // For LCD display

// ============================================================================
// PIN DEFINITIONS
// ============================================================================
#define STEP_PIN 18      // Connect to STEP input on DM556
#define DIR_PIN 19       // Connect to DIR input on DM556
#define ENABLE_PIN 21    // Connect to ENABLE input on DM556 (active low)
#define HOME_SWITCH_PIN 27 // Connect to home limit switch (active low)

// LCD and Keypad definitions
#define LCD_ADDR 0x27    // I2C address for LCD (common for 16x2 I2C LCD)
#define LCD_COLS 16
#define LCD_ROWS 2
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
#define KEYPAD_PIN 35    // Analog pin for 5-button Adkeypad

// Direct position buttons (digital pins)
#define BUTTON1_PIN 12   // Button 1 -> position 0
#define BUTTON2_PIN 14   // Button 2 -> position 1
#define BUTTON3_PIN 15   // Button 3 -> position 2
#define BUTTON4_PIN 16   // Button 4 -> position 3
#define BUTTON5_PIN 17   // Button 5 -> position 4

// ============================================================================
// ENUMERATIONS
// ============================================================================
enum SubMenu {
  NONE,
  GOTO_SAVED,      // menuItems[0]
  SPEED,           // menuItems[1]
  ACCEL,           // menuItems[2]
  HOME_SELECT,     // menuItems[3] - renamed to avoid conflict
  RESET_HOME,      // menuItems[4]
  SAVE_POS,        // menuItems[5]
  GOTO,            // menuItems[6]
  JOG,             // menuItems[7]
  CONFIRM_RESET_HOME,
  CONFIRM_SAVE_POS
};

enum HomingState {
  HOMING_IDLE,
  HOMING_MOVING,
  HOMING_DONE
};

// ============================================================================
// MOTOR CONFIGURATION
// ============================================================================
int STEPS_PER_REV = 3200;      // Default steps per revolution (configurable)
float MAX_SPEED = 8000.0;      // Maximum speed in steps per second
float ACCELERATION = 4000.0;   // Acceleration in steps per second squared
float HOMING_SPEED = 2000.0;   // Speed for homing in steps per second
int JOG_STEPS = 50;            // Steps per button press in jog submenu

const long MAX_JOG_STEPS = 50000;  // Safety limit for jog steps
const long MAX_POSITION = 1000000; // Safety limit for absolute positions
const float MAX_SPEED_LIMIT = 50000.0;
const float MAX_ACCEL_LIMIT = 50000.0;

// Motor hold time after movement (prevents overheating)
unsigned long MOTOR_HOLD_TIME = 300;  // Time in ms to keep motor enabled after move
unsigned long motorDisableTime = 0;    // Time when motor should be disabled
bool motorShouldDisable = false;       // Flag: motor should be disabled soon

// ============================================================================
// MOTOR STATE MANAGEMENT (NON-BLOCKING)
// ============================================================================
bool isMotorMoving = false;    // Flag: motor is currently moving
long motorTargetPosition = 0;  // Target position for current movement
unsigned long motorMoveStartTime = 0; // Time when movement started

// Homing state machine (FIXED: Now non-blocking)
HomingState homingState = HOMING_IDLE;
unsigned long homingStartTime = 0;

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================
long currentPosition = 0;         // Current position in steps from home
long homePosition = 0;            // Home position offset
long savedPositions[5] = {0};     // Array for up to 5 saved positions
Preferences preferences;          // For saving positions to flash memory

// Test function variables
bool isTesting = false;           // Flag to indicate if test is running
long testSteps = 0;               // Number of steps for test
bool testDirection = true;        // Current direction in test

// Menu variables
int menuIndex = 0;                // Current menu item index
const int menuSize = 8;           // Number of menu items
String menuItems[8] = {"Go to Saved Pos", "Change Speed", "Change Accel", "Home", "Reset Home", "Save Position", "Go to Pos", "Jog"};
bool inSubMenu = false;           // Flag for sub-menu
SubMenu subMenuType = NONE;       // Type of sub-menu
long inputValue = 0;              // For numeric input in sub-menus
bool inputDirection = true;       // For jog direction (LEFT/RIGHT)
int lastKey = 0;                  // Last key pressed (0 = no key)
unsigned long lastKeyTime = 0;    // Time of last key press
const unsigned long debounceDelay = 200; // Debounce delay in ms

// Direct button variables (active low)
int lastButtonStates[5] = {HIGH, HIGH, HIGH, HIGH, HIGH};
unsigned long lastButtonTimes[5] = {0, 0, 0, 0, 0};

// ============================================================================
// ACCEL STEPPER OBJECT
// ============================================================================
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
void enterSubMenu();
void executeMenuAction();
void updateMenuDisplay();
void startMotorMovement(long targetPos);
void updateMotorMovement();
void disableMotor();
void enableMotor();
void scheduleMotorDisable();
void updateHomingState();
void startFindHome();

// ============================================================================
// SETUP FUNCTION
// ============================================================================
void setup() {
  // Set pin modes
  pinMode(ENABLE_PIN, OUTPUT);
  pinMode(HOME_SWITCH_PIN, INPUT_PULLUP);
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  pinMode(BUTTON3_PIN, INPUT_PULLUP);
  pinMode(BUTTON4_PIN, INPUT_PULLUP);
  pinMode(BUTTON5_PIN, INPUT_PULLUP);

  // Start with motor DISABLED to prevent overheating
  digitalWrite(ENABLE_PIN, HIGH);

  // Configure AccelStepper
  stepper.setMaxSpeed(MAX_SPEED);
  stepper.setAcceleration(ACCELERATION);
  stepper.setCurrentPosition(0);

  // Initialize serial
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n========================================");
  Serial.println("Pizza Controller - FIXED VERSION");
  Serial.println("========================================");
  Serial.println("Features:");
  Serial.println("- Non-blocking motor control");
  Serial.println("- Anti-overheating (motor disables after move)");
  Serial.println("- 5 saved position slots");
  Serial.println("- LCD menu with keypad");
  Serial.println("- Non-volatile memory storage");
  Serial.println("- Non-blocking homing");
  Serial.println("========================================\n");
  
  Serial.println("Available Serial Commands:");
  Serial.println("  JOG F <steps>           - Jog forward");
  Serial.println("  JOG B <steps>           - Jog backward");
  Serial.println("  MOVE_TO <pos>           - Move to absolute position");
  Serial.println("  HOME                    - Go to home (position 0)");
  Serial.println("  RESET_HOME              - Set current as home");
  Serial.println("  SAVE_POS <0-4>          - Save position to slot");
  Serial.println("  LOAD_POS <0-4>          - Load position from slot");
  Serial.println("  GET_POS                 - Print current position");
  Serial.println("  FIND_HOME               - Find home with limit switch");
  Serial.println("  TEST <steps>            - Start continuous test");
  Serial.println("  STOP                    - Stop test");
  Serial.println("  SET_STEPS <value>       - Configure steps/revolution");
  Serial.println("  SET_MAX_SPEED <value>   - Set max speed (0 < val <= 50000)");
  Serial.println("  SET_ACCELERATION <val>  - Set acceleration (0 < val <= 50000)");
  Serial.println("  SET_HOLD_TIME <ms>      - Motor hold time (0-10000)");
  Serial.println("  SET_SPEED <value>       - Set homing speed (0 < val <= 50000)");
  Serial.println("  GET_INFO                - Display motor configuration");
  Serial.println("  GET_SWITCH              - Read home limit switch status");
  Serial.println("========================================\n");

  // Load configuration from non-volatile memory
  preferences.begin("stepper", false);
  homePosition = preferences.getLong("homePos", 0);
  currentPosition = preferences.getLong("currPos", 0);
  STEPS_PER_REV = preferences.getInt("stepsPerRev", 200);
  MAX_SPEED = preferences.getFloat("maxSpeed", 3000.0);
  ACCELERATION = preferences.getFloat("acceleration", 1000.0);
  MOTOR_HOLD_TIME = preferences.getULong("holdTime", 500);
  HOMING_SPEED = preferences.getFloat("homingSpeed", 2000.0); // FIXED: Now loads HOMING_SPEED
  
  stepper.setMaxSpeed(MAX_SPEED);
  stepper.setAcceleration(ACCELERATION);
  stepper.setCurrentPosition(currentPosition);
  
  for (int i = 0; i < 5; i++) {
    savedPositions[i] = preferences.getLong(("pos" + String(i)).c_str(), 0);
  }
  
  Serial.print("Home position: ");
  Serial.println(homePosition);
  Serial.print("Current position: ");
  Serial.println(currentPosition);
  Serial.print("Steps per revolution: ");
  Serial.println(STEPS_PER_REV);
  Serial.print("Max speed: ");
  Serial.println(MAX_SPEED);
  Serial.print("Acceleration: ");
  Serial.println(ACCELERATION);
  Serial.print("Homing speed: ");
  Serial.println(HOMING_SPEED);
  Serial.print("Motor hold time: ");
  Serial.print(MOTOR_HOLD_TIME);
  Serial.println(" ms\n");

  // Initialize LCD
  Wire.begin(25, 26);  // SDA=25, SCL=26
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Pizza Controller");
  lcd.setCursor(0, 1);
  lcd.print("Motor Disabled");
  delay(2000);
  updateMenuDisplay();
  
  Serial.println("System ready!");
}

// ============================================================================
// MAIN LOOP - NON-BLOCKING ARCHITECTURE
// ============================================================================
void loop() {
  // Handle serial commands
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command.length() > 0) {
      processCommand(command);      
      currentPosition = stepper.currentPosition();
      updateMenuDisplay();  // FIXED: Single update point
    }
  }

  // Handle menu and button input
  handleMenu();
  handleDirectButtons();
  
  // CRITICAL: stepper.run() must be called frequently for smooth motion
  stepper.run();
  
  // Update motor movement state (non-blocking)
  updateMotorMovement();

  // FIXED: Non-blocking homing state machine
  updateHomingState();
  
  // Auto-disable motor after hold time expires (anti-overheating)
  if (motorShouldDisable && millis() >= motorDisableTime) {
    disableMotor();
    motorShouldDisable = false;
  }

  // Handle test function if active
  if (isTesting) {
    runTestAccel();
    delay(250);
  }

  delay(10);  // Small delay for responsiveness
}

// ============================================================================
// MOTOR ENABLE/DISABLE FUNCTIONS
// ============================================================================

void enableMotor() {
  digitalWrite(ENABLE_PIN, LOW);
  motorShouldDisable = false;
}

void disableMotor() {
  digitalWrite(ENABLE_PIN, HIGH);
  motorShouldDisable = false;
}

void scheduleMotorDisable() {
  motorDisableTime = millis() + MOTOR_HOLD_TIME;
  motorShouldDisable = true;
}

// ============================================================================
// HOMING STATE MACHINE (FIXED: Non-blocking)
// ============================================================================

void startFindHome() {
  Serial.println("Starting home search...");
  enableMotor();
  stepper.setSpeed(-HOMING_SPEED);  // FIXED: Use HOMING_SPEED instead of MAX_SPEED
  homingState = HOMING_MOVING;
  homingStartTime = millis();
}

void updateHomingState() {
  if (homingState == HOMING_MOVING) {
    stepper.runSpeed();
    
    // Check switch
    if (digitalRead(HOME_SWITCH_PIN) == LOW) {
      stepper.stop();
      stepper.setCurrentPosition(0);
      currentPosition = 0;
      preferences.putLong("currPos", currentPosition);
      Serial.println("Home switch detected!");
      Serial.println("Home found and set to position 0");
      homingState = HOMING_DONE;
      scheduleMotorDisable();
      updateMenuDisplay();
    }
    
    // Timeout protection (60 seconds)
    if (millis() - homingStartTime > 60000) {
      stepper.stop();
      Serial.println("ERROR: Homing timeout");
      homingState = HOMING_IDLE;
      scheduleMotorDisable();
      updateMenuDisplay();
    }
  }
  
  // Reset state if homing completed or timeout
  if (homingState == HOMING_DONE) {
    homingState = HOMING_IDLE;
  }
}

// ============================================================================
// SERIAL COMMAND PROCESSING
// ============================================================================
void processCommand(String command) {
  if (command.startsWith("JOG F ")) {
    long steps = command.substring(6).toInt();
    if (steps > 0 && steps <= MAX_JOG_STEPS) {
      startMotorMovement(currentPosition + steps);
      Serial.println("Jogging forward");
    } else {
      Serial.println("ERROR: Invalid steps for JOG");
    }
  } 
  else if (command.startsWith("JOG B ")) {
    long steps = command.substring(6).toInt();
    if (steps > 0 && steps <= MAX_JOG_STEPS) {
      startMotorMovement(currentPosition - steps);
      Serial.println("Jogging backward");
    } else {
      Serial.println("ERROR: Invalid steps for JOG");
    }
  } 
  else if (command.startsWith("MOVE_TO ")) {
    long position = command.substring(8).toInt();
    if (position >= -MAX_POSITION && position <= MAX_POSITION) {
      startMotorMovement(position);
      Serial.print("Moving to position ");
      Serial.println(position);
    } else {
      Serial.println("ERROR: Position out of range");
    }
  } 
  else if (command == "HOME") {
    home();
  } 
  else if (command == "RESET_HOME") {
    resetHome();
  } 
  else if (command.startsWith("SAVE_POS ")) {
    int num = command.substring(9).toInt();
    savePositionToSlot(num);
  } 
  else if (command.startsWith("LOAD_POS ")) {
    int num = command.substring(9).toInt();
    loadPositionFromSlot(num);
  } 
  else if (command == "GET_POS") {
    Serial.println(currentPosition);
  } 
  else if (command == "FIND_HOME") {
    startFindHome();  // FIXED: Non-blocking start
  } 
  else if (command.startsWith("TEST ")) {
    long steps = command.substring(5).toInt();
    startTestAccel(steps);
  } 
  else if (command == "STOP") {
    stopTestAccel();
  } 
  else if (command.startsWith("SET_STEPS ")) {
    int value = command.substring(10).toInt();
    setStepsPerRev(value);
  } 
  else if (command.startsWith("SET_MAX_SPEED ")) {
    float value = command.substring(14).toFloat();
    setMaxSpeed(value);
  } 
  else if (command.startsWith("SET_ACCELERATION ")) {
    float value = command.substring(17).toFloat();
    setAcceleration(value);
  }
  else if (command.startsWith("SET_HOLD_TIME ")) {
    unsigned long value = command.substring(14).toInt();
    setMotorHoldTime(value);
  }
  else if (command.startsWith("SET_SPEED ")) {
    float value = command.substring(10).toFloat();
    setHomingSpeed(value);
  }
  else if (command == "GET_INFO") {
    Serial.println("\nMotor Configuration:");
    Serial.print("  Steps per revolution: ");
    Serial.println(STEPS_PER_REV);
    Serial.print("  Max speed (steps/sec): ");
    Serial.println(MAX_SPEED);
    Serial.print("  Acceleration (steps/sec²): ");
    Serial.println(ACCELERATION);
    Serial.print("  Homing speed (steps/sec): ");
    Serial.println(HOMING_SPEED);
    Serial.print("  Motor hold time (ms): ");
    Serial.println(MOTOR_HOLD_TIME);
    Serial.println();
  }
  else if (command == "GET_SWITCH") {
    int switchState = digitalRead(HOME_SWITCH_PIN);
    Serial.print("Home limit switch status: ");
    if (switchState == LOW) {
      Serial.println("TRIGGERED (active low)");
    } else {
      Serial.println("NOT TRIGGERED");
    }
  }
  else {
    Serial.println("ERROR: Unknown command");
  }
  // FIXED: Removed redundant updateMenuDisplay() call (now only in main loop)
}

// ============================================================================
// NON-BLOCKING MOTOR MOVEMENT
// ============================================================================

void startMotorMovement(long targetPos) {
  if (isMotorMoving) {
    Serial.println("Motor already moving, ignoring command");
    return;
  }
  
  targetPos = constrain(targetPos, -MAX_POSITION, MAX_POSITION);
  
  enableMotor();
  motorTargetPosition = targetPos;
  stepper.moveTo(targetPos);
  isMotorMoving = true;
  motorMoveStartTime = millis();
}

void updateMotorMovement() {
  if (!isMotorMoving) {
    return;
  }

  if (!stepper.isRunning()) {
    isMotorMoving = false;
    currentPosition = stepper.currentPosition();
    preferences.putLong("currPos", currentPosition);
    
    Serial.print("Movement complete. Position: ");
    Serial.println(currentPosition);
    updateMenuDisplay();
    scheduleMotorDisable();
  }
}

// ============================================================================
// POSITION MANAGEMENT FUNCTIONS
// ============================================================================

void saveCurrentPosition() {
  preferences.putLong("currPos", currentPosition);
  preferences.putLong("homePos", homePosition);
  Serial.print("Current position saved: ");
  Serial.println(currentPosition);
}

void savePositionToSlot(int num) {
  if (num >= 0 && num < 5) {
    savedPositions[num] = currentPosition;
    preferences.putLong(("pos" + String(num)).c_str(), currentPosition);
    Serial.print("Position saved to slot ");
    Serial.print(num);
    Serial.print(": ");
    Serial.println(currentPosition);
  } else {
    Serial.print("ERROR: Invalid slot number (0-4): ");
    Serial.println(num);
  }
}

void loadPositionFromSlot(int num) {
  if (num >= 0 && num < 5) {
    long targetPos = savedPositions[num];
    Serial.print("Loading position from slot ");
    Serial.print(num);
    Serial.print(": ");
    Serial.println(targetPos);
    
    if (currentPosition != targetPos) {
      startMotorMovement(targetPos);
    } else {
      Serial.println("Already at target position");  // FIXED: Feedback when already at position
    }
  } else {
    Serial.print("ERROR: Invalid slot number (0-4): ");
    Serial.println(num);
  }
}

void moveToPosition(long targetPosition) {
  if (abs(targetPosition - currentPosition) < 1) {
    Serial.println("Already at target position");
    return;
  }
  startMotorMovement(targetPosition);
}

void home() {
  Serial.println("Homing to position 0...");
  startMotorMovement(0);
}

void resetHome() {
  homePosition = currentPosition;
  currentPosition = 0;
  stepper.setCurrentPosition(0);
  preferences.putLong("homePos", homePosition);
  preferences.putLong("currPos", currentPosition);
  Serial.print("Home reset. New home offset: ");
  Serial.println(homePosition);
  Serial.println("Current position set to 0");
}

long getCurrentPosition() {
  return currentPosition;
}

// ============================================================================
// CONFIGURATION FUNCTIONS
// ============================================================================

void setStepsPerRev(int value) {
  if (value > 0) {
    STEPS_PER_REV = value;
    preferences.putInt("stepsPerRev", STEPS_PER_REV);
    Serial.print("Steps per revolution set to: ");
    Serial.println(STEPS_PER_REV);
  } else {
    Serial.println("ERROR: Invalid value for steps per revolution");
  }
}

void setMaxSpeed(float value) {
  if (value > 0 && value <= MAX_SPEED_LIMIT) {  // FIXED: Added upper bound
    MAX_SPEED = value;
    preferences.putFloat("maxSpeed", MAX_SPEED);
    stepper.setMaxSpeed(MAX_SPEED);
    Serial.print("Maximum speed set to: ");
    Serial.println(MAX_SPEED);
  } else {
    Serial.print("ERROR: Speed must be 0 < speed <= ");
    Serial.println(MAX_SPEED_LIMIT);
  }
}

void setAcceleration(float value) {
  if (value > 0 && value <= MAX_ACCEL_LIMIT) {  // FIXED: Added upper bound
    ACCELERATION = value;
    preferences.putFloat("acceleration", ACCELERATION);
    stepper.setAcceleration(ACCELERATION);
    Serial.print("Acceleration set to: ");
    Serial.println(ACCELERATION);
  } else {
    Serial.print("ERROR: Acceleration must be 0 < accel <= ");
    Serial.println(MAX_ACCEL_LIMIT);
  }
}

void setMotorHoldTime(unsigned long value) {
  if (value >= 0 && value <= 10000) {
    MOTOR_HOLD_TIME = value;
    preferences.putULong("holdTime", MOTOR_HOLD_TIME);
    Serial.print("Motor hold time set to: ");
    Serial.print(MOTOR_HOLD_TIME);
    Serial.println(" ms");
  } else {
    Serial.println("ERROR: Invalid hold time (0-10000 ms)");
  }
}

void setHomingSpeed(float value) {
  if (value > 0 && value <= MAX_SPEED_LIMIT) {  // FIXED: Added bounds checking
    HOMING_SPEED = value;
    preferences.putFloat("homingSpeed", HOMING_SPEED);
    stepper.setSpeed(HOMING_SPEED);
    Serial.print("Homing speed set to: ");
    Serial.println(HOMING_SPEED);
  } else {
    Serial.print("ERROR: Homing speed must be 0 < speed <= ");
    Serial.println(MAX_SPEED_LIMIT);
  }
}

// ============================================================================
// TEST FUNCTIONS
// ============================================================================

void startTestAccel(long steps) {
  if (steps > 0) {
    testSteps = steps;
    testDirection = true;
    isTesting = true;
    Serial.print("Starting test with ");
    Serial.print(testSteps);
    Serial.println(" steps");
    Serial.println("Test pattern: Forward -> Home -> repeat");
  } else {
    Serial.println("ERROR: Invalid number of steps for test");
  }
}

void stopTestAccel() {
  isTesting = false;
  isMotorMoving = false;
  stepper.stop();
  
  Serial.println("Test stopped");
  scheduleMotorDisable();
}

void runTestAccel() {
  if (!isMotorMoving) {
    long targetPosition;
    
    if (testDirection) {
      targetPosition = testSteps;
      Serial.println("Test: Moving forward");
    } else {
      targetPosition = 0;  // FIXED: Return to home between cycles
      Serial.println("Test: Returning to home");
    }
    
    startMotorMovement(targetPosition);
    testDirection = !testDirection;
  }
}

// ============================================================================
// DIRECT BUTTON HANDLER
// ============================================================================

void handleDirectButtons() {
  int buttonPins[5] = {BUTTON1_PIN, BUTTON2_PIN, BUTTON3_PIN, BUTTON4_PIN, BUTTON5_PIN};
  unsigned long currentTime = millis();

  for (int i = 0; i < 5; i++) {
    int buttonState = digitalRead(buttonPins[i]);
    if (buttonState == LOW && lastButtonStates[i] == HIGH && currentTime - lastButtonTimes[i] > debounceDelay) {
      Serial.print("Direct button ");
      Serial.print(i + 1);
      Serial.println(" pressed");
      loadPositionFromSlot(i);
      lastButtonTimes[i] = currentTime;
    }
    lastButtonStates[i] = buttonState;
  }
}

// ============================================================================
// LCD AND KEYPAD FUNCTIONS
// ============================================================================

int readKeypad() {
  int adcValue = analogRead(KEYPAD_PIN);
  if (adcValue < 100) return 4;
  if (adcValue < 800) return 2;
  if (adcValue < 1500) return 3;
  if (adcValue < 2100) return 1;
  if (adcValue < 3200) return 5;
  return 0;
}

void updateMenuDisplay() {
  lcd.clear();
  if (!inSubMenu) {
    lcd.setCursor(0, 0);
    lcd.print("Pos:");
    String posStr = String(currentPosition);
    if (posStr.length() > 10) posStr = posStr.substring(0, 10);
    lcd.print(posStr);
    
    lcd.setCursor(0, 1);
    String menuStr = menuItems[menuIndex];
    if (menuStr.length() > 16) menuStr = menuStr.substring(0, 16);
    lcd.print(menuStr);
  } else {
    switch (subMenuType) {
      case JOG:
        lcd.setCursor(0, 0);
        lcd.print("Jog: ");
        lcd.print(inputValue);
        lcd.setCursor(0, 1);
        lcd.print("L:<  R:>  Sel:X");
        break;
        
      case SPEED:
        lcd.setCursor(0, 0);
        lcd.print("Max Speed:");
        lcd.setCursor(0, 1);
        lcd.print(inputValue);
        break;
        
      case ACCEL:
        lcd.setCursor(0, 0);
        lcd.print("Accel:");
        lcd.setCursor(0, 1);
        lcd.print(inputValue);
        break;
        
      case SAVE_POS: {
        lcd.setCursor(0, 0);
        lcd.print("Slot ");
        lcd.print(inputValue + 1);
        lcd.print(": ");
        String savedStr = String(savedPositions[inputValue]);
        if (savedStr.length() > 5) savedStr = savedStr.substring(0, 5);
        lcd.print(savedStr);
        lcd.setCursor(0, 1);
        lcd.print("Select to Save");
        break;
      }
        
      case GOTO: {
        lcd.setCursor(0, 0);
        lcd.print("Go to Pos:");
        lcd.setCursor(0, 1);
        lcd.print(inputValue);
        break;
      }
        
      case GOTO_SAVED: {
        lcd.setCursor(0, 0);
        lcd.print("Slot ");
        lcd.print(inputValue + 1);
        lcd.print(": ");
        String loadStr = String(savedPositions[inputValue]);
        if (loadStr.length() > 5) loadStr = loadStr.substring(0, 5);
        lcd.print(loadStr);
        lcd.setCursor(0, 1);
        lcd.print("Select to Load");
        break;
      }
        
      case CONFIRM_RESET_HOME: {
        lcd.setCursor(0, 0);
        lcd.print("Reset Home?");
        lcd.setCursor(0, 1);
        lcd.print("Sel:Yes  Any:No");
        break;
      }
        
      case CONFIRM_SAVE_POS: {
        lcd.setCursor(0, 0);
        lcd.print("Save to Slot ");
        lcd.print(inputValue + 1);
        lcd.print("?");
        lcd.setCursor(0, 1);
        lcd.print("Sel:Yes  Any:No");
        break;
      }
        
      default: {
        lcd.setCursor(0, 0);
        lcd.print("Menu");
        break;
      }
    }
  }
}

void handleMenu() {
  int key = readKeypad();
  unsigned long currentTime = millis();

  if (key != lastKey && currentTime - lastKeyTime > debounceDelay) {
    lastKey = key;
    lastKeyTime = currentTime;

    if (!inSubMenu) {
      switch (key) {
        case 2:
          menuIndex = (menuIndex - 1 + menuSize) % menuSize;
          updateMenuDisplay();
          break;
          
        case 3:
          menuIndex = (menuIndex + 1) % menuSize;
          updateMenuDisplay();
          break;
          
        case 5:
          if (menuIndex == 3) {  // HOME - special case
            home();
            updateMenuDisplay();
          } else if (menuIndex == 4) {  // RESET_HOME
            inSubMenu = true;
            subMenuType = CONFIRM_RESET_HOME;
            updateMenuDisplay();
          } else {
            enterSubMenu();
          }
          break;
      }
    } else {
      if (subMenuType == JOG) {
        switch (key) {
          case 1:
            startMotorMovement(currentPosition + inputValue);
            inputDirection = true;
            break;
            
          case 4:
            startMotorMovement(currentPosition - inputValue);
            inputDirection = false;
            break;
            
          case 2:
            inputValue = min(inputValue + 20, (long)MAX_JOG_STEPS);  // FIXED: Use min()
            updateMenuDisplay();
            break;
            
          case 3:
            inputValue = (inputValue - 20 < 0) ? 0 : inputValue - 20;
            updateMenuDisplay();
            break;
            
          case 5:
            inSubMenu = false;
            subMenuType = NONE;
            updateMenuDisplay();
            break;
        }
      } 
      else if (subMenuType == GOTO_SAVED || subMenuType == SAVE_POS) {
        switch (key) {
          case 1:
            inputValue = (inputValue + 1 > 4) ? 4 : inputValue + 1;
            updateMenuDisplay();
            break;
            
          case 4:
            inputValue = (inputValue - 1 < 0) ? 0 : inputValue - 1;
            updateMenuDisplay();
            break;
            
          case 5:
            executeMenuAction();
            break;
        }
      } 
      else if (subMenuType == CONFIRM_RESET_HOME || subMenuType == CONFIRM_SAVE_POS) {
        switch (key) {
          case 5:
            if (subMenuType == CONFIRM_RESET_HOME) {
              resetHome();
              inSubMenu = false;
              subMenuType = NONE;
              updateMenuDisplay();
            } else if (subMenuType == CONFIRM_SAVE_POS) {
              savePositionToSlot((int)inputValue);
              inSubMenu = true;
              subMenuType = SAVE_POS;
              updateMenuDisplay();
            }
            break;
            
          default:
            inSubMenu = false;
            subMenuType = NONE;
            updateMenuDisplay();
            break;
        }
      } 
      else {
        switch (key) {
          case 1:
            inputValue += 10;
            updateMenuDisplay();
            break;
            
          case 4:
            inputValue -= 10;
            updateMenuDisplay();
            break;
            
          case 2:
            inputValue += 100;
            updateMenuDisplay();
            break;
            
          case 3:
            inputValue = (inputValue - 100 < 0) ? 0 : inputValue - 100;
            updateMenuDisplay();
            break;
            
          case 5:
            executeMenuAction();
            break;
        }
      }
    }
  } else if (key == 0) {
    lastKey = 0;
  }
}

void enterSubMenu() {
  inSubMenu = true;
  
  // FIXED: Use safe mapping instead of direct enum cast
  SubMenu menuMap[] = {
    GOTO_SAVED,
    SPEED,
    ACCEL,
    HOME_SELECT,
    RESET_HOME,
    SAVE_POS,
    GOTO,
    JOG
  };
  
  if (menuIndex < menuSize) {
    subMenuType = menuMap[menuIndex];
  }
  
  if (subMenuType == SPEED) {
    inputValue = (long)MAX_SPEED;
  } else if (subMenuType == ACCEL) {
    inputValue = (long)ACCELERATION;
  } else if (subMenuType == JOG) {
    inputValue = JOG_STEPS;
  } else {
    inputValue = 0;
  }
  
  updateMenuDisplay();
}

void executeMenuAction() {
  switch (subMenuType) {
    case JOG:
      break;
      
    case SPEED:
      setMaxSpeed((float)inputValue);
      inSubMenu = false;
      subMenuType = NONE;
      updateMenuDisplay();
      break;
      
    case ACCEL:
      setAcceleration((float)inputValue);
      inSubMenu = false;
      subMenuType = NONE;
      updateMenuDisplay();
      break;
      
    case SAVE_POS:
      inSubMenu = true;
      subMenuType = CONFIRM_SAVE_POS;
      updateMenuDisplay();
      break;
      
    case GOTO:
      moveToPosition(inputValue);
      inSubMenu = false;
      subMenuType = NONE;
      updateMenuDisplay();
      break;
      
    case GOTO_SAVED:
      loadPositionFromSlot((int)inputValue);
      inSubMenu = false;
      subMenuType = NONE;
      updateMenuDisplay();
      break;
      
    default:
      inSubMenu = false;
      subMenuType = NONE;
      updateMenuDisplay();
      break;
  }
}

// ============================================================================
// END OF PROGRAM
// ============================================================================
