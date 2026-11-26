/* The Banshee's and Voodoo 3's command FIFO, driven as Glide drives it: a ring in frame memory the
 * CPU writes packets into through BAR1 and the card reads as it draws.
 *
 * # How it works
 * The card reads from its read pointer up to the last word written, hole counting finding that
 * word. A run of registers is a PKT1, texels a PKT5 to TMU0's texture port; a word is kept at the
 * ring's end for a jump back to its start. The CPU waits only when it would write over what the
 * card has yet to read.
 */
#ifndef TDFX_CMDFIFO_H
#define TDFX_CMDFIFO_H
#include "plat/dos32/accel/driver.h"
#include <stdint.h>

enum : uint32_t {
    CMDFIFO_BYTES = 256 << 10,
    CMDFIFO_BUSY = 1u << 11, /* status: words in the FIFO */
};

typedef struct {
    const AccelHost *host;
    volatile uint8_t *regs; /* its registers, in BAR0 */
    volatile uint8_t *ring; /* through BAR1 */
    uint32_t at;            /* the ring's first byte in frame memory */
    uint32_t sent, free;    /* the bytes written, a jump's skip too; those free before the reader */
} CmdFifo;

/* The FIFO on and empty at byte at of frame memory, which frame maps; io maps BAR0. The Banshee's
   fetch thresholds, else the Voodoo 3's. */
void cmdfifo_open(
    CmdFifo *f,
    const AccelHost *host,
    volatile uint8_t *io,
    volatile uint8_t *frame,
    uint32_t at,
    bool banshee
);
void cmdfifo_close(const CmdFifo *f);
/* n words to the 3D registers from reg's byte on. */
void cmdfifo_registers(CmdFifo *f, uint32_t reg, const uint32_t *v, uint32_t n);
/* n texels, n even, to TMU0's texture port from its byte at. */
void cmdfifo_texels(CmdFifo *f, uint32_t at, const uint16_t *texels, uint32_t n);
/* The place after the words written so far. */
uint32_t cmdfifo_mark(const CmdFifo *f);
/* Waits, up to TDFX_WAIT_US, until the card has read the words before mark. */
void cmdfifo_read_to(const CmdFifo *f, uint32_t mark);
#endif
