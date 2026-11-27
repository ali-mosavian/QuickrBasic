/* The command FIFO of the Voodoo 2, the Banshee and the Voodoo 3, written as Glide writes it: a
 * ring in frame memory the CPU writes packets into and the card reads as it draws.
 *
 * # How it works
 * The card reads from its read pointer up to the last word written, hole counting finding that
 * word. Registers are a PKT4 for each 14 a mask spans, a setup unit's corners a PKT3, texels a
 * PKT5 to texture space; a word is kept at the ring's end for a jump back to its start. The CPU
 * waits only when it would write over what the card has yet to read. Each card's bring-up places
 * the ring and turns it on; this module only writes it. A write-combined ring is fenced either side
 * of a jump and before each wait, as Glide fences it: hole counting takes words in any order, but
 * not those still in the CPU.
 */
#ifndef TDFX_CMDFIFO_H
#define TDFX_CMDFIFO_H
#include "plat/dos32/accel/driver.h"
#include <stdint.h>

typedef struct {
    const AccelHost *host;
    volatile uint8_t *ring;        /* the ring as the CPU writes it */
    const volatile uint32_t *read; /* the card's read pointer, a byte of frame memory */
    uint32_t at, bytes;            /* the ring's first byte in frame memory, its size */
    uint32_t sent, free;           /* the bytes written, a jump's skip too; those free */
    uint32_t mark;                 /* sent at the last mark */
    void(ACCEL_CALL *fence)(void); /* the host's, the ring write-combined: null, it is not */
} CmdFifo;

/* The ring of bytes, a power of two, at byte at of frame memory, empty: ring where the CPU writes
   its first word, read the card's read pointer. */
void cmdfifo_begin(
    CmdFifo *f,
    const AccelHost *host,
    volatile uint8_t *ring,
    const volatile uint32_t *read,
    uint32_t at,
    uint32_t bytes
);
/* The registers from reg's byte that mask picks, a bit each, written v's word at each's place. */
void cmdfifo_registers(CmdFifo *f, uint32_t reg, uint32_t mask, const uint32_t *v);
/* n corners to the setup unit, at most 15, stride words apart in v, each its words at at's places,
   the colour packed: mode, sSetupMode's; three a triangle, or one fan. */
void cmdfifo_corners(
    CmdFifo *f,
    uint32_t mode,
    const uint8_t *at,
    uint32_t words,
    const uint32_t *v,
    uint32_t stride,
    uint32_t n,
    bool fan
);
/* w by h texels, w even, to texture space: row y from its byte at + y * stride, from texels'
   row y * pitch; indexed, w a multiple of 4, a byte each, their low bytes. */
void cmdfifo_texels(
    CmdFifo *f,
    uint32_t at,
    uint32_t stride,
    const uint16_t *texels,
    uint32_t pitch,
    uint32_t w,
    uint32_t h,
    bool indexed
);
/* A mark after the words written so far. */
void cmdfifo_marked(CmdFifo *f);
/* Waits, up to TDFX_WAIT_US, until the card has read the words before the last mark; before all
   of them. Write-combined words are fenced first, as the card reads only those that reach it. */
void cmdfifo_waited(const CmdFifo *f);
void cmdfifo_drained(const CmdFifo *f);
#endif
