/* The editor's shared constants, records and state, for every module in src/core/ui.
 *
 * # How it works
 * uimn (uimain.c) defines uimn. A field of the asm's g_ui that only one module used is that
 * module's static instead.
 *
 * The screen: row 0 the menu bar, rows 1 to 23 the windows (window.h), row 24 the status bar. A
 * place on it is a row and a column, 0-based. Drawing names attribute slots (SL_*), which the theme
 * (theme.h) makes attributes.
 *
 * The editor's key loop never returns. Each command runs under repl_call (io/repl.h), and a run
 * under its own: an error or a run's end unwinds the C stack to it and comes back as a Status.
 *
 * # asm structure (retired/asm32/ui/ui.inc)
 * | asm | here | |
 * |---|---|---|
 * | g_ui | uimn | MIRROR; D1 |
 * | TLBUF, TEDBUF | LineBuf | D2 |
 * | TTEXT | UiText | MIRROR |
 * | TEDIT, TWIN, TIMM, TMOUSE | UiEdit, UiWin, UiImm, UiMouse | MIRROR |
 * | data after u_kedit .. | StatusKey, StatusKeys | MIRROR |
 * | A_ED .. (NATTR) | SL_ED .. (NSLOT) | D3 |
 * | MS_KEY | KEY_MOUSE | MIRROR |
 * | IMN, IMW, ED_COLS, DLBUF, FLDMAX, TXMAX, LNMAX, LNBUF, HLPSZ | same | MIRROR |
 * | NWIN, W_HELP, W_ED1, W_ED2, W_IMM, W_DBG | same | MIRROR |
 * | DP_COL, DP_W, DP_TW | same | MIRROR |
 * | g_clpb | uimn.text.clip, clip_cap (CLIPSZ at most) | MIRROR; grown |
 *
 * - D1. Zeroed, so out of the image's data: uimn_start sets what needs a value. The dialog line
 *   buffers' texts are arrays in it, their LineBufs pointing at them; the editor's lines, the
 *   clipboard and BASIC.HLP's text are on the C side's heap, grown as they fill.
 * - D2. text points at the storage; a field's and the immediate line's hold DLBUF bytes, the
 *   editor's lines grow to LNBUF.
 * - D3. Renamed: comp's dfa.h has an A_STR.
 */
#ifndef UI_UI_H
#define UI_UI_H
#include "defs.h"
#include "plat/pscr.h"
#include "ui/tree.h"

enum {
    HLPSZ = 2048,          /* BASIC.HLP's bytes at most: the menus' help lines */
    DLBUF = 260,           /* a dialog's line buffer's text */
    FLDMAX = 80,           /* a dialog field's characters at most */
    TXMAX = 0xFF00,        /* the text's bytes at most */
    LNMAX = TXMAX - 0x100, /* an editor line's characters at most */
    LNBUF = LNMAX + 4,     /* an editor line buffer's text */
    ED_COLS = 78,          /* the immediate window's text columns */
    IMN = 10,              /* the immediate window's lines kept */
    IMW = 80,              /* each: its length, its text */
    CLIPSZ = 0x10000,      /* the clipboard's bytes (the asm's g_clpb) */
};

/* Attribute slots (the asm's A_ED ..): each theme's attribute for it (theme.h). */
enum : uint8_t {
    SL_ED,   /* the edit windows' text, frames */
    SL_SEL,  /* the selection */
    SL_TTL,  /* the focused window's title */
    SL_BAR,  /* the menu bar, scroll bars */
    SL_HOT,  /* a hotkey on the bar */
    SL_STAT, /* the status bar */
    SL_MSEL, /* the menu item lit */
    SL_MHOT, /* its hotkey */
    SL_GRAY, /* a gray item */
    SL_BPL,  /* a breakpoint's line */
    SL_CUR,  /* the statement a stop or trace is at */
    SL_DLG,  /* dialogs */
    SL_DSEL, /* a dialog's selected item */
    SL_DFOC, /* a button with the focus */
    SL_SHD,  /* a box's shadow */
    SL_THB,  /* a scroll bar's thumb */
    SL_KW,   /* synt: keywords */
    SL_STR,  /*   strings */
    SL_NUM,  /*   numbers */
    SL_COM,  /*   comments */
    SL_NAM,  /*   names */
    SL_FNM,  /*   a SUB's or FUNCTION's name where declared */
    SL_PAR,  /*   its parameters */
    SL_TYP,  /*   types */
    SL_OP,   /*   operators */
    NSLOT,   /* the asm's NATTR */
};

/* A key as uimn_getkey gives it: scan code << 8 | character; the character 0 for a key with
   none (an E0h one too). */
enum : uint16_t {
    KEY_ALT = 0xFFFF,   /* Alt pressed and let go alone */
    KEY_MOUSE = 0xFFFE, /* a mouse event: uimn.mouse (MS_KEY) */
};

/* A key's scan code (its high byte). */
enum : uint8_t {
    SCAN_F1 = 0x3B,
    SCAN_F10 = 0x44,
    SCAN_SHIFT_F1 = 0x54,
    SCAN_CTRL_F1 = 0x5E,
    SCAN_ALT_F1 = 0x68,
    SCAN_HOME = 0x47,
    SCAN_UP = 0x48,
    SCAN_PGUP = 0x49,
    SCAN_LEFT = 0x4B,
    SCAN_RIGHT = 0x4D,
    SCAN_END = 0x4F,
    SCAN_DOWN = 0x50,
    SCAN_PGDN = 0x51,
    SCAN_INS = 0x52,
    SCAN_DEL = 0x53,
    SCAN_CTRL_LEFT = 0x73,
    SCAN_CTRL_RIGHT = 0x74,
    SCAN_CTRL_END = 0x75,
    SCAN_CTRL_PGDN = 0x76,
    SCAN_CTRL_HOME = 0x77,
    SCAN_CTRL_PGUP = 0x84,
    SCAN_ESC = 0x01,
    SCAN_ENTER = 0x1C,
    SCAN_L = 0x26,
    SCAN_GREY_MINUS = 0x4A,
    SCAN_GREY_PLUS = 0x4E,
    SCAN_ALT_MINUS = 0x82,
    SCAN_ALT_EQUALS = 0x83,
    SCAN_CTRL_INS = 0x92,
    SCAN_ALT_UP = 0x98,
    SCAN_ALT_LEFT = 0x9B,
    SCAN_ALT_RIGHT = 0x9D,
    SCAN_ALT_DOWN = 0xA0,
    SCAN_ALT_PAD_ENTER = 0xA6,
    SCAN_ALT_FIRST = 0x10,
    SCAN_ALT_LAST = 0x32, /* Alt+letter's: Q .. M */
};
enum : uint16_t { KEY_SHIFT_TAB = 0x0F00 };
/* A key's character (its low byte). */
enum : uint8_t { CH_BACKSPACE = 0x08, CH_TAB = 0x09, CH_ENTER = 0x0D, CH_ESC = 0x1B, CH_E0 = 0xE0 };

enum : uint8_t { TAB_STOP = 8 };
/* The cursor's shapes (pscr_shape). */
enum : uint16_t { CURSOR_INSERT = 0x0607, CURSOR_OVERWRITE = 0x0407 };
/* Code page 437's glyphs the screens draw. */
enum : uint8_t {
    GLYPH_POINT_RIGHT = 0x10,
    GLYPH_POINT_LEFT = 0x11,
    GLYPH_UP = 0x18,
    GLYPH_DOWN = 0x19,
    GLYPH_RIGHT = 0x1A,
    GLYPH_LEFT = 0x1B,
    GLYPH_SHADE = 0xB0,
    GLYPH_V = 0xB3,
    GLYPH_V_LEFT = 0xB4,
    GLYPH_TOP_RIGHT = 0xBF,
    GLYPH_BOTTOM_LEFT = 0xC0,
    GLYPH_V_RIGHT = 0xC3,
    GLYPH_T_DOWN = 0xC2,
    GLYPH_SQUARE = 0xFE,
    GLYPH_H = 0xC4,
    GLYPH_BOTTOM_RIGHT = 0xD9,
    GLYPH_TOP_LEFT = 0xDA,
};

/* uimn.tree.workspace.focus: which window has the keys (ui_foc), its child by index. */
enum : uint8_t { FOC_EDIT = 0, FOC_IMM = 1, FOC_PANE = 2 };

/* The windows, top down (window.h); the debug pane is beside them, not in uimn.win.win[]. */
enum : uint8_t { W_HELP = 0, W_ED1 = 1, W_ED2 = 2, W_IMM = 3, NWIN = 4, W_DBG = NWIN };

/* The debug pane's columns (debugpane.h): the edit windows have uimn.dp_col, DP_COL at first, the
   pane the rest, their last one the line between them; a drag of it moves it within DP_COLMIN ..
   DP_COLMAX. A row of the pane holds DP_TWMAX characters at most. */
enum {
    DP_COL = 50,
    DP_COLMIN = 20,
    DP_COLMAX = PSCR_COLS - 20,
    DP_TWMAX = PSCR_COLS - DP_COLMIN - 1,
};

/* A line being edited (TLBUF, TEDBUF): its text where text points, cap bytes: DLBUF for a dialog
   field or the immediate line; the editor's lines' grow, as they fill, to LNBUF (edit_lb_room).
   The cursor may be past the text: typing there pads it with spaces. */
typedef struct {
    uint16_t col; /* the cursor's column */
    uint16_t len;
    bool dirty; /* changed since loaded (dty) */
    bool grows; /* text on the C side's heap */
    uint8_t *text;
    uint32_t cap;
} LineBuf;

/* The status bar's keys for the mouse (the data after u_kedit ..): a key's last column, its
   command; text the bar's line. */
typedef struct {
    uint8_t last;
    void (*run)(Session *S);
} StatusKey;
typedef struct {
    const char *text;
    StatusKey keys[6]; /* to the first with run nullptr */
} StatusKeys;

/* The text: the program store's lines and the editor's view of them (TTEXT); text.h. */
typedef struct {
    uint32_t n;                                 /* tx_n: the view's lines, from first */
    uint32_t first;                             /* tx_v0: the view's first line in the text */
    uint32_t all;                               /* tx_all: the text's lines */
    void (*hook)(uint32_t line, int32_t delta); /* tx_hook: text line added (1), removed (-1) */
    bool changed;      /* tx_mod: since the program was entered (a run enters it again) */
    bool unsaved;      /* tx_chg: since it was saved */
    bool check;        /* syn_on: Options, Syntax Checking */
    bool asis;         /* tx_asis: the next line stored as typed */
    uint16_t clip_len; /* cb_len: the clipboard's bytes */
    bool clip_lines;   /* cb_ln: whole lines, each ended by NL; else a piece of one */
    uint8_t *clip;     /* g_clpb: clip_cap bytes, grown to CLIPSZ */
    uint32_t clip_cap;
} UiText;

/* An edit window: its view, the cursor, the selection, its place (TEDIT). The active window's
   is uimn.ed, the other's waits in uimn.win.win[w].ed (window.h swaps them). */
typedef struct {
    uint32_t line;   /* ed_line: the cursor's line in the view */
    uint32_t top;    /* ed_top: the first shown */
    uint16_t left;   /* ed_left: the first column shown */
    bool insert;     /* ed_ins: insert, else overtype */
    uint16_t indent; /* ed_ind: ed_enter's new line's indent */
    uint16_t nlen;   /* ed_nlen: a line kept while lines join or split */
    uint8_t prefix;  /* ed_pfx: Ctrl+Q waiting for its second key (11h), else 0 */
    bool sel_on;     /* a selection from (sel_line, sel_col) to the cursor */
    uint32_t sel_line;
    uint16_t sel_col;
    uint32_t sel_a, sel_b; /* edit_sel's: columns of the cursor's line, or lines */
    uint8_t view;          /* ed_vw: the view shown (view.h) */
    uint16_t row0;         /* ed_r0: the first text row */
    uint16_t rows;         /* ed_rows: text rows */
    uint8_t x;             /* ed_x: the frame's first column */
    uint16_t width;        /* ed_tw: the text's columns, the frame's less 2 */
} UiEdit;

/* A window: its rows, an edit window's state while not active (TWIN). */
typedef struct {
    uint8_t height; /* wh: its rows with its title, 0 hidden */
    uint16_t col;   /* wcol: an inactive edit window's cursor column (edline.col) */
    UiEdit ed;      /* wed */
} UiWin;

typedef struct {
    UiWin win[NWIN];
    uint8_t cols;     /* wcols: the edit windows' columns: 80, DP_COL with the pane */
    bool maxed;       /* wmax: Ctrl+F10's; saved the heights before */
    bool vert;        /* wvert: ED2 beside ED1, in ED1's rows */
    bool vert_saved;  /* wvsav: vert before Ctrl+F10 */
    uint8_t ed1_cols; /* ED1's columns beside ED2 (a drag's), 0 half */
    uint8_t saved[NWIN];
    uint8_t imm_saved; /* the immediate window's height the debug pane hid, 0 none */
} UiWins;

/* The immediate window (TIMM, im_h, im_lb). */
typedef struct {
    uint16_t n;             /* im_n: lines kept */
    uint16_t cur;           /* im_i: the one being edited */
    uint16_t top;           /* im_top: the first shown */
    uint16_t row0;          /* im_r0: the window's first text row */
    uint16_t rows;          /* im_rows */
    uint8_t hist[IMN][IMW]; /* im_h: each line's length, then its text */
} UiImm;

/* The mouse (TMOUSE); mouse.h. */
typedef struct {
    bool on;          /* ms_on: a driver answered */
    uint8_t button;   /* ms_b: the left button as last polled */
    uint8_t event;    /* ms_ev: MS_* (mouse.h) */
    uint8_t row, col; /* ms_pos: its cell at the event */
    bool hover;       /* ms_hov: a move with no button is an event (the key loop's wait) */
} UiMouse;

typedef struct {
    bool on;          /* lp.ideon: the editor runs (the core's ui_on) */
    uint8_t keyflags; /* lp.ui_kf: the shift flags at the last key (pkey_shift) */
    const char *help; /* lp.ui_help: the status bar's help line, nullptr the keys */
    uint8_t rows;     /* the screen's: 25, 43 or 50 (uimn_rows) */
    bool pane_on;     /* dp.dp_on: the debug pane shown (a stop) */
    uint8_t dp_col;   /* the debug pane's first column */
    bool tip_mouse;   /* dp.tt_ms: the tooltip by the mouse, else by the cursor */
    bool stopped;     /* ud.stopd: stopped, CONT goes on (not after END) */

    /* Before the big arrays: dialog.c's static tables point at these, and llrm-cc's
       object writer keeps only 16 bits of a data pointer's offset. */
    bool match_case;   /* fd.fd_case: Find's Match Upper/Lowercase */
    bool whole_word;   /* fd.fd_whole: Whole Word */
    LineBuf edline;    /* bf.ed_lb: the cursor's line, LNBUF */
    LineBuf immline;   /* bf.im_lb: the immediate line, DLBUF */
    LineBuf fname;     /* bf.dl_fn: a dialog's File Name field, DLBUF */
    LineBuf find_what; /* bf.fd_what: Find What */
    LineBuf change_to; /* bf.fd_to: Change To */

    struct {
        Widget screen, menubar, workspace, statusbar; /* workspace.focus: FOC_*, lp.ui_foc */
        Widget editors, imm, pane; /* the workspace's; editors.focus: the active edit window */
        Widget ed[2];              /* the editors': W_ED1, W_ED2 (wact) */
        Widget dp[4];              /* the pane's (dpan_build); pane.focus: dp_foc */
    } tree;

    UiText text;
    UiEdit ed; /* the active edit window's */
    UiWins win;
    UiImm imm;
    UiMouse mouse;

    uint16_t help_len; /* tx.hl_len: BASIC.HLP's bytes */
    char *help_text;   /* g_hlpb: BASIC.HLP, a line per menu item; HLPSZ at most */

    LineBuf line;     /* bf.uc_line: a line got from the text, a copy the lexer may change */
    LineBuf build;    /* bf.sy_buf: a line being built */
    char file[80];    /* bf.ui_fn: the file's name, "" none */
    char chosen[80];  /* bf.dl_name: the file a dialog chose */
    char errmsg[600]; /* bf.ui_ebuf: an error's message, '|' between lines */

    /* The dialog line buffers' texts (immline.text .. point here). */
    uint8_t immline_text[DLBUF];
    uint8_t fname_text[DLBUF];
    uint8_t find_what_text[DLBUF];
    uint8_t change_to_text[DLBUF];
} Uimn;
extern Uimn uimn;

/* The status bar's row; the windows' rows, the screen's less the menu and status bars. */
static inline uint8_t status_row(void) { return (uint8_t)(uimn.rows - 1); }
static inline uint8_t win_rows(void) { return (uint8_t)(uimn.rows - 2); }
static_assert(offsetof(Uimn, change_to) + sizeof(LineBuf) <= 0x10000);

/* c in capitals, a to z only (ui_upper, menu.asm's). */
#endif
