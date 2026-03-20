#pragma once
#include <Arduino.h>

void stepper_init();
void stepper_update();   // call every loop()
void stepper_start();    // begin back-and-forth
void stepper_stop();     // stop after current leg
void stepper_printPos();