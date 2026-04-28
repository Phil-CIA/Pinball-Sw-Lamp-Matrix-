# Firmware

Preferred workflow: **PlatformIO** using the verified **ESP-IDF** environment for ESP32-C6.

## Layout
- `src/` - production firmware entry point (PlatformIO default)
- `bringup/` - one-off test programs and notes

## Build (PlatformIO)
From `firmware/`:
- `python -m platformio run`
- `python -m platformio run -t upload --upload-port COM4`
- `python -m platformio device monitor --port COM4 --baud 115200`

If ESP-IDF tool version checks block a local bring-up build, run in the same shell session:
- PowerShell: `$env:IDF_MAINTAINER='1'`

## Notes
- Verified on 2026-04-06: `esp32-c6-devkitm-1` currently builds with `framework = espidf` in PlatformIO.
- If you later move to a board definition with Arduino support, an Arduino environment can be added back into `platformio.ini`.
- Update the `board = ...` value in `platformio.ini` if your exact ESP32-C6 dev kit differs.
- For this repository, build from the no-space path (`C:\cfhe\Pinball-Sw-Lamp-Matrix\firmware`) when possible. Path whitespace has caused intermittent ESP-IDF failures.

## Bring-up Runtime Modes (2026-04-28)
Current firmware provides fixed bring-up runtime behavior selected in `src/main.cpp` using `kRuntimeMode`.

Available modes:
- `RuntimeMode::LampAttract`
	- Low-duty attract chase for lamp mapping and brightness sanity
	- Timing controlled by `ATTRACT_SLOT_MS` and `ATTRACT_ON_MS`
- `RuntimeMode::SwitchScan` (current default)
	- Rows are scanned continuously with lamp columns held off
	- Switch-column reads are sampled per row and accumulated
	- Serial prints include:
		- `HB ...`
		- `SWITCH_SCAN row=... hits[rX]=...`
		- `SW_EDGE/s: ...`
		- `CTRL_I2C edges/s: ...`

## Current validated pin map
- Shift register drive:
	- `S_Data = GPIO15`
	- `S_CLK = GPIO22`
	- `S_latch = GPIO23`
	- `SR_/OE = GPIO10` (driven low to enable outputs)
- Switch comparator outputs:
	- `SW_COL_0 = GPIO18`
	- `SW_COL_1 = GPIO19`
	- `SW_COL_2 = GPIO20`
	- `SW_COL_3 = GPIO21`
- OLED:
	- `SDA = GPIO7`
	- `SCL = GPIO6`

## Quick verify sequence
1. Build and upload from `C:\cfhe\Pinball-Sw-Lamp-Matrix\firmware`
2. Open serial monitor on COM4
3. Confirm startup shows `Runtime mode: SWITCH_SCAN ...`
4. Confirm per-second updates show `HB`, `SWITCH_SCAN`, and `SW_EDGE/s`