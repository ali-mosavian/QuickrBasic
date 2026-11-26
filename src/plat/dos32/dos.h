/* The DOS32 layer's own: software interrupts and the BIOS data area. Only plat/dos32 includes it. */
#ifndef PLAT_DOS32_DOS_H
#define PLAT_DOS32_DOS_H
#include <cdecl.h>
#include <stdint.h>

typedef struct {
    uint32_t eax, ebx, ecx, edx, esi, edi, flags;
} PdosRegs;

enum { PDOS_CF = 1, PDOS_ZF = 0x40 };

/* The interrupts called. */
enum { INT_VIDEO = 0x10, INT_KEYBOARD = 0x16, INT_DOS = 0x21, INT_DPMI = 0x31, INT_MOUSE = 0x33 };

/* INT 21h's functions (AH). */
enum : uint8_t {
    DOS_READ_KEY = 0x07,
    DOS_INPUT_STATUS = 0x0B,
    DOS_CUR_DRIVE = 0x19,
    DOS_SET_DTA = 0x1A,
    DOS_GET_DATE = 0x2A,
    DOS_SET_DATE = 0x2B,
    DOS_GET_TIME = 0x2C,
    DOS_SET_TIME = 0x2D,
    DOS_FREE = 0x36,
    DOS_MKDIR = 0x39,
    DOS_RMDIR = 0x3A,
    DOS_CHDIR = 0x3B,
    DOS_CREATE = 0x3C,
    DOS_OPEN = 0x3D,
    DOS_CLOSE = 0x3E,
    DOS_READ = 0x3F,
    DOS_WRITE = 0x40,
    DOS_DELETE = 0x41,
    DOS_SEEK = 0x42,
    DOS_ATTR = 0x43,
    DOS_IOCTL = 0x44,
    DOS_CURDIR = 0x47,
    DOS_EXEC = 0x4B,
    DOS_EXIT = 0x4C,
    DOS_FIND_FIRST = 0x4E,
    DOS_FIND_NEXT = 0x4F,
    DOS_RENAME = 0x56,
};
/* INT 16h's (AH). */
enum : uint8_t { KBD_READ = 0x00, KBD_STATUS = 0x01, KBD_READ_EXT = 0x10, KBD_STATUS_EXT = 0x11 };
/* INT 10h's (AH). */
enum : uint8_t {
    VID_MODE = 0x00,
    VID_SHAPE = 0x01,
    VID_CURSOR = 0x02,
    VID_WHERE = 0x03,
    VID_PAGE = 0x05,
    VID_SCROLL = 0x06,
    VID_WRITE = 0x09,
    VID_TELETYPE = 0x0E,
    VID_FONT = 0x11, /* AL=12h: the 8x8 font, rows to fill the scan lines */
    VID_ALT = 0x12,  /* BL=30h: AL the next mode's scan lines, 1 350, 2 400 */
};
/* INT 33h's (AX). */
enum : uint16_t {
    MOUSE_RESET = 0,
    MOUSE_SHOW = 1,
    MOUSE_HIDE = 2,
    MOUSE_STATE = 3,
    MOUSE_PRESS = 5
};
/* DPMI's (AX). */
enum : uint16_t {
    DPMI_SEG_BASE = 0x0006,
    DPMI_DOS_ALLOC = 0x0100,
    DPMI_DOS_FREE = 0x0101,
    DPMI_GET_RM_VECTOR = 0x0200,
    DPMI_SET_RM_VECTOR = 0x0201,
    DPMI_RM_INT = 0x0300,
    DPMI_MEM_INFO = 0x0500,
    DPMI_MEM_ALLOC = 0x0501,
    DPMI_MAP = 0x0800,
};

/* The BIOS data area's: the shift flags, the video mode, the tick count, a character's scan lines. */
enum : uint32_t { BDA_SHIFT = 0x417, BDA_MODE = 0x449, BDA_TICKS = 0x46C, BDA_CHAR_H = 0x485 };

/* AX of function fn (AH) and al. */
static inline uint32_t pdos_fn(const uint8_t fn, const uint8_t al)
{
    return (uint32_t)fn << 8 | al;
}
/* A 32-bit value a call returns in two registers' low words. */
static inline uint32_t pdos_pair(const uint32_t hi, const uint32_t lo)
{
    return (hi & 0xFFFF) << 16 | (lo & 0xFFFF);
}

/* DPMI's real-mode call structure (0300h). */
typedef struct __attribute__((packed)) {
    uint32_t edi, esi, ebp, reserved, ebx, edx, ecx, eax;
    uint16_t flags, es, ds, fs, gs, ip, cs, sp, ss;
} PdosRmRegs;
static_assert(sizeof(PdosRmRegs) == 0x32);

/* Interrupt `vector` with r's registers, which it returns; the flags returned too. */
CDECL uint32_t pdos_int(int vector, PdosRegs *r);

/* A DOS memory block: its real-mode segment, its selector, its DS address. */
typedef struct {
    uint16_t seg, sel;
    uint8_t *at;
} PdosBlock;
/* A block of size bytes below 1 MB (dosmem.c): whether there was room. */
bool pdos_alloc(uint32_t size, PdosBlock *b);
void pdos_free(const PdosBlock *b);
/* Real-mode interrupt `vector` with rm's registers, which it returns: whether DPMI ran it. */
bool pdos_rm_int(uint8_t vector, PdosRmRegs *rm);

/* VBE's controller (dosvbe.c), asked through a DOS block of PDOS_VBE_BLOCK bytes. */
enum { PDOS_VBE_BLOCK = 512 };
typedef struct {
    uint16_t version;
    uint32_t memory; /* video memory's bytes */
} PdosVbe;
/* The controller (4F00h), its modes' numbers to modes, at most most: how many; 0, no VBE. */
uint32_t pdos_vbe_modes(const PdosBlock *b, PdosVbe *card, uint16_t *modes, uint32_t most);
/* Mode's information (4F01h), in b: false, none. */
bool pdos_vbe_mode(const PdosBlock *b, uint16_t mode);

/* The environment block (index 0) copied to buf, at most cap bytes; its length. */
CDECL uint32_t pdos_environ(uint32_t index, uint8_t *buf, uint32_t cap);

/* DS's selector. */
CDECL uint16_t pdos_ds(void);
/* CS's selector. */
CDECL uint16_t pdos_cs(void);
/* Selector sel's limit: its segment's last offset. */
CDECL uint32_t pdos_limit(uint32_t sel);

/* The PSP's command tail, its leading blanks skipped, at most max bytes; its length. */
CDECL uint32_t pdos_cmdline(char *buf, uint32_t max);

/* DS's base: a linear address less it is a DS offset. */
static inline uint32_t pdos_ds_base(void)
{
    PdosRegs r = {.eax = DPMI_SEG_BASE, .ebx = pdos_ds()};

    pdos_int(INT_DPMI, &r);
    return pdos_pair(r.ecx, r.edx);
}

/* The flat segment's byte at linear address `a` (DS is zero-based under DOS/32A). */
static inline volatile uint8_t *pdos_lin(const uint32_t a)
{
    return (volatile uint8_t *)(uintptr_t)a;
}
#endif
