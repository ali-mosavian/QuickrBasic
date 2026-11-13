/* The process: the exit, the command line, the environment, SHELL's EXEC, the I/O ports, and
   the STOP key. On DOS32 the STOP key is Ctrl+C or Ctrl+Break: INT 23h when DOS sees it, and
   the tick (INT 1Ch) scanning the BIOS key buffer for one, as DOS sees one only when it reads
   the console, which a running loop never does. */
#ifndef PLAT_PSYS_H
#define PLAT_PSYS_H
#include "base.h"

/* The STOP key's handlers in: each ORs bit into *stop (exec's stop flags, SF_BRK); the tick
   then calls tick (evnt's, ON TIMER and ON KEY; nullptr none), in the interrupt: it touches
   only memory it alone writes and flags. */
void psys_init(volatile uint8_t *stop, uint8_t bit, void (*tick)(void));
/* The handlers out, exit with code. Standard output is the caller's to flush first. */
[[noreturn]] void psys_exit(uint8_t code);

/* The command line after the program's name, as typed (DOS's PSP tail, its CR not counted);
   cap bytes at most, 0-ended; its length. */
uint32_t psys_cmdline(char *buf, uint32_t cap);
/* The environment at the start: NAME=value 0 each, a 0 after the last; cap bytes at most;
   its bytes, the last 0 counted. */
uint32_t psys_environ(char *buf, uint32_t cap);
/* Run the program at path with the command tail (len bytes, no count nor CR), the child given
   env (envlen bytes, as psys_environ's); false: it did not run. */
bool psys_exec(const char *path, const char *tail, uint8_t len, const char *env,
               uint32_t envlen);

/* INP, OUT: an I/O port's byte. */
uint8_t psys_inp(uint16_t port);
void psys_outp(uint16_t port, uint8_t v);

/* SYS: call the machine code at a, in the POKE memory (declared by vm). */
void psys_call(Addr a);
#endif
