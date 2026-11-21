/* 10k NTC thermistor read through an ADC.
 * Wiring: 3.3V -- 10k fixed resistor -- ADC pin -- NTC -- GND.
 * Uses the beta equation (beta = 3950, 10k at 25 C). */
#ifndef NTC_H
#define NTC_H
#include <stdint.h>

#define NTC_BAD_READING  INT16_MIN   /* returned when adc is 0 or full scale */

/* Temperature in tenths of a degree C (253 = 25.3 C). adc_max is 4095 for 12 bit. */
int16_t ntc_temp_x10(uint16_t adc, uint16_t adc_max);
#endif
