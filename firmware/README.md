# Firmware

Preferred workflow: **PlatformIO** (supports Arduino and ESP-IDF).

## Layout
- `src/` - production firmware entry point (PlatformIO default)
- `bringup/` - one-off test programs and notes

## Build (PlatformIO)
From repo root or `firmware/`:
- `pio run`
- `pio run -t upload`
- `pio device monitor`

Note: Update the `board = ...` value in `platformio.ini` to match your ESP32-C6 dev kit.