#include "stepper.h"
#include <AccelStepper.h>
#include <EEPROM.h>

// EEPROM addresses (each long = 4 bytes)
const int EEPROM_HOME_ADDR = 10; // offset from 0 to avoid collision with loadcell cal factor at 0
const int EEPROM_POS_ADDR  = 14; // last known position

// -------------------------------------------------------
// Motor config — DM542
// -------------------------------------------------------
const int STEP_PIN = 32;
const int DIR_PIN  = 33;

// -------------------------------------------------------
// Motor 2 config — DM542 #2 (disabled: shares same pins as motor 1,
// causing double-stepping. Re-enable by assigning separate GPIO pins
// e.g. STEP_PIN_2=22, DIR_PIN_2=23 and uncommenting motor2 below.)
// -------------------------------------------------------
// const int STEP_PIN_2 = 22;
// const int DIR_PIN_2  = 23;

// -------------------------------------------------------
// Motion config
// We want to move the baggage off in 10s, actuator has to
// move a distance of 70cm, with every 1600 pulse/rev moving
// it by 10mm (driver set to 1600 pulse/rev).
// Through calculation, we can determine that the speed it
// has to reach is 11200 steps/sec, 112000 steps total
// -------------------------------------------------------
const float MAX_SPEED    = 11200; // steps/sec
const float ACCELERATION = 2500;  // steps/sec^2
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

static AccelStepper motor1(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);
// static AccelStepper motor2(AccelStepper::DRIVER, STEP_PIN_2, DIR_PIN_2);
static MotorState state = IDLE;

static long home1 = 0;
// static long home2 = 0;
static unsigned long pauseStart = 0;

// -------------------------------------------------------
// Init
// -------------------------------------------------------
void stepper_init() {
  motor1.setMaxSpeed(MAX_SPEED);
  motor1.setAcceleration(ACCELERATION);
  // motor2.setMaxSpeed(MAX_SPEED);
  // motor2.setAcceleration(ACCELERATION);

  // Load saved home position from EEPROM
  EEPROM.get(EEPROM_HOME_ADDR, home1);
  if (isnan((float)home1) || home1 > 10000000 || home1 < -10000000) home1 = 0;

  // Load last known position (where motor stopped on last power off)
  long lastPos1;
  EEPROM.get(EEPROM_POS_ADDR, lastPos1);
  if (isnan((float)lastPos1) || lastPos1 > 10000000 || lastPos1 < -10000000) lastPos1 = home1;

  motor1.setCurrentPosition(lastPos1);
  // motor2.setCurrentPosition(lastPos1); // use same position as motor1 on re-enable

  Serial.println("Stepper ready.");
  Serial.print("Home: "); Serial.print(home1); Serial.print(" | Last pos: "); Serial.println(lastPos1);

  if (lastPos1 != home1) {
    Serial.println("WARNING: Motor is not at home. Send 'retract' to return.");
  }
}

// -------------------------------------------------------
// Update — call every loop(), as fast as possible
// -------------------------------------------------------
void stepper_update() {
  motor1.run();
  // motor2.run();

  if (state == PUSHING) {
    if (motor1.distanceToGo() == 0) {
      Serial.println("Push complete. Send 'retract' to return.");
      state = IDLE;
    }
  }

  if (state == RETRACTING) {
    if (motor1.distanceToGo() == 0) {
      Serial.println("Retract complete. Ready for next push.");
      EEPROM.put(EEPROM_POS_ADDR, home1);
      EEPROM.commit();
      state = IDLE;
    }
  }

  // Unload sequence: push -> pause 2s -> retract automatically
  if (state == UNLOADING) {
    if (motor1.distanceToGo() == 0) {
      Serial.println("Unload push complete. Waiting 2 seconds...");
      pauseStart = millis();
      state = UNLOAD_PAUSE;
    }
  }

  if (state == UNLOAD_PAUSE) {
    if (millis() - pauseStart >= 2000) {
      motor1.moveTo(home1);
      // motor2.moveTo(home2);
      state = UNLOAD_RETRACTING;
      Serial.println("Retracting...");
    }
  }

  if (state == UNLOAD_RETRACTING) {
    if (motor1.distanceToGo() == 0) {
      Serial.println("Unload complete. Ready for next luggage.");
      EEPROM.put(EEPROM_POS_ADDR, home1);
      EEPROM.commit();
      state = IDLE;
    }
  }
}

// -------------------------------------------------------
// Push / retract / unload
// -------------------------------------------------------
void stepper_unload() {
  if (state != IDLE) { Serial.println("Motors busy. Send 'stop' first."); return; }
  motor1.moveTo(home1 + TRAVEL_STEPS);
  // motor2.moveTo(home2 + TRAVEL_STEPS);
  state = UNLOADING;
  Serial.println("Unloading — will auto retract after 2s pause...");
}

void stepper_push() {
  if (state != IDLE) { Serial.println("Motors busy. Send 'stop' first."); return; }
  motor1.moveTo(home1 + TRAVEL_STEPS);
  // motor2.moveTo(home2 + TRAVEL_STEPS);
  state = PUSHING;
  Serial.println("Pushing...");
}

void stepper_retract() {
  if (state != IDLE) { Serial.println("Motors busy. Send 'stop' first."); return; }
  motor1.moveTo(home1);
  // motor2.moveTo(home2);
  state = RETRACTING;
  Serial.println("Retracting...");
}

void stepper_stop() {
  motor1.stop();
  motor1.setCurrentPosition(motor1.currentPosition());
  // motor2.stop();
  // motor2.setCurrentPosition(motor2.currentPosition());
  state = IDLE;
  Serial.println("Emergency stop. Motor halted.");
  Serial.println("Send 'retract' to return to home.");
}

// -------------------------------------------------------
// Independent moves
// -------------------------------------------------------
void stepper_moveMotor1(long steps) {
  if (state != IDLE) { Serial.println("Motors busy. Send 'stop' first."); return; }
  motor1.move(steps);
  Serial.print("Motor 1 moving "); Serial.print(steps); Serial.println(" steps...");
}

void stepper_moveMotor2(long steps) {
  // Disabled: uncomment when motor2 has its own GPIO pins assigned
  // if (state != IDLE) { Serial.println("Motors busy. Send 'stop' first."); return; }
  // motor2.move(steps);
  // Serial.print("Motor 2 moving "); Serial.print(steps); Serial.println(" steps...");
  Serial.println("Motor 2 disabled. Assign separate GPIO pins and uncomment in stepper.cpp.");
}

void stepper_setHome() {
  if (state != IDLE) { Serial.println("Motors busy. Send 'stop' first."); return; }
  home1 = motor1.currentPosition();
  // home2 = motor2.currentPosition();
  EEPROM.put(EEPROM_HOME_ADDR, home1);
  EEPROM.commit();
  Serial.println("Home set and saved to EEPROM.");
  Serial.print("Home: "); Serial.println(home1);
}

void stepper_printPos() {
  Serial.print("Motor 1 position: "); Serial.print(motor1.currentPosition()); Serial.println(" steps");
  // Serial.print("Motor 2 position: "); Serial.print(motor2.currentPosition()); Serial.println(" steps");
}
