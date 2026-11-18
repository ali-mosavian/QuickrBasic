# Bad record length / Bad record number
keys: Bad record length, Bad record number, error 59, error 63

Errors 59 and 63

A GET or PUT variable does not match the record length, or the record
number is below 1.

See also: [OPEN](OPEN), [GET](GET), [PUT](PUT), [LEN](LEN)

## Details

Cause: A GET or PUT variable does not match the file's record length,
or the record number is zero or negative.

Remedy: Make LEN in OPEN equal the size of the record variable. Record
numbers start at 1.

Error 59 is Bad record length; error 63 is Bad record number. The
default record length is 128.

Trap it with ON ERROR; ERR returns the number.

## Example
    OPEN "R.DAT" FOR RANDOM AS #1 LEN = 4
    a& = 7
    PUT #1, 0, a&        ' error 63: records start at 1
    PUT #1, 1, a&        ' fixed
    CLOSE #1
