/* Quickr's run-time routines' interface. How it works: qrt.c. */
#ifndef QR_QRT_H
#define QR_QRT_H
#include "base.h"
#include "defs.h"

enum { QRT_TXT = 32 }; /* qrt_text's text at most */

/* A value's kind as CALL QPR prints it and QSTR spells it; QRT_SPACE: a space, no value. An int
   cell's integer is QRT_INT's, but a uint's; a long's and a ulong's are 8 bytes. */
typedef enum : uint8_t {
    QRT_INT,
    QRT_SINGLE,
    QRT_DOUBLE,
    QRT_BOOL,
    QRT_STRING,
    QRT_SPACE,
    QRT_UINT,
    QRT_LONG,
    QRT_ULONG,
} QrtKind;

/* A number as QX.INT and QX.CONV take it: an integer's bits and sign, in an int cell to 32 bits
   (a uint's bits as they are), in 8 bytes for 64; a float's. */
typedef enum : uint8_t {
    QN_I8,
    QN_I16,
    QN_I32,
    QN_I64,
    QN_U8,
    QN_U16,
    QN_U32,
    QN_U64,
    QN_F32,
    QN_F64,
} QrtNum;

/* QX.INT's operator: ( a b -- r ), ( a -- r ) for NEG and NOT; CMP's r a flag of its relation, a
   shift's b an int. */
typedef enum : uint8_t {
    QI_ADD,
    QI_SUB,
    QI_MUL,
    QI_DIV,
    QI_MOD,
    QI_POW,
    QI_AND,
    QI_OR,
    QI_XOR,
    QI_SHL,
    QI_SHR,
    QI_NEG,
    QI_NOT,
    QI_CMP,
} QrtIop;

/* QX's operand: its routine in the low byte, then a routine's own: QX.INT's operator, number,
   wrap, relation (num/double.h's DC_* mask) and which operands are int cells, widened there;
   QX.CONV's from, to and wrap. */
enum { QX_OP = 8, QX_NUM = 12, QX_WRAP = 16, QX_REL = 17, QX_ICA = 20, QX_ICB = 21, QX_TO = 12 };

/* CALL QX's routines, its operand on top ( args.. op -- results ): a list's, by its cell and its
   elements' ET_* (cell et); an f-string's value by its spec. Each takes the cell again after
   another's: one that grows or shrinks the list may move it. */
typedef enum : uint8_t {
    QX_LEN,     /* ( cell -- n ) */
    QX_PUSH,    /* ( cell et -- i ): one more, empty, at i */
    QX_POP,     /* ( cell et -- v ): the last, a number or a string, taken */
    QX_LAST,    /* ( cell -- addr ): the last's address */
    QX_DROP,    /* ( cell et -- ): the last dropped */
    QX_SINK,    /* ( cell et i -- ): the last moved to i, those from i up one */
    QX_RAISE,   /* ( cell et i -- ): i moved last, those past it down one */
    QX_HAS,     /* ( cell et v -- flag ): v among the elements, a number or a string */
    QX_FMT,     /* ( v spec kind -- s ): v of kind formatted by spec, Python's format spec */
    QX_MAPAT,   /* ( key ket vet map how -- addr ): key's value in the map record at map, by how */
    QX_MAPDEL,  /* ( key ket vet map -- ): key's entry, if there, removed */
    QX_BYTE,    /* ( s i -- n ): s's byte i */
    QX_SLICE,   /* ( s a b -- t ): s's bytes from a up to b, each clamped to s */
    QX_LSLICE,  /* ( a b src dst -- ): dst a new list, src's elements from a up to b, clamped */
    QX_FNCALL,  /* ( args.. proc -- ): a call of procedure proc, as PCALL's */
    QX_INT,     /* an integer's operator (QrtIop) the int ops don't have: Overflow past its range */
    QX_CONV,    /* ( v -- w ): a number converted: Overflow when it doesn't fit, unless wrapped */
    QX_FIND,    /* ( s t [from] -- i ): where t is in s from from (QX_FROM), else 0; -1 none */
    QX_FOPEN,   /* ( name mode -- n ): file number n open, mode r w a or rw; -ERR an error */
    QX_FREAD,   /* ( n count -- s ): count bytes at most, fewer at the end */
    QX_FWRITE,  /* ( n s -- st ): s written; 0, or -ERR */
    QX_FSEEK,   /* ( n at -- st ): to byte at; 0, or -ERR */
    QX_FSIZE,   /* ( n -- bytes ) */
    QX_FCLOSE,  /* ( n -- ) */
    QX_FLIST,   /* ( pattern -- names ): the files matching, each ended by a newline */
    QX_ERRTEXT, /* ( err -- s ): ERR err's message */
} QrtX;

/* QX.FIND's operand with it: a from on the stack. */
enum { QX_FROM = 0x100 };

/* QX.HAS's et with it: elements compared by their 8 bytes, a long's. */
enum { QRT_RAW = 0x1000 };

/* QX_MAPAT's how when the key is not there: Subscript out of range, made with an empty value, or
   the address 0. */
typedef enum : uint8_t { QM_READ, QM_SET, QM_FIND } QrtHow;

/* CALL QJ's operand: the offset from past the CALL, shifted by QJ_SHIFT, and when it jumps. */
typedef enum : uint8_t { QJ_ALWAYS, QJ_FALSE, QJ_TRUE } QrtWhen;
enum { QJ_SHIFT = 2 };

/* v, a number or bool of kind, as print spells it: an int in decimal, a bool true or false, a
   single to 7 significant digits and a double to 15, as %g with a point always. Returns its
   length. */
uint32_t qrt_text(QrtKind kind, Cell v, uint8_t out[static QRT_TXT]);
/* v of kind formatted by spec's n bytes, a Python format spec sema checked, to out (room for
   SBMAX); its length. A string v is its Desc. */
uint32_t qrt_format(QrtKind kind, Cell v, Desc str, const uint8_t *spec, uint32_t n, uint8_t *out);
#endif
