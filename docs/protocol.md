# CAN messages (500 kbit/s, 11-bit IDs, little endian)

| ID | From | Period | Bytes |
| --- | --- | --- | --- |
| 0x100 ECU_STATUS | STM32 | 50 ms | `[0]` throttle % (0-100), `[1..2]` temperature x10 (int16, 253 = 25.3 C), `[3]` counter, `[4]` flags (bit 0 = over-temp) |
| 0x200 ACT_STATUS | ESP32 | 100 ms | `[0]` servo angle (0-180), `[1..2]` current in mA (uint16), `[3]` counter, `[4]` flags (bit 0 = ECU lost) |

## Fault handling

- **Timeout:** no valid frame for more than 500 ms = the other node is lost. The ESP32 then sends the servo to 0 degrees (fail-safe) and sets `ACT_FLAG_ECU_LOST`. The STM32 blinks its green LED.
- **Lost frames:** the counter goes up by one each frame. A gap means frames were missed, and the gap size is added to `lost_frames`.
- **Over-temperature:** the STM32 sets bit 0 of the flags when the temperature is above 60.0 C, or the reading is out of the -20..85 C range, or the NTC is open/shorted. The ESP32 then forces the servo to 0.
- **Bad frames** (wrong ID, wrong length, throttle over 100, angle over 180) are ignored.

The code for all of this is in `common/can_protocol.c` and is tested in `tests/test_protocol.c`.
