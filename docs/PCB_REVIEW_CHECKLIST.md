# PCB Review Checklist (before ordering)

## 1) VNQ7E100AJTR (PowerSSO-16) EPAD plan
- [ ] EPAD is connected to **GND** copper (per datasheet intent).
- [ ] Add **via stitching** under/around EPAD to spread heat to other layers.
- [ ] Consider paste stencil **windowing** (avoid too much paste so part doesn’t float).
- [ ] Keep high-current loops short/wide (VCC -> VNQ -> harness -> return).

## 2) Decoupling placement
### VNQ7E100AJTR
- [ ] 100 nF ceramic placed as close as possible to VCC/GND pins (same side if possible).
- [ ] 1 uF ceramic placed close to the IC as well.
- [ ] Bulk cap near power entry and reasonably near the VNQs.

### LMV393 + 74HC595
- [ ] 100 nF at each IC, close to VCC/GND pins.

## 3) Power entry protection
- [ ] Fuse/PTC is upstream of TVS (TVS should not have to eat sustained faults).
- [ ] TVS return path to ground is **short + wide** (min loop inductance).
- [ ] Bulk electrolytic voltage rating margin OK for 18–22V rail + spikes.

## 4) Grounding / noise control
- [ ] Even if using one GND net, keep **snubber currents** and **lamp return currents**
      from sharing thin traces with comparator reference/divider returns.
- [ ] Keep comparator reference divider node physically quiet (short traces, local return).

## 5) Lamp row EMI tuning options (stuff/DNP)
Per row output at connector:
- [ ] Ferrite bead footprint in series with row line.
- [ ] RC snubber footprint: R (series) + C to GND (DNP by default).
- [ ] Keep these parts close to the connector to damp harness ringing.

## 6) 74HC595 boot safety
- [ ] /MR has a defined pull-up so the register doesn’t randomly clear.
- [ ] /OE default policy decided:
  - safer: /OE pulled HIGH (disabled) and firmware enables after shifting all-off pattern
- [ ] Confirm power-up behavior cannot flash lamps unintentionally.

## 7) Test points
- [ ] Add test points for: 3.3V, 5V, 18/22V, GND, S_CLK, S_LATCH, S_DATA, S_DATA_1
- [ ] Add test points for each switch column output (post-comparator) if possible.