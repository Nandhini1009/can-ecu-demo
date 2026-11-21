/* STM32 Nucleo-F411RE ECU: sets up the pins and peripherals by hand
 * (no CubeMX), then runs ecu_app. Clock = default 16 MHz internal oscillator. */
#include "main.h"
#include "ecu_app.h"

SPI_HandleTypeDef  hspi1;
ADC_HandleTypeDef  hadc1;
UART_HandleTypeDef huart2;

static void gpio_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};

    /* PA5 = green LED, PB6 = MCP2515 chip select (idle high) */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
    g.Pin = GPIO_PIN_5; g.Mode = GPIO_MODE_OUTPUT_PP; g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_6;
    HAL_GPIO_Init(GPIOB, &g);

    /* SPI1 on PB3 (SCK), PB4 (MISO), PB5 (MOSI) = alternate function 5 */
    g.Pin = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
    g.Mode = GPIO_MODE_AF_PP; g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_HIGH; g.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOB, &g);

    /* ADC inputs PA0 (pot) and PA1 (NTC) */
    g.Pin = GPIO_PIN_0 | GPIO_PIN_1; g.Mode = GPIO_MODE_ANALOG; g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &g);

    /* USART2 on PA2 (TX) and PA3 (RX) = alternate function 7 (ST-Link virtual COM port) */
    g.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    g.Mode = GPIO_MODE_AF_PP; g.Pull = GPIO_PULLUP; g.Speed = GPIO_SPEED_FREQ_LOW; g.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &g);
}

static void spi1_init(void)
{
    __HAL_RCC_SPI1_CLK_ENABLE();
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;          /* SPI mode 0 */
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;                      /* we toggle CS ourselves */
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;   /* 16 MHz / 8 = 2 MHz */
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 10;
    HAL_SPI_Init(&hspi1);
}

static void adc1_init(void)
{
    __HAL_RCC_ADC1_CLK_ENABLE();
    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    HAL_ADC_Init(&hadc1);
}

static void usart2_init(void)
{
    __HAL_RCC_USART2_CLK_ENABLE();
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);
}

int main(void)
{
    HAL_Init();
    gpio_init();
    spi1_init();
    adc1_init();
    usart2_init();

    ecu_app_init(&hspi1, &hadc1, &huart2);
    while (1) {
        ecu_app_poll();
    }
}
