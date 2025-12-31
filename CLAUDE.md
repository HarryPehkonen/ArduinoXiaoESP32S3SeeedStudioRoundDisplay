# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Arduino-based analog clock display for round TFT displays (GC9A01 driver) using the TFT_eSPI library. Designed for Seeed Studio XIAO ESP32S3/C3 boards with integrated BM8563 RTC for timekeeping.

## Hardware Configuration

- **Display**: Round Display for Seeed Studio XIAO (GC9A01 driver, 240px diameter)
- **Board**: ESP32S3 or ESP32C3 (controlled via `CONFIG_IDF_TARGET_ESP32S3`/`CONFIG_IDF_TARGET_ESP32C3`)
- **RTC**: BM8563 I2C Real-Time Clock module
- **Board identifier**: `BOARD_SCREEN_COMBO 501` in `driver.h`

## Build & Upload

This is a standard Arduino sketch. Build and upload using:

**Arduino IDE**:
1. Open `TFT_eSPI_Clock/TFT_eSPI_Clock.ino`
2. Select board: ESP32S3 Dev Module or ESP32C3 Dev Module
3. Configure TFT_eSPI library (see TFT_eSPI Configuration below)
4. Click Upload

**Arduino CLI**:
```bash
arduino-cli compile --fqbn esp32:esp32:esp32s3 TFT_eSPI_Clock/
arduino-cli upload --fqbn esp32:esp32:esp32s3 -p /dev/ttyACM0 TFT_eSPI_Clock/
```

**PlatformIO** (if configured):
```bash
pio run -t upload
```

## TFT_eSPI Configuration

The TFT_eSPI library requires configuration in its `User_Setup.h` or via a custom setup file. This project expects:
- Driver: GC9A01
- SPI pins defined for XIAO ESP32 boards
- Display size: 240x240 round

The library is configured at the library installation level, not in this repository.

## Code Architecture

### Rendering Strategy

The clock uses **sprite-based rendering** to eliminate flicker:
1. All graphics drawn to `face` sprite (TFT_eSprite) in memory
2. Sprite pushed to display in one operation via `pushSprite()`
3. Complete redraw every 100ms for smooth sub-pixel hand movement

### Time Management

**Dual time system**:
- `time_secs` (float): Internal smooth time counter incremented by 0.1s each loop iteration
- `rtc` (I2C_BM8563): Hardware RTC for persistent timekeeping

**Flow**:
1. On boot: WiFi NTP sync (ESP32S3/C3 only) → set RTC → initialize `time_secs`
2. Runtime: `time_secs` increments smoothly every 100ms for animation
3. Periodic sync with `syncTime()` can resync `time_secs` from RTC (currently commented out in loop)

### Key Components

**TFT_eSPI_Clock.ino**:
- `setup()`: Initialize display, create sprite, WiFi NTP sync, RTC initialization
- `loop()`: 100ms tick updates `time_secs`, calls `renderFace()`
- `renderFace(float t)`: Draws complete clock face with anti-aliased graphics
  - Background circle
  - Hour numerals (1-12) positioned via polar coordinates
  - Hour/minute/second hands with anti-aliased lines
  - Center pivot circle
- `getCoord()`: Polar-to-Cartesian coordinate conversion for hand positions
- `syncTime()`: Reads RTC and updates `time_secs`

**driver.h**: Board/display identifier (`BOARD_SCREEN_COMBO 501`)

**NotoSansBold15.h**: Binary font data array for clock numerals

### Angle Calculation

Hands rotate smoothly using fractional angles:
- `SECOND_ANGLE = 6°` (360°/60)
- `MINUTE_ANGLE = 0.1°` per second (smooth sub-minute movement)
- `HOUR_ANGLE = 0.00833°` per second (smooth sub-hour movement)

Angles computed from `time_secs` float, enabling smooth animation between integer second boundaries.

## WiFi Configuration

WiFi credentials are hardcoded in TFT_eSPI_Clock.ino:10-11:
```cpp
const char *ssid     = "We Don't Have WiFi";
const char *password = "abracadabra";
```

NTP sync only occurs on ESP32S3/C3 boards during `setup()` via Cloudflare's NTP server. Timezone is hardcoded to UTC+8.

## Dependencies

Required Arduino libraries:
- `TFT_eSPI` - Display driver (must be configured for GC9A01)
- `I2C_BM8563` - RTC library
- `WiFi` - ESP32 WiFi (built-in)
- Standard: `Arduino.h`, `SPI.h`, `Wire.h`

## Display Customization

Color scheme defined by macros in TFT_eSPI_Clock.ino:27-30:
- `CLOCK_FG`: Sky blue (hands, numerals)
- `CLOCK_BG`: Navy blue (face background)
- `SECCOND_FG`: Red (second hand)
- `LABEL_FG`: Gold (text label)

Hand lengths and clock radius in TFT_eSPI_Clock.ino:32-35.
