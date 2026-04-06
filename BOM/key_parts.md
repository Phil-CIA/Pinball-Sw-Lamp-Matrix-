# Key Parts / BOM Notes

## Lamp row drivers (high-side)
- ST **VNQ7E100AJTR** (PowerSSO-16) — 4ch smart high-side switch
  - Qty: 2 (8 rows total)

### VNQ strap guidance (from datasheet snippets used in design)
- FaultRST: active low; keep low for auto-restart mode
- Unused connections:
  - SEn, SEL0, SEL1, FaultRST -> GND through 15k
  - CS -> GND through 1k

## Switch input conditioning
- Comparator: **LMV393** (dual), 5V supply, open-collector outputs
  - Qty: 2 (4 channels)
- Clamp diodes: **BAT54** dual Schottky (SOT-23) or equivalent
  - Qty: 4 (one per switch column)

## Shift registers (output expansion)
- **74HC595** @ 3.3V (or use 74HCT595 if powering at 5V)
  - Qty: 2

## Row output EMI parts (tuning)
- Ferrite bead (0805):
  - Murata **BLM21PG601SN1D** (600Ω @ 100MHz)
- Snubber (DNP by default):
  - R: 100Ω (footprint provided)
  - C: 1nF (footprint provided)

## Power entry
- TVS: (project-selected part)
- Fuse/PTC: current design target ~1.1A–1.85A hold equivalent (confirm after measurements)