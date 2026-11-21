/* STM32 Nucleo-F411RE: the "powertrain ECU".
 *  - reads the pot (PA0) = throttle and the NTC (PA1) = temperature
 *  - sends them every 50 ms as CAN frame 0x100 through the MCP2515
 *  - listens for the ESP32's status frame 0x200; LD2 (green LED) blinks if it goes silent
 *  - prints one text line per second on the ST-Link virtual COM port (115200 baud)
 *
 * Put this file and ecu_app.h in Core/Src and Core/Inc of your CubeIDE project, and copy
 * the ../common folder in too (add its path under Project > Properties > C/C++ Build >
 * Settings > Include paths, and add the .c files to the build).
 * Keep MCP2515_MODE on MCP_MODE_LOOPBACK for Day 1-2, then switch to MCP_MODE_NORMAL.
 */
#include "ecu_app.h"
#include <stdio.h>
#include <string.h>
#include "mcp2515.h"
#include "can_protocol.h"
#include "ntc.h"

#define MCP2515_MODE     MCP_MODE_NORMAL
#define TX_PERIOD_MS     50u
#define LOG_PERIOD_MS    1000u
#define ADC_MAX          4095u

#define CS_PORT          GPIOB
#define CS_PIN           GPIO_PIN_6          /* D10 on the Nucleo */

static SPI_HandleTypeDef  *s_spi;
static ADC_HandleTypeDef  *s_adc;
static UART_HandleTypeDef *s_uart;
static mcp2515_t           s_can;
static can_monitor_t       s_act_mon;
static act_status_t        s_last_act;
static uint8_t             s_counter;
static uint32_t            s_next_tx, s_next_log;
static bool                s_can_ok;
static uint8_t             s_throttle;
static int16_t             s_temp_x10;
static uint32_t            s_tx_busy;

static void spi_xfer(void *ctx, const uint8_t *tx, uint8_t *rx, size_t len)
{
    (void)ctx;
    HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(s_spi, (uint8_t *)tx, rx, (uint16_t)len, 20);
    HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_SET);
}

static uint16_t adc_read(uint32_t channel)
{
    ADC_ChannelConfTypeDef cfg = {0};
    cfg.Channel = channel;
    cfg.Rank = 1;
    cfg.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    HAL_ADC_ConfigChannel(s_adc, &cfg);
    /* the first conversion after switching channel still carries charge from the
       previous channel (the pot was pulling the temperature reading), so throw it away
       and keep the second one */
    uint16_t v = 0;
    for (int i = 0; i < 2; i++) {
        HAL_ADC_Start(s_adc);
        if (HAL_ADC_PollForConversion(s_adc, 5) == HAL_OK) v = (uint16_t)HAL_ADC_GetValue(s_adc);
        HAL_ADC_Stop(s_adc);
    }
    return v;
}

static void log_line(const char *text)
{
    HAL_UART_Transmit(s_uart, (uint8_t *)text, (uint16_t)strlen(text), 50);
}

void ecu_app_init(SPI_HandleTypeDef *hspi, ADC_HandleTypeDef *hadc, UART_HandleTypeDef *huart)
{
    s_spi = hspi; s_adc = hadc; s_uart = huart;
    HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_SET);
    s_can.xfer = spi_xfer;
    s_can.ctx = NULL;
    s_can.delay_ms = HAL_Delay;
    can_monitor_init(&s_act_mon);
    s_can_ok = (mcp2515_init_500k_8mhz(&s_can, MCP2515_MODE) == MCP2515_OK);
    log_line(s_can_ok ? "MCP2515 init OK\r\n" : "MCP2515 init FAILED (check wiring, 5V and CS)\r\n");
    s_next_tx = s_next_log = HAL_GetTick();
}

void ecu_app_poll(void)
{
    uint32_t now = HAL_GetTick();

    /* 1. empty the receive buffers */
    can_frame_t rx;
    while (s_can_ok && mcp2515_receive(&s_can, &rx) == MCP2515_OK) {
        act_status_t a;
        if (can_decode_act_status(&rx, &a)) {
            s_last_act = a;
            can_monitor_on_frame(&s_act_mon, now, a.counter);
        }
    }

    /* 2. send our status every TX_PERIOD_MS */
    if ((int32_t)(now - s_next_tx) >= 0) {
        s_next_tx += TX_PERIOD_MS;
        ecu_status_t e;
        e.throttle_pct = (uint8_t)((adc_read(ADC_CHANNEL_0) * 100u) / ADC_MAX);
        int16_t t = ntc_temp_x10(adc_read(ADC_CHANNEL_1), ADC_MAX);
        e.temp_x10 = t;
        s_throttle = e.throttle_pct;
        s_temp_x10 = t;
        e.flags = (t == NTC_BAD_READING || !temp_in_range(t) || t > TEMP_WARN_X10) ? ECU_FLAG_OVERTEMP : 0;
        e.counter = s_counter++;
        can_frame_t f;
        can_encode_ecu_status(&e, &f);
        if (s_can_ok && mcp2515_send(&s_can, &f) == MCP2515_ERR_TX_BUSY) s_tx_busy++;
    }

    /* 3. green LED blinks when the ESP32 is silent (not in loopback: it is always "silent") */
    bool lost = can_monitor_timed_out(&s_act_mon, now);
    if (MCP2515_MODE == MCP_MODE_NORMAL && lost) {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, ((now / 250u) & 1u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
    }

    /* 4. one log line per second */
    if ((int32_t)(now - s_next_log) >= 0) {
        s_next_log += LOG_PERIOD_MS;
        char line[170];
        int n = snprintf(line, sizeof line, "ecu tx=%u throttle=%u%% temp_x10=%d act servo=%u deg %u mA lost_frames=%lu tec=%u rec=%u eflg=0x%02X busy=%lu %s\r\n",
                         (unsigned)s_counter, (unsigned)s_throttle, (int)s_temp_x10, s_last_act.servo_deg, s_last_act.current_ma,
                         (unsigned long)s_act_mon.lost_frames,
                         (unsigned)mcp2515_read_reg(&s_can, 0x1C), (unsigned)mcp2515_read_reg(&s_can, 0x1D),
                         (unsigned)mcp2515_read_reg(&s_can, 0x2D), (unsigned long)s_tx_busy, (MCP2515_MODE == MCP_MODE_NORMAL && lost) ? "[ESP32 SILENT]" : "");
        if (n > 0) log_line(line);
    }
}
