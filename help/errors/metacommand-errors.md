# $Metacommand Error
keys: $Metacommand Error, Form variable already in use or illegal form name, Form variable already in use or illegal

A $ metacommand comment is written wrong.

See also: [metacommands](metacommands)

## Details

Cause: A metacommand comment is written wrong: `'$INCLUDE: 'file'`
without its quotes, `$LINESIZE` without a size, or a bad `$FORM` name.

Remedy: Match the form in the metacommand topic.

Metacommands live in comments: '$INCLUDE, '$DYNAMIC, '$STATIC, '$FORM
and '$LINESIZE.

## Example
    '$INCLUDE: file.bi   ' error: quotes missing
    '$INCLUDE: 'file.bi'  ' fixed
