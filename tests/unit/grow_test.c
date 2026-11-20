/* Buffers that grow on demand: util/heap, and the session's past their first bytes. */
#include <stdio.h>
#include "session.h"
#include "unit.h"
#include "util/heap.h"

/* In place into the free bytes after it; else moved, its bytes kept; freed bytes taken again. */
TEST(heap_grows_in_place_moves_and_reuses)
{
    static alignas(max_align_t) uint8_t mem[4096];
    Heap h;
    heap_init(&h, mem, sizeof mem);
    uint8_t *const a = heap_resize(&h, nullptr, 100);
    CHECK(a);
    memset(a, 7, 100);
    CHECK(heap_resize(&h, a, 1000) == a); /* the rest is free */
    uint8_t *const b = heap_resize(&h, nullptr, 100);
    CHECK(b && b > a);
    uint8_t *const c = heap_resize(&h, a, 2000); /* b after it: moved */
    CHECK(c && c != a && c[0] == 7 && c[99] == 7);
    CHECK(heap_resize(&h, nullptr, 900) == a);     /* a's bytes free again */
    CHECK(!heap_resize(&h, b, 8000) && b[0] == 0); /* no room: b kept */
    heap_free(&h, c);
    CHECK(heap_resize(&h, b, 2500) == b); /* into c's freed bytes */
}

/* Calls nest past the frames' first room: each frame kept as more grow. */
TEST(frames_grow_as_calls_nest)
{
    static const char *const lines[] = {
        "10 PRINT f&(700)",
        "20 FUNCTION f& (n%)",
        "30 IF n% > 0 THEN f& = n% + f&(n% - 1)",
        "40 END FUNCTION",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 245350 \r");
}

/* The frames' room and the data stack's segments a deep call grew stayed taken from the heap:
   each deep run left less for the next. */
TEST(deep_calls_give_their_memory_back)
{
    static const char *const deep[] = {
        "10 PRINT f&(20000)",
        "20 FUNCTION f& (n&)",
        "30 IF n& > 0 THEN f& = 1 + f&(n& - 1)",
        "40 END FUNCTION",
        nullptr,
    };

    static const char *const next[] = {"10 PRINT 1", nullptr};

    CHECK(unit_run(S, next) == ST_OK);
    const Addr top = S->allo.top;
    CHECK(unit_run(S, deep) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 20000 \r");
    CHECK(unit_run(S, next) == ST_OK);
    CHECK(S->allo.top == top);
}

TEST(endless_calls_end_out_of_stack_space)
{
    static const char *const lines[] = {
        "10 s 0",
        "20 SUB s (n%)",
        "30 s n% + 1",
        "40 END SUB",
        nullptr,
    };

    CHECK(unit_run(S, lines) == E_OS);
}

/* A line whose code is past the code buffer's first bytes: compiled again into a larger one, its
   CONST no Duplicate definition. */
TEST(a_long_lines_code_grows_its_buffer)
{
    static char line[16384];
    int n = snprintf(line, sizeof line, "10 CONST k = 2: a = 0");

    for (uint32_t i = 0; i < 1000; i++)
        n += snprintf(line + n, sizeof line - (size_t)n, ": a = a + 1");

    snprintf(line + n, sizeof line - (size_t)n, ": PRINT a * k");
    const char *const lines[] = {line, nullptr};

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 2000 \r");
    CHECK(S->comp.xcap > 0x1000);
}

/* A line number past the scopes' first line-number bits: they grow, the label found. */
TEST(line_number_bits_grow)
{
    static const char *const lines[] = {"10 GOTO 63999", "20 PRINT 2", "63999 PRINT 1", nullptr};

    CHECK(unit_run_file(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 1 \r");
    CHECK(S->scop.nlnbit > 63999 / WORD_BITS);
}

/* The editor's columns past their first ones: grown, the end.s column right. */
TEST(lexer_columns_grow)
{
    static uint8_t text[1024];
    uint32_t n = (uint32_t)snprintf((char *)text, sizeof text, "PRINT 1");

    for (uint32_t i = 0; i < 100; i++)
        n += (uint32_t)snprintf((char *)text + n, sizeof text - n, ": PRINT 1");

    S->lexr.cols = true;
    lexr_line(&S->lexr, text, S->comp.cbuf);
    S->lexr.cols = false;
    CHECK(S->lexr.ncol > (uint32_t)S->lexr.last);
    CHECK(lexr_colof(&S->lexr, (uint32_t)S->lexr.last) == n); /* the end, past the last 1 */
}

/* ENVIRON past the environment's first bytes: it grows, the value read back. */
TEST(environ_grows)
{
    static const char *const lines[] = {
        "10 ENVIRON \"QBGROW=\" + STRING$(200, \"e\")",
        "20 PRINT LEN(ENVIRON$(\"QBGROW\"))",
        "30 ENVIRON \"QBGROW=\"",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 200 \r");
    CHECK(S->syst.envcap > 200);
}
