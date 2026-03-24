#include "stepper.h"
#include <AccelStepper.h>
#include <EEPROM.h>

// EEPROM addresses for home positions (each long = 4 bytes)
const int EEPROM_HOME1_ADDR = 10; // offset from 0 to avoid collision with loadcell cal factor at 0
const int EEPROM_HOME2_ADDR = 14;
const int EEPROM_POS1_ADDR  = 18; // last known position of motor 1
const int EEPROM_POS2_ADDR  = 22; // last known position of motor 2

// -------------------------------------------------------
// Motor 1 config — DM542 #1
// -------------------------------------------------------
const int STEP_PIN_1 = 32;
const int DIR_PIN_1  = 33;

// -------------------------------------------------------
// Motor 2 config — DM542 #2
// -------------------------------------------------------
const int STEP_PIN_2 = 22;
const int DIR_PIN_2  = 23;

// -------------------------------------------------------
// Motion config
// We want to move the baggage off in 10s, actuator has to 
// move a distance of 70cm, with every 1600 pulse/rev moving
// it by 10mm (driver set to 1600 pulse/rev).
// Through calculation, we can determine that the speed it
// has to reach is 11200 steps/sec, 112000 steps total
// 1 step = 1 pulse/rev
// -------------------------------------------------------
const float MAX_SPEED    = 11200; // steps/sec, given operation is 10s long
const float ACCELERATION = 2500;   // steps/sec^2 
const long  TRAVEL_STEPS = 112000; // steps for full push distance

// -------------------------------------------------------
// State
// -------------------------------------------------------
typedef enum {
  IDLE,
  PUSHING,
  RETRACTING,
  UNLOADING,
  UNLOAD_PAUSE,
  UNLOAD_RETRACTING
} MotorState;

static AccelStepper motor1(AccelStepper::DRIVER, STEP_PIN_1, DIR_PIN_1);
static AccelStepper motor2(AccelStepper::DRIVER, STEP_PIN_2, DIR_PIN_2);
static MotorState state = IDLE;

static long home1 = 0;
static long home2 = 0;
static unsigned long pauseStart = 0;

// -------------------------------------------------------
// Init
// -------------------------------------------------------
void stepper_init() {
  motor1.setMaxSpeed(MAX_SPEED);
  motor1.setAcceleration(ACCELERATION);
  motor2.setMaxSpeed(MAX_SPEED);
  motor2.setAcceleration(ACCELERATION);

  // Load saved home positions from EEPROM
  EEPROM.get(EEPROM_HOME1_ADDR, home1);
  EEPROM.get(EEPROM_HOME2_ADDR, home2);

  // Validate — if EEPROM was never written, values will be garbage
  if (isnan((float)home1) || home1 > 10000000 || home1 < -10000000) home1 = 0;
  if (isnan((float)home2) || home2 > 10000000 || home2 < -10000000) home2 = 0;

  // Load last known motor positions (where they stopped on last power off)
  long lastPos1, lastPos2;
  EEPROM.get(EEPROM_POS1_ADDR, lastPos1);
  EEPROM.get(EEPROM_POS2_ADDR, lastPos2);

  if (isnan((float)lastPos1) || lastPos1 > 10000000 || lastPos1 < -10000000) lastPos1 = home1;
  if (isnan((float)lastPos2) || lastPos2 > 10000000 || lastPos2 < -10000000) lastPos2 = home2;

  // Set current position to where motors actually are physically
  motor1.setCurrentPosition(lastPos1);
  motor2.setCurrentPosition(lastPos2);

  Serial.println("Stepper ready (2 motors).");
  Serial.print("Home1: "); Serial.print(home1); Serial.print(" | Last pos1: "); Serial.println(lastPos1);
  Serial.print("Home2: "); Serial.print(home2); Serial.print(" | Last pos2: "); Serial.println(lastPos2);

  // If motors are not at home, prompt user to retract
  if (lastPos1 != home1 || lastPos2 != home2) {
    Serial.println("WARNING: Motors are not at home. Send 'retract' to return.");
  }
}

// -------------------------------------------------------
// Update — call every loop(), as fast as possible
// -------------------------------------------------------
void stepper_update() {
  motor1.run();
  motor2.run();

  if (state == PUSHING) {
    if (motor1.distanceToGo() == 0 && motor2.distanceToGo() == 0) {
      Serial.println("Push complete. Send 'retract' to return.");
      state = IDLE;
    }
  }

  if (state == RETRACTING) {
    if (motor1.distanceToGo() == 0 && motor2.distanceToGo() == 0) {
      Serial.println("Retract complete. Ready for next push.");
      EEPROM.put(EEPROM_POS1_ADDR, home1);
      EEPROM.put(EEPROM_POS2_ADDR, home2);
      EEPROM.commit();
      state = IDLE;
    }
  }

  // Unload sequence: push -> pause 2s -> retract automatically
  if (state == UNLOADING) {
    if (motor1.distanceToGo() == 0 && motor2.distanceToGo() == 0) {
      Serial.println("Unload push complete. Waiting 2 seconds...");
      pauseStart = millis();
      state = UNLOAD_PAUSE;
    }
  }

  if (state == UNLOAD_PAUSE) {
    if (millis() - pauseStart >= 2000) {
      motor1.moveTo(home1);
      motor2.moveTo(home2);
      state = UNLOAD_RETRACTING;
      Serial.println("Retracting...");
    }
  }

  if (state == UNLOAD_RETRACTING) {
    if (motor1.distanceToGo() == 0 && motor2.distanceToGo() == 0) {
      Serial.println("Unload complete. Ready for next luggage.");
      EEPROM.put(EEPROM_POS1_ADDR, home1);
      EEPROM.put(EEPROM_POS2_ADDR, home2);
      EEPROM.commit();
      state = IDLE;
    }
  }
}

// -------------------------------------------------------
// Synchronized push/retract using AccelStepper
// Both motors get identical targets so they stay in sync
// -------------------------------------------------------
void stepper_unload() {
  if (state != IDLE) {
    Serial.println("Motors busy. Send 'stop' first.");
    return;
  }
  motor1.moveTo(home1 + TRAVEL_STEPS);
  motor2.moveTo(home2 + TRAVEL_STEPS);
  state = UNLOADING;
  Serial.println("Unloading — will auto retract after 2s pause...");
}

void stepper_push() {
  if (state != IDLE) {
    Serial.println("Motors busy. Send 'stop' first.");
    return;
  }
  motor1.moveTo(home1 + TRAVEL_STEPS);
  motor2.moveTo(home2 + TRAVEL_STEPS);
  state = PUSHING;
  Serial.println("Pushing...");
}

void stepper_retract() {
  if (state != IDLE) {
    Serial.println("Motors busy. Send 'stop' first.");
    return;
  }
  motor1.moveTo(home1);
  motor2.moveTo(home2);
  state = RETRACTING;
  Serial.println("Retracting...");
}

void stepper_stop() {
  motor1.stop();
  motor2.stop();
  motor1.setCurrentPosition(motor1.currentPosition());
  motor2.setCurrentPosition(motor2.currentPosition());
  state = IDLE;
  Serial.println("Emergency stop. Both motors halted.");
  Serial.println("Send 'retract' to return to home.");
}

// -------------------------------------------------------
// Independent moves
// -------------------------------------------------------
void stepper_moveMotor1(long steps) {
  if (state != IDLE) {
    Serial.println("Motors busy. Send 'stop' first.");
    return;
  }
  motor1.move(steps);
  Serial.print("Motor 1 moving ");
  Serial.print(steps);
  Serial.println(" steps...");
}

void stepper_moveMotor2(long steps) {
  if (state != IDLE) {
    Serial.println("Motors busy. Send 'stop' first.");
    return;
  }
  motor2.move(steps);
  Serial.print("Motor 2 moving ");
  Serial.print(steps);
  Serial.println(" steps...");
}

void stepper_setHome() {
  if (state != IDLE) {
    Serial.println("Motors busy. Send 'stop' first.");
    return;
  }
  home1 = motor1.currentPosition();
  home2 = motor2.currentPosition();

  // Save to EEPROM so it persists across power cycles
  EEPROM.put(EEPROM_HOME1_ADDR, home1);
  EEPROM.put(EEPROM_HOME2_ADDR, home2);
  EEPROM.commit();

  Serial.println("Home set and saved to EEPROM.");
  Serial.print("Home1: "); Serial.println(home1);
  Serial.print("Home2: "); Serial.println(home2);
}

void stepper_printPos() {
  Serial.print("Motor 1 position: "); Serial.print(motor1.currentPosition()); Serial.println(" steps");
  Serial.print("Motor 2 position: "); Serial.print(motor2.currentPosition()); Serial.println(" steps");
}