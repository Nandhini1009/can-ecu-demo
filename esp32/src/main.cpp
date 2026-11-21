/* ESP32: the "actuator + gateway" node.
 *  - receives the ECU frame 0x100 (throttle, temperature) from the STM32 through an MCP2515
 *  - moves the servo to follow the throttle; goes to 0 degrees if the ECU goes silent
 *    (CAN timeout) or reports over-temperature
 *  - measures the servo's supply current with an INA219
 *  - sends frame 0x200 (servo angle, current) back, shows everything on the OLED
 *  - prints one JSON line every 500 ms on USB serial (the dashboard reads this on Day 5)
 */
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_INA219.h>
#include <ESP32Servo.h>

extern "C" {
#include "mcp2515.h"
#include "mcp2515_regs.h"
#include "can_protocol.h"
}

#define MCP2515_MODE   MCP_MODE_NORMAL

#define PIN_CS     5     /* VSPI: SCK 18, MISO 19 (through the divider), MOSI 23 */
#define PIN_SERVO  13
#define SERVO_SAFE_DEG  0

static mcp2515_t    can;
static Adafruit_SSD1306 oled(128, 64, &Wire, -1);
static Adafruit_INA219  ina;
static Servo        servo;
static can_monitor_t ecu_mon;
static ecu_status_t  last_ecu;
static uint8_t       act_counter;
static uint8_t       servo_deg = SERVO_SAFE_DEG;
static uint16_t      current_ma;
static uint32_t      rx_count;
static bool          can_ok, ina_ok, oled_ok;

static void spi_xfer(void *, const uint8_t *tx, uint8_t *rx, size_t len)
{
    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    digitalWrite(PIN_CS, LOW);
    SPI.transferBytes(tx, rx, len);
    digitalWrite(PIN_CS, HIGH);
    SPI.endTransaction();
}

void setup()
{
    Serial.begin(115200);
    pinMode(PIN_CS, OUTPUT);
    digitalWrite(PIN_CS, HIGH);
    SPI.begin();                       /* default VSPI pins */
    Wire.begin();                      /* SDA 21, SCL 22 */

    can.xfer = spi_xfer;
    can.ctx = nullptr;
    can.delay_ms = [](uint32_t ms) { delay(ms); };
    can_monitor_init(&ecu_mon);
    can_ok = (mcp2515_init_500k_8mhz(&can, MCP2515_MODE) == MCP2515_OK);
    Serial.println(can_ok ? "MCP2515 init OK" : "MCP2515 init FAILED (check wiring, 5V and CS)");
    /* debug: CANSTAT 0x80 = stuck in config mode (bus problem); CNF2 0x90 = chip answers over SPI */
    Serial.printf("debug: CANSTAT=0x%02X CNF2=0x%02X\n", mcp2515_read_reg(&can, MCP_CANSTAT), mcp2515_read_reg(&can, MCP_CNF2));

    ina_ok = ina.begin();              /* address 0x40 */
    oled_ok = oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    Serial.printf("INA219 %s, OLED %s\n", ina_ok ? "found" : "NOT FOUND (check SDA/SCL/VCC/GND)", oled_ok ? "found" : "NOT FOUND (check SDA/SCL/VCC/GND)");
    servo.setPeriodHertz(50);
    servo.attach(PIN_SERVO, 500, 2400);
    servo.write(SERVO_SAFE_DEG);

    /* power-on self test: sweep the servo once so you can see it works */
    for (int a = 0; a <= 90; a += 3) { servo.write(a); delay(20); }
    for (int a = 90; a >= 0; a -= 3) { servo.write(a); delay(20); }
    Serial.println("servo self-test done");
}

static void update_screen(bool timed_out, bool overtemp)
{
    if (!oled_ok) return;
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(0, 0);
    oled.printf("Throttle %3u %%\n", last_ecu.throttle_pct);
    oled.printf("Temp  %5.1f C\n", last_ecu.temp_x10 / 10.0f);
    oled.printf("Servo %3u deg\n", servo_deg);
    if (ina_ok) oled.printf("Current %4u mA\n", current_ma);
    else        oled.print("Current n/a\n");
    oled.printf("Lost frames %lu\n", (unsigned long)ecu_mon.lost_frames);
    if (timed_out)      oled.print("ECU SILENT: failsafe");
    else if (overtemp)  oled.print("OVER-TEMP: throttle 0");
    else                oled.print("OK");
    oled.display();
}

void loop()
{
    static uint32_t next_tx, next_screen, next_json;
    uint32_t now = millis();
    static uint32_t last_loop, max_gap;           /* longest pause of this loop since the last print */
    if (last_loop && now - last_loop > max_gap) max_gap = now - last_loop;
    last_loop = now;

    /* 1. receive */
    can_frame_t rx;
    while (can_ok && mcp2515_receive(&can, &rx) == MCP2515_OK) {
        rx_count++;
        ecu_status_t e;
        if (can_decode_ecu_status(&rx, &e)) {
            last_ecu = e;
            can_monitor_on_frame(&ecu_mon, now, e.counter);
        }
    }

    /* 2. decide what the servo does (fail-safe first) */
    bool timed_out = can_monitor_timed_out(&ecu_mon, now);
    bool overtemp  = (last_ecu.flags & ECU_FLAG_OVERTEMP) || !temp_in_range(last_ecu.temp_x10);
    if (MCP2515_MODE != MCP_MODE_NORMAL) timed_out = false;   /* loopback: no ECU to hear */
    servo_deg = (timed_out || overtemp) ? SERVO_SAFE_DEG : throttle_to_servo_deg(last_ecu.throttle_pct);
    servo.write(servo_deg);

    /* 3. measure current, send our status every 100 ms */
    if ((int32_t)(now - next_tx) >= 0) {
        next_tx = now + 100;
        if (ina_ok) { float ma = fabsf(ina.getCurrent_mA()); current_ma = (uint16_t)ma; }
        act_status_t a = { servo_deg, current_ma, act_counter++, (uint8_t)(timed_out ? ACT_FLAG_ECU_LOST : 0) };
        can_frame_t f;
        can_encode_act_status(&a, &f);
        if (can_ok) (void)mcp2515_send(&can, &f);
    }

    if ((int32_t)(now - next_screen) >= 0) { next_screen = now + 500; update_screen(timed_out, overtemp); }

    if ((int32_t)(now - next_json) >= 0) {
        next_json = now + 500;
        Serial.printf("{\"t\":%lu,\"throttle\":%u,\"temp\":%.1f,\"servo\":%u,\"current_ma\":%u,\"lost\":%lu,\"ecu_lost\":%d,\"overtemp\":%d,\"rx\":%lu,\"tec\":%u,\"rec\":%u,\"eflg\":%u,\"gap\":%lu}\n",
                      (unsigned long)now, last_ecu.throttle_pct, last_ecu.temp_x10 / 10.0f, servo_deg,
                      current_ma, (unsigned long)ecu_mon.lost_frames, timed_out, overtemp,
                      (unsigned long)rx_count, mcp2515_read_reg(&can, 0x1C), mcp2515_read_reg(&can, 0x1D), mcp2515_read_reg(&can, 0x2D), (unsigned long)max_gap);
        max_gap = 0;
    }
}
