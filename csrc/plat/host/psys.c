/* The process on the host.
 *
 * # How it works
 * argv, environ, SIGINT as the STOP key, SHELL through /bin/sh.
 */
#include <crt_externs.h>
#include <mach-o/dyld.h>
#include <signal.h>
#include <spawn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include "plat/host/host.h"
#include "plat/psys.h"

extern char **environ;

static volatile uint8_t *stop_flag;
static uint8_t stop_bit;

static void on_stop(int sig) { *stop_flag |= stop_bit; }

void psys_init(volatile uint8_t *const stop, const uint8_t bit)
{
    stop_flag = stop;
    stop_bit = bit;
    signal(SIGINT, on_stop);
}

void psys_exit(const uint8_t code) { exit(code); }

/* Appends s to buf (cap bytes, n used) up to its 0, which is kept out of n. */
static uint32_t append(char *const buf, const uint32_t cap, uint32_t n, const char *const s)
{
    for (uint32_t i = 0; s[i] && n + 1 < cap; i++)
        buf[n++] = s[i];

    buf[n] = 0;
    return n;
}

uint32_t psys_cmdline(char *const buf, const uint32_t cap)
{
    const int argc = *_NSGetArgc();
    char **const argv = *_NSGetArgv();
    uint32_t n = 0;
    buf[0] = 0;

    for (int i = 1; i < argc; i++) {
        if (i > 1)
            n = append(buf, cap, n, " ");

        n = append(buf, cap, n, argv[i]);
    }

    return n;
}

/* DOS's environment as DOSBox starts it, not the host's: programs see the same either way. */
static const char COMSPEC[] = "Z:\\COMMAND.COM";
static const char DOS_ENV[] = "COMSPEC=Z:\\COMMAND.COM\0PATH=Z:\\\0";

uint32_t psys_environ(char *const buf, const uint32_t cap)
{
    const uint32_t n = sizeof DOS_ENV < cap ? sizeof DOS_ENV : cap;
    memcpy(buf, DOS_ENV, n);
    buf[n - 1] = 0;
    return n;
}

uint32_t psys_self(char *const buf, const uint32_t cap)
{
    uint32_t size = cap;

    if (_NSGetExecutablePath(buf, &size))
        return 0;

    return (uint32_t)strlen(buf);
}

/* COMSPEC's shell is /bin/sh, its environment env; any other program does not run. */
bool psys_exec(
    const char *const path,
    const char *const tail,
    const uint8_t len,
    const char *const env,
    const uint32_t envlen
)
{
    if (strcasecmp(path, COMSPEC) && strcasecmp(path, COMSPEC + 3))
        return false;

    static const char run[] = " /C ";
    char cmd[256];
    const uint32_t skip =
        len >= strlen(run) && !strncasecmp(tail, run, strlen(run)) ? strlen(run) : 0;
    memcpy(cmd, tail + skip, len - skip);
    cmd[len - skip] = 0;
    char *vars[64];
    uint32_t nvars = 0;

    for (const char *v = env; *v && nvars + 1 < sizeof vars / sizeof *vars; v += strlen(v) + 1)
        vars[nvars++] = (char *)v;

    vars[nvars] = nullptr;
    char *const argv[] = {"/bin/sh", "-c", cmd, nullptr};

    char *const shell[] = {"/bin/sh", nullptr};

    pid_t pid;

    if (posix_spawn(&pid, "/bin/sh", nullptr, nullptr, len ? argv : shell, vars))
        return false;

    int status;
    return waitpid(pid, &status, 0) == pid;
}

/* ---- the ports programs use: the VGA's DAC and status, the 8042's command to type a key ---- */

enum : uint16_t {
    PORT_KBD_DATA = 0x60,
    PORT_KBD_CMD = 0x64,
    PORT_DAC_READ = 0x3C7,
    PORT_DAC_WRITE = 0x3C8,
    PORT_DAC_DATA = 0x3C9,
    PORT_STATUS = 0x3DA,
};
enum : uint8_t { KBD_WRITE_OUTPUT = 0xD2, RETRACE = 8 };

static struct {
    uint8_t dac[256][3];
    uint8_t index, part;
    uint8_t status;
    bool kbd_write; /* the next data byte is a key typed */
} ports;

uint8_t psys_inp(const uint16_t port)
{
    switch (port) {
    case PORT_DAC_DATA: {
        const uint8_t v = ports.dac[ports.index][ports.part];

        if (++ports.part == 3) {
            ports.part = 0;
            ports.index++;
        }

        return v;
    }
    case PORT_STATUS:
        return ports.status ^= RETRACE; /* retrace starts and ends at each look */
    default:
        return 0xFF;
    }
}

void psys_outp(const uint16_t port, const uint8_t v)
{
    switch (port) {
    case PORT_DAC_READ:
    case PORT_DAC_WRITE:
        ports.index = v;
        ports.part = 0;
        break;

    case PORT_DAC_DATA:
        ports.dac[ports.index][ports.part] = v & 0x3F;

        if (++ports.part == 3) {
            ports.part = 0;
            ports.index++;
        }

        break;

    case PORT_KBD_CMD:
        ports.kbd_write = v == KBD_WRITE_OUTPUT;
        break;

    case PORT_KBD_DATA:
        if (ports.kbd_write)
            host_key_push((uint16_t)(v << 8));

        ports.kbd_write = false;
        break;

    default:
        break;
    }
}
void psys_call(const Addr a) {}
