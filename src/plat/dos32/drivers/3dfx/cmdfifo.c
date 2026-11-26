#include "plat/dos32/drivers/3dfx/cmdfifo.h"
#include "plat/dos32/drivers/3dfx/tdfx.h"

enum : uint32_t {
    /* registers, from BAR0's: its base page, its pages less one and its switch, the reader's place
       and the high half of it, the bounds of what is written, the words in it, the holes in them;
       when it fetches */
    CMD = 0x80000,
    BASE = 0x20,
    SIZE = 0x24,
    READ = 0x2C,
    READ_HIGH = 0x30,
    LEAST = 0x34,
    MOST = 0x3C,
    DEPTH = 0x44,
    HOLES = 0x48,
    THRESH = 0x80,
    ON = 1u << 8,
    PAGE = 4096,
    BANSHEE_THRESH = 9u << 5 | 2,
    VOODOO3_THRESH = 0xFu << 5 | 8,
    /* packets: registers, from the 2nd bit their place, its words from the 16th, the place
       incremented; texels to the texture port, their words from the 3rd bit; a jump in frame
       memory, its place from the 4th */
    REGISTERS = 1,
    REGISTER_AT = 1,
    WORDS = 16,
    INCREMENTED = 1u << 15,
    TEXELS = 5 | 3u << 30,
    TEXEL_WORDS = 3,
    JUMP = 3 << 3,
    JUMP_AT = 4,
    WORD = 4,
};

static uint32_t get(const CmdFifo *const f, const uint32_t reg)
{
    return *(const volatile uint32_t *)(f->regs + CMD + reg);
}

static void set(const CmdFifo *const f, const uint32_t reg, const uint32_t v)
{
    *(volatile uint32_t *)(f->regs + CMD + reg) = v;
}

void cmdfifo_open(
    CmdFifo *const f,
    const AccelHost *const host,
    volatile uint8_t *const io,
    volatile uint8_t *const frame,
    const uint32_t at,
    const bool banshee
)
{
    *f = (CmdFifo){host, io, frame + at, at, 0, CMDFIFO_BYTES - WORD};

    /* Glide's hwcInitFifo */
    set(f, SIZE, 0);
    set(f, BASE, at / PAGE);
    set(f, READ, at);
    set(f, READ_HIGH, 0);
    set(f, LEAST, at - WORD);
    set(f, MOST, at - WORD);
    set(f, DEPTH, 0);
    set(f, HOLES, 0);
    set(f, THRESH, banshee ? BANSHEE_THRESH : VOODOO3_THRESH);
    set(f, SIZE, (CMDFIFO_BYTES / PAGE - 1) | ON);
}

void cmdfifo_close(const CmdFifo *const f) { set(f, SIZE, 0); }

static uint32_t written(const CmdFifo *const f) { return f->sent & (CMDFIFO_BYTES - 1); }

/* The bytes the card has yet to read. */
static uint32_t unread(const CmdFifo *const f)
{
    return (written(f) - (get(f, READ) - f->at)) & (CMDFIFO_BYTES - 1);
}

/* The bytes free before the card's reader, a word kept for a jump. */
static uint32_t room(const CmdFifo *const f) { return CMDFIFO_BYTES - WORD - unread(f); }

static void word(CmdFifo *const f, const uint32_t v)
{
    *(volatile uint32_t *)(f->ring + written(f)) = v;
    f->sent += WORD;
}

/* Room for n words, the card's reader read only once the room last seen is used up and waited on
   while it frees too little; past the ring's end, a jump back to its start first. */
static void reserved(CmdFifo *const f, const uint32_t n)
{
    const uint32_t tail = CMDFIFO_BYTES - written(f);
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

void cmdfifo_registers(
    CmdFifo *const f,
    const uint32_t reg,
    const uint32_t *const v,
    const uint32_t n
)
{
    reserved(f, n + 1);
    word(f, n << WORDS | INCREMENTED | reg << REGISTER_AT | REGISTERS);

    for (uint32_t i = 0; i < n; i++)
        word(f, v[i]);
}

void cmdfifo_texels(
    CmdFifo *const f,
    const uint32_t at,
    const uint16_t *const texels,
    const uint32_t n
)
{
    const uint32_t words = n / 2;

    reserved(f, words + 2);
    word(f, words << TEXEL_WORDS | TEXELS);
    word(f, at);

    for (uint32_t i = 0; i < n; i += 2)
        word(f, texels[i] | (uint32_t)texels[i + 1] << 16);
}

uint32_t cmdfifo_mark(const CmdFifo *const f) { return f->sent; }

void cmdfifo_read_to(const CmdFifo *const f, const uint32_t mark)
{
    const uint64_t until = f->host->clock_us() + TDFX_WAIT_US;

    while (unread(f) > f->sent - mark && f->host->clock_us() < until)
        ;
}
