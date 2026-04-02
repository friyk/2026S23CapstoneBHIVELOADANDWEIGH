#include "loadcell.h"
#include <HX711_ADC.h>
#include <EEPROM.h>

// -------------------------------------------------------
// Pin config
// -------------------------------------------------------
const int HX711_DOUT = 5;   // D5
const int HX711_SCK  = 18;  // D18

// -------------------------------------------------------
// EEPROM
// -------------------------------------------------------
const int CAL_EEPROM_ADDR = 0; // float = 4 bytes, addresses 0-3

// -------------------------------------------------------
// Config
// -------------------------------------------------------
const unsigned long PRINT_INTERVAL = 2000; // ms — print weight every 2 seconds

// -------------------------------------------------------
// Globals
// -------------------------------------------------------
static HX711_ADC LoadCell(HX711_DOUT, HX711_SCK);
static float latestWeight   = 0.0;
static boolean newDataReady = false;
static unsigned long lastPrintTime = 0;

// -------------------------------------------------------
// Calibration state
// -------------------------------------------------------
#if MODE == CALIBRATE

// How long to wait after 'r' before sampling (ms).
// Lets the initial load cell creep settle before readings are taken.
static const unsigned long CAL_SETTLE_MS  = 8000;
// Number of samples to average for the final reading.
static const int           CAL_NUM_SAMPLES = 30;

static float   knownMassGrams  = 0.0;
static boolean awaitingMass    = true;
static boolean awaitingRecord  = false;
static boolean awaitingConfirm = false;
static float   finalCalFactor  = 0.0;

#endif

// -------------------------------------------------------
// Init
// -------------------------------------------------------
void loadcell_init() {
  Serial.println("Initializing load cell...");

  LoadCell.begin();
  LoadCell.start(3000, true); // 3s stabilize + auto tare

  if (LoadCell.getTareTimeoutFlag() || LoadCell.getSignalTimeoutFlag()) {
    Serial.println("ERROR: HX711 timeout. Check wiring (DT=GPIO5, SCK=GPIO18).");
    while (1);
  }

#if MODE == CALIBRATE
  LoadCell.setCalFactor(1.0);
  Serial.println("Load cell ready — CALIBRATION MODE.");
  Serial.println("==================================================");
  Serial.println("Step 1: Ensure platform is EMPTY, send 't' to tare.");
  Serial.println("        Then enter your known weight in GRAMS");
  Serial.println("        (e.g. 20000 for 20kg) and press Enter.");
  Serial.println("==================================================\n");

#else
  float savedCal;
  EEPROM.get(CAL_EEPROM_ADDR, savedCal);

  if (isnan(savedCal) || savedCal == 0.0) {
    Serial.println("WARNING: No calibration factor found in EEPROM.");
    Serial.println("Switch MODE to CALIBRATE in loadcell.h and re-flash.");
    LoadCell.setCalFactor(1.0);
  } else {
    LoadCell.setCalFactor(savedCal);
    Serial.print("Load cell ready. Cal factor: ");
    Serial.println(savedCal, 4);
  }
#endif
}

// -------------------------------------------------------
// Update — call every loop(), non-blocking
// -------------------------------------------------------
void loadcell_update() {
  if (LoadCell.update()) newDataReady = true;

  if (newDataReady && (millis() - lastPrintTime >= PRINT_INTERVAL)) {
    latestWeight = LoadCell.getData();
    float weightKg = latestWeight / 1000.0;
    Serial.print("Weight: ");
    Serial.print(weightKg, 3);
    Serial.println(" kg");
    newDataReady  = false;
    lastPrintTime = millis();
  }

  if (LoadCell.getTareStatus()) {
    Serial.println("Tare complete. Now enter known weight in grams (e.g. 20000).");
  }
}

// -------------------------------------------------------
// Returns latest weight in kg
// -------------------------------------------------------
float loadcell_getWeight() {
  return latestWeight / 1000.0;
}

// -------------------------------------------------------
// Tare
// -------------------------------------------------------
void loadcell_tare() {
  LoadCell.tareNoDelay();
  Serial.println("Taring...");
}

// -------------------------------------------------------
// Flip sign
// -------------------------------------------------------
void loadcell_flipSign() {
  LoadCell.setReverseOutput();
  Serial.println("Weight sign flipped.");
}

#if MODE == CALIBRATE

void loadcell_startCalibration(float massGrams) {
  if (massGrams <= 0) {
    Serial.println("Invalid. Enter a positive mass in grams (e.g. 20000 for 20kg).");
    return;
  }

  knownMassGrams = massGrams;
  awaitingMass   = false;
  awaitingRecord = true;

  Serial.print("Known mass: ");
  Serial.print(knownMassGrams / 1000.0, 2);
  Serial.println(" kg");
  Serial.println("--------------------------------------------------");
  Serial.println("Place weight anywhere on the platform.");
  Serial.println("Send 'r'. Waits 8s for creep to settle, then");
  Serial.println("takes 30 samples automatically.");
  Serial.println("--------------------------------------------------");
}

void loadcell_recordPoint() {
  if (!awaitingRecord || awaitingConfirm) return;

  Serial.println("Waiting 8s for load cell to settle...");
  delay(CAL_SETTLE_MS);

  Serial.print("Collecting ");
  Serial.print(CAL_NUM_SAMPLES);
  Serial.println(" samples...");

  float sum = 0.0;
  int   count = 0;
  unsigned long deadline = millis() + 10000;

  while (count < CAL_NUM_SAMPLES) {
    if (millis() > deadline) {
      Serial.println("ERROR: Timeout. Check load cell wiring and try again.");
      return;
    }
    if (LoadCell.update()) {
      sum += LoadCell.getData();
      count++;
      delay(100);
    }
  }

  float mean = sum / CAL_NUM_SAMPLES;
  finalCalFactor = mean / knownMassGrams;

  Serial.println("Done.");
  Serial.print("  Raw mean    : "); Serial.println(mean, 1);
  Serial.print("  Known mass  : "); Serial.print(knownMassGrams / 1000.0, 3); Serial.println(" kg");
  Serial.print("  Cal factor  : "); Serial.println(finalCalFactor, 4);
  Serial.println("--------------------------------------------------");
  Serial.println("Send 'y' to save to EEPROM, any other key to discard.");

  awaitingRecord  = false;
  awaitingConfirm = true;
}

void loadcell_confirmCalibration() {
  if (!awaitingConfirm) return;
  EEPROM.put(CAL_EEPROM_ADDR, finalCalFactor);
  EEPROM.commit();
  LoadCell.setCalFactor(finalCalFactor);
  awaitingConfirm = false;
  Serial.println("Calibration factor saved to EEPROM.");
  Serial.println(">>> Change MODE to READ in loadcell.h and re-flash.");
}

void loadcell_discardCalibration() {
  awaitingConfirm = false;
  Serial.println("Calibration discarded. Send mass in grams to try again.");
  awaitingRecord = true;
}

#else

// Stubs for READ mode — must exist for the linker
void loadcell_startCalibration(float knownMassGrams) {}
void loadcell_recordPoint()                          {}
void loadcell_confirmCalibration()                   {}
void loadcell_discardCalibration()                   {}

#endif
