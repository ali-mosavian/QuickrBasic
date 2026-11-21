/* The debugger's call stack. How it works: debugstack.c. */
#ifndef UI_DEBUGSTACK_H
#define UI_DEBUGSTACK_H
#include "ui/ui.h"

/* A frame of the stop (its number the run's, vm/inspect.h): its text line (-1 the immediate's), its
   view (DSTK_NOVIEW none), its row. */
enum : uint32_t { DSTK_NOVIEW = UINT32_MAX };
typedef struct {
    int32_t line;
    uint32_t view;
    char text[DP_TWMAX + 1];
} StackFrame;

/* The stop's frames read, newest first. */
void dstk_read(Session *S);
/* The frames read; frame i of them. */
uint8_t dstk_count(void);
const StackFrame *dstk_at(uint8_t i);
#endif
