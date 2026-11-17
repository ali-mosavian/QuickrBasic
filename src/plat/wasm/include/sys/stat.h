#ifndef SYS_STAT_H
#define SYS_STAT_H
#include <sys/types.h>
struct stat {
    mode_t st_mode;
    off_t st_size;
    time_t st_mtime;
};
enum : mode_t {
    S_IFMT = 0170000,
    S_IFDIR = 0040000,
    S_IFCHR = 0020000,
    S_IFREG = 0100000,
    S_IWUSR = 0200
};
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
int stat(const char *path, struct stat *st);
int fstat(int fd, struct stat *st);
int lstat(const char *path, struct stat *st); /* stat's: the page's files have no links */
int mkdir(const char *path, mode_t mode);
int chmod(const char *path, mode_t mode);
#endif
