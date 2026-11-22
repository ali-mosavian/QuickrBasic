/* Buffers that grow on demand: util/heap, and the session's past their first bytes. */
#include <stdio.h>
#include "session.h"
#include "unit.h"
#include "util/heap.h"

/* In place into the free bytes after it, its bytes kept; no room, kept; freed bytes taken again. */
TEST(heap_grows_in_place_moves_and_reuses)
{
    static alignas(max_align_t) uint8_t mem[4096];
    Heap h;
    heap_init(&h);
    heap_add(&h, mem, sizeof mem);
    uint8_t *const a = heap_bottom(&h, 100);
    CHECK(a > mem && a < mem + 64); /* the bottom, past its header */
    memset(a, 7, 100);
    CHECK(heap_resize(&h, a, 1000) == a); /* the rest is free */
    uint8_t *const b = heap_resize(&h, nullptr, 100);
    CHECK(b && b > a + 1000);
    CHECK(!heap_resize(&h, a, 4000));                /* no room anywhere */
    CHECK(a[0] == 7 && heap_resize(&h, a, 50) == a); /* kept, and smaller in place */
    heap_free(&h, b);
    CHECK(heap_resize(&h, a, 4000) == a); /* b's bytes free again */
}

/* Regions in the order added: a block goes to the first with room, at its top; the bottom's
   block grows into the room after it and no further, and says how much there is. */
TEST(heap_regions_low_and_high)
{
    static alignas(max_align_t) uint8_t first[600], second[8192];
    Heap h;
    heap_init(&h);
    heap_add(&h, first, sizeof first);
    heap_add(&h, second, sizeof second);
    uint8_t *const low = heap_bottom(&h, 1000); /* the largest room's bottom */
    CHECK(low > second && low < second + 64);
    uint8_t *const c = heap_resize(&h, nullptr, 500);
    CHECK(c > first && c + 500 <= first + sizeof first); /* the first region's */
    uint8_t *const d = heap_resize(&h, nullptr, 500);
    CHECK(
        d > low + 1000 && d + 500 <= second + sizeof second && d + 564 > second + sizeof second
    ); /* its top */
    const size_t room = heap_after(&h, low);
    CHECK(room > 6000 && room < 7000);
    CHECK(!heap_extend(&h, low, 1000 + room + 16));
    CHECK(heap_after(&h, low) == room);
    CHECK(heap_extend(&h, low, 1000 + room));
    CHECK(heap_after(&h, low) == 0);
    CHECK(!heap_resize(&h, nullptr, 500)); /* full */
}

/* The bottom's block in the first region: a block goes to the others while they have room. Taken
   from the first region's top, the IDE's blocks ate the BASIC heap's room at 2 MB, and FRE was
   74 KB with DPMI's block half free. */
TEST(heap_blocks_leave_the_bottoms_region)
{
    static alignas(max_align_t) uint8_t first[8192], second[600];
    Heap h;
    heap_init(&h);
    heap_add(&h, first, sizeof first);
    heap_add(&h, second, sizeof second);
    uint8_t *const low = heap_bottom(&h, 1000);
    CHECK(low > first && low < first + 64);
    const size_t room = heap_after(&h, low);
    uint8_t *const c = heap_resize(&h, nullptr, 500);
    CHECK(c > second && c + 500 <= second + sizeof second);
    CHECK(heap_after(&h, low) == room);
    uint8_t *const d = heap_resize(&h, nullptr, 500); /* the second is full: the first's top */
    CHECK(d > low + 1000 && d + 500 <= first + sizeof first);
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
   each deep run left less for the next. The frames shrunk in place cut 3.9 MB off FRE. */
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
    const uint32_t free = allo_free(&S->allo);
    CHECK(unit_run(S, deep) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 20000 \r");
    CHECK(unit_run(S, next) == ST_OK);
    CHECK(allo_free(&S->allo) == free);
}

/* Strings past the BASIC heap's first bytes grow it in place, FRE its room less theirs. Fixed at
   HMIN, 256 KB of strings was Out of memory. */
TEST(strings_grow_the_heap)
{
    static const char *const lines[] = {
        "10 DIM a$(40)",
        "20 f& = FRE(\"\")",
        "30 FOR i% = 1 TO 40: a$(i%) = STRING$(30000, 120): NEXT",
        "40 PRINT LEN(a$(40)); f& - FRE(\"\") >= 1200000",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 30000 -1 \r");
    CHECK(S->allo.top - S->allo.bot > 1200000);
}

/* A dynamic array past the free bytes grows the heap, the strings slid up over the room. */
TEST(arrays_grow_the_heap)
{
    static const char *const lines[] = {
        "10 s$ = STRING$(100, 120) + \"\"",
        "20 REDIM a&(200000)",
        "30 a&(200000) = 7",
        "40 PRINT a&(200000); LEN(s$)",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 7  100 \r");
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

/* A high line number: the label found. */
TEST(line_number_high)
{
    static const char *const lines[] = {"10 GOTO 63999", "20 PRINT 2", "63999 PRINT 1", nullptr};

    CHECK(unit_run_file(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 1 \r");
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
