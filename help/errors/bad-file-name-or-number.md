# Bad file name or number / Bad file name
keys: Bad file name or number, Bad file name, error 52, error 64

Errors 52 and 64

## Cause

A statement used a file number that is not open or is out of range, or
a file name that is empty or has characters DOS does not allow.

## Remedy

Open the file first, keep its number in a variable, and check the
name. Do not use a number again after CLOSE.

See also: [OPEN](OPEN), [CLOSE](CLOSE), [FREEFILE](FREEFILE)
