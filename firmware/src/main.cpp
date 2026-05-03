#include <cstdio>
#include <cstdint>
#include <cstring>

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_rom_sys.h"
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
static constexpr uint32_t SWITCH_SCAN_ROW_US = SWITCH_SCAN_ROW_MS * 1000U;
static constexpr uint32_t ROW_BLANK_US = 100;
static constexpr uint32_t ROW_SETTLE_US = 100;
static constexpr uint32_t ROW_OFF_DEADTIME_US = 50;
static constexpr uint32_t DISPLAY_FRAME_MS = 50;
static constexpr uint32_t DISPLAY_HOLD_MS = 1000;
static constexpr uint32_t DISPLAY_SCROLL_REV_MS = 5000;
static constexpr uint32_t LINK_STALL_MS = 500;
static constexpr uint32_t TRACE_MODE_PERIOD_MS = 4000;
static constexpr uint8_t MAX_TRACE_ACTIVITY = 46;
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
  I2cSlaveRegmap = 2,
};

enum class RowPhase : uint8_t
{
  Blank = 0,
  Drive = 1,
  Settle = 2,
  Sample = 3,
  Hold = 4,
};

struct RowScheduler
{
  RowPhase phase;
  uint8_t activeRow;
  uint8_t nextRow;
  uint64_t slotStartUs;
  uint64_t phaseDeadlineUs;
  uint32_t overruns;
};

static constexpr RuntimeMode kRuntimeMode = RuntimeMode::I2cSlaveRegmap;

static constexpr uint8_t CAPTAIN_MATRIX_I2C_ADDRESS = 0x24;
static constexpr uint8_t CAPTAIN_MATRIX_REG_LAMP_BASE = 0x00;
static constexpr uint8_t CAPTAIN_MATRIX_REG_LAMP_END = 0x07;
static constexpr uint8_t CAPTAIN_MATRIX_REG_SWITCH_BASE = 0x40;
static constexpr uint8_t CAPTAIN_MATRIX_REG_SWITCH_END = 0x43;
static constexpr uint8_t CAPTAIN_MATRIX_REG_DIAG_BASE = 0xF0;
static constexpr uint8_t CAPTAIN_MATRIX_REG_DIAG_END = 0xF3;
static constexpr uint8_t CAPTAIN_MATRIX_CMD_SYSTEM_SETUP = 0x20;
static constexpr uint8_t CAPTAIN_MATRIX_CMD_SYSTEM_ENABLE = 0x01;
static constexpr uint8_t CAPTAIN_MATRIX_CMD_OUTPUT_SETUP = 0x80;
static constexpr uint8_t CAPTAIN_MATRIX_CMD_OUTPUT_ENABLE = 0x01;
static constexpr uint8_t CAPTAIN_MATRIX_CMD_PULSE_WIDTH_BASE = 0xE0;
static constexpr uint8_t CAPTAIN_MATRIX_CMD_PULSE_WIDTH_MASK = 0x0F;
static constexpr uint8_t CAPTAIN_MATRIX_DEFAULT_PULSE_WIDTH_LEVEL = 4;
static constexpr uint8_t CAPTAIN_MATRIX_DIAG_FLAG_SYSTEM_ENABLED = 0x01;
static constexpr uint8_t CAPTAIN_MATRIX_DIAG_FLAG_OUTPUT_ENABLED = 0x02;
static constexpr size_t CAPTAIN_SWITCH_BYTES = 4;
static constexpr size_t CAPTAIN_LAMP_BYTES = 8;
static constexpr uint8_t CAPTAIN_LAMP_COLS = 5;
static constexpr i2c_port_t CAPTAIN_SLAVE_PORT = I2C_NUM_0;
static constexpr int CAPTAIN_SLAVE_RX_BUF = 128;
static constexpr int CAPTAIN_SLAVE_TX_BUF = 128;
static constexpr int OLED_SW_I2C_DELAY_US = 4;

struct MatrixSlaveRuntime
{
  uint8_t lampRows[CAPTAIN_LAMP_BYTES];
  uint8_t switchBytes[CAPTAIN_SWITCH_BYTES];
  uint8_t readPointer;
  uint8_t pulseWidthLevel;
  bool systemEnabled;
  bool outputEnabled;
  bool linkSeen;
  uint32_t rxPackets;
  uint32_t txWindows;
  uint32_t badWrites;
  uint32_t ignoredWrites;
};

static MatrixSlaveRuntime s_matrixSlave = {
    {},
    {},
    CAPTAIN_MATRIX_REG_SWITCH_BASE,
    CAPTAIN_MATRIX_DEFAULT_PULSE_WIDTH_LEVEL,
    false,
    false,
    false,
    0,
    0,
    0,
    0};

static bool is_lamp_register(uint8_t reg)
{
  return (reg >= CAPTAIN_MATRIX_REG_LAMP_BASE) && (reg <= CAPTAIN_MATRIX_REG_LAMP_END);
}

static bool is_switch_register(uint8_t reg)
{
  return (reg >= CAPTAIN_MATRIX_REG_SWITCH_BASE) && (reg <= CAPTAIN_MATRIX_REG_SWITCH_END);
}

static bool is_diag_register(uint8_t reg)
{
  return (reg >= CAPTAIN_MATRIX_REG_DIAG_BASE) && (reg <= CAPTAIN_MATRIX_REG_DIAG_END);
}

enum class LinkState : uint8_t
{
  Wait = 0,
  Live = 1,
  Degraded = 2,
};

static LinkState current_link_state(bool linkDegraded)
{
  if (s_matrixSlave.rxPackets == 0)
  {
    return LinkState::Wait;
  }
  if (linkDegraded)
  {
    return LinkState::Degraded;
  }
  return LinkState::Live;
}

static const char* link_state_name(LinkState state)
{
  switch (state)
  {
    case LinkState::Wait:
      return "WAIT";
    case LinkState::Degraded:
      return "DEGRADED";
    case LinkState::Live:
    default:
      return "LIVE";
  }
}

static uint8_t matrix_diag_value(uint8_t reg)
{
  switch (reg)
  {
    case 0xF0:
    {
      uint8_t flags = 0;
      if (s_matrixSlave.systemEnabled)
      {
        flags |= CAPTAIN_MATRIX_DIAG_FLAG_SYSTEM_ENABLED;
      }
      if (s_matrixSlave.outputEnabled)
      {
        flags |= CAPTAIN_MATRIX_DIAG_FLAG_OUTPUT_ENABLED;
      }
      return flags;
    }
    case 0xF1:
      return static_cast<uint8_t>(s_matrixSlave.pulseWidthLevel & CAPTAIN_MATRIX_CMD_PULSE_WIDTH_MASK);
    case 0xF2:
      return static_cast<uint8_t>(s_matrixSlave.rxPackets & 0xFFU);
    case 0xF3:
      return static_cast<uint8_t>(s_matrixSlave.txWindows & 0xFFU);
    default:
      return 0;
  }
}

static void matrix_prepare_tx_window(uint8_t startReg)
{
  uint8_t txBuf[CAPTAIN_LAMP_BYTES] = {};
  size_t txLen = 1;
  if (is_switch_register(startReg))
  {
    const uint8_t offset = static_cast<uint8_t>(startReg - CAPTAIN_MATRIX_REG_SWITCH_BASE);
    txLen = CAPTAIN_SWITCH_BYTES - offset;
    for (size_t i = 0; i < txLen; ++i)
    {
      txBuf[i] = s_matrixSlave.switchBytes[offset + i];
    }
    s_matrixSlave.readPointer = CAPTAIN_MATRIX_REG_SWITCH_END;
  }
  else if (is_lamp_register(startReg))
  {
    const uint8_t offset = static_cast<uint8_t>(startReg - CAPTAIN_MATRIX_REG_LAMP_BASE);
    txLen = CAPTAIN_LAMP_BYTES - offset;
    for (size_t i = 0; i < txLen; ++i)
    {
      txBuf[i] = s_matrixSlave.lampRows[offset + i];
    }
    s_matrixSlave.readPointer = CAPTAIN_MATRIX_REG_LAMP_END;
  }
  else if (is_diag_register(startReg))
  {
    uint8_t diag[4] = {
        matrix_diag_value(0xF0),
        matrix_diag_value(0xF1),
        matrix_diag_value(0xF2),
        matrix_diag_value(0xF3)};
    const uint8_t offset = static_cast<uint8_t>(startReg - CAPTAIN_MATRIX_REG_DIAG_BASE);
    txLen = sizeof(diag) - offset;
    for (size_t i = 0; i < txLen; ++i)
    {
      txBuf[i] = diag[offset + i];
    }
    s_matrixSlave.readPointer = CAPTAIN_MATRIX_REG_DIAG_END;
  }
  else
  {
    txBuf[0] = 0;
    txLen = 1;
  }

  i2c_reset_tx_fifo(CAPTAIN_SLAVE_PORT);
  const int queued = i2c_slave_write_buffer(CAPTAIN_SLAVE_PORT, txBuf, txLen, 0);
  if (queued > 0)
  {
    s_matrixSlave.txWindows++;
  }
}

static void matrix_handle_command(uint8_t command)
{
  if ((command & 0xF0U) == CAPTAIN_MATRIX_CMD_PULSE_WIDTH_BASE)
  {
    s_matrixSlave.pulseWidthLevel = static_cast<uint8_t>(command & CAPTAIN_MATRIX_CMD_PULSE_WIDTH_MASK);
    return;
  }

  if ((command & 0xFEU) == CAPTAIN_MATRIX_CMD_SYSTEM_SETUP)
  {
    s_matrixSlave.systemEnabled = ((command & CAPTAIN_MATRIX_CMD_SYSTEM_ENABLE) != 0U);
    return;
  }

  if ((command & 0xFEU) == CAPTAIN_MATRIX_CMD_OUTPUT_SETUP)
  {
    s_matrixSlave.outputEnabled = ((command & CAPTAIN_MATRIX_CMD_OUTPUT_ENABLE) != 0U);
  }
}

static void matrix_handle_write_packet(const uint8_t* packet, size_t length)
{
  if ((packet == nullptr) || (length == 0))
  {
    return;
  }

  s_matrixSlave.linkSeen = true;
  s_matrixSlave.rxPackets++;

  const uint8_t first = packet[0];
  if ((length == 1) && (((first & 0xFEU) == CAPTAIN_MATRIX_CMD_SYSTEM_SETUP) ||
                        ((first & 0xFEU) == CAPTAIN_MATRIX_CMD_OUTPUT_SETUP) ||
                        (first & 0xF0U) == CAPTAIN_MATRIX_CMD_PULSE_WIDTH_BASE))
  {
    matrix_handle_command(first);
    matrix_prepare_tx_window(s_matrixSlave.readPointer);
    return;
  }

  s_matrixSlave.readPointer = first;
  if (length == 1)
  {
    matrix_prepare_tx_window(s_matrixSlave.readPointer);
    return;
  }

  if (!is_lamp_register(first))
  {
    s_matrixSlave.ignoredWrites += static_cast<uint32_t>(length - 1U);
    matrix_prepare_tx_window(s_matrixSlave.readPointer);
    return;
  }

  for (size_t idx = 1; idx < length; ++idx)
  {
    const uint8_t reg = static_cast<uint8_t>(first + (idx - 1));
    const uint8_t value = packet[idx];
    if (is_lamp_register(reg))
    {
      s_matrixSlave.lampRows[reg - CAPTAIN_MATRIX_REG_LAMP_BASE] = static_cast<uint8_t>(value & 0x1FU);
    }
    else if (is_switch_register(reg) || is_diag_register(reg))
    {
      s_matrixSlave.ignoredWrites++;
    }
    else
    {
      s_matrixSlave.ignoredWrites++;
    }
  }

  matrix_prepare_tx_window(s_matrixSlave.readPointer);
}

static void matrix_pack_switch_bytes(const bool swState[SR_ROW_COUNT][SR_COL_COUNT])
{
  for (uint8_t col = 0; col < SR_COL_COUNT; ++col)
  {
    uint8_t byteValue = 0;
    for (uint8_t row = 0; row < SR_ROW_COUNT; ++row)
    {
      if (swState[row][col])
      {
        byteValue = static_cast<uint8_t>(byteValue | static_cast<uint8_t>(1U << row));
      }
    }
    s_matrixSlave.switchBytes[col] = byteValue;
  }
}

static void matrix_print_buffer_view(void)
{
  static bool hasPrev = false;
  static uint8_t prevRows[CAPTAIN_LAMP_BYTES] = {};

  std::printf("VIEW sys=%u out=%u pulse=%u reg=0x%02X rx=%lu tx=%lu badW=%lu ignW=%lu\n",
              static_cast<unsigned>(s_matrixSlave.systemEnabled ? 1 : 0),
              static_cast<unsigned>(s_matrixSlave.outputEnabled ? 1 : 0),
              static_cast<unsigned>(s_matrixSlave.pulseWidthLevel),
              static_cast<unsigned>(s_matrixSlave.readPointer),
              static_cast<unsigned long>(s_matrixSlave.rxPackets),
              static_cast<unsigned long>(s_matrixSlave.txWindows),
              static_cast<unsigned long>(s_matrixSlave.badWrites),
              static_cast<unsigned long>(s_matrixSlave.ignoredWrites));
  std::printf("      C0 C1 C2 C3 C4   HEX  D\n");
  for (uint8_t row = 0; row < CAPTAIN_LAMP_BYTES; ++row)
  {
    const uint8_t rowMask = s_matrixSlave.lampRows[row];
    const bool changed = hasPrev && (rowMask != prevRows[row]);
    std::printf("R%u |  %c  %c  %c  %c  %c   0x%02X  %c\n",
                static_cast<unsigned>(row),
                (rowMask & (1U << 0)) ? 'X' : '.',
                (rowMask & (1U << 1)) ? 'X' : '.',
                (rowMask & (1U << 2)) ? 'X' : '.',
                (rowMask & (1U << 3)) ? 'X' : '.',
                (rowMask & (1U << 4)) ? 'X' : '.',
                static_cast<unsigned>(rowMask),
                changed ? '!' : '.');
    prevRows[row] = rowMask;
  }
  hasPrev = true;
}

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

  static void fb_clear(void);
static void fb_set_pixel(int x, int y, bool on);

static inline void oled_sw_i2c_delay(void)
{
  esp_rom_delay_us(OLED_SW_I2C_DELAY_US);
}

static inline void oled_sw_sda(int level)
{
  gpio_set_level(PIN_OLED_I2C_SDA, level);
}

static inline void oled_sw_scl(int level)
{
  gpio_set_level(PIN_OLED_I2C_SCL, level);
}

static void oled_sw_i2c_start(void)
{
  oled_sw_sda(1);
  oled_sw_scl(1);
  oled_sw_i2c_delay();
  oled_sw_sda(0);
  oled_sw_i2c_delay();
  oled_sw_scl(0);
}

static void oled_sw_i2c_stop(void)
{
  oled_sw_sda(0);
  oled_sw_i2c_delay();
  oled_sw_scl(1);
  oled_sw_i2c_delay();
  oled_sw_sda(1);
  oled_sw_i2c_delay();
}

static bool oled_sw_i2c_write_byte(uint8_t value)
{
  for (int bit = 7; bit >= 0; --bit)
  {
    oled_sw_sda((value >> bit) & 0x01U);
    oled_sw_i2c_delay();
    oled_sw_scl(1);
    oled_sw_i2c_delay();
    oled_sw_scl(0);
  }

  oled_sw_sda(1);
  oled_sw_i2c_delay();
  oled_sw_scl(1);
  oled_sw_i2c_delay();
  const bool ack = (gpio_get_level(PIN_OLED_I2C_SDA) == 0);
  oled_sw_scl(0);
  return ack;
}

static esp_err_t ssd1306_sw_write(uint8_t control, const uint8_t* data, size_t len)
{
  oled_sw_i2c_start();
  if (!oled_sw_i2c_write_byte(static_cast<uint8_t>(SSD1306_ADDR_A << 1)))
  {
    oled_sw_i2c_stop();
    return ESP_FAIL;
  }
  if (!oled_sw_i2c_write_byte(control))
  {
    oled_sw_i2c_stop();
    return ESP_FAIL;
  }
  for (size_t i = 0; i < len; ++i)
  {
    if (!oled_sw_i2c_write_byte(data[i]))
    {
      oled_sw_i2c_stop();
      return ESP_FAIL;
    }
  }
  oled_sw_i2c_stop();
  return ESP_OK;
}

static esp_err_t ssd1306_sw_write_cmd(uint8_t cmd)
{
  return ssd1306_sw_write(0x00, &cmd, 1);
}

static esp_err_t ssd1306_sw_set_addr_window(void)
{
  esp_err_t err = ssd1306_sw_write_cmd(0x21);
  if (err != ESP_OK)
  {
    return err;
  }
  err = ssd1306_sw_write_cmd(0x00);
  if (err != ESP_OK)
  {
    return err;
  }
  err = ssd1306_sw_write_cmd(DISPLAY_W - 1);
  if (err != ESP_OK)
  {
    return err;
  }

  err = ssd1306_sw_write_cmd(0x22);
  if (err != ESP_OK)
  {
    return err;
  }
  err = ssd1306_sw_write_cmd(0x00);
  if (err != ESP_OK)
  {
    return err;
  }
  return ssd1306_sw_write_cmd((DISPLAY_H / 8) - 1);
}

static esp_err_t ssd1306_sw_flush(void)
{
  esp_err_t err = ssd1306_sw_set_addr_window();
  if (err != ESP_OK)
  {
    return err;
  }

  for (int page = 0; page < (DISPLAY_H / 8); ++page)
  {
    const uint8_t* src = &s_framebuffer[page * DISPLAY_W];
    err = ssd1306_sw_write(0x40, src, DISPLAY_W);
    if (err != ESP_OK)
    {
      return err;
    }
  }

  return ESP_OK;
}

static esp_err_t ssd1306_sw_init(void)
{
  gpio_config_t odCfg = {};
  odCfg.pin_bit_mask = (1ULL << PIN_OLED_I2C_SDA) | (1ULL << PIN_OLED_I2C_SCL);
  odCfg.mode = GPIO_MODE_OUTPUT_OD;
  odCfg.pull_up_en = GPIO_PULLUP_ENABLE;
  odCfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  odCfg.intr_type = GPIO_INTR_DISABLE;
  const esp_err_t cfgErr = gpio_config(&odCfg);
  if (cfgErr != ESP_OK)
  {
    return cfgErr;
  }

  oled_sw_sda(1);
  oled_sw_scl(1);
  esp_rom_delay_us(1000);

  const uint8_t initSeq[] = {
      0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
      0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12,
      0x81, 0x8F, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6,
      0x2E, 0xAF};

  for (uint8_t cmd : initSeq)
  {
    const esp_err_t err = ssd1306_sw_write_cmd(cmd);
    if (err != ESP_OK)
    {
      return err;
    }
  }

  fb_clear();
  return ssd1306_sw_flush();
}

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

static uint16_t sr_write_image(uint8_t rowByte, uint8_t colByte)
{
  const uint16_t frame = sr_compose_frame(rowByte, colByte);
  sr_shift_frame(frame);
  return frame;
}

static uint8_t matrix_row_col_byte(uint8_t row)
{
  if (kRuntimeMode != RuntimeMode::I2cSlaveRegmap)
  {
    return 0x00;
  }
  if (!s_matrixSlave.systemEnabled || !s_matrixSlave.outputEnabled)
  {
    return 0x00;
  }
  return static_cast<uint8_t>(s_matrixSlave.lampRows[row] & 0x1FU);
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

static void fb_draw_hline(int x0, int x1, int y)
{
  for (int x = x0; x <= x1; ++x)
  {
    fb_set_pixel(x, y, true);
  }
}

static void fb_draw_vline(int x, int y0, int y1)
{
  for (int y = y0; y <= y1; ++y)
  {
    fb_set_pixel(x, y, true);
  }
}

static void fb_draw_rect_outline(int x, int y, int w, int h)
{
  fb_draw_hline(x, x + w - 1, y);
  fb_draw_hline(x, x + w - 1, y + h - 1);
  fb_draw_vline(x, y, y + h - 1);
  fb_draw_vline(x + w - 1, y, y + h - 1);
}

static void fb_fill_rect(int x, int y, int w, int h)
{
  for (int yy = y; yy < (y + h); ++yy)
  {
    for (int xx = x; xx < (x + w); ++xx)
    {
      fb_set_pixel(xx, yy, true);
    }
  }
}

static void fb_draw_matrix_oled_view(bool linkWaiting,
                                     bool linkLive,
                                     bool linkDegraded,
                                     bool traceMode,
                                     uint8_t activityLevel)
{
  static uint8_t prevRows[CAPTAIN_LAMP_BYTES] = {};
  static bool hasPrev = false;
  static uint8_t traceCols[50] = {};

  fb_clear();

  // Banner strip: two state boxes + pulse bar + heartbeat pixels.
  fb_draw_rect_outline(0, 0, DISPLAY_W, 10);
  fb_draw_rect_outline(2, 2, 10, 6);
  fb_draw_rect_outline(14, 2, 10, 6);
  if (s_matrixSlave.systemEnabled)
  {
    fb_fill_rect(3, 3, 8, 4);
  }
  if (s_matrixSlave.outputEnabled)
  {
    fb_fill_rect(15, 3, 8, 4);
  }

  fb_draw_rect_outline(28, 2, 40, 6);
  const int pulseWidth = static_cast<int>((s_matrixSlave.pulseWidthLevel * 38U) / 15U);
  if (pulseWidth > 0)
  {
    fb_fill_rect(29, 3, pulseWidth, 4);
  }

  // Link activity blinkers from rx/tx LSBs.
  fb_set_pixel(74, 5, (s_matrixSlave.rxPackets & 0x01U) != 0U);
  fb_set_pixel(78, 5, (s_matrixSlave.txWindows & 0x01U) != 0U);

  // Link state labels as three boxes: WAIT / LIVE / DEG.
  fb_draw_rect_outline(84, 2, 12, 6);
  fb_draw_rect_outline(98, 2, 12, 6);
  fb_draw_rect_outline(112, 2, 12, 6);
  if (linkWaiting)
  {
    fb_fill_rect(85, 3, 10, 4);
  }
  if (linkLive)
  {
    fb_fill_rect(99, 3, 10, 4);
  }
  if (linkDegraded)
  {
    fb_fill_rect(113, 3, 10, 4);
  }

  // Keep rx/tx bars as secondary activity hints.
  fb_draw_rect_outline(84, 10, 20, 6);
  fb_draw_rect_outline(106, 10, 20, 6);
  const int rxBar = static_cast<int>(s_matrixSlave.rxPackets & 0x0FU);
  const int txBar = static_cast<int>(s_matrixSlave.txWindows & 0x0FU);
  if (rxBar > 0)
  {
    fb_fill_rect(85, 11, rxBar, 4);
  }
  if (txBar > 0)
  {
    fb_fill_rect(107, 11, txBar, 4);
  }

  // 8x5 matrix grid from current lamp RAM.
  const int gridX = 2;
  const int gridY = 14;
  const int cellW = 12;
  const int cellH = 6;
  for (uint8_t row = 0; row < CAPTAIN_LAMP_BYTES; ++row)
  {
    const uint8_t rowMask = s_matrixSlave.lampRows[row];
    const bool rowChanged = hasPrev && (rowMask != prevRows[row]);
    for (uint8_t col = 0; col < CAPTAIN_LAMP_COLS; ++col)
    {
      const int x = gridX + (col * cellW);
      const int y = gridY + (row * cellH);
      fb_draw_rect_outline(x, y, cellW - 1, cellH - 1);
      if ((rowMask & (1U << col)) != 0U)
      {
        fb_fill_rect(x + 2, y + 2, cellW - 5, cellH - 3);
      }
    }
    // Right-side per-row change marker.
    if (rowChanged)
    {
      fb_fill_rect(66, gridY + (row * cellH) + 1, 3, cellH - 2);
    }
    prevRows[row] = rowMask;
  }

  const int paneX = 74;
  const int paneY = 14;
  const int paneW = 52;
  const int paneH = 48;
  fb_draw_rect_outline(paneX, paneY, paneW, paneH);
  if (traceMode)
  {
    const int maxHeight = paneH - 2;
    for (int i = 0; i < (paneW - 3); ++i)
    {
      traceCols[i] = traceCols[i + 1];
    }
    traceCols[paneW - 3] = (activityLevel > maxHeight) ? static_cast<uint8_t>(maxHeight) : activityLevel;
    for (int x = 0; x < (paneW - 2); ++x)
    {
      const int h = traceCols[x];
      for (int y = 0; y < h; ++y)
      {
        fb_set_pixel(paneX + 1 + x, paneY + paneH - 2 - y, true);
      }
    }
  }
  else
  {
    const int sweep = static_cast<int>(s_matrixSlave.txWindows % static_cast<uint32_t>(paneW - 2));
    fb_draw_vline(paneX + 1 + sweep, paneY + 1, paneY + paneH - 2);
  }

  hasPrev = true;
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
  else if (kRuntimeMode == RuntimeMode::I2cSlaveRegmap)
  {
    std::printf("Runtime mode: I2C_SLAVE_REGMAP addr=0x%02X SDA=GPIO2 SCL=GPIO3\n",
                CAPTAIN_MATRIX_I2C_ADDRESS);
  }
  else
  {
    std::printf("Runtime mode: SWITCH_SCAN row_step=%lums sw_active=%s\n",
                static_cast<unsigned long>(SWITCH_SCAN_ROW_MS),
                SW_ACTIVE_LOW ? "LOW" : "HIGH");
  }
  std::printf("OLED I2C map: SDA=GPIO7 SCL=GPIO6 (try SSD1306 0x3C / 0x3D)\n");
  std::printf("CTRL link pins: SDA=GPIO2 SCL=GPIO3\n");

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

  esp_err_t ctrlCfgErr = ESP_OK;
  if (kRuntimeMode != RuntimeMode::I2cSlaveRegmap)
  {
    gpio_config_t ctrlI2cCfg = {};
    ctrlI2cCfg.pin_bit_mask = (1ULL << PIN_CTRL_I2C_SDA) | (1ULL << PIN_CTRL_I2C_SCL);
    ctrlI2cCfg.mode = GPIO_MODE_INPUT;
    ctrlI2cCfg.pull_up_en = GPIO_PULLUP_DISABLE;
    ctrlI2cCfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    ctrlI2cCfg.intr_type = GPIO_INTR_DISABLE;
    ctrlCfgErr = gpio_config(&ctrlI2cCfg);
    std::printf("gpio_config(CTRL I2C monitor) -> %d\n", static_cast<int>(ctrlCfgErr));
  }
  int prevCtrlSda = gpio_get_level(PIN_CTRL_I2C_SDA);
  int prevCtrlScl = gpio_get_level(PIN_CTRL_I2C_SCL);

  esp_err_t busErr = ESP_FAIL;
  i2c_master_bus_handle_t busHandle = nullptr;
  if (kRuntimeMode != RuntimeMode::I2cSlaveRegmap)
  {
    log_i2c_line_levels();

    i2c_master_bus_config_t busCfg = {};
    busCfg.i2c_port = I2C_NUM_0;
    busCfg.sda_io_num = PIN_OLED_I2C_SDA;
    busCfg.scl_io_num = PIN_OLED_I2C_SCL;
    busCfg.clk_source = I2C_CLK_SRC_DEFAULT;
    busCfg.glitch_ignore_cnt = 7;
    busCfg.flags.enable_internal_pullup = true;

    busErr = i2c_new_master_bus(&busCfg, &busHandle);
    std::printf("i2c_new_master_bus -> %d (%s)\n",
                static_cast<int>(busErr),
                esp_err_to_name(busErr));
  }

  if (kRuntimeMode == RuntimeMode::I2cSlaveRegmap)
  {
    i2c_config_t slaveCfg = {};
    slaveCfg.mode = I2C_MODE_SLAVE;
    slaveCfg.sda_io_num = PIN_CTRL_I2C_SDA;
    slaveCfg.sda_pullup_en = GPIO_PULLUP_ENABLE;
    slaveCfg.scl_io_num = PIN_CTRL_I2C_SCL;
    slaveCfg.scl_pullup_en = GPIO_PULLUP_ENABLE;
    slaveCfg.slave.addr_10bit_en = 0;
    slaveCfg.slave.slave_addr = CAPTAIN_MATRIX_I2C_ADDRESS;

    const esp_err_t paramErr = i2c_param_config(CAPTAIN_SLAVE_PORT, &slaveCfg);
    std::printf("i2c_param_config(slave) -> %d (%s)\n",
                static_cast<int>(paramErr),
                esp_err_to_name(paramErr));
    const esp_err_t installErr = (paramErr == ESP_OK)
                                     ? i2c_driver_install(CAPTAIN_SLAVE_PORT,
                                                          I2C_MODE_SLAVE,
                                                          CAPTAIN_SLAVE_RX_BUF,
                                                          CAPTAIN_SLAVE_TX_BUF,
                                                          0)
                                     : paramErr;
    std::printf("i2c_driver_install(slave) -> %d (%s)\n",
                static_cast<int>(installErr),
                esp_err_to_name(installErr));
    if (installErr == ESP_OK)
    {
      matrix_prepare_tx_window(s_matrixSlave.readPointer);
    }
  }

  i2c_master_dev_handle_t oledHandle = nullptr;
  uint8_t oledAddrInUse = 0;
  esp_err_t oledInitErr = ESP_FAIL;
  bool oledSwReady = false;
  static constexpr char kScrollMsg[] = "Matrix boad rev 1";
  const int msgPixelWidth = (static_cast<int>(sizeof(kScrollMsg)) - 1) * 6;
  int lastStartX = 0;
  esp_err_t lastFlushErr = ESP_OK;
  if ((kRuntimeMode != RuntimeMode::I2cSlaveRegmap) && (busErr == ESP_OK))
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
  else if (kRuntimeMode == RuntimeMode::I2cSlaveRegmap)
  {
    oledInitErr = ssd1306_sw_init();
    oledSwReady = (oledInitErr == ESP_OK);
    std::printf("ssd1306_sw_init(GPIO7/6 @0x3C) -> %d (%s)\n",
                static_cast<int>(oledInitErr),
                esp_err_to_name(oledInitErr));
  }

  uint32_t beat = 0;
  uint8_t activeRow = kAttractOrder[0].row;
  uint8_t activeCol = kAttractOrder[0].col;
  uint8_t activeColByte = static_cast<uint8_t>(1U << activeCol);
  uint8_t attractStep = 0;
  uint16_t activeFrame = sr_compose_frame(static_cast<uint8_t>(1U << activeRow),
                                          activeColByte);
  uint32_t ctrlSdaEdges = 0;
  uint32_t ctrlSclEdges = 0;
  uint32_t sw0Edges = 0;
  uint32_t sw1Edges = 0;
  uint32_t sw2Edges = 0;
  uint32_t sw3Edges = 0;
  uint32_t swHits[SR_ROW_COUNT][SR_COL_COUNT] = {};
  bool swState[SR_ROW_COUNT][SR_COL_COUNT] = {};
  LinkStats lastLinkStats = {0, 0, false};
  int prevSw0 = gpio_get_level(PIN_SW_COL0);
  int prevSw1 = gpio_get_level(PIN_SW_COL1);
  int prevSw2 = gpio_get_level(PIN_SW_COL2);
  int prevSw3 = gpio_get_level(PIN_SW_COL3);

  const uint64_t bootUs = static_cast<uint64_t>(esp_timer_get_time());
  const uint64_t bootMs = bootUs / 1000U;
  uint64_t lastHeartbeatMs = 0;
  uint64_t lastDisplayMs = 0;
  uint64_t txStallStartMs = 0;
  uint32_t prevRxPackets = s_matrixSlave.rxPackets;
  uint32_t prevTxWindows = s_matrixSlave.txWindows;
  uint32_t lastDisplayRx = s_matrixSlave.rxPackets;
  uint32_t lastDisplayTx = s_matrixSlave.txWindows;
  bool linkDegraded = false;
  RowScheduler rowScheduler = {
      RowPhase::Blank,
      0,
      0,
      bootUs,
      bootUs + ROW_BLANK_US,
      0};

  if (kRuntimeMode != RuntimeMode::LampAttract)
  {
    activeRow = rowScheduler.activeRow;
    activeCol = 0;
    activeColByte = 0;
    activeFrame = sr_write_image(0x00, 0x00);
  }

  while (true)
  {
    const uint64_t nowUs = static_cast<uint64_t>(esp_timer_get_time());
    const uint64_t nowMs = nowUs / 1000U;

    if (kRuntimeMode != RuntimeMode::I2cSlaveRegmap)
    {
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
    }

    bool rowSampleTick = false;
    uint8_t sampledRow = activeRow;
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
        activeColByte = static_cast<uint8_t>(1U << activeCol);
        const uint8_t rowByte = static_cast<uint8_t>(1U << activeRow);
        activeFrame = sr_compose_frame(rowByte, activeColByte);
      }

      const bool attractLampOn = (inSlotMs < ATTRACT_ON_MS);
      activeFrame = sr_write_image(attractLampOn ? static_cast<uint8_t>(1U << activeRow) : 0x00,
                   attractLampOn ? activeColByte : 0x00);
    }
    else
    {
      while (nowUs >= rowScheduler.phaseDeadlineUs)
      {
        switch (rowScheduler.phase)
        {
          case RowPhase::Blank:
            rowScheduler.phase = RowPhase::Drive;
            continue;
          case RowPhase::Drive:
          {
            activeRow = rowScheduler.nextRow;
            rowScheduler.activeRow = activeRow;
            const uint8_t rowByte = static_cast<uint8_t>(1U << activeRow);
            activeCol = 0;
            activeColByte = 0x00;
            activeFrame = sr_write_image(rowByte, 0x00);
            activeColByte = matrix_row_col_byte(activeRow);
            activeFrame = sr_write_image(rowByte, activeColByte);
            rowScheduler.phase = RowPhase::Settle;
            rowScheduler.phaseDeadlineUs = static_cast<uint64_t>(esp_timer_get_time()) + ROW_SETTLE_US;
            break;
          }
          case RowPhase::Settle:
            rowScheduler.phase = RowPhase::Sample;
            continue;
          case RowPhase::Sample:
            sampledRow = rowScheduler.activeRow;
            rowSampleTick = true;
            rowScheduler.phase = RowPhase::Hold;
            rowScheduler.phaseDeadlineUs = rowScheduler.slotStartUs + SWITCH_SCAN_ROW_US;
            break;
          case RowPhase::Hold:
          {
            const uint8_t rowByte = static_cast<uint8_t>(1U << rowScheduler.activeRow);
            activeColByte = 0x00;
            activeFrame = sr_write_image(rowByte, 0x00);
            if (ROW_OFF_DEADTIME_US > 0U)
            {
              esp_rom_delay_us(ROW_OFF_DEADTIME_US);
            }
            activeFrame = sr_write_image(0x00, 0x00);
            rowScheduler.slotStartUs += SWITCH_SCAN_ROW_US;
            if (nowUs > rowScheduler.slotStartUs)
            {
              rowScheduler.slotStartUs = nowUs;
              rowScheduler.overruns++;
            }
            rowScheduler.nextRow = static_cast<uint8_t>((rowScheduler.activeRow + 1U) % SR_ROW_COUNT);
            activeRow = rowScheduler.nextRow;
            rowScheduler.phase = RowPhase::Blank;
            rowScheduler.phaseDeadlineUs = rowScheduler.slotStartUs + ROW_BLANK_US;
            break;
          }
        }
      }

      attractStep = 0;
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

    if ((kRuntimeMode != RuntimeMode::LampAttract) && rowSampleTick)
    {
      const bool swActive0 = SW_ACTIVE_LOW ? (sw0 == 0) : (sw0 != 0);
      const bool swActive1 = SW_ACTIVE_LOW ? (sw1 == 0) : (sw1 != 0);
      const bool swActive2 = SW_ACTIVE_LOW ? (sw2 == 0) : (sw2 != 0);
      const bool swActive3 = SW_ACTIVE_LOW ? (sw3 == 0) : (sw3 != 0);
      swState[sampledRow][0] = swActive0;
      swState[sampledRow][1] = swActive1;
      swState[sampledRow][2] = swActive2;
      swState[sampledRow][3] = swActive3;
      if (swActive0) ++swHits[sampledRow][0];
      if (swActive1) ++swHits[sampledRow][1];
      if (swActive2) ++swHits[sampledRow][2];
      if (swActive3) ++swHits[sampledRow][3];
      if (kRuntimeMode == RuntimeMode::I2cSlaveRegmap)
      {
        matrix_pack_switch_bytes(swState);
      }
    }

    if (kRuntimeMode == RuntimeMode::I2cSlaveRegmap)
    {
      uint8_t rxPacket[32] = {};
      const int bytesRead = i2c_slave_read_buffer(CAPTAIN_SLAVE_PORT, rxPacket, sizeof(rxPacket), 0);
      if (bytesRead > 0)
      {
        matrix_handle_write_packet(rxPacket, static_cast<size_t>(bytesRead));
      }

      const uint32_t curRx = s_matrixSlave.rxPackets;
      const uint32_t curTx = s_matrixSlave.txWindows;
      const bool rxAdvanced = (curRx != prevRxPackets);
      const bool txAdvanced = (curTx != prevTxWindows);
      if (rxAdvanced && !txAdvanced)
      {
        if (txStallStartMs == 0)
        {
          txStallStartMs = nowMs;
        }
        else if ((nowMs - txStallStartMs) > LINK_STALL_MS)
        {
          linkDegraded = true;
        }
      }
      if (txAdvanced)
      {
        txStallStartMs = 0;
        linkDegraded = false;
      }
      if (curRx == 0)
      {
        linkDegraded = false;
      }
      prevRxPackets = curRx;
      prevTxWindows = curTx;
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
                  static_cast<unsigned>(activeColByte),
                  static_cast<unsigned>(activeFrame));
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
        if (kRuntimeMode == RuntimeMode::I2cSlaveRegmap)
        {
          std::printf("       ROW_SCHED phase=%u active=%u next=%u overruns=%lu blank=%luus settle=%luus off=%luus\n",
                      static_cast<unsigned>(rowScheduler.phase),
                      static_cast<unsigned>(rowScheduler.activeRow),
                      static_cast<unsigned>(rowScheduler.nextRow),
                      static_cast<unsigned long>(rowScheduler.overruns),
                      static_cast<unsigned long>(ROW_BLANK_US),
                      static_cast<unsigned long>(ROW_SETTLE_US),
                      static_cast<unsigned long>(ROW_OFF_DEADTIME_US));
        }
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
      if (kRuntimeMode == RuntimeMode::I2cSlaveRegmap)
      {
        std::printf("       I2C_SLAVE reg=0x%02X sys=%u out=%u pulse=%u rx=%lu tx=%lu badW=%lu ignW=%lu sw=%02X %02X %02X %02X\n",
                    static_cast<unsigned>(s_matrixSlave.readPointer),
                    static_cast<unsigned>(s_matrixSlave.systemEnabled ? 1 : 0),
                    static_cast<unsigned>(s_matrixSlave.outputEnabled ? 1 : 0),
                    static_cast<unsigned>(s_matrixSlave.pulseWidthLevel),
                    static_cast<unsigned long>(s_matrixSlave.rxPackets),
                    static_cast<unsigned long>(s_matrixSlave.txWindows),
              static_cast<unsigned long>(s_matrixSlave.badWrites),
              static_cast<unsigned long>(s_matrixSlave.ignoredWrites),
                    static_cast<unsigned>(s_matrixSlave.switchBytes[0]),
                    static_cast<unsigned>(s_matrixSlave.switchBytes[1]),
                    static_cast<unsigned>(s_matrixSlave.switchBytes[2]),
                    static_cast<unsigned>(s_matrixSlave.switchBytes[3]));
        const LinkState linkState = current_link_state(linkDegraded);
        std::printf("       LINK state=%s\n", link_state_name(linkState));
        if ((beat % 2U) == 0U)
        {
          matrix_print_buffer_view();
        }
      }
      else
      {
        const bool ctrlLinkActive = (ctrlSdaEdges > 0U) && (ctrlSclEdges > 0U);
        lastLinkStats = {ctrlSdaEdges, ctrlSclEdges, ctrlLinkActive};
        std::printf("       CTRL_I2C edges/s: SDA=%lu SCL=%lu link=%s\n",
            static_cast<unsigned long>(ctrlSdaEdges),
            static_cast<unsigned long>(ctrlSclEdges),
            ctrlLinkActive ? "ACTIVE" : "IDLE");
        ctrlSdaEdges = 0;
        ctrlSclEdges = 0;
      }

      if ((kRuntimeMode != RuntimeMode::I2cSlaveRegmap) && (oledInitErr == ESP_OK))
      {
        std::printf("       OLED@0x%02X scroll x=%d err=%d (%s)\n",
                    oledAddrInUse,
                    lastStartX,
                    static_cast<int>(lastFlushErr),
                    esp_err_to_name(lastFlushErr));
      }
      else if (kRuntimeMode != RuntimeMode::I2cSlaveRegmap)
      {
        std::printf("       OLED init failed: %d (%s)\n",
                    static_cast<int>(oledInitErr),
                    esp_err_to_name(oledInitErr));
      }
    }

    if ((kRuntimeMode == RuntimeMode::I2cSlaveRegmap) && oledSwReady)
    {
      if ((nowMs - lastDisplayMs) >= DISPLAY_FRAME_MS)
      {
        lastDisplayMs = nowMs;
        const LinkState linkState = current_link_state(linkDegraded);
        const bool linkWaiting = (linkState == LinkState::Wait);
        const bool linkLive = (linkState == LinkState::Live);
        const bool traceMode = ((nowMs / TRACE_MODE_PERIOD_MS) & 0x1ULL) != 0ULL;
        const uint32_t dispRx = s_matrixSlave.rxPackets;
        const uint32_t dispTx = s_matrixSlave.txWindows;
        const uint32_t delta = (dispRx - lastDisplayRx) + (dispTx - lastDisplayTx);
        lastDisplayRx = dispRx;
        lastDisplayTx = dispTx;
        const uint8_t activityLevel = static_cast<uint8_t>((delta > MAX_TRACE_ACTIVITY) ? MAX_TRACE_ACTIVITY : delta);
        fb_draw_matrix_oled_view(linkWaiting, linkLive, linkDegraded, traceMode, activityLevel);
        lastFlushErr = ssd1306_sw_flush();
      }
    }
    else if ((kRuntimeMode != RuntimeMode::I2cSlaveRegmap) && (oledInitErr == ESP_OK))
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

    if (kRuntimeMode == RuntimeMode::LampAttract)
    {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
    else
    {
      const uint64_t sleepNowUs = static_cast<uint64_t>(esp_timer_get_time());
      if (rowScheduler.phaseDeadlineUs > sleepNowUs)
      {
        const uint64_t remainingUs = rowScheduler.phaseDeadlineUs - sleepNowUs;
        if (remainingUs > 1000U)
        {
          vTaskDelay(pdMS_TO_TICKS(1));
        }
        else if (remainingUs > 25U)
        {
          esp_rom_delay_us(static_cast<uint32_t>(remainingUs - 10U));
        }
      }
    }
  }
}
