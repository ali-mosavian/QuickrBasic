/* Paths: a file's folder, a name joined to one. '/' and '\' both separate, as DOS takes both. */
#ifndef CORE_IO_PATH_H
#define CORE_IO_PATH_H
#include "base.h"

/* path's folder: its bytes up to its last separator or drive's ':', that one included; 0 none. */
uint32_t path_dir(const char *path);
/* dir's first n bytes then name, to out (cap bytes, 0-ended); name alone when it is absolute: a
   separator or a drive first. false: too long. */
bool path_join(char *out, uint32_t cap, const char *dir, uint32_t n, const char *name);
/* A file is there; a folder. */
bool path_exists(const char *path);
bool path_isdir(const char *path);
/* path's "." and "dir/.." parts taken out, in place. */
void path_norm(char *path);
#endif
