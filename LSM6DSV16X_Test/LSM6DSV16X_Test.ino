// LSM6DSV16X IMU Test Sketch
// Simple "hello world" to verify I2C connection to gyroscope/accelerometer

#include <Wire.h>

// Touch controller configuration (to verify I2C bus is working)
const int TOUCH_RST_PIN = 9;
const uint8_t TOUCH_I2C_ADDR = 0x2E;  // CHSC6X

// LSM6DSV16X I2C addresses (try both)
#define LSM6DSV16X_ADDR1 0x6A  // SDO/SA0 pin LOW
#define LSM6DSV16X_ADDR2 0x6B  // SDO/SA0 pin HIGH

// LSM6DSV16X registers
#define LSM6DSV16X_WHO_AM_I   0x0F  // Should return 0x70
#define LSM6DSV16X_CTRL1_XL   0x10  // Accelerometer control
#define LSM6DSV16X_CTRL2_G    0x11  // Gyroscope control
#define LSM6DSV16X_CTRL3_C    0x12  // Control register 3
#define LSM6DSV16X_CTRL4_C    0x13  // Control register 4
#define LSM6DSV16X_STATUS_REG 0x1E  // Status register
#define LSM6DSV16X_OUTX_L_G   0x22  // Gyro X output low byte
#define LSM6DSV16X_OUTX_L_A   0x28  // Accel X output low byte

uint8_t imuAddress = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  delay(2000);

  Serial.println("\n================================");
  Serial.println("  LSM6DSV16X IMU Test");
  Serial.println("================================\n");

  // Initialize touch controller (to verify I2C bus is working)
  Serial.println("Initializing touch controller...");
  pinMode(TOUCH_RST_PIN, OUTPUT);
  digitalWrite(TOUCH_RST_PIN, LOW);
  delay(10);
  digitalWrite(TOUCH_RST_PIN, HIGH);
  delay(50);
  Serial.println("Touch controller reset complete\n");

  // Initialize I2C with default pins (same as main clock code)
  Wire.begin();  // Use default pins
  delay(100);
  Serial.println("I2C initialized with default pins\n");

  // Scan for I2C devices
  Serial.println("Scanning I2C bus...");
  scanI2C();
  Serial.println();

  // Try to find LSM6DSV16X
  if (testAddress(LSM6DSV16X_ADDR1)) {
    imuAddress = LSM6DSV16X_ADDR1;
    Serial.printf("✓ LSM6DSV16X found at 0x%02X\n\n", imuAddress);
  } else if (testAddress(LSM6DSV16X_ADDR2)) {
    imuAddress = LSM6DSV16X_ADDR2;
    Serial.printf("✓ LSM6DSV16X found at 0x%02X\n\n", imuAddress);
  } else {
    Serial.println("✗ LSM6DSV16X NOT FOUND!");
    Serial.println("Check I2C connections (SDA/SCL)");
    while(1) delay(1000);
  }

  // Initialize IMU
  initIMU();

  Serial.println("Streaming accelerometer + gyroscope data...");
  Serial.println("(Press reset to re-scan)\n");
}

void loop() {
  // Check status before reading
  uint8_t status = readRegister(imuAddress, LSM6DSV16X_STATUS_REG);

  // Read accelerometer (in mg)
  int16_t accelX, accelY, accelZ;
  readAccel(&accelX, &accelY, &accelZ);

  // Read gyroscope (in mdps)
  int16_t gyroX, gyroY, gyroZ;
  readGyro(&gyroX, &gyroY, &gyroZ);

  // Print data with status
  Serial.printf("Status=0x%02X | Accel: X=%6d Y=%6d Z=%6d | ", status, accelX, accelY, accelZ);
  Serial.printf("Gyro: X=%6d Y=%6d Z=%6d\n", gyroX, gyroY, gyroZ);

  delay(500);  // 2Hz update rate
}

void scanI2C() {
  uint8_t count = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  Device found at 0x%02X\n", addr);
      count++;
    }
  }
  if (count == 0) {
    Serial.println("  No I2C devices found!");
  } else {
    Serial.printf("  Found %d device(s)\n", count);
  }
}

bool testAddress(uint8_t addr) {
  uint8_t whoAmI = readRegister(addr, LSM6DSV16X_WHO_AM_I);
  Serial.printf("Address 0x%02X: WHO_AM_I = 0x%02X ", addr, whoAmI);

  if (whoAmI == 0x70) {
    Serial.println("(LSM6DSV16X ✓)");
    return true;
  } else {
    Serial.println("(wrong ID)");
    return false;
  }
}

void initIMU() {
  Serial.println("Initializing IMU...");

  // Software reset first
  Serial.println("Performing software reset...");
  writeRegister(imuAddress, LSM6DSV16X_CTRL3_C, 0x01);  // SOFT_RESET
  delay(100);

  // Read current state
  uint8_t ctrl1 = readRegister(imuAddress, LSM6DSV16X_CTRL1_XL);
  uint8_t ctrl2 = readRegister(imuAddress, LSM6DSV16X_CTRL2_G);
  uint8_t ctrl3 = readRegister(imuAddress, LSM6DSV16X_CTRL3_C);
  Serial.printf("After reset: CTRL1_XL=0x%02X, CTRL2_G=0x%02X, CTRL3_C=0x%02X\n",
                ctrl1, ctrl2, ctrl3);

  // Configure CTRL3_C: BDU + IF_INC (auto-increment addresses)
  writeRegister(imuAddress, LSM6DSV16X_CTRL3_C, 0x44);  // BDU=1, IF_INC=1
  delay(10);

  // Try enabling CTRL9_C - device configuration register
  writeRegister(imuAddress, 0x18, 0x02);  // CTRL9_C: DEVICE_CONF=1
  delay(10);

  // Try 120 Hz (ST library default): ODR=0111 (7) = 0x70
  // Enable accelerometer: 120 Hz, ±2g
  writeRegister(imuAddress, LSM6DSV16X_CTRL1_XL, 0x70);
  delay(10);

  // Enable gyroscope: 120 Hz, ±250 dps
  writeRegister(imuAddress, LSM6DSV16X_CTRL2_G, 0x70);
  delay(10);

  // Try CTRL5_C and CTRL6_C for additional enables
  writeRegister(imuAddress, 0x14, 0x00);  // CTRL5_C: defaults
  writeRegister(imuAddress, 0x15, 0x00);  // CTRL6_C: defaults
  delay(10);

  // Read back to verify
  ctrl1 = readRegister(imuAddress, LSM6DSV16X_CTRL1_XL);
  ctrl2 = readRegister(imuAddress, LSM6DSV16X_CTRL2_G);
  ctrl3 = readRegister(imuAddress, LSM6DSV16X_CTRL3_C);
  Serial.printf("After init:  CTRL1_XL=0x%02X, CTRL2_G=0x%02X, CTRL3_C=0x%02X\n",
                ctrl1, ctrl2, ctrl3);

  delay(200);  // Let IMU stabilize and produce first samples

  // Check status
  uint8_t status = readRegister(imuAddress, LSM6DSV16X_STATUS_REG);
  Serial.printf("Status register: 0x%02X (XLDA=%d, GDA=%d)\n",
                status, (status >> 0) & 1, (status >> 1) & 1);

  // Dump first few output registers to see if there's any data
  Serial.print("Raw output registers: ");
  for (int i = 0x22; i <= 0x2D; i++) {
    Serial.printf("%02X ", readRegister(imuAddress, i));
  }
  Serial.println();

  Serial.println("IMU initialized (120 Hz, ±2g, ±250dps)\n");
}

void readAccel(int16_t *x, int16_t *y, int16_t *z) {
  Wire.beginTransmission(imuAddress);
  Wire.write(LSM6DSV16X_OUTX_L_A);
  uint8_t error = Wire.endTransmission(false);

  uint8_t bytesReceived = Wire.requestFrom(imuAddress, (uint8_t)6);

  // Debug: check if we got the expected bytes
  static bool firstRead = true;
  if (firstRead) {
    Serial.printf("readAccel: I2C error=%d, bytesReceived=%d\n", error, bytesReceived);
    firstRead = false;
  }

  uint8_t xlo = Wire.read();
  uint8_t xhi = Wire.read();
  uint8_t ylo = Wire.read();
  uint8_t yhi = Wire.read();
  uint8_t zlo = Wire.read();
  uint8_t zhi = Wire.read();

  *x = (int16_t)(xhi << 8 | xlo);
  *y = (int16_t)(yhi << 8 | ylo);
  *z = (int16_t)(zhi << 8 | zlo);
}

void readGyro(int16_t *x, int16_t *y, int16_t *z) {
  Wire.beginTransmission(imuAddress);
  Wire.write(LSM6DSV16X_OUTX_L_G);
  Wire.endTransmission(false);
  Wire.requestFrom(imuAddress, (uint8_t)6);

  uint8_t xlo = Wire.read();
  uint8_t xhi = Wire.read();
  uint8_t ylo = Wire.read();
  uint8_t yhi = Wire.read();
  uint8_t zlo = Wire.read();
  uint8_t zhi = Wire.read();

  *x = (int16_t)(xhi << 8 | xlo);
  *y = (int16_t)(yhi << 8 | ylo);
  *z = (int16_t)(zhi << 8 | zlo);
}

uint8_t readRegister(uint8_t addr, uint8_t reg) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(addr, (uint8_t)1);
  return Wire.read();
}

void writeRegister(uint8_t addr, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}
