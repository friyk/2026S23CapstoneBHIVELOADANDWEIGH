#pragma once
#include <Arduino.h>

// -------------------------------------------------------
// Mode — set to CALIBRATE on first run to find cal factor
//        set to READ for normal operation
// -------------------------------------------------------
#define CALIBRATE 0
#define READ      1
#define MODE      READ   // <-- change to CALIBRATE for first run

void loadcell_init();
void loadcell_update();
void loadcell_tare();
void loadcell_flipSign();
float loadcell_getWeight();

// Calibration functions — active only when MODE == CALIBRATE
// Declared here always so main.cpp can compile regardless of mode
void loadcell_startCalibration(float knownMassGrams);
void loadcell_recordPoint();
void loadcell_confirmCalibration();
void loadcell_discardCalibration();