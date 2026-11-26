/* A native module's file, .qbx: its parts' interface. How it works: qbxfile.c. */
#ifndef CORE_IO_QBXFILE_H
#define CORE_IO_QBXFILE_H
#include "base.h"

enum { QBXF_FORMAT = 2 };

/* A .qbx's ABI (its major, the minor it needs) and the slice of the tag asked for (null: none
   for it), pointing into the file's bytes. */
typedef struct {
    uint16_t abi, minor;
    const uint8_t *slice;
    uint32_t nslice;
} QbxFile;

/* The size bytes at data as a .qbx, its slice tagged tag: false, not one. */
bool qbxf_read(const uint8_t *data, uint32_t size, const char *tag, QbxFile *out);
/* The interface of the part named name, compared without case, "" the module's own: its BASIC
   text, *bytes of it; null none, or one outside the file. data is a .qbx (qbxf_read). */
const uint8_t *qbxf_part(const uint8_t *data, uint32_t size, const char *name, uint32_t *bytes);
/* path names a .qbx, by its extension in any case. */
bool qbxf_is(const char *path);
#endif
