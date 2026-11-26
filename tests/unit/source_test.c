/* A program file's lines as loaded: whole ones copied, the rest byte by byte (io/source.c). */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "io/source.h"
#include "plat/pmem.h"
#include "session.h"
#include "unit.h"

/* Lines that end in a blank or '_', CR, CR LF, LF, one across the 4 KB read-ahead, ^Z: what each
   reads as. */
TEST(a_files_lines_read_the_same_whole_or_byte_by_byte)
{
    static char src[8192];
    size_t n = 0;
#define PUT(s) (memcpy(src + n, (s), sizeof(s) - 1), n += sizeof(s) - 1)
    PUT("a$ = \"x  \r\n"); /* a string's blanks kept */
    PUT("PRINT LEN(a$)\r\n");
    PUT("PRINT 1 + _\n2\n"); /* joined */
    PUT("PRINT 4\r");        /* CR alone */
    PUT("PRINT 5 \t\n");     /* code's blanks dropped */
    PUT("READ d$: PRINT LEN(d$)\n");
    PUT("DATA ab  \n");

    while (n < 4090) /* the next line across the read-ahead's end */
        PUT("' pad\n");

    PUT("PRINT 7\n");
    PUT("\x1a"
        "PRINT 8\n"); /* ^Z: the file's end */
#undef PUT
    CHECK(unit_run_bytes(S, src, n) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 3 \r 3 \r 4 \r 5 \r 2 \r 7 \r");
}

/* cons.inbuf back to its first bytes, as boot leaves it. */
static void inbuf_first(Session *const S)
{
    pmem_free(S->cons.inbuf);
    S->cons.inbuf = nullptr;
    S->cons.incap = 0;
    cons_inroom(&S->cons, INFIRST);
}

/* Lines past the window's first bytes: joined ones, a line past SRCAP that slides it, blanks
   held in a joined line. The window grows as it fills; it was INMAX + 1 bytes from the start. */
TEST(a_files_long_lines_grow_the_window)
{
    static char src[60000];
    size_t n = (size_t)snprintf(src, sizeof src, "a = 0 _\n");

    for (uint32_t i = 0; i < 1000; i++)
        n += (size_t)snprintf(src + n, sizeof src - n, "+ 1 _\n");

    n += (size_t)snprintf(src + n, sizeof src - n, "\nPRINT a\nPRINT 9 ' ");
    memset(src + n, 'r', 40000);
    n += 40000;
    n += (size_t)snprintf(src + n, sizeof src - n, "\nPRINT 3;%300s4 _\n; 5\n", "");
    inbuf_first(S);
    CHECK(unit_run_bytes(S, src, n) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 1000 \r 9 \r 3  4  5 \r");
}

/* LINE INPUT # of a line past INMAX: its first INMAX characters, then the next line whole, the
   line in inbuf's room as it grew. */
TEST(line_input_reads_a_line_past_inmax)
{
    static char text[40010], line[128];
    char path[] = "/tmp/qb_unit_XXXXXX";
    FILE *const f = fdopen(mkstemp(path), "wb");
    memset(text, 'i', 40000);
    memcpy(text + 40000, "\nabc\n", 5);
    CHECK(f && fwrite(text, 1, 40005, f) == 40005);
    fclose(f);
    snprintf(line, sizeof line, "10 OPEN \"%s\" FOR INPUT AS #1", path);
    const char *const lines[] = {
        line,
        "20 LINE INPUT #1, a$: LINE INPUT #1, b$: CLOSE #1",
        "30 PRINT LEN(a$); RIGHT$(a$, 1); b$",
        nullptr,
    };

    inbuf_first(S);
    CHECK(unit_run(S, lines) == ST_OK);
    remove(path);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 32767 iabc\r");
    CHECK(S->cons.incap > INMAX); /* the line was in inbuf's own room */
}
