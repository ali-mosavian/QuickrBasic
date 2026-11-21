#ifndef UNISTD_H
#define UNISTD_H
#include <sys/types.h>
enum {
    STDIN_FILENO = 0,
    STDOUT_FILENO = 1,
    STDERR_FILENO = 2,
    SEEK_SET = 0,
    SEEK_CUR = 1,
    SEEK_END = 2
};
ssize_t read(int fd, void *p, size_t n);
ssize_t write(int fd, const void *p, size_t n);
off_t lseek(int fd, off_t off, int whence);
int close(int fd);
/* fd is a terminal: a standard handle the host's, else none. */
int isatty(int fd);
int ftruncate(int fd, off_t size);
int unlink(const char *path);
int rmdir(const char *path);
int chdir(const char *path);
char *getcwd(char *buf, size_t size);
#endif
