// LSM6DSV16X Test using SparkFun Library
// This version uses the working SparkFun library

#include "SparkFun_LSM6DSV16X.h"
#include <Wire.h>

SparkFun_LSM6DSV16X myLSM;

// Structs for X,Y,Z data
sfe_lsm_data_t accelData;
sfe_lsm_data_t gyroData;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  delay(2000);

  Serial.println("\n================================");
  Serial.println("  LSM6DSV16X SparkFun Test");
  Serial.println("================================\n");

  Wire.begin();

  if (!myLSM.begin()) {
    Serial.println("✗ LSM6DSV16X NOT FOUND!");
    Serial.println("Check I2C connections (SDA/SCL)");
    while (1) delay(1000);
  }

  Serial.println("✓ LSM6DSV16X found\n");

  // Reset the device to default settings
  myLSM.deviceReset();

  // Wait for it to finish resetting
  while (!myLSM.getDeviceReset()) {
    delay(1);
  }

  Serial.println("Device reset complete");
  Serial.println("Configuring sensor...\n");

  // Block data update - prevents reading partially updated data
  myLSM.enableBlockDataUpdate();

  // Set accelerometer: 120 Hz, ±2g
  myLSM.setAccelDataRate(LSM6DSV16X_ODR_AT_120Hz);
  myLSM.setAccelFullScale(LSM6DSV16X_2g);

  // Set gyroscope: 120 Hz, ±250 dps
  myLSM.setGyroDataRate(LSM6DSV16X_ODR_AT_120Hz);
  myLSM.setGyroFullScale(LSM6DSV16X_250dps);

  // Enable filter settling
  myLSM.enableFilterSettling();

  // Enable and configure accelerometer low-pass filter
  myLSM.enableAccelLP2Filter();
  myLSM.setAccelLP2Bandwidth(LSM6DSV16X_XL_STRONG);

  // Enable and configure gyroscope low-pass filter
  myLSM.enableGyroLP1Filter();
  myLSM.setGyroLP1Bandwidth(LSM6DSV16X_GY_ULTRA_LIGHT);

  Serial.println("Sensor configured:");
  Serial.println("  Accel: 120 Hz, ±2g");
  Serial.println("  Gyro:  120 Hz, ±250 dps");
  Serial.println("  Filters enabled\n");

  Serial.println("Streaming data...\n");
}

void loop() {
  // Check if both gyroscope and accelerometer data is available
  if (myLSM.checkStatus()) {
    myLSM.getAccel(&accelData);
    myLSM.getGyro(&gyroData);

    Serial.print("Accel: X=");
    Serial.print(accelData.xData, 2);
    Serial.print(" Y=");
    Serial.print(accelData.yData, 2);
    Serial.print(" Z=");
    Serial.print(accelData.zData, 2);
    Serial.print(" | Gyro: X=");
    Serial.print(gyroData.xData, 2);
    Serial.print(" Y=");
    Serial.print(gyroData.yData, 2);
    Serial.print(" Z=");
    Serial.println(gyroData.zData, 2);
  }

  delay(100);
}
