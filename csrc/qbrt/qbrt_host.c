/* The runtime's device on the host: llrm's headless one, a framebuffer tests read (video.h). */
#include "qbrt/qbrt.h"
#include "headless.h"

void qbrt_device(void) { headless_reset(); }
