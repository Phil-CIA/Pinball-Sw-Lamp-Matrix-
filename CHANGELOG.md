# Changelog

All notable changes to this project will be documented in this file.

The format follows a simple **Keep a Changelog** style.

## [Unreleased]
### Added
- Structured three-phase bring-up firmware: walking-1 SR test, switch-column poll with change detection, one-row/one-column lamp test, then continuous SW monitor
- `docs/MACHINE_TRANSFER_HANDOFF_2026-04-27.md` added for session resume

### Changed
- `docs/PROJECT_STATUS.md` updated: 5V and 3.3V rails confirmed up; phase is now active bring-up
- `docs/BRINGUP_CHECKLIST.md`: assembly, continuity, and first power-up items marked complete
- `docs/NEXT_ITERATION.md`: active bring-up focus noted; Rev 2 trace-width note added (0805 pads → ~1.5 mm)
- `firmware/src/main.cpp` refactored from simple walking-1 loop into three explicit bring-up phases with labelled console output
