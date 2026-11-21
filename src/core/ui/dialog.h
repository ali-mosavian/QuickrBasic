/* The dialogs's interface. How it works: dialog.c. */
#ifndef UI_DIALOG_H
#define UI_DIALOG_H
#include "ui/ui.h"

/* A control's kind, flags. */
enum : uint8_t { CT_FLD = 1, CT_LST = 2, CT_BTN = 3, CT_CHK = 4, CT_TXT = 5 };
enum : uint8_t {
    CF_NOF = 1,
    CF_PATH = 2
}; /* takes no focus; a text that is a path: cut in its middle */

/* The buttons the boxes end with (their control's index). */
enum : uint8_t {
    DO_OK = 4, /* Open's OK */
    DS_OK = 3, /* Save As's */
    DF_OK = 3, /* Find's */
    DC_FV = 4, /* Change's Find and Verify; DC_FV + 1 Change All */
    DLOG_ESC = 0xFF,
};

/* A hook's why: a button pressed; a list's selection moved; Enter in a list. */
enum : uint8_t { DLOG_PRESS = 0, DLOG_MOVED = 1, DLOG_ENTER = 2 };
/* A control's hook (TCTL.cb). true: the box stays (a button's: CF set; a list's Enter: CF
   clear, the asm's sense being the other way); false: a button ends the box, Enter in a list
   presses the default button. A list's DLOG_MOVED's result is not looked at. */
typedef bool (*DlogHook)(uint8_t why);

/* TLIST: a dialog's list. */
typedef struct {
    uint32_t n;   /* its items */
    uint32_t sel; /* the one selected */
    uint32_t top; /* the first shown */
    char *items;  /* n items, size bytes each, 0-ended */
    uint32_t size;
    bool columns; /* down, then across: as many columns as the box fits of the widest */
} DlogList;

/* TCTL */
typedef struct {
    uint8_t kind; /* CT_* */
    uint8_t row, col, cols, rows;
    uint8_t flags; /* CF_* */
    union {
        LineBuf *field;   /* CT_FLD */
        DlogList *list;   /* CT_LST */
        const char *text; /* CT_BTN, CT_CHK: its label; CT_TXT: its text (a buffer it shows) */
    };

    DlogHook hook;     /* CT_BTN, CT_LST: nullptr none */
    bool *check;       /* CT_CHK: its flag */
    const char *label; /* nullptr none */
    uint8_t lrow, lcol;
} DlogCtl;

/* TDLG */
typedef struct {
    uint8_t row, col, rows, cols;
    const char *title; /* nullptr none */
    uint8_t n;         /* its controls */
    uint8_t focus;     /* the one with the focus */
    uint8_t sep;       /* the separator's row, 0 none */
    uint8_t def;       /* the default button */
    DlogCtl *ctl;      /* n of them */
} Dlog;

/* TBTN: a message box's button; a list of them ends with text nullptr. */
typedef struct {
    const char *text;
    uint8_t cols; /* its width, 0 its text's + 4 */
} MsgButton;

/* dl_run: d until a button: its index, DLOG_ESC for Esc; uimn.help the dialog's status line
   meanwhile. */
uint8_t dlog_run(Session *S, Dlog *d);
/* dl_msg: a message box: text's lines split at '|', centred (dlog_err: left-aligned under a
   title), the buttons; the button's index, DLOG_ESC for Esc. */
uint8_t dlog_msg(Session *S, const char *text, const MsgButton *buttons);
/* dl_ok: a message box with OK (and Help). */
void dlog_ok(Session *S, const char *text);
/* dlog_ok's box titled title, its lines left-aligned under it. */
void dlog_text(Session *S, const char *title, const char *text);
/* dl_ync: a message box with Yes, No, Cancel (and Help): 0 Yes, 1 No, 2 Cancel or Esc. */
uint8_t dlog_ync(Session *S, const char *text);
enum : uint8_t { YNC_YES = 0, YNC_NO = 1 };
/* dl_err: dlog_ok for an error at the cursor's line: the box off that line; text past its first
   '|' (the '|' made 0) left-aligned under the first line, its title; the selection off after. */
void dlog_err(Session *S, char *text);
/* dl_wrap: len bytes at src to dst as a box's lines, each indent spaces in: '|' at a space past
   44 columns; a '|' of src a box's line drawing character (B3h). Past them in dst. */
uint8_t *dlog_wrap(uint8_t *dst, const uint8_t *src, uint32_t len, uint8_t indent);

/* dl_open: File, Open's box (*.BAS); true: uimn.chosen the file. */
bool dlog_open(Session *S);
/* dl_saveas: Save As's box, the field the file's name; true: uimn.chosen the file. */
bool dlog_saveas(Session *S);
/* dl_fok: OK in Open and Save As (a button's hook, also uimn_start's): uimn.fname a mask (* ?)
   lists it, the box stays (true); empty: stays; else the name, .BAS added when it has no dot, in
   uimn.chosen (false). */
bool dlog_fileok(uint8_t why);
/* dl_bhelp: a Help button's hook: no help to show; a beep, the box stays (true). */
bool dlog_nohelp(uint8_t why);
/* dl_find: Search, Find's box (uimn.find_what, match_case, whole_word); true: OK. */
bool dlog_find(Session *S);
/* dl_conf: Find and Verify's question: its buttons only, at row 19, or row 2 when the cursor
   is that low: 0 Change, 1 Skip, else cancelled. */
uint8_t dlog_conf(Session *S);
enum : uint8_t { CONF_CHANGE = 0, CONF_SKIP = 1 };
/* dl_change: Search, Change's box: 0 Find and Verify, 1 Change All, else cancelled. */
uint8_t dlog_change(Session *S);
enum : uint8_t { CHANGE_VERIFY = 0, CHANGE_ALL = 1 };
/* ui_ledit: an editing key in the one-line buffer b (a field, the immediate line): a character
   typed (FLDMAX at most), Backspace, Left, Right, Home, End, Del, Ctrl+Left, Ctrl+Right. */
bool dlog_ledit(LineBuf *b, uint16_t key);
/* Names a, b compared, case aside: a list's order. */
int dlog_namecmp(const char *a, const char *b);
/* The dialog's modal widget, a child of screen, hidden till dlog_run. */
void dlog_build(Widget *screen); /* false: not a line edit's key */
#endif
