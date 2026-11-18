# IDE Additions
keys: ide additions, IDE extras, Step Out

What the editor does that QBasic's did not. Menu commands are in
[menus](menus).

 - Highlighting: keywords, numbers, strings, comments and names in
   their own colours ([Syntax Highlighting](syntax highlighting)).
   It comes from the compiler's lexer, so it never disagrees with it.
 - Each line is checked as it is entered
   ([syntax checking](syntax checking)).
 - [Completion](completion): Tab lists what fits at the cursor.
 - [Debug pane](debug pane): at a stop, Watch, Locals and Call stack
   beside the code.
 - Debug, Step Out (Shift+F8) runs on until the current procedure
   returns, then stops at the next statement in its caller. Step (F8)
   and Step Over (F7) run one statement, into calls or over them
   ([Step Out Command](Step Out Command)).
 - Breakpoints (F9) move with their line when lines are added above.
 - [Themes](themes): nine, in Options, Theme.
 - [Display size](display size): 25, 43 or 50 lines. The command-line switch
   /H starts the editor with 50.
 - View, Split divides the screen into two windows on the same
   program; Split Vertically places them side by side. A line between
   windows can be dragged with the mouse, and so can the line between
   the code and the debug pane. Lines can be dragged between windows.
 - F2 lists procedures sorted by name, in columns. The Open box lists
   files sorted, in three columns, and shows a long path shortened.
 - A program's graphics screen is kept while the editor is shown.

See also [QuickrBasic](QuickrBasic), [command line](command line),
[test scripts](test scripts)
