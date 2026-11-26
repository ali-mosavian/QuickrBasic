/* Where a module's file is: the search path's interface. How it works: locate.c. */
#ifndef CORE_IO_LOCATE_H
#define CORE_IO_LOCATE_H
#include "comp/import.h"
#include "plat/pfil.h"

/* Where to look: the main program's path, BASICLIB's value (null none), the importing file's path;
   the file's extension, null a .bas then a .qbx. */
typedef struct {
    const char *main;
    const char *lib;
    const char *from;
    const char *ext;
} Locate;

/* Module n's file to out. A FROM's (dir not null) may be a package's folder instead: out that,
   *dir set. false: neither found. */
bool locate(const Locate *l, const ImptName *n, char out[static PFIL_PATH], bool *dir);
/* A name FROM gives, n, of the package dir, a folder or a .qbx of parts: its module's file, of
   extension ext (null as Locate's), or the .qbx's part, FILE.qbx/part, to out. false: none. */
bool locate_in(const char *dir, const ImptName *n, const char *ext, char out[static PFIL_PATH]);
#endif
