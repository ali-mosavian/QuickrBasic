/* The mouse on the host: none. */
#include "plat/pmou.h"

bool pmou_init(void) { return false; }
void pmou_show(void) {}
void pmou_hide(void) {}
PmouState pmou_state(void) { return (PmouState){0}; }
