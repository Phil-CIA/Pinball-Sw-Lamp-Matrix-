# Pulse Timing Handoff - 2026-05-01

Status: Ready for implementation and bench validation on next workstation.
Scope: Matrix row pulse timing and sequence stabilization only.

## Why this handoff exists
Logic analyzer captures indicate row pulse behavior is not yet shaped as intended.
Observed concern: row transitions appear stacked/overlapping and current firmware does not explicitly enforce blank/off and settle timing windows between row changes and sampling.

## Current implementation snapshot (confirmed)
Primary source file: firmware/src/main.cpp

- Runtime mode in current build: `RuntimeMode::SwitchScan`
- Target row step constant: `SWITCH_SCAN_ROW_MS = 5`
- Loop delay: `vTaskDelay(pdMS_TO_TICKS(10))`
- Row advance: increments `activeRow` when row interval elapses
- Frame drive: writes one-hot row frame immediately with `sr_shift_frame(...)`
- Sampling: switch columns are read in same loop pass after frame shift
- Missing explicit timing phases:
  - no forced blank/off phase between rows
  - no explicit settle delay before sampling

This is likely the source of non-ideal waveform shape and overlap appearance on LA.

## Stabilization objective
Introduce deterministic row timing phases so each row cycle is explicit and measurable:

1. Blank phase (all rows off)
2. Drive phase (exactly one row on)
3. Settle phase (allow hardware propagation/RC settle)
4. Sample phase (read columns at a fixed point)
5. Hold/advance phase (maintain total slot period target)

## Planned timing model (initial bench values)
Start with constants, then tune from captures:

- Row slot target: 5 ms
- Blank window: 50 to 150 us
- Settle window: 50 to 200 us
- Sample point: fixed after settle
- Dwell: remainder of slot to keep stable cadence

Note: microsecond windows are starting points for bench tuning, not locked values.

## Implementation tasks
1. Add grouped timing constants in firmware/src/main.cpp for blank/settle/sample timing.
2. Refactor row scheduler to explicit phase/state machine.
3. Enforce one-row-active invariant:
   - all rows off during blank
   - one row active during drive
   - no direct row-to-row transition without blank
4. Move switch sampling to a deterministic post-settle sample point.
5. Add lightweight instrumentation:
   - phase overrun/miss counters
   - optional debug GPIO pulse at sample instant for LA alignment
6. Keep existing bring-up visibility (OLED status, heartbeat diagnostics) intact.

## Verification checklist (required)
Use same LA channels/timebase for before/after comparisons.

- [ ] No dual-row overlap during transitions
- [ ] One-hot row behavior at all times
- [ ] Sample instant consistently after settle window
- [ ] Stable full scan period over >= 10 seconds
- [ ] Switch detection remains correct under rapid actuation
- [ ] No new false double-trigger artifacts

## Dependencies and non-goals
Included now:
- matrix row pulse timing/sequence stabilization
- measurement-backed waveform validation

Not included in this pass:
- gameplay scoring logic
- control-board solenoid mapping changes
- audio parity migration
- broader protocol expansion beyond current scope

## Related references
In this repo:
- firmware/src/main.cpp
- docs/CONTROL_MATRIX_INTERFACE_V1.md
- docs/SYSTEM_BEHAVIOR_CONTRACT_MATRIX_BOARD.md

Cross-repo context (control board repo):
- firmware/control-board/reference/legacy_control_main.cpp (deterministic pulse style reference)
- firmware/control-board/include/solenoid_gpio_config.h (pulse-width constants)

## Immediate next action on other workstation
1. Pull latest from main.
2. Implement scheduler phase model in firmware/src/main.cpp.
3. Capture LA before/after and record measured timing deltas.
4. Update docs/PROJECT_STATUS.md with measured results once first clean non-overlap capture is confirmed.

Last updated: 2026-05-01
Owner for next step: bench firmware dev workstation
