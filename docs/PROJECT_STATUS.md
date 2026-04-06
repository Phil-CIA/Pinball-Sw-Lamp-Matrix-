# Project Status

## Summary
This project is building a combined **8x4 lamp matrix driver** and **8x4 switch matrix scanner** board for a home-edition pinball machine.

## Current state
### Hardware
- Schematic is the active source of truth
- Key parts and review notes are captured
- `hardware/Pinball Matrix board Rev 1.kicad_pcb` is still a **placeholder**
- First real PCB layout has **not** been started yet

### Firmware
- PlatformIO project is present under `firmware/`
- Verified environment: `esp32-c6-devkitm-1` using **ESP-IDF**
- Bring-up code currently shifts a walking test pattern and reads four switch-column inputs

### Documentation
- Core repo layout and baseline docs are now in place
- Pin and connector mapping now lives in `docs/PINMAP.md`
- PCB review checklist exists in `docs/PCB_REVIEW_CHECKLIST.md`
- Key parts summary exists in `BOM/key_parts.md`

## Immediate next priorities
1. Freeze row/column naming and connector pin assignments
2. Finish schematic review and footprint checks
3. Start the first routed PCB revision
4. Bring up the real row/column hardware on the ESP32-C6 board

## Open questions to lock down
- Final connector choice and pin numbering
- Exact ESP32-C6 module/dev board used for bring-up
- Lamp current expectations and fuse sizing
- Whether future firmware stays ESP-IDF-only or later adds Arduino support
