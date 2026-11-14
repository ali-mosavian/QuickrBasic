/* An error a module raises without the Session. The session's handler reports it and unwinds;
   fault_raise does not return. */
#ifndef FAULT_H
#define FAULT_H
#include <stdint.h>

enum : uint32_t { FAULT_KEEP = UINT32_MAX }; /* line: the current one; at: no caret */

typedef struct {
    uint8_t code, dg;
    uint32_t at, line;
    const char *other; /* {other}, len bytes */
    uint32_t len;
} Failure;

typedef struct Fault Fault;
struct Fault {
    void (*raise)(Fault *f, const Failure *e);
};

[[noreturn]]
static inline void fault_raise(Fault *const f, const Failure e)
{
    f->raise(f, &e);
    __builtin_unreachable();
}
#endif
