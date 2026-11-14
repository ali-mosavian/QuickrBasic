/* Completion's corpora (suggest_corpus_test.c): typed lines against what is offered. */
#ifndef CORPUS_H
#define CORPUS_H
#include "base.h"

/* The line typed; the first offered ("" none fit); the rest shown dim past the cursor ("" none);
   '|'-separated texts offered, and not offered; nullptr not checked. at: the line number typed
   over, 0 a new last line. */
typedef struct {
    const char *typed, *first, *guess, *has, *hasnt;
    uint16_t at;
} CorpusCase;

/* The n cases over program: the count not as expected, each printed. */
uint32_t corpus_run(Session *S, const char *const *program, const CorpusCase *cases, uint32_t n);
#endif
