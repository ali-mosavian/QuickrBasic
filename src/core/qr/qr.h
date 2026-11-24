/* Quickr, the modern dialect (docs/language-design.md): running a .qr file. How it works: qr.c. */
#ifndef QR_QR_H
#define QR_QR_H
#include "base.h"

/* Is path a Quickr file: does it end in .qr, any case? */
bool qr_is_file(const char *path);
/* Load repl.file, a .qr, and report its compile errors; none: link it, ready to run. The errors'
   count. */
uint32_t qr_build(Session *S);
/* Run repl.file, a .qr: every compile error reported, then exit 1; --check (repl.checking): exit
   after the check. */
void qr_run(Session *S);
/* Runtime error code at a Quickr line (vm.line): file:line: message, then the line. */
void qr_report(Session *S, uint8_t code);
#endif
