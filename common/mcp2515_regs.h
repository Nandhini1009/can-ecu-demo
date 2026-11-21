/* MCP2515 SPI instructions and registers (see the MCP2515 datasheet, section 12 and 11). */
#ifndef MCP2515_REGS_H
#define MCP2515_REGS_H

/* SPI instructions (first byte of every SPI transaction) */
#define MCP_CMD_RESET        0xC0
#define MCP_CMD_READ         0x03   /* + addr, then read data back */
#define MCP_CMD_WRITE        0x02   /* + addr, then data bytes */
#define MCP_CMD_BIT_MODIFY   0x05   /* + addr, mask, value */
#define MCP_CMD_LOAD_TXB0    0x40   /* then SIDH, SIDL, EID8, EID0, DLC, D0..D7 */
#define MCP_CMD_RTS_TXB0     0x81   /* request to send buffer 0 */
#define MCP_CMD_READ_RXB0    0x90   /* then SIDH, SIDL, EID8, EID0, DLC, D0..D7 */
#define MCP_CMD_READ_RXB1    0x94

/* Registers */
#define MCP_CANSTAT   0x0E   /* bits 7:5 = current mode */
#define MCP_CANCTRL   0x0F   /* bits 7:5 = requested mode */
#define MCP_CNF3      0x28
#define MCP_CNF2      0x29
#define MCP_CNF1      0x2A
#define MCP_CANINTE   0x2B
#define MCP_CANINTF   0x2C   /* bit 0 = RX0IF (message in buffer 0), bit 1 = RX1IF */
#define MCP_TXB0CTRL  0x30   /* bit 3 = TXREQ (still sending) */
#define MCP_RXB0CTRL  0x60   /* bits 6:5 = RXM, 11 = accept every message */
#define MCP_RXB1CTRL  0x70

#define MCP_MODE_MASK 0xE0
#define MCP_TXREQ     0x08
#define MCP_RX0IF     0x01
#define MCP_RX1IF     0x02

/* Bit timing for an 8 MHz crystal at 500 kbit/s (8 time quanta per bit) */
#define MCP_CNF1_8MHZ_500K  0x00
#define MCP_CNF2_8MHZ_500K  0x90
#define MCP_CNF3_8MHZ_500K  0x02

#endif
