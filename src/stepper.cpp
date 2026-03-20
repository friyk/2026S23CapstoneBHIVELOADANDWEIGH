#include "stepper.h"
#include <AccelStepper.h>

// -------------------------------------------------------
// Config — tune these for your setup
// -------------------------------------------------------
const int STEP_PIN     = 32;     // DM542 PUL
const int DIR_PIN      = 33;     // DM542 DIR

const float MAX_SPEED    = 2000; // steps/sec
const float ACCELERATION = 800;  // steps/sec^2

// Distance from one end to the other in steps.
// 1600 steps = 1 revolution at your DM542 dip switch setting.
// Increase this until the carriage reaches the far end.
const long TRAVEL_STEPS = 3200;  // <-- tune this (e.g. 3200 = 2 revolutions)

// -------------------------------------------------------
// Globals
// -------------------------------------------------------
static AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);
static boolean running  = false;
static boolean goingFwd = true;  // current direction

// -------------------------------------------------------
// Init
// -------------------------------------------------------
void stepper_init() {
  stepper.setMaxSpeed(MAX_SPEED);
  stepper.setAcceleration(ACCELERATION);
  stepper.setCurrentPosition(0); // current position = home (end A)
  Serial.println("Stepper ready.");
  Serial.println("Commands: 'go' = start, 'stop' = stop, 'p' = position");
}

// -------------------------------------------------------
// Update — must be called every loop(), as fast as possible
// -------------------------------------------------------
void stepper_update() {
  if (!running) return;

  stepper.run();

  // When target reached, reverse direction and go again
  if (stepper.distanceToGo() == 0) {
    goingFwd = !goingFwd;
    long target = goingFwd ? TRAVEL_STEPS : 0;
    stepper.moveTo(target);
    Serial.print("Reversing -> moving to position ");
    Serial.println(target);
  }
}

// -------------------------------------------------------
// Commands
// -------------------------------------------------------
void stepper_start() {
  if (running) {
    Serial.println("Already running.");
    return;
  }
  running  = true;
  goingFwd = true;
  stepper.setCurrentPosition(0);
  stepper.moveTo(TRAVEL_STEPS);
  Serial.println("Stepper started — back and forth.");
}

void stepper_stop() {
  running = false;
  stepper.stop();
  stepper.setCurrentPosition(stepper.currentPosition());
  Serial.println("Stepper stopped.");
}

void stepper_printPos() {
  Serial.print("Stepper position: ");
  Serial.print(stepper.currentPosition());
  Serial.println(" steps");
}