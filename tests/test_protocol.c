#include "test.h"
#include "can_protocol.h"

int main(void)
{
    /* ECU frame round trip, including a negative temperature */
    ecu_status_t e = { .throttle_pct = 73, .temp_x10 = -125, .counter = 200, .flags = ECU_FLAG_OVERTEMP };
    can_frame_t f; ecu_status_t e2;
    can_encode_ecu_status(&e, &f);
    CHECK_EQ(f.id, 0x100); CHECK_EQ(f.dlc, 5);
    CHECK_EQ(f.data[0], 73);
    CHECK_EQ(f.data[1], 0x83); CHECK_EQ(f.data[2], 0xFF);   /* -125 = 0xFF83 */
    CHECK(can_decode_ecu_status(&f, &e2));
    CHECK_EQ(e2.throttle_pct, 73); CHECK_EQ(e2.temp_x10, -125);
    CHECK_EQ(e2.counter, 200); CHECK_EQ(e2.flags, ECU_FLAG_OVERTEMP);

    /* bad frames are refused */
    f.data[0] = 101;               CHECK(!can_decode_ecu_status(&f, &e2));
    can_encode_ecu_status(&e, &f); f.dlc = 4; CHECK(!can_decode_ecu_status(&f, &e2));
    can_encode_ecu_status(&e, &f); f.id = 0x101; CHECK(!can_decode_ecu_status(&f, &e2));

    /* actuator frame round trip */
    act_status_t a = { .servo_deg = 135, .current_ma = 412, .counter = 7, .flags = ACT_FLAG_ECU_LOST };
    act_status_t a2;
    can_encode_act_status(&a, &f);
    CHECK_EQ(f.id, 0x200);
    CHECK_EQ(f.data[1], 412 & 0xFF); CHECK_EQ(f.data[2], 412 >> 8);
    CHECK(can_decode_act_status(&f, &a2));
    CHECK_EQ(a2.servo_deg, 135); CHECK_EQ(a2.current_ma, 412);
    f.data[0] = 181; CHECK(!can_decode_act_status(&f, &a2));

    /* throttle to servo angle */
    CHECK_EQ(throttle_to_servo_deg(0), 0);
    CHECK_EQ(throttle_to_servo_deg(50), 90);
    CHECK_EQ(throttle_to_servo_deg(100), 180);
    CHECK_EQ(throttle_to_servo_deg(250), 180);   /* clamped */

    /* temperature limits */
    CHECK(temp_in_range(-200)); CHECK(temp_in_range(850));
    CHECK(!temp_in_range(-201)); CHECK(!temp_in_range(851));

    /* monitor: timeout and lost frames */
    can_monitor_t m; can_monitor_init(&m);
    CHECK(can_monitor_timed_out(&m, 0));              /* nothing received yet = fail-safe */
    can_monitor_on_frame(&m, 1000, 10);
    CHECK(!can_monitor_timed_out(&m, 1500));          /* exactly 500 ms is still ok */
    CHECK(can_monitor_timed_out(&m, 1501));
    can_monitor_on_frame(&m, 1050, 11);  CHECK_EQ(m.lost_frames, 0);
    can_monitor_on_frame(&m, 1150, 14);  CHECK_EQ(m.lost_frames, 2);   /* 12 and 13 missing */
    can_monitor_on_frame(&m, 1200, 255); can_monitor_on_frame(&m, 1250, 0);
    CHECK_EQ(m.lost_frames, 2 + 240);                  /* 14 -> 255 skipped 15..254 */
    CHECK(!can_monitor_timed_out(&m, 1250));
    /* millisecond counter wrapping around must not cause a false timeout */
    can_monitor_on_frame(&m, 0xFFFFFFF0u, 1);
    CHECK(!can_monitor_timed_out(&m, 20));

    TEST_DONE("test_protocol");
}
