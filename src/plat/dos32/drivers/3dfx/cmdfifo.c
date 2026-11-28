#include "plat/dos32/drivers/3dfx/cmdfifo.h"
#include "plat/dos32/drivers/3dfx/setup.h"
#include "plat/dos32/drivers/3dfx/tdfx.h"
#include <stddef.h>

enum : uint32_t {
    /* packets: registers, from the 2nd bit the first's place, from the 16th a bit for each of the
       PICKED after it written; texels to texture space, their words from the 3rd bit; a jump in
       frame memory, its place from the 4th */
    REGISTERS = 4,
    REGISTER_AT = 1,
    MASK_AT = 15,
    PICKED = 14,
    /* triangles' corners: three each, or a fan's, its first corner beginning it and sSetupMode's
       fan bit from the 23rd; their count from the 7th bit, sSetupMode's from the 11th, the colour
       packed */
    CORNERS = 3,
    FAN = 1u << 3 | 1u << 22,
    CORNERS_AT = 6,
    MODE_AT = 10,
    PACKED = 1u << 28,
    TEXELS = 5 | 3u << 30,
    TEXEL_WORDS = 3,
    JUMP = 3 << 3,
    JUMP_AT = 4,
    WORD = 4,
};

void cmdfifo_begin(
    CmdFifo *const f,
    const AccelHost *const host,
    volatile uint8_t *const ring,
    const volatile uint32_t *const read,
    const uint32_t at,
    const uint32_t bytes
)
{
    *f = (CmdFifo){host, ring, read, at, bytes, 0, bytes - WORD, 0, nullptr};
}

static void fenced(const CmdFifo *const f)
{
    if (f->fence)
        f->fence();
}

static uint32_t written(const CmdFifo *const f) { return f->sent & (f->bytes - 1); }

/* The bytes the card has yet to read. */
static uint32_t unread(const CmdFifo *const f)
{
    return (written(f) - (*f->read - f->at)) & (f->bytes - 1);
}

/* The bytes free before the card's reader, a word kept for a jump. */
static uint32_t room(const CmdFifo *const f) { return f->bytes - WORD - unread(f); }

static void word(CmdFifo *const f, const uint32_t v)
{
    *(volatile uint32_t *)(f->ring + written(f)) = v;
    f->sent += WORD;
}

/* Room for n words, the card's reader read only once the room last seen is used up and waited on
   while it frees too little; past the ring's end, a jump back to its start first. */
static void reserved(CmdFifo *const f, const uint32_t n)
{
    const uint32_t tail = f->bytes - written(f);
    const bool back = WORD * (n + 1) > tail;
    const uint32_t need = WORD * n + (back ? tail : 0);

    if (f->free < need) {
        const uint64_t until = f->host->clock_us() + TDFX_WAIT_US;

        fenced(f);

        while ((f->free = room(f)) < need && f->host->clock_us() < until)
            ;
    }

    f->free = f->free > need ? f->free - need : 0;

    if (back) {
        fenced(f);
        word(f, f->at << JUMP_AT | JUMP);
        fenced(f);
        f->sent += tail - WORD;
    }
}

/* Each packet's words stored from a local: a store through the ring could alias f's. */
void cmdfifo_registers(
    CmdFifo *const f,
    const uint32_t reg,
    const uint32_t mask,
    const uint32_t *const v
)
{
    for (uint32_t from = 0; from < 32 && mask >> from; from += PICKED) {
        const uint32_t picked = mask >> from & ((1u << PICKED) - 1);

        uint32_t n = 0;

        for (uint32_t left = picked; left; left &= left - 1)
            n++;

        if (!n)
            continue;

        reserved(f, n + 1);

        volatile uint32_t *to = (volatile uint32_t *)(f->ring + written(f));

        *to++ = picked << MASK_AT | (reg + WORD * from) << REGISTER_AT | REGISTERS;

        for (uint32_t i = 0; picked >> i; i++)
            if (picked >> i & 1)
                *to++ = v[from + i];

        f->sent += WORD * (n + 1);
    }
}

uint32_t *cmdfifo_corners(
    CmdFifo *const f,
    const uint32_t mode,
    const uint32_t words,
    const uint32_t n,
    const bool fan
)
{
    const uint32_t all = n * words;

    reserved(f, all + 1);

    volatile uint32_t *const to = (volatile uint32_t *)(f->ring + written(f));

    *to = n << CORNERS_AT | (mode & 0xFF) << MODE_AT | (fan ? FAN : 0) | PACKED | CORNERS;
    f->sent += WORD * (all + 1);
    return (uint32_t *)(to + 1); /* hole counting takes the words in any order */
}

void cmdfifo_texels(
    CmdFifo *const f,
    const uint32_t at,
    const uint32_t stride,
    const uint16_t *const texels,
    const uint32_t pitch,
    const uint32_t w,
    const uint32_t h,
    const bool indexed
)
{
    const uint32_t a_word = indexed ? 4 : 2, words = w / a_word;

    for (uint32_t y = 0; y < h; y++) {
        const uint16_t *const row = texels + (size_t)y * pitch;

        reserved(f, words + 2);

        volatile uint32_t *to = (volatile uint32_t *)(f->ring + written(f));

        *to++ = words << TEXEL_WORDS | TEXELS;
        *to++ = at + y * stride;

        if (indexed)
            for (uint32_t k = 0; k < words; k++)
                *to++ = tdfx_indices(row + 4 * k);
        else
            for (uint32_t k = 0; k < words; k++)
                *to++ = tdfx_pair(row + 2 * k);

        f->sent += WORD * (words + 2);
    }
}

void cmdfifo_marked(CmdFifo *const f) { f->mark = f->sent; }

/* Waits, up to TDFX_WAIT_US, until the card has read the words before mark. */
static void read_to(const CmdFifo *const f, const uint32_t mark)
{
    const uint64_t until = f->host->clock_us() + TDFX_WAIT_US;

    fenced(f);

    while (unread(f) > f->sent - mark && f->host->clock_us() < until)
        ;
}

void cmdfifo_waited(const CmdFifo *const f) { read_to(f, f->mark); }

void cmdfifo_drained(const CmdFifo *const f) { read_to(f, f->sent); }
