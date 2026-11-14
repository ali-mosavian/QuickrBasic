/* PRINT USING's interface. How it works: using.c. */
#ifndef CORE_VM_USNG_H
#define CORE_VM_USNG_H
#include "vm/exec.h"

enum { FMTMAX = 255, NDIG = 80 };

/* A field's kind (us_fld). */
typedef enum : uint8_t { UF_NONE, UF_FIRST, UF_ALL, UF_WIDE, UF_NUM } FieldKind;

/* A numeric field (TUSF). */
typedef struct {
    uint32_t len;   /* its characters in the format */
    uint32_t width; /* its columns, the signs not counted */
    bool plus_lead; /* a leading + */
    uint8_t trail;  /* a trailing + or -, else 0 */
    bool stars;     /* ** */
    bool dollar;    /* a $ */
    bool commas;
    uint32_t before; /* places before the point */
    uint32_t after;  /* after it */
    bool point;
    uint32_t carets; /* 4 or 5 for an exponent, else 0 */
} UsngField;

/* PRINT USING's state */
typedef struct {
    uint8_t fmt[FMTMAX];
    uint32_t fmt_len, pos; /* the format's length, the place */
    UsngField f;
    uint8_t digits[NDIG + 20]; /* the number: 0.digits * 10^scale */
    int32_t count, scale;
    bool neg, sci; /* its sign; PRINT would give it an exponent */
    uint8_t body[FMTMAX + NDIG];
    uint8_t line[FMTMAX + NDIG + 8];
} Usng;

#endif
