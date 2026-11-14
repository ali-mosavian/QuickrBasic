/* An error a module raises without the Session, and the stages that catch their own.
 *
 * # How it works
 * The session's handler reports it and unwinds; fault_raise does not return. A stage that returns
 * a Status (the compiler, folding) pushes a Catch while it runs: the session's handler throws an
 * error to the innermost one instead.
 */
#ifndef FAULT_H
#define FAULT_H
#include <setjmp.h>
#include "defs.h"

/* An error as a value: its E_* code, for E_USR its QbErr above it; ST_OK none. */
typedef uint16_t Status;
enum : Status { ST_OK = 0xFFFF };

enum : uint32_t { FAULT_KEEP = UINT32_MAX }; /* line: the current one; at: no caret */

typedef struct {
    uint8_t code, dg;
    uint32_t at, line;
    const char *other; /* {other}, len bytes */
    uint32_t len;
    uint16_t user; /* E_USR's QBasic error number (QbErr) */
} Failure;

/* fault_push's place: setjmp(jb) is 0 there, the error thrown's code after. */
typedef struct Catch Catch;
struct Catch {
    jmp_buf jb;
    Status code;
    Catch *outer;
};

typedef struct Fault Fault;
struct Fault {
    void (*raise)(Fault *f, const Failure *e);
    Catch *catch; /* the innermost, nullptr none */
};

/* c the innermost catch: setjmp(c->jb) next, then fault_pop past its scope. */
static inline void fault_push(Fault *const f, Catch *const c)
{
    c->outer = f->catch;
    f->catch = c;
}

static inline void fault_pop(Fault *const f) { f->catch = f->catch->outer; }

/* code to the innermost catch, popped. */
[[noreturn]]
static inline void fault_throw(Fault *const f, const Status code)
{
    Catch *const c = f->catch;
    f->catch = c->outer;
    c->code = code;
    longjmp(c->jb, 1);
}

[[noreturn]]
static inline void fault_raise(Fault *const f, const Failure e)
{
    f->raise(f, &e);
    __builtin_unreachable();
}

/* Error code on the current line, no caret. Out of line and cold: a hot caller keeps no frame for
   the Failure. */
[[noreturn, gnu::cold, gnu::noinline]]
static inline void fault_error(Fault *const f, const uint8_t code)
{
    fault_raise(f, (Failure){.code = code, .at = FAULT_KEEP, .line = FAULT_KEEP});
}

/* QBasic's error qb (QbErr), as E_USR. */
[[noreturn, gnu::cold, gnu::noinline]]
static inline void fault_qberr(Fault *const f, const uint16_t qb)
{
    fault_raise(
        f,
        (Failure){
            .code = E_USR,
            .at = FAULT_KEEP,
            .line = FAULT_KEEP,
            .user = qb,
        }

    );
}
#endif
