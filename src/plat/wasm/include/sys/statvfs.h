#ifndef SYS_STATVFS_H
#define SYS_STATVFS_H
struct statvfs {
    unsigned long f_bavail, f_frsize;
};
int statvfs(const char *path, struct statvfs *v);
#endif
