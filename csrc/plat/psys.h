/* The process: the exit, the command line, the environment, SHELL's EXEC, the I/O ports, and
   the STOP key. On DOS32 the STOP key is Ctrl+C or Ctrl+Break: INT 23h when DOS sees it, and
   the tick (INT 1Ch) scanning the BIOS key buffer for one, as DOS sees one only when it reads
   the console, which a running loop never does. */
#ifndef PLAT_PSYS_H
#define PLAT_PSYS_H
#include <cdecl.h>
#include "base.h"

/* The STOP key's handlers in: each ORs bit into *stop (exec's stop flags, SF_BRK). No C runs in
   an interrupt: ON TIMER's and ON KEY's tick is pclk's (pclk_init), polled by the core. */
void psys_init(volatile uint8_t *stop, uint8_t bit);
/* The handlers out, exit with code. Standard output is the caller's to flush first. */
[[noreturn]]
CDECL void psys_exit(uint8_t code);

/* The command line after the program's name, its leading blanks skipped (DOS's PSP tail);
   cap bytes at most, 0-ended; its length. */
uint32_t psys_cmdline(char *buf, uint32_t cap);
/* The environment at the start: NAME=value 0 each, a 0 after the last; cap bytes at most;
   its bytes, the last 0 counted. */
uint32_t psys_environ(char *buf, uint32_t cap);
/* The running program's own path, as DOS put it after the environment (drive, directory, name);
   cap bytes at most, 0-ended; its length, 0 unknown (declared by ui). */
uint32_t psys_self(char *buf, uint32_t cap);
/* Run the program at path with the command tail (len bytes, no count nor CR), the child given
   env (envlen bytes, as psys_environ's); false: it did not run. */
bool psys_exec(const char *path, const char *tail, uint8_t len, const char *env, uint32_t envlen);

/* INP, OUT: an I/O port's byte. */
CDECL uint8_t psys_inp(uint16_t port);
CDECL void psys_outp(uint16_t port, uint8_t v);

/* SYS: call the machine code at a, in the POKE memory (declared by vm). */
CDECL void psys_call(Addr a);
#endif
