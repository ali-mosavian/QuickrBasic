/* Quickr's run-time routines' interface. How it works: qrt.c. */
#ifndef QR_QRT_H
#define QR_QRT_H
#include "base.h"

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

/* CALL QJ's operand: the offset from past the CALL, shifted by QJ_SHIFT, and when it jumps. */
typedef enum : uint8_t { QJ_ALWAYS, QJ_FALSE, QJ_TRUE } QrtWhen;
enum { QJ_SHIFT = 2 };

/* v, a number or bool of kind, as print spells it: an int in decimal, a bool true or false, a
   single to 7 significant digits and a double to 15, as %g with a point always. Returns its
   length. */
uint32_t qrt_text(QrtKind kind, Cell v, uint8_t out[static QRT_TXT]);
#endif
