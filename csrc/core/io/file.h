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

void file_opendef(Vm *vm);  /* OPEN's defaults: RANDOM, 128-byte records */
void file_mode_in(Vm *vm);  /* FOR INPUT */
void file_mode_out(Vm *vm); /* FOR OUTPUT */
void file_mode_app(Vm *vm); /* FOR APPEND */
void file_mode_rnd(Vm *vm); /* FOR RANDOM */
void file_mode_bin(Vm *vm); /* FOR BINARY */
void file_reclen(Vm *vm);   /* LEN = ( n -- ); 59 Bad record length */
void file_open(Vm *vm);     /* OPEN name FOR mode AS n ( name n -- ) */
void file_open2(Vm *vm);    /* OPEN mode, n, name [, len] ( mode n name -- ); 54 a bad mode */
void file_close(Vm *vm);    /* CLOSE n ( n -- ): not open, nothing */
void file_closeall(Vm *vm); /* CLOSE alone, RESET */
void file_out(Vm *vm);      /* FI.OUT ( n -- ): output to file n */
void file_in(Vm *vm);       /* FI.IN ( n -- ): input from file n */
void file_console(Vm *vm);  /* FI.CLR: the console's again */
void file_lineinput(Vm *vm); /* LINE INPUT's ( -- s ): the line read, whole */
void file_write_int(Vm *vm); /* WRITE an INT ( i -- ): as PRINT, no spaces */
void file_write_flt(Vm *vm); /* WRITE a FLOAT ( f -- ) */
void file_write_dbl(Vm *vm); /* WRITE a DOUBLE ( d -- ) */
void file_write_str(Vm *vm); /* WRITE a string ( s -- ): in quotes */
void file_write_comma(Vm *vm); /* WRITE's comma between */
void file_eof(Vm *vm);      /* EOF ( n -- i ): read to its end, or a GET past it */
void file_lof(Vm *vm);      /* LOF ( n -- i ) */
void file_loc(Vm *vm);      /* LOC ( n -- i ): RANDOM's last record, BINARY's byte, else /128 */
void file_seekpos(Vm *vm);  /* SEEK ( n -- i ): the next record, or byte, from 1 */
void file_seek(Vm *vm);     /* SEEK #n, p ( n p -- ); 63 Bad record number */
void file_freefile(Vm *vm); /* FREEFILE ( -- i ) */
void file_input_str(Vm *vm); /* INPUT$ ( n [f] count -- s ): from file f, else keys unshown */
void file_attr(Vm *vm);     /* FILEATTR ( n a -- i ): 1 the mode, 2 the handle */
void file_kill(Vm *vm);     /* KILL ( name -- ) */
void file_rename(Vm *vm);   /* NAME old AS new ( old new -- ); 58 File already exists */
void file_mkdir(Vm *vm);    /* MKDIR ( name -- ) */
void file_rmdir(Vm *vm);    /* RMDIR ( name -- ) */
void file_chdir(Vm *vm);    /* CHDIR ( name -- ) */
void file_files(Vm *vm);    /* FILES [name] ( [name] flag -- ); 53 none fits */
void file_novar(Vm *vm);    /* GET's, PUT's no variable ( -- 0 0 ) */
void file_get(Vm *vm);      /* GET #n [, p] [, v] ( n p addr kind -- ) */
void file_put(Vm *vm);      /* PUT #n [, p] [, v] ( n p addr kind -- ) */
void file_field(Vm *vm);    /* FIELD #n ( n -- ): its fields from the record's start */
void file_fielditem(Vm *vm); /* FIELD's w AS v ( w addr kind -- ); 50, 13 */
void file_lset(Vm *vm);     /* LSET v = s ( addr kind s -- ) */
void file_rset(Vm *vm);     /* RSET v = s ( addr kind s -- ) */
void file_bsave(Vm *vm);    /* BSAVE name, addr, n ( name addr n -- ) */
void file_bload(Vm *vm);    /* BLOAD name [, addr] ( name [addr] flag -- ) */
void file_lock(Vm *vm);     /* LOCK, UNLOCK #n ( n -- ): checked, nothing locked */
void file_drop(Vm *vm);     /* ( x -- ) */
void file_noadv(Vm *vm);    /* 73 Advanced feature unavailable */
#endif
