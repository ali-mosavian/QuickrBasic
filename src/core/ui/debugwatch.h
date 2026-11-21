/* The debugger's Watch. How it works: debugwatch.c. */
#ifndef UI_DEBUGWATCH_H
#define UI_DEBUGWATCH_H
#include "ui/ui.h"

/* A Break When's: what stops the run, its value turning true or changing; only at a line (the nth
   of the text's marked lines, its breakpoint), else at any statement of its scope (insp_scope's). */
typedef enum : uint8_t { WK_TRUE, WK_CHANGE } WatchKind;
typedef struct {
    WatchKind kind;
    bool line;
    uint32_t nth, scope;
} BreakWhen;

/* Each watch's row: its expression, " = ", what PRINT shows of it in the stopped procedure's scope,
   or its error's message; a Break When's marked, its value the one a change is from. */
void dwat_eval(Session *S);
/* The watches; watch i's row. */
uint8_t dwat_count(void);
const char *dwat_row(uint8_t i);
/* expr, n long, a watch; a Break When, of how, in place of its line's; false: no room. */
bool dwat_add(const uint8_t *expr, uint32_t n);
bool dwat_break(Session *S, const uint8_t *expr, uint32_t n, BreakWhen how);
/* Watch i dropped. */
void dwat_del(Session *S, uint8_t i);
/* Watch i is a Break When at a line: its mark's place to *nth. */
bool dwat_mark(uint8_t i, uint32_t *nth);
/* A statement starts (DebugHooks.halts): a Break When of any line stops the run. */
bool dwat_halts(Session *S);
/* The nth marked line's breakpoint reached (DebugHooks.breaks): it stops, its Break When's
   condition met, or it has none. */
bool dwat_breaks(Session *S, uint32_t nth);
/* The nth marked line's breakpoint has a Break When. */
bool dwat_conditional(uint32_t nth);
/* uimn.text.marks: the marks moved, a removed one's Break When dropped. */
void dwat_marks(uint32_t nth, int32_t delta);
#endif
