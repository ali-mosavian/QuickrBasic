# Record and element errors
keys: Element not defined, Record variable required, ~Record/String assignment required, Typed variable not allowed in expression

## Cause

A name is not a field of the record, a record is needed where
something else is given, or a whole record is used as a value.

## Remedy

Check the field names in [TYPE](TYPE). Use `rec.field`, not `rec`, in
expressions.

See also: [TYPE](TYPE), [data types](data types)
