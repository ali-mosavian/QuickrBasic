# Command Line
keys: command line, --check, basic --check, --test, /H

    basic [FILE.BAS]           run FILE and exit; none: READY
    basic --check FILE.BAS     check FILE and run nothing
    basic --test FILE.QR       run FILE's test blocks, not FILE
    IDE32 [/H] [FILE.BAS]      the editor, FILE opened; /H 50 lines

See also [QuickrBasic](QuickrBasic), [test scripts](test scripts)

## Details
BASIC32.EXE is basic on DOS.

--check
Compiles the whole file and reports every compile error, not only
the first: the message, the file and line with a mark at the place,
and a hint. Nothing runs. The exit status is 1 if any error was found
or the file could not be loaded, else 0, and a clean file prints
nothing. A block pairing error (a FOR without NEXT) is still reported
once, the first.

Use it in scripts and editors to syntax-check without the program's
side effects.

--test
Runs each test block of a Quickr file and of the modules it imports,
printing pass or FAIL for each, then the tally. The file's top level
runs its declarations alone. The exit status is 1 if a test failed.

## Example
    basic --check game.bas
    ' game.bas(12): Expected THEN      (a clean file prints nothing)
    echo $?
    ' 1                                (0 when clean)
