/* PRINT USING (src/vm/using.asm, as llrm's runtime/qb/using.c). All are K handlers
   (vm/ops.def): US.BEGIN takes the format, each item goes to its next field, US.END writes the
   text up to the next field.

   The format is literal text and fields: ! the first character, & all of it, \  \ as wide as
   that, or a number: # a digit, a point, commas, a leading + or $$ or ** or **$, a trailing + or
   -, ^^^^ an exponent; _ makes the next character literal. The format starts again past its last
   field. */
#ifndef CORE_VM_USNG_H
#define CORE_VM_USNG_H
#include "vm/exec.h"
#endif
