/* QBasic's file statements and functions, over the channels (io/fio.asm). Each is an op's
   handler, its stack effect ( in -- out ) on exec's data stack.
   A variable's place, as the compiler's va gives it: its address and a kind in the top byte,
   VA_DESC (a string's Desc; n its STRING * n), VA_RAW (n bytes), VA_I2 (an INT kept as 2
   bytes); n in the low word. A RANDOM channel's record is RECMAX bytes of file.recs at its
   index: FIELD's strings point into it, so it is in the arena. */
#ifndef CORE_IO_FILE_H
#define CORE_IO_FILE_H
#include "defs.h"

typedef struct {
    uint8_t mode;    /* OPEN's mode, FM_* */
    uint32_t reclen; /* OPEN's LEN */
    Addr recs;       /* NCH * RECMAX: each channel's record */
} File;
extern File file;

/* The records' region carved (allo_carve). */
void file_init(void);

void file_opendef(void);  /* OPEN's defaults: RANDOM, 128-byte records */
void file_mode_in(void);  /* FOR INPUT */
void file_mode_out(void); /* FOR OUTPUT */
void file_mode_app(void); /* FOR APPEND */
void file_mode_rnd(void); /* FOR RANDOM */
void file_mode_bin(void); /* FOR BINARY */
void file_reclen(void);   /* LEN = ( n -- ); 59 Bad record length */
void file_open(void);     /* OPEN name FOR mode AS n ( name n -- ) */
void file_open2(void);    /* OPEN mode, n, name [, len] ( mode n name -- ); 54 a bad mode */
void file_close(void);    /* CLOSE n ( n -- ): not open, nothing */
void file_closeall(void); /* CLOSE alone, RESET */
void file_out(void);      /* FI.OUT ( n -- ): output to file n */
void file_in(void);       /* FI.IN ( n -- ): input from file n */
void file_console(void);  /* FI.CLR: the console's again */
void file_lineinput(void); /* LINE INPUT's ( -- s ): the line read, whole */
void file_write_int(void); /* WRITE an INT ( i -- ): as PRINT, no spaces */
void file_write_flt(void); /* WRITE a FLOAT ( f -- ) */
void file_write_dbl(void); /* WRITE a DOUBLE ( d -- ) */
void file_write_str(void); /* WRITE a string ( s -- ): in quotes */
void file_write_comma(void); /* WRITE's comma between */
void file_eof(void);      /* EOF ( n -- i ): read to its end, or a GET past it */
void file_lof(void);      /* LOF ( n -- i ) */
void file_loc(void);      /* LOC ( n -- i ): RANDOM's last record, BINARY's byte, else /128 */
void file_seekpos(void);  /* SEEK ( n -- i ): the next record, or byte, from 1 */
void file_seek(void);     /* SEEK #n, p ( n p -- ); 63 Bad record number */
void file_freefile(void); /* FREEFILE ( -- i ) */
void file_input_str(void); /* INPUT$ ( n [f] count -- s ): from file f, else keys unshown */
void file_attr(void);     /* FILEATTR ( n a -- i ): 1 the mode, 2 the handle */
void file_kill(void);     /* KILL ( name -- ) */
void file_rename(void);   /* NAME old AS new ( old new -- ); 58 File already exists */
void file_mkdir(void);    /* MKDIR ( name -- ) */
void file_rmdir(void);    /* RMDIR ( name -- ) */
void file_chdir(void);    /* CHDIR ( name -- ) */
void file_files(void);    /* FILES [name] ( [name] flag -- ); 53 none fits */
void file_novar(void);    /* GET's, PUT's no variable ( -- 0 0 ) */
void file_get(void);      /* GET #n [, p] [, v] ( n p addr kind -- ) */
void file_put(void);      /* PUT #n [, p] [, v] ( n p addr kind -- ) */
void file_field(void);    /* FIELD #n ( n -- ): its fields from the record's start */
void file_fielditem(void); /* FIELD's w AS v ( w addr kind -- ); 50, 13 */
void file_lset(void);     /* LSET v = s ( addr kind s -- ) */
void file_rset(void);     /* RSET v = s ( addr kind s -- ) */
void file_bsave(void);    /* BSAVE name, addr, n ( name addr n -- ) */
void file_bload(void);    /* BLOAD name [, addr] ( name [addr] flag -- ) */
void file_lock(void);     /* LOCK, UNLOCK #n ( n -- ): checked, nothing locked */
void file_drop(void);     /* ( x -- ) */
void file_noadv(void);    /* 73 Advanced feature unavailable */
#endif
