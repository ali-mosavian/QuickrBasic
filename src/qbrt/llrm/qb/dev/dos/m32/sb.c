/* The synthesizer's voices on DOS: a Sound Blaster (DSP 2.00 or later) where BLASTER names one, else the speaker plays
   the first voice as a square wave.

   The card plays a DMA buffer of two halves over and over (8-bit, auto-initialize) and interrupts as each half ends;
   the interrupt fills that half from the synthesizer (plat/synth). The card is started by the first note for the
   synthesizer, so a program that never uses one never touches it. */
#include "device.h"
#include "llrm_os.h"
#include "music.h"
#include "plat/synth/synth.h"

enum {
    /* the DSP's ports, from its base */
    DSP_RESET = 0x6,
    DSP_READ = 0xA,
    DSP_WRITE = 0xC, /* bit 7 set: busy */
    DSP_STATUS = 0xE, /* bit 7 set: a byte to read; reading it acknowledges an 8-bit interrupt */
    DSP_READY = 0xAA,
    DSP_SPEAKER_ON = 0xD1,
    DSP_SPEAKER_OFF = 0xD3,
    DSP_TIME_CONSTANT = 0x40,
    DSP_BLOCK_SIZE = 0x48,
    DSP_AUTO_OUTPUT = 0x1C, /* 8-bit, auto-initialize: DSP 2.00 on */
    DSP_HALT = 0xD0,
    DSP_EXIT_AUTO = 0xDA,
    DSP_VERSION = 0xE1,
    DSP_TRIES = 65536,
    /* the 8-bit DMA controller */
    DMA_MASK = 0x0A,
    DMA_MASK_ON = 0x04,
    DMA_MODE = 0x0B,
    DMA_PLAY_LOOP = 0x58, /* single, auto-initialize, memory to the card */
    DMA_FLIP_FLOP = 0x0C,
    /* the PICs */
    PIC1 = 0x20,
    PIC2 = 0xA0,
    PIC_EOI = 0x20,
    PIC1_BASE = 0x08,
    PIC2_BASE = 0x70,
    /* DPMI */
    DPMI_SEGMENT_BASE = 0x0006,
    DPMI_DOS_ALLOC = 0x0100,
    DPMI_DOS_FREE = 0x0101,
    CARRY = 0x1,
    /* the sound: 1000000 / (256 - 193) = 15873 samples a second */
    TIME_CONSTANT = 193,
    HALF = 512, /* samples a half: 32 ms */
    PARAGRAPH = 16,
    PAGE = 0x10000, /* DMA does not cross a 64K page */
    ENV_MAX = 128,
};

static const unsigned dma_page[4] = {0x87, 0x83, 0x81, 0x82};

static struct {
    unsigned base, irq, dma;
    unsigned char opened, tried;
    unsigned selector; /* the DOS memory's */
    unsigned char *buffer;
    unsigned half; /* the one to fill next */
    unsigned pic_mask; /* the card's line's mask bit as it was */
} card;

static Synth synth;

/* ---- the DSP ---- */

static int dsp_write(unsigned value)
{
    unsigned tries;

    for (tries = 0; tries < DSP_TRIES; tries++)
        if (!(dev_inb(card.base + DSP_WRITE) & 0x80)) {
            dev_outb(card.base + DSP_WRITE, value);
            return 1;
        }
    return 0;
}

static int dsp_read(void)
{
    unsigned tries;

    for (tries = 0; tries < DSP_TRIES; tries++)
        if (dev_inb(card.base + DSP_STATUS) & 0x80)
            return (int)dev_inb(card.base + DSP_READ);
    return -1;
}

static int dsp_reset(void)
{
    unsigned wait;

    dev_outb(card.base + DSP_RESET, 1);
    for (wait = 0; wait < 8; wait++) /* 3 us: a port read is about 1 */
        dev_inb(card.base + DSP_RESET);
    dev_outb(card.base + DSP_RESET, 0);
    return dsp_read() == DSP_READY;
}

/* ---- finding it ---- */

static unsigned hex_digit(char c)
{
    return c >= '0' && c <= '9' ? (unsigned)(c - '0') : (unsigned)((c | 0x20) - 'a' + 10);
}

/* The number after a BLASTER setting's letter, in `radix`. */
static unsigned setting(const char *at, unsigned radix)
{
    unsigned value = 0;

    while ((*at >= '0' && *at <= '9') || (radix == 16 && ((*at | 0x20) >= 'a' && (*at | 0x20) <= 'f')))
        value = value * radix + hex_digit(*at++);
    return value;
}

/* BLASTER=A220 I5 D1 ...: the base port, the interrupt line and the 8-bit DMA channel. */
static int blaster(void)
{
    static const char name[] = "BLASTER=";
    char text[ENV_MAX];
    unsigned index, at;
    int length;

    for (index = 0; (length = llrm_os_environment(index, (unsigned char *)text, ENV_MAX - 1)) >= 0; index++) {
        text[length] = 0;
        for (at = 0; name[at] && (text[at] & ~0x20) == (name[at] & ~0x20); at++)
            ;
        if (name[at])
            continue;
        card.base = card.irq = card.dma = ~0u;
        for (; text[at]; at++) {
            const char letter = (char)(text[at] & ~0x20);

            if (at > 0 && text[at - 1] != ' ' && text[at - 1] != '=')
                continue;
            if (letter == 'A')
                card.base = setting(text + at + 1, 16);
            else if (letter == 'I')
                card.irq = setting(text + at + 1, 10);
            else if (letter == 'D')
                card.dma = setting(text + at + 1, 10);
        }
        return card.base != ~0u && card.irq < 16 && card.dma < 4;
    }
    return 0;
}

/* ---- the buffer and the interrupt ---- */

/* The buffer: DOS memory (below 1 MB, where DMA reaches), the two halves within one 64K page; its physical address. */
static unsigned long buffer_open(void)
{
    Regs r = {0};
    unsigned long physical, base;

    r.rax = DPMI_DOS_ALLOC;
    r.rbx = 4 * HALF / PARAGRAPH;
    dev_int31(&r);
    if (r.rflags & CARRY)
        return 0;
    card.selector = r.rdx;
    physical = (unsigned long)r.rax * PARAGRAPH;
    if (physical / PAGE != (physical + 2 * HALF - 1) / PAGE)
        physical += 2 * HALF;

    r = (Regs){0};
    r.rax = DPMI_SEGMENT_BASE;
    r.rbx = dev_ds_selector();
    dev_int31(&r);
    base = (unsigned long)r.rcx << 16 | r.rdx;
    card.buffer = (unsigned char *)(physical - base); /* DOS memory's linear address is its physical one */
    return physical;
}

static void buffer_close(void)
{
    Regs r = {0};

    r.rax = DPMI_DOS_FREE;
    r.rdx = card.selector;
    dev_int31(&r);
}

static void fill(unsigned half)
{
    static short samples[HALF];
    unsigned char *const out = card.buffer + half * HALF;
    unsigned i;

    synth_render(&synth, samples, HALF);
    for (i = 0; i < HALF; i++)
        out[i] = (unsigned char)((samples[i] >> 8) + 128);
}

void dev_sb_fill(void)
{
    dev_inb(card.base + DSP_STATUS);
    fill(card.half);
    card.half ^= 1;
    if (card.irq >= 8)
        dev_outb(PIC2, PIC_EOI);
    dev_outb(PIC1, PIC_EOI);
}

static unsigned pic_port(void)
{
    return (card.irq < 8 ? PIC1 : PIC2) + 1;
}

static void dma_start(unsigned long physical)
{
    const unsigned count = 2 * HALF - 1, ch = card.dma;

    dev_outb(DMA_MASK, DMA_MASK_ON | ch);
    dev_outb(DMA_FLIP_FLOP, 0);
    dev_outb(DMA_MODE, DMA_PLAY_LOOP | ch);
    dev_outb(ch * 2, physical & 0xFF);
    dev_outb(ch * 2, physical >> 8 & 0xFF);
    dev_outb(dma_page[ch], physical >> 16 & 0xFF);
    dev_outb(ch * 2 + 1, count & 0xFF);
    dev_outb(ch * 2 + 1, count >> 8);
    dev_outb(DMA_MASK, ch);
}

void dev_synth_open(void)
{
    unsigned long physical;
    unsigned bit;

    if (card.tried)
        return;
    card.tried = 1;
    if (!blaster() || !dsp_reset() || !dsp_write(DSP_VERSION) || dsp_read() < 2 || dsp_read() < 0)
        return;
    physical = buffer_open();
    if (!physical)
        return;

    synth_init(&synth, 1000000L / (256 - TIME_CONSTANT));
    synth_clock(&synth, music_fine_tick, MUSIC_FINE_RATE);
    card.half = 0;
    fill(0);
    fill(1);

    bit = 1u << (card.irq & 7);
    dev_sb_hook(card.irq < 8 ? PIC1_BASE + card.irq : PIC2_BASE + card.irq - 8);
    card.pic_mask = dev_inb(pic_port()) & bit;
    dev_outb(pic_port(), dev_inb(pic_port()) & ~bit);
    dma_start(physical);

    dsp_write(DSP_SPEAKER_ON);
    dsp_write(DSP_TIME_CONSTANT);
    dsp_write(TIME_CONSTANT);
    dsp_write(DSP_BLOCK_SIZE);
    dsp_write((HALF - 1) & 0xFF);
    dsp_write((HALF - 1) >> 8);
    dsp_write(DSP_AUTO_OUTPUT);
    card.opened = 1;
}

void dev_synth_close(void)
{
    if (!card.opened)
        return;
    card.opened = 0;
    dsp_write(DSP_HALT);
    dsp_write(DSP_EXIT_AUTO);
    dsp_write(DSP_SPEAKER_OFF);
    dev_outb(DMA_MASK, DMA_MASK_ON | card.dma);
    dev_outb(pic_port(), dev_inb(pic_port()) | card.pic_mask);
    dev_sb_unhook();
    dsp_reset();
    buffer_close();
}

void dev_voice(unsigned voice, const DevVoice *v)
{
    if (card.opened) {
        const unsigned flags = dev_irq_save();

        synth_voice(&synth, voice, v);
        dev_irq_restore(flags);
    } else if (voice == 0)
        dev_tone(v->gate ? v->hertz : 0);
}
