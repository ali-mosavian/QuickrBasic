/* The runtime's device on DOS32: llrm's own (dev/dos, its asm), and what its OS layer and start-up
 * give it, from this platform's.
 *
 * # How it works
 * The tick is the timer interrupt's (tick.asm), so a wait for the music only waits.
 */
#include "qbrt/qbrt.h"
#include "llrm_os.h"
#include "plat/pfil.h"
#include "plat/pkey.h"
#include "plat/psys.h"

long llrm_os_write_file(const short handle, const unsigned char *const p, const unsigned n)
{
    uint32_t put;
    return pfil_write((PfilHandle)handle, p, n, &put) ? -1 : (long)put;
}

void llrm_os_exit(const unsigned char code) { psys_exit(code); }

unsigned char llrm_os_console_key_ready(void) { return pkey_ready(); }

unsigned char llrm_os_console_read_key(void) { return (unsigned char)pkey_wait(); }

void qb_sincos(const double *const angle, double *const sine, double *const cosine)
{
    __asm__("fsincos" : "=t"(*cosine), "=u"(*sine) : "0"(*angle));
}

void qbrt_device(void) {}

void qbrt_realtime(const bool yes) {}

void qbrt_tick_wait(void) {}
