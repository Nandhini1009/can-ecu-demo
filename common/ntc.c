#include "ntc.h"
#include <math.h>

#define R_FIXED   10000.0f
/* resistance of the NTC at 25 C: 10k by default; build with -DNTC_R_NOMINAL=100000.0f for a 100k NTC (marked 104) */
#ifndef NTC_R_NOMINAL
#define NTC_R_NOMINAL 10000.0f
#endif
#define R_NOMINAL NTC_R_NOMINAL
#define T_NOMINAL 298.15f   /* 25 C in kelvin */
#define BETA      3950.0f

int16_t ntc_temp_x10(uint16_t adc, uint16_t adc_max)
{
    if (adc == 0 || adc >= adc_max) return NTC_BAD_READING; /* open or shorted */
    float r_ntc = R_FIXED * (float)adc / (float)(adc_max - adc);
    float inv_t = 1.0f / T_NOMINAL + logf(r_ntc / R_NOMINAL) / BETA;
    float temp_c = 1.0f / inv_t - 273.15f;
    return (int16_t)lroundf(temp_c * 10.0f);
}
