/* MCP2515 CAN controller driver (SPI). Talks to the chip only through m->xfer().
 * One xfer call = one SPI transaction: CS low, send tx[], read rx[], CS high.
 * Datasheet sections used: 12 (SPI instructions), 5 (bit timing), 4 and 5 (TX/RX buffers). */
#include "mcp2515.h"
#include <string.h>

void mcp2515_reset(mcp2515_t *m)
{
    uint8_t tx[1] = { MCP_CMD_RESET }, rx[1];
    m->xfer(m->ctx, tx, rx, 1);
    if (m->delay_ms) m->delay_ms(5);
}

uint8_t mcp2515_read_reg(mcp2515_t *m, uint8_t addr)
{
    uint8_t tx[3] = { MCP_CMD_READ, addr, 0 }, rx[3];
    m->xfer(m->ctx, tx, rx, 3);
    return rx[2];
}

void mcp2515_write_reg(mcp2515_t *m, uint8_t addr, uint8_t value)
{
    uint8_t tx[3] = { MCP_CMD_WRITE, addr, value }, rx[3];
    m->xfer(m->ctx, tx, rx, 3);
}

void mcp2515_bit_modify(mcp2515_t *m, uint8_t addr, uint8_t mask, uint8_t value)
{
    uint8_t tx[4] = { MCP_CMD_BIT_MODIFY, addr, mask, value }, rx[4];
    m->xfer(m->ctx, tx, rx, 4);
}

mcp2515_status_t mcp2515_set_mode(mcp2515_t *m, mcp2515_mode_t mode)
{
    mcp2515_bit_modify(m, MCP_CANCTRL, MCP_MODE_MASK, (uint8_t)mode);
    for (int i = 0; i < 10; i++) {
        if ((mcp2515_read_reg(m, MCP_CANSTAT) & MCP_MODE_MASK) == (uint8_t)mode)
            return MCP2515_OK;
        if (m->delay_ms) m->delay_ms(1);
    }
    return MCP2515_ERR_MODE;
}

mcp2515_status_t mcp2515_init_500k_8mhz(mcp2515_t *m, mcp2515_mode_t mode)
{
    mcp2515_reset(m);
    if ((mcp2515_read_reg(m, MCP_CANSTAT) & MCP_MODE_MASK) != MCP_MODE_CONFIG)
        return MCP2515_ERR_MODE;
    mcp2515_write_reg(m, MCP_CNF3, MCP_CNF3_8MHZ_500K);
    mcp2515_write_reg(m, MCP_CNF2, MCP_CNF2_8MHZ_500K);
    mcp2515_write_reg(m, MCP_CNF1, MCP_CNF1_8MHZ_500K);
    mcp2515_write_reg(m, MCP_RXB0CTRL, 0x64);   /* accept all, roll over to buffer 1 */
    mcp2515_write_reg(m, MCP_RXB1CTRL, 0x60);   /* accept all */
    mcp2515_write_reg(m, MCP_CANINTE, 0x00);    /* we poll, no interrupts */
    return mcp2515_set_mode(m, mode);
}

mcp2515_status_t mcp2515_send(mcp2515_t *m, const can_frame_t *f)
{
    if (mcp2515_read_reg(m, MCP_TXB0CTRL) & MCP_TXREQ) return MCP2515_ERR_TX_BUSY;
    uint8_t dlc = f->dlc > 8 ? 8 : f->dlc;
    uint8_t tx[14], rx[14];
    tx[0] = MCP_CMD_LOAD_TXB0;
    tx[1] = (uint8_t)(f->id >> 3);
    tx[2] = (uint8_t)((f->id & 0x07) << 5);
    tx[3] = 0; tx[4] = 0;
    tx[5] = dlc;
    memcpy(&tx[6], f->data, dlc);
    m->xfer(m->ctx, tx, rx, (size_t)(6 + dlc));
    uint8_t rts[1] = { MCP_CMD_RTS_TXB0 }, r[1];
    m->xfer(m->ctx, rts, r, 1);
    return MCP2515_OK;
}

mcp2515_status_t mcp2515_receive(mcp2515_t *m, can_frame_t *f)
{
    uint8_t flags = mcp2515_read_reg(m, MCP_CANINTF);
    uint8_t cmd;
    if (flags & MCP_RX0IF) cmd = MCP_CMD_READ_RXB0;
    else if (flags & MCP_RX1IF) cmd = MCP_CMD_READ_RXB1;
    else return MCP2515_ERR_NO_MSG;
    uint8_t tx[14] = { cmd }, rx[14];
    m->xfer(m->ctx, tx, rx, 14);
    f->id = (uint16_t)(((uint16_t)rx[1] << 3) | (rx[2] >> 5));
    f->dlc = rx[5] & 0x0F;
    if (f->dlc > 8) f->dlc = 8;
    memset(f->data, 0, 8);
    memcpy(f->data, &rx[6], f->dlc);
    return MCP2515_OK;
}
