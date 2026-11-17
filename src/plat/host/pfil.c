/* Files and directories on the host.
 *
 * # How it works
 * POSIX calls, with DOS's names and error codes.
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <time.h>
#include <unistd.h>
#include "plat/pfil.h"

enum { PATH_CAP = 1024, DRIVE_C = 2 };

enum { DOS_FIRST_FILE = 5 }; /* DOS's handles 0 to 4: the console's, AUX, PRN */

/* Files get DOS's handle numbers: the host's free ones under 5 held. */
[[gnu::constructor]]
static void hold_dos_handles(void)
{
    for (int fd = open("/dev/null", O_RDWR); fd >= 0 && fd < DOS_FIRST_FILE - 1;)
        fd = open("/dev/null", O_RDWR);
}

/* The entry in dir matching name (len bytes at name) but for case, written over name. */
static void match_case(const char *const dir, char *const name, const size_t len)
{
    DIR *const d = opendir(dir);

    if (!d)
        return;

    for (const struct dirent *e; (e = readdir(d));) {
        if (strlen(e->d_name) == len && !strncasecmp(e->d_name, name, len)) {
            memcpy(name, e->d_name, len);
            break;
        }
    }

    closedir(d);
}

/* DOS's name as the host's: no drive, '\' to '/', and on a case-sensitive disk each part as it is
   spelled there, as DOS finds a name in any case. */
static const char *host_path(const char *name, char out[static PATH_CAP])
{
    if (name[0] && name[1] == ':')
        name += 2;

    uint32_t i = 0;

    for (; name[i] && i + 1 < PATH_CAP; i++)
        out[i] = name[i] == '\\' ? '/' : name[i];

    out[i] = 0;
    struct stat st;

    if (!lstat(out, &st))
        return out;

    for (uint32_t at = 0; at < i;) {
        uint32_t end = at;

        while (end < i && out[end] != '/')
            end++;

        if (end > at) {
            const char cut = out[end];
            out[end] = 0;

            if (lstat(out, &st)) {
                char parent[PATH_CAP];
                memcpy(parent, out, at);
                parent[at] = 0;
                match_case(at ? parent : ".", out + at, end - at);
            }

            out[end] = cut;
        }

        at = end + 1;
    }

    return out;
}

static uint16_t dos_error(const int e)
{
    switch (e) {
    case ENOENT:
        return PFIL_NOTFOUND;

    case ENOTDIR:
        return PFIL_NOPATH;

    case EMFILE:
    case ENFILE:
        return PFIL_TOOMANY;

    case EBADF:
        return PFIL_BADHANDLE;

    default:
        return PFIL_DENIED;
    }
}

/* r's error, else PFIL_OK. */
static uint16_t result(const long r) { return r < 0 ? dos_error(errno) : PFIL_OK; }

uint16_t pfil_open(const char *const name, const uint8_t access, PfilHandle *const h)
{
    static const int flags[] = {O_RDONLY, O_WRONLY, O_RDWR};

    char p[PATH_CAP];
    const int fd = open(host_path(name, p), flags[access % 3]);
    *h = (PfilHandle)fd;
    return result(fd);
}

uint16_t pfil_create(const char *const name, PfilHandle *const h)
{
    char p[PATH_CAP];
    const int fd = open(host_path(name, p), O_RDWR | O_CREAT | O_TRUNC, 0644);
    *h = (PfilHandle)fd;
    return result(fd);
}

uint16_t pfil_read(const PfilHandle h, void *const p, const uint32_t n, uint32_t *const got)
{
    const ssize_t r = read(h, p, n);
    *got = r < 0 ? 0 : (uint32_t)r;
    return result(r);
}

uint16_t pfil_write(const PfilHandle h, const void *const p, const uint32_t n, uint32_t *const put)
{
    if (!n) { /* DOS: a write of 0 bytes truncates the file at its place */
        *put = 0;
        return result(ftruncate(h, lseek(h, 0, SEEK_CUR)) < 0 && errno != EINVAL ? -1 : 0);
    }

    const ssize_t r = write(h, p, n);
    *put = r < 0 ? 0 : (uint32_t)r;
    return result(r);
}

uint16_t pfil_seek(const PfilHandle h, const int32_t off, const uint8_t origin, uint32_t *const pos)
{
    static const int whence[] = {SEEK_SET, SEEK_CUR, SEEK_END};

    const off_t r = lseek(h, off, whence[origin % 3]);

    if (pos && r >= 0)
        *pos = (uint32_t)r;

    return result(r);
}

uint16_t pfil_close(const PfilHandle h) { return result(close(h)); }

/* The printer's pages: LPT1.OUT, appended to. */
uint16_t pfil_printer(PfilHandle *const h)
{
    char p[PATH_CAP];
    const int fd = open(host_path("LPT1.OUT", p), O_WRONLY | O_CREAT | O_APPEND, 0644);
    *h = (PfilHandle)fd;
    return result(fd);
}

bool pfil_isdev(const PfilHandle h)
{
    struct stat st;
    return !fstat(h, &st) && S_ISCHR(st.st_mode);
}

/* The call's error on name; a missing file's directory missing is Path not found. */
static uint16_t on_name(const int r, const char *const p)
{
    if (r >= 0)
        return PFIL_OK;

    const uint16_t err = dos_error(errno);

    if (err != PFIL_NOTFOUND)
        return err;

    char dir[PATH_CAP];
    strcpy(dir, p);
    char *const slash = strrchr(dir, '/');

    if (!slash)
        return err;

    *slash = 0;
    struct stat st;
    return !*dir || !stat(dir, &st) ? err : PFIL_NOPATH;
}

uint16_t pfil_delete(const char *const name)
{
    char p[PATH_CAP];
    return on_name(unlink(host_path(name, p)), p);
}

/* A directory's call: one not there is Path not found, as DOS says. */
static uint16_t on_dir(const int r, const char *const p)
{
    const uint16_t err = on_name(r, p);
    return err == PFIL_NOTFOUND ? PFIL_NOPATH : err;
}

uint16_t pfil_mkdir(const char *const name)
{
    char p[PATH_CAP];
    return on_dir(mkdir(host_path(name, p), 0755), p);
}

uint16_t pfil_rmdir(const char *const name)
{
    char p[PATH_CAP];
    return on_dir(rmdir(host_path(name, p)), p);
}

uint16_t pfil_chdir(const char *const name)
{
    char p[PATH_CAP];
    host_path(name, p);
    return on_dir(chdir(*p ? p : "/"), p);
}

uint16_t pfil_rename(const char *const from, const char *const to)
{
    char a[PATH_CAP], b[PATH_CAP];
    struct stat st;

    if (!stat(host_path(to, b), &st))
        return PFIL_DENIED; /* DOS does not replace */
    return on_name(rename(host_path(from, a), b), a);
}

uint16_t pfil_curdir(const uint8_t drive, char *const buf, const uint32_t cap)
{
    char p[PATH_CAP];

    if (!getcwd(p, sizeof p))
        return dos_error(errno);

    uint32_t n = 0;

    for (const char *s = p + 1; *s && n + 1 < cap; s++)
        buf[n++] = *s == '/' ? '\\' : *s;

    buf[n] = 0;
    return PFIL_OK;
}

uint8_t pfil_curdrive(void) { return DRIVE_C; }

uint16_t pfil_free(const uint8_t drive, uint32_t *const bytes)
{
    struct statvfs v;

    if (statvfs(".", &v))
        return PFIL_BADDRIVE;

    *bytes = (uint32_t)((uint64_t)v.f_bavail * v.f_frsize);
    return PFIL_OK;
}

static uint8_t attributes(const struct stat *const st)
{
    return (uint8_t)((S_ISDIR(st->st_mode) ? PFIL_ADIR : PFIL_AARCH) |
                     (st->st_mode & S_IWUSR ? 0 : PFIL_ARDONLY));
}

uint16_t pfil_getattr(const char *const name, uint8_t *const attr)
{
    char p[PATH_CAP];
    struct stat st;
    const uint16_t err = on_name(stat(host_path(name, p), &st), p);
    *attr = err ? 0 : attributes(&st);
    return err;
}

uint16_t pfil_setattr(const char *const name, const uint8_t attr)
{
    char p[PATH_CAP];
    struct stat st;

    if (stat(host_path(name, p), &st))
        return on_name(-1, p);

    const mode_t mode = attr & PFIL_ARDONLY ? st.st_mode & ~(mode_t)0222 : st.st_mode | S_IWUSR;
    return on_name(chmod(p, mode), p);
}

/* ---- the search: its directory, pattern and place in PfilFind.host ---- */

typedef struct PACKED {
    uint32_t index; /* the next entry's */
    uint8_t attrs;
    uint8_t search; /* searches[]'s */
} Place;
static_assert(sizeof(Place) <= sizeof(((PfilFind *)0)->host));

enum { NSEARCH = 4 };
static struct {
    char dir[PATH_CAP];
    char pattern[16]; /* NAME.EXT, upper case */
} searches[NSEARCH];
static uint8_t next_search;

static char upper(const char c) { return c >= 'a' && c <= 'z' ? (char)(c - 32) : c; }

/* One part (name or extension) against a pattern part, * and ? as DOS's. */
static bool part_matches(const char *p, uint32_t pn, const char *s, uint32_t sn)
{
    for (uint32_t i = 0; i < pn; i++) {
        if (p[i] == '*')
            return true;
        if (i >= sn ? p[i] != '?' : p[i] != '?' && p[i] != upper(s[i]))
            return false;
    }

    return pn >= sn;
}

/* name (8.3, any case) against pattern (upper case). */
static bool matches(const char *const pattern, const char *const name)
{
    const char *const pd = strchr(pattern, '.'), *const nd = strchr(name, '.');
    const uint32_t pn = pd ? (uint32_t)(pd - pattern) : (uint32_t)strlen(pattern);
    const uint32_t nn = nd ? (uint32_t)(nd - name) : (uint32_t)strlen(name);
    const char *const pe = pd ? pd + 1 : "", *const ne = nd ? nd + 1 : "";
    return part_matches(pattern, pn, name, nn) &&
           part_matches(pe, (uint32_t)strlen(pe), ne, (uint32_t)strlen(ne));
}

static bool is_short(const char *const name)
{
    const char *const dot = strchr(name, '.');
    const size_t n = dot ? (size_t)(dot - name) : strlen(name);
    return n && n <= 8 && (!dot || (strlen(dot + 1) <= 3 && !strchr(dot + 1, '.')));
}

static uint16_t dos_time(const struct tm *const t)
{
    return (uint16_t)(t->tm_hour << 11 | t->tm_min << 5 | t->tm_sec / 2);
}

static uint16_t dos_date(const struct tm *const t)
{
    return (uint16_t)((t->tm_year - 80) << 9 | (t->tm_mon + 1) << 5 | t->tm_mday);
}

static uint16_t search(PfilFind *const f)
{
    Place *const at = (Place *)f->host;
    DIR *const d = opendir(searches[at->search].dir);

    if (!d)
        return PFIL_NOPATH;

    uint16_t err = PFIL_NOMORE;
    uint32_t i = 0;

    for (const struct dirent *e; (e = readdir(d));) {
        if (i++ < at->index)
            continue;

        at->index = i;

        if (!is_short(e->d_name) || !matches(searches[at->search].pattern, e->d_name))
            continue;

        char p[PATH_CAP * 2];
        struct stat st;
        snprintf(p, sizeof p, "%s/%s", searches[at->search].dir, e->d_name);

        if (stat(p, &st))
            continue;

        const uint8_t attr = attributes(&st);

        if (attr & PFIL_ADIR && !(at->attrs & PFIL_ADIR))
            continue;

        struct tm tm;
        localtime_r(&st.st_mtime, &tm);
        f->attr = attr;
        f->time = dos_time(&tm);
        f->date = dos_date(&tm);
        f->size = (uint32_t)st.st_size;
        uint32_t n = 0;

        for (; e->d_name[n] && n < sizeof f->name - 1; n++)
            f->name[n] = upper(e->d_name[n]);

        f->name[n] = 0;
        err = PFIL_OK;
        break;
    }

    closedir(d);
    return err;
}

uint16_t pfil_first(const char *const pattern, const uint8_t attrs, PfilFind *const f)
{
    char p[PATH_CAP];
    host_path(pattern, p);
    const uint8_t s = next_search++ % NSEARCH;
    char *const slash = strrchr(p, '/');
    const char *const file = slash ? slash + 1 : p;

    if (slash) {
        *slash = 0;
        strcpy(searches[s].dir, *p ? p : "/");
    } else
        strcpy(searches[s].dir, ".");

    uint32_t n = 0;

    for (; file[n] && n < sizeof searches[s].pattern - 1; n++)
        searches[s].pattern[n] = upper(file[n]);

    searches[s].pattern[n] = 0;
    *(Place *)f->host = (Place){.index = 0, .attrs = attrs, .search = s};

    const uint16_t err = search(f);
    return err == PFIL_NOMORE ? PFIL_NOTFOUND : err;
}

uint16_t pfil_next(PfilFind *const f) { return search(f); }
