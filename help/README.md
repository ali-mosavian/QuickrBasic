# Help sources

One Markdown file per topic under `help/`. `python3 tools/mkhelp.py` compiles them, with
`status.txt`, into `BASIC.HLP`; `tools/mkhelp_test.py` checks the compiler.

## A topic

```
# PRINT Statement
keys: PRINT, ?, LPRINT

Syntax
    PRINT [expressionlist][{; | ,}]

Body text, hard-wrapped at 70 columns. A [link](KEY) jumps to the topic that has KEY.

## Example
    PRINT "Hello"
```

- The first line is the title, shown after `HELP:` in the window's border.
- `keys:` lists the words F1 finds the topic by, comma-separated, case-insensitive. A key is a
  keyword (`LEFT$`), a phrase (`data types`) or an error's message. The file name is a key too.
  Two topics may not share a key. A key starting with `~` is found by F1 but left out of the Index.
- A line starting `## ` is a heading, drawn bright. Fenced code (```) is kept as typed, with no links.
- `[text](KEY)` is a hot link, drawn `◄text►`; the arrows take two columns. KEY must exist.
- Backticks are dropped; no other Markdown. Lines fit in 76 columns, links counted with their arrows.
- Characters must exist in code page 437 (box drawing, arrows, accents).
- Every topic ends with a short `See also` line of links where useful.

## Where

| Directory | Holds |
|---|---|
| `lang/` | keywords: one topic per statement, function or operator |
| `ref/` | concepts and tables: data types, operators, PRINT USING, PLAY and DRAW strings, SCREEN modes |
| `ide/` | the editor: menus, dialogs, keys, the debugger |
| `errors/` | each error message: cause, remedy |
| `ext/` | what QuickrBasic adds to QBasic |
| `qr/` | Quickr: its index, the language, one per standard library module; keys `~quickr word` |
| `status.txt` | one status-bar line per menu item, in menu order |

Required topics: `contents` (the table of contents, key `contents`), `using-help` (key `using help`),
`survival`. The Index is built from every key.

Write in your own words. Say what QuickrBasic does, not what QBasic documented.
