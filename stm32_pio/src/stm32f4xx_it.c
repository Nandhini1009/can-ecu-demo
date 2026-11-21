#include "main.h"

/* HAL needs the 1 ms SysTick interrupt for HAL_GetTick() and HAL_Delay(). */
void SysTick_Handler(void)
{
    HAL_IncTick();
}
