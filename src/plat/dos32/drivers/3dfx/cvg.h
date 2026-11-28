/* The Voodoo 2's (CVG's) command FIFO as Glide 3 writes it (glide3x/cvg/init/util.c:
 * sst1InitCmdFifo, sst1InitCmdFifoDirect), when the environment sets SSTV2_CMDFIFO: else the
 * Voodoo Graphics' direct writes (sst_chip), which 86Box shows more frames by.
 *
 * # How it works
 * The ring is the frame buffer past the buffers, as Glide places it: the most a power of two holds
 * there, up to 256 KB. The CPU writes it through BAR0's command FIFO window; while it is on, every
 * 3D register goes through it, and the init registers are written only around it. Texels go in
 * packets to texture space at their window's addresses. Pixels are read and copied through the
 * frame buffer's window, the card idle around them. A show waits until the card has read the last
 * swap.
 */
#ifndef TDFX_CVG_H
#define TDFX_CVG_H
#include "plat/dos32/drivers/3dfx/tdfx.h"

/* Asked for, the command FIFO on and empty past the buffers of pages each, the memory FIFO off,
   and c's chip writing through it. */
void cvg_queued(Tdfx *c, uint32_t pages);
/* On, the command FIFO read and off, the graphics unit reset, and c's chip direct again: the init
   registers may be written. */
void cvg_unqueued(Tdfx *c);
#endif
