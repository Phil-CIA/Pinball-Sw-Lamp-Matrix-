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
- lock down connector pin naming and numbering
- confirm the exact ESP32-C6 bring-up board/module
- review lamp current expectations and fuse sizing
- prepare for the first real PCB layout revision

## Parking lot
- confirm whether the final firmware should stay ESP-IDF-only or also support Arduino later
- decide if lamp column drivers need any extra protection or test points beyond the current plan
- consider whether a dedicated `docs/PINMAP.md` should become the source of truth for signal naming

## Assumptions to validate
- `VNQ7E100AJTR` remains the preferred high-side row driver
- `LMV393` input conditioning is still the right switch-scan approach
- `74HC595` output expansion is sufficient for the first revision
- the current power-entry protection plan matches the real machine environment

## Things that may need reconsideration
- connector family and harness approach
- row/column labeling convention
- default boot-safe behavior for lamp outputs
- EMI tuning parts to stuff by default vs DNP
- future firmware architecture once hardware bring-up is complete

## Open questions
- what exact connector count and pinout should revision 1 use?
- what lamp load/current should be treated as the normal design target?
- which signals need permanent test points on the first PCB revision?
- should the repo track a formal decision log later?

## Notes log
- 2026-04-06: initial parking-lot document added for repo housekeeping and iteration tracking.
