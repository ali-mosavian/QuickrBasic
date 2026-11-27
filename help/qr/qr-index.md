# Quickr
keys: ~quickr, ~quickr index, ~quickr help

Quickr is QuickrBasic's modern dialect. A file is Quickr's when its
first statement is option dialect quickr; any other file is QBasic's.

  [The language](quickr language)

  Standard library modules:
  [io](quickr io)          the console
  [text](quickr text)        strings: search, case, split, join
  [math](quickr math)        numbers, vectors
  [random](quickr random)      random numbers
  [time](quickr time)        the clock and the date
  [fs](quickr fs)          files
  [os](quickr os)          the command line and environment
  [keys](quickr keys)        the keyboard and the mouse

See also [The language](quickr language)

## Details
F1 on a Quickr keyword opens the language's topic; on a module's name,
or on a name after it (text.upper), that module's topic. F1 on any
other word opens this one.

A module is imported by name: import text, then text.upper(s). print
needs no import.

## Example
    option dialect quickr
    import text
    print text.upper("hello")
