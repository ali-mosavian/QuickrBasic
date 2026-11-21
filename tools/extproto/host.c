/* Mac/Linux host: dlopen the slice, call its entries through the table. */
#include "qx.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void say(const char *s, uint32_t n) { printf("say: %.*s\n", (int)n, s); }
static void *alloc(uint32_t n) { return calloc(1, n); }
static const QxHost host = {sizeof host, 1, say, alloc};

int main(int argc, char **argv)
{
    void *const lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!lib) { printf("dlopen: %s\n", dlerror()); return 1; }
    const QxModule *(*init)(const QxHost *) = (const QxModule *(*)(const QxHost *))dlsym(lib, "qx_init_ext");
    const QxModule *const m = init(&host);
    QxCall c = {.host = &host, .arg = {40, 2}};
    for (uint32_t i = 0; i < m->nentries; i++)
        if (!strcmp(m->entries[i].name, "add")) m->entries[i].fn(&c);
    printf("add(40, 2) = %d\n", c.result);
    m->entries[1].fn(&c);
    c.arg[0] = 1000;
    m->entries[2].fn(&c);
    int16_t out[4];
    m->render(out, 4);
    printf("render: %d %d %d %d\n", out[0], out[1], out[2], out[3]);
    return 0;
}
