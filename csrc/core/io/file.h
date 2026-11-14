/* QBasic's file statements and functions, over the channels (io/fio.asm). Each is an op's
   handler, its stack effect ( in -- out ) on exec's data stack.
   A variable's place, as the compiler's va gives it: its address and a kind in the top byte,
   VA_DESC (a string's Desc; n its STRING * n), VA_RAW (n bytes), VA_I2 (an INT kept as 2
   bytes); n in the low word. A RANDOM channel's record is RECMAX bytes of file.recs at its
   index: FIELD's strings point into it, so it is in the arena. */
#ifndef CORE_IO_FILE_H
#define CORE_IO_FILE_H
#include "io/chnl.h"

typedef struct {
    uint8_t mode;    /* OPEN's mode, FM_* */
    uint32_t reclen; /* OPEN's LEN */
    Addr recs;       /* NCH * RECMAX: each channel's record */
    Chan *field_chan;        /* FIELD's channel */
    char name2[128];  /* NAME's new name; FILES's directory */
    char bname[264];  /* BSAVE's, BLOAD's name, .BAS added */
} File;

/* The records' region carved (allo_carve). */
void file_init(Session *S);

void file_opendef(Session *S);  /* OPEN's defaults: RANDOM, 128-byte records */
void file_mode_in(Session *S);  /* FOR INPUT */
void file_mode_out(Session *S); /* FOR OUTPUT */
void file_mode_app(Session *S); /* FOR APPEND */
void file_mode_rnd(Session *S); /* FOR RANDOM */
void file_mode_bin(Session *S); /* FOR BINARY */
void file_reclen(Session *S);   /* LEN = ( n -- ); 59 Bad record length */
void file_open(Session *S);     /* OPEN name FOR mode AS n ( name n -- ) */
void file_open2(Session *S);    /* OPEN mode, n, name [, len] ( mode n name -- ); 54 a bad mode */
void file_close(Session *S);    /* CLOSE n ( n -- ): not open, nothing */
void file_closeall(Session *S); /* CLOSE alone, RESET */
void file_out(Session *S);      /* FI.OUT ( n -- ): output to file n */
void file_in(Session *S);       /* FI.IN ( n -- ): input from file n */
void file_console(Session *S);  /* FI.CLR: the console's again */
void file_lineinput(Session *S); /* LINE INPUT's ( -- s ): the line read, whole */
void file_write_int(Session *S); /* WRITE an INT ( i -- ): as PRINT, no spaces */
void file_write_flt(Session *S); /* WRITE a FLOAT ( f -- ) */
void file_write_dbl(Session *S); /* WRITE a DOUBLE ( d -- ) */
void file_write_str(Session *S); /* WRITE a string ( s -- ): in quotes */
void file_write_comma(Session *S); /* WRITE's comma between */
void file_eof(Session *S);      /* EOF ( n -- i ): read to its end, or a GET past it */
void file_lof(Session *S);      /* LOF ( n -- i ) */
void file_loc(Session *S);      /* LOC ( n -- i ): RANDOM's last record, BINARY's byte, else /128 */
void file_seekpos(Session *S);  /* SEEK ( n -- i ): the next record, or byte, from 1 */
void file_seek(Session *S);     /* SEEK #n, p ( n p -- ); 63 Bad record number */
void file_freefile(Session *S); /* FREEFILE ( -- i ) */
void file_input_str(Session *S); /* INPUT$ ( n [f] count -- s ): from file f, else keys unshown */
void file_attr(Session *S);     /* FILEATTR ( n a -- i ): 1 the mode, 2 the handle */
void file_kill(Session *S);     /* KILL ( name -- ) */
void file_rename(Session *S);   /* NAME old AS new ( old new -- ); 58 File already exists */
void file_mkdir(Session *S);    /* MKDIR ( name -- ) */
void file_rmdir(Session *S);    /* RMDIR ( name -- ) */
void file_chdir(Session *S);    /* CHDIR ( name -- ) */
void file_files(Session *S);    /* FILES [name] ( [name] flag -- ); 53 none fits */
void file_novar(Session *S);    /* GET's, PUT's no variable ( -- 0 0 ) */
void file_get(Session *S);      /* GET #n [, p] [, v] ( n p addr kind -- ) */
void file_put(Session *S);      /* PUT #n [, p] [, v] ( n p addr kind -- ) */
void file_field(Session *S);    /* FIELD #n ( n -- ): its fields from the record's start */
void file_fielditem(Session *S); /* FIELD's w AS v ( w addr kind -- ); 50, 13 */
void file_lset(Session *S);     /* LSET v = s ( addr kind s -- ) */
void file_rset(Session *S);     /* RSET v = s ( addr kind s -- ) */
void file_bsave(Session *S);    /* BSAVE name, addr, n ( name addr n -- ) */
void file_bload(Session *S);    /* BLOAD name [, addr] ( name [addr] flag -- ) */
void file_lock(Session *S);     /* LOCK, UNLOCK #n ( n -- ): checked, nothing locked */
void file_drop(Session *S);     /* ( x -- ) */
void file_noadv(Session *S);    /* 73 Advanced feature unavailable */
#endif
