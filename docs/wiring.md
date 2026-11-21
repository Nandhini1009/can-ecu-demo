# Wiring

Both MCP2515 modules run from **5 V**. Their **SO** pin (data going back to the board) is also 5 V,
so put a voltage divider on it before it reaches the STM32 or ESP32 pin:

```
module SO ---[ 1k ]---+---[ 2.2k ]--- GND
                      |
                      +--> board MISO pin   (about 3.3 V)
```

The board's own signals (SCK, MOSI, CS) are 3.3 V and drive the module inputs fine.
Join all grounds together (STM32, ESP32, both modules, servo supply).

## STM32 Nucleo-F411RE  <->  MCP2515 module #1

| MCP2515 pin | Nucleo pin | Note |
| --- | --- | --- |
| VCC | 5V (CN6) | not 3.3V |
| GND | GND | |
| SCK | PB3 (D3) | SPI1_SCK |
| SI (MOSI) | PB5 (D4) | SPI1_MOSI |
| SO (MISO) | PB4 (D5) | SPI1_MISO, **through the 1k/2.2k divider** |
| CS | PB6 (D10) | plain GPIO output |
| INT | not used | we poll |

Sensors on the Nucleo:

| Part | Wiring |
| --- | --- |
| Pot (10k) | outer pins to 3.3V and GND, middle pin to **PA0** (A0) |
| NTC (10k or 100k) | 3.3V -- 10k resistor -- **PA1** (A1) -- NTC -- GND. A 100k NTC (marked 104) needs the `-DNTC_R_NOMINAL=100000.0f` build flag |

## ESP32 DevKit  <->  MCP2515 module #2

| MCP2515 pin | ESP32 pin | Note |
| --- | --- | --- |
| VCC | VIN (5V) | works when powered by USB |
| GND | GND | |
| SCK | GPIO 18 | |
| SI (MOSI) | GPIO 23 | |
| SO (MISO) | GPIO 19 | **through the 1k/2.2k divider** |
| CS | GPIO 5 | |

Other ESP32 parts:

| Part | Wiring |
| --- | --- |
| OLED 128x64 (I2C, 0x3C) | SDA = GPIO 21, SCL = GPIO 22, VCC 3.3V |
| INA219 (I2C, 0x40) | SDA/SCL on the same two wires, VCC 3.3V; servo **+5V from its own supply goes in VIN+, out VIN- to the servo red wire** |
| Servo signal | GPIO 13 |
| Servo power | separate 5V supply, its GND joined to ESP32 GND |

## The CAN bus between the two modules

```
module #1 CANH ------------------- CANH module #2
module #1 CANL ------------------- CANL module #2
```

- 120 ohm across CANH/CANL at **each end** (many modules have a jumper "J1" for this: close it on both).
- Check with a multimeter, power off: CANH to CANL should read about 60 ohm.
- Both nodes 500 kbit/s, 8 MHz crystal (look at the metal can on each module).


## Servo power (what worked on the bench)
- Servo red -> ESP32 VIN, brown/black -> GND, orange/yellow -> D13 (three separate jumper wires, never the servo plug straight onto the three neighbouring pins).
- The INA219 is optional: if it is not wired, the ESP32 prints `INA219 NOT FOUND`, the OLED shows `Current n/a`, and everything else works.
