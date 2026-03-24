#pragma once
#include <Arduino.h>

void stepper_init();
void stepper_update();                // call every loop()
void stepper_unload();                // push -> 2s pause -> auto retract
void stepper_push();                  // both motors extend to push luggage
void stepper_retract();               // both motors return to start
void stepper_stop();                  // emergency stop both motors
void stepper_printPos();              // print both motor positions
void stepper_moveMotor1(long steps);  // move motor 1 independently
void stepper_moveMotor2(long steps);  // move motor 2 independently
void stepper_setHome();               // lock current positions as home