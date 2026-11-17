# num: float/ → core/num/, plat/pfpu.h

A FLOAT is a `Sngl` (`uint32_t`, packed IEEE single), in a Cell's `.u`; never pass a C `float`
(llrm keeps it at the x87's precision until stored). A DOUBLE is a Cell's `.d`. Binary FLOAT ops
are `fmth_op(a, b)` = a OP b: the asm's EDX (ARG) is `a`, EAX (FAC) is `b`. `err_ov` and the
other `err_*` are `repl_error(E_*)`.

## flt.asm → single

In `single.c`'s header.

## ftr.asm → random

In `random.c`'s header.

## fpu.asm → fpmath

In `fpmath.c`'s header.

## dbl.asm → double

In `double.c`'s header.

## plat/pfpu.h (DOS32: plat/dos32/pfpu.asm)

`pfpu_init`, `pfpu_reset`, `pfpu_fn`, `pfpu_dfn`, `pfpu_pow`, `pfpu_dpow`, `pfpu_scale10`,
`pfpu_digits`; status `PFPU_OK`, `PFPU_IQ`, `PFPU_OV`; functions `PF_SQR` .. `PF_EXP`.
