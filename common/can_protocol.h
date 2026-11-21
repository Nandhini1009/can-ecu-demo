/* Shared CAN message format for the STM32 (ECU) <-> ESP32 (gateway) demo.
 * Plain C, no hardware calls, so it also runs (and is tested) on a PC. */
#ifndef CAN_PROTOCOL_H
#define CAN_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#define CAN_ID_ECU_STATUS   0x100u  /* STM32 -> ESP32: throttle, temperature */
#define CAN_ID_ACT_STATUS   0x200u  /* ESP32 -> STM32: servo angle, current   */
#define CAN_TIMEOUT_MS      500u    /* no frame for this long = peer lost     */

#define TEMP_MIN_X10        (-200)  /* -20.0 C: below this the reading is bad  */
#define TEMP_MAX_X10        (850)   /*  85.0 C: above this the reading is bad  */
#define TEMP_WARN_X10       (600)   /*  60.0 C: ECU raises the over-temp flag  */

#define ECU_FLAG_OVERTEMP   0x01u   /* ecu_status_t.flags bit 0 */
#define ACT_FLAG_ECU_LOST   0x01u   /* act_status_t.flags bit 0 */

typedef struct {
    uint16_t id;        /* 11-bit standard ID */
    uint8_t  dlc;       /* 0..8 */
    uint8_t  data[8];
} can_frame_t;

/* 0x100, 5 bytes: [0] throttle % | [1..2] temp x10 (int16, little endian)
 *                 [3] counter    | [4] flags */
typedef struct {
    uint8_t throttle_pct;   /* 0..100 */
    int16_t temp_x10;       /* 253 = 25.3 C */
    uint8_t counter;        /* +1 every frame, wraps at 255 */
    uint8_t flags;
} ecu_status_t;

/* 0x200, 5 bytes: [0] servo angle deg | [1..2] current mA (uint16, little endian)
 *                 [3] counter         | [4] flags */
typedef struct {
    uint8_t  servo_deg;     /* 0..180 */
    uint16_t current_ma;
    uint8_t  counter;
    uint8_t  flags;
} act_status_t;

void can_encode_ecu_status(const ecu_status_t *s, can_frame_t *f);
bool can_decode_ecu_status(const can_frame_t *f, ecu_status_t *s);
void can_encode_act_status(const act_status_t *s, can_frame_t *f);
bool can_decode_act_status(const can_frame_t *f, act_status_t *s);

uint8_t throttle_to_servo_deg(uint8_t throttle_pct);   /* 0..100 -> 0..180 */
bool    temp_in_range(int16_t temp_x10);

/* Watches one incoming message: timeout and lost-frame count. */
typedef struct {
    bool     seen;          /* got at least one frame */
    uint32_t last_rx_ms;
    uint8_t  last_counter;
    uint32_t lost_frames;   /* counter gaps added up */
} can_monitor_t;

void can_monitor_init(can_monitor_t *m);
void can_monitor_on_frame(can_monitor_t *m, uint32_t now_ms, uint8_t counter);
/* true if no frame yet, or none for more than CAN_TIMEOUT_MS (fail-safe) */
bool can_monitor_timed_out(const can_monitor_t *m, uint32_t now_ms);

#endif
