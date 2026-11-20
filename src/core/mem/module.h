/* The program's source files and modules' interface. How it works: module.c. */
#ifndef MEM_MODULE_H
#define MEM_MODULE_H
#include "base.h"
#include "fault.h"

typedef struct {
    uint32_t path;   /* its path: an offset in Modu.text */
    uint32_t module; /* its module: 0 the main program's */
} ModuFile;

/* A module importing another. */
typedef struct {
    uint32_t from, to;
} ModuEdge;

/* Each module's: its path, its LF_TOP line as read (the END before its lines; 0 the main's). */
typedef struct {
    uint32_t path; /* an offset in Modu.text */
    uint32_t top;
} ModuMod;

typedef struct Modu {
    ModuFile *files; /* by number, Line.file */
    uint32_t nfiles, filecap;
    ModuMod *mods; /* by number: [0] the main program's */
    uint32_t nmods, modcap;
    ModuEdge *edges; /* the imports */
    uint32_t nedges, edgecap;
    uint32_t *order; /* modu_order's */
    uint8_t *seen;
    uint32_t ordercap, seencap;
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
/* Module from imports module to. */
void modu_import(Modu *mu, uint32_t from, uint32_t to);
/* Module m's lines start past line, its LF_TOP line, as read. */
void modu_top(Modu *mu, uint32_t m, uint32_t line);
/* Module m's LF_TOP line as read; the main's 0. */
uint32_t modu_top_of(const Modu *mu, uint32_t m);
/* The modules in the order their declarations go: each after those it imports, the main's last;
   in a cycle, the first met first. modu_count of them. */
const uint32_t *modu_order(Modu *mu);
/* File number file is path's, a part of module m. */
void modu_file(Modu *mu, uint32_t file, const char *path, uint32_t m);
/* File number file's path, "" unknown; its module, 0 unknown. */
const char *modu_path(const Modu *mu, uint32_t file);
uint32_t modu_of(const Modu *mu, uint32_t file);
#endif
