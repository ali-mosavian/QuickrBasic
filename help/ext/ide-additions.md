# IDE Additions
keys: ide additions, IDE extras, Step Out, Go to Definition

What the editor does that QBasic's did not: highlighting, line
checking, completion, a debug pane, nine themes, split windows.

See also [QuickrBasic](QuickrBasic), [command line](command line),
[test scripts](test scripts)

## Details
Menu commands are in [menus](menus).

 - Highlighting: keywords, numbers, strings, comments and names in
   their own colours ([Syntax Highlighting](syntax highlighting)).
   It comes from the compiler's lexer, so it never disagrees with it.
 - [Undo](Undo Command) and [Redo](Redo Command): Ctrl+Z and
   Ctrl+Shift+Z, back to when the program was opened.
 - Each line is checked as it is entered
   ([syntax checking](syntax checking)). An error is lit, not boxed;
   the cursor moves on and Ctrl+F8 goes back to it.
 - Find opens a bar on the status row, not a box: the match moves as
   you type, through every SUB and module ([Find Bar](Find Bar)).
 - Ctrl+Backspace and Ctrl+Del delete a word; a double click selects
   one, a triple click the line.
 - Alt+Up and Alt+Down move lines, with Shift copy them, and Ctrl+/
   comments them out ([editing keys](editing keys)).
 - [Completion](completion): Tab lists what fits at the cursor.
 - [Debug pane](debug pane): at a stop, Watch, Locals and Call stack
   beside the code. Records and arrays open in Locals, values can be
   set, and Break When (Ctrl+F9) stops the run when an expression
   turns true or changes.
 - Debug, Step Out (Shift+F8) runs on until the current procedure
   returns, then stops at the next statement in its caller. Step (F8)
   and Step Over (F7) run one statement, into calls or over them
   ([Step Out Command](Step Out Command)).
 - Breakpoints (F9) move with their line when lines are added above.
 - [Themes](themes): nine, in Options, Theme.
 - [Display size](display size): 25, 43 or 50 lines. The command-line
   switch /H starts the editor with 50.
 - View, Split divides the screen into two windows on the same
   program; Split Vertically places them side by side. A line between
   windows can be dragged with the mouse, and so can the line between
   the code and the debug pane. Lines can be dragged between windows.
 - Go to Definition: Ctrl+B, or Ctrl+click, on a name shows the line
   that defines it: its SUB or FUNCTION, DIM, CONST, TYPE, parameter
   or label, in another file for a name an IMPORT gives. Ctrl+Alt+B,
   or Ctrl+Alt+click, shows it in the other window of a split, split
   side by side first when there is none.
 - F2 lists procedures sorted by name, in columns. The Open box lists
   files sorted, in three columns, and shows a long path shortened.
 - A program's graphics screen is kept while the editor is shown.

## Example
Step out of a procedure:
    Type a program with a SUB that calls PRINT, press F9 on a line
    inside the SUB to set a breakpoint
    Shift+F5        run: the program stops at the breakpoint
    Shift+F8        Step Out: runs until the SUB returns, stops on
                    the statement after the CALL
    F8              Step: one statement more
