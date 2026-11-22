// Orion S4 bring-up firmware (STM32F401RET6)
//
// Pin map taken from the schematic:
//   USB CDC       PA11 (DM) / PA12 (DP)   -> "Serial" (virtual COM port)
//   USART6        PC6 TX / PC7 RX         -> "Serial6" (header U5 TX/RX)
//   I2C1          PB8 SCL / PB9 SDA       -> "Wire"    (header U5 SCL/SDA)
//   ADC           PA1..PA4                -> header U4 (ADC1_IN1..4)
//   Digital IO    headers U2, U3, U6
//   SWD           PA13 / PA14, NRST
//
// The board has no user LED, so a heartbeat is output on PB0 (header U3)
// and the USB serial prints status. Commands over USB serial:
//   h = help, a = read ADC, s = I2C scan, t = toggle PB0, u = send "hello" on USART6

#include <Arduino.h>
#include <Wire.h>

static const uint8_t ADC_PINS[4] = {PA1, PA2, PA3, PA4};
static const uint8_t HEARTBEAT_PIN = PB0;

HardwareSerial Serial6(PC7, PC6);  // RX, TX

// 25 MHz HSE -> 84 MHz SYSCLK, 48 MHz for USB
extern "C" void SystemClock_Config(void) {
  RCC_OscInitTypeDef osc = {};
  RCC_ClkInitTypeDef clk = {};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  osc.HSEState = RCC_HSE_ON;
  osc.PLL.PLLState = RCC_PLL_ON;
  osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  osc.PLL.PLLM = 25;   // 25 MHz / 25 = 1 MHz
  osc.PLL.PLLN = 336;  // VCO 336 MHz
  osc.PLL.PLLP = RCC_PLLP_DIV4;  // 84 MHz
  osc.PLL.PLLQ = 7;    // 48 MHz USB
  if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
    while (1) {}
  }

  clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clk.APB1CLKDivider = RCC_HCLK_DIV2;  // 42 MHz
  clk.APB2CLKDivider = RCC_HCLK_DIV1;  // 84 MHz
  if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) {
    while (1) {}
  }
}

static void printHelp() {
  Serial.println(F("Orion S4 ready. Commands:"));
  Serial.println(F("  h  help"));
  Serial.println(F("  a  read ADC (PA1..PA4)"));
  Serial.println(F("  s  I2C scan (PB8 SCL / PB9 SDA)"));
  Serial.println(F("  t  toggle PB0"));
  Serial.println(F("  u  send 'hello' on USART6 (PC6 TX)"));
}

static void readAdc() {
  analogReadResolution(12);
  for (int i = 0; i < 4; i++) {
    uint32_t raw = analogRead(ADC_PINS[i]);
    Serial.printf("ADC%d (PA%d): %lu  (%.3f V)\r\n", i + 1, i + 1,
                  (unsigned long)raw, raw * 3.3f / 4095.0f);
  }
}

static void i2cScan() {
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("I2C device at 0x%02X\r\n", addr);
      found++;
    }
  }
  if (!found) Serial.println(F("No I2C devices found (pull-ups fitted?)"));
}

void setup() {
  pinMode(HEARTBEAT_PIN, OUTPUT);

  Serial.begin(115200);     // USB CDC; baud is ignored
  Serial6.begin(115200);

  Wire.setSDA(PB9);
  Wire.setSCL(PB8);
  Wire.begin();

  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 2000) {}  // wait briefly for a host
  printHelp();
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last >= 500) {
    last = millis();
    digitalToggle(HEARTBEAT_PIN);
  }

  while (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case 'h': printHelp(); break;
      case 'a': readAdc(); break;
      case 's': i2cScan(); break;
      case 't': digitalToggle(HEARTBEAT_PIN); break;
      case 'u': Serial6.println(F("hello")); Serial.println(F("sent")); break;
      default: break;
    }
  }

  // Anything arriving on USART6 is forwarded to the USB console
  while (Serial6.available()) {
    Serial.write(Serial6.read());
  }
}
