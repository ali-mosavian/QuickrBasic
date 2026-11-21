/* The native extension ABI: the one header an extension includes, and the host's side of it
 * (core/vm/native.c). docs/native-extensions-design.md is the design.
 *
 * # How it works
 * The host loads an extension's slice and calls qx_init_<name> with its QxHost table. The
 * extension returns its QxModule: its entries, each a name, the kinds of its arguments and a
 * QxFn. A native DECLARE in the module's interface binds to the entry of its ALIAS (else its own
 * name) when the program links; a call lays its arguments out as QxArgs and calls the entry.
 * tools/qbx.py glue writes the entries from the interface, so an author writes plain C.
 *
 * Kinds, a letter an argument: I a 32-bit INTEGER or LONG, W a SHORT, U a BYTE, F a SINGLE, D a
 * DOUBLE, S a string; R an array, T a record; & before a number passed by reference.
 *
 * Frozen for a major: QxStr, QxArray, QxArg, QxCall, QX_CALL and the kinds. A minor adds to the
 * end of QxHost and QxModule only: each side reads the other's size before a newer field.
 */
#ifndef QX_QX_H
#define QX_QX_H
#include <stdbool.h>
#include <stdint.h>

#if defined(__i386__)
#define QX_CALL __attribute__((cdecl)) /* the ABI's, whatever the core's -mregparm */
#else
#define QX_CALL
#endif
#if defined(_WIN32)
#define QX_EXPORT __declspec(dllexport)
#else
#define QX_EXPORT __attribute__((visibility("default")))
#endif

enum { QX_ABI = 1, QX_MINOR = 0 };

typedef struct QxCall QxCall;
typedef void QX_CALL QxFn(QxCall *c);

/* A string's bytes, read-only, for the call only: the collector may move them after. */
typedef struct {
    const char *ptr;
    uint32_t len;
} QxStr;

/* An array's elements, for the call only: count of them, each elem bytes, the first's index. */
typedef struct {
    void *data;
    uint32_t count, elem;
    int32_t lbound;
} QxArray;

/* An argument, as its kind says. */
typedef union {
    int32_t i; /* I, W, U by value */
    float f;
    double d;
    QxStr s;
    void *ref; /* T, and &: the variable */
    QxArray *a;
} QxArg;

/* What an extension's render is given: the rate asked, then what the host gives. */
typedef struct {
    uint32_t rate;
    bool stereo;
    void(QX_CALL *render)(int16_t *out, int32_t frames);
} QxAudio;

typedef struct QxHost {
    uint32_t size, abi, minor;
    /* Memory the extension keeps: zeroed, null when none is left. */
    void *(QX_CALL *alloc)(uint32_t bytes);
    void(QX_CALL *free)(void *p);
    /* A FUNCTION's string result: its bytes copied. */
    void(QX_CALL *result_str)(QxCall *c, const char *p, uint32_t len);
    /* Ctrl+Break pressed during the call: a long entry looks, and returns. */
    bool(QX_CALL *stopped)(QxCall *c);
    /* A stream of samples, render called from elsewhere (docs/native-extensions-design.md):
       false, none on this machine. */
    bool(QX_CALL *audio_open)(QxAudio *a);
} QxHost;

/* One call: its arguments; a FUNCTION's result; an error raised at the calling line once the
   entry returns (0 none). */
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

/* A TYPE the entries read, and its bytes: checked against the interface's at link. */
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
    void(QX_CALL *start)(void); /* a run starts */
    void(QX_CALL *stop)(void);  /* it ends: END, an error, Break, CHAIN, an edit */
} QxModule;

typedef const QxModule *QX_CALL QxInit(const QxHost *host);
#endif
