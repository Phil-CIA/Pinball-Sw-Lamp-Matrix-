# Rev 1 Bring-Up Checklist

Use this note when the first ordered boards arrive.

## Current status
- Rev 1 PCB, stencil, and parts were ordered on **2026-04-09**.
- Board was **assembled** and **5V + 3.3V rails confirmed up** on **2026-04-27**.
- Current project phase is **active bring-up** — logic validation and staged lamp testing.
- Goal for Rev 1 is **fast validation**, not perfection.

---

## 1) Assembly / visual inspection
- [x] Check orientation of diodes, TVS parts, polarized caps, connectors, and the ESP32 board/module
- [x] Inspect `VNQ7E100AJTR` thermal pad soldering and general stencil results
- [x] Look for solder bridges, tombstones, lifted pads, or missing passives
- [x] Verify connector labels and pin 1 direction before attaching harnesses

## 2) Pre-power continuity checks
- [x] Check resistance / continuity between `18V` and `GND`
- [x] Check resistance / continuity between `5V` and `GND`
- [x] Check resistance / continuity between `3.3V` and `GND`
- [x] Confirm fuse / TVS placement matches the schematic

## 3) First power-up
- [x] Use current limiting if possible
- [x] Bring up the supply and verify `5V` rail first ✓ **5V confirmed 2026-04-27**
- [x] Verify `3.3V` regulator / logic rail is correct ✓ **3.3V confirmed 2026-04-27**
- [ ] Confirm the ESP32-C6 boots and the bring-up firmware still runs
- [ ] Verify no unexpected heating on regulators, VNQs, or protection parts

## 4) Logic validation
- [ ] Confirm shift-register clock / latch / data activity
- [x] Confirm `/OE` behaves safely at boot (`SR_/OE` on GPIO10, driven low to enable outputs)
- [x] Read all four switch-column outputs and compare with expected states
- [ ] Probe key debug test points as needed
- [x] In serial monitor run switch bring-up mode and verify:
	- `Runtime mode: SWITCH_SCAN ...`
	- `HB ...`
	- `SWITCH_SCAN row=... hits[rX]=...`
	- `SW_EDGE/s: ...`

## 5) Lamp-path validation
- [ ] Confirm lamp tests are explicitly armed first: `ARM ON`
- [ ] Test one lamp row and one lamp column first
- [ ] Confirm the expected row/column energizing pattern
- [ ] Then run a full matrix scan at low risk / supervised conditions
- [ ] Watch connector temperature, trace heating, and supply sag during testing
- [ ] End with safe shutdown command sequence: `ALLOFF` then `ARM OFF`

## 6) Capture results
- [ ] Record any polarity, routing, thermal, or noise surprises
- [ ] Note which DNP / tuning parts should become default-fit later
- [ ] Update `docs/NEXT_ITERATION.md` and `docs/PROJECT_STATUS.md` with findings
- [ ] Open a Rev 2 checklist if hardware testing shows needed changes

## 7) 2026-04-28 checkpoint
- [x] Lamp side hardware path validated enough for controlled attract testing
- [x] Found/adjusted swapped gate resistor placement on 3 of 4 lamp columns during bench debug
- [x] Added low-duty attract chase routine in firmware for brightness/mapping checks
- [x] Added dedicated switch scan mode (`kRuntimeMode = RuntimeMode::SwitchScan`) for row-by-row switch input verification
- [x] Verified firmware prints switch diagnostics (`SWITCH_SCAN`, `SW_EDGE/s`) on live board
- [ ] Use known-good bulbs to isolate possible burnt lamp bulbs from circuit faults
- [ ] Finalize clamp diode part choice at comparator input stage (`BAT54S` style clamp preferred for signal-to-rails)
