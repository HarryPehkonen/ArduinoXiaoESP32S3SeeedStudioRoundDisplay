// Touch Controller Enable & I2C Scanner
// Tests common touch enable pins then scans I2C

#include <Wire.h>

// Common touch controller pins for XIAO Round Display
const int TOUCH_RST_PINS[] = {8, 9, 10, -1};  // Try these reset pins
const int TOUCH_INT_PINS[] = {7, 6, 5, -1};   // Common interrupt pins

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\nTouch Controller Detection");
  Serial.println("==========================\n");

  Wire.begin();

  // Try different reset pin configurations
  for (int i = 0; TOUCH_RST_PINS[i] != -1; i++) {
    int rstPin = TOUCH_RST_PINS[i];

    Serial.print("Trying reset pin GPIO");
    Serial.print(rstPin);
    Serial.println("...");

    // Initialize reset pin
    pinMode(rstPin, OUTPUT);
    digitalWrite(rstPin, LOW);
    delay(10);
    digitalWrite(rstPin, HIGH);
    delay(50);

    // Scan I2C bus
    scanI2C();

    Serial.println();
  }

  Serial.println("\n==========================");
  Serial.println("If no touch found, your display might:");
  Serial.println("1. Not have touch capability");
  Serial.println("2. Use SPI touch controller");
  Serial.println("3. Use resistive touch (ADC pins)");
}

void scanI2C() {
  Serial.println("  Scanning I2C bus...");

  for (byte addr = 0x01; addr < 0x7F; addr++) {
    Wire.beginTransmission(addr);
    byte error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("  ✓ Found device at 0x");
      if (addr < 16) Serial.print("0");
      Serial.print(addr, HEX);

      // Identify touch controllers
      if (addr == 0x15) Serial.print(" - CST816S touch");
      else if (addr == 0x2E) Serial.print(" - CHSC6X touch ⭐");
      else if (addr == 0x38) Serial.print(" - FT6336 touch");
      else if (addr == 0x5D || addr == 0x14) Serial.print(" - GT911 touch");
      else if (addr == 0x51) Serial.print(" - BM8563 RTC");

      Serial.println();
    }
  }
}

void loop() {
  // Nothing
}
