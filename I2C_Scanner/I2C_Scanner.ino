// I2C Scanner sketch
// Scans I2C bus and reports all device addresses found

#include <Wire.h>

void setup() {
  Serial.begin(115200);
  while (!Serial);  // Wait for Serial Monitor

  Serial.println("\n\nI2C Scanner");
  Serial.println("===========\n");

  Wire.begin();

  byte count = 0;

  Serial.println("Scanning I2C bus (0x00 - 0x7F)...\n");

  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      Serial.print(" (");
      Serial.print(address);
      Serial.print(")");

      // Identify known devices
      if (address == 0x15) Serial.print(" - Likely CST816S touch controller");
      if (address == 0x2E) Serial.print(" - Likely CHSC6X touch controller");
      if (address == 0x38) Serial.print(" - Likely FT6336 touch controller");
      if (address == 0x5D || address == 0x14) Serial.print(" - Likely GT911 touch controller");
      if (address == 0x51) Serial.print(" - Likely BM8563 RTC");

      Serial.println();
      count++;
    }
    else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
    }
  }

  Serial.println("\n===========");
  if (count == 0)
    Serial.println("No I2C devices found");
  else
    Serial.print(count);
    Serial.println(" device(s) found");
  Serial.println("\nScan complete!");
}

void loop() {
  // Nothing to do here
}
