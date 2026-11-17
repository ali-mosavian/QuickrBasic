#ifndef FCNTL_H
#define FCNTL_H
#include <sys/types.h>
enum { O_RDONLY = 0, O_WRONLY = 1, O_RDWR = 2, O_CREAT = 0x40, O_TRUNC = 0x200, O_APPEND = 0x400 };
int open(const char *path, int flags, ...);
#endif
