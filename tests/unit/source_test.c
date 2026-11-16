/* A program file's lines as loaded: whole ones copied, the rest byte by byte (io/source.c). */
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
