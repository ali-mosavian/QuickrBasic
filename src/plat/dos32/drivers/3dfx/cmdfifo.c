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
    *f = (CmdFifo){host, ring, read, at, bytes, 0, bytes - WORD, 0};
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

        while ((f->free = room(f)) < need && f->host->clock_us() < until)
            ;
    }

    f->free = f->free > need ? f->free - need : 0;

    if (back) {
        word(f, f->at << JUMP_AT | JUMP);
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
        word(f, words << TEXEL_WORDS | TEXELS);
        word(f, at + y * stride);

        for (uint32_t x = 0; x < w; x += a_word)
            word(f, indexed ? tdfx_indices(row + x) : row[x] | (uint32_t)row[x + 1] << 16);
    }
}

void cmdfifo_fenced(CmdFifo *const f) { f->fence = f->sent; }

/* Waits, up to TDFX_WAIT_US, until the card has read the words before mark. */
static void read_to(const CmdFifo *const f, const uint32_t mark)
{
    const uint64_t until = f->host->clock_us() + TDFX_WAIT_US;

    while (unread(f) > f->sent - mark && f->host->clock_us() < until)
        ;
}

void cmdfifo_waited(const CmdFifo *const f) { read_to(f, f->fence); }

void cmdfifo_drained(const CmdFifo *const f) { read_to(f, f->sent); }
