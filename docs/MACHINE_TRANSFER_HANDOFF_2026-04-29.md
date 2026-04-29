# Machine Transfer Handoff - 2026-04-29

## Snapshot
- Repo: `Pinball-Sw-Lamp-Matrix`
- Branch: `main` (local working tree has uncommitted housekeeping updates)
- Active firmware mode: `RuntimeMode::I2cSlaveRegmap` in `firmware/src/main.cpp`
- Control-link role: matrix board as I2C slave at `0x24` on `GPIO2/GPIO3`
- OLED role: active in slave mode using software I2C on `GPIO7/GPIO6`

## What Was Completed Today
1. Implemented and stabilized matrix I2C slave register map flow.
2. Added diagnostics and cleaned telemetry counters (`badWrites` vs `ignoredWrites`).
3. Re-enabled OLED output while slave mode is active using software I2C.
4. Added OLED display contract v1:
   - top banner status elements
   - 8x5 matrix grid from lamp row buffer
   - right telemetry pane (sweep / trace)
   - link states (`WAIT`, `LIVE`, `DEGRADED`) with TX-stall detection
5. Housekeeping updates made in:
   - `firmware/src/main.cpp`
   - `firmware/README.md`
   - `CHANGELOG.md`

## Current Constraints
- BAT54 switch-path hardware update is pending (user has BAT54S incoming).
- Switch validation is intentionally deferred until BAT54S replacement is complete.
- PlatformIO build still reports flash warning:
  - expected 4MB, detected 2MB
  - currently non-blocking for bring-up workflow

## Verified Runtime Evidence
- Build succeeds from no-space path: `C:\cfhe\Pinball-Sw-Lamp-Matrix\firmware`
- Upload succeeds on COM4.
- Boot log confirms:
  - `Runtime mode: I2C_SLAVE_REGMAP addr=0x24 SDA=GPIO2 SCL=GPIO3`
  - `ssd1306_sw_init(GPIO7/6 @0x3C) -> 0 (ESP_OK)`
- Link telemetry shows active `rx/tx` progression and link state output.

## Next Mainline Steps
1. Keep default mode in `I2cSlaveRegmap` until switch hardware is updated.
2. After BAT54S install:
   - validate physical switch closures against registers `0x40..0x43`
   - confirm no false edges/noise
3. Run control-link acceptance checks:
   - stable `LIVE` state
   - no sustained `DEGRADED`
   - lamp row writes reflected correctly in grid/buffer

## Resume Commands
From PowerShell:

```powershell
Set-Location C:\cfhe\Pinball-Sw-Lamp-Matrix\firmware
C:\Users\user\.platformio\penv\Scripts\platformio.exe run
C:\Users\user\.platformio\penv\Scripts\platformio.exe run --target upload
C:\Users\user\.platformio\penv\Scripts\platformio.exe device monitor --port COM4 --baud 115200
```

## Git Checklist Before Push
If you want to capture this checkpoint as a commit:

```powershell
Set-Location C:\cfhe\Pinball-Sw-Lamp-Matrix
git status --short
git add firmware/src/main.cpp firmware/README.md CHANGELOG.md docs/MACHINE_TRANSFER_HANDOFF_2026-04-29.md
git commit -m "Matrix slave+OLED integration: display v1, link states, and housekeeping"
git push
```
