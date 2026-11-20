/* The program's source files and modules' interface. How it works: module.c. */
#ifndef MEM_MODULE_H
#define MEM_MODULE_H
#include "base.h"
#include "fault.h"

typedef struct {
    uint32_t path;   /* its path: an offset in Modu.text */
    uint32_t module; /* its module: 0 the main program's */
} ModuFile;

typedef struct {
    ModuFile *files; /* by number, Line.file */
    uint32_t nfiles, filecap;
    uint32_t *mods; /* each module's path: an offset in Modu.text; [0] the main program's */
    uint32_t nmods, modcap;
    char *text; /* the paths, each 0-ended */
    uint32_t ntext, textcap;
    Fault *fault; /* Out of memory */
} Modu;

void modu_init(Modu *mu, Fault *f);
/* No files, and no modules but the main program, at path main. */
void modu_clear(Modu *mu, const char *main);
/* The module at path, added last when new (paths compared without case): its number. */
uint32_t modu_module(Modu *mu, const char *path);
uint32_t modu_count(const Modu *mu);
const char *modu_module_path(const Modu *mu, uint32_t m);
/* File number file is path's, a part of module m. */
void modu_file(Modu *mu, uint32_t file, const char *path, uint32_t m);
/* File number file's path, "" unknown; its module, 0 unknown. */
const char *modu_path(const Modu *mu, uint32_t file);
uint32_t modu_of(const Modu *mu, uint32_t file);
#endif
