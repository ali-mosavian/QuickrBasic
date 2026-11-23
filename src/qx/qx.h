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
#define QX_CALL __attribute__((cdecl, regparm(0))) /* cdecl alone keeps -mregparm's registers */
#else
#define QX_CALL
#endif
#if defined(_WIN32)
#define QX_EXPORT __declspec(dllexport)
#else
#define QX_EXPORT __attribute__((visibility("default")))
#endif

enum { QX_ABI = 1, QX_MINOR = 1 };

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

/* A screen asked for (gfx_open), then what the host gives. */
typedef struct {
    uint16_t w, h;         /* the canvas */
    uint8_t depth, flags;  /* 8 bits a pixel; gfx.display's flags */
    bool gpu;              /* given: the host plays lists, else it lends pages */
    uint16_t out_w, out_h; /* given: the pixels the canvas is shown at */
    float hz;              /* given: the refresh, 0 unknown */
} QxScreen;

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
       false, none on this machine. The rate and channels are then the device's. */
    bool(QX_CALL *audio_open)(QxAudio *a);
    /* render held off while what it reads changes, till audio_unlock with the token: nests. */
    uint32_t(QX_CALL *audio_lock)(void);
    void(QX_CALL *audio_unlock)(uint32_t token);
    /* Minor 1. A screen shown in the runtime's place until gfx_close or the run's end: false,
       none of that depth. */
    bool(QX_CALL *gfx_open)(QxScreen *s);
    void(QX_CALL *gfx_close)(void);
    /* The page to draw the next frame in, pitch bytes a row; the last frame's pixels still in it. */
    void *(QX_CALL *gfx_page)(uint32_t *pitch);
    /* The page is shown: at 8 bits with these 256 colours, &HRRGGBB. */
    void(QX_CALL *gfx_show)(const uint32_t *palette);
    /* Microseconds, only ever forward. */
    uint64_t(QX_CALL *clock_us)(void);
    /* The file at path (len bytes) whole, in alloc's memory, *bytes of it: null, none. */
    void *(QX_CALL *file_read)(const char *path, uint32_t len, uint32_t *bytes);
    /* Minor 2, for a screen gfx_open gave a GPU. The list played (qxlist.h): it returns when the
       host is done with the list's memory. */
    void(QX_CALL *gfx_play)(const uint32_t *list, uint32_t words);
    /* w by h pixels at x, y of a texture, as played so far, into out: bytes R, G, B and A, rows
       top first. */
    void(QX_CALL
             *gfx_read)(uint32_t texture, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t *out);
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
