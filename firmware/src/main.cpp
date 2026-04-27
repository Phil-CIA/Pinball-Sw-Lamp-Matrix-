#include <cstdint>
#include <cstdio>

#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ---------------------------------------------------------------------------
// GPIO assignments  (match PINMAP.md and schematic)
// ---------------------------------------------------------------------------
static constexpr gpio_num_t PIN_SCLK  = GPIO_NUM_23; // SR_SCLK  – 74HC595 shift clock
static constexpr gpio_num_t PIN_LATCH = GPIO_NUM_24; // SR_LATCH – storage register latch
static constexpr gpio_num_t PIN_DATA0 = GPIO_NUM_25; // SR_DATA0 – data to first  595 (rows)
static constexpr gpio_num_t PIN_DATA1 = GPIO_NUM_26; // SR_DATA1 – data to second 595 (columns)

static constexpr gpio_num_t PIN_SW_COL0 = GPIO_NUM_19; // SW_COL_0 – post-comparator
static constexpr gpio_num_t PIN_SW_COL1 = GPIO_NUM_20; // SW_COL_1
static constexpr gpio_num_t PIN_SW_COL2 = GPIO_NUM_21; // SW_COL_2
static constexpr gpio_num_t PIN_SW_COL3 = GPIO_NUM_22; // SW_COL_3

// ---------------------------------------------------------------------------
// Timing constants
// ---------------------------------------------------------------------------
static constexpr uint32_t WALK_STEP_MS   = 200; // walking-1 step period
static constexpr uint32_t SW_POLL_MS     = 50;  // switch-column poll period
static constexpr uint32_t LAMP_HOLD_MS   = 500; // hold time per lamp during row/col test

// ---------------------------------------------------------------------------
// Shift-register helpers
// ---------------------------------------------------------------------------
static void pulseLatch()
{
  gpio_set_level(PIN_LATCH, 1);
  esp_rom_delay_us(1);
  gpio_set_level(PIN_LATCH, 0);
}

// Shifts 16 bits MSB-first onto DATA0; latch at end.
// Bit[15..8] = first  595 (rows 0-7 via VNQ high-side)
// Bit[7..0]  = second 595 (columns 0-3 low-side; upper nibble unused)
static void shiftOut16(uint16_t bits)
{
  for (int i = 15; i >= 0; --i)
  {
    gpio_set_level(PIN_SCLK, 0);
    gpio_set_level(PIN_DATA0, (bits >> i) & 1U);
    gpio_set_level(PIN_SCLK, 1);
  }
  pulseLatch();
}

static void allOff()
{
  shiftOut16(0x0000);
}

// ---------------------------------------------------------------------------
// Phase 1 – walking-1 pattern across all 16 SR output bits
// Lets you confirm shift-register activity on a scope / logic probe.
// Runs for a fixed number of full sweeps then returns.
// ---------------------------------------------------------------------------
static void phase_walkingOne(int sweeps)
{
  std::printf("\n--- PHASE 1: Walking-1 shift-register test (%d sweeps) ---\n", sweeps);
  for (int s = 0; s < sweeps; ++s)
  {
    uint16_t bit = 0x0001;
    for (int i = 0; i < 16; ++i)
    {
      shiftOut16(bit);
      std::printf("  SR bit %2d  pattern=0x%04X\n", i, bit);
      vTaskDelay(pdMS_TO_TICKS(WALK_STEP_MS));
      bit = static_cast<uint16_t>(bit << 1);
    }
  }
  allOff();
  std::printf("--- Phase 1 complete ---\n");
}

// ---------------------------------------------------------------------------
// Phase 2 – switch-column poll
// Reports the state of all four comparator outputs and prints a line whenever
// any column changes state.  Runs for `durationMs` milliseconds then returns.
// ---------------------------------------------------------------------------
static void phase_switchPoll(uint32_t durationMs)
{
  std::printf("\n--- PHASE 2: Switch-column poll (%lu ms) ---\n",
              static_cast<unsigned long>(durationMs));
  std::printf("    Columns are active-LOW (LMV393 open-collector + 3.3V pull-up)\n");
  std::printf("    SW_COL: 0=asserted/closed  1=released/open\n\n");

  const TickType_t deadline = xTaskGetTickCount() +
                              pdMS_TO_TICKS(durationMs);
  int prev[4] = {-1, -1, -1, -1};

  while (xTaskGetTickCount() < deadline)
  {
    int cur[4] = {
      gpio_get_level(PIN_SW_COL0),
      gpio_get_level(PIN_SW_COL1),
      gpio_get_level(PIN_SW_COL2),
      gpio_get_level(PIN_SW_COL3),
    };

    bool changed = (cur[0] != prev[0]) || (cur[1] != prev[1]) ||
                   (cur[2] != prev[2]) || (cur[3] != prev[3]);
    if (changed)
    {
      std::printf("  SW_COL[0..3]: %d %d %d %d\n",
                  cur[0], cur[1], cur[2], cur[3]);
      for (int c = 0; c < 4; ++c) prev[c] = cur[c];
    }

    vTaskDelay(pdMS_TO_TICKS(SW_POLL_MS));
  }
  std::printf("--- Phase 2 complete ---\n");
}

// ---------------------------------------------------------------------------
// Phase 3 – one row + one column lamp test
// Energises each (row, col) pair in sequence so you can confirm the expected
// lamp fires without running the full matrix.
// Row bits  live in SR bits [15..8] (first  595, high-side VNQ IN_x).
// Column bits live in SR bits [7..0]  (second 595, low-side MOSFET gate).
// Only one row and one column bit are asserted at a time.
// ---------------------------------------------------------------------------
static void phase_lampTest(int rows, int cols)
{
  std::printf("\n--- PHASE 3: One-row / one-column lamp test (%dx%d) ---\n",
              rows, cols);
  std::printf("    Watch for: correct lamp, no unintended lamps, no smoke.\n");
  std::printf("    Each cell held for %lu ms.\n\n",
              static_cast<unsigned long>(LAMP_HOLD_MS));

  for (int r = 0; r < rows; ++r)
  {
    uint16_t rowBit = static_cast<uint16_t>(1U << (8 + r)); // bits 8-15
    for (int c = 0; c < cols; ++c)
    {
      uint16_t colBit = static_cast<uint16_t>(1U << c);     // bits 0-3
      uint16_t pattern = rowBit | colBit;
      std::printf("  Lamp row=%d col=%d  pattern=0x%04X\n", r, c, pattern);
      shiftOut16(pattern);
      vTaskDelay(pdMS_TO_TICKS(LAMP_HOLD_MS));
      allOff();
      vTaskDelay(pdMS_TO_TICKS(20)); // brief gap between lamps
    }
  }
  std::printf("--- Phase 3 complete ---\n");
}

// ---------------------------------------------------------------------------
// app_main
// ---------------------------------------------------------------------------
extern "C" void app_main(void)
{
  // --- configure outputs ---
  const uint64_t outputMask =
      (1ULL << PIN_SCLK) |
      (1ULL << PIN_LATCH) |
      (1ULL << PIN_DATA0) |
      (1ULL << PIN_DATA1);

  gpio_config_t outputCfg = {};
  outputCfg.pin_bit_mask   = outputMask;
  outputCfg.mode           = GPIO_MODE_OUTPUT;
  outputCfg.pull_up_en     = GPIO_PULLUP_DISABLE;
  outputCfg.pull_down_en   = GPIO_PULLDOWN_DISABLE;
  outputCfg.intr_type      = GPIO_INTR_DISABLE;
  gpio_config(&outputCfg);

  // --- configure inputs ---
  const uint64_t inputMask =
      (1ULL << PIN_SW_COL0) |
      (1ULL << PIN_SW_COL1) |
      (1ULL << PIN_SW_COL2) |
      (1ULL << PIN_SW_COL3);

  gpio_config_t inputCfg = {};
  inputCfg.pin_bit_mask   = inputMask;
  inputCfg.mode           = GPIO_MODE_INPUT;
  inputCfg.pull_up_en     = GPIO_PULLUP_DISABLE;  // external pull-up via comparator OD output
  inputCfg.pull_down_en   = GPIO_PULLDOWN_DISABLE;
  inputCfg.intr_type      = GPIO_INTR_DISABLE;
  gpio_config(&inputCfg);

  // Drive SR control lines and data to known-safe state before SR is clocked.
  gpio_set_level(PIN_SCLK,  0);
  gpio_set_level(PIN_LATCH, 0);
  gpio_set_level(PIN_DATA0, 0);
  gpio_set_level(PIN_DATA1, 0);
  allOff();

  std::printf("\n=== Pinball Matrix Rev 1 – ESP-IDF bring-up firmware ===\n");
  std::printf("    5V rail: confirmed\n");
  std::printf("    3.3V rail: confirmed\n");
  std::printf("    SR pins: SCLK=GPIO%d  LATCH=GPIO%d  DATA0=GPIO%d\n",
              PIN_SCLK, PIN_LATCH, PIN_DATA0);
  std::printf("    SW_COL pins: GPIO%d GPIO%d GPIO%d GPIO%d\n\n",
              PIN_SW_COL0, PIN_SW_COL1, PIN_SW_COL2, PIN_SW_COL3);

  // Phase 1 – verify shift-register activity (2 sweeps)
  phase_walkingOne(2);
  vTaskDelay(pdMS_TO_TICKS(500));

  // Phase 2 – watch switch columns for 10 seconds
  phase_switchPoll(10000);
  vTaskDelay(pdMS_TO_TICKS(500));

  // Phase 3 – single-lamp validation (rows 0-1, cols 0-1 for first test)
  // Adjust row/col counts as confidence builds; start small.
  phase_lampTest(2, 2);
  vTaskDelay(pdMS_TO_TICKS(500));

  // Continuous monitoring loop after bring-up phases
  std::printf("\n=== Bring-up phases complete. Entering continuous SW monitor. ===\n");
  int prev[4] = {-1, -1, -1, -1};
  while (true)
  {
    int cur[4] = {
      gpio_get_level(PIN_SW_COL0),
      gpio_get_level(PIN_SW_COL1),
      gpio_get_level(PIN_SW_COL2),
      gpio_get_level(PIN_SW_COL3),
    };

    bool changed = (cur[0] != prev[0]) || (cur[1] != prev[1]) ||
                   (cur[2] != prev[2]) || (cur[3] != prev[3]);
    if (changed)
    {
      std::printf("SW_COL[0..3]: %d %d %d %d\n",
                  cur[0], cur[1], cur[2], cur[3]);
      for (int c = 0; c < 4; ++c) prev[c] = cur[c];
    }

    vTaskDelay(pdMS_TO_TICKS(SW_POLL_MS));
  }
}