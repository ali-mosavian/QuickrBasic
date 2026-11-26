/* Native modules' files and code: the interface. How it works: native.c. */
#ifndef CORE_IO_NATIVE_H
#define CORE_IO_NATIVE_H
#include "fault.h"
#include "io/qbxfile.h"
#include "qx/qx.h"

/* A .qbx read: its path, its bytes (kept for the process: a slice may run from them), its header
   (valid false: not a .qbx); its code once loaded (null not yet, or it would not load). */
typedef struct {
    char *path;
    uint8_t *bytes;
    uint32_t size;
    QbxFile qbx;
    bool valid;
    const QxModule *module;
    bool tried;
} NatvFile;

/* A call as the host makes it (vm/ncall.c): the extension's QxCall, the run's stop flag. */
typedef struct {
    QxCall c;
    const volatile uint8_t *stop;
} NatvCall;

/* The .qbx files read, a process's: CHAIN and RUN keep them; the host's table they are given. */
typedef struct Natv {
    NatvFile *files;
    uint32_t nfiles, filecap;
    QxHost host;
    Fault *fault;
    bool running; /* the loaded modules' start called, their stop not yet */
} Natv;

void natv_init(Natv *n, Fault *f);
/* path names a native module: a .qbx, or a part of a package's, FILE.qbx/PART. */
bool natv_is(const char *path);
/* path's part: "" a whole .qbx's. Points into path. */
const char *natv_part(const char *path);
/* path's interface, its file read once: its offset in its file, *file the .qbx's path. Raised: File
   not found, the file or the part; Advanced feature unavailable, not a .qbx or another ABI's. */
uint32_t natv_open(Natv *n, const char *path, const char **file);
/* The .qbx at path is a package: parts alone, no interface of its own. Raised: File not found. */
bool natv_package(Natv *n, const char *path);
/* path's code, its file's loaded once, qx_init_<the file's name> called: null, none for this
   machine, or it would not load. */
const QxModule *natv_module(Natv *n, const char *path);
/* A run starts: each loaded module's stop, if it ran, then its start. */
void natv_start(Natv *n);
/* A run ends: each loaded module's stop, once; gfx's screen closed. */
void natv_stop(Natv *n);
/* Module m's entry part.name (part "": name, an ALIAS's or a module's own), compared without case:
   null none. */
const QxEntry *natv_entry(const QxModule *m, const char *part, const char *name, uint32_t len);
/* Module m's TYPE part.name: its bytes, 0 none. */
uint32_t natv_type(const QxModule *m, const char *part, const char *name, uint32_t len);
#endif
