/* errno and the codes the JS host's file calls return, negated (wasm.mjs). */
#ifndef ERRNO_H
#define ERRNO_H
extern int errno;
enum {
    ENOENT = 2,
    EBADF = 9,
    EACCES = 13,
    EEXIST = 17,
    ENOTDIR = 20,
    EISDIR = 21,
    EINVAL = 22,
    ENFILE = 23,
    EMFILE = 24,
    ENOSPC = 28,
    ENOTEMPTY = 39
};
#endif
