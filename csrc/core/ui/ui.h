/* The editor's shared constants, records and state (src/ui/ui.inc's g_ui), for every csrc/core/ui
   module. uimn (ui/uimn.c) defines uimn; a g_ui field only one module used is that module's
   static instead (csrc/portmap/ui.md says where each went).

   The screen: row 0 the menu bar, rows 1 to 23 the windows (wind.h), row 24 the status bar. A
   place on it is a row and a column, 0-based. Drawing names attribute slots (SL_*), which the
   theme (them.h) makes attributes.

   The editor runs under repl_main's setjmp (io/repl.h): uimn_start, uimn_ready and uimn_error
   end in the key loop, which never returns. A run or an error unwinds the C stack to repl.top,
   then comes back through uimn_ready or uimn_error: nothing the editor keeps lives on the stack. */
#ifndef UI_UI_H
#define UI_UI_H
#include "defs.h"
#include "plat/pscr.h"

enum {
    HLPSZ = 2048,     /* BASIC.HLP's bytes at most: the menus' help lines */
    DLBUF = 260,      /* a dialog's line buffer's text */
    FLDMAX = 80,      /* a dialog field's characters at most */
    TXMAX = 0xFF00,   /* the text's bytes at most */
    LNMAX = TXMAX - 0x100, /* an editor line's characters at most */
    LNBUF = LNMAX + 4,     /* an editor line buffer's text */
    ED_COLS = 78,     /* the immediate window's text columns */
    IMN = 10,         /* the immediate window's lines kept */
    IMW = 80,         /* each: its length, its text */
    CLIPSZ = 0x10000, /* the clipboard's bytes (the asm's g_clpb) */
};

/* Attribute slots (the asm's A_ED ..): each theme's attribute for it (them.h). */
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
    SCAN_F1 = 0x3B, SCAN_F10 = 0x44, SCAN_SHIFT_F1 = 0x54, SCAN_CTRL_F1 = 0x5E, SCAN_ALT_F1 = 0x68,
    SCAN_HOME = 0x47, SCAN_UP = 0x48, SCAN_PGUP = 0x49, SCAN_LEFT = 0x4B, SCAN_RIGHT = 0x4D,
    SCAN_END = 0x4F, SCAN_DOWN = 0x50, SCAN_PGDN = 0x51, SCAN_INS = 0x52, SCAN_DEL = 0x53,
    SCAN_CTRL_LEFT = 0x73, SCAN_CTRL_RIGHT = 0x74, SCAN_CTRL_END = 0x75, SCAN_CTRL_PGDN = 0x76,
    SCAN_CTRL_HOME = 0x77, SCAN_CTRL_PGUP = 0x84,
    SCAN_ESC = 0x01, SCAN_ENTER = 0x1C, SCAN_L = 0x26, SCAN_GREY_MINUS = 0x4A,
    SCAN_GREY_PLUS = 0x4E, SCAN_ALT_MINUS = 0x82, SCAN_ALT_EQUALS = 0x83, SCAN_CTRL_INS = 0x92,
    SCAN_ALT_UP = 0x98, SCAN_ALT_LEFT = 0x9B, SCAN_ALT_RIGHT = 0x9D, SCAN_ALT_DOWN = 0xA0,
    SCAN_ALT_PAD_ENTER = 0xA6,
    SCAN_ALT_FIRST = 0x10, SCAN_ALT_LAST = 0x32, /* Alt+letter's: Q .. M */
};
enum : uint16_t { KEY_SHIFT_TAB = 0x0F00 };
/* A key's character (its low byte). */
enum : uint8_t { CH_BACKSPACE = 0x08, CH_TAB = 0x09, CH_ENTER = 0x0D, CH_ESC = 0x1B, CH_E0 = 0xE0 };

enum : uint8_t {
    STATUS_ROW = PSCR_ROWS - 1, /* the status bar */
    WIN_ROWS = PSCR_ROWS - 2,   /* the windows': the screen's less the menu and status bars */
    TAB_STOP = 8,
};
/* The cursor's shapes (pscr_shape). */
enum : uint16_t { CURSOR_INSERT = 0x0607, CURSOR_OVERWRITE = 0x0407 };
/* Code page 437's glyphs the screens draw. */
enum : uint8_t {
    GLYPH_POINT_RIGHT = 0x10, GLYPH_POINT_LEFT = 0x11, GLYPH_UP = 0x18, GLYPH_DOWN = 0x19,
    GLYPH_RIGHT = 0x1A, GLYPH_LEFT = 0x1B, GLYPH_SHADE = 0xB0, GLYPH_V = 0xB3, GLYPH_V_LEFT = 0xB4,
    GLYPH_TOP_RIGHT = 0xBF, GLYPH_BOTTOM_LEFT = 0xC0, GLYPH_V_RIGHT = 0xC3, GLYPH_H = 0xC4,
    GLYPH_BOTTOM_RIGHT = 0xD9, GLYPH_TOP_LEFT = 0xDA,
};

/* uimn.focus: which window has the keys (ui_foc). */
enum : uint8_t { FOC_EDIT = 0, FOC_IMM = 1, FOC_PANE = 2 };

/* The windows, top down (wind.h); the debug pane is beside them, not in uimn.win.win[]. */
enum : uint8_t { W_HELP = 0, W_ED1 = 1, W_ED2 = 2, W_IMM = 3, NWIN = 4, W_DBG = NWIN };

/* The debug pane's columns (dpan.h): the edit windows have DP_COL while it is shown. */
enum { DP_COL = 50, DP_W = 30, DP_TW = DP_W - 2 };

/* A line being edited (TLBUF, TEDBUF): its text where text points, DLBUF bytes for a dialog
   field or the immediate line, LNBUF for the editor's line. The cursor may be past the text:
   typing there pads it with spaces. */
typedef struct {
    uint16_t col;  /* the cursor's column */
    uint16_t len;
    bool dirty;    /* changed since loaded (dty) */
    uint8_t *text;
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
    uint32_t n;      /* tx_n: the view's lines, from first */
    uint32_t first;  /* tx_v0: the view's first line in the text */
    uint32_t all;    /* tx_all: the text's lines */
    void (*hook)(uint32_t line, int32_t delta); /* tx_hook: text line added (1), removed (-1) */
    bool changed;    /* tx_mod: since the program was entered (a run enters it again) */
    bool unsaved;    /* tx_chg: since it was saved */
    bool check;      /* syn_on: Options, Syntax Checking */
    bool asis;       /* tx_asis: the next line stored as typed */
    uint16_t clip_len;  /* cb_len: the clipboard's bytes */
    bool clip_lines;    /* cb_ln: whole lines, each ended by NL; else a piece of one */
    uint8_t clip[CLIPSZ]; /* g_clpb */
} UiText;

/* An edit window: its view, the cursor, the selection, its place (TEDIT). The active window's
   is uimn.ed, the other's waits in uimn.win.win[w].ed (wind.h swaps them). */
typedef struct {
    uint32_t line;    /* ed_line: the cursor's line in the view */
    uint32_t top;     /* ed_top: the first shown */
    uint16_t left;    /* ed_left: the first column shown */
    bool insert;      /* ed_ins: insert, else overtype */
    uint16_t indent;  /* ed_ind: ed_enter's new line's indent */
    uint16_t nlen;    /* ed_nlen: a line kept while lines join or split */
    uint8_t prefix;   /* ed_pfx: Ctrl+Q waiting for its second key (11h), else 0 */
    bool sel_on;      /* a selection from (sel_line, sel_col) to the cursor */
    uint32_t sel_line;
    uint16_t sel_col;
    uint32_t sel_a, sel_b; /* edit_sel's: columns of the cursor's line, or lines */
    uint8_t view;     /* ed_vw: the view shown (view.h) */
    uint16_t row0;    /* ed_r0: the first text row */
    uint16_t rows;    /* ed_rows: text rows */
    uint8_t x;        /* ed_x: the frame's first column */
    uint16_t width;   /* ed_tw: the text's columns, the frame's less 2 */
} UiEdit;

/* A window: its rows, an edit window's state while not active (TWIN). */
typedef struct {
    uint8_t height;  /* wh: its rows with its title, 0 hidden */
    uint16_t col;    /* wcol: an inactive edit window's cursor column (edline.col) */
    UiEdit ed;       /* wed */
} UiWin;

typedef struct {
    UiWin win[NWIN];
    uint8_t cols;    /* wcols: the edit windows' columns: 80, DP_COL with the pane */
    uint8_t active;  /* wact: the active edit window, W_ED1 or W_ED2 */
    bool maxed;      /* wmax: Ctrl+F10's; saved the heights before */
    bool vert;       /* wvert: ED2 beside ED1, in ED1's rows */
    bool vert_saved; /* wvsav: vert before Ctrl+F10 */
    uint8_t saved[NWIN];
} UiWins;

/* The immediate window (TIMM, im_h, im_lb). */
typedef struct {
    uint16_t n;     /* im_n: lines kept */
    uint16_t cur;   /* im_i: the one being edited */
    uint16_t top;   /* im_top: the first shown */
    uint16_t row0;  /* im_r0: the window's first text row */
    uint16_t rows;  /* im_rows */
    uint8_t hist[IMN][IMW]; /* im_h: each line's length, then its text */
} UiImm;

/* The mouse (TMOUSE); mous.h. */
typedef struct {
    bool on;        /* ms_on: a driver answered */
    uint8_t button; /* ms_b: the left button as last polled */
    uint8_t event;  /* ms_ev: MS_* (mous.h) */
    uint8_t row, col; /* ms_pos: its cell at the event */
    bool hover;     /* ms_hov: a move with no button is an event (the key loop's wait) */
} UiMouse;

typedef struct {
    bool on;          /* lp.ideon: the editor runs (the core's ui_on) */
    uint8_t focus;    /* lp.ui_foc: FOC_* */
    uint8_t keyflags; /* lp.ui_kf: the shift flags at the last key (pkey_shift) */
    const char *help; /* lp.ui_help: the status bar's help line, nullptr the keys */
    bool pane_on;     /* dp.dp_on: the debug pane shown (a stop) */
    bool tip_mouse;   /* dp.tt_ms: the tooltip by the mouse, else by the cursor */
    bool stopped;     /* ud.stopd: stopped, CONT goes on (not after END) */

    /* Before the big arrays: dlog.c's static tables point at these, and llrm-cc's
       object writer keeps only 16 bits of a data pointer's offset. */
    bool match_case;  /* fd.fd_case: Find's Match Upper/Lowercase */
    bool whole_word;  /* fd.fd_whole: Whole Word */
    LineBuf edline;        /* bf.ed_lb: the cursor's line, LNBUF */
    LineBuf immline;       /* bf.im_lb: the immediate line, DLBUF */
    LineBuf fname;         /* bf.dl_fn: a dialog's File Name field, DLBUF */
    LineBuf find_what;     /* bf.fd_what: Find What */
    LineBuf change_to;     /* bf.fd_to: Change To */

    UiText text;
    UiEdit ed;        /* the active edit window's */
    UiWins win;
    UiImm imm;
    UiMouse mouse;

    uint16_t help_len;     /* tx.hl_len: BASIC.HLP's bytes */
    char help_text[HLPSZ]; /* g_hlpb: BASIC.HLP, a line per menu item */

    uint8_t line[LNBUF];   /* bf.uc_line: a line got from the text, a copy the lexer may change */
    uint8_t build[LNBUF];  /* bf.sy_buf: a line being built */
    char file[80];         /* bf.ui_fn: the file's name, "" none */
    char chosen[80];       /* bf.dl_name: the file a dialog chose */
    char errmsg[600];      /* bf.ui_ebuf: an error's message, '|' between lines */

    /* The line buffers' texts (edline.text .. point here). */
    uint8_t edline_text[LNBUF];
    uint8_t immline_text[DLBUF];
    uint8_t fname_text[DLBUF];
    uint8_t find_what_text[DLBUF];
    uint8_t change_to_text[DLBUF];
} Uimn;
extern Uimn uimn;
static_assert(offsetof(Uimn, change_to) + sizeof(LineBuf) <= 0x10000);

/* c in capitals, a to z only (ui_upper, menu.asm's). */
#endif
