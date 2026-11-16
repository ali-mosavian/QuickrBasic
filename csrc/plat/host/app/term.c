/* The interactive host binary's keyboard is the terminal; the tests' binaries leave this out. */
#include "plat/host/host.h"

[[gnu::constructor]]
static void open_term(void)
{
    host_term_open();
}
