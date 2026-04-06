# Schematic Review and Cleanup Notes

Review date: **2026-04-06**

This note captures the current schematic cleanup work that should be finished **before spending time routing the PCB**.

Scope reviewed:
- `hardware/Pinball Matrix board Rev 1.kicad_sch`
- repo documentation and footprint/library setup

> This is a schematic/design review note based on the checked-in KiCad source and repo contents. It should be used as the cleanup guide before layout work continues.

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
