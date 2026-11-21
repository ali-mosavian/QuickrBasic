/* The debugger's Watch. How it works: debugwatch.c. */
#ifndef UI_DEBUGWATCH_H
#define UI_DEBUGWATCH_H
#include "ui/ui.h"

/* Each watch's row: its expression, " = ", what PRINT shows of it in procedure proc's scope (0 the
   module), or its error's message. */
void dwat_eval(Session *S, uint32_t proc);
/* The watches; watch i's row. */
uint8_t dwat_count(void);
const char *dwat_row(uint8_t i);
/* expr, n long, a watch; false: no room. */
bool dwat_add(const uint8_t *expr, uint32_t n);
/* Watch i dropped. */
void dwat_del(uint8_t i);
#endif
