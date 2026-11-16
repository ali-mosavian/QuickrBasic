/* noaslr COMMAND [ARG...]: COMMAND with ASLR off, for tools/bench.py. Under Rosetta an interpreter's
   count moved by up to a third with the layout; off, by 0.15%. */
#include <spawn.h>
#include <stdio.h>
#include <sys/wait.h>

#ifndef _POSIX_SPAWN_DISABLE_ASLR
#define _POSIX_SPAWN_DISABLE_ASLR 0x0100
#endif

extern char **environ;

int main(const int argc, char **const argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: noaslr COMMAND [ARG...]\n");
        return 2;
    }

    posix_spawnattr_t a;
    posix_spawnattr_init(&a);
    posix_spawnattr_setflags(&a, _POSIX_SPAWN_DISABLE_ASLR);
    pid_t pid;

    if (posix_spawnp(&pid, argv[1], nullptr, &a, argv + 1, environ)) {
        perror("noaslr");
        return 127;
    }

    int st;
    waitpid(pid, &st, 0);
    return WIFEXITED(st) ? WEXITSTATUS(st) : 1;
}
