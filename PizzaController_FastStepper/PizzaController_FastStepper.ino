// PizzaController_FastAccelStepper.ino
// ESP32 Stepper Controller with DM556 Driver - PRODUCTION-READY VERSION
// Features: Non-blocking control, anti-overheating, position saving, LCD menu, optimized for speed
// Fixes Applied: ISR safety, non-blocking I/O, motor hold logic, array sizing, cached position refresh
// Original Author: Eng. Fredy Osorio <ing.fredyosorio@gmail.com>
// Fixed Version: February 9, 2026
// Based on: FastAccelStepper library for high-performance stepper control

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
// - SET_HOME_DIR <dir>: Set homing direction (-1 or +1) and save to memory
// - GET_INFO: Display motor configuration (steps, speed, acceleration, hold time)
// - GET_SWITCH: Read home limit switch status
// - SET_POS <steps>: Override current position tracking to <steps> without moving motor
// - HELP: Shows all the Serial Commands

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
#define STEP_PIN 18
#define DIR_PIN 19
#define ENABLE_PIN 21
#define HOME_SWITCH_PIN 27

#define LCD_ADDR 0x27
#define LCD_COLS 16
#define LCD_ROWS 2
#define LCD_SDA 25
#define LCD_SCL 26
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

#define KEYPAD_PIN 35
#define DIRECT_KEYPAD_PIN 34

#define ESTOP_PIN 33    // Digital input, NC button (active LOW)
#define LED_HOME_PIN 32 // Optional: onboard LED for home status indication

// ============================================================================
// KEYPAD CALIBRATION CONSTANTS
// ============================================================================

// Main keypad thresholds (adjust based on your hardware)
const int KEYPAD_THRESHOLD_1 = 220;
const int KEYPAD_THRESHOLD_2 = 800;
const int KEYPAD_THRESHOLD_3 = 1400;
const int KEYPAD_THRESHOLD_4 = 2300;
const int KEYPAD_THRESHOLD_5 = 3600;

// Direct keypad thresholds
const int DIRECT_KEYPAD_THRESHOLD_1 = 130;
const int DIRECT_KEYPAD_THRESHOLD_2 = 570;
const int DIRECT_KEYPAD_THRESHOLD_3 = 1170;
const int DIRECT_KEYPAD_THRESHOLD_4 = 1740;
const int DIRECT_KEYPAD_THRESHOLD_5 = 2370;
const int DIRECT_KEYPAD_THRESHOLD_6 = 3100;
const int DIRECT_KEYPAD_THRESHOLD_7 = 3700;

// Home switch threshold (hall sensor)
const int HOME_SWITCH_THRESHOLD = 1500;

// ============================================================================
// ENUMERATIONS
// ============================================================================
enum SubMenu
{
  NONE,
  GOTO_SAVED,
  SPEED,
  ACCEL,
  RESET_HOME,
  SAVE_POS,
  GOTO,
  JOG,
  CONFIRM_RESET_HOME,
  CONFIRM_SAVE_POS
};

enum HomingState
{
  HOMING_IDLE,
  HOMING_MOVING,
  HOMING_DONE
};

// ============================================================================
// SERIAL OUTPUT BUFFER (Non-blocking logging)
// ============================================================================
struct SerialLogBuffer
{
  static const int MAX_MESSAGES = 16;
  String messages[MAX_MESSAGES];
  int count = 0;

  void add(const String &msg)
  {
    if (count < MAX_MESSAGES)
    {
      messages[count++] = msg;
    }
  }

  void flush()
  {
    for (int i = 0; i < count; i++)
    {
      Serial.println(messages[i]);
    }
    count = 0;
  }

  bool isEmpty()
  {
    return count == 0;
  }
};

SerialLogBuffer logBuffer;

// ============================================================================
// MOTOR CONFIGURATION
// ============================================================================
int STEPS_PER_REV = 3200;
float MAX_SPEED = 8000.0;
float ACCELERATION = 4000.0;
float HOMING_SPEED = 2000.0;
int JOG_STEPS = 50;
int HOME_DIRECTION = -1; // -1 = move negative to find home, +1 = move positive
int lastDirectKey = 0;   // For direct button state tracking
const long MAX_JOG_STEPS = 50000;
const long MAX_POSITION = 1000000;
const float MAX_SPEED_LIMIT = 50000.0;
const float MAX_ACCEL_LIMIT = 50000.0;

unsigned long MOTOR_HOLD_TIME = 300;
unsigned long motorDisableTime = 0;
bool motorShouldDisable = false;

// ============================================================================
// MOTOR STATE MANAGEMENT
// ============================================================================
bool isMotorMoving = false;
long motorTargetPosition = 0;
unsigned long motorMoveStartTime = 0;

HomingState homingState = HOMING_IDLE;
unsigned long homingStartTime = 0;

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================
long currentPosition = 0;
long homePosition = 0;
long savedPositions[5] = {0};
Preferences preferences;

bool isTesting = false;
long testSteps = 0;
bool testDirection = true;
unsigned long lastTestTime = 0; // For non-blocking test timing

// Menu state
int menuIndex = 0;
const int menuSize = 8;
String menuItems[8] = {
  "Go to Saved Pos", "Change Speed", "Change Accel", "Home",
  "Reset Home", "Save Position", "Go to Pos", "Jog"
};
bool inSubMenu = false;
SubMenu subMenuType = NONE;
long inputValue = 0;
bool inputDirection = true;
int lastKey = 0;
unsigned long lastKeyTime = 0;
long targetPos = 0;                      // For direct button target position
const unsigned long debounceDelay = 300; // Reduced from 200ms for better responsiveness
bool flag = false;
// LCD refresh tracking (optimization)
bool lastMenuWasSubMenu = false;
int lastDisplayedIndex = -1;
long lastDisplayedPosition = -999999;

// Direct buttons selection state
int selectedPositionIndex = -1; // -1=none, 0-4=selected slot
int lastSelectedPosIndex = -1;
bool lastMotorState = false;

// ============================================================================
// FASTACCELSTEPPER OBJECTS
// ============================================================================
FastAccelStepperEngine engine = FastAccelStepperEngine();
FastAccelStepper *stepper = nullptr;

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
void handleSerialInput();
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
void setHomeDirection(int value);
void startTestAccel(long steps);
void setPositionWithoutMoving(long newPos);
void stopTestAccel();
void runTestAccel();
int readKeypad();
int readDirectKeypad();
int readHomeSwitch();
int readEStop()
{
  return digitalRead(ESTOP_PIN);
}
void logSmart(const String &msg);
void motorInfo();
void help();

// ============================================================================
// SMART LOGGING (Buffer when motor moving, immediate otherwise)
// ============================================================================
void logSmart(const String &msg)
{
  if (!isMotorMoving && !isTesting && homingState == HOMING_IDLE)
  {
    Serial.println(msg);
  }
  else
  {
    logBuffer.add(msg);
  }
}

// ============================================================================
// SETUP
// ============================================================================
void setup()
{
  pinMode(ENABLE_PIN, OUTPUT);
  pinMode(HOME_SWITCH_PIN, INPUT_PULLUP);
  pinMode(LED_HOME_PIN, OUTPUT);
  digitalWrite(ENABLE_PIN, HIGH); // Motor initially disabled

  // Init FastAccelStepper
  engine.init();
  stepper = engine.stepperConnectToPin(STEP_PIN);
  if (stepper)
  {
    stepper->setDirectionPin(DIR_PIN);
    stepper->setEnablePin(ENABLE_PIN, true); // Active low
    stepper->setAutoEnable(false);           // Manual enable/disable control
    stepper->setCurrentPosition(0);
  }

  Serial.begin(115200);
  delay(500);
  Serial.println(F("\n\n========================================"));
  Serial.println(F("Pizza Controller - LOEM PUC-Rio"));
  Serial.println(F("========================================\n"));

  // Load configuration from NVS (wear-leveled automatically)
  preferences.begin("stepper", false);
  homePosition = preferences.getLong("homePos", 0);
  currentPosition = preferences.getLong("currPos", 0);
  STEPS_PER_REV = preferences.getInt("stepsPerRev", 3200);
  MAX_SPEED = preferences.getFloat("maxSpeed", 8000.0);
  ACCELERATION = preferences.getFloat("acceleration", 4000.0);
  MOTOR_HOLD_TIME = preferences.getULong("holdTime", 300);
  HOMING_SPEED = preferences.getFloat("homingSpeed", 2000.0);
  HOME_DIRECTION = preferences.getInt("homeDir", -1);

  // Validate and clamp loaded values
  if (MAX_SPEED <= 0 || MAX_SPEED > MAX_SPEED_LIMIT)
    MAX_SPEED = 8000.0;
  if (ACCELERATION <= 0 || ACCELERATION > MAX_ACCEL_LIMIT)
    ACCELERATION = 4000.0;
  if (HOME_DIRECTION != -1 && HOME_DIRECTION != 1)
    HOME_DIRECTION = -1;

  if (stepper)
  {
    stepper->setSpeedInHz((uint32_t)MAX_SPEED);
    stepper->setAcceleration((uint32_t)ACCELERATION);
    stepper->setCurrentPosition(currentPosition);
  }

  // Load saved positions with validation
  for (int i = 0; i < 5; i++)
  {
    long pos = preferences.getLong(("pos" + String(i)).c_str(), 0);
    if (pos >= -MAX_POSITION && pos <= MAX_POSITION)
    {
      savedPositions[i] = pos;
    }
    else
    {
      savedPositions[i] = 0;
      Serial.print(F("WARNING: Slot "));
      Serial.print(i);
      Serial.println(F(" position out of range, reset to 0"));
    }
  }

  motorInfo();
  help();

  pinMode(ESTOP_PIN, INPUT_PULLUP);

  int lastSelectedPosIndex = -1;
  bool lastMotorState = false;

  // LCD init (main menu)
  Wire.begin(LCD_SDA, LCD_SCL);
  lcd.begin(Wire);

  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(F("  LOEM PUC-Rio  "));
  lcd.setCursor(0, 1);
  lcd.print(F("Pizza  Controller"));

  delay(2000);
  updateMenuDisplay();

  Serial.println(F("System ready!"));
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop()
{
  // E-Stop check - immediate stop if pressed
  if (readEStop() == LOW)
  {
    if (stepper)
      stepper->forceStop();
    isMotorMoving = false;
    isTesting = false;
    homingState = HOMING_IDLE;
    disableMotor();
    logSmart("E-STOP ACTIVATED - All movement stopped");
  }
  // AAA
  // logSmart("lastDirectKey: " + String(lastDirectKey) + ", isMotorMoving: " + String(isMotorMoving));
  // if (lastDirectKey == 6 || lastDirectKey == 7) {
  //   if (not isMotorMoving) {
  //     currentPosition = targetPos;
  //     preferences.putLong("currPos", targetPos);
  //     logSmart("Direct jog complete. Position: " + String(targetPos));
  //     updateMenuDisplay();
  //     lastDirectKey = 8;

  //   }
  // }

  // convert target position into current position
  resetPosition();

  // Handle serial commands (non-blocking)
  handleSerialInput();

  // Handle menu and buttons
  handleMenu();
  handleDirectButtons();

  // FastAccelStepper runs in background ISR
  updateMotorMovement();
  updateHomingState();

  // Auto-disable motor after hold time
  if (motorShouldDisable && millis() >= motorDisableTime)
  {
    disableMotor();
    motorShouldDisable = false;
  }

  // Test function (non-blocking timing)
  if (isTesting)
  {
    unsigned long now = micros();
    if (now - lastTestTime >= 250000)
    { // 250ms interval
      lastTestTime = now;
      runTestAccel();
    }
  }

  // Flush serial buffer when motor idle
  if (!logBuffer.isEmpty() && !isMotorMoving && !isTesting && homingState == HOMING_IDLE)
  {
    logBuffer.flush();
  }

  // 1ms loop period for responsive control
  delay(100);
}

// ============================================================================
// MOTOR ENABLE/DISABLE
// FIXED: Proper hold-time state management
// ============================================================================
void enableMotor()
{
  if (stepper)
  {
    stepper->enableOutputs();
  }
  // Clear any pending disable timer when actively enabling
  motorShouldDisable = false;
  motorDisableTime = 0;
}

void disableMotor()
{
  if (stepper)
  {
    stepper->disableOutputs();
  }
  motorShouldDisable = false;
}

void scheduleMotorDisable()
{
  motorDisableTime = millis() + MOTOR_HOLD_TIME;
  motorShouldDisable = true;
}

// ============================================================================
// INTERRUPT-DRIVEN HOMING WITH ISR-SAFE DEBOUNCING
// FIXED: Debouncing moved outside ISR, uses millis() in main loop only
// ============================================================================
void startFindHome()
{
  if (!stepper)
    return;
  Serial.println(F("Starting home search..."));
  Serial.print(F("Moving in direction: "));
  Serial.println(HOME_DIRECTION);

  enableMotor();
  stepper->setSpeedInHz((uint32_t)HOMING_SPEED / 2);
  stepper->setAcceleration((uint32_t)ACCELERATION / 2);
  stepper->move(HOME_DIRECTION * MAX_POSITION); // Configurable direction
  homingState = HOMING_MOVING;
  homingStartTime = millis();
}

void updateHomingState()
{
  if (!stepper)
    return;

  if (homingState == HOMING_MOVING)
  {
    // Poll hall sensor with debouncing
    static unsigned long lastTriggerTime = 0;
    unsigned long now = millis();

    if (readHomeSwitch() && now - lastTriggerTime > 50)
    { // 50ms debounce
      lastTriggerTime = now;

      // Home switch detected!
      stepper->forceStop();
      delay(50); // Allow motion to settle

      // NOTE: setCurrentPosition() only works reliably in standstill.
      // The RMT module may have off-by-X steps in the current command.
      // We stop first to minimize error.
      stepper->setCurrentPosition(0);
      currentPosition = 0;
      preferences.putLong("currPos", currentPosition);

      logSmart("Home switch detected!");
      logSmart("Home found and set to position 0");
      homingState = HOMING_DONE;
      scheduleMotorDisable();
      updateMenuDisplay();
    }

    // Timeout safety
    if (millis() - homingStartTime > 60000)
    {
      stepper->forceStop();
      logSmart("ERROR: Homing timeout (60s). Check switch connection and HOME_DIRECTION setting.");
      homingState = HOMING_IDLE;
      scheduleMotorDisable();
      updateMenuDisplay();
    }
  }

  if (homingState == HOMING_DONE)
  {
    homingState = HOMING_IDLE;
  }
}

// ============================================================================
// NON-BLOCKING SERIAL INPUT HANDLER
// FIXED: Character-by-character parsing instead of blocking readStringUntil()
// ============================================================================
void handleSerialInput()
{
  static String commandBuffer = "";

  while (Serial.available() > 0)
  {
    char c = Serial.read();

    if (c == '\n')
    {
      commandBuffer.trim();
      if (commandBuffer.length() > 0)
      {
        processCommand(commandBuffer);
        currentPosition = getCurrentPosition(); // Refresh cache
        updateMenuDisplay();
      }
      commandBuffer = "";
    }
    else if (c != '\r')
    { // Skip carriage returns
      commandBuffer += c;

      // Safety: discard if too long
      if (commandBuffer.length() > 100)
      {
        logSmart("ERROR: Command too long, discarded");
        commandBuffer = "";
      }
    }
  }
}

// ============================================================================
// SERIAL COMMAND PROCESSING
// ============================================================================
void processCommand(String command)
{
  if (command.startsWith("JOG F "))
  {
    long steps = command.substring(6).toInt();
    if (steps > 0 && steps <= MAX_JOG_STEPS)
    {
      startMotorMovement(currentPosition + steps);
      logSmart("Jogging forward");
    }
    else
    {
      logSmart("ERROR: Invalid steps for JOG (must be 1-" + String(MAX_JOG_STEPS) + ")");
    }
  }
  else if (command.startsWith("JOG B "))
  {
    long steps = command.substring(6).toInt();
    if (steps > 0 && steps <= MAX_JOG_STEPS)
    {
      startMotorMovement(currentPosition - steps);
      logSmart("Jogging backward");
    }
    else
    {
      logSmart("ERROR: Invalid steps for JOG (must be 1-" + String(MAX_JOG_STEPS) + ")");
    }
  }
  else if (command.startsWith("MOVE_TO "))
  {
    long position = command.substring(8).toInt();
    if (position >= -MAX_POSITION && position <= MAX_POSITION)
    {
      startMotorMovement(position);
      logSmart("Moving to position " + String(position));
    }
    else
    {
      logSmart("ERROR: Position out of range (±" + String(MAX_POSITION) + ")");
    }
  }
  else if (command == "HOME")
  {
    home();
  }
  else if (command == "RESET_HOME")
  {
    resetHome();
  }
  else if (command.startsWith("SAVE_POS "))
  {
    int num = command.substring(9).toInt();
    savePositionToSlot(num);
  }
  else if (command.startsWith("LOAD_POS "))
  {
    int num = command.substring(9).toInt();
    loadPositionFromSlot(num);
  }
  else if (command == "GET_POS")
  {
    Serial.println(getCurrentPosition());
  }
  else if (command == "FIND_HOME")
  {
    startFindHome();
  }
  else if (command.startsWith("TEST "))
  {
    long steps = command.substring(5).toInt();
    startTestAccel(steps);
  }
  else if (command == "STOP")
  {
    stopTestAccel();
  }
  else if (command.startsWith("SET_STEPS "))
  {
    int value = command.substring(10).toInt();
    setStepsPerRev(value);
  }
  else if (command.startsWith("SET_MAX_SPEED "))
  {
    float value = command.substring(14).toFloat();
    setMaxSpeed(value);
  }
  else if (command.startsWith("SET_ACCELERATION "))
  {
    float value = command.substring(17).toFloat();
    setAcceleration(value);
  }
  else if (command.startsWith("SET_HOLD_TIME "))
  {
    unsigned long value = command.substring(14).toInt();
    setMotorHoldTime(value);
  }
  else if (command.startsWith("SET_SPEED "))
  {
    float value = command.substring(10).toFloat();
    setHomingSpeed(value);
  }
  else if (command.startsWith("SET_HOME_DIR "))
  {
    int value = command.substring(13).toInt();
    setHomeDirection(value);
  }
  else if (command == "GET_INFO")
  {
    motorInfo();
  }
  else if (command.startsWith("SET_POS "))
  {
    long newPos = command.substring(8).toInt();
    setPositionWithoutMoving(newPos);
  }
  else if (command == "GET_SWITCH")
  {
    int switchState = digitalRead(HOME_SWITCH_PIN);
    Serial.print(F("Home limit switch status: "));
    Serial.println((switchState == LOW) ? F("TRIGGERED") : F("NOT TRIGGERED"));
    digitalWrite(LED_HOME_PIN, (switchState == LOW) ? HIGH : LOW); // Optional: LED indication
  }
  else if (command == "HELP")
  {
    help();
  }
  else
  {
    logSmart("ERROR: Unknown command: " + command);
  }
}

void motorInfo()
{
  Serial.println("=========================================================");
  Serial.println(F("\nMotor Configuration:"));
  Serial.print(F("- Home position: "));
  Serial.println(homePosition);
  Serial.print(F("- Current position: "));
  Serial.println(currentPosition);
  Serial.print(F("- Steps per revolution: "));
  Serial.println(STEPS_PER_REV);
  Serial.print(F("- Max speed (steps/sec): "));
  Serial.println(MAX_SPEED);
  Serial.print(F("- Acceleration (steps/sec²): "));
  Serial.println(ACCELERATION);
  Serial.print(F("- Homing speed (steps/sec): "));
  Serial.println(HOMING_SPEED);
  Serial.print(F("- Homing direction: "));
  Serial.println(HOME_DIRECTION);
  Serial.print(F("- Motor hold time (ms): "));
  Serial.println(MOTOR_HOLD_TIME);
  Serial.println();
  Serial.println(F("Saved Positions:"));
  for (int i = 0; i < 5; i++)
  {
    Serial.print(F("- Slot "));
    Serial.print(i);
    Serial.print(F(": "));
    Serial.println(savedPositions[i]);
  }
  Serial.println();
  Serial.println("=========================================================");
}

void help()
{
  Serial.println("=========================================================");
  Serial.println("<<<<<  Serial Commands  >>>>>");
  Serial.println("- JOG F <steps>: Jog forward (clockwise) by <steps> steps");
  Serial.println("- JOG B <steps>: Jog backward (counterclockwise) by <steps> steps");
  Serial.println("- MOVE_TO <position>: Move to absolute position <position>");
  Serial.println("- HOME: Move to home position (0)");
  Serial.println("- RESET_HOME: Set current position as new home");
  Serial.println("- SAVE_POS <num>: Save current position to slot <num> (0-4)");
  Serial.println("- LOAD_POS <num>: Load position from slot <num>");
  Serial.println("- GET_POS: Print current position");
  Serial.println("- FIND_HOME: Find home using limit switch (non-blocking)");
  Serial.println("- TEST <steps>: Start continuous test with <steps> steps");
  Serial.println("- STOP: Stop the test");
  Serial.println("- SET_STEPS <value>: Set steps per revolution and save to memory");
  Serial.println("- SET_MAX_SPEED <value>: Set maximum speed and save to memory (0 < value <= 50000)");
  Serial.println("- SET_ACCELERATION <value>: Set acceleration and save to memory (0 < value <= 50000)");
  Serial.println("- SET_HOLD_TIME <ms>: Set motor hold time after move (0-10000 ms)");
  Serial.println("- SET_SPEED <value>: Set homing speed and save to memory (0 < value <= 50000)");
  Serial.println("- SET_HOME_DIR <dir>: Set homing direction (-1 or +1) and save to memory");
  Serial.println("- GET_INFO: Display motor configuration (steps, speed, acceleration, hold time)");
  Serial.println("- GET_SWITCH: Read home limit switch status");
  Serial.println("- SET_POS <steps>: Override current position tracking to <steps> without moving motor");
  Serial.println("- HELP: Shows all the Serial Commands");
  Serial.println("=========================================================");
}

// ============================================================================
// NON-BLOCKING MOTOR MOVEMENT
// ============================================================================
void startMotorMovement(long targetPos)
{
  if (!stepper)
    return;

  if (isMotorMoving)
  {
    logSmart("Motor already moving, ignoring command");
    return;
  }

  targetPos = constrain(targetPos, -MAX_POSITION, MAX_POSITION);

  enableMotor();
  delay(50);
  motorTargetPosition = targetPos;
  stepper->setSpeedInHz((uint32_t)MAX_SPEED);
  stepper->setAcceleration((uint32_t)ACCELERATION);
  stepper->moveTo(targetPos);
  isMotorMoving = true;
  motorMoveStartTime = millis();
}

void updateMotorMovement()
{
  if (!stepper || !isMotorMoving)
    return;

  if (!stepper->isRunning())
  {
    isMotorMoving = false;
    currentPosition = stepper->getCurrentPosition(); // Update cache
    preferences.putLong("currPos", currentPosition);

    logSmart("Movement complete. Position: " + String(currentPosition));
    updateMenuDisplay();
    scheduleMotorDisable();
  }
}

// ============================================================================
// POSITION MANAGEMENT
// ============================================================================
void saveCurrentPosition()
{
  preferences.putLong("currPos", currentPosition);
  preferences.putLong("homePos", homePosition);
  logSmart("Current position saved: " + String(currentPosition));
}

void savePositionToSlot(int num)
{
  if (num >= 0 && num < 5)
  {
    savedPositions[num] = currentPosition;
    preferences.putLong(("pos" + String(num)).c_str(), currentPosition);
    logSmart("Position saved to slot " + String(num) + ": " + String(currentPosition));
  }
  else
  {
    logSmart("ERROR: Invalid slot number (0-4): " + String(num));
  }
}

void loadPositionFromSlot(int num)
{
  if (num >= 0 && num < 5)
  {
    long targetPos = savedPositions[num];
    logSmart("Loading position from slot " + String(num) + ": " + String(targetPos));

    if (currentPosition != targetPos)
    {
      startMotorMovement(targetPos);
    }
    else
    {
      logSmart("Already at target position");
    }
  }
  else
  {
    logSmart("ERROR: Invalid slot number (0-4): " + String(num));
  }
}

void moveToPosition(long targetPosition)
{
  if (abs(targetPosition - currentPosition) < 1)
  {
    logSmart("Already at target position");
    return;
  }
  startMotorMovement(targetPosition);
}

void home()
{
  logSmart("Homing to position 0...");
  startMotorMovement(0);
}

void resetHome()
{
  if (!stepper)
    return;
  homePosition = currentPosition;
  currentPosition = 0;
  stepper->setCurrentPosition(0);
  preferences.putLong("homePos", homePosition);
  preferences.putLong("currPos", currentPosition);
  logSmart("Home reset. New home offset: " + String(homePosition));

  logSmart("Current position set to 0");
  updateMenuDisplay();
}

void resetPosition()
{
  if (isMotorMoving)
  {
    return;
  } 
  if (currentPosition != targetPos)
  {
    return;
  }
  if (flag)
  {
    setPositionWithoutMoving(targetPos);
    updateMenuDisplay();
    flag = false;
  }
  else
  {
    return;
  }
  
}

// FIXED: Always refresh from stepper for accurate reading
long getCurrentPosition()
{
  if (stepper)
  {
    currentPosition = stepper->getCurrentPosition();
  }
  return currentPosition;
}

// ============================================================================
// CONFIGURATION
// ============================================================================
void setStepsPerRev(int value)
{
  if (value > 0)
  {
    STEPS_PER_REV = value;
    preferences.putInt("stepsPerRev", STEPS_PER_REV);
    logSmart("Steps per revolution set to: " + String(STEPS_PER_REV));
  }
  else
  {
    logSmart("ERROR: Invalid value for steps per revolution");
  }
}

void setMaxSpeed(float value)
{
  if (!stepper)
    return;
  if (value > 0 && value <= MAX_SPEED_LIMIT)
  {
    MAX_SPEED = value;
    preferences.putFloat("maxSpeed", MAX_SPEED);
    stepper->setSpeedInHz((uint32_t)MAX_SPEED);
    logSmart("Maximum speed set to: " + String(MAX_SPEED));
  }
  else
  {
    logSmart("ERROR: Speed must be 0 < speed <= " + String(MAX_SPEED_LIMIT));
  }
}

void setAcceleration(float value)
{
  if (!stepper)
    return;
  if (value > 0 && value <= MAX_ACCEL_LIMIT)
  {
    ACCELERATION = value;
    preferences.putFloat("acceleration", ACCELERATION);
    stepper->setAcceleration((uint32_t)ACCELERATION);
    logSmart("Acceleration set to: " + String(ACCELERATION));
  }
  else
  {
    logSmart("ERROR: Acceleration must be 0 < accel <= " + String(MAX_ACCEL_LIMIT));
  }
}

void setMotorHoldTime(unsigned long value)
{
  if (value <= 10000)
  {
    MOTOR_HOLD_TIME = value;
    preferences.putULong("holdTime", MOTOR_HOLD_TIME);
    logSmart("Motor hold time set to: " + String(MOTOR_HOLD_TIME) + " ms");
  }
  else
  {
    logSmart("ERROR: Invalid hold time (0-10000 ms)");
  }
}

void setHomingSpeed(float value)
{
  if (value > 0 && value <= MAX_SPEED_LIMIT)
  {
    HOMING_SPEED = value;
    preferences.putFloat("homingSpeed", HOMING_SPEED);
    logSmart("Homing speed set to: " + String(HOMING_SPEED));
  }
  else
  {
    logSmart("ERROR: Homing speed must be 0 < speed <= " + String(MAX_SPEED_LIMIT));
  }
}

void setHomeDirection(int value)
{
  if (value == -1 || value == 1)
  {
    HOME_DIRECTION = value;
    preferences.putInt("homeDir", HOME_DIRECTION);
    logSmart("Homing direction set to: " + String(HOME_DIRECTION));
    Serial.println(F("NOTE: -1 = move negative to find home, +1 = move positive"));
  }
  else
  {
    logSmart("ERROR: Homing direction must be -1 or +1");
  }
}

void setPositionWithoutMoving(long newPos)
{
  if (newPos >= -MAX_POSITION && newPos <= MAX_POSITION)
  {
    currentPosition = newPos;
    if (stepper)
      stepper->setCurrentPosition(newPos);
    preferences.putLong("currPos", currentPosition);
    logSmart("Position overridden to: " + String(currentPosition));
    updateMenuDisplay();
  }
  else
  {
    logSmart("ERROR: SET_POS value out of range (+/-" + String(MAX_POSITION) + ")");
  }
}

// ============================================================================
// TEST FUNCTIONS
// ============================================================================
void startTestAccel(long steps)
{
  if (steps > 0 && steps <= MAX_POSITION)
  {
    testSteps = steps;
    testDirection = true;
    isTesting = true;
    lastTestTime = micros(); // Initialize non-blocking timer
    logSmart("Starting test with " + String(testSteps) + " steps");
    logSmart("Test pattern: Forward -> Home -> repeat");
  }
  else
  {
    logSmart("ERROR: Invalid number of steps for test (1-" + String(MAX_POSITION) + ")");
  }
}

void stopTestAccel()
{
  if (stepper)
  {
    stepper->forceStop();
  }
  isTesting = false;
  isMotorMoving = false;

  logSmart("Test stopped");
  scheduleMotorDisable();
}

void runTestAccel()
{
  if (!stepper)
    return;
  if (!isMotorMoving && homingState == HOMING_IDLE)
  {
    long targetPosition = testDirection ? testSteps : 0;
    logSmart(testDirection ? "Test: Moving forward" : "Test: Returning to home");
    startMotorMovement(targetPosition);
    testDirection = !testDirection;
  }
}

// ============================================================================
// DIRECT BUTTON HANDLER
// FIXED: Jog movement uses saved position as reference, not current position
// - If current position matches saved position, no movement occurs
// - Movement calculated based on shortest path considering full rotation
// ============================================================================

void calculateGoToSavedPosition(long currentPos, long targetPos, long STEPS_PER_REV, String direction)
{

  if (currentPos == targetPos)
  {
    logSmart("Already at target position");
    return;
  }

  if((direction == "CW" && targetPos > currentPos) || (direction == "CCW" && targetPos < currentPos))
  {
    return targetPos;
  }
  else
  {
    if (direction == "CW")
    {
      return targetPos + STEPS_PER_REV; 
    }
    else
    {
      return targetPos - STEPS_PER_REV; 
    }
  }

}


void handleDirectButtons()
{
  // static int lastDirectKey = 0;
  static unsigned long lastDirectKeyTime = 0;
  String commandTemp = "";
  unsigned long currentTime = millis();

  // Position selection (GPIO34)
  int posKey = readDirectKeypad();
  if (posKey != lastDirectKey && currentTime - lastDirectKeyTime > debounceDelay)
  {
    lastDirectKey = posKey;
    lastDirectKeyTime = currentTime;

    if (posKey >= 1 && posKey <= 5)
    { // Buttons 2-5 -> slots 0-3
      selectedPositionIndex = posKey - 1;
      logSmart("Position slot " + String(posKey));
    }
    else if (posKey == 6 || posKey == 7)
    { // CW jog - move towards saved position

      bool direction = true;
      String dir = "CW";
      if (posKey == 6)
      {
        direction = false; // CW = button 6, CCW = button 7
        dir = "CCW";
      }
      // Get current position and target from selected slot
      long currentPos = getCurrentPosition();
      targetPos = savedPositions[selectedPositionIndex];

      // Calculate shortest path to target considering full rotation
      long moveSteps = calculateJogToPosition(currentPos, targetPos, STEPS_PER_REV, dir);
      
      logSmart("Going to saved position " + String(selectedPositionIndex) + ": " + String(targetPos));
      startMotorMovement(moveSteps);

      // long jogSteps = calculateJogToPosition(currentPos, targetPos, STEPS_PER_REV, true);

      // if (jogSteps == 0)
      // {
      //   logSmart("Already at target position");
      // }
      // else
      // {
      //   logSmart("Direct jog to " + String(targetPos) + " by " + String(getCurrentPosition()) + " + jog " + String(jogSteps) + " " + dir);
      //   if (dir == "CW")
      //   {
      //     commandTemp = "JOG F " + String(jogSteps);
      //     Serial.println("Generated command: " + commandTemp);
      //     processCommand(commandTemp);
      //   }
      //   else
      //   {
      //     commandTemp = "JOG B " + String(abs(jogSteps));
      //     Serial.println("Generated command: " + commandTemp);
      //     processCommand(commandTemp);
      //   }

        flag = true; // Set flag to update position after jog completes
      }
    }
  }
  updateMenuDisplay();
}

// Calculate jog steps to reach target position
// Returns 0 if already at target, otherwise returns shortest path
long calculateJogToPosition(long currentPos, long targetPos, long fullRotation, bool rotationDirection)
{
  // If already at target, no movement needed
  if (currentPos == targetPos)
  {
    return 0;
  }

  long stepsToJog = targetPos - currentPos;
  Serial.print(">> 1 Initial steps to jog: " + String(stepsToJog));
  if (rotationDirection)
  {
    // Clockwise (CW) direction has to return positive
    if (stepsToJog < 0)
    {
      stepsToJog = currentPos - fullRotation - targetPos; // Wrap around for CW
    }
    Serial.print(">> 2 Returned " + String(stepsToJog));

    return abs(stepsToJog);
  }
  else
  {
    // Counter-clockwise (CCW) direction has to return positive
    if (stepsToJog > 0)
    {
      stepsToJog = abs(fullRotation - targetPos + currentPos); // Wrap around for CCW
    }
    Serial.print(">> 2 Returned " + String(stepsToJog));
    return -abs(stepsToJog); // Negative for CCW direction
  }
}

// ============================================================================
// LCD AND KEYPAD
// ============================================================================
int readKeypad()
{
  static int readingsK[3] = {0, 0, 0};
  static int index = 0;

  int currentReadingK = analogRead(KEYPAD_PIN);
  // Serial.println(currentReadingK);
  readingsK[index] = currentReadingK;
  index = (index + 1) % 3;

  int sum = readingsK[0] + readingsK[1] + readingsK[2];
  int avgValue = sum / 3;

  // Calculate standard deviation
  float variance = 0;
  for (int i = 0; i < 3; i++)
  {
    variance += pow(readingsK[i] - avgValue, 2);
  }
  variance /= 3;
  float stdDev = sqrt(variance);

  int key = 0;
  if (stdDev < 50)
  {
    // Use named constants for threshold values
    if (avgValue < KEYPAD_THRESHOLD_1)
      key = 1;
    else if (avgValue < KEYPAD_THRESHOLD_2)
      key = 2;
    else if (avgValue < KEYPAD_THRESHOLD_3)
      key = 3;
    else if (avgValue < KEYPAD_THRESHOLD_4)
      key = 4;
    else if (avgValue < KEYPAD_THRESHOLD_5)
      key = 5;
  }

  return key;
}

int readDirectKeypad()
{
  static int readingsD[3] = {0, 0, 0};
  static int index = 0;

  int currentReading = analogRead(DIRECT_KEYPAD_PIN);
  // Serial.println(currentReading);
  readingsD[index] = currentReading;
  index = (index + 1) % 3;

  int sum = readingsD[0] + readingsD[1] + readingsD[2];
  int avgValue = sum / 3;

  // Calculate standard deviation
  float variance = 0;
  for (int i = 0; i < 3; i++)
  {
    variance += pow(readingsD[i] - avgValue, 2);
  }
  variance /= 3;
  float stdDev = sqrt(variance);

  int key = 0;
  if (stdDev < 50)
  {
    // Use named constants for threshold values
    if (avgValue < DIRECT_KEYPAD_THRESHOLD_1)
      key = 1;
    else if (avgValue < DIRECT_KEYPAD_THRESHOLD_2)
      key = 2;
    else if (avgValue < DIRECT_KEYPAD_THRESHOLD_3)
      key = 3;
    else if (avgValue < DIRECT_KEYPAD_THRESHOLD_4)
      key = 4;
    else if (avgValue < DIRECT_KEYPAD_THRESHOLD_5)
      key = 5;
    else if (avgValue < DIRECT_KEYPAD_THRESHOLD_6)
      key = 6;
    else if (avgValue < DIRECT_KEYPAD_THRESHOLD_7)
      key = 7;
  }

  return key;
}

int readHomeSwitch()
{
  static int readingsH[3] = {0, 0, 0};
  static int index = 0;

  int currentReadingH = analogRead(HOME_SWITCH_PIN);
  // Serial.println(currentReadingH);
  readingsH[index] = currentReadingH;
  index = (index + 1) % 3;

  int sum = readingsH[0] + readingsH[1] + readingsH[2];
  int avgValue = sum / 3;

  // Calculate standard deviation
  float variance = 0;
  for (int i = 0; i < 3; i++)
  {
    variance += pow(readingsH[i] - avgValue, 2);
  }
  variance /= 3;
  float stdDev = sqrt(variance);

  int triggered = 0;
  if (stdDev < 50)
  {
    // Use named constant for threshold value
    if (avgValue < HOME_SWITCH_THRESHOLD)
      triggered = 1;
  }

  return triggered;
}

// ============================================================================
// LCD MENU DISPLAY
// ============================================================================
void updateMenuDisplay()
{
  long pos = getCurrentPosition(); // Always fresh from stepper
  bool positionChanged = (pos != lastDisplayedPosition);
  bool menuStateChanged = (inSubMenu != lastMenuWasSubMenu) || (menuIndex != lastDisplayedIndex);
  bool selChanged = (selectedPositionIndex != lastSelectedPosIndex);
  bool moveChanged = (isMotorMoving != lastMotorState);
  lastSelectedPosIndex = selectedPositionIndex;
  lastMotorState = isMotorMoving;

  // Skip refresh if nothing changed (saves I2C bus time)
  if (!positionChanged && !menuStateChanged && !selChanged && !moveChanged && !inSubMenu)
  {
    return;
  }

  // LCD: Main menu
  lcd.clear();

  // PRIORITY 1: Show direct button state prominently
  if (selectedPositionIndex >= 0)
  {
    // Slot selected via direct button - show prominently
    lcd.setCursor(0, 0);
    lcd.print(F("Sel "));
    lcd.print(selectedPositionIndex);
    lcd.print(F("="));
    // Show saved position for this slot
    String slotPosStr = String(savedPositions[selectedPositionIndex]);
    if (slotPosStr.length() > 10)
      slotPosStr = slotPosStr.substring(0, 10);
    lcd.print(slotPosStr);

    // Row 1: Show motor state / ready to load
    lcd.setCursor(0, 1);
    if (isMotorMoving)
    {
      lcd.print(F("Moving..."));
    }
    else if (homingState != HOMING_IDLE)
    {
      lcd.print(F("Homing..."));
    }
    else
    {
      // Show current position
      String curPosStr = String(pos);
      if (curPosStr.length() > 14)
        curPosStr = curPosStr.substring(0, 14);
      lcd.print(F("Cur:"));
      lcd.print(curPosStr);
    }
  }
  // PRIORITY 2: Show motor action when moving (from direct buttons)
  else if (isMotorMoving)
  {
    lcd.setCursor(0, 0);
    lcd.print(F("Pos:"));
    String posStr = String(pos);
    if (posStr.length() > 10)
      posStr = posStr.substring(0, 10);
    lcd.print(posStr);

    lcd.setCursor(0, 1);
    lcd.print(F("Moving..."));
  }
  // PRIORITY 3: Show homing state
  else if (homingState != HOMING_IDLE)
  {
    lcd.setCursor(0, 0);
    lcd.print(F("Pos:"));
    String posStr = String(pos);
    if (posStr.length() > 10)
      posStr = posStr.substring(0, 10);
    lcd.print(posStr);

    lcd.setCursor(0, 1);
    lcd.print(F("Finding Home..."));
  }
  // PRIORITY 4: Normal menu mode (when no direct button action)
  else if (!inSubMenu)
  {
    lcd.setCursor(0, 0);
    lcd.print(F("P:"));
    String posStr = String(pos);
    if (posStr.length() > 12)
      posStr = posStr.substring(0, 12);
    lcd.print(posStr);

    lcd.setCursor(0, 1);
    String menuStr = menuItems[menuIndex];
    if (menuStr.length() > 16)
      menuStr = menuStr.substring(0, 16);
    lcd.print(menuStr);
  }
  // PRIORITY 5: Submenu mode
  else
  {
    lastMenuWasSubMenu = true;

    switch (subMenuType)
    {
      case JOG:
        lcd.setCursor(0, 0);
        lcd.print(F("Jog Steps:"));
        lcd.setCursor(0, 1);
        lcd.print(inputValue);
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

      case SAVE_POS:
        {
          lcd.setCursor(0, 0);
          lcd.print(F("Slot "));
          lcd.print(inputValue + 1);
          lcd.print(F(": "));
          String savedStr = String(savedPositions[inputValue]);
          if (savedStr.length() > 5)
            savedStr = savedStr.substring(0, 5);
          lcd.print(savedStr);
          lcd.setCursor(0, 1);
          lcd.print(F("Select to Save"));
          break;
        }

      case GOTO:
        {
          lcd.setCursor(0, 0);
          lcd.print(F("Go to Pos:"));
          lcd.setCursor(0, 1);
          lcd.print(inputValue);
          break;
        }

      case GOTO_SAVED:
        {
          lcd.setCursor(0, 0);
          lcd.print(F("Slot "));
          lcd.print(inputValue + 1);
          lcd.print(F(": "));
          String loadStr = String(savedPositions[inputValue]);
          if (loadStr.length() > 5)
            loadStr = loadStr.substring(0, 5);
          lcd.print(loadStr);
          lcd.setCursor(0, 1);
          lcd.print(F("Select to Load"));
          break;
        }

      case CONFIRM_RESET_HOME:
        {
          lcd.setCursor(0, 0);
          lcd.print(F("Reset Home?"));
          lcd.print(F("Sel:Yes  Any:No"));
          break;
        }

      case CONFIRM_SAVE_POS:
        {
          lcd.setCursor(0, 0);
          lcd.print(F("Save to Slot "));
          lcd.print(inputValue + 1);
          lcd.print(F("?"));
          lcd.setCursor(0, 1);
          lcd.print(F("Sel:Yes  Any:No"));
          break;
        }

      default:
        {
          lcd.setCursor(0, 0);
          lcd.print(F("Menu"));
          break;
        }
    }
  }

  lastDisplayedPosition = pos;
  lastDisplayedIndex = menuIndex;
  lastMenuWasSubMenu = inSubMenu;
}

// ============================================================================
// MENU HANDLER
// ============================================================================
void handleMenu()
{
  int key = readKeypad();
  unsigned long currentTime = millis();

  if (key != lastKey && currentTime - lastKeyTime > debounceDelay)
  {
    lastKey = key;
    lastKeyTime = currentTime;

    if (!inSubMenu)
    {
      switch (key)
      {
        case 2:
          menuIndex = (menuIndex - 1 + menuSize) % menuSize;
          updateMenuDisplay();
          break;

        case 3:
          menuIndex = (menuIndex + 1) % menuSize;
          updateMenuDisplay();
          break;

        case 5:
          if (menuIndex == 3)
          {
            home();
            updateMenuDisplay();
          }
          else if (menuIndex == 4)
          {
            inSubMenu = true;
            subMenuType = CONFIRM_RESET_HOME;
            updateMenuDisplay();
          }
          else
          {
            enterSubMenu();
          }
          break;
      }
    }
    else
    {
      if (subMenuType == JOG)
      {
        switch (key)
        {
          case 1:
            startMotorMovement(getCurrentPosition() + inputValue);
            inputDirection = true;
            break;

          case 4:
            startMotorMovement(getCurrentPosition() - inputValue);
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
            inSubMenu = false;
            subMenuType = NONE;
            updateMenuDisplay();
            break;
        }
      }
      else if (subMenuType == GOTO_SAVED || subMenuType == SAVE_POS)
      {
        switch (key)
        {
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
      else if (subMenuType == CONFIRM_RESET_HOME || subMenuType == CONFIRM_SAVE_POS)
      {
        switch (key)
        {
          case 5:
            if (subMenuType == CONFIRM_RESET_HOME)
            {
              resetHome();
              inSubMenu = false;
              subMenuType = NONE;
              updateMenuDisplay();
            }
            else if (subMenuType == CONFIRM_SAVE_POS)
            {
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
      else
      {
        switch (key)
        {
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
  }
  else if (key == 0)
  {
    lastKey = 0;
  }
}

void enterSubMenu()
{
  inSubMenu = true;

  SubMenu menuMap[] = {
    GOTO_SAVED, SPEED, ACCEL, NONE, // NONE for "Home" (index 3)
    RESET_HOME, SAVE_POS, GOTO, JOG
  };

  if (menuIndex < menuSize)
  {
    subMenuType = menuMap[menuIndex];
  }

  if (subMenuType == SPEED)
  {
    inputValue = (long)MAX_SPEED;
  }
  else if (subMenuType == ACCEL)
  {
    inputValue = (long)ACCELERATION;
  }
  else if (subMenuType == JOG)
  {
    inputValue = JOG_STEPS;
  }
  else
  {
    inputValue = 0;
  }

  updateMenuDisplay();
}

void executeMenuAction()
{
  switch (subMenuType)
  {
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
