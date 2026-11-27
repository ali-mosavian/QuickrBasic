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

/* A module's READ place (vm/stmts.c), kept while another module's DATA is read: none before its
   first READ; its DATA's line and first byte (0 none left), the item's offset in its text. */
typedef struct {
    bool set;
    bool before_first;
    uint32_t line, at, pos;
} ModuRead;

/* Each module's: its path, its LF_TOP line (the END before its lines; 0 the main's), as read and
   again at each link; a native module's (a .qbx's: io/native.c); the editor's: its lines changed
   since saved; its run's state: its code run by an IMPORT, its ON ERROR handler's line (0 none), its
   READ place. off: the words its OPTION DISABLE lines made names so far (comp/lexer.h's
   Lexr.off). */
typedef struct {
    uint32_t path; /* an offset in Modu.text */
    uint32_t top;
    uint64_t off[8];
    bool native;
    const struct Dialect
        *dialect;   /* a QBasic program's module of another (io/dialect.h); else null */
    uint16_t file;  /* such a module's lines' file */
    uint32_t clash; /* a name two of such a module's publics spell, case aside; 0 none */
    bool edited;
    bool declared; /* a QBasic module's, its lines declared this link (io/repl.c) */
    bool crosses;  /* a QBasic module's: it imports another dialect's, or one that does */
    bool ran;
    uint32_t handler;
    ModuRead read;
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
    uint32_t epoch, drops; /* bumped by a module added or dropped; by one dropped */
    Fault *fault;          /* Out of memory */
} Modu;

/* Only the main program's module, its path unknown. */
void modu_init(Modu *mu, Fault *f);
/* No files, and no modules but the main program, at path main, its run's ON ERROR and READ kept. */
void modu_clear(Modu *mu, const char *main);
/* The first n modules kept (n at most their count), the main's path main; their run's state kept;
   no imports; no files but each module's own, file m module m's. */
void modu_relink(Modu *mu, const char *main, uint32_t n);
/* Module m dropped, those after it a number down, their files' too; no imports. */
void modu_drop(Modu *mu, uint32_t m);
/* The first module before n that no import reaches from the main's or an edited one's; 0 none. */
uint32_t modu_unreached(Modu *mu, uint32_t n);
/* The module at path, added last when new (paths compared without case): its number. Out of memory:
   past 65536 modules. */
uint32_t modu_module(Modu *mu, const char *path);
/* The module at path, modu_count none. */
uint32_t modu_find(const Modu *mu, const char *path);
uint32_t modu_count(const Modu *mu);
const char *modu_module_path(const Modu *mu, uint32_t m);
/* Module from imports module to. */
void modu_import(Modu *mu, uint32_t from, uint32_t to);
/* Each module's crosses, none declared yet; modu_order's order made first. */
void modu_crossings(Modu *mu);
/* Module m's lines start past line, its LF_TOP line, as read. */
void modu_top(Modu *mu, uint32_t m, uint32_t line);
/* Module m's LF_TOP line as read; the main's 0. */
uint32_t modu_top_of(const Modu *mu, uint32_t m);
/* Module m's run state (the main's when m is past them). */
ModuMod *modu_mod(Modu *mu, uint32_t m);
/* No module's code run, no handler: a run's start. */
void modu_run_reset(Modu *mu);
/* The modules in the order their declarations go: each after those it imports, the main's last;
   in a cycle, the first met first. modu_count of them. */
const uint32_t *modu_order(Modu *mu);
/* modu_order's last answer. */
static inline const uint32_t *modu_ordered(const Modu *const mu) { return mu->order; }
/* File number file is path's, a part of module m. */
void modu_file(Modu *mu, uint32_t file, const char *path, uint32_t m);
/* File number file's path, "" unknown; its module, 0 unknown. */
const char *modu_path(const Modu *mu, uint32_t file);
uint32_t modu_of(const Modu *mu, uint32_t file);
#endif
