/* Quickr's native entries: a native fn's code, by its name. How it works: qnat.c. */
#ifndef QR_QNAT_H
#define QR_QNAT_H
#include "base.h"

/* A step of an entry's code: an argument pushed, an op, an int, a CALL, a SYSCALL, a CALL QX, an
   X op of argument n's number (QrtNum): a generic's, of any number. */
typedef enum : uint8_t { QN_ARG, QN_OP, QN_OPB, QN_INT, QN_CALL, QN_SYS, QN_X, QN_NUM } QNatKind;

typedef struct {
    QNatKind kind;
    uint8_t op; /* QN_OP's, QN_OPB's op; QN_CALL's K_*, QN_SYS's SC_*, QN_NUM's X_* */
    int32_t n;  /* QN_ARG's argument, QN_OPB's byte, QN_INT's value, QN_X's operand */
} QNatStep;

typedef struct QNat {
    const char *name;
    const QNatStep *steps;
    uint32_t n;
} QNat;

/* The entry named name, null none. */
const QNat *qnat_find(const char *name, uint32_t len);
#endif
