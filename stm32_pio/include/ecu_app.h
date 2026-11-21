#ifndef ECU_APP_H
#define ECU_APP_H
#include "main.h"   /* CubeIDE header: brings in the HAL and the handle types */

/* Call once after MX_SPI1_Init(), MX_ADC1_Init() and MX_USART2_UART_Init(). */
void ecu_app_init(SPI_HandleTypeDef *hspi, ADC_HandleTypeDef *hadc, UART_HandleTypeDef *huart);
/* Call as often as you can from the while(1) loop in main(). Never blocks for long. */
void ecu_app_poll(void);
#endif
