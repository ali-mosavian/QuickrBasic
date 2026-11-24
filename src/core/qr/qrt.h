/* Quickr's run-time routines' interface. How it works: qrt.c. */
#ifndef QR_QRT_H
#define QR_QRT_H
#include "base.h"
#include "defs.h"

enum { QRT_TXT = 32 }; /* qrt_text's text at most */

/* A value's kind as CALL QPR prints it and QSTR spells it; QRT_SPACE: a space, no value. */
typedef enum : uint8_t {
    QRT_INT,
    QRT_SINGLE,
    QRT_DOUBLE,
    QRT_BOOL,
    QRT_STRING,
    QRT_SPACE,
} QrtKind;

/* CALL QX's routines, its operand on top ( args.. op -- results ): a list's, by its cell and its
   elements' ET_* (cell et); an f-string's value by its spec. Each takes the cell again after
   another's: one that grows or shrinks the list may move it. */
typedef enum : uint8_t {
    QX_LEN,   /* ( cell -- n ) */
    QX_PUSH,  /* ( cell et -- i ): one more, empty, at i */
    QX_POP,   /* ( cell et -- v ): the last, a number or a string, taken */
    QX_LAST,  /* ( cell -- addr ): the last's address */
    QX_DROP,  /* ( cell et -- ): the last dropped */
    QX_SINK,  /* ( cell et i -- ): the last moved to i, those from i up one */
    QX_RAISE, /* ( cell et i -- ): i moved last, those past it down one */
    QX_HAS,   /* ( cell et v -- flag ): v among the elements, a number or a string */
    QX_FMT,   /* ( v spec kind -- s ): v of kind formatted by spec, Python's format spec */
    QX_MAPAT,  /* ( key ket vet map how -- addr ): key's value in the map record at map, by how */
    QX_MAPDEL, /* ( key ket vet map -- ): key's entry, if there, removed */
    QX_BYTE,   /* ( s i -- n ): s's byte i */
    QX_SLICE,  /* ( s a b -- t ): s's bytes from a up to b, each clamped to s */
    QX_LSLICE, /* ( a b src dst -- ): dst a new list, src's elements from a up to b, clamped */
} QrtX;

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
