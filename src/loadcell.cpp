#include "loadcell.h"
#include <HX711_ADC.h>
#include <EEPROM.h>

// -------------------------------------------------------
// Config
// -------------------------------------------------------
const int HX711_DOUT      = 5;
const int HX711_SCK       = 18;
const int CAL_EEPROM_ADDR = 0;
float CAL_FACTOR          = 696.0;  // <-- paste your calibration factor here

const unsigned long PRINT_INTERVAL = 500;

// -------------------------------------------------------
// Globals
// -------------------------------------------------------
static HX711_ADC LoadCell(HX711_DOUT, HX711_SCK);
static unsigned long lastPrintTime = 0;
static boolean newDataReady = false;

// -------------------------------------------------------
// Init
// -------------------------------------------------------
void loadcell_init() {
  EEPROM.begin(512);
  Serial.println("Initializing load cell...");

  LoadCell.begin();
  LoadCell.start(2000, true); // 2s stabilize + auto tare

  if (LoadCell.getTareTimeoutFlag() || LoadCell.getSignalTimeoutFlag()) {
    Serial.println("ERROR: HX711 timeout. Check wiring (DT=GPIO5, SCK=GPIO18).");
    while (1);
  }

  LoadCell.setCalFactor(CAL_FACTOR);
  Serial.println("Load cell ready.");
}

// -------------------------------------------------------
// Update — call every loop()
// -------------------------------------------------------
void loadcell_update() {
  if (LoadCell.update()) newDataReady = true;

  if (newDataReady && (millis() - lastPrintTime > PRINT_INTERVAL)) {
    float weight = LoadCell.getData();
    Serial.print("Weight: ");
    Serial.print(weight, 1);
    Serial.println(" g");
    newDataReady = false;
    lastPrintTime = millis();
  }

  if (LoadCell.getTareStatus()) {
    Serial.println("Tare complete.");
  }
}

// -------------------------------------------------------
// Commands
// -------------------------------------------------------
void loadcell_tare() {
  LoadCell.tareNoDelay();
  Serial.println("Taring...");
}

void loadcell_flipSign() {
  LoadCell.setReverseOutput();
  Serial.println("Weight sign flipped.");
}