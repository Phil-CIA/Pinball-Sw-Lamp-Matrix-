#include <Arduino.h>

// 74HC595 control pins (set to your wiring)
static constexpr int PIN_SCLK  = 23; // shift clock
static constexpr int PIN_LATCH = 24; // storage register latch
static constexpr int PIN_DATA0 = 25; // data to first 595 (rows)
static constexpr int PIN_DATA1 = 26; // optional: if you used a second data line

// Switch column inputs (post-comparator)
static constexpr int PIN_SW_COL0 = 19;
static constexpr int PIN_SW_COL1 = 20;
static constexpr int PIN_SW_COL2 = 21;
static constexpr int PIN_SW_COL3 = 22;

// If you wire /OE to a GPIO, define it here and default-disable outputs until init
// static constexpr int PIN_OE = xx;

static void pulseLatch()
{
  digitalWrite(PIN_LATCH, HIGH);
  delayMicroseconds(1);
  digitalWrite(PIN_LATCH, LOW);
}

static void shiftOut16(uint16_t bits)
{
  // Shifts MSB first. Adjust bit order to match your schematic.
  for (int i = 15; i >= 0; --i)
  {
    digitalWrite(PIN_SCLK, LOW);
    digitalWrite(PIN_DATA0, (bits >> i) & 1);
    digitalWrite(PIN_SCLK, HIGH);
  }
  pulseLatch();
}

static void allOff()
{
  shiftOut16(0x0000);
}

void setup()
{
  Serial.begin(115200);

  pinMode(PIN_SCLK, OUTPUT);
  pinMode(PIN_LATCH, OUTPUT);
  pinMode(PIN_DATA0, OUTPUT);

  digitalWrite(PIN_SCLK, LOW);
  digitalWrite(PIN_LATCH, LOW);
  digitalWrite(PIN_DATA0, LOW);

  pinMode(PIN_SW_COL0, INPUT);
  pinMode(PIN_SW_COL1, INPUT);
  pinMode(PIN_SW_COL2, INPUT);
  pinMode(PIN_SW_COL3, INPUT);

  // Put outputs into a known safe state immediately.
  allOff();

  Serial.println("Pinball matrix firmware skeleton starting...");
}

void loop()
{
  // Example: walk a single bit for basic bring-up
  static uint16_t pattern = 0x0001;
  shiftOut16(pattern);
  pattern = (pattern << 1);
  if (pattern == 0) pattern = 0x0001;

  int sw0 = digitalRead(PIN_SW_COL0);
  int sw1 = digitalRead(PIN_SW_COL1);
  int sw2 = digitalRead(PIN_SW_COL2);
  int sw3 = digitalRead(PIN_SW_COL3);

  Serial.printf("SW_COL: %d %d %d %d\n", sw0, sw1, sw2, sw3);

  delay(200);
}