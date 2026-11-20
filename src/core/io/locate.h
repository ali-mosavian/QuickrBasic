/* Where a module's file is: the search path's interface. How it works: locate.c. */
#ifndef CORE_IO_LOCATE_H
#define CORE_IO_LOCATE_H
#include "comp/import.h"
#include "plat/pfil.h"

/* Where to look: the main program's path, BASICLIB's value (null none), the importing file's path. */
typedef struct {
    const char *main;
    const char *lib;
    const char *from;
} Locate;

/* Module n's file to out. A FROM's (dir not null) may be a package's folder instead: out that,
   *dir set. false: neither found. */
bool locate(const Locate *l, const ImptName *n, char out[static PFIL_PATH], bool *dir);
/* A name FROM gives, n, of the package in folder dir: its module's file to out. false: none. */
bool locate_in(const char *dir, const ImptName *n, char out[static PFIL_PATH]);
#endif
