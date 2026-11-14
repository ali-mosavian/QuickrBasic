/* A source file's lines' interface. How it works: source.c. */
#ifndef CORE_IO_SRCR_H
#define CORE_IO_SRCR_H
#include "comp/lexer.h"
#include "io/console.h"

enum { SRCAP = INMAX - 4 }; /* the window's text: then it slides */

/* Where a physical line's byte is. */
typedef enum : uint8_t { IN_CODE, IN_STRING, IN_COMMENT } Where;

/* Whether the physical line continues, by its bytes */
typedef struct {
    Where st;
    uint8_t letters; /* the word's letters so far */
    bool under;      /* the last byte in code a '_' after a blank */
    bool blank;      /* the byte before a blank */
    char rem[3];     /* the word's first letters, small */
} SrcrCont;

typedef struct {
    LexrSource src; /* the window's end, its 0, in cons.inbuf; the lexer reads it */
    uint32_t pend;  /* blanks in code not yet put */
    bool done;      /* the logical line is all in */
    bool any;       /* a byte or a line's end was read */
    SrcrCont cont;
    Cons *cons;      /* reads from */
    Chnl *chnl;
    uint16_t *fline; /* counts the physical lines joined */
} Srcr;

/* sr reads co's input channel ch; joined lines counted in *fline. */
void srcr_init(Srcr *sr, Cons *co, Chnl *ch, uint16_t *fline);

/* The input channel's next logical line into the window; its length there, -1 at the end with
   nothing read. */
int32_t srcr_line(Srcr *sr);
/* The lexer's reader (Srcr.src's more): the window used up, its bytes from *keep on moved to its
   start, then more of the line after them; *keep moved. False: the line has no more. */
bool srcr_more(LexrSource *src, const uint8_t **keep);

/* Whether a physical line continues, by its bytes (cn_*; the editor's too): a line starts; its
   next byte; it ended, true when it continues. */
void srcr_phys_start(SrcrCont *cn);
void srcr_phys_byte(SrcrCont *cn, uint8_t c);
bool srcr_phys_end(SrcrCont *cn);
#endif
