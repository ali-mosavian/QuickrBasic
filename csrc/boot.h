/* Everything set up for a run: the arena, its regions, each module's state. Exits with "Out of
   memory" when the host gives too little. */
#ifndef BOOT_H
#define BOOT_H
#include "base.h"

void boot(Session *S);
#endif
