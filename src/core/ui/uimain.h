/* The editor's interface. How it works: uimain.c. */
#ifndef UI_UIMAIN_H
#define UI_UIMAIN_H
#include "io/repl.h"
#include "ui/ui.h"

/* The editor's part at the READY loop's places: uimn_start. */
extern const FrontEnd uimn_front_end;

/* ui_start (repl_main's start): when both standard handles are the console, the editor (a file
   the command line names, else the welcome box), never returning; else returns, and the READY
   loop runs. Each key's command runs under repl_call: an error comes back to the key loop. */
void uimn_start(Session *S);
/* The editor's state at its start, an empty program, the screen aside (uimn_start's). */
void uimn_init(Session *S);
/* File, Open's: the program at name and the modules it imports read; one not found errors at the
   run. Not there: a box. */
void uimn_open(Session *S, const char *name);
/* File, Save: the program and each module changed written (unnamed: Save As). false: not saved. */
bool uimn_save(Session *S);
/* ui_sync: a run's first step: the program entered again when the text changed. Errors: as
   repl_relink, at its line. */
void uimn_sync(Session *S);
/* The screen of size, one of uimn.sizes (80x25 if not): the text mode set, the windows fitted
   to it. */
void uimn_size(PscrSize size);

/* ui_getkey: a key (scan << 8 | char, KEY_ALT, KEY_MOUSE), the screen shown and the mouse polled
   while none waits; uimn.keyflags the shift flags then. An Alt already down at the call (after
   Alt+F) is no tap when let go. */
uint16_t uimn_getkey(void);
/* ui_key: key: Alt and the mouse; else to the focused window, then up the tree to the screen's
   run, window and menu keys. */
void uimn_key(Session *S, uint16_t key);
/* The key loop's: key's command (uimn_key) one undo step, its error shown. */
void uimn_command(Session *S, uint16_t key);
/* A modal widget w (a menu, a dialog) shown and run, every key and click its, till it hides. */
void uimn_modal(Session *S, Widget *w);
/* ui_act: menu command action (ACT_*, menu.h); ACT_NONE and past: nothing. */
void uimn_act(Session *S, uint8_t action);

/* ui_stopped: a stop to go on from, the text as it was (uimn.stopped, !uimn.text.changed). */
bool uimn_stopped(void);
/* ui_cancont: the line committed; then uimn_stopped. false: CONT cannot go on. */
bool uimn_cancont(Session *S);
/* ui_runp: Run, Start: the text entered again if changed, the debugger prepared, the output
   page cleared, RUN; its end or error shown. */
void uimn_run(Session *S);
/* ui_contp: Run, Continue: CONT after a stop, else uimn_run. */
void uimn_cont(Session *S);
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
