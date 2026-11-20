/* What an IMPORT or FROM binds in the module that has it. How it works: bind.c. */
#ifndef CORE_COMP_BIND_H
#define CORE_COMP_BIND_H
#include "comp/import.h"
#include "mem/scope.h"
#include "mem/symbols.h"

/* An alias by FROM's *: it yields to the module's own name. */
enum : uint32_t { BIND_STAR = 0x80000000 };

/* Module m binds n (its AS name, else its text) to module to: IMPORT's, FROM's of a package; again
   to the same, nothing. Duplicate definition: m declares the name, or binds it otherwise. */
void bind_module(Scop *sc, Symb *sy, Fault *f, uint32_t m, const ImptName *n, uint32_t to);
/* Module m binds n (its AS name, else its text) to module from's export of n's text. Name not
   exported: from has none, or no array for n's (). Duplicate definition: m declares the name, or
   binds it already, or a variable's or CONST's suffix is not its type. */
void bind_name(Scop *sc, Symb *sy, Fault *f, uint32_t m, uint32_t from, const ImptName *n);
/* Module m binds each SUB, FUNCTION, CONST and TYPE module from exports, but its own names and
   those it binds already. One two *'s bind: Duplicate definition where it is used. */
void bind_star(Scop *sc, Symb *sy, uint32_t m, uint32_t from);
/* Name n declared by its module (DIM, CONST, SUB, FUNCTION, TYPE): a * binding of it dropped.
   false: a FROM's listed name, a module IMPORT binds (Duplicate definition). */
bool bind_own(Scop *sc, Symb *sy, uint32_t n);
/* Name n used: its error, NM_BAD's; Duplicate definition: a module IMPORT binds. */
void bind_use(const Symb *sy, Fault *f, uint32_t n);
/* Name n, COMMON's, stands for another module's COMMON variable, to. */
void bind_common(Scop *sc, Symb *sy, uint32_t n, uint32_t to);
/* Each alias's procedure, its exporter's: after link gave each procedure's name its own. */
void bind_procs(Symb *sy);
#endif
