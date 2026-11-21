#ifndef MCP2515_H
#define MCP2515_H

#include <stddef.h>
#include <stdint.h>
#include "can_protocol.h"
#include "mcp2515_regs.h"

/* One complete SPI transaction: pull CS low, send tx[0..len-1] while filling
 * rx[0..len-1], release CS. Written once per board (STM32 HAL, Arduino SPI, test). */
typedef void (*mcp2515_xfer_fn)(void *ctx, const uint8_t *tx, uint8_t *rx, size_t len);
typedef void (*mcp2515_delay_fn)(uint32_t ms);

typedef struct {
    mcp2515_xfer_fn  xfer;
    void            *ctx;
    mcp2515_delay_fn delay_ms;
} mcp2515_t;

typedef enum {
    MCP2515_OK          =  0,
    MCP2515_ERR_MODE    = -1,   /* chip did not enter the requested mode */
    MCP2515_ERR_TX_BUSY = -2,   /* previous frame is still being sent */
    MCP2515_ERR_NO_MSG  = -3    /* nothing waiting in the receive buffers */
} mcp2515_status_t;

typedef enum {
    MCP_MODE_NORMAL   = 0x00,
    MCP_MODE_SLEEP    = 0x20,
    MCP_MODE_LOOPBACK = 0x40,   /* sends to itself, no bus needed: good for bring-up */
    MCP_MODE_LISTEN   = 0x60,
    MCP_MODE_CONFIG   = 0x80
} mcp2515_mode_t;

void    mcp2515_reset(mcp2515_t *m);
uint8_t mcp2515_read_reg(mcp2515_t *m, uint8_t addr);
void    mcp2515_write_reg(mcp2515_t *m, uint8_t addr, uint8_t value);
void    mcp2515_bit_modify(mcp2515_t *m, uint8_t addr, uint8_t mask, uint8_t value);

mcp2515_status_t mcp2515_set_mode(mcp2515_t *m, mcp2515_mode_t mode);
/* reset, set 500 kbit/s for an 8 MHz crystal, accept all frames, enter `mode` */
mcp2515_status_t mcp2515_init_500k_8mhz(mcp2515_t *m, mcp2515_mode_t mode);

mcp2515_status_t mcp2515_send(mcp2515_t *m, const can_frame_t *f);
mcp2515_status_t mcp2515_receive(mcp2515_t *m, can_frame_t *f);

#endif
