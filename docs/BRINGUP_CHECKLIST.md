# Rev 1 Bring-Up Checklist

Use this note when the first ordered boards arrive.

## Current status
- Rev 1 PCB, stencil, and parts were ordered on **2026-04-09**.
- Current project phase is **wait for hardware, then validate on bench**.
- Goal for Rev 1 is **fast validation**, not perfection.

---

## 1) Assembly / visual inspection
- [ ] Check orientation of diodes, TVS parts, polarized caps, connectors, and the ESP32 board/module
- [ ] Inspect `VNQ7E100AJTR` thermal pad soldering and general stencil results
- [ ] Look for solder bridges, tombstones, lifted pads, or missing passives
- [ ] Verify connector labels and pin 1 direction before attaching harnesses

## 2) Pre-power continuity checks
- [ ] Check resistance / continuity between `18V` and `GND`
- [ ] Check resistance / continuity between `5V` and `GND`
- [ ] Check resistance / continuity between `3.3V` and `GND`
- [ ] Confirm fuse / TVS placement matches the schematic

## 3) First power-up
- [ ] Use current limiting if possible
- [ ] Bring up the supply and verify `5V` rail first
- [ ] Verify `3.3V` regulator / logic rail is correct
- [ ] Confirm the ESP32-C6 boots and the bring-up firmware still runs
- [ ] Verify no unexpected heating on regulators, VNQs, or protection parts

## 4) Logic validation
- [ ] Confirm shift-register clock / latch / data activity
- [ ] Confirm `/OE` and `/MR` behave safely at boot
- [ ] Read all four switch-column outputs and compare with expected states
- [ ] Probe key debug test points as needed

## 5) Lamp-path validation
- [ ] Test one lamp row and one lamp column first
- [ ] Confirm the expected row/column energizing pattern
- [ ] Then run a full matrix scan at low risk / supervised conditions
- [ ] Watch connector temperature, trace heating, and supply sag during testing

## 6) Capture results
- [ ] Record any polarity, routing, thermal, or noise surprises
- [ ] Note which DNP / tuning parts should become default-fit later
- [ ] Update `docs/NEXT_ITERATION.md` and `docs/PROJECT_STATUS.md` with findings
- [ ] Open a Rev 2 checklist if hardware testing shows needed changes
