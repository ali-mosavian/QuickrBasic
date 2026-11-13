/* Numbers as their bytes (src/vm/bin.asm): MKI$ MKL$ MKS$ MKD$, CVI CVL CVS CVD, and the Microsoft
   Binary Format's MKSMBF$ MKDMBF$ CVSMBF CVDMBF. All are K handlers (vm/ops.def).

   MBF: the significand low first, its sign in the top bit of the byte under the exponent; the
   exponent last, 0 for zero, biased 129 with the point before the hidden 1 (IEEE's + 2 single,
   - 894 double). */
#ifndef CORE_VM_MKCV_H
#define CORE_VM_MKCV_H
#include "vm/exec.h"
#endif
