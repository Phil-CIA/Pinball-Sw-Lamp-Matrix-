# Pulse Timing Handoff - 2026-05-01

Status: Local implementation drafted on this workstation, not yet built or bench-validated.
Scope: Matrix shift-register timing, lamp-first row sequencing, and follow-on switch sampling placement.

## Why this handoff exists
The important point from today's review is that determinism has to come from the shift-register write itself.
The time-critical primitive should do only this:

1. Take the current row/column image.
2. Apply the intended row/column value.
3. Clock and latch the frame.

No scan logic, no conditional behavior, and no readback work should live inside that primitive.
That keeps row-to-row edge placement repeatable and makes the logic analyzer captures interpretable.

## Current status on this workstation
Primary source file: firmware/src/main.cpp

Local edits have already been made in that file to move the non-attract scan path to an explicit scheduler with microsecond deadlines.
The scheduler now uses these phases:

1. Blank
2. Drive
3. Settle
4. Sample
5. Hold

The current local implementation also separates row and column SR writes instead of relying on one combined row transition.

Important limitation: this workstation shell does not have ESP-IDF initialized, so the change has not been built here. `idf.py` was not available on `PATH`, and editor diagnostics only reported missing ESP-IDF include paths.

## Lamp-first design decision
The same base SR routine applies to both lamp drive and switch scanning, but the next bench target should be lamp timing first.
Reason: if row and column transitions are electrically clean for lamps, the same scheduler structure gives a stable foundation for switch sampling afterward.

The intended per-row write order is:

1. Blank write: all rows off, all columns off.
2. Row-on write: target row active, columns still off.
3. Column-on write: same row active, desired columns driven.
4. Settle delay: allow external hardware to reach a stable state.
5. Sample point: used for switches later, at a fixed delay from the final latch.
6. Columns-off write: same row still selected, columns forced off.
7. Dead-time: short off interval to cover driver/storage/RC recovery.
8. Row-off write: force row inactive before the next row begins.

This ordering avoids both of the bad overlap cases:

- new row with old columns
- old row with new columns

## Timing model to bench-tune
Current local constants in firmware/src/main.cpp:

- `SWITCH_SCAN_ROW_MS = 5`
- `ROW_BLANK_US = 100`
- `ROW_SETTLE_US = 100`
- `ROW_OFF_DEADTIME_US = 50`

These are starting values only. The MCU is not the limiting factor; the external devices are. Budget must cover:

- shift-register propagation and latch behavior
- row-driver turn-on and turn-off timing
- column-line RC settling
- any analog or GPIO recovery before a sample is trusted

## What was changed locally
In firmware/src/main.cpp, the local uncommitted edit does the following:

1. Adds a dedicated `RowPhase` enum and `RowScheduler` state.
2. Adds `sr_write_image(rowByte, colByte)` so the SR write path stays simple and constant-time.
3. Replaces the old coarse row-step logic with explicit microsecond phase deadlines.
4. Splits non-attract row service into separate writes for row-only and row-plus-columns.
5. Moves switch sampling to the fixed post-settle sample phase.
6. Adds heartbeat visibility for scheduler phase and overrun count.

This is the right direction, but it still needs a real build and bench capture before commit.

## Outstanding files to carry and commit
As of this handoff, the matrix repo has these outstanding modified files:

- `firmware/src/main.cpp`
- `docs/PULSE_TIMING_HANDOFF_2026-05-01.md`

Control-board repo on this workstation showed no outstanding changes.

## Required validation on the other workstation
Use an ESP-IDF-enabled shell on the other workstation.

1. Pull the latest matrix repo state from the remote.
2. Bring over or reapply the local edits if they are not yet committed.
3. Run `idf.py build` in the firmware directory.
4. Capture LA traces on at least: row output, SR latch, SR clock, and one column input.
5. Confirm this sequence electrically:
   - blank frame
   - row-on / columns-off frame
   - row-on / columns-on frame
   - settle interval
   - sample instant
   - columns-off frame
   - row-off frame
6. Tune `ROW_BLANK_US`, `ROW_SETTLE_US`, and `ROW_OFF_DEADTIME_US` until overlap is gone and timing is repeatable.

## Commit target after validation
If the build passes and the waveform looks correct, commit the two modified matrix files above as the timing-stabilization checkpoint.

Suggested commit scope:

- deterministic SR scheduler and explicit row/column timing sequence
- updated handoff notes with lamp-first validation plan

## Non-goals for this pass
Do not widen this pass into gameplay or control integration yet.
Still out of scope until the lamp timing is proven:

- control-board solenoid coupling
- scoring behavior
- audio parity work
- broader protocol feature work beyond what is needed to exercise lamp timing

## Immediate next action on the other workstation
Open an ESP-IDF shell, build the matrix firmware, bench the lamp waveform first, then commit the outstanding matrix files once the timing constants are validated.

Last updated: 2026-05-03
Owner for next step: bench firmware dev workstation
