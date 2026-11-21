/* Tests the MCP2515 driver against a small software model of the chip.
 * These are the tests you make pass on Day 1. */
#include "test.h"
#include <string.h>
#include "mcp2515.h"

typedef struct {
    uint8_t reg[128];
    int     resets;
    int     first_cmd_seen;     /* first instruction ever sent */
} fake_chip_t;

static void sync_status(fake_chip_t *c)
{   /* the real chip copies the requested mode into CANSTAT */
    c->reg[MCP_CANSTAT] = (uint8_t)((c->reg[MCP_CANSTAT] & 0x1F) | (c->reg[MCP_CANCTRL] & MCP_MODE_MASK));
}

static void fake_xfer(void *ctx, const uint8_t *tx, uint8_t *rx, size_t len)
{
    fake_chip_t *c = ctx;
    memset(rx, 0, len);
    if (c->first_cmd_seen < 0) c->first_cmd_seen = tx[0];
    switch (tx[0]) {
    case MCP_CMD_RESET:
        memset(c->reg, 0, sizeof c->reg);
        c->reg[MCP_CANSTAT] = 0x80; c->reg[MCP_CANCTRL] = 0x87;   /* config mode after reset */
        c->resets++;
        break;
    case MCP_CMD_READ:
        if (len == 3) rx[2] = c->reg[tx[1]];
        break;
    case MCP_CMD_WRITE:
        for (size_t i = 2; i < len; i++) c->reg[tx[1] + i - 2] = tx[i];
        sync_status(c);
        break;
    case MCP_CMD_BIT_MODIFY:
        c->reg[tx[1]] = (uint8_t)((c->reg[tx[1]] & ~tx[2]) | (tx[3] & tx[2]));
        sync_status(c);
        break;
    case MCP_CMD_LOAD_TXB0:
        for (size_t i = 1; i < len; i++) c->reg[0x31 + i - 1] = tx[i];
        break;
    case MCP_CMD_RTS_TXB0:   /* loopback: the frame lands in receive buffer 0 */
        if ((c->reg[MCP_CANSTAT] & MCP_MODE_MASK) == MCP_MODE_LOOPBACK) {
            memcpy(&c->reg[0x61], &c->reg[0x31], 13);
            c->reg[MCP_CANINTF] |= MCP_RX0IF;
        }
        break;
    case MCP_CMD_READ_RXB0:
        for (size_t i = 1; i < len; i++) rx[i] = c->reg[0x61 + i - 1];
        c->reg[MCP_CANINTF] &= (uint8_t)~MCP_RX0IF;
        break;
    default: break;
    }
}
static void no_delay(uint32_t ms) { (void)ms; }

int main(void)
{
    fake_chip_t chip; memset(&chip, 0, sizeof chip); chip.first_cmd_seen = -1;
    mcp2515_t m = { .xfer = fake_xfer, .ctx = &chip, .delay_ms = no_delay };

    /* register access */
    mcp2515_write_reg(&m, MCP_CNF1, 0x5A);
    CHECK_EQ(mcp2515_read_reg(&m, MCP_CNF1), 0x5A);
    mcp2515_bit_modify(&m, MCP_CNF1, 0x0F, 0x03);
    CHECK_EQ(mcp2515_read_reg(&m, MCP_CNF1), 0x53);

    /* init: reset first, 500 kbit/s timing for 8 MHz, accept all, requested mode */
    memset(&chip, 0, sizeof chip); chip.first_cmd_seen = -1;
    CHECK_EQ(mcp2515_init_500k_8mhz(&m, MCP_MODE_LOOPBACK), MCP2515_OK);
    CHECK_EQ(chip.first_cmd_seen, MCP_CMD_RESET);
    CHECK_EQ(chip.reg[MCP_CNF1], 0x00);
    CHECK_EQ(chip.reg[MCP_CNF2], 0x90);
    CHECK_EQ(chip.reg[MCP_CNF3], 0x02);
    CHECK_EQ(chip.reg[MCP_RXB0CTRL] & 0x60, 0x60);
    CHECK_EQ(chip.reg[MCP_RXB1CTRL] & 0x60, 0x60);
    CHECK_EQ(chip.reg[MCP_CANSTAT] & MCP_MODE_MASK, MCP_MODE_LOOPBACK);

    /* nothing received yet */
    can_frame_t rx;
    CHECK_EQ(mcp2515_receive(&m, &rx), MCP2515_ERR_NO_MSG);

    /* send a frame in loopback and get it back */
    can_frame_t tx = { .id = 0x100, .dlc = 5, .data = { 73, 0x83, 0xFF, 200, 1 } };
    CHECK_EQ(mcp2515_send(&m, &tx), MCP2515_OK);
    CHECK_EQ(chip.reg[0x31], 0x20);        /* SIDH = id >> 3  */
    CHECK_EQ(chip.reg[0x32], 0x00);        /* SIDL = (id & 7) << 5 */
    CHECK_EQ(chip.reg[0x35], 5);           /* DLC */
    CHECK_EQ(mcp2515_receive(&m, &rx), MCP2515_OK);
    CHECK_EQ(rx.id, 0x100); CHECK_EQ(rx.dlc, 5);
    CHECK(memcmp(rx.data, tx.data, 5) == 0);
    CHECK_EQ(mcp2515_receive(&m, &rx), MCP2515_ERR_NO_MSG);   /* flag was cleared */

    /* an ID that uses the low 3 bits (ends up in SIDL) */
    can_frame_t tx2 = { .id = 0x2A5, .dlc = 2, .data = { 0xDE, 0xAD } };
    CHECK_EQ(mcp2515_send(&m, &tx2), MCP2515_OK);
    CHECK_EQ(chip.reg[0x32], (0x2A5 & 7) << 5);
    CHECK_EQ(mcp2515_receive(&m, &rx), MCP2515_OK);
    CHECK_EQ(rx.id, 0x2A5); CHECK_EQ(rx.dlc, 2);
    CHECK_EQ(rx.data[0], 0xDE); CHECK_EQ(rx.data[1], 0xAD);

    /* busy transmit buffer */
    chip.reg[MCP_TXB0CTRL] |= MCP_TXREQ;
    CHECK_EQ(mcp2515_send(&m, &tx), MCP2515_ERR_TX_BUSY);

    TEST_DONE("test_mcp2515");
}
