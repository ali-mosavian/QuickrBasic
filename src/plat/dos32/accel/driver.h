/* A 3D accelerator's driver: the ABI between DOS's fixed-function player (accel/player.c) and a
 * card's driver, a .DRV file in lib/drivers/ that is loaded only for the card found (accel/load.c).
 *
 * # How it works
 * A driver file is a header, then an image (util/image.h) whose entry is accel_init. The header
 * is "QXD" and 1, the ABI, the count of the PCI IDs it drives, then each as vendor << 16 | device,
 * a double word each, little-endian. accel_init is given the host's services (AccelHost) and
 * returns the driver's table (AccelDriver).
 *
 * The player turns gfx's GPU list (qx/qxlist.h's FIXED) into the table's calls. It keeps each
 * texture's texels and gives the driver textures of its sizes only, cutting triangles where they
 * cross a tile or a wrap, and sends state when it changes. A driver keeps nothing of the list's.
 *
 * A newer ABI adds to the tables' ends; each side reads the other's size first.
 */
#ifndef PLAT_DOS32_ACCEL_DRIVER_H
#define PLAT_DOS32_ACCEL_DRIVER_H
#include "plat/dos32/pci.h"
#include "qx/qx.h"

#define ACCEL_CALL QX_CALL

enum { ACCEL_ABI = 1, ACCEL_HEAD = 12 };

/* The target shown, made by open; the slots a driver takes, 1 up to these, textures' and
   targets' each their own. */
enum { ACCEL_SCREEN = 0, ACCEL_TEXTURES = 1024, ACCEL_TARGETS = 64 };

typedef struct {
    int32_t x, y, w, h;
} AccelRect;

/* An open card: its textures' sides powers of two from 1 << least up to side, the longer at most
   1 << aspect times the shorter, memory their bytes in all; its screen's refresh in millihertz, 0
   unknown. */
typedef struct {
    uint16_t side;
    uint8_t least, aspect;
    uint32_t memory;
    uint32_t millihz;
} AccelCard;

/* What a pixel is: the colour alone; the texture's texel times the colour; or the colour, its
   alpha the texel's times its own (MASK). */
typedef enum : uint8_t { ACCEL_COLOUR = 1, ACCEL_TEXEL, ACCEL_MASK } AccelSource;

/* KEY: drawn where the texel's alpha is set, the colour alone too; LINEAR: the four texels round
   the point mixed; WRAP_S, WRAP_T: texels repeat across the texture, else its edge's are kept past
   it. */
enum { ACCEL_KEY = 1, ACCEL_LINEAR = 2, ACCEL_WRAP_S = 4, ACCEL_WRAP_T = 8 };

/* A pixel's colour c goes in as c; c times alpha a over what is there (MIX); c times a added
   (ADD); KEEP writes none. a is the state's alpha times the colour's, each over 255. */
typedef enum : uint8_t { ACCEL_NONE, ACCEL_MIX, ACCEL_ADD, ACCEL_KEEP } AccelBlend;

/* TEST: drawn only where nearer than the target's depth; WRITE: the target's depth set. */
enum { ACCEL_DEPTH_TEST = 1, ACCEL_DEPTH_WRITE = 2 };

/* The draws': the texture, a slot (0 none), its source and flags, the blend, its share, depth. */
typedef struct {
    uint32_t texture;
    AccelSource source;
    uint8_t flags;
    AccelBlend blend;
    uint8_t alpha;
    uint8_t depth;
} AccelState;

/* x, y: the target's pixels, a pixel's centre at whole numbers; z: its depth, 0 to 1, greater
   nearer; q: 1 over perspective's w, 1 without; s, t: the texture's texels, not over w; colour:
   &HAARRGGBB. */
typedef struct {
    float x, y, z, q, s, t;
    uint32_t colour;
} AccelVertex;

/* What the host lends a driver: a card's configuration space, physical memory mapped (null, not;
   each card's once), I/O ports, memory (zeroed; null, none left) and a clock in microseconds. */
typedef struct {
    uint32_t size, abi;
    uint32_t(ACCEL_CALL *pci_read)(const PciFunction *card, uint8_t reg);
    void(ACCEL_CALL *pci_write)(const PciFunction *card, uint8_t reg, uint32_t v);
    volatile uint8_t *(ACCEL_CALL *map)(uint32_t phys, uint32_t bytes);
    uint8_t(ACCEL_CALL *inb)(uint16_t port);
    void(ACCEL_CALL *outb)(uint16_t port, uint8_t v);
    uint32_t(ACCEL_CALL *ind)(uint16_t port);
    void(ACCEL_CALL *outd)(uint16_t port, uint32_t v);
    void *(ACCEL_CALL *alloc)(uint32_t bytes);
    void(ACCEL_CALL *free)(void *p);
    uint64_t(ACCEL_CALL *clock_us)(void);
} AccelHost;

typedef struct {
    uint32_t size, abi;
    const char *name;
    /* The card readied, showing a w by h screen in the VGA's place, black, its depth buffer
       cleared: what it is to *out. False, it cannot, the VGA's as it was: no mode holds w by h, or
       it does not answer. */
    bool(ACCEL_CALL *open)(const PciFunction *card, uint16_t w, uint16_t h, AccelCard *out);
    /* The VGA's screen shown again, the card stopped. */
    void(ACCEL_CALL *close)(void);
    /* Texture slot made w by h, within the card's limits, its texels undefined: false, no room. */
    bool(ACCEL_CALL *texture)(uint32_t slot, uint16_t w, uint16_t h);
    /* Texels r of texture slot from texels, ARGB1555 (A, KEY's), pitch texels a row; r.x and
       r.w even. */
    void(ACCEL_CALL *upload)(uint32_t slot, AccelRect r, const uint16_t *texels, uint32_t pitch);
    void(ACCEL_CALL *forget)(uint32_t slot);
    /* Target slot made w by h, black, a depth buffer with it, cleared, when depth: false, no room. */
    bool(ACCEL_CALL *target)(uint32_t slot, uint16_t w, uint16_t h, bool depth);
    void(ACCEL_CALL *drop)(uint32_t slot);
    /* The draws after go into target slot, within clip. */
    void(ACCEL_CALL *aim)(uint32_t slot, AccelRect clip);
    void(ACCEL_CALL *state)(const AccelState *s);
    /* n vertices, three a triangle. */
    void(ACCEL_CALL *triangles)(const AccelVertex *v, uint32_t n);
    /* The screen's pixels drawn shown, at the display's retrace with vsync; the draws after go into
       the next frame's, whose pixels are undefined. */
    void(ACCEL_CALL *show)(bool vsync);
    /* The next frame's pixels made the shown one's. */
    void(ACCEL_CALL *carry)(void);
    /* Pixels r of target slot, as drawn so far, into out: bytes R, G, B and A each, rows top
       first; A 255 where the target holds none. */
    void(ACCEL_CALL *read)(uint32_t slot, AccelRect r, uint8_t *out);
} AccelDriver;

typedef const AccelDriver *ACCEL_CALL AccelInit(const AccelHost *host);
#endif
