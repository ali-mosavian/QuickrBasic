/* The editor: its start, the key loop, the menu commands, running the program (src/ui/ui.asm).
   Page 0 is the editor's, page 1 the program's output. A run enters the text again when it
   changed (repl_relink); an error, BREAK or STOP comes back to the editor at its line. The
   editor runs only when standard input and output are both the console. uimn itself is ui.h's. */
#ifndef UI_UIMAIN_H
#define UI_UIMAIN_H
#include "io/repl.h"
#include "ui/ui.h"

/* The editor's part at the READY loop's places: uimn_start, then uimn_ready and uimn_error
   while it is on. */
extern const FrontEnd uimn_front_end;

/* ui_start (repl_main's REPL_START): when both standard handles are the console, the editor
   (a file the command line names, else the welcome box), never returning; else returns, and
   the READY loop runs. */
void uimn_start(Session *S);
/* ui_ready (the READY loop's place, uimn.on): the output page until a key, unless an immediate
   line printed nothing; then the editor. */
[[noreturn]] void uimn_ready(Session *S);
/* ui_err (repl's report, uimn.on): an error code (E_*), or E_BRK for BREAK and STOP, in the
   editor: the cursor at its line, the message in a box, a stop's debug pane. repl_error's
   stacks are reset already. */
[[noreturn]] void uimn_error(Session *S, uint8_t code);

/* ui_getkey: a key (scan << 8 | char, KEY_ALT, KEY_MOUSE), the screen shown and the mouse polled
   while none waits; uimn.keyflags the shift flags then. An Alt already down at the call (after
   Alt+F) is no tap when let go. */
uint16_t uimn_getkey(void);
/* ui_key: key: the menus, the run and window keys, then the focused window's. */
void uimn_key(Session *S, uint16_t key);
/* ui_act: menu command action (ACT_*, menu.h); ACT_NONE and past: nothing. */
void uimn_act(Session *S, uint8_t action);

/* ui_stopped: a stop to go on from, the text as it was (uimn.stopped, !uimn.text.changed). */
bool uimn_stopped(void);
/* ui_cancont: the line committed; then uimn_stopped. false: CONT cannot go on. */
bool uimn_cancont(Session *S);
/* ui_runp: Run, Start: the text entered again if changed, the debugger prepared, the output
   page cleared, RUN. */
[[noreturn]] void uimn_run(Session *S);
/* ui_contp: Run, Continue: CONT after a stop, else uimn_run. */
[[noreturn]] void uimn_cont(Session *S);
/* ui_selw: the cursor at column col of the cursor's line, the word there selected. */
void uimn_selword(uint16_t col);
/* ui_dec: v in decimal at dst; past it. */
char *uimn_dec(char *dst, uint16_t v);

/* The status bar's keys: the editor's, a stop's (Continue), the immediate window's
   (u_kedit, u_krun, u_kimm). */
extern const StatusKeys uimn_keys_edit, uimn_keys_run, uimn_keys_imm;
/* Strings more than one module shows. uimn.help is one of the last two while the menus or a
   dialog run: the status bar then shows "F1=Help" as their own. */
extern const char uimn_ok[];        /* u_ok: "OK" */
extern const char uimn_cancel[];    /* u_cancel: "Cancel" */
extern const char uimn_helpbtn[];   /* u_bhelp: "&Help" */
extern const char uimn_untitled[];  /* u_untl: "Untitled" */
extern const char uimn_menu_help[]; /* u_mbar: the menus' status line */
extern const char uimn_dlog_help[]; /* u_dlg: a dialog's status line */
#endif
