// PizzaController_FastAccelStepper_OPTIMIZED.ino
// ESP32 Stepper Controller with DM556 Driver - PERFORMANCE OPTIMIZED VERSION
// Features: Non-blocking control, anti-overheating, position saving, LCD menu, optimized for speed
// Optimizations: Serial buffering, reduced loop delay, cached position, interrupt homing, LCD refresh optimization
// Original Author: Eng. Fredy Osorio <ing.fredyosorio@gmail.com>
// Optimized Version: January 28, 2026
// Optimizations: Switched to FastAccelStepper for better performance, added analog keypad for direct buttons, improved interrupt handling

// ============================================================================
// INCLUDES
// ============================================================================
#include <Preferences.h>
#include <FastAccelStepper.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ============================================================================
// PIN DEFINITIONS
// ============================================================================
#define STEP_PIN        18
#define DIR_PIN         19
#define ENABLE_PIN      21
#define HOME_SWITCH_PIN 27

#define LCD_ADDR 0x27
#define LCD_COLS 16
#define LCD_ROWS 2
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
#define KEYPAD_PIN 35
#define DIRECT_KEYPAD_PIN 34

#define BUTTON1_PIN 12
#define BUTTON2_PIN 14
#define BUTTON3_PIN 15
#define BUTTON4_PIN 16
#define BUTTON5_PIN 17

// ============================================================================
// ENUMERATIONS
// ============================================================================
enum SubMenu {
  NONE,
  GOTO_SAVED,
  SPEED,
  ACCEL,
  HOME_SELECT,
  RESET_HOME,
  SAVE_POS,
  GOTO,
  JOG,
  CONFIRM_RESET_HOME,
  CONFIRM_SAVE_POS
};

enum HomingState {
  HOMING_IDLE,
  HOMING_MOVING,
  HOMING_DONE
};

// ============================================================================
// OPTIMIZATION: SERIAL OUTPUT BUFFER
// ============================================================================
struct SerialLogBuffer {
  static const int MAX_MESSAGES = 16;
  String messages[MAX_MESSAGES];
  int count = 0;
  
  void add(const String &msg) {
    if (count < MAX_MESSAGES) {
      messages[count++] = msg;
    }
  }
  
  void flush() {
    for (int i = 0; i < count; i++) {
      Serial.println(messages[i]);
    }
    count = 0;
  }
  
  bool isEmpty() {
    return count == 0;
  }
};

SerialLogBuffer logBuffer;

// Helper function: log immediately if motor idle, otherwise buffer[web:19]
void logSmart(const String &msg) {
  extern bool isMotorMoving;
  extern bool isTesting;
  extern HomingState homingState;
  
  if (!isMotorMoving && !isTesting && homingState == HOMING_IDLE) {
    Serial.println(msg);
  } else {
    logBuffer.add(msg);
  }
}

// ============================================================================
// MOTOR CONFIGURATION
// ============================================================================
int   STEPS_PER_REV  = 3200;
float MAX_SPEED      = 8000.0;
float ACCELERATION   = 4000.0;
float HOMING_SPEED   = 2000.0;
int   JOG_STEPS      = 50;

const long  MAX_JOG_STEPS   = 50000;
const long  MAX_POSITION    = 1000000;
const float MAX_SPEED_LIMIT = 50000.0;
const float MAX_ACCEL_LIMIT = 50000.0;

unsigned long MOTOR_HOLD_TIME   = 300;
unsigned long motorDisableTime  = 0;
bool          motorShouldDisable= false;

// ============================================================================
// MOTOR STATE MANAGEMENT
// ============================================================================
bool          isMotorMoving       = false;
long          motorTargetPosition = 0;
unsigned long motorMoveStartTime  = 0;

HomingState   homingState         = HOMING_IDLE;
unsigned long homingStartTime     = 0;
volatile bool homingSwitchTriggered = false;  // Interrupt flag[web:30][web:32]

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================
long currentPosition   = 0;  // Cached position[web:12]
long homePosition      = 0;
long savedPositions[5] = {0};
Preferences preferences;

bool isTesting      = false;
long testSteps      = 0;
bool testDirection  = true;

// Menu state
int menuIndex       = 0;
const int menuSize  = 8;
String menuItems[8] = {
  "Go to Saved Pos", "Change Speed", "Change Accel", "Home",
  "Reset Home", "Save Position", "Go to Pos", "Jog"
};
bool     inSubMenu      = false;
SubMenu  subMenuType    = NONE;
long     inputValue     = 0;
bool     inputDirection = true;
int      lastKey        = 0;
unsigned long lastKeyTime = 0;
const unsigned long debounceDelay = 200;

int lastButtonStates[5]  = {HIGH, HIGH, HIGH, HIGH, HIGH};
unsigned long lastButtonTimes[5] = {0, 0, 0, 0, 0};

// OPTIMIZATION: LCD refresh tracking[web:19]
bool lastMenuWasSubMenu = false;
int  lastDisplayedIndex = -1;
long lastDisplayedPosition = -999999;

// ============================================================================
// FASTACCELSTEPPER OBJECTS
// ============================================================================
FastAccelStepperEngine engine = FastAccelStepperEngine();
FastAccelStepper *stepper = nullptr;

// ============================================================================
// OPTIMIZATION: INTERRUPT SERVICE ROUTINE FOR HOMING[web:30][web:32][web:35]
// ============================================================================
void IRAM_ATTR homeSwitchISR() {
  static unsigned long lastInterruptTime = 0;
  unsigned long currentTime = millis();
  
  // Debounce: ignore if within 50ms of last interrupt[web:32][web:38]
  if (currentTime - lastInterruptTime > 50) {
    homingSwitchTriggered = true;
    lastInterruptTime = currentTime;
  }
}

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
void handleMenu();
void handleDirectButtons();
void processCommand(String command);
void saveCurrentPosition();
void savePositionToSlot(int num);
void loadPositionFromSlot(int num);
void moveToPosition(long targetPosition);
void home();
void resetHome();
long getCurrentPosition();
void setStepsPerRev(int value);
void setMaxSpeed(float value);
void setAcceleration(float value);
void setMotorHoldTime(unsigned long value);
void setHomingSpeed(float value);
void startTestAccel(long steps);
void stopTestAccel();
void runTestAccel();
int  readKeypad();

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  pinMode(ENABLE_PIN, OUTPUT);
  pinMode(HOME_SWITCH_PIN, INPUT_PULLUP);
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  pinMode(BUTTON3_PIN, INPUT_PULLUP);
  pinMode(BUTTON4_PIN, INPUT_PULLUP);
  pinMode(BUTTON5_PIN, INPUT_PULLUP);

  digitalWrite(ENABLE_PIN, HIGH);

  // Attach interrupt for home switch (OPTIMIZATION)[web:30][web:33]
  attachInterrupt(digitalPinToInterrupt(HOME_SWITCH_PIN), homeSwitchISR, FALLING);

  // Init FastAccelStepper[web:1][web:3]
  engine.init();
  stepper = engine.stepperConnectToPin(STEP_PIN);
  if (stepper) {
    stepper->setDirectionPin(DIR_PIN);
    stepper->setEnablePin(ENABLE_PIN, true);  // active low
    stepper->setAutoEnable(false);
    stepper->setCurrentPosition(0);
  }

  Serial.begin(115200);
  delay(500);
  Serial.println(F("\n\n========================================"));
  Serial.println(F("Pizza Controller - OPTIMIZED"));
  Serial.println(F("========================================"));
  Serial.println(F("Features:"));
  Serial.println(F("- Non-blocking motor control"));
  Serial.println(F("- Anti-overheating"));
  Serial.println(F("- Buffered serial I/O"));
  Serial.println(F("- Interrupt-driven homing"));
  Serial.println(F("- Optimized LCD refresh"));
  Serial.println(F("========================================\n"));

  // Load configuration from NVS (wear-leveled automatically)[web:31][web:34][web:37]
  preferences.begin("stepper", false);
  homePosition    = preferences.getLong("homePos", 0);
  currentPosition = preferences.getLong("currPos", 0);
  STEPS_PER_REV   = preferences.getInt("stepsPerRev", 200);
  MAX_SPEED       = preferences.getFloat("maxSpeed", 3000.0);
  ACCELERATION    = preferences.getFloat("acceleration", 1000.0);
  MOTOR_HOLD_TIME = preferences.getULong("holdTime", 500);
  HOMING_SPEED    = preferences.getFloat("homingSpeed", 2000.0);

  if (stepper) {
    stepper->setSpeedInHz((uint32_t)MAX_SPEED);      // Hz units[web:46]
    stepper->setAcceleration((uint32_t)ACCELERATION);
    stepper->setCurrentPosition(currentPosition);
  }

  for (int i = 0; i < 5; i++) {
    savedPositions[i] = preferences.getLong(("pos" + String(i)).c_str(), 0);
  }

  Serial.print(F("Home position: ")); Serial.println(homePosition);
  Serial.print(F("Current position: ")); Serial.println(currentPosition);
  Serial.print(F("Steps per revolution: ")); Serial.println(STEPS_PER_REV);
  Serial.print(F("Max speed: ")); Serial.println(MAX_SPEED);
  Serial.print(F("Acceleration: ")); Serial.println(ACCELERATION);
  Serial.print(F("Homing speed: ")); Serial.println(HOMING_SPEED);
  Serial.print(F("Motor hold time: ")); Serial.print(MOTOR_HOLD_TIME);
  Serial.println(F(" ms\n"));

  Wire.begin(25, 26);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(F("Pizza Ctrl v2.0"));
  lcd.setCursor(0, 1);
  lcd.print(F("Motor Disabled"));
  delay(2000);
  updateMenuDisplay();

  Serial.println(F("System ready!"));
}

// ============================================================================
// MAIN LOOP - OPTIMIZED FOR LOW LATENCY[web:19]
// ============================================================================
void loop() {
  // OPTIMIZATION: Reduced loop delay from 10ms to 1ms for better responsiveness[web:19]
  static unsigned long lastLoopTime = 0;
  unsigned long currentTime = micros();
  
  // Handle serial commands (non-blocking)
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command.length() > 0) {
      processCommand(command);
      if (stepper) {
        currentPosition = stepper->getCurrentPosition();  // Update cache
      }
      updateMenuDisplay();
    }
  }

  // Handle menu and buttons
  handleMenu();
  handleDirectButtons();

  // FastAccelStepper runs in background ISR[web:3][web:11]
  updateMotorMovement();
  updateHomingState();

  // Auto-disable motor after hold time
  if (motorShouldDisable && millis() >= motorDisableTime) {
    disableMotor();
    motorShouldDisable = false;
  }

  // Test function
  if (isTesting) {
    runTestAccel();
    delayMicroseconds(250000);  // 250ms
  }

  // OPTIMIZATION: Flush serial buffer when motor idle[web:19]
  if (!logBuffer.isEmpty() && !isMotorMoving && !isTesting && homingState == HOMING_IDLE) {
    logBuffer.flush();
  }

  // OPTIMIZATION: 1ms loop period instead of 10ms[web:19]
  delayMicroseconds(1000);
}

// ============================================================================
// MOTOR ENABLE/DISABLE
// ============================================================================
void enableMotor() {
  if (stepper) {
    stepper->enableOutputs();
  }
  motorShouldDisable = false;
}

void disableMotor() {
  if (stepper) {
    stepper->disableOutputs();
  }
  motorShouldDisable = false;
}

void scheduleMotorDisable() {
  motorDisableTime = millis() + MOTOR_HOLD_TIME;
  motorShouldDisable = true;
}

// ============================================================================
// OPTIMIZATION: INTERRUPT-DRIVEN HOMING[web:30][web:33]
// ============================================================================
void startFindHome() {
  if (!stepper) return;
  Serial.println(F("Starting home search..."));
  enableMotor();
  homingSwitchTriggered = false;
  stepper->setSpeedInHz((uint32_t)HOMING_SPEED);
  stepper->setAcceleration((uint32_t)ACCELERATION);
  stepper->move(-MAX_POSITION);  // Move towards switch
  homingState   = HOMING_MOVING;
  homingStartTime = millis();
}

void updateHomingState() {
  if (!stepper) return;

  if (homingState == HOMING_MOVING) {
    // OPTIMIZATION: Interrupt-driven detection[web:30][web:32]
    if (homingSwitchTriggered) {
      stepper->forceStop();
      delay(10);  // Allow motion to settle
      stepper->setCurrentPosition(0);
      currentPosition = 0;
      preferences.putLong("currPos", currentPosition);
      logSmart("Home switch detected!");
      logSmart("Home found and set to position 0");
      homingState = HOMING_DONE;
      scheduleMotorDisable();
      updateMenuDisplay();
      homingSwitchTriggered = false;  // Clear flag
    }

    // Timeout safety
    if (millis() - homingStartTime > 60000) {
      stepper->forceStop();
      logSmart("ERROR: Homing timeout");
      homingState = HOMING_IDLE;
      scheduleMotorDisable();
      updateMenuDisplay();
    }
  }

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
      logSmart("Jogging forward");
    } else {
      logSmart("ERROR: Invalid steps for JOG");
    }
  }
  else if (command.startsWith("JOG B ")) {
    long steps = command.substring(6).toInt();
    if (steps > 0 && steps <= MAX_JOG_STEPS) {
      startMotorMovement(currentPosition - steps);
      logSmart("Jogging backward");
    } else {
      logSmart("ERROR: Invalid steps for JOG");
    }
  }
  else if (command.startsWith("MOVE_TO ")) {
    long position = command.substring(8).toInt();
    if (position >= -MAX_POSITION && position <= MAX_POSITION) {
      startMotorMovement(position);
      logSmart("Moving to position " + String(position));
    } else {
      logSmart("ERROR: Position out of range");
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
    startFindHome();
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
    Serial.println(F("\nMotor Configuration:"));
    Serial.print(F("  Steps per revolution: ")); Serial.println(STEPS_PER_REV);
    Serial.print(F("  Max speed (steps/sec): ")); Serial.println(MAX_SPEED);
    Serial.print(F("  Acceleration (steps/sec²): ")); Serial.println(ACCELERATION);
    Serial.print(F("  Homing speed (steps/sec): ")); Serial.println(HOMING_SPEED);
    Serial.print(F("  Motor hold time (ms): ")); Serial.println(MOTOR_HOLD_TIME);
    Serial.println();
  }
  else if (command == "GET_SWITCH") {
    int switchState = digitalRead(HOME_SWITCH_PIN);
    Serial.print(F("Home limit switch status: "));
    Serial.println((switchState == LOW) ? F("TRIGGERED") : F("NOT TRIGGERED"));
  }
  else {
    logSmart("ERROR: Unknown command");
  }
}

// ============================================================================
// NON-BLOCKING MOTOR MOVEMENT
// ============================================================================
void startMotorMovement(long targetPos) {
  if (!stepper) return;

  if (isMotorMoving) {
    logSmart("Motor already moving, ignoring command");
    return;
  }

  targetPos = constrain(targetPos, -MAX_POSITION, MAX_POSITION);

  enableMotor();
  motorTargetPosition = targetPos;
  stepper->setSpeedInHz((uint32_t)MAX_SPEED);
  stepper->setAcceleration((uint32_t)ACCELERATION);
  stepper->moveTo(targetPos);
  isMotorMoving      = true;
  motorMoveStartTime = millis();
}

void updateMotorMovement() {
  if (!stepper || !isMotorMoving) return;

  if (!stepper->isRunning()) {
    isMotorMoving = false;
    currentPosition = stepper->getCurrentPosition();  // OPTIMIZATION: Update cache once[web:12]
    preferences.putLong("currPos", currentPosition);

    logSmart("Movement complete. Position: " + String(currentPosition));
    updateMenuDisplay();
    scheduleMotorDisable();
  }
}

// ============================================================================
// POSITION MANAGEMENT
// ============================================================================
void saveCurrentPosition() {
  preferences.putLong("currPos", currentPosition);
  preferences.putLong("homePos", homePosition);
  logSmart("Current position saved: " + String(currentPosition));
}

void savePositionToSlot(int num) {
  if (num >= 0 && num < 5) {
    savedPositions[num] = currentPosition;
    preferences.putLong(("pos" + String(num)).c_str(), currentPosition);
    logSmart("Position saved to slot " + String(num) + ": " + String(currentPosition));
  } else {
    logSmart("ERROR: Invalid slot number (0-4): " + String(num));
  }
}

void loadPositionFromSlot(int num) {
  if (num >= 0 && num < 5) {
    long targetPos = savedPositions[num];
    logSmart("Loading position from slot " + String(num) + ": " + String(targetPos));

    if (currentPosition != targetPos) {
      startMotorMovement(targetPos);
    } else {
      logSmart("Already at target position");
    }
  } else {
    logSmart("ERROR: Invalid slot number (0-4): " + String(num));
  }
}

void moveToPosition(long targetPosition) {
  if (abs(targetPosition - currentPosition) < 1) {
    logSmart("Already at target position");
    return;
  }
  startMotorMovement(targetPosition);
}

void home() {
  logSmart("Homing to position 0...");
  startMotorMovement(0);
}

void resetHome() {
  if (!stepper) return;
  homePosition    = currentPosition;
  currentPosition = 0;
  stepper->setCurrentPosition(0);
  preferences.putLong("homePos", homePosition);
  preferences.putLong("currPos", currentPosition);
  logSmart("Home reset. New home offset: " + String(homePosition));
  logSmart("Current position set to 0");
}

long getCurrentPosition() {
  return currentPosition;
}

// ============================================================================
// CONFIGURATION
// ============================================================================
void setStepsPerRev(int value) {
  if (value > 0) {
    STEPS_PER_REV = value;
    preferences.putInt("stepsPerRev", STEPS_PER_REV);
    logSmart("Steps per revolution set to: " + String(STEPS_PER_REV));
  } else {
    logSmart("ERROR: Invalid value for steps per revolution");
  }
}

void setMaxSpeed(float value) {
  if (!stepper) return;
  if (value > 0 && value <= MAX_SPEED_LIMIT) {
    MAX_SPEED = value;
    preferences.putFloat("maxSpeed", MAX_SPEED);
    stepper->setSpeedInHz((uint32_t)MAX_SPEED);
    logSmart("Maximum speed set to: " + String(MAX_SPEED));
  } else {
    logSmart("ERROR: Speed must be 0 < speed <= " + String(MAX_SPEED_LIMIT));
  }
}

void setAcceleration(float value) {
  if (!stepper) return;
  if (value > 0 && value <= MAX_ACCEL_LIMIT) {
    ACCELERATION = value;
    preferences.putFloat("acceleration", ACCELERATION);
    stepper->setAcceleration((uint32_t)ACCELERATION);
    logSmart("Acceleration set to: " + String(ACCELERATION));
  } else {
    logSmart("ERROR: Acceleration must be 0 < accel <= " + String(MAX_ACCEL_LIMIT));
  }
}

void setMotorHoldTime(unsigned long value) {
  if (value <= 10000) {
    MOTOR_HOLD_TIME = value;
    preferences.putULong("holdTime", MOTOR_HOLD_TIME);
    logSmart("Motor hold time set to: " + String(MOTOR_HOLD_TIME) + " ms");
  } else {
    logSmart("ERROR: Invalid hold time (0-10000 ms)");
  }
}

void setHomingSpeed(float value) {
  if (value > 0 && value <= MAX_SPEED_LIMIT) {
    HOMING_SPEED = value;
    preferences.putFloat("homingSpeed", HOMING_SPEED);
    logSmart("Homing speed set to: " + String(HOMING_SPEED));
  } else {
    logSmart("ERROR: Homing speed must be 0 < speed <= " + String(MAX_SPEED_LIMIT));
  }
}

// ============================================================================
// TEST FUNCTIONS
// ============================================================================
void startTestAccel(long steps) {
  if (steps > 0) {
    testSteps     = steps;
    testDirection = true;
    isTesting     = true;
    logSmart("Starting test with " + String(testSteps) + " steps");
    logSmart("Test pattern: Forward -> Home -> repeat");
  } else {
    logSmart("ERROR: Invalid number of steps for test");
  }
}

void stopTestAccel() {
  if (stepper) {
    stepper->forceStop();
  }
  isTesting     = false;
  isMotorMoving = false;

  logSmart("Test stopped");
  scheduleMotorDisable();
}

void runTestAccel() {
  if (!stepper) return;
  if (!isMotorMoving && homingState == HOMING_IDLE) {
    long targetPosition = testDirection ? testSteps : 0;
    logSmart(testDirection ? "Test: Moving forward" : "Test: Returning to home");
    startMotorMovement(targetPosition);
    testDirection = !testDirection;
  }
}

// ============================================================================
// DIRECT BUTTON HANDLER
// ============================================================================
void handleDirectButtons() {
  static int lastDirectKey = 0;
  static unsigned long lastDirectKeyTime = 0;

  unsigned long currentTime = millis();
  int key = readDirectKeypad();

  if (key != lastDirectKey && currentTime - lastDirectKeyTime > debounceDelay) {
    lastDirectKey = key;
    lastDirectKeyTime = currentTime;

    if (key >= 1 && key <= 5) {
      logSmart("Direct keypad button " + String(key) + " pressed");
      loadPositionFromSlot(key - 1);  // Slots are 0-4 for buttons 1-5
    }
  } else if (key == 0) {
    lastDirectKey = 0;
  }
}

// ============================================================================
// LCD AND KEYPAD
// ============================================================================
int readKeypad() {
  int adcValue = analogRead(KEYPAD_PIN);
  if (adcValue < 100)  return 4;
  if (adcValue < 800)  return 2;
  if (adcValue < 1500) return 3;
  if (adcValue < 2100) return 1;
  if (adcValue < 3200) return 5;
  return 0;
}

int readDirectKeypad() {
  int adcValue = analogRead(DIRECT_KEYPAD_PIN);
  if (adcValue < 100)  return 5;
  if (adcValue < 2250) return 4;
  if (adcValue < 2750) return 3;
  if (adcValue < 3050) return 2;
  if (adcValue < 3500) return 1;
  return 0;
}

// OPTIMIZATION: Smart LCD refresh - skip if nothing changed[web:19]
void updateMenuDisplay() {
  bool positionChanged = (currentPosition != lastDisplayedPosition);
  bool menuStateChanged = (inSubMenu != lastMenuWasSubMenu) || (menuIndex != lastDisplayedIndex);
  
  // Skip refresh if nothing changed (saves I2C bus time)
  if (!positionChanged && !menuStateChanged && !inSubMenu) {
    return;
  }

  lcd.clear();
  
  if (!inSubMenu) {
    lcd.setCursor(0, 0);
    lcd.print(F("Pos:"));
    String posStr = String(currentPosition);
    if (posStr.length() > 10) posStr = posStr.substring(0, 10);
    lcd.print(posStr);

    lcd.setCursor(0, 1);
    String menuStr = menuItems[menuIndex];
    if (menuStr.length() > 16) menuStr = menuStr.substring(0, 16);
    lcd.print(menuStr);
    
    lastDisplayedPosition = currentPosition;
    lastDisplayedIndex = menuIndex;
    lastMenuWasSubMenu = false;
  } else {
    lastMenuWasSubMenu = true;
    
    switch (subMenuType) {
      case JOG:
        lcd.setCursor(0, 0);
        lcd.print(F("Jog: "));
        lcd.print(inputValue);
        lcd.setCursor(0, 1);
        lcd.print(F("L:<  R:>  Sel:X"));
        break;

      case SPEED:
        lcd.setCursor(0, 0);
        lcd.print(F("Max Speed:"));
        lcd.setCursor(0, 1);
        lcd.print(inputValue);
        break;

      case ACCEL:
        lcd.setCursor(0, 0);
        lcd.print(F("Accel:"));
        lcd.setCursor(0, 1);
        lcd.print(inputValue);
        break;

      case SAVE_POS: {
        lcd.setCursor(0, 0);
        lcd.print(F("Slot "));
        lcd.print(inputValue + 1);
        lcd.print(F(": "));
        String savedStr = String(savedPositions[inputValue]);
        if (savedStr.length() > 5) savedStr = savedStr.substring(0, 5);
        lcd.print(savedStr);
        lcd.setCursor(0, 1);
        lcd.print(F("Select to Save"));
        break;
      }

      case GOTO: {
        lcd.setCursor(0, 0);
        lcd.print(F("Go to Pos:"));
        lcd.setCursor(0, 1);
        lcd.print(inputValue);
        break;
      }

      case GOTO_SAVED: {
        lcd.setCursor(0, 0);
        lcd.print(F("Slot "));
        lcd.print(inputValue + 1);
        lcd.print(F(": "));
        String loadStr = String(savedPositions[inputValue]);
        if (loadStr.length() > 5) loadStr = loadStr.substring(0, 5);
        lcd.print(loadStr);
        lcd.setCursor(0, 1);
        lcd.print(F("Select to Load"));
        break;
      }

      case CONFIRM_RESET_HOME: {
        lcd.setCursor(0, 0);
        lcd.print(F("Reset Home?"));
        lcd.setCursor(0, 1);
        lcd.print(F("Sel:Yes  Any:No"));
        break;
      }

      case CONFIRM_SAVE_POS: {
        lcd.setCursor(0, 0);
        lcd.print(F("Save to Slot "));
        lcd.print(inputValue + 1);
        lcd.print(F("?"));
        lcd.setCursor(0, 1);
        lcd.print(F("Sel:Yes  Any:No"));
        break;
      }

      default: {
        lcd.setCursor(0, 0);
        lcd.print(F("Menu"));
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
          if (menuIndex == 3) {
            home();
            updateMenuDisplay();
          } else if (menuIndex == 4) {
            inSubMenu  = true;
            subMenuType= CONFIRM_RESET_HOME;
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
            inputValue = min(inputValue + 20, (long)MAX_JOG_STEPS);
            updateMenuDisplay();
            break;

          case 3:
            inputValue = (inputValue - 20 < 0) ? 0 : inputValue - 20;
            updateMenuDisplay();
            break;

          case 5:
            inSubMenu   = false;
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
              inSubMenu   = false;
              subMenuType = NONE;
              updateMenuDisplay();
            } else if (subMenuType == CONFIRM_SAVE_POS) {
              savePositionToSlot((int)inputValue);
              inSubMenu   = true;
              subMenuType = SAVE_POS;
              updateMenuDisplay();
            }
            break;

          default:
            inSubMenu   = false;
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

  SubMenu menuMap[] = {
    GOTO_SAVED, SPEED, ACCEL, HOME_SELECT,
    RESET_HOME, SAVE_POS, GOTO, JOG
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
      inSubMenu   = false;
      subMenuType = NONE;
      updateMenuDisplay();
      break;

    case ACCEL:
      setAcceleration((float)inputValue);
      inSubMenu   = false;
      subMenuType = NONE;
      updateMenuDisplay();
      break;

    case SAVE_POS:
      inSubMenu   = true;
      subMenuType = CONFIRM_SAVE_POS;
      updateMenuDisplay();
      break;

    case GOTO:
      moveToPosition(inputValue);
      inSubMenu   = false;
      subMenuType = NONE;
      updateMenuDisplay();
      break;

    case GOTO_SAVED:
      loadPositionFromSlot((int)inputValue);
      inSubMenu   = false;
      subMenuType = NONE;
      updateMenuDisplay();
      break;

    default:
      inSubMenu   = false;
      subMenuType = NONE;
      updateMenuDisplay();
      break;
  }
}

// ============================================================================
// END OF OPTIMIZED PROGRAM
// ============================================================================
