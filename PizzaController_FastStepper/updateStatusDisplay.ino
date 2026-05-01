void updateStatusDisplay() {
  // LCD2 status display - shows selected position and motor state
  static unsigned long lastStatusUpdate = 0;
  if (millis() - lastStatusUpdate < 200) return;  // Update every 200ms
  lastStatusUpdate = millis();

  String line1 = "Ready Sel:";
  if (selectedPositionIndex >= 0) {
    line1 += String(selectedPositionIndex);
  } else {
    line1 += "-";
  }

  String line2;
  if (isMotorMoving || isTesting) {
    line2 = "Moving";
  } else if (homingState != HOMING_IDLE) {
    line2 = "Homing";
  } else {
    line2 = "Idle";
  }

//  lcd2.clear();
//  lcd2.setCursor(0, 0);
//  lcd2.print(line1);
//  lcd2.setCursor(0, 1);
//  lcd2.print(line2);
}
