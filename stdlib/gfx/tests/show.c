/* gfx's first slice in the Mac's window: CHARLIE's frame (charlie.h) drawn and shown in the IDE's
   place, for QB_SHOT (plat/sdl/window.c) to take; the window ends the process once it has. */
#include <unistd.h>
#include "charlie.h"
#include "plat/pmem.h"

int qb_main(void)
{
    static CharlieCut cuts[CHARLIE_CUTS];
    int32_t views[16];
    Addr lo, hi;
    Natv n;

    if (!pmem_init(0, 0, &lo, &hi) || !charlie_cuts(CHARLIE_SHEET, cuts, CHARLIE_CUTS))
        return 1;

    natv_init(&n, nullptr);

    if (charlie_draw(&n.host, cuts, views) || CALL_ON(&n.host, "display.flip").error)
        return 1;

    for (;;)
        pause();
}
