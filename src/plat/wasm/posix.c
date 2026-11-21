/* The POSIX calls the host's pfil.c and pclk's time make, on the JS host's files and clock.
 *
 * # How it works
 * Each is the import of its name (wasm.h), its negated errno set in errno.
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <time.h>
#include <unistd.h>
#include "wasm.h"

int errno;

static int ret(const int r)
{
    if (r >= 0)
        return r;

    errno = -r;
    return -1;
}

int open(const char *const path, const int flags, ...) { return ret(js_fs_open(path, flags)); }
ssize_t read(const int fd, void *const p, const size_t n) { return ret(js_fs_read(fd, p, n)); }
ssize_t write(const int fd, const void *const p, const size_t n)
{
    return ret(js_fs_write(fd, p, n));
}
int close(const int fd) { return ret(js_fs_close(fd)); }
int isatty(const int fd) { return js_fs_isatty(fd); }
int ftruncate(const int fd, const off_t size) { return ret(js_fs_truncate(fd, (double)size)); }
int unlink(const char *const path) { return ret(js_fs_unlink(path)); }
int rmdir(const char *const path) { return ret(js_fs_rmdir(path)); }
int chdir(const char *const path) { return ret(js_fs_chdir(path)); }
int mkdir(const char *const path, const mode_t mode) { return ret(js_fs_mkdir(path)); }
int chmod(const char *const path, const mode_t mode) { return ret(js_fs_chmod(path, (int)mode)); }
int rename(const char *const from, const char *const to) { return ret(js_fs_rename(from, to)); }

off_t lseek(const int fd, const off_t off, const int whence)
{
    const double r = js_fs_seek(fd, (double)off, whence);
    return r < 0 ? ret((int)r) : (off_t)r;
}

char *getcwd(char *const buf, const size_t size)
{
    return ret(js_fs_getcwd(buf, size)) < 0 ? nullptr : buf;
}

typedef struct {
    uint32_t mode, size;
    double mtime;
} JsStat;

static int stat_of(const char *const path, const int fd, struct stat *const st)
{
    JsStat s;

    if (ret(js_fs_stat(path, fd, &s)) < 0)
        return -1;

    *st = (struct stat){.st_mode = s.mode, .st_size = s.size, .st_mtime = (time_t)(s.mtime / 1000)};

    return 0;
}

int stat(const char *const path, struct stat *const st) { return stat_of(path, -1, st); }
int fstat(const int fd, struct stat *const st) { return stat_of(nullptr, fd, st); }
int lstat(const char *const path, struct stat *const st) { return stat(path, st); }

int statvfs(const char *const path, struct statvfs *const v)
{
    *v = (struct statvfs){.f_bavail = 1u << 20, .f_frsize = 512}; /* 512 MB free */
    return 0;
}

/* ---- directories: a search's path and place ---- */

struct DIR {
    char path[1024];
    uint32_t index;
    struct dirent entry;
};

static DIR dirs[4];

DIR *opendir(const char *const path)
{
    struct stat st;

    if (stat(path, &st) || !S_ISDIR(st.st_mode))
        return nullptr;

    for (uint32_t i = 0; i < sizeof dirs / sizeof *dirs; i++)
        if (!dirs[i].path[0]) {
            strncpy(dirs[i].path, path, sizeof dirs[i].path - 1);
            dirs[i].index = 0;
            return &dirs[i];
        }

    errno = EMFILE;
    return nullptr;
}

struct dirent *readdir(DIR *const d)
{
    const int n = js_fs_readdir(d->path, d->index++, d->entry.d_name, sizeof d->entry.d_name - 1);

    if (n < 0)
        return nullptr;

    d->entry.d_name[n] = 0;
    return &d->entry;
}

int closedir(DIR *const d)
{
    d->path[0] = 0;
    return 0;
}

/* ---- time: the host's clock and its local offset; the civil calendar from days since 1970 ---- */

enum { DAY = 86400 };

int clock_gettime(const int clock, struct timespec *const ts)
{
    const double ms = js_now();
    ts->tv_sec = (time_t)(ms / 1000);
    ts->tv_nsec = (long)((ms - (double)ts->tv_sec * 1000) * 1e6);
    return 0;
}

int nanosleep(const struct timespec *const wait, struct timespec *const left) { return 0; }

/* Howard Hinnant's days_from_civil and civil_from_days. */
static int64_t days_from_civil(int64_t y, const int m, const int d)
{
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const int64_t yoe = y - era * 400;
    const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    return era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
}

struct tm *localtime_r(const time_t *const t, struct tm *const tm)
{
    const int64_t s = *t + (int64_t)js_tz((double)*t * 1000) * 60;
    const int64_t days = (s >= 0 ? s : s - (DAY - 1)) / DAY, sec = s - days * DAY;
    const int64_t z = days + 719468, era = (z >= 0 ? z : z - 146096) / 146097;
    const int64_t doe = z - era * 146097;
    const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
    const int mon = (int)(mp < 10 ? mp + 3 : mp - 9);
    const int64_t year = yoe + era * 400 + (mon <= 2);
    *tm = (struct tm){
        .tm_sec = (int)(sec % 60),
        .tm_min = (int)(sec / 60 % 60),
        .tm_hour = (int)(sec / 3600),
        .tm_mday = (int)(doy - (153 * mp + 2) / 5 + 1),
        .tm_mon = mon - 1,
        .tm_year = (int)(year - 1900),
        .tm_wday = (int)((days % 7 + 11) % 7),
    };

    return tm;
}

time_t mktime(struct tm *const tm)
{
    const int64_t local = days_from_civil(tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday) * DAY +
                          tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec;
    return (time_t)(local - (int64_t)js_tz((double)local * 1000) * 60);
}
