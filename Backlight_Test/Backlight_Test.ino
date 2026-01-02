// Backlight Test sketch
// Cycles through candidate GPIO pins to identify backlight control
// Tests each pin HIGH and LOW with user prompts

#include <Arduino.h>

// Candidate GPIO pins for backlight control
// NOTE: XIAO pin labels: D6=GPIO43, D7=GPIO44
const int candidatePins[] = {43, 44, /*20, */ /*21, */ 22, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
const int numPins = sizeof(candidatePins) / sizeof(candidatePins[0]);

int currentPinIndex = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial);  // Wait for Serial Monitor
  delay(2000);      // Settling time

  // Clear any garbage in serial buffer
  while (Serial.available()) {
    Serial.read();
  }

  Serial.println("\n\n=================================");
  Serial.println("   Backlight GPIO Test Tool");
  Serial.println("=================================\n");

  Serial.println("This tool will test each GPIO pin HIGH then LOW.");
  Serial.println("Watch your display backlight and note which");
  Serial.println("pin/state combination controls it.\n");

  Serial.println("Candidate GPIO pins:");
  for (int i = 0; i < numPins; i++) {
    Serial.print("  GPIO");
    Serial.println(candidatePins[i]);
  }
  Serial.println();
}

void loop() {
  // Test each pin
  for (currentPinIndex = 0; currentPinIndex < numPins; currentPinIndex++) {
    int pin = candidatePins[currentPinIndex];

    // Test HIGH
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
    Serial.println("\n=================================");
    Serial.print("Testing GPIO");
    Serial.print(pin);
    Serial.println(" = HIGH");
    Serial.println("=================================");
    Serial.println("Press any key to continue...");
    waitForKeypress();
    delay(500);

    // Test LOW
    digitalWrite(pin, LOW);
    Serial.println("\n=================================");
    Serial.print("Testing GPIO");
    Serial.print(pin);
    Serial.println(" = LOW");
    Serial.println("=================================");
    Serial.println("Press any key to continue...");
    waitForKeypress();
    delay(500);

    // Clean up
    pinMode(pin, INPUT);
  }

  // All pins tested
  Serial.println("\n\n*** All pins tested! Restarting cycle... ***\n");
  delay(2000);
}

void waitForKeypress() {
  // Clear buffer first
  while (Serial.available()) {
    Serial.read();
  }

  // Wait for any key
  while (!Serial.available()) {
    delay(50);
  }

  // Clear buffer after keypress
  while (Serial.available()) {
    Serial.read();
  }
}
