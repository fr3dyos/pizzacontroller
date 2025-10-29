// PizzaController.ino - ESP32 Program to Control NEMA23 Stepper Motor with DM556 Driver
// This program demonstrates basic stepper motor control with maximum microstepping (1/256) for precise positioning.
// It includes functions for jogging, saving positions, homing, and resetting home position.
// Author: Eng. Fredy Osorio <ing.fredyosorio@gmail.com>

// Step 1: Include necessary libraries
#include <Preferences.h>  // For saving positions to non-volatile memory
// If using AccelStepper, uncomment the line below:
// #include <AccelStepper.h>

// Step 2: Define pin connections for ESP32 to DM556 driver
// These pins are GPIO pins on ESP32. Adjust based on your wiring.
#define STEP_PIN 18      // Connect to STEP input on DM556
#define DIR_PIN 19       // Connect to DIR input on DM556
#define ENABLE_PIN 21    // Connect to ENABLE input on DM556 (active low)
#define MS1_PIN 22       // Connect to MS1 on DM556 for microstepping
#define MS2_PIN 23       // Connect to MS2 on DM556
#define MS3_PIN 25       // Connect to MS3 on DM556
#define HOME_SWITCH_PIN 26 // Connect to home limit switch (active low)

// Step 3: Define motor parameters
// NEMA23 typically has 200 steps per revolution (full step).
// With 1/256 microstepping, total steps per revolution = 200 * 256 = 51200
const int STEPS_PER_REV = 51200;  // Steps per revolution at 1/256 microstep
const int SPEED_DELAY = 250;      // Delay between steps in microseconds (adjust for speed)

// Step 3.1: Global variables for position tracking
long currentPosition = 0;         // Current position in steps from home
long homePosition = 0;            // Home position offset
long savedPositions[5] = {0};     // Array for up to 5 saved positions
Preferences preferences;          // For saving positions to flash memory

// Step 4: Setup function - runs once at startup
void setup() {
  // Step 4.1: Set pin modes
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  pinMode(MS1_PIN, OUTPUT);
  pinMode(MS2_PIN, OUTPUT);
  pinMode(MS3_PIN, OUTPUT);
  pinMode(HOME_SWITCH_PIN, INPUT_PULLUP);  // Home switch with internal pull-up

  // Step 4.2: Configure microstepping to 1/256 (smallest possible for maximum control)
  // DM556 microstep settings: MS1=1, MS2=1, MS3=1 for 1/256
  digitalWrite(MS1_PIN, HIGH);
  digitalWrite(MS2_PIN, HIGH);
  digitalWrite(MS3_PIN, HIGH);

  // Step 4.3: Enable the driver (ENABLE is active low, so set to LOW)
  digitalWrite(ENABLE_PIN, LOW);

  // Step 4.4: Set initial direction (clockwise)
  digitalWrite(DIR_PIN, LOW);

  // Optional: Initialize serial for debugging
  Serial.begin(115200);
  Serial.println("Stepper Motor Controller Initialized");

  // Step 4.5: Initialize preferences for position storage
  preferences.begin("stepper", false);  // Namespace "stepper", read-write mode
  homePosition = preferences.getLong("homePos", 0);
  currentPosition = preferences.getLong("currPos", 0);
  for (int i = 0; i < 5; i++) {
    savedPositions[i] = preferences.getLong(("pos" + String(i)).c_str(), 0);
  }
  Serial.print("Home position loaded: ");
  Serial.println(homePosition);
  Serial.print("Current position loaded: ");
  Serial.println(currentPosition);
  Serial.println("Saved positions loaded");
}

// Step 5: Loop function - runs repeatedly
void loop() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    processCommand(command);
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
  } else {
    Serial.println("Unknown command");
  }
}

// Step 6: Advanced control functions

// Function to move a specific number of steps in a given direction
// direction: true for clockwise, false for counterclockwise
void moveSteps(long steps, bool direction) {
  digitalWrite(ENABLE_PIN, LOW);  // Enable driver before moving
  digitalWrite(DIR_PIN, direction ? LOW : HIGH);  // LOW = clockwise, HIGH = counterclockwise
  for (long i = 0; i < steps; i++) {
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(SPEED_DELAY);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(SPEED_DELAY);
  }
  digitalWrite(ENABLE_PIN, HIGH);  // Disable driver after moving to prevent overheating
  // Update current position
  currentPosition += direction ? steps : -steps;
  preferences.putLong("currPos", currentPosition);  // Save current position after every move
  Serial.print("Moved to position: ");
  Serial.println(currentPosition);
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
    long stepsToMove = targetPos - currentPosition;
    bool direction = (stepsToMove > 0) ? true : false;
    long absSteps = abs(stepsToMove);
    moveSteps(absSteps, direction);
    Serial.println(currentPosition);
  }
}

// Move to absolute position
void moveToPosition(long targetPosition) {
  long stepsToMove = targetPosition - currentPosition;
  bool direction = (stepsToMove > 0) ? true : false;
  long absSteps = abs(stepsToMove);
  Serial.print("Moving to absolute position ");
  Serial.print(targetPosition);
  Serial.print(": ");
  Serial.print(absSteps);
  Serial.println(direction ? " steps clockwise" : " steps counterclockwise");
  moveSteps(absSteps, direction);
  Serial.println(currentPosition);
}

// Home function: Move to home position (position 0)
// Assumes home is at current position when called, or implement limit switch logic
void home() {
  long stepsToHome = -currentPosition;  // Steps needed to reach home (0)
  bool direction = (stepsToHome > 0) ? true : false;  // true = clockwise if positive
  long absSteps = abs(stepsToHome);

  Serial.print("Homing: moving ");
  Serial.print(absSteps);
  Serial.println(direction ? " steps clockwise" : " steps counterclockwise");

  moveSteps(absSteps, direction);
  currentPosition = 0;  // Set current position to home
  savePosition();
  Serial.println("Homed to position 0");
  Serial.println(currentPosition);
}

// Reset home function: Set current position as new home (position 0)
void resetHome() {
  homePosition = currentPosition;
  currentPosition = 0;
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
  digitalWrite(DIR_PIN, homeDirection ? LOW : HIGH);

  // Move until limit switch is triggered
  while (digitalRead(HOME_SWITCH_PIN) == HIGH) {  // Assuming active low switch
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(SPEED_DELAY);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(SPEED_DELAY);
  }

  digitalWrite(ENABLE_PIN, HIGH);  // Disable driver after moving to prevent overheating

  // Stop and set position to 0
  currentPosition = 0;
  preferences.putLong("currPos", currentPosition);
  Serial.println("Home found and set to position 0");
  Serial.println(currentPosition);
}

// Get current position relative to home
long getCurrentPosition() {
  return currentPosition;
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
