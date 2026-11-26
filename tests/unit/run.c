/* Runs every TEST, once the core is booted; exits 1 when one failed. */
#include <stdio.h>
#include <stdlib.h>
#include "boot.h"
#include "session.h"
#include "comp/link.h"
#include "ui/linemap.h"
#include "unit.h"

UnitList unit_tests = {nullptr, &unit_tests.first};
static uint32_t failures;
static const char *current;

void unit_add(UnitList *const l, UnitTest *const t)
{
    t->next = nullptr;
    *l->end = t;
    l->end = &t->next;
}

void unit_fail(const char *const file, const int line, const char *const what)
{
    failures++;
    fprintf(stderr, "FAIL %s: %s:%d: %s\n", current, file, line, what);
}

/* text entered as a typed line. */
static uint8_t entered(Session *const S, const char *const text)
{
    const uint32_t n = (uint32_t)strlen(text);
    memcpy(cons_inroom(&S->cons, n + 1), text, n);
    return repl_enter(S, n);
}

static void direct(Session *const S) { repl_run(S); }

/* A typed direct line run. */
static Status typed(Session *const S, const char *const text)
{
    CHECK(entered(S, text) == ENTER_DIRECT);
    return repl_call(S, direct);
}

void unit_store(Session *const S, const char *const *const lines)
{
    session_new(S);

    for (size_t i = 0; lines[i]; i++)
        CHECK(entered(S, lines[i]) == ENTER_STORED);
}

Status unit_run_shown(Session *const S, const char *const *const lines)
{
    unit_store(S, lines);
    return typed(S, "RUN");
}

Status unit_typed(Session *const S, const char *const line)
{
    S->cons.capture = CAP_SBUF;
    S->cons.sblen = 0;
    const Status st = typed(S, line);
    S->cons.capture = 0;
    return st;
}

Status unit_run(Session *const S, const char *const *const lines)
{
    unit_store(S, lines);
    return unit_typed(S, "RUN");
}

static Session session;

/* A test's name holds one of the arguments, or there are none. */
static bool picked(const char *const name, const int argc, const char *const *const argv)
{
    for (int i = 1; i < argc; i++)
        if (strstr(name, argv[i]))
            return true;

    return argc < 2;
}

/* Each test, or with arguments those whose names hold one. */
int main(const int argc, const char *const *const argv)
{
    Session *const S = &session;
    static const FrontEnd editor = {
        .regions = lmap_init
    }; /* the editor's line map (editor_test.c) */

    boot(S, &editor);

    uint32_t ran = 0;

    for (const UnitTest *t = unit_tests.first; t; t = t->next) {
        if (!picked(t->name, argc, argv))
            continue;

        current = t->name;
        t->fn(S);
        ran++;
    }

    printf("%u tests, %u failures\n", ran, failures);
    return failures ? 1 : 0;
}

uint32_t unit_ops(const Session *const S, const uint8_t op)
{
    uint32_t n = 0;

    for (uint32_t l = PROGBASE, len; (len = prog_line(&S->prog, l)->len); l += len) {
        const uint8_t *b = at(S->allo.cache + prog_line(&S->prog, l)->code);

        for (; *b != O_EOL && *b != O_EOL_TO; b += link_opsize(b))
            n += *b == op;
    }

    return n;
}

uint32_t unit_calls(const Session *const S, const uint8_t k)
{
    uint32_t n = 0;

    for (uint32_t l = PROGBASE, len; (len = prog_line(&S->prog, l)->len); l += len) {
        const uint8_t *b = at(S->allo.cache + prog_line(&S->prog, l)->code);

        for (; *b != O_EOL && *b != O_EOL_TO; b += link_opsize(b))
            n += *b == O_CALL && b[1] == k;
    }

    return n;
}

Status unit_run_bytes(Session *const S, const char *const src, const size_t n)
{
    char path[] = "/tmp/qb_unit_XXXXXX";
    FILE *const f = fdopen(mkstemp(path), "wb");
    CHECK(f && fwrite(src, 1, n, f) == n);
    fclose(f);
    session_new(S);
    snprintf(S->repl.file, sizeof S->repl.file, "%s", path);
    Status st = repl_call(S, repl_load);
    S->repl.loading = false; /* an error ended the load: the IDE's (ui/uimain.c) */
    remove(path);

    if (st != ST_OK)
        return st;

    S->cons.capture = CAP_SBUF;
    S->cons.sblen = 0;
    st = typed(S, "RUN");
    S->cons.capture = 0;
    return st;
}

Status unit_run_file(Session *const S, const char *const *const lines)
{
    session_new(S);
    S->repl.loading = true;

    for (size_t i = 0; lines[i]; i++)
        CHECK(entered(S, lines[i]) == ENTER_STORED);

    S->repl.loading = false;
    S->cons.capture = CAP_SBUF;
    S->cons.sblen = 0;
    const Status st = typed(S, "RUN");
    S->cons.capture = 0;
    return st;
}
