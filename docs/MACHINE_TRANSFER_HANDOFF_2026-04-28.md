# Machine Transfer Handoff (2026-04-28)

Purpose: resume matrix-board bring-up tomorrow from another machine, then continue execution here with minimal context loss.

## Repo and Branch
- Repo: `Phil-CIA/Pinball-Sw-Lamp-Matrix-`
- Branch: `main`
- Working tree status at handoff creation: clean

## Last Pushed Commits (newest first)
- `0d4cc17` Ignore local bring-up capture logs
- `850f767` Checkpoint: Rev1 hardware edits and firmware platform config
- `8a48dfa` Add switch-scan bring-up mode and update checkpoint docs

## Current Firmware State (Matrix Repo)
- File: `firmware/src/main.cpp`
- Runtime mode selector exists:
  - `RuntimeMode::LampAttract`
  - `RuntimeMode::SwitchScan`
- Current default is:
  - `kRuntimeMode = RuntimeMode::SwitchScan`
- Validated pins:
  - Shift register: `DATA=GPIO15`, `CLK=GPIO22`, `LATCH=GPIO23`, `OE_N=GPIO10`
  - Switch columns: `SW_COL_0..3 = GPIO18..GPIO21`
  - OLED: `SDA=GPIO7`, `SCL=GPIO6`
  - Control-link monitor: `SDA=GPIO2`, `SCL=GPIO3`

## Bench Findings So Far
- Lamp hardware path now mostly functional after correcting swapped gate resistor placements on 3 of 4 lamp columns.
- Brightness concern addressed in firmware by low-duty attract pulse timing.
- Switch validation mode implemented and running with telemetry:
  - `HB ...`
  - `SWITCH_SCAN row=... hits[rX]=...`
  - `SW_EDGE/s: ...`
  - `CTRL_I2C edges/s: ...`
- OLED is considered "good enough" for bring-up status display.

## Hardware Action Item (Comparator Clamp)
- Current fitted clamp device: BAT54C in SOT-23.
- For signal-to-rails clamp use at comparator input, BAT54S topology is preferred.
- Temporary field fix discussed:
  - Lift BAT54C pin tied to +5 side (pin 1 in your described orientation).
  - Keep signal node and GND clamp side connected.
  - Add external `1N4148` with **anode at signal**, **cathode at +5V**.

## Build/Upload Notes (important)
- Preferred build path: no-space path to avoid ESP-IDF/PlatformIO path issues:
  - `C:\cfhe\Pinball-Sw-Lamp-Matrix\firmware`
- In PowerShell, if tool version guard blocks build:
  - `$env:IDF_MAINTAINER='1'`
- Typical commands:
  - `C:\Users\user\.platformio\penv\Scripts\platformio.exe run -e esp32c6_espidf`
  - `C:\Users\user\.platformio\penv\Scripts\platformio.exe run -e esp32c6_espidf -t upload --upload-port COM4`
  - `C:\Users\user\.platformio\penv\Scripts\platformio.exe device monitor --port COM4 --baud 115200`

## Tomorrow Morning Plan (execution order)
1. Hardware pre-check
- Confirm temporary clamp fix applied (or BAT54S replacement if parts arrived).
- Verify no accidental shorts around comparator input network and +5/GND rails.

2. Firmware sanity
- Pull latest `main`.
- Build and upload from `C:\cfhe\Pinball-Sw-Lamp-Matrix\firmware`.
- Confirm boot banner and runtime mode line in serial output.

3. Switch verification pass
- Keep runtime mode in `SwitchScan`.
- Manually actuate known switches and confirm:
  - `SW_EDGE/s` increments on expected columns.
  - `SWITCH_SCAN hits[rX]` increments on expected row/col combinations.
- Record mismatches in a row/col table.

4. Lamp verification pass
- Temporarily switch to `LampAttract` if needed for mapping/brightness checks.
- Confirm expected lamp mapping and acceptable brightness/thermal behavior.

5. End-of-session hygiene
- Save logs/notes, update `docs/BRINGUP_CHECKLIST.md` and `CHANGELOG.md` if new findings occur.
- Commit only intentional source/docs changes.

## Suggested Resume Prompt (for tomorrow)
"Resume matrix board bring-up from docs/MACHINE_TRANSFER_HANDOFF_2026-04-28.md. Start in RuntimeMode::SwitchScan, verify switch row/column hits and edge counters, then optionally run RuntimeMode::LampAttract for lamp mapping/brightness check. Build/upload from C:\\cfhe\\Pinball-Sw-Lamp-Matrix\\firmware and log pass/fail with measured observations."
