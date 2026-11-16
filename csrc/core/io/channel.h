/* Channels' interface. How it works: channel.c. */
#ifndef CORE_IO_CHNL_H
#define CORE_IO_CHNL_H
#include "defs.h"
#include "fault.h"

typedef struct Cons Cons;

typedef struct {
    uint8_t num;     /* its file number, 0 free */
    uint8_t kind;    /* CK_* */
    uint8_t mode;    /* FM_* */
    uint8_t col;     /* its output column while another is used */
    uint16_t handle; /* the file's (pfil) */
    uint16_t reclen; /* RANDOM's record length */
    uint16_t count;  /* bytes in buf */
    uint16_t next;   /* read ahead: the next one */
    bool dirty;      /* buf holds bytes to write */
    bool eof;        /* GET read past the end */
    uint32_t pos;    /* the file's offset of buf */
    uint32_t last;   /* LOC: the last record, or byte */
    uint16_t field;  /* FIELD's next offset */
    uint8_t *buf;    /* CBSZ bytes, a file's: made at its first OPEN */
} Chan;

enum { CH_LPT = NCH }; /* LPRINT's channel, past the files' */
enum : uint8_t { LPT_WIDTH = 80, LPT_NOWRAP = 255 };

typedef struct {
    Cons *cons; /* the console: channel 0, and the output column */
    Chan ch[NCH + 1];
    uint8_t out, in;   /* output's and input's channel, 0 the console */
    Status err;        /* the byte path's first error since chnl_take, ST_OK none */
    uint8_t lpt_col;   /* LPT1's column, its channels' and LPRINT's, while not used */
    uint8_t lpt_width; /* WIDTH LPRINT's, LPT_NOWRAP none */
    bool lpt_open;     /* lpt_handle is the printer's */
    uint16_t lpt_handle;
} Chnl;

/* The console the only channel. */
void chnl_init(Chnl *cl, Cons *co);
/* The channel of file number num, if open; nullptr none. */
Chan *chnl_look(Chnl *cl, int32_t num);
/* chnl_look's to *c, or Bad file name or number (not open, or out of 1..255). */
[[nodiscard]]
Status chnl_find(Chnl *cl, int32_t num, Chan **c);
/* A platform (DOS) error as QBasic's (QB_*). */
[[nodiscard]]
Status chnl_doserr(uint16_t dos);

/* Open file number num on name, mode FM_*, records of reclen bytes, to *c; a device's name (SCRN:
   CONS: NUL:) no file. Errors: QB_BAD_FILE_NUMBER, QB_FILE_OPEN, QB_TOO_MANY_FILES,
   QB_BAD_RECORD_LENGTH; the file's. */
[[nodiscard]]
Status chnl_open(Chnl *cl, int32_t num, const char *name, uint8_t mode, uint32_t reclen, Chan **c);
/* Close a channel, its bytes written; the console stays. */
[[nodiscard]]
Status chnl_close(Chnl *cl, Chan *c);
/* Close every file: CLOSE alone, RESET, END, RUN; the first error stops it. */
[[nodiscard]]
Status chnl_closeall(Chnl *cl);
/* Output to c, its column cons's. Error: QB_BAD_FILE_MODE, not one written to. */
[[nodiscard]]
Status chnl_out(Chnl *cl, Chan *c);
/* Output to the printer, LPRINT's. Error: QB_DEVICE_UNAVAILABLE, not ready. */
[[nodiscard]]
Status chnl_lprint(Chnl *cl);
/* LPT1's column: col's (0 the first) while output is LPT1's. */
uint8_t chnl_lpos(const Chnl *cl);
/* A character at output column col goes on the next line: LPT1's width reached. */
bool chnl_wraps(const Chnl *cl, uint8_t col);
/* Input from c. Error: QB_BAD_FILE_MODE, not one read. */
[[nodiscard]]
Status chnl_in(Chnl *cl, Chan *c);
/* Output and input the console's again; INPUT#'s state cleared, chnl.err dropped. */
void chnl_clear(Chnl *cl);
/* chnl.err, cleared: the byte path's error, for its statement to raise. */
[[nodiscard]]
Status chnl_take(Chnl *cl);

/* A character to the output channel (not the console's), NL as CR LF. An error: chnl.err, and
   the file's bytes dropped till it is taken. */
void chnl_putc(Chnl *cl, uint8_t ch);
/* The input channel's next byte; -1 at the end or a ^Z (a text file's end), or an error
   (chnl.err). */
int chnl_getb(Chnl *cl);
/* Past the input channel's next byte if it is LF. An error: chnl.err. */
void chnl_skiplf(Chnl *cl);
/* c's next byte, not read, to *b; -1 none: the end, or ^Z in a sequential file. */
[[nodiscard]]
Status chnl_peek(Chan *c, int *b);
/* c's place: the next byte's offset. */
uint32_t chnl_tell(const Chan *c);
/* c to offset at, its buffer written and dropped. */
[[nodiscard]]
Status chnl_seek(Chan *c, uint32_t at);
/* c's file's length to *len; 0 not a file. */
[[nodiscard]]
Status chnl_lof(Chan *c, uint32_t *len);
/* n bytes read into p, or written from p, at c's place, which moves past them; how many to *done.
   Not a file: 0. Errors: the file's. */
[[nodiscard]]
Status chnl_read(Chan *c, uint8_t *p, uint32_t n, uint32_t *done);
[[nodiscard]]
Status chnl_write(Chan *c, const uint8_t *p, uint32_t n, uint32_t *done);
#endif
