# Project Status

## Summary
This project is building a combined **8x4 lamp matrix driver** and **8x4 switch matrix scanner** board for a home-edition pinball machine.

## Current state (2026-04-09)
### Hardware
- Rev 1 schematic, PCB snapshots, Gerbers, drill files, BOM, and production ZIP are checked in under `hardware/`
- PCB, stencil, and parts were ordered on **2026-04-09**
- The project is now in a **wait-for-hardware / bring-up-prep** phase rather than active rerouting
- Because several PCB snapshots exist, the ordered Rev 1 package should be treated as the frozen reference point until bring-up starts

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
1. Keep the ordered Rev 1 package and notes as the reference point
2. Prepare the bench bring-up setup, tools, and test harness
3. When boards arrive: inspect, short-check rails, power up safely, and validate logic before full lamp loading
4. Record findings for a likely Rev 2 cleanup pass

## Open questions for bring-up / Rev 2
- actual measured lamp current and connector heating on real hardware
- whether any snubber / EMI options need to be stuffed by default
- any connector pinout or labeling adjustments after first install
- whether firmware remains ESP-IDF-only or later adds Arduino support
