/* poll: standard input is always ready, its read says whether a byte is there (pkey_poll's). */
#ifndef POLL_H
#define POLL_H
struct pollfd {
    int fd;
    short events, revents;
};
enum { POLLIN = 1 };
static inline int poll(struct pollfd *const p, const unsigned n, const int ms)
{
    for (unsigned i = 0; i < n; i++)
        p[i].revents = POLLIN;

    return (int)n;
}
#endif
