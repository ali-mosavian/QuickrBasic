/* IMPORT and FROM: the modules a line names, read from its tokens; their statements compiled.
 * How it works: import.c. */
#ifndef CORE_COMP_IMPORT_H
#define CORE_COMP_IMPORT_H
#include "comp/comp.h"

/* A name as written: its dotted text, len bytes (0: FROM's dots alone); the folders up a module's
   leading dots go (0 none: the search path's; 1 the importing file's folder); the name its AS gives
   (aslen 0 none); FROM's name with (): an array. */
typedef struct {
    const uint8_t *text;
    uint8_t len;
    uint8_t up;
    uint8_t aslen;
    bool arr;
    const uint8_t *as;
} ImptName;

/* What a name in the statement is: IMPORT's module; FROM's module; a name FROM gives, which is a
   module of its package when FROM's module is a folder; FROM's *. */
typedef enum : uint8_t { IMPT_MODULE, IMPT_FROM, IMPT_NAME, IMPT_STAR } ImptKind;

typedef void ImptEach(void *ctx, ImptKind kind, const ImptName *n);

/* The IMPORT or FROM statement at p, its names to each in order (each null: none). Returns where
   it stopped: its end, else the token that does not fit. *ok: it is written as one. */
const uint8_t *impt_scan(const uint8_t *p, ImptEach *each, void *ctx, bool *ok);
/* A line's body starts with an IMPORT or FROM statement, written as one: its names to each. */
bool impt_line(const uint8_t *body, ImptEach *each, void *ctx);
/* IMPORT or FROM at comp.p (TK_X, then its token): checked. Syntax error: not the line's first
   statement, inside a SUB or FUNCTION, or not written as one. Illegal direct: typed alone. */
void impt_compile(Comp *C);
#endif
