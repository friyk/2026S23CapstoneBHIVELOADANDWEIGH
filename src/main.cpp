#include <Arduino.h>
#include <EEPROM.h>
#include "loadcell.h"
#include "stepper.h"

void setup() {
  delay(1000);
  Serial.begin(115200);
  EEPROM.begin(512);

  Serial.println("\n=== ESP32 Load Cell + Dual Stepper ===");

  loadcell_init();
  stepper_init();

#if MODE == READ
  Serial.println("\nLoad cell : 't' = tare, 'r' = flip sign");
  Serial.println("Stepper   : 'unload' = push + auto retract after 2s");
  Serial.println("            'push', 'retract', 'stop', 'p', 'sethome'");
  Serial.println("            'm1<steps>' = move motor 1, 'm2<steps>' = move motor 2\n");
#endif
}

void loop() {
  loadcell_update();
  stepper_update();

  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();

#if MODE == CALIBRATE
    // Calibration mode command handling
    if (input == "t") {
      loadcell_tare();
    }
    else if (input == "y") {
      loadcell_confirmCalibration();
    }
    else if (input == "r") {
      loadcell_recordPoint();
    }
    else {
      // Check if input is a valid positive number
      boolean isNumeric = true;
      for (int i = 0; i < input.length(); i++) {
        if (!isDigit(input[i]) && input[i] != '.' && input[i] != '-') {
          isNumeric = false;
          break;
        }
      }
      if (isNumeric && input.length() > 0) {
        float mass = input.toFloat();
        if (mass > 0) {
          loadcell_startCalibration(mass);
        } else {
          Serial.println("Enter a positive number in grams (e.g. 20000 for 20kg).");
        }
      } else {
        Serial.println("Unknown command. In calibration mode: 't'=tare, 'r'=record point, 'y'=save, or enter mass in grams.");
      }
    }

#else
    // Read mode — full command set
    if      (input == "t")           loadcell_tare();
    else if (input == "r")           loadcell_flipSign();
    else if (input == "unload")      stepper_unload();
    else if (input == "push")        stepper_push();
    else if (input == "retract")     stepper_retract();
    else if (input == "stop")        stepper_stop();
    else if (input == "p")           stepper_printPos();
    else if (input == "sethome")     stepper_setHome();
    else if (input.startsWith("m1")) {
      long steps = input.substring(2).toInt();
      if (steps != 0) stepper_moveMotor1(steps);
      else Serial.println("Invalid. Use e.g. m1800 or m1-800");
    }
    else if (input.startsWith("m2")) {
      long steps = input.substring(2).toInt();
      if (steps != 0) stepper_moveMotor2(steps);
      else Serial.println("Invalid. Use e.g. m2800 or m2-800");
    }
    else Serial.println("Unknown command.");
#endif
  }
}