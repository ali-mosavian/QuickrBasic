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
    uint8_t buf[CBSZ];
} Chan;

typedef struct {
    Cons *cons;   /* the console: channel 0, and the output column */
    Fault *fault; /* the QBasic errors */
    Chan ch[NCH];
    uint8_t out, in; /* output's and input's channel, 0 the console */
} Chnl;

/* The console the only channel. */
void chnl_init(Chnl *cl, Cons *co, Fault *f);
/* The channel of file number num, if open; nullptr none. */
Chan *chnl_look(Chnl *cl, int32_t num);
/* chnl_look, or Bad file name or number (not open, or out of 1..255). */
Chan *chnl_find(Chnl *cl, int32_t num);
/* A platform (DOS) error raised as QBasic's (QB_*). */
[[noreturn]]
void chnl_doserr(Chnl *cl, uint16_t dos);

/* Open file number num on name, mode FM_*, records of reclen bytes; a device's name (SCRN:
   CONS: NUL:) no file. Errors: QB_BAD_FILE_NUMBER, QB_FILE_OPEN, QB_TOO_MANY_FILES,
   QB_BAD_RECORD_LENGTH; the file's. */
Chan *chnl_open(Chnl *cl, int32_t num, const char *name, uint8_t mode, uint32_t reclen);
/* Close a channel, its bytes written; the console stays. */
void chnl_close(Chnl *cl, Chan *c);
/* Close every file: CLOSE alone, RESET, END, RUN. */
void chnl_closeall(Chnl *cl);
/* Output to c, its column cons's. Error: QB_BAD_FILE_MODE, not one written to. */
void chnl_out(Chnl *cl, Chan *c);
/* Input from c. Error: QB_BAD_FILE_MODE, not one read. */
void chnl_in(Chnl *cl, Chan *c);
/* Output and input the console's again; INPUT#'s state cleared. */
void chnl_clear(Chnl *cl);

/* A character to the output channel (not the console's), NL as CR LF. */
void chnl_putc(Chnl *cl, uint8_t ch);
/* The input channel's next byte; -1 at the end or a ^Z (a text file's end). */
int chnl_getb(Chnl *cl);
/* Past the input channel's next byte if it is LF. */
void chnl_skiplf(Chnl *cl);
/* c's next byte, not read; -1 none: the end, or ^Z in a sequential file. */
int chnl_peek(Chnl *cl, Chan *c);
/* c's place: the next byte's offset. */
uint32_t chnl_tell(const Chan *c);
/* c to offset at, its buffer written and dropped. */
void chnl_seek(Chnl *cl, Chan *c, uint32_t at);
/* c's file's length; 0 not a file. */
uint32_t chnl_lof(Chnl *cl, Chan *c);
/* n bytes read into p, or written from p, at c's place, which moves past them; how many.
   Not a file: 0. Errors: the file's. */
uint32_t chnl_read(Chnl *cl, Chan *c, uint8_t *p, uint32_t n);
uint32_t chnl_write(Chnl *cl, Chan *c, const uint8_t *p, uint32_t n);
#endif
