/* The help file (BASIC.HLP): its keys found, its topics read. How it works: helpfile.c. */
#ifndef UI_HELPFILE_H
#define UI_HELPFILE_H
#include "base.h"
#include "plat/pfil.h"

enum : uint16_t { HLF_NONE = 0xFFFF };

typedef struct {
    char path[PFIL_PATH];
    uint8_t *tab; /* the file but its topics' data: header, tables, keys, dictionary */
    uint8_t *work; /* the dictionary, a topic's text, then its compressed bytes */
    uint32_t work_cap;
    uint32_t off_topics, off_keys, off_pool, off_dict, off_data;
    uint16_t ntopics, nkeys, dict_len;
    uint16_t contents, index, using_help, status, survival;
    uint16_t nnav; /* the topics before it: Ctrl+F1 steps through them */
} HelpFile;

/* path opened and its tables read; false: not there, or not a help file. */
bool hlf_open(HelpFile *f, const char *path);
void hlf_close(HelpFile *f);
/* The topic with key (any case, spaces collapsed, len bytes), HLF_NONE if none. */
uint16_t hlf_find(const HelpFile *f, const char *key, uint32_t len);
/* Topic id's text, *len its bytes: valid till the next hlf_read; nullptr if unreadable. */
const uint8_t *hlf_read(HelpFile *f, uint16_t id, uint32_t *len);
#endif
