/* A procedure's frame layout. How it works: frame.c. */
#ifndef COMP_FRAME_H
#define COMP_FRAME_H
#include "comp/blocks.h"
#include "mem/symbols.h"
#include "comp/declare.h"

/* Procedure proc's frame, every line compiled: each local's Loc, its arrays' room, p->nframe and
   PF_ARRAYS. */
void frame_layout(Symb *sy, const Decl *d, Proc *p, uint32_t proc);
#endif
