/* QBasic's error numbers and messages: the interface. How it works: errmsg.c. */
#ifndef CORE_IO_EMSG_H
#define CORE_IO_EMSG_H
#include "defs.h"

/* ERR by E_*: 0 none; FFh Out of stack space, never trapped; E0h.. BC's compile errors, which
   have no ERR of their own. */
extern const uint8_t emsg_errnum[E_LAST + 1];

/* The message of error code (E_*), 0-ended; E_USR's by exec's uerr; "Unprintable error" when
   the number has none. */
const char *emsg_text(Session *S, uint8_t code);
#endif
