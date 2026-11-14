/* Errors as values: comp_text's Status. */
#include "session.h"
#include "unit.h"
#include "plat/pfil.h"

static Status compiled(Session *const S, const char *const text)
{
    const uint32_t n = (uint32_t)strlen(text);
    memcpy(S->cons.inbuf, text, n);
    CHECK(repl_enter(S, n) == ENTER_DIRECT);
    return comp_line(&S->comp);
}

TEST(compile_error_is_its_status)
{
    CHECK(compiled(S, "x = 1") == ST_OK);
    CHECK(compiled(S, "x = (") == E_SN);
}

/* Folding's catch is its own: 1 / 0 left unfolded, to raise when it runs. */
TEST(fold_error_is_not_the_compiles)
{
    CHECK(compiled(S, "x = 1 / 0") == ST_OK);
    CHECK(compiled(S, "x = 1") == ST_OK);
}

/* The channels' errors are values. */
TEST(channel_error_is_its_status)
{
    Chan *c = nullptr;
    CHECK(chnl_find(&S->chnl, 7, &c) == st_qb(QB_BAD_FILE_NUMBER));
    CHECK(chnl_open(&S->chnl, 0, "x.tmp", FM_OUT, RECLEN_DEFAULT, &c) == st_qb(QB_BAD_FILE_NUMBER));
}

/* A byte write's error is kept for its statement; the file gets none of the bytes after it. */
TEST(byte_error_kept_and_bytes_dropped)
{
    static const char name[] = "status_test.tmp";
    Chnl *const cl = &S->chnl;
    Chan *c;
    CHECK(chnl_open(cl, 1, name, FM_OUT, RECLEN_DEFAULT, &c) == ST_OK);
    CHECK(chnl_out(cl, c) == ST_OK);
    cl->err = st_qb(QB_DISK_FULL);
    chnl_putc(cl, 'x');
    CHECK(chnl_take(cl) == st_qb(QB_DISK_FULL));
    CHECK(chnl_take(cl) == ST_OK);
    CHECK(chnl_close(cl, c) == ST_OK);
    uint32_t len = 1;
    CHECK(chnl_open(cl, 1, name, FM_IN, RECLEN_DEFAULT, &c) == ST_OK);
    CHECK(chnl_lof(c, &len) == ST_OK && len == 0);
    CHECK(chnl_close(cl, c) == ST_OK);
    CHECK(!pfil_delete(name));
}
