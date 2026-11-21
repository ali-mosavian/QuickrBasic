/* The prototype's ABI, a subset of docs/native-extensions-design.md's: the host's table, a call,
   the module's table. */
#ifndef QX_H
#define QX_H
#include <stdint.h>

#if defined(__i386__)
#define QX_CALL __attribute__((cdecl))
#else
#define QX_CALL
#endif
#if defined(_WIN32)
#define QX_EXPORT __declspec(dllexport)
#else
#define QX_EXPORT __attribute__((visibility("default")))
#endif

enum { QX_ABI = 1 };

typedef struct QxCall QxCall;
typedef void QX_CALL QxFn(QxCall *c);

typedef struct {
    const char *ptr;
    uint32_t len;
} QxStr;

typedef struct {
    void *data;
    uint32_t count, elem;
    int32_t lbound;
} QxArray;

typedef union {
    int32_t i;
    float f;
    double d;
    QxStr s;
    void *ref;
    QxArray *a;
} QxArg;

typedef struct {
    uint32_t rate;
    bool stereo;
    void (QX_CALL *render)(int16_t *out, int32_t frames);
} QxAudio;

typedef struct QxHost {
    uint32_t size, abi;
    void *(QX_CALL *alloc)(uint32_t bytes);
    void (QX_CALL *result_str)(QxCall *c, const char *p, uint32_t len);
    bool (QX_CALL *audio_open)(QxAudio *a);
    void (QX_CALL *say)(const char *p, uint32_t len); /* the prototype's: PRINT */
} QxHost;

struct QxCall {
    const QxHost *host;
    const QxArg *arg;
    QxArg result;
    uint8_t error;
};

typedef struct {
    const char *name, *kinds;
    QxFn *fn;
} QxEntry;

typedef struct {
    const char *name;
    uint32_t bytes;
} QxType;

typedef struct {
    uint32_t size, abi;
    const QxEntry *entries;
    uint32_t nentries;
    const QxType *types;
    uint32_t ntypes;
    void (QX_CALL *start)(void);
    void (QX_CALL *stop)(void);
} QxModule;
#endif
