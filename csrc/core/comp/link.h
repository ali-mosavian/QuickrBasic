/* The code cache's interface. How it works: link.c. */
#ifndef COMP_LINK_H
#define COMP_LINK_H
#include "defs.h"

typedef struct {
    Addr end;         /* cend: the cache's next byte */
    Addr lim;         /* elim: link_xlate's limit */
    Addr eol;         /* eeol: the stream's EOL */
    Addr inbeg;       /* einb: an INPUT's end to fill in, 0 none */
    Addr map;         /* emap: the stream's map */
    Addr code;        /* ecode: its code, past the map */
    uint32_t mapnext; /* the next statement link_xlate maps */
    bool linked;      /* the cache is ready for the program */
    uint32_t cursor;  /* mem/prog's and vm/flow's line cursor; 0 when linked */
    uint8_t flushes;  /* nfl: cache flushes, mod 256 */
} Link;

void link_start(Session *S); /* cm_link: before a run: blocks paired, the cache empty */
void link_compile(Session *S, uint32_t line); /* cm_comp: line compiled to comp_xbuf */
Addr link_cache(Session *S, uint32_t line);   /* cm_cache: line compiled into the cache; its code */
bool link_xlate(
    Session *S,
    Addr *at
); /* comp_xbuf as a stream at *at, to its end; link.code its code;
                                    false: past link.lim */
void link_walk(const uint8_t *code, void (*each)(uint8_t op, const uint8_t *at)); /* each op */
#endif
