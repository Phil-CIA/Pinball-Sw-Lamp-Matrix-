# Next Iteration / Parking Lot

This file is the project **parking lot** for ideas, questions, risks, and assumptions that should be captured now and revisited later.

Use it to avoid losing useful thoughts while keeping the current work focused.

## How to use this file
- add short notes, not long essays
- keep the newest items near the top of each section
- mark items as resolved when they are closed
- move stable decisions into `README.md`, `docs/PROJECT_STATUS.md`, or the schematic notes

---

## Current next-iteration focus
- ✅ 5V and 3.3V rails confirmed up on assembled Rev 1 board (2026-04-27)
- Active phase: firmware flash → logic validation → staged lamp testing
- keep `docs/PINMAP.md` and bring-up notes ready for first-board debugging

## Restart here when hardware arrives
1. **Visual inspection**
   - polarity/orientation check for diodes, TVS parts, connectors, and the ESP32 board
   - inspect VNQ thermal pad soldering and stencil results
   - check for bridges, tombstones, and damaged pads

2. **Pre-power checks**
   - continuity / resistance check on `18V`, `5V`, `3.3V`, and `GND`
   - verify fuse and TVS placement against the schematic
   - confirm connector polarity and harness pin 1 direction

3. **First power-up**
   - bring the board up with current limiting if possible
   - verify `5V` and `3.3V` rails first
   - then confirm the ESP32-C6 boots and the firmware still runs as expected

4. **Functional validation**
   - verify shift-register activity
   - verify all four switch-column comparator outputs
   - test one lamp row / one lamp column first
   - then move to a full matrix scan

5. **Capture findings**
   - record any thermal, noise, connector, or routing surprises
   - note anything that should roll into a likely Rev 2

## Parking lot
- confirm whether the final firmware should stay ESP-IDF-only or also support Arduino later
- decide if any lamp column protection or snubber options should be stuffed by default after bench testing
- keep `docs/PINMAP.md` updated as the source of truth for signal naming and board-mating notes

## Assumptions to validate on real hardware
- `VNQ7E100AJTR` remains the preferred high-side row driver
- `LMV393` input conditioning is the right switch-scan approach without extra hysteresis
- `74HC595` output expansion is sufficient for the first revision
- the current power-entry protection plan matches the real machine environment
- `1 oz` copper with `0.3 mm` minimum signal traces and `2.0–2.5 mm` row-current traces is adequate for Rev 1

## Things that may need reconsideration after bring-up
- connector family / harness approach in long-term service
- row / column labeling convention on the installed machine
- default boot-safe behavior for lamp outputs
- EMI tuning parts to stuff by default vs DNP
- firmware architecture once hardware validation is complete

## Open questions
- what real lamp current and connector heating are seen on the bench?
- do any comparator thresholds or reference values need tweaking after real switch tests?
- which DNP protection / tuning parts should become default-fit for Rev 2?
- should the repo track a formal decision log later?

## Notes log
- 2026-04-27: **5V and 3.3V rails confirmed up on assembled Rev 1 board.** Active bring-up phase started.
- 2026-04-27: **Rev 2 note:** 0805 component pads benefit from ~1.5 mm trace width at the pad; update design rules for Rev 2.
- 2026-04-09: Rev 1 PCB, stencil, and parts were ordered. Expected flow is to pause active design work, wait for the hardware, and resume with a structured bring-up pass when the boards arrive.
- 2026-04-07: follow-up schematic review used the current saved repo copy; if new KiCad edits are made, save/export before the next review pass.
- 2026-04-06: initial parking-lot document added for repo housekeeping and iteration tracking.
