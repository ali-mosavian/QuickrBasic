/* The prototype's ABI: the host's table and the module's. */
#include <stdint.h>
#if defined(__i386__)
#define QX_CALL __attribute__((cdecl))
#else
#define QX_CALL
#endif

typedef struct QxHost {
    uint32_t size, abi;
    void (QX_CALL *say)(const char *s, uint32_t n);
    void *(QX_CALL *alloc)(uint32_t n);
} QxHost;

typedef struct QxCall {
    const QxHost *host;
    int32_t arg[4];
    int32_t result;
    uint8_t error;
} QxCall;

typedef struct QxEntry {
    const char *name, *kinds;
    void (QX_CALL *fn)(QxCall *c);
} QxEntry;

typedef struct QxModule {
    uint32_t size, abi, nentries;
    const QxEntry *entries;
    void (QX_CALL *render)(int16_t *out, int32_t frames);
} QxModule;
