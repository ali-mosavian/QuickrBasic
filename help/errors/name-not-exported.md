# Name not exported
keys: Name not exported, error 239

Error 239

A name is used from a module that does not export it.

See also: [IMPORT](IMPORT)

## Details

Cause: [FROM](IMPORT) lists a name the module does not export, or
`module.name` names one. A module exports its SUBs, FUNCTIONs, CONSTs
and TYPEs but the PRIVATE ones, and its DIM PUBLIC variables; not a
DIM SHARED variable, and not a name it imports itself.

Remedy: Export it from the module: drop its PRIVATE, or declare the
variable with DIM PUBLIC. A name the module imports, import from the
module that declares it.

## Example
    ' gfx.bas
    DIM SHARED pages
    ' main.bas
    FROM gfx IMPORT pages    ' error 239: DIM SHARED, not exported
    ' fix, in gfx.bas
    DIM PUBLIC pages
