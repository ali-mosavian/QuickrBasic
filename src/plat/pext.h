/* Native code: an extension's slice loaded as this machine's.
 *
 * # How it works
 * A .qbx holds a slice of code for each platform (io/qbxfile.c); this build's is the one tagged
 * pext_tag. pext_load makes it this machine's code and finds its qx_init_<name>: dlopen on the
 * Mac and Linux, a relocated image on DOS, a side module on wasm. A slice imports no symbol, so
 * nothing is resolved but its entry. docs/native-extensions-design.md has each one's steps.
 */
#ifndef PLAT_PEXT_H
#define PLAT_PEXT_H
#include "base.h"
#include "qx/qx.h"

/* This build's tag in a .qbx: "macos", "linux-x86_64", "dos-i386", "wasm32"; "" none. */
extern const char pext_tag[];

/* The slice's bytes loaded (they are the caller's to keep for the process), its entry
   qx_init_<name>: null when it cannot be. */
QxInit *pext_load(const void *slice, uint32_t bytes, const char *name);
#endif
