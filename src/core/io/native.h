/* Native modules' files and code: the interface. How it works: native.c. */
#ifndef CORE_IO_NATIVE_H
#define CORE_IO_NATIVE_H
#include "fault.h"
#include "io/qbxfile.h"
#include "qx/qx.h"

/* A .qbx read: its path, its bytes (kept for the process: a slice may run from them), its parts;
   its code once loaded (null not yet, or it would not load). */
typedef struct {
    char *path;
    uint8_t *bytes;
    uint32_t size;
    QbxFile qbx;
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
/* The .qbx at path, read once; its interface's offset in it. Raised: File not found; Advanced
   feature unavailable, not a .qbx or another ABI's. */
uint32_t natv_open(Natv *n, const char *path);
/* Its code, loaded once, qx_init_<its file's name> called: null, none for this machine, or it
   would not load. */
const QxModule *natv_module(Natv *n, const char *path);
/* A run starts: each loaded module's stop, if it ran, then its start. */
void natv_start(Natv *n);
/* A run ends: each loaded module's stop, once. */
void natv_stop(Natv *n);
/* Module m's entry of that name, compared without case: null none. */
const QxEntry *natv_entry(const QxModule *m, const char *name, uint32_t len);
/* Module m's TYPE of that name: its bytes, 0 none. */
uint32_t natv_type(const QxModule *m, const char *name, uint32_t len);
#endif
