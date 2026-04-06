#include <cstdint>
#include <cstdio>

#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 74HC595 control pins (set to your wiring)
static constexpr gpio_num_t PIN_SCLK  = GPIO_NUM_23; // shift clock
static constexpr gpio_num_t PIN_LATCH = GPIO_NUM_24; // storage register latch
static constexpr gpio_num_t PIN_DATA0 = GPIO_NUM_25; // data to first 595 (rows)
static constexpr gpio_num_t PIN_DATA1 = GPIO_NUM_26; // optional second data line

// Switch column inputs (post-comparator)
static constexpr gpio_num_t PIN_SW_COL0 = GPIO_NUM_19;
static constexpr gpio_num_t PIN_SW_COL1 = GPIO_NUM_20;
static constexpr gpio_num_t PIN_SW_COL2 = GPIO_NUM_21;
static constexpr gpio_num_t PIN_SW_COL3 = GPIO_NUM_22;

static void pulseLatch()
{
  gpio_set_level(PIN_LATCH, 1);
  esp_rom_delay_us(1);
  gpio_set_level(PIN_LATCH, 0);
}

static void shiftOut16(uint16_t bits)
{
  // Shifts MSB first. Adjust bit order to match your schematic.
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

extern "C" void app_main(void)
{
  const uint64_t outputMask =
      (1ULL << PIN_SCLK) |
      (1ULL << PIN_LATCH) |
      (1ULL << PIN_DATA0) |
      (1ULL << PIN_DATA1);

  gpio_config_t outputConfig = {};
  outputConfig.pin_bit_mask = outputMask;
  outputConfig.mode = GPIO_MODE_OUTPUT;
  outputConfig.pull_up_en = GPIO_PULLUP_DISABLE;
  outputConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
  outputConfig.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&outputConfig);

  const uint64_t inputMask =
      (1ULL << PIN_SW_COL0) |
      (1ULL << PIN_SW_COL1) |
      (1ULL << PIN_SW_COL2) |
      (1ULL << PIN_SW_COL3);

  gpio_config_t inputConfig = {};
  inputConfig.pin_bit_mask = inputMask;
  inputConfig.mode = GPIO_MODE_INPUT;
  inputConfig.pull_up_en = GPIO_PULLUP_DISABLE;
  inputConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
  inputConfig.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&inputConfig);

  gpio_set_level(PIN_SCLK, 0);
  gpio_set_level(PIN_LATCH, 0);
  gpio_set_level(PIN_DATA0, 0);
  gpio_set_level(PIN_DATA1, 0);

  // Put outputs into a known safe state immediately.
  allOff();

  std::printf("Pinball matrix ESP-IDF bring-up starting...\n");

  uint16_t pattern = 0x0001;
  while (true)
  {
    shiftOut16(pattern);
    pattern = static_cast<uint16_t>(pattern << 1);
    if (pattern == 0)
    {
      pattern = 0x0001;
    }

    const int sw0 = gpio_get_level(PIN_SW_COL0);
    const int sw1 = gpio_get_level(PIN_SW_COL1);
    const int sw2 = gpio_get_level(PIN_SW_COL2);
    const int sw3 = gpio_get_level(PIN_SW_COL3);

    std::printf("SW_COL: %d %d %d %d\n", sw0, sw1, sw2, sw3);
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}