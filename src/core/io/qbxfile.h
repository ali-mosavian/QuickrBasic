/* A native module's file, .qbx: its parts' interface. How it works: qbxfile.c. */
#ifndef CORE_IO_QBXFILE_H
#define CORE_IO_QBXFILE_H
#include "base.h"

enum { QBXF_FORMAT = 1 };

/* A .qbx's ABI (its major, the minor it needs), its interface's BASIC text, and the slice of the
   tag asked for (null: none for it). Each points into the file's bytes. */
typedef struct {
    uint16_t abi, minor;
    const uint8_t *interface;
    uint32_t ninterface;
    const uint8_t *slice;
    uint32_t nslice;
} QbxFile;

/* The size bytes at data as a .qbx, its slice tagged tag: false, not one, or a part outside it. */
bool qbxf_read(const uint8_t *data, uint32_t size, const char *tag, QbxFile *out);
/* path names a .qbx, by its extension in any case. */
bool qbxf_is(const char *path);
#endif
