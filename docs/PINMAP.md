# Pin and Connector Map

This file is the working source of truth for **signal naming**, **connector grouping**, and any future **board-to-board / HAT-style mating interface**.

> Status: draft for iteration planning. The final connector numbers and exact pin order are still **TBD**.

## Signal naming conventions

### Lamp matrix
- `LAMP_ROW_0` ... `LAMP_ROW_7` — high-side lamp row outputs
- `LAMP_COL_0` ... `LAMP_COL_3` — low-side lamp column sinks

### Switch scan
- `SW_COL_0` ... `SW_COL_3` — comparator outputs to the controller
- `SW_ROW_SCAN_0` ... `SW_ROW_SCAN_7` — row drive / scan naming placeholder if used in firmware or schematic notes

### Shift-register control
- `SR_SCLK` — shift clock
- `SR_LATCH` — storage/latch clock
- `SR_DATA0` — primary serial data line
- `SR_DATA1` — optional second serial data line if kept in the design
- `SR_OE_N` — optional active-low output enable for boot-safe control

### Power rails
- `VIN_LAMP` — lamp supply input (design target currently discussed around the machine lamp rail)
- `+5V_SW` — comparator/switch interface supply
- `+3V3_LOGIC` — logic/MCU domain
- `GND` — shared ground reference

---

## Current firmware bring-up GPIO map

These values match the present ESP-IDF bring-up code in `firmware/src/main.cpp`.

| Logical signal | ESP32-C6 GPIO | Direction | Notes |
|---|---:|---|---|
| `SR_SCLK` | `GPIO23` | Output | 74HC595 shift clock |
| `SR_LATCH` | `GPIO24` | Output | 74HC595 latch |
| `SR_DATA0` | `GPIO25` | Output | Main serial data |
| `SR_DATA1` | `GPIO26` | Output | Optional second data line |
| `SW_COL_0` | `GPIO19` | Input | Post-comparator switch column |
| `SW_COL_1` | `GPIO20` | Input | Post-comparator switch column |
| `SW_COL_2` | `GPIO21` | Input | Post-comparator switch column |
| `SW_COL_3` | `GPIO22` | Input | Post-comparator switch column |

## Draft connector grouping

### `J_PWR` — power entry
Working intent:
- `VIN_LAMP`
- `GND`
- optional local power distribution as needed for `+5V_SW` / `+3V3_LOGIC`

### `J_LAMP` — lamp matrix harness
Working signal set:
- `LAMP_ROW_0` ... `LAMP_ROW_7`
- `LAMP_COL_0` ... `LAMP_COL_3`
- return / ground pins as needed by harness strategy

### `J_CTRL` — controller or mating-board interface
Working signal set:
- `SR_SCLK`
- `SR_LATCH`
- `SR_DATA0`
- `SR_DATA1` (optional)
- `SR_OE_N` (recommended spare / future-safe control)
- `SW_COL_0` ... `SW_COL_3`
- `+3V3_LOGIC`
- `GND`

---

## Board-mating / HAT-style interface map

If this board eventually mates with a carrier, controller board, or a HAT-style board, this is the draft logic interface to preserve.

| Interface signal | Direction at matrix board | Voltage domain | Purpose | Status |
|---|---|---|---|---|
| `+3V3_LOGIC` | In | 3.3V | Logic reference / host side logic power | TBD |
| `+5V_SW` | In or local | 5V | Comparator supply domain | TBD |
| `GND` | Both | 0V | Shared reference | Required |
| `SR_SCLK` | In | 3.3V | Shift-register clock from controller | Draft |
| `SR_LATCH` | In | 3.3V | Shift-register latch from controller | Draft |
| `SR_DATA0` | In | 3.3V | Output pattern data | Draft |
| `SR_DATA1` | In | 3.3V | Optional second data path | Optional |
| `SR_OE_N` | In | 3.3V | Safe global output enable | Recommended |
| `SW_COL_0` | Out | 3.3V | Switch column readback | Draft |
| `SW_COL_1` | Out | 3.3V | Switch column readback | Draft |
| `SW_COL_2` | Out | 3.3V | Switch column readback | Draft |
| `SW_COL_3` | Out | 3.3V | Switch column readback | Draft |
| `UART_TX` | Out | 3.3V | Bring-up/debug serial | Recommended |
| `UART_RX` | In | 3.3V | Bring-up/debug serial | Recommended |
| `EN/RESET` | In | 3.3V | Reset or enable control | Optional |

## Board-mating notes
- keep **lamp power/current** off any small logic/HAT connector
- keep the logic/mating connector keyed and clearly mark **pin 1**
- group grounds near fast logic signals to reduce noise issues
- reserve at least one spare control line for future output enable / interrupt use
- document mating orientation early so schematic names and silkscreen stay consistent

## Open items to finalize
- choose the final connector family and pitch
- decide whether `SR_DATA1` remains useful or can be removed
- confirm whether the controller provides `+3V3_LOGIC`, or whether the board generates it locally
- define the exact row/column harness pin order before PCB routing
- confirm whether a HAT-style physical form factor is actually desired or if only a generic board-to-board header is needed

## Revision notes
- 2026-04-06: initial pinmap and board-mating draft added.
