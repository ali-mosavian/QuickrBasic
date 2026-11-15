/* llrm's QB runtime in the interpreter (csrc/qbrt): it draws, and its errors are QBasic's. */
#include "session.h"
#include "io/errmsg.h"
#include "io/repl.h"
#include "qbrt/qbrt.h"
#include "unit.h"

/* The headless device's framebuffer (llrm's dev/host/video.h). */
unsigned host_mode(void);
unsigned host_width(void);
const uint8_t *host_pixels(void);

TEST(runtime_draws_a_point)
{
    gfx_screen(13);
    B_N1I2(10, 20);
    B_PSTC(4);
    CHECK(host_mode() == 0x13 && host_pixels()[20 * host_width() + 10] == 4);
    CHECK(B_PNI2(10, 20) == 4);
    gfx_screen(0);
}

static void screen5(Session *const S) { gfx_screen(5); }

/* A runtime error comes back as QBasic's, not the process's end: SCREEN 5 is Illegal function
   call. */
TEST(runtime_error_raised_as_qbasic)
{
    CHECK(repl_call(S, screen5) == E_USR);
    CHECK(!strcmp(emsg_text(S->vm.err.user, E_USR), "Illegal function call"));
}
