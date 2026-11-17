/* The process on the host.
 *
 * # How it works
 * argv, environ, SIGINT as the STOP key, SHELL through /bin/sh.
 */
#ifdef __linux__
#include <unistd.h>
#else
#include <crt_externs.h>
#include <mach-o/dyld.h>
#endif
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
static volatile sig_atomic_t stopped;

static void on_stop(int sig)
{
    *stop_flag |= stop_bit;
    stopped = 1;
}

bool host_stop_taken(void)
{
    const bool s = stopped;
    stopped = 0;
    return s;
}

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

#ifdef __linux__
static int args_n;
static char **args;

/* glibc passes main's arguments to the constructors too. */
[[gnu::constructor]]
static void save_args(const int argc, char **const argv)
{
    args_n = argc;
    args = argv;
}
#endif

uint32_t psys_cmdline(char *const buf, const uint32_t cap)
{
#ifdef __linux__
    const int argc = args_n;
    char **const argv = args;
#else
    const int argc = *_NSGetArgc();
    char **const argv = *_NSGetArgv();
#endif
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
#ifdef __linux__
    const ssize_t n = readlink("/proc/self/exe", buf, cap - 1);

    if (n < 0)
        return 0;

    buf[n] = 0;
    return (uint32_t)n;
#else

    uint32_t size = cap;

    if (_NSGetExecutablePath(buf, &size))
        return 0;

    /* in an app: its files are in Contents/Resources, beside Contents/MacOS */
    static const char code[] = "/Contents/MacOS/", data[] = "/Contents/Resources/";
    char *const at = strstr(buf, code);
    const uint32_t n = (uint32_t)strlen(buf);

    if (!at || n + sizeof data - sizeof code >= cap)
        return n;

    memmove(at + sizeof data - 1, at + sizeof code - 1, strlen(at + sizeof code - 1) + 1);
    memcpy(at, data, sizeof data - 1);
    return (uint32_t)strlen(buf);
#endif
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
    host_term_pause(true);
    const bool ran = !posix_spawn(&pid, "/bin/sh", nullptr, nullptr, len ? argv : shell, vars);
    int status;
    const bool done = ran && waitpid(pid, &status, 0) == pid;
    host_term_pause(false);
    return done;
}

void psys_call(const Addr a) {}
