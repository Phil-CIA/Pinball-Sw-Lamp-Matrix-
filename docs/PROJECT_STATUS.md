# Project Status

## Summary
This project is building a combined **8x4 lamp matrix driver** and **8x4 switch matrix scanner** board for a home-edition pinball machine.

## Current state (2026-04-27)
### Hardware
- Rev 1 schematic, PCB snapshots, Gerbers, drill files, BOM, and production ZIP are checked in under `hardware/`
- PCB, stencil, and parts were ordered on **2026-04-09**
- Board was assembled and **5V and 3.3V rails confirmed up** on **2026-04-27**
- Current phase: **active bring-up** — logic validation and staged lamp testing
- Because several PCB snapshots exist, the ordered Rev 1 package should be treated as the frozen reference point for this bring-up session

### Rev 1 assumptions locked during review
- `T10/555` incandescent lamps at `6.3V`
- roughly `28` lamps in the matrix scan envelope
- `1 oz` copper
- `0.3 mm` minimum signal traces
- `2.0–2.5 mm` row/high-current traces
- solid `GND` pour strategy preferred
- lamp outputs via `.156" / 3.96 mm` KK-396 class connectors
- power entry via JST XH, with `18V` and `GND` shared across two pins each and `5V` on a lighter-current pair

### Firmware
- PlatformIO project is present under `firmware/`
- Verified environment: `esp32-c6-devkitm-1` using **ESP-IDF**
- Bring-up code currently shifts a walking test pattern and reads four switch-column inputs

### Documentation
- Design review notes live in `docs/SCHEMATIC_REVIEW.md`
- Parking / restart notes live in `docs/NEXT_ITERATION.md`
- First-board arrival plan now lives in `docs/BRINGUP_CHECKLIST.md`
- Pin and connector mapping remains in `docs/PINMAP.md`

## Immediate next priorities
1. Flash and confirm ESP32-C6 bring-up firmware boots without reset loop
2. Probe `SR_SCLK`, `SR_LATCH`, `SR_DATA0` and confirm shift-register activity
3. Read all four `SW_COL_0..SW_COL_3` inputs and compare with expected states
4. Test one lamp row + one lamp column before running a full matrix scan
5. Record rail values, current draw, thermal notes, and any mapping surprises

## Open questions for bring-up / Rev 2
- actual measured lamp current and connector heating on real hardware
- whether any snubber / EMI options need to be stuffed by default
- any connector pinout or labeling adjustments after first install
- whether firmware remains ESP-IDF-only or later adds Arduino support
