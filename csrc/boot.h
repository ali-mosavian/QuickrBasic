/* Everything set up for a run: the arena, its regions, each module's state. Exits with "Out of
   memory" when the host gives too little. fe's regions are placed with the program's. */
#ifndef BOOT_H
#define BOOT_H
#include "io/repl.h"

void boot(Session *S, const FrontEnd *fe);
#endif
