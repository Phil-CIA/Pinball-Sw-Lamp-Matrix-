# Pinball Switch + Lamp Matrix Board

8x4 lamp matrix driver and 8x4 switch matrix scanner board for a pinball machine.

## Architecture (current)
- **Lamp rows (8):** high-side smart switches (2× VNQ7E100AJTR, 4ch each)
- **Lamp columns (4):** low-side MOSFET sinks
- **Switch columns (4):** LMV393 comparators (5V powered, OD outputs pulled up to 3.3V)
- **Output expansion:** 2× 74HC595 @ 3.3V for deterministic row/column bit patterns

## Repo layout
- `hardware/` – KiCad schematic/PCB and exports
- `firmware/` – PlatformIO (Arduino + ESP-IDF environments)
- `docs/` – design notes and PCB review checklist
- `BOM/` – key part numbers and alternates

## Status
Work-in-progress redesign (started 2026-04).

- **Schematic:** active design work in progress
- **PCB layout:** not created yet; current `.kicad_pcb` file is only a placeholder
- **Firmware:** starter ESP32-C6 bring-up scaffold added

## Next Steps
- finalize the schematic and connector pinout
- assign/verify footprints for all key parts
- create the first real PCB layout revision
- bring up the shift-register and switch-scanner firmware on hardware