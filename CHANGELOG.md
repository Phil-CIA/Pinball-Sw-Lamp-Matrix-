# Changelog

All notable changes to this project will be documented in this file.

The format follows a simple **Keep a Changelog** style.

## [Unreleased]
### Added
- Structured three-phase bring-up firmware: walking-1 SR test, switch-column poll with change detection, one-row/one-column lamp test, then continuous SW monitor
- `docs/MACHINE_TRANSFER_HANDOFF_2026-04-27.md` added for session resume
- Command-driven bring-up console with explicit lamp arming (`ARM ON|OFF`) and operator-invoked tests (`WALK`, `SWPOLL`, `LAMP`, `ALLOFF`)
- Low-duty attract-style lamp chase sequence for matrix brightness/mapping validation
- Dedicated `RuntimeMode::SwitchScan` firmware path with row-driven scan and per-row switch hit counters
- Switch edge-rate telemetry (`SW_EDGE/s`) and control-link telemetry (`CTRL_I2C edges/s`) included in heartbeat output

### Changed
- `docs/PROJECT_STATUS.md` updated: 5V and 3.3V rails confirmed up; phase is now active bring-up
- `docs/BRINGUP_CHECKLIST.md`: assembly, continuity, and first power-up items marked complete
- `docs/NEXT_ITERATION.md`: active bring-up focus noted; Rev 2 trace-width note added (0805 pads → ~1.5 mm)
- `firmware/src/main.cpp` refactored from simple walking-1 loop into three explicit bring-up phases with labelled console output
- `firmware/src/main.cpp` now defaults to safe idle with outputs off and requires explicit operator command before lamp-path testing
- `firmware/src/main.cpp` now supports mode-selectable bring-up runtime with `SwitchScan` default for switch validation focus
- `firmware/README.md` updated with current validated pin map, runtime modes, and no-space build-path guidance
