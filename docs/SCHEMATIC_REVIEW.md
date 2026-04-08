# Schematic Review and Cleanup Notes

Review dates: **2026-04-06** and follow-up **2026-04-07**

This note captures the current schematic cleanup work that should be finished **before spending time routing the PCB**.

Scope reviewed:
- `hardware/Pinball Matrix board Rev 1.kicad_sch`
- repo documentation and footprint/library setup

> This is a schematic/design review note based on the checked-in KiCad source and repo contents. It should be used as the cleanup guide before layout work continues.

---

## Follow-up review status (2026-04-07)
- Fresh repo check showed `git status -sb` clean.
- The currently saved KiCad files in `hardware/` show `LastWriteTime` around **2026-04-06 7:28 AM**, so the repo copy available for review is **not within the last hour**.
- An updated netlist is **not required** for this text/design review, but a saved netlist/ERC export would help with deeper connectivity cross-checking later.
- Because no newer KiCad save was visible in the repo copy, the block-by-block notes below apply to the **current saved schematic version**.

---

## High-priority items before PCB routing

| Priority | Item | Review note / action |
|---|---|---|
| High | `JD1` connector mismatch | `JD1` uses a `Conn_01x24_MountingPin` symbol while the assigned footprint is `Connector_Molex:Molex_KK-396_5273-12A_1x12_P3.96mm_Vertical`. The symbol pin count and footprint pin count must be made consistent. |
| High | `U2` controller footprint mismatch | `U2` is labeled `ESP32-C6-DevKitM-Amazon`, but the assigned footprint is `RF_Module:ESP32-C3-DevKitM-1`. Confirm the actual controller board/module and correct the footprint now. |
| High | `U5` / `U6` comparator package choice | `LMV393` is present, but the final footprint/package choice still needs to be locked down and verified against the intended assembly plan. |
| High | Test-point footprints | `TP6`–`TP15` should be assigned the chosen small SMT test-point footprint (`Keystone 5015` was selected). |
| High | `F1` / `F2` polyfuse footprints | Polyfuse footprints still need to be replaced with a real KiCad footprint instead of placeholder-style text. |
| High | Ground strategy | `GND` and `GNDD` are both used in the schematic. Decide whether they remain split or are tied intentionally with a documented single-point/net-tie strategy. |

## Medium-priority cleanup items

- **Normalize signal naming** to match `docs/PINMAP.md`
  - `S_CLK` → `SR_SCLK`
  - `S_latch` → `SR_LATCH`
  - `S_Data` → `SR_DATA0`
  - `S_Data_1` → `SR_DATA1`
  - `Sw_Col_0...3` → `SW_COL_0...3`
- **Remove duplicated naming styles** such as:
  - `+3.3V` vs `+3V3`
  - `I2C SCL` vs `I²C SCL`
  - `Row 3` vs `Row_3`
- **Confirm connector family callouts**
  - some connector values use `B4B-XH-A` while footprints point to JST EH footprints; verify the actual intended family/pitch
- **Rename MOSFET references consistently**
  - devices using value `IRLR024` should use transistor-style references (`Qx`) instead of inductor-style references if they are MOSFETs
- **Review optional EMI tuning parts**
  - ferrites/snubber parts should be marked and documented as default-fit vs DNP for rev A
- **Re-check bulk capacitor assumptions**
  - verify voltage rating, technology, and effective capacitance on the 18V lamp rail

## Block-by-block design review

### 1) Power entry and protection
- Good signs: PTC fuses, TVS parts, and bulk/decoupling capacitors are present in the schematic.
- Follow-up review on 2026-04-08: moving to physically larger **per-rail TVS parts** is acceptable and can improve layout/rework, as long as the standoff/clamp ratings still match each rail.
- Before routing, confirm real footprints for `F1` / `F2`, verify the bulk-cap voltage margin on the lamp rail, and keep the TVS return path physically short and wide.
- Keep the fuse upstream of the TVS in the final PCB layout.

### 2) Lamp row driver block (`VNQ7E100AJTR`)
- The 8-row high-side architecture still fits the intended lamp-matrix design.
- Confirm EPAD grounding, strap-pin defaults, and any unused-channel handling before layout.
- Keep the row outputs and connector naming aligned with `docs/PINMAP.md`.

### 3) Lamp column sink block
- The low-side sink approach still looks appropriate for the matrix concept.
- Main cleanup item: confirm package/current capability and use consistent transistor-style references for MOSFET parts.
- Verify any gate resistors, pulldowns, and EMI/ringing mitigation parts you want stuffed by default.

### 4) Switch input comparator block (`LMV393` + clamps)
- The comparator-based switch readback approach is still reasonable for the design.
- Follow-up review on 2026-04-08 against the updated source confirms `LMV393` comparators with `BAT54C,215` clamp diodes and the expected `Ref_Comp1` / `Ref_Comp2` reference network.
- I do **not** see a stop-the-design issue here; the block concept looks fine for now.
- Keep the comparator outputs clearly referenced to the 3.3V logic side, and only add hysteresis later if bench testing shows chatter or noise around the threshold.
- No exposed-pad package is needed here; a standard SOIC-8 / TSSOP-8 style comparator package remains the practical choice.

### 5) Shift-register / logic-output block (`74HC595`)
- This block still matches the firmware bring-up concept.
- `/OE` and `/MR` should have explicit boot-safe defaults so the lamps do not glitch during startup.
- Normalize the control-net labels to `SR_SCLK`, `SR_LATCH`, `SR_DATA0`, and `SR_DATA1`.

### 6) Controller / dev-board block (`U2`)
- This remains a top cleanup item because the symbol name and assigned footprint do not yet agree.
- Confirm whether the design is targeting a temporary dev board, a module footprint, or a board-to-board control header.
- If GPIO assignments change, keep `docs/PINMAP.md` and firmware notes synchronized.

### 7) Connectors, test points, and board mating
- Connector families and exact pin counts still need to be frozen before routing.
- `JD1` must be corrected before layout because the symbol and footprint currently disagree.
- Assign the chosen `Keystone 5015` footprint to the debug test points and keep the board-mating notes in `docs/PINMAP.md` up to date.

## Repo / footprint-library cleanup

The schematic currently references external or non-portable footprint/library names such as:
- `PCM_JLCPCB:*`
- `Captain fantastic footprints:*`
- `POWERSSO-16_STM`

### Repo risk
At review time, the repo does **not** contain tracked KiCad footprint libraries (`*.kicad_mod`) or symbol libraries (`*.kicad_sym`). That means footprint resolution depends on the local KiCad environment.

### Recommended fix
Choose one of these approaches before layout gets deeper:
1. **Commit required custom footprints/libraries into the repo** under a dedicated hardware library folder, or
2. **Document the required external libraries clearly** in `hardware/README.md` so the project opens predictably on another machine.

## Suggested cleanup order
1. Lock the exact controller/dev-board footprint for `U2`
2. Fix `JD1` symbol-to-footprint pin-count mismatch
3. Assign/confirm comparator, fuse, and test-point footprints
4. Normalize signal names to match `docs/PINMAP.md`
5. Decide and document `GND` vs `GNDD`
6. Re-run a formal KiCad ERC/footprint check in KiCad before routing continues

## Notes to carry into PCB layout
- Keep the logic/debug connector and lamp-power connector concerns clearly separated
- Keep test points on logic/control nets easy to probe
- Do not assume all custom footprints will resolve on a fresh machine unless the repo includes them or the setup is documented

## Review log
- 2026-04-06: initial schematic cleanup review captured in repo documentation.
- 2026-04-07: follow-up review confirmed the repo copy had no newly saved KiCad files and added a block-by-block design review section.
- 2026-04-08: pulled the latest remote KiCad updates; confirmed the switch-input comparator block and larger rail-TVS direction are conceptually acceptable.
