/* The VM's stand-in, for both hosts: the link's checks, then each entry called as O_NCALL would,
   its arguments laid out as ext.bas declares them. 0: all as expected, else the failing step. */
#include "drive.h"

static int same(const char *a, const char *b)
{
    for (; *a && *a == *b; a++, b++)
        ;

    return *a == *b;
}

/* The link: the entry of that name, its kinds as declared; null none. */
static QxFn *entry(const QxModule *const m, const char *const name, const char *const kinds)
{
    for (uint32_t i = 0; i < m->nentries; i++)
        if (same(m->entries[i].name, name))
            return same(m->entries[i].kinds, kinds) ? m->entries[i].fn : nullptr;

    return nullptr;
}

typedef struct __attribute__((packed)) {
    int16_t now, peak;
} Level; /* TYPE level as the compiler lays it out */

int drive(const QxModule *const m, const QxHost *const h, DriveOut *const out)
{
    QxFn *const add = entry(m, "add", "II"), *const hello = entry(m, "hello", "");
    QxFn *const loud = entry(m, "loud", "IT"), *const bump = entry(m, "bump", "&I");
    QxFn *const shout = entry(m, "shout_text", "S");

    if (!add || !hello || !loud || !bump || !shout || m->abi != QX_ABI)
        return 1;
    if (m->ntypes != 1 || !same(m->types[0].name, "level") || m->types[0].bytes != sizeof(Level))
        return 2;

    QxArg a[2] = {{.i = 40}, {.i = 2}};
    QxCall c = {.host = h, .arg = a};
    add(&c);

    if (c.result.i != 42)
        return 3;

    hello(&c);
    Level l = {.now = 0, .peak = 500};
    a[0].i = 1000;
    a[1].ref = &l;
    loud(&c);

    if (l.now != 1000 || l.peak != 1000)
        return 4;

    int32_t n = 41;
    a[0].ref = &n;
    bump(&c);

    if (n != 42)
        return 5;

    a[0].s = (QxStr){"hello", 5};
    shout(&c);

    if (c.error || !same(out->text, "HELLO"))
        return 6;

    static const char many[100] = "too long";
    a[0].s = (QxStr){many, sizeof many};
    shout(&c);

    return c.error == 5 ? 0 : 7;
}
