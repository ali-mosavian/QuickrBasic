/* The start's interface. How it works: boot.c. */
#ifndef BOOT_H
#define BOOT_H
#include "io/repl.h"

void boot(Session *S, const FrontEnd *fe);
#endif
