#include "test.h"
#include "ntc.h"

int main(void)
{
    /* equal resistors (10k NTC = 10k fixed) means exactly 25.0 C */
    int16_t t = ntc_temp_x10(2048, 4095);
    CHECK(t >= 249 && t <= 251);
    /* lower ADC value = NTC resistance lower = hotter */
    CHECK(ntc_temp_x10(3000, 4095) < t);   /* 3000: NTC larger resistance = colder */
    CHECK(ntc_temp_x10(1000, 4095) > t);
    int16_t hot = ntc_temp_x10(1000, 4095);
    CHECK(hot > 450 && hot < 600);         /* about 50 C */
    /* open or shorted sensor */
    CHECK_EQ(ntc_temp_x10(0, 4095), NTC_BAD_READING);
    CHECK_EQ(ntc_temp_x10(4095, 4095), NTC_BAD_READING);
    TEST_DONE("test_ntc");
}
