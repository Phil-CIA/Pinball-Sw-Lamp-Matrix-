# Firmware

Preferred workflow: **PlatformIO** using the verified **ESP-IDF** environment for ESP32-C6.

## Layout
- `src/` - production firmware entry point (PlatformIO default)
- `bringup/` - one-off test programs and notes

## Build (PlatformIO)
From `firmware/`:
- `python -m platformio run`
- `python -m platformio run -t upload`
- `python -m platformio device monitor`

## Notes
- Verified on 2026-04-06: `esp32-c6-devkitm-1` currently builds with `framework = espidf` in PlatformIO.
- If you later move to a board definition with Arduino support, an Arduino environment can be added back into `platformio.ini`.
- Update the `board = ...` value in `platformio.ini` if your exact ESP32-C6 dev kit differs.