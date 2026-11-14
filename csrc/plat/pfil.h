/* Files and directories. A handle is the host's file number (on DOS32 a DOS handle; FILEATTR's
   2 shows it). Every call that can fail returns PFIL_OK or the host's error as a DOS error code,
   which BASIC's errors map from (chnl_doserr); a host translates its own to the nearest. */
#ifndef PLAT_PFIL_H
#define PLAT_PFIL_H
#include "base.h"

typedef uint16_t PfilHandle;
enum : PfilHandle { PFIL_STDIN = 0, PFIL_STDOUT = 1, PFIL_STDERR = 2 };

/* DOS's error codes. */
enum : uint16_t {
    PFIL_OK = 0,
    PFIL_NOTFOUND = 2, /* file not found */
    PFIL_NOPATH = 3,   /* path not found */
    PFIL_TOOMANY = 4,  /* no handle left */
    PFIL_DENIED = 5,   /* access denied; NAME's target exists */
    PFIL_BADHANDLE = 6,
    PFIL_BADDRIVE = 15,
    PFIL_NOMORE = 18, /* find: no more files */
};

enum : uint8_t { PFIL_READ = 0, PFIL_WRITE = 1, PFIL_RDWR = 2 }; /* open's access */
enum : uint8_t { PFIL_SET = 0, PFIL_CUR = 1, PFIL_END = 2 };     /* seek's origin */
enum : uint8_t {                                                 /* attributes */
                 PFIL_ARDONLY = 1,
                 PFIL_AHIDDEN = 2,
                 PFIL_ASYSTEM = 4,
                 PFIL_ALABEL = 8,
                 PFIL_ADIR = 0x10,
                 PFIL_AARCH = 0x20,
};

uint16_t pfil_open(const char *name, uint8_t access, PfilHandle *h);
/* Created, or truncated, with no attributes; open to read and write. */
uint16_t pfil_create(const char *name, PfilHandle *h);
/* n bytes read into p; *got how many, under n at the end. */
uint16_t pfil_read(PfilHandle h, void *p, uint32_t n, uint32_t *got);
/* n bytes written from p; *put how many, under n when the disk is full. */
uint16_t pfil_write(PfilHandle h, const void *p, uint32_t n, uint32_t *put);
/* To off from origin PFIL_*; *pos the new offset (nullptr: not wanted). */
uint16_t pfil_seek(PfilHandle h, int32_t off, uint8_t origin, uint32_t *pos);
uint16_t pfil_close(PfilHandle h);
/* The handle is a device (the console, NUL), not a disk file. */
bool pfil_isdev(PfilHandle h);

uint16_t pfil_delete(const char *name);
uint16_t pfil_rename(const char *from, const char *to);
uint16_t pfil_mkdir(const char *name);
uint16_t pfil_rmdir(const char *name);
uint16_t pfil_chdir(const char *name);
/* Drive's (0 the current, 1 A:) current directory, no drive nor leading '\', 0-ended; cap
   bytes at most (DOS's 64). */
uint16_t pfil_curdir(uint8_t drive, char *buf, uint32_t cap);
/* The current drive, 0 A:. */
uint8_t pfil_curdrive(void);
/* Drive's (0 the current) free bytes, mod 2^32. */
uint16_t pfil_free(uint8_t drive, uint32_t *bytes);
uint16_t pfil_getattr(const char *name, uint8_t *attr);
uint16_t pfil_setattr(const char *name, uint8_t attr);

/* A directory search's place and its found entry: DOS's DTA, byte for byte. */
typedef struct PACKED {
    uint8_t host[21]; /* the host's search state (DOS's DTA head) */
    uint8_t attr;     /* PFIL_A* */
    uint16_t time, date;
    uint32_t size;
    char name[13]; /* NAME.EXT, 0-ended */
} PfilFind;
static_assert(sizeof(PfilFind) == 43);
/* The first entry matching pattern (wildcards * ?) with attributes within attrs (plain files
   always); PFIL_NOTFOUND / PFIL_NOMORE none. */
uint16_t pfil_first(const char *pattern, uint8_t attrs, PfilFind *f);
uint16_t pfil_next(PfilFind *f);
#endif
