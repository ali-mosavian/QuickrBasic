/* Errors as values: comp_text's Status. */
#include "session.h"
#include "unit.h"
#include "plat/pfil.h"

static Status compiled(Session *const S, const char *const text)
{
    const uint32_t n = (uint32_t)strlen(text);
    memcpy(cons_inroom(&S->cons, n + 1), text, n);
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

static void raises(Session *const S) { repl_error(S, E_IQ); }
static void readies(Session *const S) { repl_ready(S); }
static void returns([[maybe_unused]] Session *const S) {}

static Status inner;

/* An error inside a nested call is that call's; the outer one goes on. */
static void nests(Session *const S)
{
    inner = repl_call(S, raises);
    repl_error(S, E_TM);
}

/* A command's or a run's end comes back to its caller as a Status. */
TEST(call_returns_its_status)
{
    CHECK(repl_call(S, returns) == ST_OK);
    CHECK(repl_call(S, readies) == ST_OK);
    CHECK(repl_call(S, raises) == E_IQ);
    CHECK(repl_call(S, nests) == E_TM && inner == E_IQ);
}

/* A watch's probe catches its own error. */
TEST(probe_error_is_its_status)
{
    char out[80];
    CHECK(session_eval(S, "1 / 0", 0, out) == E_DZ);
    CHECK(session_eval(S, "1 + 1", 0, out) == ST_OK);
}
