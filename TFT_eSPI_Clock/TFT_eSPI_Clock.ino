#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>

#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
#include "esp_wifi.h"
#include "WiFi.h"
// const char *ntpServer = "time.cloudflare.com";
const char *ntpServer = "time.google.com";
const char *ssid     = "We Don't Have WiFi";
const char *password = "abracadabra";
#endif

#include "I2C_BM8563.h"
#include "NotoSansBold15.h"

I2C_BM8563 rtc(I2C_BM8563_DEFAULT_ADDRESS, Wire);
I2C_BM8563_TimeTypeDef timeStruct;
I2C_BM8563_DateTypeDef dateStruct;

TFT_eSPI tft = TFT_eSPI();  // Invoke library, pins defined in User_Setup.h
TFT_eSprite face = TFT_eSprite(&tft);

const int BAT_PIN = D0;
constexpr float BATTERY_VOLTAGE_DIVIDER = 2.0f;  // Hardware voltage divider ratio
float battery_voltage = 0;
bool is_charging = false;

// Display backlight control
const int TFT_BL_PIN = 43;  // Backlight pin (GPIO43 = D6 on XIAO Round Display)

// Touch controller configuration
const int TOUCH_RST_PIN = 9;
const uint8_t TOUCH_I2C_ADDR = 0x2E;  // CHSC6X
struct TouchPoint {
  bool touched;
  int16_t x;
  int16_t y;
};
TouchPoint touch = {false, 0, 0};

// Touch zones
enum TouchZone { ZONE_LEFT, ZONE_CENTER, ZONE_RIGHT, ZONE_NONE };

// UI State machine
enum UIState { STATE_NORMAL, STATE_MENU, STATE_SLEEP };
UIState uiState = STATE_NORMAL;

// Menu system
enum MenuOption { MENU_OFF, MENU_CLOCK };
MenuOption currentOption = MENU_CLOCK;
const char* menuOptions[] = {"Off", "Clock"};
const int menuOptionCount = 2;

#define CLOCK_X_POS 10
#define CLOCK_Y_POS 10

#define CLOCK_FG   TFT_SKYBLUE
#define CLOCK_BG   TFT_NAVY
#define SECCOND_FG TFT_RED
#define LABEL_FG   TFT_GOLD

#define CLOCK_R       230.0f / 2.0f // Clock face radius (float type)
#define H_HAND_LENGTH CLOCK_R/2.0f
#define M_HAND_LENGTH CLOCK_R/1.4f
#define S_HAND_LENGTH CLOCK_R/1.3f

#define FACE_W CLOCK_R * 2 + 1
#define FACE_H CLOCK_R * 2 + 1

// Calculate 1 second increment angles. Hours and minute hand angles
// change every second so we see smooth sub-pixel movement
#define SECOND_ANGLE 360.0 / 60.0
#define MINUTE_ANGLE SECOND_ANGLE / 60.0
#define HOUR_ANGLE   MINUTE_ANGLE / 12.0

// Sprite width and height
#define FACE_W CLOCK_R * 2 + 1
#define FACE_H CLOCK_R * 2 + 1

// Time h:m:s
uint8_t h = 0, m = 0, s = 0;

float time_secs = h * 3600 + m * 60 + s;

// Time for next tick
uint32_t targetTime = 0;
uint32_t syncingTime = 0;

// =========================================================================
// Setup
// =========================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("Booting...");

  pinMode(BAT_PIN, ANALOG);

  // Initialize backlight control
  pinMode(TFT_BL_PIN, OUTPUT);
  digitalWrite(TFT_BL_PIN, HIGH);

  // ----------------------------------------------------------------
  // 1. TOUCH CONTROLLER HARDWARE RESET (CRITICAL - BEFORE DISPLAY)
  // ----------------------------------------------------------------
  // GPIO9 is shared between touch reset and display SPI MOSI.
  // Reset touch controller NOW before display claims the pin.
  pinMode(TOUCH_RST_PIN, OUTPUT);
  digitalWrite(TOUCH_RST_PIN, LOW);
  delay(10);
  digitalWrite(TOUCH_RST_PIN, HIGH);
  delay(50);
  Serial.println("Touch controller hardware reset complete");

  // ----------------------------------------------------------------
  // 2. DISPLAY INITIALIZATION
  // ----------------------------------------------------------------
  // Now safe to init display - it will reconfigure GPIO9 as SPI MOSI
  tft.init();

  // Ideally set orientation for good viewing angle range because
  // the anti-aliasing effectiveness varies with screen viewing angle
  // Usually this is when screen ribbon connector is at the bottom
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  // Create the clock face sprite
  //face.setColorDepth(8); // 8 bit will work, but reduces effectiveness of anti-aliasing
  face.createSprite(FACE_W, FACE_H);

  // Only 1 font used in the sprite, so can remain loaded
  face.loadFont(NotoSansBold15);

  // Draw initial clock - NTP time not available yet
  renderFace(time_secs);

#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while ( WiFi.status() != WL_CONNECTED )
  {
    delay ( 500 );
    Serial.print ( "." );
  }
  Serial.println("\nWiFi connected!");

  // Configure NTP and timezone
  configTime(0, 0, ntpServer); // get UTC
  setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
  tzset();

  // Wait for NTP sync (up to 10 seconds)
  Serial.print("Waiting for NTP time sync");
  struct tm timeInfo;
  bool gotTime = false;
  for (int i = 0; i < 20; i++) {
    if (getLocalTime(&timeInfo)) {
      gotTime = true;
      break;
    }
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (gotTime) {
    Serial.printf("NTP time synced: %04d-%02d-%02d %02d:%02d:%02d\n",
                  timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday,
                  timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
    timeStruct.hours   = timeInfo.tm_hour;
    timeStruct.minutes = timeInfo.tm_min;
    timeStruct.seconds = timeInfo.tm_sec;
    rtc.setTime(&timeStruct);
    dateStruct.weekDay = timeInfo.tm_wday;
    dateStruct.month   = timeInfo.tm_mon + 1;
    dateStruct.date    = timeInfo.tm_mday;
    dateStruct.year    = timeInfo.tm_year + 1900;
    rtc.setDate(&dateStruct);
  } else {
    Serial.println("ERROR: NTP sync failed! Using RTC time (may be inaccurate)");
  }
#endif

  Wire.begin();
  rtc.begin();
  syncTime();

  // Recreate sprite to clear any corruption from WiFi operations
  Serial.println("Recreating sprite...");
  face.deleteSprite();
  face.createSprite(FACE_W, FACE_H);
  face.loadFont(NotoSansBold15);

  // Initialize touch controller
  initTouch();

  // Initial battery reading
  battery_voltage = readBatteryVoltage();
  Serial.printf("Initial battery: %.2fV\n", battery_voltage);

  // Force a display update with correct time
  Serial.println("Forcing display update...");
  renderFace(time_secs);
  Serial.printf("Setup complete - time is %02d:%02d:%02d\n",
                (int)(time_secs/3600)%24, (int)(time_secs/60)%60, (int)time_secs%60);
}

// =========================================================================
// Loop
// =========================================================================
void loop() {
  // Touch handling with majority-vote filter for very bouncy touch controllers
  static uint8_t touchHistory = 0;  // Last 8 readings as bits
  static bool filteredTouch = false;
  static TouchZone touchZone = ZONE_NONE;
  static uint32_t lastActionTime = 0;
  static uint32_t touchPressTime = 0;
  static bool longPressTriggered = false;
  static bool touchActive = false;

  constexpr uint32_t DEBOUNCE_MS = 500;       // Minimum time between actions
  constexpr uint32_t LONG_PRESS_MS = 1000;    // Long press threshold
  constexpr uint8_t TOUCH_ON_THRESHOLD = 6;   // Need 6/8 to START touch
  constexpr uint8_t TOUCH_OFF_THRESHOLD = 0;  // Need 0/8 to END touch (max hysteresis!)

  TouchPoint tp;
  bool rawTouch = readTouch(&tp);
  uint32_t now = millis();

  // Shift history and add new reading (majority vote filter)
  touchHistory = (touchHistory << 1) | (rawTouch ? 1 : 0);

  // Count bits set in last 8 readings
  uint8_t touchCount = 0;
  for (int i = 0; i < 8; i++) {
    if (touchHistory & (1 << i)) touchCount++;
  }

  // Update filtered touch state with hysteresis (sticky touch)
  bool newFilteredTouch = filteredTouch;  // Default to current state
  if (!filteredTouch && touchCount >= TOUCH_ON_THRESHOLD) {
    // Not touching -> touching: need strong signal
    newFilteredTouch = true;
  } else if (filteredTouch && touchCount <= TOUCH_OFF_THRESHOLD) {
    // Touching -> not touching: need clear release
    newFilteredTouch = false;
  }
  // Otherwise maintain current state (hysteresis zone)

  // Detect touch press (rising edge)
  if (newFilteredTouch && !filteredTouch) {
    if (now - lastActionTime >= DEBOUNCE_MS) {
      touchZone = getTouchZone(tp.x);
      touchPressTime = now;
      longPressTriggered = false;
      touchActive = true;
      Serial.printf("Touch DOWN: zone=%d (count=%d/8)\n", touchZone, touchCount);
    }
  }

  // Detect touch release (falling edge)
  if (!newFilteredTouch && filteredTouch) {
    if (touchActive && touchZone != ZONE_NONE) {
      if (!longPressTriggered) {
        uint32_t duration = now - touchPressTime;
        Serial.printf("Touch UP after %dms - TAP\n", duration);
        handleTouch(touchZone);
        lastActionTime = now;
      } else {
        Serial.println("Touch UP (was long press)");
      }
    }
    touchZone = ZONE_NONE;
    touchActive = false;
    longPressTriggered = false;
  }

  filteredTouch = newFilteredTouch;

  // Detect long press
  if (filteredTouch && touchActive && !longPressTriggered && touchZone != ZONE_NONE) {
    if (now - touchPressTime >= LONG_PRESS_MS) {
      Serial.printf("LONG PRESS after %dms\n", now - touchPressTime);
      handleLongPress(touchZone);
      lastActionTime = now;
      longPressTriggered = true;
      touchActive = false;
    }
  }

  // Update time and render
  if (targetTime < millis()) {
    targetTime = millis() + 100;

    // Increment time by 100 milliseconds
    time_secs += 0.100;

    // Midnight roll-over
    if (time_secs >= (60 * 60 * 24)) time_secs = 0;

    // Render based on state
    if (uiState == STATE_NORMAL) {
      renderFace(time_secs);
    } else if (uiState == STATE_MENU) {
      renderMenu();
    } else if (uiState == STATE_SLEEP) {
      // Don't render in sleep
    }
  }

  // Periodic RTC sync
  if (syncingTime < millis()) {
    syncingTime = millis() + 1000 * 60 * 5;
    syncTime();
  }

  // Update battery voltage and charging state
  updateBatteryState();
}

// =========================================================================
// Draw the clock face in the sprite
// =========================================================================
static void renderFace(float t) {
  float h_angle = t * HOUR_ANGLE;
  float m_angle = t * MINUTE_ANGLE;
  float s_angle = t * SECOND_ANGLE;

  // The face is completely redrawn - this can be done quickly
  face.fillSprite(TFT_BLACK);

  // Draw the face circle
  face.fillSmoothCircle( CLOCK_R, CLOCK_R, CLOCK_R, CLOCK_BG );

  // Set text datum to middle centre and the colour
  face.setTextDatum(MC_DATUM);

  // The background colour will be read during the character rendering
  face.setTextColor(CLOCK_FG, CLOCK_BG);

  // Text offset adjustment
  constexpr uint32_t dialOffset = CLOCK_R - 10;

  float xp = 0.0, yp = 0.0; // Use float pixel position for smooth AA motion

  // Draw digits around clock perimeter
  for (uint32_t h = 1; h <= 12; h++) {
    getCoord(CLOCK_R, CLOCK_R, &xp, &yp, dialOffset, h * 360.0 / 12);
    face.drawNumber(h, xp, 2 + yp);
  }

  // Draw digital time display
  int hours = static_cast<int>(t / 3600) % 24;
  int minutes = static_cast<int>(t / 60) % 60;
  int seconds = static_cast<int>(t) % 60;

  face.setTextColor(LABEL_FG, CLOCK_BG);
  char timeStr[9];  // "HH:MM:SS\0"
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", hours, minutes, seconds);
  face.drawString(timeStr, CLOCK_R, CLOCK_R * 0.75);

  // Draw minute hand
  getCoord(CLOCK_R, CLOCK_R, &xp, &yp, M_HAND_LENGTH, m_angle);
  face.drawWideLine(CLOCK_R, CLOCK_R, xp, yp, 6.0f, CLOCK_FG);
  face.drawWideLine(CLOCK_R, CLOCK_R, xp, yp, 2.0f, CLOCK_BG);

  // Draw hour hand
  getCoord(CLOCK_R, CLOCK_R, &xp, &yp, H_HAND_LENGTH, h_angle);
  face.drawWideLine(CLOCK_R, CLOCK_R, xp, yp, 6.0f, CLOCK_FG);
  face.drawWideLine(CLOCK_R, CLOCK_R, xp, yp, 2.0f, CLOCK_BG);

  // Draw the central pivot circle
  face.fillSmoothCircle(CLOCK_R, CLOCK_R, 4, CLOCK_FG);

  // Battery display with color coding
  float battery_percent = batteryPercent(battery_voltage, is_charging);

  // Color based on charging state and level
  uint16_t batteryColor;
  if (is_charging) {
    batteryColor = TFT_GREEN;  // Green when charging
  } else if (battery_percent < 15.0f) {
    batteryColor = TFT_RED;    // Red when low
  } else {
    batteryColor = LABEL_FG;   // Gold when normal
  }

  face.setTextColor(batteryColor, CLOCK_BG);
  char percentageStr[10];  // "100.0%\0"
  char voltageStr[10];     // "4.20V\0"
  snprintf(percentageStr, sizeof(percentageStr), "%.1f%%", battery_percent);
  snprintf(voltageStr, sizeof(voltageStr), "%.2fV", battery_voltage);
  face.drawString(percentageStr, CLOCK_R, CLOCK_R * 0.62);
  face.drawString(voltageStr, CLOCK_R, CLOCK_R * 0.50);

  // Draw second hand
  getCoord(CLOCK_R, CLOCK_R, &xp, &yp, S_HAND_LENGTH, s_angle);
  face.drawWedgeLine(CLOCK_R, CLOCK_R, xp, yp, 2.5, 1.0, SECCOND_FG);

  face.pushSprite(5, 5, TFT_TRANSPARENT);
}

// =========================================================================
// Get coordinates of end of a line, pivot at x,y, length r, angle a
// =========================================================================
// Coordinates are returned to caller via the xp and yp pointers
#define DEG2RAD 0.0174532925
void getCoord(int16_t x, int16_t y, float *xp, float *yp, int16_t r, float a)
{
  float sx1 = cos( (a - 90) * DEG2RAD);
  float sy1 = sin( (a - 90) * DEG2RAD);
  *xp =  sx1 * r + x;
  *yp =  sy1 * r + y;
}

// =========================================================================
// Draw menu interface
// =========================================================================
void renderMenu() {
  face.fillSprite(TFT_BLACK);

  // Draw background circle
  face.fillSmoothCircle(CLOCK_R, CLOCK_R, CLOCK_R, CLOCK_BG);

  // Set text color
  face.setTextColor(LABEL_FG, CLOCK_BG);
  face.setTextDatum(MC_DATUM);

  // Draw left arrow
  face.drawString("<", CLOCK_R * 0.3, CLOCK_R);

  // Draw current option in center
  face.setTextColor(TFT_WHITE, CLOCK_BG);
  face.drawString(menuOptions[currentOption], CLOCK_R, CLOCK_R);

  // Draw right arrow
  face.setTextColor(LABEL_FG, CLOCK_BG);
  face.drawString(">", CLOCK_R * 1.7, CLOCK_R);

  // Push to screen
  face.pushSprite(5, 5, TFT_TRANSPARENT);
}

void syncTime(void){
  targetTime = millis() + 100;
  rtc.getTime(&timeStruct);
  time_secs = timeStruct.hours * 3600 + timeStruct.minutes * 60 + timeStruct.seconds;
  Serial.printf("RTC sync: %02d:%02d:%02d (%.1f secs)\n",
                timeStruct.hours, timeStruct.minutes, timeStruct.seconds, time_secs);
}

// =========================================================================
// Battery monitoring functions
// =========================================================================
float readBatteryVoltage() {
  uint32_t v_raw = analogReadMilliVolts(BAT_PIN);
  float voltage = (v_raw * BATTERY_VOLTAGE_DIVIDER) / 1000.0f;
  return voltage;
}

void updateBatteryState() {
  static float lastVoltage = 0.0f;
  static uint32_t lastCheck = 0;

  uint32_t now = millis();
  if (now - lastCheck > 5000) {  // Check every 5 seconds
    float currentVoltage = readBatteryVoltage();

    // Trend detection: rising = charging, falling = discharging
    // Use 20mV threshold to avoid noise triggering false detections
    if (currentVoltage > lastVoltage + 0.02f) {
      is_charging = true;
    } else if (currentVoltage < lastVoltage - 0.02f) {
      is_charging = false;
    }
    // If within 20mV, keep previous state (no change)

    battery_voltage = currentVoltage;
    lastVoltage = currentVoltage;
    lastCheck = now;

    Serial.printf("Battery: %.2fV, %s\n", battery_voltage, is_charging ? "Charging" : "Discharging");
  }
}

float batteryPercent(float voltage, bool charging) {
    constexpr float V_MAX = 4.2f;
    constexpr float V_MIN = 2.65f;  // Based on observed minimum voltage

    // Clamp voltage to valid range
    if (voltage >= V_MAX) return 100.0f;
    if (voltage <= V_MIN) return 0.0f;

    // Normalize to 0-1 range
    float v = (voltage - V_MIN) / (V_MAX - V_MIN);

    // Cubic polynomial coefficients - different for charging vs discharging
    float percent;
    if (charging) {
        // Charging curve (voltage higher for same capacity)
        constexpr float a = 15.0f;
        constexpr float b = -40.0f;
        constexpr float c = 125.0f;
        constexpr float d = 0.0f;
        percent = a*v*v*v + b*v*v + c*v + d;
    } else {
        // Discharging curve (voltage lower for same capacity)
        constexpr float a = 10.0f;
        constexpr float b = -30.0f;
        constexpr float c = 120.0f;
        constexpr float d = 0.0f;
        percent = a*v*v*v + b*v*v + c*v + d;
    }

    return constrain(percent, 0.0f, 100.0f);
}

// =========================================================================
// Touch controller functions (CHSC6X)
// =========================================================================
void initTouch() {
  // Hardware reset already done in setup() before display init
  // This is just software initialization (if needed)
  Serial.println("Touch controller software init complete");
}

bool readTouch(TouchPoint* tp) {
  //return false;
  Wire.beginTransmission(TOUCH_I2C_ADDR);
  Wire.write(0x02);  // Point count register
  if (Wire.endTransmission() != 0) {
    tp->touched = false;
    return false;
  }

  Wire.requestFrom(TOUCH_I2C_ADDR, (uint8_t)7);
  if (Wire.available() < 7) {
    tp->touched = false;
    return false;
  }

  uint8_t points = Wire.read();
  uint8_t x_high = Wire.read();
  uint8_t x_low = Wire.read();
  uint8_t y_high = Wire.read();
  uint8_t y_low = Wire.read();
  Wire.read();  // pressure (unused)
  Wire.read();  // area (unused)

  if (points > 0) {
    tp->touched = true;
    tp->x = ((x_high & 0x0F) << 8) | x_low;
    tp->y = ((y_high & 0x0F) << 8) | y_low;
    return true;
  } else {
    tp->touched = false;
    return false;
  }
}

TouchZone getTouchZone(int16_t x) {
  // Divide 240px screen into thirds
  if (x < 80) return ZONE_LEFT;
  else if (x < 160) return ZONE_CENTER;
  else return ZONE_RIGHT;
}

void handleTouch(TouchZone zone) {
  switch (uiState) {
    case STATE_NORMAL:
      // Any touch enters menu
      //uiState = STATE_MENU;
      //Serial.println("Entering menu mode");
      // ignore
      break;

    case STATE_MENU:
      if (zone == ZONE_LEFT) {
        // Navigate left (previous option)
        currentOption = (MenuOption)((currentOption - 1 + menuOptionCount) % menuOptionCount);
        Serial.print("Menu: ");
        Serial.println(menuOptions[currentOption]);
      }
      else if (zone == ZONE_RIGHT) {
        // Navigate right (next option)
        currentOption = (MenuOption)((currentOption + 1) % menuOptionCount);
        Serial.print("Menu: ");
        Serial.println(menuOptions[currentOption]);
      }
      else if (zone == ZONE_CENTER) {
        // Select current option
        Serial.print("Selected: ");
        Serial.println(menuOptions[currentOption]);

        if (currentOption == MENU_OFF) {
          uiState = STATE_SLEEP;
          enterSleep();
        } else if (currentOption == MENU_CLOCK) {
          uiState = STATE_NORMAL;
          Serial.println("Returning to clock display");
        }
      }
      break;

    case STATE_SLEEP:
      // Any touch wakes from sleep
      //uiState = STATE_NORMAL;
      //currentOption = MENU_CLOCK;
      //wakeFromSleep();
      // ignore
      break;
  }
}

void handleLongPress(TouchZone zone) {
  Serial.printf("Long press in zone %d, state %d\n", zone, uiState);

  // Long press behavior based on current state
  switch (uiState) {
    case STATE_NORMAL:
      // Long press in normal mode - could adjust brightness, show settings, etc.
      Serial.println("Long press in normal mode - entering menu");
      uiState = STATE_MENU;
      break;

    case STATE_MENU:
      // Long press in menu - exit menu without selecting
      Serial.println("Long press in menu - returning to clock");
      uiState = STATE_NORMAL;
      break;

    case STATE_SLEEP:
      // Long press while sleeping - wake up
      Serial.println("Long press while sleeping - waking up");
      uiState = STATE_NORMAL;
      currentOption = MENU_CLOCK;
      wakeFromSleep();
      break;
  }
}

void enterSleep() {
  Serial.println("Entering sleep mode...");
  tft.fillScreen(TFT_BLACK);

  // Try inverted logic and add debug
  Serial.println("Setting backlight pin HIGH to turn OFF");
  digitalWrite(TFT_BL_PIN, LOW);
  Serial.println("Should now be off");

  // Configure touch as wake source (GPIO interrupt)
  // Note: For light sleep, we'll just turn off display and wait for touch
  // The loop will continue checking for touch to wake
}

void wakeFromSleep() {
  Serial.println("Waking from sleep...");

  // Try inverted logic and add debug
  Serial.println("Setting backlight pin LOW to turn ON");
  digitalWrite(TFT_BL_PIN, HIGH);
  Serial.println("Should now be on");

  tft.init();
  tft.setRotation(0);
}

