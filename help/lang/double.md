# DOUBLE Data Type
keys: DOUBLE

Syntax
    DIM x AS DOUBLE
    x# = 1D+100

A DOUBLE is an 8-byte floating-point number: 15 to 16 significant
digits, magnitude about 2.2E-308 to 1.8E+308. The suffix is #, and
DEFDBL makes it the default for a letter range. Literals are
DOUBLE when written with # or a D exponent, or when they have more
than 7 digits.

CDBL converts any number. Operations on a SINGLE and a DOUBLE give a
DOUBLE. Results that overflow are an Overflow error; there are no
infinities or NaNs.

    PRINT 1 / 3          ' 0.3333333
    PRINT 1# / 3         ' .3333333333333333

See also [SINGLE](SINGLE), [CDBL](CDBL), [DEFtype](DEFINT),
[data types](data types)
