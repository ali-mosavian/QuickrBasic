/* Console output's and input's interface. How it works: console.c. */
#ifndef CORE_IO_CONS_H
#define CORE_IO_CONS_H
#include "defs.h"
#include "fault.h"
#include "io/channel.h"
#include "mem/alloc.h"
#include "vm/exec.h"

enum {
    INMAX = 32767,  /* an input line's characters at most, as QBasic's */
    SBMAX = 0xFFF0, /* sbuf: a line's LIST spelled, the rest dropped */
};

/* cons.capture */
enum : uint8_t {
    CAP_DIFF = 1,      /* VERIFY: a byte differed */
    CAP_VERIFY = 0x40, /* output compared with the input */
    CAP_SBUF = 0x80,   /* output to sbuf */
};

typedef struct Cons {
    Chnl *chnl;         /* output and input to a file's channel when set */
    Vm *vm;             /* its stop flag: STOP while a line is read */
    uint8_t capture;    /* CAP_* */
    uint8_t col;        /* the output channel's column */
    uint32_t sblen;     /* sbuf's bytes, to SBMAX */
    uint32_t inlen;     /* inbuf's line, its length */
    int32_t inpos;      /* INPUT: the end of the last field, -1 before the first */
    bool from_file;     /* INPUT#: no prompts, a field past the line reads the next */
    bool stdin_dev;     /* standard input is a device (the console), read key by key */
    uint8_t status;     /* the I/O status (I.LD.ST), always 0 */
    uint16_t oblen;     /* obuf's bytes */
    uint8_t *inbuf;     /* INMAX+1: a line read, 0 after; srcr's window */
    uint8_t *sbuf;      /* SBMAX+1: output captured (CAP_SBUF), LIST's text, scratch */
    uint8_t obuf[OBSZ]; /* standard output's, flushed on NL, before a read, at the exit */
} Cons;

/* inbuf and sbuf carved (allo_carve); standard input's kind; the console the only channel. */
void cons_init(Cons *co, Allo *al, Chnl *cl, Vm *vm);

/* A character to the output, its column kept; NL ends the line. */
void cons_putc(Cons *co, uint8_t c);
/* A space; n spaces. */
void cons_space(Cons *co);
void cons_spaces(Cons *co, uint8_t n);
/* End the line; end it when it has text. */
void cons_nl(Cons *co);
void cons_fresh(Cons *co);
/* An INT as PRINT prints it: " " or "-", digits, " ". */
void cons_int(Cons *co, int32_t v);
/* Digits only, unsigned (io_u24, io_num). */
void cons_uint(Cons *co, uint32_t v);
/* 4 hex digits. */
void cons_hex4(Cons *co, uint16_t v);
/* A 0-ended text. */
void cons_puts(Cons *co, const char *s);
/* s's chars, through cons_putc (asm io_putc). */
void cons_putdesc(Cons *co, Desc s);
/* Message E_* or MSG_* of the message list. */
void cons_msg(Cons *co, uint8_t id);
/* The keyword of token tok (80h..FEh); of FFh n. */
void cons_kw(Cons *co, uint8_t tok);
void cons_kw1(Cons *co, uint8_t n);

/* "? " and a line read; a line read into inbuf, 0 after, inpos -1. Errors: Out of DATA at
   standard input's end, 62 at a file's (or its read's error), BREAK on the STOP key. */
[[nodiscard]]
Status cons_prompt(Cons *co);
[[nodiscard]]
Status cons_read(Cons *co);
/* INPUT: inpos to the next field; none left: "??" and another line (INPUT#: the next line). */
[[nodiscard]]
Status cons_field(Cons *co);

/* Standard output: a character, NL as CR LF (plt_putc); obuf written (plt_flush). */
void cons_out(Cons *co, uint8_t c);
void cons_flush(Cons *co);
/* A line of the input into inbuf: the console key by key, else bytes to LF (CR LF in a file);
   its length, or -1 at the end of the input or on the STOP key (exec's stop flag set). */
int32_t cons_getline(Cons *co);
/* The next input byte, 0 at the end; a console with no key waiting: 0. */
uint8_t cons_getc(Cons *co);
/* The next input byte, standard input's or the input channel's; -1 at the end. */
int cons_getb(Cons *co);
/* Output flushed, the platform restored, exit. */
[[noreturn]]
void cons_exit(Cons *co, uint8_t code);
#endif
