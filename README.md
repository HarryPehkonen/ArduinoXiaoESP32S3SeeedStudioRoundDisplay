# Seeed Studio XIAO Round Display - Hardware Quirks & Solutions

This document describes hardware-specific issues and workarounds for the Seeed Studio XIAO Round Display (GC9A01 driver) with ESP32S3/C3.

## Critical: GPIO9 Pin Conflict

**Problem:** GPIO9 is shared between two functions:
- Touch controller reset pin
- Display SPI MOSI pin

**Symptom:** If touch controller is initialized after display, the display freezes and stops updating.

**Solution:** Initialize touch controller BEFORE display in a specific order:

```cpp
void setup() {
  // 1. TOUCH RESET FIRST (before display claims GPIO9)
  pinMode(TOUCH_RST_PIN, OUTPUT);  // GPIO9
  digitalWrite(TOUCH_RST_PIN, LOW);
  delay(10);
  digitalWrite(TOUCH_RST_PIN, HIGH);
  delay(50);

  // 2. DISPLAY INIT (reconfigures GPIO9 as SPI MOSI)
  tft.init();

  // 3. I2C AND SOFTWARE INIT
  Wire.begin();
  rtc.begin();
  // Touch controller software init (no pin toggling!)
}
```

**Why:** Once the TFT library initializes, GPIO9 becomes SPI MOSI. Any subsequent attempt to toggle it as a GPIO will corrupt SPI communication and freeze the display.

---

## Touch Controller (CHSC6X) Extreme Bouncing

**Problem:** The CHSC6X capacitive touch controller at I2C address 0x2E exhibits severe contact bouncing - it rapidly toggles between touched/not-touched states even when a finger is steadily held down.

**Symptoms:**
- Single tap registers as multiple rapid taps (16ms, 68ms, 94ms apart)
- Long press impossible - breaks into many short presses
- Touch state changes 10+ times per 100ms

**Root Cause:** Hardware exhibits **false negatives** (misses touches) not false positives. The controller intermittently loses contact detection while finger remains down.

**Solution:** Majority vote filter with aggressive hysteresis:

```cpp
// Keep last 8 touch readings in bit history
static uint8_t touchHistory = 0;
touchHistory = (touchHistory << 1) | (rawTouch ? 1 : 0);

// Count bits
uint8_t touchCount = __builtin_popcount(touchHistory);

// Hysteresis thresholds
constexpr uint8_t TOUCH_ON_THRESHOLD = 6;   // Need 6/8 to START touch
constexpr uint8_t TOUCH_OFF_THRESHOLD = 0;  // Need 0/8 to END touch

// Update filtered state with hysteresis
if (!filteredTouch && touchCount >= TOUCH_ON_THRESHOLD) {
  filteredTouch = true;   // Strong signal to start
} else if (filteredTouch && touchCount <= TOUCH_OFF_THRESHOLD) {
  filteredTouch = false;  // Must be completely clear to end
}
// Otherwise maintain current state (sticky zone)
```

**Key insight:** Once touch is detected (6/8 samples), it remains "touched" even if only 1/8 subsequent samples detect touch. Only when 0/8 samples (completely clear) does it register as released.

**Debouncing:** Additional 500ms minimum between touch actions prevents accidental double-triggers.

---

## NTP Time Synchronization

**Problem:** `configTime()` is asynchronous - calling `getLocalTime()` immediately after may fail or return stale time.

**Solution:** Poll with timeout until NTP sync completes:

```cpp
configTime(0, 0, ntpServer);
setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
tzset();

// Wait up to 10 seconds for NTP sync
struct tm timeInfo;
bool gotTime = false;
for (int i = 0; i < 20; i++) {
  if (getLocalTime(&timeInfo)) {
    gotTime = true;
    break;
  }
  delay(500);
}

if (gotTime) {
  // Set RTC from NTP time
  rtc.setTime(...);
}
```

---

## Display Sprite Corruption During WiFi

**Problem:** WiFi operations can corrupt the TFT sprite buffer, causing subsequent renders to fail.

**Solution:** Delete and recreate sprite after WiFi/NTP initialization:

```cpp
// After WiFi.begin() and configTime()...

// Recreate sprite for clean state
face.deleteSprite();
face.createSprite(FACE_W, FACE_H);
face.loadFont(NotoSansBold15);

// Now safe to render
renderFace(time_secs);
```

---

## Backlight Control

**Pin:** GPIO43 (D6 on XIAO)
**Logic:** Normal (HIGH = on, LOW = off)
**Hardware switch:** KE switch on board must be ON for software control

**CRITICAL:** The "KE switch" on the board must be set to ON. When OFF, the backlight stays on regardless of GPIO state.

**Implementation:**
```cpp
const int TFT_BL_PIN = 43;  // GPIO43 = D6 on XIAO

void setup() {
  pinMode(TFT_BL_PIN, OUTPUT);
  digitalWrite(TFT_BL_PIN, HIGH);  // Backlight ON
}

// Sleep mode - turn off backlight
void enterSleep() {
  digitalWrite(TFT_BL_PIN, LOW);   // Backlight OFF
}

// Wake mode - turn on backlight
void wakeFromSleep() {
  digitalWrite(TFT_BL_PIN, HIGH);  // Backlight ON
}
```

**PWM brightness control** (if supported by hardware):
```cpp
analogWrite(TFT_BL_PIN, 255);  // Full brightness
analogWrite(TFT_BL_PIN, 128);  // 50% brightness
analogWrite(TFT_BL_PIN, 0);    // Off
```

**Note:** On XIAO ESP32S3, D6 maps to GPIO43 (NOT GPIO6). Do not use GPIO19/GPIO20 as they are reserved for USB.

---

## Battery Voltage Reading

**ADC Pin:** D0
**Voltage Divider:** 2:1 (hardware divider on board)

```cpp
const int BAT_PIN = D0;
constexpr float BATTERY_VOLTAGE_DIVIDER = 2.0f;

float voltage = (analogReadMilliVolts(BAT_PIN) * BATTERY_VOLTAGE_DIVIDER) / 1000.0f;
```

**18650 Battery Voltage Range:**
- Fully charged: 4.2V
- Nominal: 3.7V
- Low cutoff: 3.0V
- Observed minimum: 2.65V (protection circuit)

**Non-linear discharge curve:** Use cubic polynomial or lookup table for accurate percentage calculation.

---

## Touch and Long Press Detection

**Gestures supported:**
- **Tap:** Quick touch and release
- **Long press:** Hold for 1000ms

**Debouncing:** 500ms minimum between actions
**Long press threshold:** 1000ms hold time

**Implementation notes:**
- Majority vote filter runs every loop iteration (~100ms rate)
- Long press detection checks filtered touch state, not raw
- Once long press triggers, tap is suppressed on release

---

## Hardware Configuration Summary

| Component | Pin/Address | Notes |
|-----------|-------------|-------|
| Display (GC9A01) | SPI | 240x240 round, MOSI on GPIO9 |
| Touch (CHSC6X) | I2C 0x2E | Reset on GPIO9 (shared!) |
| RTC (BM8563) | I2C 0x51 | Persistent timekeeping |
| Battery ADC | D0 | 2:1 voltage divider |
| Backlight | GPIO43 (D6) | HIGH=on, LOW=off. KE switch must be ON |

---

## Initialization Order (Critical!)

1. Battery pin setup
2. **Backlight pin setup** (GPIO43/D6 as output, set HIGH for on)
3. **Touch controller hardware reset** (GPIO9 as output)
4. **Display initialization** (GPIO9 becomes SPI MOSI)
5. Sprite creation
6. Font loading
7. Initial render (optional)
8. WiFi connection
9. NTP sync with polling
10. RTC initialization
11. **Sprite recreation** (post-WiFi cleanup)
12. Touch controller software init (no pin manipulation)
13. Battery reading
14. Final render with correct time

**Violating this order will cause display freeze or touch malfunction.**

---

## Lessons Learned

- **GPIO conflicts are real:** Shared pins require careful initialization sequencing
- **Capacitive touch is noisy:** Software filtering essential for reliable operation
- **Hysteresis matters:** Different thresholds for on/off transitions handle bouncing
- **WiFi corrupts memory:** Reinitialize graphics buffers after network operations
- **NTP is async:** Always poll for completion, never assume immediate availability
- **Document hardware quirks:** Future you (and others) will thank you

---

## References

- [Seeed Studio XIAO Round Display Getting Started](https://wiki.seeedstudio.com/get_started_round_display_xiao/)
- [Seeed Studio Round Display Usage Guide](https://wiki.seeedstudio.com/seeedstudio_round_display_usage/)
- [GC9A01 Display Driver Datasheet](https://www.waveshare.com/w/upload/5/5e/GC9A01A.pdf)
- [CHSC6X Touch Controller](https://github.com/Xinyuan-LilyGO/T-Display-S3-Long/blob/main/doc/CST816S_DataSheet_EN.pdf)
- [TFT_eSPI Library](https://github.com/Bodmer/TFT_eSPI)
