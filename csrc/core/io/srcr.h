/* A source file's lines, streamed to the lexer (io/srd.asm). A logical line is physical lines
   joined where one ends in a blank and '_', outside a string and a comment (BC's continuation);
   the '_' becomes a space. The window, cons.inbuf, holds what of it fits, 0 after; srcr_more
   slides it for the lexer and reads on, so a line is as long as the store takes. Blanks in code
   wait until a byte follows them: a line's last ones go. */
#ifndef CORE_IO_SRCR_H
#define CORE_IO_SRCR_H
#include "io/cons.h"

enum { SRCAP = INMAX - 4 }; /* the window's text: then it slides */

typedef struct {
    uint8_t *end;  /* the window's end, its 0, in cons.inbuf (lexr reads it) */
    uint32_t pend; /* blanks in code not yet put */
    bool done;     /* the logical line is all in */
    bool any;      /* a byte or a line's end was read */
} Srcr;
extern Srcr srcr;

/* The input channel's next logical line into the window; its length there, -1 at the end with
   nothing read. */
int32_t srcr_line(void);
/* lexr's reader (LexrMore): the window used up, its bytes from *keep on moved to its start,
   then more of the line after them; *keep moved. false: the line has no more, nothing moved. */
bool srcr_more(const uint8_t **keep);

/* Whether a physical line continues, by its bytes (cn_*; the editor's too): a line starts; its
   next byte; it ended, true when it continues. */
void srcr_phys_start(void);
void srcr_phys_byte(uint8_t c);
bool srcr_phys_end(void);
#endif
