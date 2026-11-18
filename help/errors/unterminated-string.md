# unterminated string
keys: unterminated string

A string literal has no closing quote on its line.

See also: [PRINT](PRINT)

## Details

Cause: A string literal has an opening quote but no closing one on the
line.

Remedy: Add the closing quote.

A string cannot continue onto the next line.

## Example
    PRINT "hello       ' error
    PRINT "hello"        ' hello
