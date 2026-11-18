# Record and element errors
keys: Element not defined, Record variable required, ~Record/String assignment required, Typed variable not allowed in expression

A name is not a record field, or a record is used as a value.

See also: [TYPE](TYPE), [data types](data types)

## Details

Cause: A name is not a field of the record, a record is needed where
something else is given, or a whole record is used as a value.

Remedy: Check the field names in [TYPE](TYPE). Use `rec.field`, not
`rec`, in expressions.

Fields are reached as record.field.

## Example
    TYPE P
      x AS INTEGER
    END TYPE
    DIM r AS P
    PRINT r              ' error: record as value
    PRINT r.x            ' fixed: 0
