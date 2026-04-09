# Pinball Switch + Lamp Matrix Board

8x4 lamp matrix driver and 8x4 switch matrix scanner board for a pinball machine.

## Architecture (current)
- **Lamp rows (8):** high-side smart switches (2× `VNQ7E100AJTR`, 4ch each)
- **Lamp columns (4):** low-side MOSFET sinks
- **Switch columns (4):** `LMV393` comparators (5V powered, OD outputs pulled up to 3.3V)
- **Output expansion:** 2× `74HC595` @ 3.3V for deterministic row/column bit patterns

## Repo layout
- `hardware/` – KiCad project files and production exports
- `firmware/` – PlatformIO / ESP-IDF bring-up firmware
- `docs/` – design notes, project status, review notes, and bring-up plans
- `BOM/` – key part numbers and alternates

## Project references
- `docs/PROJECT_STATUS.md` – current state, locked assumptions, and active priorities
- `docs/NEXT_ITERATION.md` – parking lot for ideas, risks, and restart notes
- `docs/SCHEMATIC_REVIEW.md` – design review and cleanup notes captured during the pre-order review
- `docs/BRINGUP_CHECKLIST.md` – first-board arrival, assembly, and power-up checklist
- `docs/PINMAP.md` – signal naming, connector grouping, and board-mating draft map
- `docs/PCB_REVIEW_CHECKLIST.md` – pre-order hardware checklist
- `docs/GITHUB_REPO_SETUP.md` – suggested GitHub repo metadata and settings
- `BOM/key_parts.md` – important component choices
- `CONTRIBUTING.md` – repo workflow and update expectations
- `SECURITY.md` – reporting guidance for risky or security-related issues
- `CHANGELOG.md` – notable repo changes over time

## Status
Rev 1 hardware was ordered on **2026-04-09**.

- **Hardware:** schematic, PCB snapshots, Gerbers, drill files, BOM, and production ZIP are checked in under `hardware/`
- **Order state:** PCB + stencil ordered, with parts ordered separately
- **Current phase:** wait for hardware and prepare bench bring-up
- **Firmware:** verified ESP32-C6 ESP-IDF bring-up scaffold is present under `firmware/`

## Next Steps
- keep the ordered Rev 1 package and notes as the reference point
- assemble and inspect the first boards when they arrive
- short-check `18V`, `5V`, and `3.3V` before full power-up
- validate ESP32, shift-register outputs, and switch-column reads on bench hardware
- run one-row / one-column lamp tests, then a full matrix scan
- capture any Rev 2 findings after first hardware validation