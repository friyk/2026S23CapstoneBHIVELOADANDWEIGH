#include <Arduino.h>
#include <EEPROM.h>
#include "loadcell.h"
#include "stepper.h"

void setup() {
  delay(1000);
  Serial.begin(115200);

  // Initialize EEPROM once for all modules
  EEPROM.begin(512);

  Serial.println("\n=== ESP32 Load Cell + Dual Stepper ===");

  loadcell_init();
  stepper_init();

  Serial.println("\nLoad cell : 't' = tare, 'r' = flip sign");
  Serial.println("Stepper   : 'unload' = push + auto retract after 2s");
  Serial.println("            'push', 'retract', 'stop', 'p', 'sethome'");
  Serial.println("            'm1<steps>' = move motor 1, 'm2<steps>' = move motor 2\n");
}

void loop() {
  loadcell_update();
  stepper_update();

  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    // Load cell
    if      (input == "t")           loadcell_tare();
    else if (input == "r")           loadcell_flipSign();

    // Stepper
    else if (input == "unload")      stepper_unload();
    else if (input == "push")        stepper_push();
    else if (input == "retract")     stepper_retract();
    else if (input == "stop")        stepper_stop();
    else if (input == "p")           stepper_printPos();
    else if (input == "sethome")     stepper_setHome();

    // Independent
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
  }
}