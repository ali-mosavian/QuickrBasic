/* The runtime's device on the host: llrm's headless one, a framebuffer tests read (video.h).
   Standard output stays a stream: the screen is the framebuffer only where a test says so. */
#include "qbrt/qbrt.h"
#include "headless.h"

void qbrt_device(void)
{
    headless_reset();
    headless_set_stdout_is_screen(0);
}
