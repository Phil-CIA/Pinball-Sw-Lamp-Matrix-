# Hardware Notes

This folder contains the KiCad project files and production exports for the pinball switch + lamp matrix board.

## Current state
- `Pinball Matrix board Rev 1.kicad_sch` is the checked-in schematic source for Rev 1.
- `hardware/` also contains routed PCB snapshots, Gerbers, drill files, a BOM CSV, and the zipped production package used for ordering.
- Rev 1 PCB + stencil were ordered on `2026-04-09`; treat the ordered files as the frozen reference until hardware bring-up starts.

## Recommended use
1. Use `../docs/SCHEMATIC_REVIEW.md` for design review context and accepted tradeoffs.
2. Use `../docs/BRINGUP_CHECKLIST.md` when the first boards arrive.
3. Record any assembly or bench findings back into `../docs/NEXT_ITERATION.md` and `../docs/PROJECT_STATUS.md`.

## Caution
- More than one PCB snapshot exists in this folder (`pre route`, `routed`, `routed power`).
- If a post-order edit is made, clearly note whether it belongs to the frozen Rev 1 archive or to future Rev 2 work.
