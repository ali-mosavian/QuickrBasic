# END Statement
keys: END

Syntax
    END

END stops the program. Open files are closed, and the run returns
to the editor, or to DOS for a compiled program. It may appear
anywhere, in a SUB too, and a program that runs off its last line
ends the same way. [STOP](STOP) is the same but leaves files open
for inspection while debugging.

END also closes blocks: END IF, END SELECT, END SUB, END FUNCTION,
END TYPE and END DEF. Those belong to the statement they close; see
[IF](IF), [SELECT CASE](SELECT CASE), [SUB](SUB), [FUNCTION](FUNCTION),
[TYPE](TYPE) and [DEF FN](DEF FN).

## Example
    INPUT "Quit"; a$
    IF UCASE$(a$) = "Y" THEN END
    PRINT "Still here"

See also [STOP](STOP), [SYSTEM](SYSTEM)
