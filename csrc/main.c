/* The start: everything set up, then the READY loop. */
#include "boot.h"
#include "io/repl.h"

int main(void)
{
    boot();
    repl_main();
}
