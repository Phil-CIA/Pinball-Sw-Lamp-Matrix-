# Contributing

This repository is currently in an early bring-up and design phase. The goal is to keep the project easy to follow while hardware, firmware, and documentation evolve together.

## Working guidelines
- Keep `main` readable and buildable.
- Prefer small commits with clear messages.
- Put one-off experiments on a temporary branch when possible.
- Update docs when design assumptions or pin mappings change.
- Do not commit generated build output from `.pio/` or temporary exports unless they are intentionally tracked.

## For hardware changes
- Keep the schematic as the source of truth.
- Mention connector pin mapping changes clearly in commit messages.
- Run through `docs/PCB_REVIEW_CHECKLIST.md` before ordering a board revision.
- The current `*.kicad_pcb` file is a placeholder until the first real board layout is created.

## For firmware changes
- Verify the PlatformIO build before claiming a bring-up step is complete.
- Note any tested board target or pin assignment updates in `firmware/README.md`.

## Documentation
If you change project direction, also update:
- `README.md`
- `docs/PROJECT_STATUS.md`
- `CHANGELOG.md`
