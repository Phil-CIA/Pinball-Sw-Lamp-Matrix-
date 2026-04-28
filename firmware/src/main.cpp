#include <cstdio>
#include <cstdint>
#include <cstring>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static constexpr gpio_num_t PIN_SW_COL0 = GPIO_NUM_18;
static constexpr gpio_num_t PIN_SW_COL1 = GPIO_NUM_19;
static constexpr gpio_num_t PIN_SW_COL2 = GPIO_NUM_20;
static constexpr gpio_num_t PIN_SW_COL3 = GPIO_NUM_21;

static constexpr gpio_num_t PIN_SR_DATA  = GPIO_NUM_15;
static constexpr gpio_num_t PIN_SR_CLK   = GPIO_NUM_22;
static constexpr gpio_num_t PIN_SR_LATCH = GPIO_NUM_23;
static constexpr gpio_num_t PIN_SR_OE_N  = GPIO_NUM_10;

static constexpr gpio_num_t PIN_OLED_I2C_SDA = GPIO_NUM_7;
static constexpr gpio_num_t PIN_OLED_I2C_SCL = GPIO_NUM_6;
static constexpr gpio_num_t PIN_CTRL_I2C_SDA = GPIO_NUM_2;
static constexpr gpio_num_t PIN_CTRL_I2C_SCL = GPIO_NUM_3;
static constexpr uint8_t SSD1306_ADDR_A = 0x3C;
static constexpr uint8_t SSD1306_ADDR_B = 0x3D;

static constexpr uint32_t HEARTBEAT_MS = 1000;
static constexpr uint32_t ATTRACT_SLOT_MS = 120;
static constexpr uint32_t ATTRACT_ON_MS = 10;
static constexpr uint32_t SWITCH_SCAN_ROW_MS = 5;
static constexpr uint32_t DISPLAY_FRAME_MS = 50;
static constexpr uint32_t DISPLAY_HOLD_MS = 1000;
static constexpr uint32_t DISPLAY_SCROLL_REV_MS = 5000;
static constexpr uint8_t SR_ROW_COUNT = 8;
static constexpr uint8_t SR_COL_COUNT = 4;
static constexpr bool SR_OUTPUT_ACTIVE_LOW = false;
static constexpr bool SW_ACTIVE_LOW = true;
static constexpr int DISPLAY_W = 128;
static constexpr int DISPLAY_H = 64;
static uint8_t s_framebuffer[(DISPLAY_W * DISPLAY_H) / 8] = {};

enum class RuntimeMode : uint8_t
{
  LampAttract = 0,
  SwitchScan = 1,
};

static constexpr RuntimeMode kRuntimeMode = RuntimeMode::SwitchScan;

struct LinkStats
{
  uint32_t sdaEdges;
  uint32_t sclEdges;
  bool active;
};

struct LampPoint
{
  uint8_t row;
  uint8_t col;
};

static constexpr LampPoint kAttractOrder[] = {
    {4, 0}, {1, 0}, {3, 0},
    {4, 2},
    {2, 0}, {0, 0}, {5, 2},
    {2, 2},
    {1, 2},
    {3, 2},
    {0, 2},
    {1, 1}, {0, 1}, {4, 1},
    {2, 1}, {3, 1}, {5, 1},
    {4, 3},
    {1, 3}, {2, 3}, {3, 3}, {0, 3},
    {6, 0}, {7, 3}};

static void fb_set_pixel(int x, int y, bool on);

static void sr_clk_pulse(void)
{
  gpio_set_level(PIN_SR_CLK, 1);
  gpio_set_level(PIN_SR_CLK, 0);
}

static void sr_latch_pulse(void)
{
  gpio_set_level(PIN_SR_LATCH, 1);
  gpio_set_level(PIN_SR_LATCH, 0);
}

static uint16_t sr_compose_frame(uint8_t rowByte, uint8_t colByte)
{
  // Daisy-chain order is U4 (rows) first, then U5 (lamp columns).
  // With MSB-first shifting, the first byte shifted ends in U5, the second in U4.
  // So shift [colByte][rowByte] to place columns on U5 and rows on U4.
  uint16_t frame = (static_cast<uint16_t>(colByte) << 8) | rowByte;
  if (SR_OUTPUT_ACTIVE_LOW)
  {
    frame = static_cast<uint16_t>(~frame);
  }
  return frame;
}

static void sr_shift_frame(uint16_t frame)
{
  for (int bit = 15; bit >= 0; --bit)
  {
    const int bitVal = ((frame >> bit) & 0x1U) ? 1 : 0;
    gpio_set_level(PIN_SR_DATA, bitVal);
    sr_clk_pulse();
  }
  sr_latch_pulse();
}

static void log_i2c_line_levels(void)
{
  gpio_config_t inputCfg = {};
  inputCfg.pin_bit_mask = (1ULL << PIN_OLED_I2C_SDA) | (1ULL << PIN_OLED_I2C_SCL);
  inputCfg.mode = GPIO_MODE_INPUT;
  inputCfg.pull_up_en = GPIO_PULLUP_DISABLE;
  inputCfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  inputCfg.intr_type = GPIO_INTR_DISABLE;
  (void)gpio_config(&inputCfg);

  const int sda = gpio_get_level(PIN_OLED_I2C_SDA);
  const int scl = gpio_get_level(PIN_OLED_I2C_SCL);
  std::printf("OLED I2C idle levels: SDA=%d SCL=%d (expect both high)\n", sda, scl);
}

static void fb_draw_link_indicator(const LinkStats& stats)
{
  const int barLeft = 2;
  const int barTopSda = 16;
  const int barTopScl = 24;
  const int barWidth = 80;

  const int sdaWidth = (stats.sdaEdges > static_cast<uint32_t>(barWidth)) ? barWidth : static_cast<int>(stats.sdaEdges);
  const int sclWidth = (stats.sclEdges > static_cast<uint32_t>(barWidth)) ? barWidth : static_cast<int>(stats.sclEdges);

  for (int x = 0; x < sdaWidth; ++x)
  {
    fb_set_pixel(barLeft + x, barTopSda, true);
    fb_set_pixel(barLeft + x, barTopSda + 1, true);
  }
  for (int x = 0; x < sclWidth; ++x)
  {
    fb_set_pixel(barLeft + x, barTopScl, true);
    fb_set_pixel(barLeft + x, barTopScl + 1, true);
  }

  const int boxX = 104;
  const int boxY = 16;
  const int boxW = 20;
  const int boxH = 12;
  for (int x = boxX; x < (boxX + boxW); ++x)
  {
    fb_set_pixel(x, boxY, true);
    fb_set_pixel(x, boxY + boxH - 1, true);
  }
  for (int y = boxY; y < (boxY + boxH); ++y)
  {
    fb_set_pixel(boxX, y, true);
    fb_set_pixel(boxX + boxW - 1, y, true);
  }
  if (stats.active)
  {
    for (int x = boxX + 2; x < (boxX + boxW - 2); ++x)
    {
      for (int y = boxY + 2; y < (boxY + boxH - 2); ++y)
      {
        fb_set_pixel(x, y, true);
      }
    }
  }
}

static esp_err_t ssd1306_write_cmd(i2c_master_dev_handle_t dev, uint8_t cmd)
{
  const uint8_t payload[2] = {0x00, cmd};
  return i2c_master_transmit(dev, payload, sizeof(payload), 50);
}

static esp_err_t ssd1306_set_addr_window(i2c_master_dev_handle_t dev)
{
  esp_err_t err = ssd1306_write_cmd(dev, 0x21);
  if (err != ESP_OK)
  {
    return err;
  }
  err = ssd1306_write_cmd(dev, 0x00);
  if (err != ESP_OK)
  {
    return err;
  }
  err = ssd1306_write_cmd(dev, DISPLAY_W - 1);
  if (err != ESP_OK)
  {
    return err;
  }

  err = ssd1306_write_cmd(dev, 0x22);
  if (err != ESP_OK)
  {
    return err;
  }
  err = ssd1306_write_cmd(dev, 0x00);
  if (err != ESP_OK)
  {
    return err;
  }
  err = ssd1306_write_cmd(dev, (DISPLAY_H / 8) - 1);
  return err;
}

static esp_err_t ssd1306_flush(i2c_master_dev_handle_t dev)
{
  esp_err_t err = ssd1306_set_addr_window(dev);
  if (err != ESP_OK)
  {
    return err;
  }

  uint8_t payload[1 + DISPLAY_W] = {};
  payload[0] = 0x40;

  for (int page = 0; page < (DISPLAY_H / 8); ++page)
  {
    const uint8_t* src = &s_framebuffer[page * DISPLAY_W];
    std::memcpy(&payload[1], src, DISPLAY_W);
    err = i2c_master_transmit(dev, payload, sizeof(payload), 100);
    if (err != ESP_OK)
    {
      return err;
    }
  }

  return ESP_OK;
}

static void fb_clear(void)
{
  std::memset(s_framebuffer, 0, sizeof(s_framebuffer));
}

static void fb_set_pixel(int x, int y, bool on)
{
  if ((x < 0) || (x >= DISPLAY_W) || (y < 0) || (y >= DISPLAY_H))
  {
    return;
  }

  const int page = y / 8;
  const int bit = y % 8;
  const int idx = (page * DISPLAY_W) + x;
  if (on)
  {
    s_framebuffer[idx] |= static_cast<uint8_t>(1U << bit);
  }
  else
  {
    s_framebuffer[idx] &= static_cast<uint8_t>(~(1U << bit));
  }
}

static const uint8_t* glyph_for_char(char c)
{
  static constexpr uint8_t SPACE[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
  static constexpr uint8_t ONE[5]   = {0x00, 0x42, 0x7F, 0x40, 0x00};
  static constexpr uint8_t M[5]     = {0x7F, 0x02, 0x0C, 0x02, 0x7F};
  static constexpr uint8_t a[5]     = {0x20, 0x54, 0x54, 0x54, 0x78};
  static constexpr uint8_t b[5]     = {0x7F, 0x48, 0x44, 0x44, 0x38};
  static constexpr uint8_t d[5]     = {0x38, 0x44, 0x44, 0x48, 0x7F};
  static constexpr uint8_t e[5]     = {0x38, 0x54, 0x54, 0x54, 0x18};
  static constexpr uint8_t i[5]     = {0x00, 0x44, 0x7D, 0x40, 0x00};
  static constexpr uint8_t o[5]     = {0x38, 0x44, 0x44, 0x44, 0x38};
  static constexpr uint8_t r[5]     = {0x7C, 0x08, 0x04, 0x04, 0x08};
  static constexpr uint8_t t[5]     = {0x04, 0x3F, 0x44, 0x40, 0x20};
  static constexpr uint8_t v[5]     = {0x1C, 0x20, 0x40, 0x20, 0x1C};
  static constexpr uint8_t x[5]     = {0x44, 0x28, 0x10, 0x28, 0x44};

  switch (c)
  {
    case ' ':
      return SPACE;
    case '1':
      return ONE;
    case 'M':
      return M;
    case 'a':
      return a;
    case 'b':
      return b;
    case 'd':
      return d;
    case 'e':
      return e;
    case 'i':
      return i;
    case 'o':
      return o;
    case 'r':
      return r;
    case 't':
      return t;
    case 'v':
      return v;
    case 'x':
      return x;
    default:
      return SPACE;
  }
}

static void fb_draw_char_5x7(int x, int y, char c)
{
  const uint8_t* glyph = glyph_for_char(c);
  for (int col = 0; col < 5; ++col)
  {
    const uint8_t bits = glyph[col];
    for (int row = 0; row < 7; ++row)
    {
      const bool on = ((bits >> row) & 0x01U) != 0U;
      fb_set_pixel(x + col, y + row, on);
    }
  }
}

static void fb_draw_text_5x7(int x, int y, const char* text)
{
  int penX = x;
  for (const char* p = text; *p != '\0'; ++p)
  {
    fb_draw_char_5x7(penX, y, *p);
    penX += 6;
  }
}

static esp_err_t ssd1306_init(i2c_master_dev_handle_t dev)
{
  const uint8_t initSeq[] = {
      0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
      0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12,
      0x81, 0x8F, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6,
      0x2E, 0xAF};

  for (uint8_t cmd : initSeq)
  {
    const esp_err_t err = ssd1306_write_cmd(dev, cmd);
    if (err != ESP_OK)
    {
      return err;
    }
  }

  fb_clear();
  return ssd1306_flush(dev);
}

static esp_err_t try_oled_address(i2c_master_bus_handle_t busHandle,
                                  uint8_t addr,
                                  i2c_master_dev_handle_t* outHandle)
{
  *outHandle = nullptr;

  const esp_err_t probeErr = i2c_master_probe(busHandle, addr, 100);
  std::printf("i2c_master_probe(0x%02X) -> %d (%s)\n",
              addr,
              static_cast<int>(probeErr),
              esp_err_to_name(probeErr));
  if (probeErr != ESP_OK)
  {
    return probeErr;
  }

  i2c_device_config_t devCfg = {};
  devCfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  devCfg.device_address = addr;
  devCfg.scl_speed_hz = 100000;

  const esp_err_t addErr = i2c_master_bus_add_device(busHandle, &devCfg, outHandle);
  std::printf("i2c_master_bus_add_device(0x%02X) -> %d (%s)\n",
              addr,
              static_cast<int>(addErr),
              esp_err_to_name(addErr));
  if (addErr != ESP_OK)
  {
    return addErr;
  }

  const esp_err_t initErr = ssd1306_init(*outHandle);
  std::printf("ssd1306_init(0x%02X) -> %d (%s)\n",
              addr,
              static_cast<int>(initErr),
              esp_err_to_name(initErr));
  if (initErr != ESP_OK)
  {
    i2c_master_bus_rm_device(*outHandle);
    *outHandle = nullptr;
    return initErr;
  }

  return ESP_OK;
}

static bool scan_i2c_bus(i2c_master_bus_handle_t busHandle, uint8_t* firstFoundAddr)
{
  bool foundAny = false;
  uint8_t first = 0;
  std::printf("I2C scan begin (0x08..0x77)\n");
  for (uint8_t addr = 0x08; addr <= 0x77; ++addr)
  {
    const esp_err_t err = i2c_master_probe(busHandle, addr, 30);
    if (err == ESP_OK)
    {
      if (!foundAny)
      {
        first = addr;
      }
      foundAny = true;
      std::printf("  I2C device found at 0x%02X\n", addr);
    }
  }
  if (!foundAny)
  {
    std::printf("I2C scan found no responding devices\n");
  }
  if (firstFoundAddr != nullptr)
  {
    *firstFoundAddr = first;
  }
  return foundAny;
}

extern "C" void app_main(void)
{
  std::printf("\n=== Matrix Rev1 SAFE HEARTBEAT FW ===\n");
  std::printf("Build: %s %s\n", __DATE__, __TIME__);
  std::printf("Mode: logic-only, no lamp drive\n");
  std::printf("SW_COL map: 0=GPIO18 1=GPIO19 2=GPIO20 3=GPIO21\n");
  std::printf("SR map: DATA=GPIO15 CLK=GPIO22 LATCH=GPIO23 OE_N=GPIO10\n");
  std::printf("SR chain: U4=row byte (2nd shifted), U5=col byte (1st shifted)\n");
  if (kRuntimeMode == RuntimeMode::LampAttract)
  {
    std::printf("Runtime mode: LAMP_ATTRACT slot=%lums on=%lums steps=%u\n",
                static_cast<unsigned long>(ATTRACT_SLOT_MS),
                static_cast<unsigned long>(ATTRACT_ON_MS),
                static_cast<unsigned>(sizeof(kAttractOrder) / sizeof(kAttractOrder[0])));
  }
  else
  {
    std::printf("Runtime mode: SWITCH_SCAN row_step=%lums sw_active=%s\n",
                static_cast<unsigned long>(SWITCH_SCAN_ROW_MS),
                SW_ACTIVE_LOW ? "LOW" : "HIGH");
  }
  std::printf("OLED I2C map: SDA=GPIO7 SCL=GPIO6 (try SSD1306 0x3C / 0x3D)\n");
  std::printf("CTRL link monitor: SDA=GPIO2 SCL=GPIO3 (listen-only)\n");

  const uint64_t inputMask =
      (1ULL << PIN_SW_COL0) |
      (1ULL << PIN_SW_COL1) |
      (1ULL << PIN_SW_COL2) |
      (1ULL << PIN_SW_COL3);

  gpio_config_t inputCfg = {};
  inputCfg.pin_bit_mask = inputMask;
  inputCfg.mode = GPIO_MODE_INPUT;
  inputCfg.pull_up_en = GPIO_PULLUP_DISABLE;
  inputCfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  inputCfg.intr_type = GPIO_INTR_DISABLE;

  const esp_err_t cfgErr = gpio_config(&inputCfg);
  std::printf("gpio_config(SW_COL) -> %d\n", static_cast<int>(cfgErr));

  const uint64_t outputMask =
      (1ULL << PIN_SR_DATA) |
      (1ULL << PIN_SR_CLK) |
      (1ULL << PIN_SR_LATCH) |
      (1ULL << PIN_SR_OE_N);
  gpio_config_t outputCfg = {};
  outputCfg.pin_bit_mask = outputMask;
  outputCfg.mode = GPIO_MODE_OUTPUT;
  outputCfg.pull_up_en = GPIO_PULLUP_DISABLE;
  outputCfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  outputCfg.intr_type = GPIO_INTR_DISABLE;

  const esp_err_t outErr = gpio_config(&outputCfg);
  std::printf("gpio_config(SR out) -> %d\n", static_cast<int>(outErr));
  gpio_set_level(PIN_SR_DATA, 0);
  gpio_set_level(PIN_SR_CLK, 0);
  gpio_set_level(PIN_SR_LATCH, 0);
  gpio_set_level(PIN_SR_OE_N, 0);
  std::printf("SR OE_N level -> %d (0 means outputs enabled)\n", gpio_get_level(PIN_SR_OE_N));

  gpio_config_t ctrlI2cCfg = {};
  ctrlI2cCfg.pin_bit_mask = (1ULL << PIN_CTRL_I2C_SDA) | (1ULL << PIN_CTRL_I2C_SCL);
  ctrlI2cCfg.mode = GPIO_MODE_INPUT;
  ctrlI2cCfg.pull_up_en = GPIO_PULLUP_DISABLE;
  ctrlI2cCfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  ctrlI2cCfg.intr_type = GPIO_INTR_DISABLE;
  const esp_err_t ctrlCfgErr = gpio_config(&ctrlI2cCfg);
  std::printf("gpio_config(CTRL I2C monitor) -> %d\n", static_cast<int>(ctrlCfgErr));
  int prevCtrlSda = gpio_get_level(PIN_CTRL_I2C_SDA);
  int prevCtrlScl = gpio_get_level(PIN_CTRL_I2C_SCL);

  log_i2c_line_levels();

  i2c_master_bus_config_t busCfg = {};
  busCfg.i2c_port = I2C_NUM_0;
  busCfg.sda_io_num = PIN_OLED_I2C_SDA;
  busCfg.scl_io_num = PIN_OLED_I2C_SCL;
  busCfg.clk_source = I2C_CLK_SRC_DEFAULT;
  busCfg.glitch_ignore_cnt = 7;
  busCfg.flags.enable_internal_pullup = true;

  i2c_master_bus_handle_t busHandle = nullptr;
  const esp_err_t busErr = i2c_new_master_bus(&busCfg, &busHandle);
  std::printf("i2c_new_master_bus -> %d (%s)\n",
              static_cast<int>(busErr),
              esp_err_to_name(busErr));

  i2c_master_dev_handle_t oledHandle = nullptr;
  uint8_t oledAddrInUse = 0;
  esp_err_t oledInitErr = ESP_FAIL;
  static constexpr char kScrollMsg[] = "Matrix boad rev 1";
  const int msgPixelWidth = (static_cast<int>(sizeof(kScrollMsg)) - 1) * 6;
  int lastStartX = 0;
  esp_err_t lastFlushErr = ESP_OK;
  if (busErr == ESP_OK)
  {
    uint8_t firstI2cAddr = 0;
    const bool foundAny = scan_i2c_bus(busHandle, &firstI2cAddr);

    oledInitErr = try_oled_address(busHandle, SSD1306_ADDR_A, &oledHandle);
    if (oledInitErr == ESP_OK)
    {
      oledAddrInUse = SSD1306_ADDR_A;
    }
    else
    {
      oledInitErr = try_oled_address(busHandle, SSD1306_ADDR_B, &oledHandle);
      if (oledInitErr == ESP_OK)
      {
        oledAddrInUse = SSD1306_ADDR_B;
      }
    }

    if ((oledInitErr != ESP_OK) && foundAny)
    {
      std::printf("Trying first detected I2C address 0x%02X as OLED candidate\n", firstI2cAddr);
      oledInitErr = try_oled_address(busHandle, firstI2cAddr, &oledHandle);
      if (oledInitErr == ESP_OK)
      {
        oledAddrInUse = firstI2cAddr;
      }
    }
  }

  uint32_t beat = 0;
  uint8_t activeRow = kAttractOrder[0].row;
  uint8_t activeCol = kAttractOrder[0].col;
  uint8_t attractStep = 0;
  uint16_t activeFrame = sr_compose_frame(static_cast<uint8_t>(1U << activeRow),
                                          static_cast<uint8_t>(1U << activeCol));
  uint16_t offFrame = sr_compose_frame(0x00, 0x00);
  bool lampPulseOn = false;
  uint32_t ctrlSdaEdges = 0;
  uint32_t ctrlSclEdges = 0;
  uint32_t sw0Edges = 0;
  uint32_t sw1Edges = 0;
  uint32_t sw2Edges = 0;
  uint32_t sw3Edges = 0;
  uint32_t swHits[SR_ROW_COUNT][SR_COL_COUNT] = {};
  LinkStats lastLinkStats = {0, 0, false};
  int prevSw0 = gpio_get_level(PIN_SW_COL0);
  int prevSw1 = gpio_get_level(PIN_SW_COL1);
  int prevSw2 = gpio_get_level(PIN_SW_COL2);
  int prevSw3 = gpio_get_level(PIN_SW_COL3);

  const uint64_t bootMs = static_cast<uint64_t>(esp_timer_get_time() / 1000);
  uint64_t lastHeartbeatMs = 0;
  uint64_t lastDisplayMs = 0;
  uint64_t lastSwitchScanRowMs = 0;

  while (true)
  {
    const uint64_t nowMs = static_cast<uint64_t>(esp_timer_get_time() / 1000);

    const int ctrlSda = gpio_get_level(PIN_CTRL_I2C_SDA);
    const int ctrlScl = gpio_get_level(PIN_CTRL_I2C_SCL);
    if (ctrlSda != prevCtrlSda)
    {
      ++ctrlSdaEdges;
      prevCtrlSda = ctrlSda;
    }
    if (ctrlScl != prevCtrlScl)
    {
      ++ctrlSclEdges;
      prevCtrlScl = ctrlScl;
    }

    bool rowSampleTick = false;
    if (kRuntimeMode == RuntimeMode::LampAttract)
    {
      const uint32_t stepCount = static_cast<uint32_t>(sizeof(kAttractOrder) / sizeof(kAttractOrder[0]));
      const uint64_t slotIndex = (nowMs / ATTRACT_SLOT_MS) % stepCount;
      const uint64_t inSlotMs = nowMs % ATTRACT_SLOT_MS;
      if (slotIndex != attractStep)
      {
        attractStep = static_cast<uint8_t>(slotIndex);
        activeRow = kAttractOrder[attractStep].row;
        activeCol = kAttractOrder[attractStep].col;
        const uint8_t rowByte = static_cast<uint8_t>(1U << activeRow);
        const uint8_t colByte = static_cast<uint8_t>(1U << activeCol);
        activeFrame = sr_compose_frame(rowByte, colByte);
      }

      lampPulseOn = (inSlotMs < ATTRACT_ON_MS);
      sr_shift_frame(lampPulseOn ? activeFrame : offFrame);
    }
    else
    {
      if ((nowMs - lastSwitchScanRowMs) >= SWITCH_SCAN_ROW_MS)
      {
        lastSwitchScanRowMs = nowMs;
        activeRow = static_cast<uint8_t>((activeRow + 1U) % SR_ROW_COUNT);
        rowSampleTick = true;
      }

      activeCol = 0;
      attractStep = 0;
      const uint8_t rowByte = static_cast<uint8_t>(1U << activeRow);
      activeFrame = sr_compose_frame(rowByte, 0x00);
      lampPulseOn = true;
      sr_shift_frame(activeFrame);
    }

    const int sw0 = gpio_get_level(PIN_SW_COL0);
    const int sw1 = gpio_get_level(PIN_SW_COL1);
    const int sw2 = gpio_get_level(PIN_SW_COL2);
    const int sw3 = gpio_get_level(PIN_SW_COL3);

    if (sw0 != prevSw0)
    {
      ++sw0Edges;
      prevSw0 = sw0;
    }
    if (sw1 != prevSw1)
    {
      ++sw1Edges;
      prevSw1 = sw1;
    }
    if (sw2 != prevSw2)
    {
      ++sw2Edges;
      prevSw2 = sw2;
    }
    if (sw3 != prevSw3)
    {
      ++sw3Edges;
      prevSw3 = sw3;
    }

    if ((kRuntimeMode == RuntimeMode::SwitchScan) && rowSampleTick)
    {
      const bool swActive0 = SW_ACTIVE_LOW ? (sw0 == 0) : (sw0 != 0);
      const bool swActive1 = SW_ACTIVE_LOW ? (sw1 == 0) : (sw1 != 0);
      const bool swActive2 = SW_ACTIVE_LOW ? (sw2 == 0) : (sw2 != 0);
      const bool swActive3 = SW_ACTIVE_LOW ? (sw3 == 0) : (sw3 != 0);
      if (swActive0) ++swHits[activeRow][0];
      if (swActive1) ++swHits[activeRow][1];
      if (swActive2) ++swHits[activeRow][2];
      if (swActive3) ++swHits[activeRow][3];
    }

    if ((nowMs - lastHeartbeatMs) >= HEARTBEAT_MS)
    {
      lastHeartbeatMs = nowMs;
      std::printf("HB %lu  SW_COL[0..3]=%d %d %d %d  SR row=%u col=%u frame=0x%04X\n",
                  static_cast<unsigned long>(beat++),
                  sw0,
                  sw1,
                  sw2,
                  sw3,
                  static_cast<unsigned>(activeRow),
                  static_cast<unsigned>(activeCol),
                  static_cast<unsigned>(lampPulseOn ? activeFrame : offFrame));
      if (kRuntimeMode == RuntimeMode::LampAttract)
      {
        std::printf("       ATTRACT step=%u lamp=(r%u,c%u->LD%u) slot=%lums on=%lums\n",
                    static_cast<unsigned>(attractStep),
                    static_cast<unsigned>(activeRow),
                    static_cast<unsigned>(activeCol),
                    static_cast<unsigned>(activeCol + 4U),
                    static_cast<unsigned long>(ATTRACT_SLOT_MS),
                    static_cast<unsigned long>(ATTRACT_ON_MS));
      }
      else
      {
        std::printf("       SWITCH_SCAN row=%u sw_active=%s hits[r%u]=%lu %lu %lu %lu\n",
                    static_cast<unsigned>(activeRow),
                    SW_ACTIVE_LOW ? "LOW" : "HIGH",
                    static_cast<unsigned>(activeRow),
                    static_cast<unsigned long>(swHits[activeRow][0]),
                    static_cast<unsigned long>(swHits[activeRow][1]),
                    static_cast<unsigned long>(swHits[activeRow][2]),
                    static_cast<unsigned long>(swHits[activeRow][3]));
      }
      std::printf("       SW_EDGE/s: %lu %lu %lu %lu\n",
          static_cast<unsigned long>(sw0Edges),
          static_cast<unsigned long>(sw1Edges),
          static_cast<unsigned long>(sw2Edges),
          static_cast<unsigned long>(sw3Edges));
      sw0Edges = 0;
      sw1Edges = 0;
      sw2Edges = 0;
      sw3Edges = 0;
      const bool ctrlLinkActive = (ctrlSdaEdges > 0U) && (ctrlSclEdges > 0U);
      lastLinkStats = {ctrlSdaEdges, ctrlSclEdges, ctrlLinkActive};
      std::printf("       CTRL_I2C edges/s: SDA=%lu SCL=%lu link=%s\n",
          static_cast<unsigned long>(ctrlSdaEdges),
          static_cast<unsigned long>(ctrlSclEdges),
          ctrlLinkActive ? "ACTIVE" : "IDLE");
      ctrlSdaEdges = 0;
      ctrlSclEdges = 0;

      if (oledInitErr == ESP_OK)
      {
        std::printf("       OLED@0x%02X scroll x=%d err=%d (%s)\n",
                    oledAddrInUse,
                    lastStartX,
                    static_cast<int>(lastFlushErr),
                    esp_err_to_name(lastFlushErr));
      }
      else
      {
        std::printf("       OLED init failed: %d (%s)\n",
                    static_cast<int>(oledInitErr),
                    esp_err_to_name(oledInitErr));
      }
    }

    if (oledInitErr == ESP_OK)
    {
      if ((nowMs - lastDisplayMs) >= DISPLAY_FRAME_MS)
      {
        lastDisplayMs = nowMs;

        int startX = 0;
        const uint64_t sinceBootMs = nowMs - bootMs;
        if (sinceBootMs < DISPLAY_HOLD_MS)
        {
          startX = (DISPLAY_W - msgPixelWidth) / 2;
        }
        else
        {
          const uint64_t scrollMs = (sinceBootMs - DISPLAY_HOLD_MS) % DISPLAY_SCROLL_REV_MS;
          const int travel = DISPLAY_W + msgPixelWidth;
          const int offset = static_cast<int>((scrollMs * static_cast<uint64_t>(travel)) / DISPLAY_SCROLL_REV_MS);
          startX = DISPLAY_W - offset;
        }

        fb_clear();
        fb_draw_text_5x7(startX, 0, kScrollMsg);
        fb_draw_link_indicator(lastLinkStats);
        lastFlushErr = ssd1306_flush(oledHandle);
        lastStartX = startX;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
