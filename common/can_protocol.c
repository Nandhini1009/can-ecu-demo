#include "can_protocol.h"
#include <string.h>

void can_encode_ecu_status(const ecu_status_t *s, can_frame_t *f)
{
    memset(f, 0, sizeof *f);
    f->id = CAN_ID_ECU_STATUS;
    f->dlc = 5;
    f->data[0] = s->throttle_pct;
    f->data[1] = (uint8_t)((uint16_t)s->temp_x10 & 0xFF);
    f->data[2] = (uint8_t)((uint16_t)s->temp_x10 >> 8);
    f->data[3] = s->counter;
    f->data[4] = s->flags;
}

bool can_decode_ecu_status(const can_frame_t *f, ecu_status_t *s)
{
    if (f->id != CAN_ID_ECU_STATUS || f->dlc != 5) return false;
    if (f->data[0] > 100) return false;
    s->throttle_pct = f->data[0];
    s->temp_x10 = (int16_t)((uint16_t)f->data[1] | ((uint16_t)f->data[2] << 8));
    s->counter = f->data[3];
    s->flags = f->data[4];
    return true;
}

void can_encode_act_status(const act_status_t *s, can_frame_t *f)
{
    memset(f, 0, sizeof *f);
    f->id = CAN_ID_ACT_STATUS;
    f->dlc = 5;
    f->data[0] = s->servo_deg;
    f->data[1] = (uint8_t)(s->current_ma & 0xFF);
    f->data[2] = (uint8_t)(s->current_ma >> 8);
    f->data[3] = s->counter;
    f->data[4] = s->flags;
}

bool can_decode_act_status(const can_frame_t *f, act_status_t *s)
{
    if (f->id != CAN_ID_ACT_STATUS || f->dlc != 5) return false;
    if (f->data[0] > 180) return false;
    s->servo_deg = f->data[0];
    s->current_ma = (uint16_t)((uint16_t)f->data[1] | ((uint16_t)f->data[2] << 8));
    s->counter = f->data[3];
    s->flags = f->data[4];
    return true;
}

uint8_t throttle_to_servo_deg(uint8_t throttle_pct)
{
    if (throttle_pct > 100) throttle_pct = 100;
    return (uint8_t)((throttle_pct * 180u) / 100u);
}

bool temp_in_range(int16_t temp_x10)
{
    return temp_x10 >= TEMP_MIN_X10 && temp_x10 <= TEMP_MAX_X10;
}

void can_monitor_init(can_monitor_t *m)
{
    memset(m, 0, sizeof *m);
}

void can_monitor_on_frame(can_monitor_t *m, uint32_t now_ms, uint8_t counter)
{
    if (m->seen) {
        uint8_t gap = (uint8_t)(counter - m->last_counter - 1u);
        m->lost_frames += gap;
    }
    m->seen = true;
    m->last_rx_ms = now_ms;
    m->last_counter = counter;
}

bool can_monitor_timed_out(const can_monitor_t *m, uint32_t now_ms)
{
    if (!m->seen) return true;
    return (uint32_t)(now_ms - m->last_rx_ms) > CAN_TIMEOUT_MS;
}
