# Machine Transfer Handoff (2026-04-27)

Purpose: resume matrix-board bring-up on your development-station PC without losing current context.

## Repo and Branch
- Repo: `Phil-CIA/Pinball-Sw-Lamp-Matrix-`
- Branch: `main`
- This handoff was created in the matrix-board repo (correct target).

## Current Hardware/Design State
- You indicated this is the latest matrix-board PCB revision session.
- Local repo currently contains unsaved-in-chat but real working changes in KiCad files (left untouched by this handoff commit).
- Existing repo docs still describe the Rev 1 bring-up flow and are valid as the startup checklist baseline.

## Current Bench Stage
- Stage: **assembled, not powered yet**.
- Immediate objective: safe first power-up and logic validation before wider lamp stress testing.

## First Session Plan on Dev Station
1. Bench prep and stop conditions
   - Prepare current-limited bench supply, DMM, and scope/logic probe.
   - Set hard stop conditions before power: overcurrent, wrong rail, unusual heat/smell.

2. Unpowered checks
   - Visual check orientation/polarity/connectors and solder quality.
   - Continuity/short checks:
     - `18V` to `GND`
     - `5V` to `GND`
     - `3.3V` to `GND`
   - Verify no accidental coupling from lamp rail into logic rails.

3. First power-up (current-limited)
   - Power up conservatively.
   - Verify `5V` rail, then `3.3V` rail.
   - Check for abnormal heating in regulator/protection/driver areas.

4. Firmware/logic smoke test
   - Run current ESP32-C6 bring-up firmware baseline.
   - Confirm boot output and no reset loop.
   - Probe:
     - `SR_SCLK`, `SR_LATCH`, `SR_DATA0`
     - `SW_COL_0..SW_COL_3`

5. Boot safety behavior
   - Confirm `/OE` and `/MR` are safe at startup.
   - Confirm no unintended lamp flash on boot.

6. Staged lamp validation
   - Test one row + one column first.
   - Expand to full matrix scan only after first pass is stable.
   - Watch connector temperature, supply sag, and thermal hotspots.

7. Capture findings immediately
   - Record rail values, current draw, thermal notes, and mapping/polarity findings.
   - Update docs with dated notes before ending the session.

## Use These Files First
- `README.md`
- `docs/PROJECT_STATUS.md`
- `docs/BRINGUP_CHECKLIST.md`
- `docs/NEXT_ITERATION.md`
- `docs/PINMAP.md`
- `firmware/src/main.cpp`

## Suggested First Prompt on Dev Station
"Resuming matrix-board bring-up on assembled unpowered hardware. Use docs/BRINGUP_CHECKLIST.md and docs/NEXT_ITERATION.md as the runbook. Execute staged checks: unpowered continuity, current-limited first power, logic probe of SR_SCLK/SR_LATCH/SR_DATA0 and SW_COL_0..SW_COL_3, then one-row/one-column lamp test before full scan. Log measured values and pass/fail at each phase."

## Important Scope Note
- This commit only adds this handoff document.
- Existing KiCad modifications in `hardware/` were intentionally not included in this commit.
