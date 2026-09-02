// PizzaController_FastStepper.ino
// ESP32 Stepper Controller with DM556 Driver - PRODUCTION-READY VERSION
// Hardware: ESP32, NEMA23 57HS56-1504A08-D21, DM556 (DIP set to 1/32 microstep / ~4.0A peak),
//          24V/5A PSU, firmware default STEPS_PER_REV = 12800 (32x motor's natural 400 steps)
// Pins: STEP=18, DIR=19, ENABLE=21, HOME=27, ESTOP=33, LCD SDA=25 SCL=26, POS_BTN=34, ROT_BTN=36
// Original Author: Eng. Fredy Osorio <ing.fredyosorio@gmail.com>
// Updated: August 2026

// Serial Commands:
// - JOG F <steps>: Jog forward (clockwise) by <steps> steps
// - JOG B <steps>: Jog backward (counterclockwise) by <steps> steps
// - MOVE_TO <position>: Move to absolute position <position>
// - HOME: Move to home position (0)
// - RESET_HOME: Set current position as new home (0)
// - SAVE_POS <num>: Save current position to slot <num> (0-4)
// - LOAD_POS <num>: Load position from slot <num> (0-4)
// - GET_POS: Print current position
// - FIND_HOME: Find home using limit switch (two-stage non-blocking)
// - TEST <steps>: Start continuous test with <steps> steps
// - STOP: Stop continuous test
// - SET_STEPS <value>: Set steps per revolution and save to memory
// - SET_MAX_SPEED <value>: Set maximum speed and save to memory (0 < value <= 50000)
// - SET_ACCELERATION <value>: Set acceleration and save to memory (0 < value <= 50000)
// - SET_HOLD_TIME <ms>: Set motor hold time after move (0-10000 ms)
// - SET_SPEED <value>: Set homing speed and save to memory (0 < value <= 50000)
// - SET_HOME_DIR <dir>: Set homing direction (-1 or +1) and save to memory
// - SET_HYST_POS <value>: Set positive rotation hysteresis compensation steps (X) and save
// - SET_HYST_NEG <value>: Set negative rotation hysteresis compensation steps (Y) and save
// - GET_INFO: Display motor configuration (steps, speed, acceleration, hold time, hysteresis)
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

#define KEYPAD_PIN 35
#define DIRECT_KEYPAD_PIN 34       // Position selection buttons (slots 0-4)
#define ROTATION_KEYPAD_PIN 36     // CW/CCW rotation direction buttons

#define ESTOP_PIN 33    // Digital input, INPUT_PULLUP. NC button to GND: HIGH when OK (button closed), LOW when E-Stop triggered (button opened or wire cut).
#define LED_HOME_PIN 32 // Optional: onboard LED for home status indication (ADC1_CH4, used as digital output)

// ============================================================================
// DISPLAY OBJECTS
// ============================================================================
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// ============================================================================
// KEYPAD CALIBRATION CONSTANTS
// ============================================================================
// Main keypad thresholds
const int KEYPAD_THRESHOLD_1 = 220;
const int KEYPAD_THRESHOLD_2 = 800;
const int KEYPAD_THRESHOLD_3 = 1400;
const int KEYPAD_THRESHOLD_4 = 2300;
const int KEYPAD_THRESHOLD_5 = 3600;

// Direct keypad thresholds (GPIO34 - 5 position buttons: 0, 20, 40, 60, 80% of 4095)
// Midpoints: 10%, 30%, 50%, 70% = 410, 1229, 2048, 2867
const int DIRECT_KEYPAD_THRESHOLD_1 = 410;   // ~10% midpoint between 0% and 20%
const int DIRECT_KEYPAD_THRESHOLD_2 = 1229;  // ~30% midpoint between 20% and 40%
const int DIRECT_KEYPAD_THRESHOLD_3 = 2048;  // ~50% midpoint between 40% and 60%
const int DIRECT_KEYPAD_THRESHOLD_4 = 2867;  // ~70% midpoint between 60% and 80%
const int DIRECT_KEYPAD_THRESHOLD_5 = 3686;  // ~90% midpoint between 80% and 100%

// Rotation keypad thresholds (GPIO36 - CCW/CW buttons: 25% and 75% of 4095)
// Midpoint: 50% = 2048
const int ROTATION_KEYPAD_THRESHOLD = 2048;  // 50% midpoint between 25% and 75%

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


enum HomingState {
  HOMING_IDLE,
  HOMING_INITIAL_BACKOFF,
  HOMING_STAGE1_FAST,
  HOMING_STAGE1_STOPPING,
  HOMING_BACKOFF,
  HOMING_STAGE2_SLOW,
  HOMING_STAGE2_STOPPING,
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

// ============================================================================
// MOTOR CONFIGURATION (Default parameters matching hardware requirements)
// ============================================================================
int STEPS_PER_REV = 12800;
float MAX_SPEED = 1600.0;
float ACCELERATION = 100.0 ;
float HOMING_SPEED = 1600.0;
const float HOMING_SLOW_SPEED = 200.0;
const long HOMING_BACKOFF_STEPS = 500;
int JOG_STEPS = 50;
int HOME_DIRECTION = 1; // 1 = move positive to find home
int HYSTERESIS_POS_STEPS = 32; // CW compensation for one full revolution (scaled proportionally)
int HYSTERESIS_NEG_STEPS = 90; // CCW compensation for one full revolution (scaled proportionally)
int lastDirectKey = 0;   // For direct button state tracking

const long MAX_JOG_STEPS = 50000;
const long MAX_POSITION = 1000000;
const float MAX_SPEED_LIMIT = 50000.0;
const float MAX_ACCEL_LIMIT = 50000.0;

unsigned long MOTOR_HOLD_TIME = 300; // Deprecated hold-time setting, kept for NVS compatibility
unsigned long motorDisableTime = 0;
bool motorShouldDisable = false;

// ============================================================================
// MOTOR STATE MANAGEMENT
// ============================================================================
bool isMotorMoving = false;
long motorTargetPosition = 0;       // Logical position to report after active move completes
unsigned long motorMoveStartTime = 0;

HomingState homingState = HOMING_IDLE;
unsigned long homingStartTime = 0;
unsigned long homingStopStartTime = 0;

// ============================================================================
// POSITION AND STORAGE STATE
// ============================================================================
// Semantic Definitions:
// - currentPosition: Cached absolute motor step count (synchronized with stepper->getCurrentPosition())
// - homePosition: Saved reference coordinate/offset in NVS
// - savedPositions[5]: Array of user-saved absolute coordinates (slots 0 to 4)
long currentPosition = 0;
long homePosition = 0;
long savedPositions[5] = {0};
Preferences preferences;

// ============================================================================
// TEST STATE
// ============================================================================
bool isTesting = false;
long testSteps = 0;
bool testDirection = true;
unsigned long lastTestTime = 0; // For non-blocking test timing

// ============================================================================
// MENU STATE
// ============================================================================
int menuIndex = 0;
const int menuSize = 8;
String menuItems[8] = {
  "Saved Pos", "Change Speed", "Change Accel", "Home",
  "Reset Home", "Save Position", "Move to", "Jog"
};

bool inSubMenu = false;
SubMenu subMenuType = NONE;
long inputValue = 0;
bool inputDirection = true;
int lastKey = 0;
unsigned long lastKeyTime = 0;
const unsigned long debounceDelay = 300;

// ============================================================================
// DIRECT BUTTON STATE
// ============================================================================
int selectedPositionIndex = -1; // -1 = none, 0-4 = selected slot (0-4)
int lastSelectedPosIndex = -1;
bool lastMotorState = false;

// ============================================================================
// LCD REFRESH TRACKING
// ============================================================================
bool lastMenuWasSubMenu = false;
int lastDisplayedIndex = -1;
long lastDisplayedPosition = -999999;

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================
SerialLogBuffer logBuffer;
FastAccelStepperEngine engine = FastAccelStepperEngine();
FastAccelStepper *stepper = nullptr;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
void setup();
void loop();

void enterSubMenu();
void executeMenuAction();
void updateMenuDisplay();
void handleMenu();
void handleDirectButtons();
void handleSerialInput();
void processCommand(String command);

void startMotorMovement(long targetPos);
void startMotorMovement(long physicalTargetPos, long logicalTargetPos);
void updateMotorMovement();
void disableMotor();
void enableMotor();
void scheduleMotorDisable();

void updateHomingState();
void startFindHome();
void findHomeDirection(int direction);

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
void setHysteresisPos(int value);
void setHysteresisNeg(int value);
void setPositionWithoutMoving(long newPos);

void startTestAccel(long steps);
void stopTestAccel();
void runTestAccel();

int readKeypad();
int readDirectKeypad();
int readRotationKeypad();
int readHomeSwitch();
int readEStop();

void eStopCheck();
void logSmart(const String &msg);
void motorInfo();
void help();

long calculateGoToSavedPosition(long currentPos, long targetPos, long stepsPerRev, const String &direction);
long calculateJogToPosition(long currentPos, long targetPos, long fullRotation, bool rotationDirection);

// ============================================================================
// BASIC INPUT HELPERS
// ============================================================================
int readEStop()
{
  return digitalRead(ESTOP_PIN);
}

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
  // HOME_SWITCH_PIN is read via analogRead() with a 4-sample moving average (see readHomeSwitch()).
  // Use plain INPUT (no pullup) so the analog threshold is well-defined.
  pinMode(HOME_SWITCH_PIN, INPUT);
  pinMode(LED_HOME_PIN, OUTPUT);
  pinMode(ESTOP_PIN, INPUT_PULLUP);

  Serial.begin(115200);
  delay(500);
  Serial.println(F("\n\n========================================"));
  Serial.println(F("Pizza Controller - LOEM PUC-Rio"));
  Serial.println(F("========================================\n"));

  // Load configuration from NVS (wear-leveled automatically)
  preferences.begin("stepper", false);
  homePosition = preferences.getLong("homePos", 0);
  currentPosition = preferences.getLong("currPos", 0);
  STEPS_PER_REV = preferences.getInt("stepsPerRev", 12800);
  MAX_SPEED = preferences.getFloat("maxSpeed", 1600.0);
  ACCELERATION = preferences.getFloat("acceleration", 300.0);
  MOTOR_HOLD_TIME = preferences.getULong("holdTime", 300);
  HOMING_SPEED = preferences.getFloat("homingSpeed", 1600.0);
  HOME_DIRECTION = 1; // Always positive homing direction (+1)
  preferences.putInt("homeDir", HOME_DIRECTION);
  HYSTERESIS_POS_STEPS = preferences.getInt("hystPos", 0);
  HYSTERESIS_NEG_STEPS = preferences.getInt("hystNeg", 0);

  // Validate and clamp loaded values
  if (STEPS_PER_REV <= 0)
    STEPS_PER_REV = 12800;
  if (MAX_SPEED <= 0 || MAX_SPEED > MAX_SPEED_LIMIT)
    MAX_SPEED = 1600.0;
  if (ACCELERATION <= 0 || ACCELERATION > MAX_ACCEL_LIMIT)
    ACCELERATION = 300.0;
  if (HOMING_SPEED <= 0 || HOMING_SPEED > MAX_SPEED_LIMIT)
    HOMING_SPEED = 1600.0;
  if (HYSTERESIS_POS_STEPS < 0 || HYSTERESIS_POS_STEPS > MAX_JOG_STEPS)
    HYSTERESIS_POS_STEPS = 0;
  if (HYSTERESIS_NEG_STEPS < 0 || HYSTERESIS_NEG_STEPS > MAX_JOG_STEPS)
    HYSTERESIS_NEG_STEPS = 0;

  // Initialize FastAccelStepper engine
  engine.init();
  stepper = engine.stepperConnectToPin(STEP_PIN);
  if (stepper)
  {
    // Configure DIR pin with 200us direction change delay for DM556 optocoupler safety
    stepper->setDirectionPin(DIR_PIN, true, 200);
    // Configure ENABLE pin (low_active_enables_stepper = true)
    stepper->setEnablePin(ENABLE_PIN, true);
    stepper->setAutoEnable(false);
    stepper->setSpeedInHz((uint32_t)MAX_SPEED);
    stepper->setAcceleration((uint32_t)ACCELERATION);
    stepper->setCurrentPosition(currentPosition);
    stepper->enableOutputs(); // Energize motor and hold position permanently
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

  // LCD init (main menu)
  Wire.begin(LCD_SDA, LCD_SCL);
  lcd.begin(Wire);
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(F("  LOEM PUC-Rio  "));
  lcd.setCursor(0, 1);
  lcd.print(F("Pizza  Control"));

  delay(2000);

  Serial.println(F("System ready!"));

  // Initiate non-blocking homing calibration at startup
  startFindHome();
  updateMenuDisplay();
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop()
{
  // E-Stop check - immediate stop and disable if pressed
  eStopCheck();

  updateMenuDisplay();

  // Handle serial commands (non-blocking)
  handleSerialInput();

  // Handle menu and buttons
  handleMenu();
  handleDirectButtons();

  // FastAccelStepper runs in background ISR; update state machines
  updateMotorMovement();
  updateHomingState();

  // Auto-disable motor only if explicitly scheduled (e.g. shutdown)
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

  // 1ms loop period for responsive control and high sensor polling repeatability
  delay(1);
}

// ============================================================================
// SAFETY
// ============================================================================
void eStopCheck()
{
  static bool lastEStopState = false;
  bool isEStopPressed = (readEStop() == HIGH);

  if (isEStopPressed)
  {
    if (stepper)
      stepper->forceStop();
    isMotorMoving = false;
    isTesting = false;
    homingState = HOMING_IDLE;
    disableMotor();

    if (!lastEStopState)
    {
      logSmart("E-STOP ACTIVATED - All movement stopped and motor disabled");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(F("E-STOP ACTIVE   "));
      lastEStopState = true;
    }
  }
  else if (lastEStopState)
  {
    // Transition from active E-STOP to normal state
    lastEStopState = false;
    enableMotor();
    currentPosition = getCurrentPosition();
    motorTargetPosition = currentPosition;
    logSmart("E-STOP RELEASED - Motor re-enabled at position: " + String(currentPosition));
    updateMenuDisplay();
  }
}

// ============================================================================
// MOTOR ENABLE / DISABLE
// ============================================================================
void enableMotor()
{
  if (stepper)
  {
    stepper->enableOutputs();
  }
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
  // Kept for backward compatibility if explicitly needed
  motorDisableTime = millis() + MOTOR_HOLD_TIME;
  motorShouldDisable = true;
}

// ============================================================================
// HOMING
// TWO-STAGE NON-BLOCKING STATE MACHINE
// Sequence: Fast approach -> detect -> stop -> back off -> slow approach -> detect -> set 0
// ============================================================================
void findHomeDirection(int direction)
{
  if (direction == 1 || direction == -1)
  {
    setHomeDirection(direction);
  }
  startFindHome();
}

void startFindHome()
{
  if (!stepper)
    return;
  if (readEStop() == HIGH)
    return;

  if (isMotorMoving || isTesting)
  {
    stepper->forceStop();
    isMotorMoving = false;
    isTesting = false;
  }

  enableMotor();

  // Check if sensor is already triggered
  if (readHomeSwitch() == 1)
  {
    logSmart("Homing: Sensor active at start, backing away...");
    stepper->setSpeedInHz((uint32_t)HOMING_SLOW_SPEED);
    stepper->setAcceleration((uint32_t)ACCELERATION);
    stepper->move(-HOME_DIRECTION * HOMING_BACKOFF_STEPS);
    homingStartTime = millis();
    homingState = HOMING_INITIAL_BACKOFF;
  }
  else
  {
    logSmart("Homing: Stage 1 fast approach...");
    stepper->setSpeedInHz((uint32_t)HOMING_SPEED);
    stepper->setAcceleration((uint32_t)ACCELERATION);
    stepper->move(HOME_DIRECTION * MAX_POSITION);
    homingStartTime = millis();
    homingState = HOMING_STAGE1_FAST;
  }
}

void updateHomingState()
{
  if (!stepper || homingState == HOMING_IDLE)
    return;

  // Timeout safety (60s)
  if (millis() - homingStartTime > 60000)
  {
    stepper->forceStop();
    logSmart("ERROR: Homing timeout (60s). Check switch connection and HOME_DIRECTION setting.");
    homingState = HOMING_IDLE;
    updateMenuDisplay();
    return;
  }

  switch (homingState)
  {
    case HOMING_INITIAL_BACKOFF:
      if (!stepper->isRunning())
      {
        if (readHomeSwitch() == 1)
        {
          // Still active, back off further
          stepper->move(-HOME_DIRECTION * HOMING_BACKOFF_STEPS);
        }
        else
        {
          logSmart("Homing: Switch cleared. Stage 1 fast approach...");
          stepper->setSpeedInHz((uint32_t)HOMING_SPEED);
          stepper->setAcceleration((uint32_t)ACCELERATION);
          stepper->move(HOME_DIRECTION * MAX_POSITION);
          homingState = HOMING_STAGE1_FAST;
        }
      }
      break;

    case HOMING_STAGE1_FAST:
      if (readHomeSwitch() == 1)
      {
        logSmart("Homing: Stage 1 switch detected, stopping...");
        stepper->forceStop();
        homingState = HOMING_STAGE1_STOPPING;
      }
      else if (!stepper->isRunning())
      {
        logSmart("ERROR: Homing reached max travel without triggering switch.");
        homingState = HOMING_IDLE;
        updateMenuDisplay();
      }
      break;

    case HOMING_STAGE1_STOPPING:
      if (!stepper->isRunning())
      {
        logSmart("Homing: Backing off for precision touch...");
        stepper->setSpeedInHz((uint32_t)HOMING_SLOW_SPEED);
        stepper->setAcceleration((uint32_t)ACCELERATION);
        stepper->move(-HOME_DIRECTION * HOMING_BACKOFF_STEPS);
        homingState = HOMING_BACKOFF;
      }
      break;

    case HOMING_BACKOFF:
      if (!stepper->isRunning())
      {
        if (readHomeSwitch() == 1)
        {
          // Still active, back off further
          stepper->move(-HOME_DIRECTION * HOMING_BACKOFF_STEPS);
        }
        else
        {
          logSmart("Homing: Stage 2 slow precision approach...");
          stepper->setSpeedInHz((uint32_t)HOMING_SLOW_SPEED);
          stepper->setAcceleration((uint32_t)ACCELERATION);
          stepper->move(HOME_DIRECTION * (HOMING_BACKOFF_STEPS * 2));
          homingState = HOMING_STAGE2_SLOW;
        }
      }
      break;

    case HOMING_STAGE2_SLOW:
      if (readHomeSwitch() == 1)
      {
        stepper->forceStop();
        homingState = HOMING_STAGE2_STOPPING;
      }
      else if (!stepper->isRunning())
      {
        logSmart("ERROR: Stage 2 slow approach missed switch.");
        homingState = HOMING_IDLE;
        updateMenuDisplay();
      }
      break;

    case HOMING_STAGE2_STOPPING:
      if (!stepper->isRunning())
      {
        stepper->setCurrentPosition(0);
        currentPosition = 0;
        preferences.putLong("currPos", currentPosition);

        logSmart("Home switch detected!");
        logSmart("Home found and calibrated to position 0");
        homingState = HOMING_DONE;
        updateMenuDisplay();
      }
      break;

    case HOMING_DONE:
      homingState = HOMING_IDLE;
      break;

    default:
      homingState = HOMING_IDLE;
      break;
  }
}
// ============================================================================
// SERIAL INPUT
// NON-BLOCKING SERIAL INPUT HANDLER
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
      startMotorMovement(getCurrentPosition() + steps);
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
      startMotorMovement(getCurrentPosition() - steps);
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
  else if (command.startsWith("SET_HYST_POS "))
  {
    int value = command.substring(13).toInt();
    setHysteresisPos(value);
  }
  else if (command.startsWith("SET_HYST_NEG "))
  {
    int value = command.substring(13).toInt();
    setHysteresisNeg(value);
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
    int switchState = readHomeSwitch();
    int rawAnalog = analogRead(HOME_SWITCH_PIN);
    Serial.print(F("Home limit switch status: "));
    Serial.println((switchState == 1) ? F("TRIGGERED") : F("NOT TRIGGERED"));
    Serial.print(F("Home switch ADC reading: "));
    Serial.println(rawAnalog);
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

// ============================================================================
// REPORTING
// ============================================================================
void motorInfo()
{
  Serial.println(F("========================================================="));
  Serial.println(F("Motor Configuration:"));
  Serial.print(F("- Home position: "));
  Serial.println(homePosition);
  Serial.print(F("- Current position: "));
  Serial.println(getCurrentPosition());
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
  Serial.print(F("- Hysteresis Pos (+/CW) steps: "));
  Serial.println(HYSTERESIS_POS_STEPS);
  Serial.print(F("- Hysteresis Neg (-/CCW) steps: "));
  Serial.println(HYSTERESIS_NEG_STEPS);
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
  Serial.println(F("========================================================="));
}

void help()
{
  Serial.println(F("========================================================="));
  Serial.println(F("<<<<<  Serial Commands  >>>>>"));
  Serial.println(F("- JOG F <steps>: Jog forward (clockwise) by <steps> steps"));
  Serial.println(F("- JOG B <steps>: Jog backward (counterclockwise) by <steps> steps"));
  Serial.println(F("- MOVE_TO <position>: Move to absolute position <position>"));
  Serial.println(F("- HOME: Move to home position (0)"));
  Serial.println(F("- RESET_HOME: Set current position as new home"));
  Serial.println(F("- SAVE_POS <num>: Save current position to slot <num> (0-4)"));
  Serial.println(F("- LOAD_POS <num>: Load position from slot <num> (0-4)"));
  Serial.println(F("- GET_POS: Print current position"));
  Serial.println(F("- FIND_HOME: Find home using limit switch (two-stage non-blocking)"));
  Serial.println(F("- TEST <steps>: Start continuous test with <steps> steps"));
  Serial.println(F("- STOP: Stop continuous test"));
  Serial.println(F("- SET_STEPS <value>: Set steps per revolution and save to memory"));
  Serial.println(F("- SET_MAX_SPEED <value>: Set maximum speed and save to memory (0 < value <= 50000)"));
  Serial.println(F("- SET_ACCELERATION <value>: Set acceleration and save to memory (0 < value <= 50000)"));
  Serial.println(F("- SET_HOLD_TIME <ms>: Set motor hold time after move (0-10000 ms)"));
  Serial.println(F("- SET_SPEED <value>: Set homing speed and save to memory (0 < value <= 50000)"));
  Serial.println(F("- SET_HOME_DIR <dir>: Set homing direction (-1 or +1) and save to memory"));
  Serial.println(F("- SET_HYST_POS <value>: Set positive rotation hysteresis compensation steps (X) and save"));
  Serial.println(F("- SET_HYST_NEG <value>: Set negative rotation hysteresis compensation steps (Y) and save"));
  Serial.println(F("- GET_INFO: Display motor configuration (steps, speed, acceleration, hold time, hysteresis)"));
  Serial.println(F("- GET_SWITCH: Read home limit switch status"));
  Serial.println(F("- SET_POS <steps>: Override current position tracking to <steps> without moving motor"));
  Serial.println(F("- HELP: Shows all the Serial Commands"));
  Serial.println(F("========================================================="));
}

// ============================================================================
// NON-BLOCKING MOTOR MOVEMENT
// ============================================================================
void startMotorMovement(long targetPos)
{
  // Normal movement: physical and logical targets are the same.
  startMotorMovement(targetPos, targetPos);
}

void startMotorMovement(long physicalTargetPos, long logicalTargetPos)
{
  if (!stepper)
    return;
  if (readEStop() == HIGH)
    return;
  if (isMotorMoving || homingState != HOMING_IDLE)
  {
    logSmart("Motor busy, ignoring command");
    return;
  }

  enableMotor();

  physicalTargetPos = constrain(physicalTargetPos, -MAX_POSITION, MAX_POSITION);
  logicalTargetPos = constrain(logicalTargetPos, -MAX_POSITION, MAX_POSITION);
  long curPos = getCurrentPosition();

  // If the requested physical movement is zero, update the logical coordinate only.
  if (physicalTargetPos == curPos)
  {
    currentPosition = logicalTargetPos;
    stepper->setCurrentPosition(currentPosition);
    motorTargetPosition = currentPosition;
    preferences.putLong("currPos", currentPosition);
    logSmart("Already at target position. Logical position: " + String(currentPosition));
    updateMenuDisplay();
    return;
  }

  // The position that must be reported after the motor physically arrives.
  // For normal moves this equals physicalTargetPos. For direct CW/CCW moves
  // this is the selected saved position, while physicalTargetPos may be an
  // unwrapped coordinate used only to force the requested rotation direction.
  motorTargetPosition = logicalTargetPos;

  // Hysteresis compensation is proportional to the commanded rotation.
  // A full revolution receives 100% of the configured hysteresis; half a
  // revolution receives 50%, quarter revolution 25%, etc.
  long movementSteps = labs(physicalTargetPos - curPos);
  float rotationFraction = (STEPS_PER_REV > 0)
                             ? ((float)movementSteps / (float)STEPS_PER_REV)
                             : 0.0f;

  long physicalTarget = physicalTargetPos;
  long hysteresisComp = 0;

  if (physicalTargetPos > curPos)
  {
    hysteresisComp = lroundf((float)HYSTERESIS_POS_STEPS * rotationFraction);
    physicalTarget += hysteresisComp;
  }
  else if (physicalTargetPos < curPos)
  {
    hysteresisComp = lroundf((float)HYSTERESIS_NEG_STEPS * rotationFraction);
    physicalTarget -= hysteresisComp;
  }

  physicalTarget = constrain(physicalTarget, -MAX_POSITION, MAX_POSITION);

  stepper->setSpeedInHz((uint32_t)MAX_SPEED);
  stepper->setAcceleration((uint32_t)ACCELERATION);
  stepper->moveTo(physicalTarget);

  isMotorMoving = true;
  motorMoveStartTime = millis();

  logSmart("Move physical target: " + String(physicalTargetPos) +
           ", logical target: " + String(logicalTargetPos) +
           ", hysteresis: " + String(hysteresisComp) + " steps");
}

void updateMotorMovement()
{
  if (!stepper || !isMotorMoving)
    return;

  if (!stepper->isRunning())
  {
    isMotorMoving = false;
    currentPosition = motorTargetPosition; // Nominal logical coordinate
    stepper->setCurrentPosition(currentPosition); // Sync internal coordinate to nominal
    preferences.putLong("currPos", currentPosition);

    logSmart("Movement complete. Position: " + String(currentPosition));
    updateMenuDisplay();
    // Motor remains energized between movements for holding torque and repeatability
  }
}

// ============================================================================
// POSITION MANAGEMENT
// ============================================================================
void saveCurrentPosition()
{
  currentPosition = getCurrentPosition();
  preferences.putLong("currPos", currentPosition);
  preferences.putLong("homePos", homePosition);
  logSmart("Current position saved: " + String(currentPosition));
}

void savePositionToSlot(int num)
{
  if (num >= 0 && num < 5)
  {
    currentPosition = getCurrentPosition();
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
    long targetSlot = savedPositions[num];
    logSmart("Loading position from slot " + String(num) + ": " + String(targetSlot));
    if (getCurrentPosition() != targetSlot)
    {
      startMotorMovement(targetSlot);
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
  if (abs(targetPosition - getCurrentPosition()) < 1)
  {
    logSmart("Already at target position");
    return;
  }
  startMotorMovement(targetPosition);
}

void home()
{
  logSmart("Moving to position 0...");
  startMotorMovement(0);
}

void resetHome()
{
  if (!stepper)
    return;
  homePosition = getCurrentPosition();
  currentPosition = 0;
  stepper->setCurrentPosition(0);
  preferences.putLong("homePos", homePosition);
  preferences.putLong("currPos", currentPosition);
  logSmart("Home reset. Current position calibrated to 0");
  updateMenuDisplay();
}

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

void setHysteresisPos(int value)
{
  if (value >= 0 && value <= MAX_JOG_STEPS)
  {
    HYSTERESIS_POS_STEPS = value;
    preferences.putInt("hystPos", HYSTERESIS_POS_STEPS);
    logSmart("Positive hysteresis (CW) set to: " + String(HYSTERESIS_POS_STEPS) + " steps");
  }
  else
  {
    logSmart("ERROR: Positive hysteresis must be 0 <= steps <= " + String(MAX_JOG_STEPS));
  }
}

void setHysteresisNeg(int value)
{
  if (value >= 0 && value <= MAX_JOG_STEPS)
  {
    HYSTERESIS_NEG_STEPS = value;
    preferences.putInt("hystNeg", HYSTERESIS_NEG_STEPS);
    logSmart("Negative hysteresis (CCW) set to: " + String(HYSTERESIS_NEG_STEPS) + " steps");
  }
  else
  {
    logSmart("ERROR: Negative hysteresis must be 0 <= steps <= " + String(MAX_JOG_STEPS));
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
  if (abs(steps) <= MAX_POSITION)
  {
    testSteps = steps;
    testDirection = true;
    isTesting = true;
    lastTestTime = micros();
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
  currentPosition = getCurrentPosition();

  logSmart("Test stopped at position: " + String(currentPosition));
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
// Accurately calculates target absolute position for rotational moves (CW / CCW)
// ============================================================================
long calculateGoToSavedPosition(long currentPos, long targetPos, long stepsPerRev, const String &direction)
{
  if (stepsPerRev <= 0)
    stepsPerRev = 12800;

  // Normalize positions to [0, stepsPerRev)
  long normCurrent = currentPos % stepsPerRev;
  if (normCurrent < 0)
    normCurrent += stepsPerRev;

  long normTarget = targetPos % stepsPerRev;
  if (normTarget < 0)
    normTarget += stepsPerRev;

  if (direction == "CW")
  {
    long delta = normTarget - normCurrent;
    if (delta <= 0)
      delta += stepsPerRev;
    return currentPos + delta;
  }
  else // CCW
  {
    long delta = normTarget - normCurrent;
    if (delta >= 0)
      delta -= stepsPerRev;
    return currentPos + delta;
  }
}

void handleDirectButtons()
{
  static unsigned long lastDirectKeyTime = 0;
  static unsigned long lastRotationKeyTime = 0;
  static int lastRotationKey = 0;
  unsigned long currentTime = millis();

  // Position selection (GPIO34) - 5 buttons for slots 0-4
  int posKey = readDirectKeypad();
  if (posKey != lastDirectKey && (currentTime - lastDirectKeyTime > debounceDelay))
  {
    lastDirectKey = posKey;
    lastDirectKeyTime = currentTime;

    if (posKey >= 1 && posKey <= 5)
    { // Buttons 1-5 -> slots 0-4
      selectedPositionIndex = posKey - 1;
      logSmart("Position slot " + String(selectedPositionIndex) + " selected (Saved Pos: " + String(savedPositions[selectedPositionIndex]) + ")");
    }
  }

  // Rotation direction buttons (GPIO36) - CCW and CW
  int rotKey = readRotationKeypad();
  if (rotKey != lastRotationKey && (currentTime - lastRotationKeyTime > debounceDelay))
  {
    lastRotationKey = rotKey;
    lastRotationKeyTime = currentTime;

    if (rotKey >= 1 && rotKey <= 2)
    { // Button 1 = CCW, Button 2 = CW
      if (selectedPositionIndex < 0 || selectedPositionIndex >= 5)
      {
        logSmart("ERROR: Select a position first (buttons 1-5)");
        return;
      }

      String dir = (rotKey == 1) ? "CCW" : "CW";
      long currentPos = getCurrentPosition();
      long targetSlotPos = savedPositions[selectedPositionIndex];

      // Calculate absolute target position to move in the desired rotational direction
      long absoluteTarget = calculateGoToSavedPosition(currentPos, targetSlotPos, STEPS_PER_REV, dir);

      logSmart("Going to slot " + String(selectedPositionIndex) + " (" + String(targetSlotPos) + ") moving " + dir + " -> Physical Target: " + String(absoluteTarget) + ", Logical Target: " + String(targetSlotPos));
      startMotorMovement(absoluteTarget, targetSlotPos);
    }
  }

  updateMenuDisplay();
}

long calculateJogToPosition(long currentPos, long targetPos, long fullRotation, bool rotationDirection)
{
  if (fullRotation <= 0)
    fullRotation = STEPS_PER_REV;

  long normCurrent = currentPos % fullRotation;
  if (normCurrent < 0)
    normCurrent += fullRotation;

  long normTarget = targetPos % fullRotation;
  if (normTarget < 0)
    normTarget += fullRotation;

  if (rotationDirection) // CW
  {
    long delta = normTarget - normCurrent;
    if (delta <= 0)
      delta += fullRotation;
    return delta;
  }
  else // CCW
  {
    long delta = normTarget - normCurrent;
    if (delta >= 0)
      delta -= fullRotation;
    return delta;
  }
}

// ============================================================================
// LCD AND KEYPAD INPUT
// ============================================================================
int readKeypad()
{
  static int readingsK[3] = {0, 0, 0};
  static int index = 0;

  int currentReadingK = analogRead(KEYPAD_PIN);
  readingsK[index] = currentReadingK;
  index = (index + 1) % 3;

  int sum = readingsK[0] + readingsK[1] + readingsK[2];
  int avgValue = sum / 3;

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
  readingsD[index] = currentReading;
  index = (index + 1) % 3;

  int sum = readingsD[0] + readingsD[1] + readingsD[2];
  int avgValue = sum / 3;

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
  }

  return key;
}

int readRotationKeypad()
{
  static int readingsR[3] = {0, 0, 0};
  static int index = 0;

  int currentReading = analogRead(ROTATION_KEYPAD_PIN);
  readingsR[index] = currentReading;
  index = (index + 1) % 3;

  int sum = readingsR[0] + readingsR[1] + readingsR[2];
  int avgValue = sum / 3;

  float variance = 0;
  for (int i = 0; i < 3; i++)
  {
    variance += pow(readingsR[i] - avgValue, 2);
  }
  variance /= 3;
  float stdDev = sqrt(variance);

  int key = 0;
  if (stdDev < 50)
  {
    if (avgValue < ROTATION_KEYPAD_THRESHOLD)
      key = 1;  // CCW button (25% region)
    else
      key = 2;  // CW button (75% region)
  }

  return key;
}

int readHomeSwitch()
{
  static int readingsH[4] = {4095, 4095, 4095, 4095};
  static int index = 0;

  int currentReadingH = analogRead(HOME_SWITCH_PIN);
  readingsH[index] = currentReadingH;
  index = (index + 1) & 3;

  int sum = readingsH[0] + readingsH[1] + readingsH[2] + readingsH[3];
  int avgValue = sum >> 2;

  int triggered = (avgValue < HOME_SWITCH_THRESHOLD) ? 1 : 0;
  digitalWrite(LED_HOME_PIN, triggered ? HIGH : LOW);

  return triggered;
}

// ============================================================================
// LCD MENU DISPLAY
// ============================================================================
void updateMenuDisplay()
{
  long pos = getCurrentPosition(); // Always fresh from stepper
  static HomingState lastHomingState = HOMING_IDLE;
  bool positionChanged = (pos != lastDisplayedPosition);
  bool menuStateChanged = (inSubMenu != lastMenuWasSubMenu) || (menuIndex != lastDisplayedIndex);
  bool selChanged = (selectedPositionIndex != lastSelectedPosIndex);
  bool moveChanged = (isMotorMoving != lastMotorState);
  bool homingChanged = (homingState != lastHomingState);
  lastSelectedPosIndex = selectedPositionIndex;
  lastMotorState = isMotorMoving;
  lastHomingState = homingState;

  // Skip refresh if nothing changed (saves I2C bus time)
  if (!positionChanged && !menuStateChanged && !selChanged && !moveChanged && !homingChanged && !inSubMenu)
  {
    return;
  }

  lcd.clear();

  // PRIORITY 1: If homing, show homing status prominently
  if (homingState != HOMING_IDLE)
  {
    lcd.setCursor(0, 0);
    lcd.print(F("** HOMING... ** "));
    lcd.setCursor(0, 1);
    String posStr = "Pos: " + String(pos);
    if (posStr.length() > 16)
      posStr = posStr.substring(0, 16);
    lcd.print(posStr);

    lastDisplayedPosition = pos;
    lastMenuWasSubMenu = inSubMenu;
    lastDisplayedIndex = menuIndex;
    return;
  }

  // PRIORITY 2: Show direct button state prominently
  if (selectedPositionIndex >= 0 && selectedPositionIndex < 5)
  {
    lcd.setCursor(0, 0);
    lcd.print(F("Pos "));
    lcd.print(selectedPositionIndex+1);
    lcd.print(F("="));

    String slotPosStr = String(savedPositions[selectedPositionIndex]);
    if (slotPosStr.length() > 10)
      slotPosStr = slotPosStr.substring(0, 10);
    lcd.print(slotPosStr);

    lcd.setCursor(0, 1);
    if (isMotorMoving)
    {
      lcd.print(F("Moving..."));
    }
    else
    {
      String curPosStr = String(pos);
      if (curPosStr.length() > 16)
        curPosStr = curPosStr.substring(0, 16);
      lcd.print(F("Cur: "));
      lcd.print(curPosStr);
    }

    lastDisplayedPosition = pos;
    lastMenuWasSubMenu = inSubMenu;
    lastDisplayedIndex = menuIndex;
    return;
  }

  if (!inSubMenu)
  {
    // Main menu: show menu item and current position
    lcd.setCursor(0, 0);
    lcd.print(F("> "));
    String menuText = menuItems[menuIndex];
    if (menuText.length() > 14)
      menuText = menuText.substring(0, 14);
    lcd.print(menuText);

    lcd.setCursor(0, 1);
    String posStr = "Pos: " + String(pos);
    if (posStr.length() > 16)
      posStr = posStr.substring(0, 16);
    lcd.print(posStr);
  }
  else
  {
    switch (subMenuType)
    {
      case GOTO_SAVED:
        lcd.setCursor(0, 0);
        lcd.print(F("Goto saved:"));
        lcd.setCursor(0, 1);
        lcd.print(inputValue);
        break;

      case SPEED:
        lcd.setCursor(0, 0);
        lcd.print(F("Speed:    "));
        lcd.setCursor(0, 1);
        lcd.print(inputValue);
        break;

      case ACCEL:
        lcd.setCursor(0, 0);
        lcd.print(F("Accel:    "));
        lcd.setCursor(0, 1);
        lcd.print(inputValue);
        break;

      case RESET_HOME:
      case CONFIRM_RESET_HOME:
        lcd.setCursor(0, 0);
        lcd.print(F("Reset home?"));
        lcd.setCursor(0, 1);
        lcd.print(inputDirection ? F("No") : F("Yes"));
        break;

      case SAVE_POS:
      case CONFIRM_SAVE_POS:
        lcd.setCursor(0, 0);
        lcd.print(F("Save to slot:"));
        lcd.setCursor(0, 1);
        lcd.print(inputValue);
        break;

      case GOTO:
        lcd.setCursor(0, 0);
        lcd.print(F("Go to pos:"));
        lcd.setCursor(0, 1);
        lcd.print(inputValue);
        break;

      case JOG:
        lcd.setCursor(0, 0);
        lcd.print(F("Jog steps:"));
        lcd.setCursor(0, 1);
        lcd.print(inputValue);
        break;

      default:
        break;
    }
  }

  lastDisplayedPosition = pos;
  lastMenuWasSubMenu = inSubMenu;
  lastDisplayedIndex = menuIndex;
}

// ============================================================================
// MENU HANDLING
// ============================================================================
void enterSubMenu()
{
  inSubMenu = true;

  switch (menuIndex)
  {
    case 0:
      subMenuType = GOTO_SAVED;
      inputValue = 0;
      break;
    case 1:
      subMenuType = SPEED;
      inputValue = (long)MAX_SPEED;
      break;
    case 2:
      subMenuType = ACCEL;
      inputValue = (long)ACCELERATION;
      break;
    case 3:
      home();
      inSubMenu = false;
      subMenuType = NONE;
      break;
    case 4:
      subMenuType = CONFIRM_RESET_HOME;
      inputDirection = true;
      break;
    case 5:
      subMenuType = SAVE_POS;
      inputValue = 0;
      break;
    case 6:
      subMenuType = GOTO;
      inputValue = getCurrentPosition();
      break;
    case 7:
      subMenuType = JOG;
      inputValue = JOG_STEPS;
      break;
  }

  updateMenuDisplay();
}

void executeMenuAction()
{
  switch (subMenuType)
  {
    case GOTO_SAVED:
      loadPositionFromSlot((int)inputValue);
      break;

    case SPEED:
      setMaxSpeed((float)inputValue);
      break;

    case ACCEL:
      setAcceleration((float)inputValue);
      break;

    case CONFIRM_RESET_HOME:
      if (!inputDirection)
        resetHome();
      break;

    case SAVE_POS:
    case CONFIRM_SAVE_POS:
      savePositionToSlot((int)inputValue);
      break;

    case GOTO:
      moveToPosition(inputValue);
      break;

    case JOG:
      JOG_STEPS = inputValue;
      break;

    default:
      break;
  }

  inSubMenu = false;
  subMenuType = NONE;
  updateMenuDisplay();
}

void handleMenu()
{
  int key = readKeypad();
  unsigned long currentTime = millis();

  if (key != lastKey && (currentTime - lastKeyTime > debounceDelay))
  {
    lastKey = key;
    lastKeyTime = currentTime;

    if (!inSubMenu)
    {
      if (key == 1)
      {
        menuIndex = (menuIndex - 1 + menuSize) % menuSize;
      }
      else if (key == 2)
      {
        menuIndex = (menuIndex + 1) % menuSize;
      }
      else if (key == 5)
      {
        enterSubMenu();
      }
    }
    else
    {
      if (key == 4)
      {
        inSubMenu = false;
        subMenuType = NONE;
      }
      else if (key == 5)
      {
        executeMenuAction();
      }
      else
      {
        switch (subMenuType)
        {
          case GOTO_SAVED:
          case SAVE_POS:
          case CONFIRM_SAVE_POS:
            if (key == 1 && inputValue > 0)
              inputValue--;
            if (key == 2 && inputValue < 4)
              inputValue++;
            break;

          case SPEED:
          case ACCEL:
          case GOTO:
          case JOG:
            if (key == 1)
              inputValue -= 10;
            if (key == 2)
              inputValue += 10;
            if (key == 3)
              inputValue -= 100;
            if (key == 4)
              inputValue += 100;
            break;

          case CONFIRM_RESET_HOME:
            if (key == 1 || key == 2)
              inputDirection = !inputDirection;
            break;

          default:
            break;
        }
      }
    }

    updateMenuDisplay();
  }
}
