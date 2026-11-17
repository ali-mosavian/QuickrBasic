/* The program's graphics screen kept while the editor shows: the runtime's framebuffer is not the
   text pages', so nothing is copied; the display shows the text instead (frame.c). */
#include "plat/view/view.h"
#include "qbrt/qbrt.h"

uint32_t qbrt_shot_size(void) { return 1; }
void qbrt_shot_take(void *const shot) { view_text_shown = true; }
void qbrt_shot_show(const void *const shot) { view_text_shown = false; }
void qbrt_shot_drop(void) { view_text_shown = false; }
