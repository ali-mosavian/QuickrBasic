/* A 3D accelerator's driver: the ABI between DOS's fixed-function player (accel/player.c) and a
 * card's driver, a .DRV file in lib/drivers/ that is loaded only for the card found (accel/load.c).
 *
 * # How it works
 * A driver file is an i386 DLL (util/image.h) with two exports. accel_ids is the PCI IDs it
 * drives, each ACCEL_ID's double word, 0 after the last. accel_init is given the host's services
 * (AccelHost) and returns the driver's table (AccelDriver).
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

/* ABI 2 adds indexed textures and their palette; 3, write combining; 4, polygons; 5, a palette's
   entries set in part; 6, a corner's reach; 7, a texture's levels. */
enum { ACCEL_ABI = 7 };

/* A card's PCI ID as accel_ids holds it. */
#define ACCEL_ID(vendor, device) ((uint32_t)(vendor) << 16 | (uint32_t)(device))

/* The target shown, made by open; the slots a driver takes, 1 up to these, textures' and
   targets' each their own. */
enum { ACCEL_SCREEN = 0, ACCEL_TEXTURES = 1024, ACCEL_TARGETS = 64 };

/* A polygon's corners at most. */
enum { ACCEL_SIDES = 12 };

typedef struct {
    int32_t x, y, w, h;
} AccelRect;

/* An open card: its textures' sides powers of two from 1 << least up to side, the longer at most
   1 << aspect times the shorter, memory their bytes in all; its screen's refresh in millihertz, 0
   unknown; how far past a target's sides a corner may lie, in pixels, 0 any. */
typedef struct {
    uint16_t side;
    uint8_t least, aspect;
    uint32_t memory;
    uint32_t millihz;
    uint16_t reach;
} AccelCard;

/* What a pixel is: the colour alone; the texture's texel times the colour; or the colour, its
   alpha the texel's times its own (MASK). */
typedef enum : uint8_t { ACCEL_COLOUR = 1, ACCEL_TEXEL, ACCEL_MASK } AccelSource;

/* KEY: drawn where the texel's alpha is set, or an indexed texel's index is not 0, the colour
   alone too; LINEAR: the four texels round the point mixed; WRAP_S, WRAP_T: texels repeat across
   the texture, else its edge's are kept past it; REPLACE: TEXEL's texel alone, not times the
   colour; MIPMAP: read from the texture's level nearest the texels a pixel steps, LINEAR within
   it, else its first. */
enum {
    ACCEL_KEY = 1,
    ACCEL_LINEAR = 2,
    ACCEL_WRAP_S = 4,
    ACCEL_WRAP_T = 8,
    ACCEL_REPLACE = 16,
    ACCEL_MIPMAP = 32,
};

/* A pixel's colour c goes in as c; c times alpha a over what is there (MIX); c times a added
   (ADD); KEEP writes none. a is the state's alpha times the colour's, each over 255. */
typedef enum : uint8_t { ACCEL_NONE, ACCEL_MIX, ACCEL_ADD, ACCEL_KEEP } AccelBlend;

/* TEST: drawn only where nearer than the target's depth; WRITE: the target's depth set. */
enum { ACCEL_DEPTH_TEST = 1, ACCEL_DEPTH_WRITE = 2 };

/* What a clear fills: the colour, the depth. */
enum { ACCEL_CLEAR_COLOUR = 1, ACCEL_CLEAR_DEPTH = 2 };

/* The draws': the texture, a slot (0 none), its source and flags, the blend, its share, depth. */
typedef struct {
    uint32_t texture;
    AccelSource source;
    uint8_t flags;
    AccelBlend blend;
    uint8_t alpha;
    uint8_t depth;
} AccelState;

/* x, y: the target's pixels, a pixel's centre at halves; z: its depth, 0 to 1, greater
   nearer; q: 1 over perspective's w, 1 without; s, t: the texture's texels, not over w; colour:
   &HAARRGGBB. */
typedef struct {
    float x, y, z, q, s, t;
    uint32_t colour;
} AccelVertex;

/* A VESA mode: its size, its bytes a row, its frame buffer's physical address. */
typedef struct {
    uint16_t w, h;
    uint32_t pitch, base;
} AccelMode;

/* What the host lends a driver: a card's configuration space, physical memory mapped (null, not;
   each card's once), I/O ports, memory (zeroed; null, none left), a clock in microseconds, the
   VGA's VESA modes, and the environment's variables. */
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
    /* The smallest mode of bits a pixel with a linear frame buffer holding w by h set, to *out:
       false, none. The host's text screen is shown again after close, or an open that fails. */
    bool(ACCEL_CALL *vesa)(uint16_t w, uint16_t h, uint8_t bits, AccelMode *out);
    /* Variable name's value to out, as cap allows with its 0: its length; 0, unset. */
    uint32_t(ACCEL_CALL *env)(const char *name, char *out, uint32_t cap);
    /* Physical bytes from phys write-combined, one range at a time: false, the CPU cannot. Writes
       to them then reach the card late, merged and in any order, until fence sends those made so
       far on, in order. uncombine puts the range back as it was. */
    bool(ACCEL_CALL *combine)(uint32_t phys, uint32_t bytes);
    void(ACCEL_CALL *uncombine)(void);
    void(ACCEL_CALL *fence)(void);
} AccelHost;

typedef struct {
    uint32_t size, abi;
    const char *name;
    /* The card readied, showing a w by h screen in the VGA's place, black, its depth buffer
       cleared: what it is to *out. False, it cannot: no mode holds w by h, or it does not
       answer. */
    bool(ACCEL_CALL *open)(const PciFunction *card, uint16_t w, uint16_t h, AccelCard *out);
    /* The VGA's screen shown again, the card stopped. */
    void(ACCEL_CALL *close)(void);
    /* Texture slot made w by h, within the card's limits, with levels, each half the last, its
       shorter side 1 << least at least, its texels undefined: false, no room. */
    bool(ACCEL_CALL *texture)(uint32_t slot, uint16_t w, uint16_t h, uint8_t levels);
    /* Texels r of texture slot's level from texels, ARGB1555 (A, KEY's), pitch texels a row;
       r.x and r.w even. An indexed slot's texel is its index, the low byte; r.x and r.w
       multiples of 4. */
    void(ACCEL_CALL *upload)(
        uint32_t slot,
        uint8_t level,
        AccelRect r,
        const uint16_t *texels,
        uint32_t pitch
    );
    void(ACCEL_CALL *forget)(uint32_t slot);
    /* Target slot made w by h, black, a depth buffer with it, cleared, when depth: false, no room. */
    bool(ACCEL_CALL *target)(uint32_t slot, uint16_t w, uint16_t h, bool depth);
    void(ACCEL_CALL *drop)(uint32_t slot);
    /* The draws after go into target slot, within clip. */
    void(ACCEL_CALL *aim)(uint32_t slot, AccelRect clip);
    void(ACCEL_CALL *state)(const AccelState *s);
    /* n polygons from v, polygon i sides[i] corners round, from 3 to ACCEL_SIDES, each drawn as a fan
       from its first corner. */
    void(ACCEL_CALL *fans)(const AccelVertex *v, const uint8_t *sides, uint32_t n);
    /* The screen's pixels drawn shown, at the display's retrace with vsync; the draws after go into
       the next frame's, whose pixels are undefined. */
    void(ACCEL_CALL *show)(bool vsync);
    /* The next frame's pixels made the shown one's. */
    void(ACCEL_CALL *carry)(void);
    /* Pixels r of target slot, as drawn so far, into out: bytes R, G, B and A each, rows top
       first; A 255 where the target holds none. */
    void(ACCEL_CALL *read)(uint32_t slot, AccelRect r, uint8_t *out);
    /* The aimed target's clip filled, as the card fills fastest: its colour argb, its depth 0,
       far, as what has them. */
    void(ACCEL_CALL *clear)(uint32_t argb, uint8_t what);
    /* Texture slot made w by h with levels, as texture, a byte a texel: drawn as the palette's
       colour for its index. False, no room. */
    bool(ACCEL_CALL *indexed)(uint32_t slot, uint16_t w, uint16_t h, uint8_t levels);
    /* Of the 256 colours, &HRRGGBB, an indexed texel draws as, n from first set from colours',
       the others kept. */
    void(ACCEL_CALL *palette)(const uint32_t *colours, uint32_t first, uint32_t n);
} AccelDriver;

typedef const AccelDriver *ACCEL_CALL AccelInit(const AccelHost *host);
#endif
