#include <Arduino.h>
#include "loadcell.h"
#include "stepper.h"

void setup() {
  delay(1000);
  Serial.begin(115200);
  Serial.println("\n=== ESP32 Load Cell + Stepper ===");

  loadcell_init();
  stepper_init();

  Serial.println("\nLoad cell commands: 't' = tare, 'r' = flip sign");
  Serial.println("Stepper commands:   'go' = start, 'stop' = stop, 'p' = position\n");
}

void loop() {
  loadcell_update();
  stepper_update();

  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    // Load cell commands
    if      (input == "t")    loadcell_tare();
    else if (input == "r")    loadcell_flipSign();

    // Stepper commands
    else if (input == "go")   stepper_start();
    else if (input == "stop") stepper_stop();
    else if (input == "p")    stepper_printPos();

    else Serial.println("Unknown command.");
  }
}