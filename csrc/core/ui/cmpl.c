/* Tab completion (src/ui/compl.asm). An entry of the list: the length, the text and 0 (AC_LEN at
   most); the kind (AK_*); the rank, 0 first; a token or a line. */
#include "ui/cmpl.h"
#if AC
#include "comp/comp.h"
#include "comp/decl.h"
#include "comp/dfa.h"
#include "comp/lexr.h"
#include "io/gen.h"
#include "ui/draw.h"
#include "ui/edit.h"
#include "ui/mous.h"
#include "ui/scrn.h"
#include "ui/text.h"
#include "ui/uimn.h"

enum {
    AC_MAX = 48,  /* completions listed at most */
    AC_SZ = 18,   /* one: its length, its text */
    AC_EVN = 8,   /* a line's FORs and NEXTs */
    AC_ROWS = 8,  /* the list's rows */
    AC_LEN = 10,  /* a name's characters at most */
    AC_PREV = 40, /* a label's box: columns */
};
/* What fits (ac_m). */
enum : uint8_t { AM_ST = 1, AM_FN = 2, AM_OP = 4, AM_VR = 8, AM_LB = 0x10 };
/* What came last (ac_pv), when not a token. */
enum : uint8_t { AP_ST = 0, AP_ID = 1, AP_SEP = 2, AP_COM = 3 };
/* An entry's kind: a keyword, a function ('(' put after), a variable, a line label. */
enum : uint8_t { AK_KW = 0, AK_FN = 1, AK_VR = 2, AK_LB = 3 };
/* The type the expression wants (ac_ty). */
enum : uint8_t { AT_ANY = 0, AT_STR = 1, AT_NUM = 2 };

/* TACE */
typedef struct PACKED {
    uint8_t len;
    uint8_t text[AC_LEN + 1];
    uint8_t kind;
    uint8_t rank;
    uint32_t xt; /* its token or line */
} Entry;
static_assert(sizeof(Entry) == AC_SZ);

/* TCOMPL, ac_list, ac_ev. */
static struct {
    uint16_t n;      /* the list's names */
    uint16_t sel;    /* the one lit */
    uint16_t top;    /* the first shown */
    uint16_t pn;     /* the word's length */
    uint16_t ps;     /* its column */
    uint8_t at_row, at_col; /* the list's box */
    uint8_t m;       /* what fits (AM_*) */
    uint8_t st;      /* the statement's keyword */
    uint8_t pv;      /* what came last (AP_*, a token) */
    bool lc;         /* the word in small letters */
    uint8_t bw;      /* the box's columns */
    uint8_t bn;      /* its rows of names */
    uint8_t pfx[AC_SZ]; /* the word in capitals */
    uint8_t ty;      /* the type the expression wants (AT_*) */
    uint8_t lt;      /* the last operand's type */
    uint8_t fn;      /* the function whose ( is open, FFh an array */
    uint8_t flt;     /* its value's type */
    uint8_t plk;     /* the line before's last statement */
    bool cur;        /* ac_evs on the word's own line */
    uint16_t dp;     /* NEXTs not yet matched */
    uint8_t nm[AC_SZ]; /* a name: its length, it, 0 */
    uint8_t on[AC_SZ]; /* the open FOR's variable */
    char sig[32];    /* a function's form */
    uint8_t ev[AC_EVN + 1][AC_SZ]; /* a line's FORs (the name) and NEXTs (0, a count); the
                                      asm's past-the-end slot made one */
    Entry list[AC_MAX];
} ac;

static const uint8_t var_stmts[] = {TK_FOR, TK_NEXT, TK_DIM, TK_READ, TK_INPUT, TK_INPUTN, TK_GET, 0};
/* the words that go on after an operand, by statement */
static const uint8_t ops[][3] = {
    {TK_IF, TK_THEN, TK_GOTO}, {TK_FOR, TK_TO, TK_STEP}, {TK_ON, TK_GOTO, TK_GOSUB}};
/* the arguments of the functions not of one number */
static const struct {
    uint8_t tok;
    const char *args;
} func_args[] = {
    {TK_LEN - 1, "address"}, {TK_LEN, "s$"}, {TK_LEN + 2, "s$"}, {TK_LEN + 3, "s$"},
    {TK_LEN + 4, "code"}, {TK_LEN + 5, "s$, n"}, {TK_LEN + 6, "s$, n"},
    {TK_MID, "s$, start[, n]"}, {TK_TAB, "column"}, {TK_SPC, "n"},
};

static bool is_alpha(uint8_t c)
{
    c = uimn_upper(c);
    return c >= 'A' && c <= 'Z';
}
static bool is_alnum(uint8_t c) { return (c >= '0' && c <= '9') || is_alpha(c); }
static bool is_digit(uint8_t c) { return c == '.' || (c >= '0' && c <= '9'); }

/* ac_word: the word before the cursor; false: the cursor is inside a word or past the line. */
static bool word(void)
{
    const LineBuf *b = &uimn.edline;
    uint16_t col = b->col;
    if (col > b->len || (col < b->len && is_alnum(b->text[col])))
        return false;
    uint16_t s = col;
    while (s && is_alnum(b->text[s - 1]))
        s--;
    ac.ps = s;
    ac.pn = col - s;
    if (ac.pn >= AC_SZ - 2)
        return false;
    ac.lc = false;
    for (uint16_t i = 0; i < ac.pn; i++) {
        uint8_t c = b->text[s + i];
        ac.lc = c >= 'a';
        ac.pfx[i] = uimn_upper(c);
    }
    ac.pfx[ac.pn] = 0;
    return true;
}

/* ac_lex: the 0-ended uimn.build tokenized into comp_cbuf; the body. */
static uint8_t *lex(void)
{
    lexr_line(uimn.build, comp_cbuf);
    return comp_cbuf;
}

/* ac_line: text line line tokenized (the cursor's from the editor): its text in uimn.build, its
   body in comp_cbuf. */
static uint8_t *line_body(uint32_t line)
{
    uint16_t len;
    if (line == uimn.ed.line) {
        len = uimn.edline.len;
        memcpy(uimn.build, uimn.edline.text, len);
    } else
        len = (uint16_t)text_get(line, uimn.build);
    uimn.build[len] = 0;
    return lex();
}

/* ac_tkty: the types keyword tok gives: a function's value, what an operator wants. */
static uint8_t tok_type(uint8_t tok)
{
    uint8_t t = AT_NUM;
    if (tok == TK_FN || (tok >= TK_PLUS + 10 && tok <= TK_MID)) {
        if (tok != TK_FN && (tok == TK_LEN + 1 || tok >= TK_LEN + 4)) /* STR$, CHR$ .. MID$ */
            t = AT_STR;
        ac.fn = tok;
        ac.flt = t;
        ac.lt = t;
        return t;
    }
    t = ac.lt; /* + = < >: the other operand's type */
    if (tok == TK_PLUS || (tok >= TK_PLUS + 7 && tok <= TK_PLUS + 9)) {
        ac.ty = t;
        return t;
    }
    if (tok >= TK_PLUS + 7)
        return t;
    if (tok > TK_PLUS || tok == TK_NOT || tok == TK_TO || tok == TK_STEP || tok == TK_FOR ||
        tok == TK_NEXT)
        t = AT_NUM; /* - * / ^ AND OR NOT TO STEP FOR NEXT */
    else
        t = AT_ANY;
    ac.ty = t;
    return t;
}

/* ac_vnm: the name at p's M_VAR to ac.nm: its length, then it in capitals, AC_LEN at most; a
   '(' after it added. Past it, at the '('. */
static const uint8_t *var_name(const uint8_t *p)
{
    uint32_t n = p[1];
    const uint8_t *next = p + n + 4;
    if (n > AC_LEN)
        n = AC_LEN;
    for (uint32_t i = 0; i < n; i++)
        ac.nm[1 + i] = uimn_upper(p[2 + i]);
    if (*next == '(' && n < AC_LEN)
        ac.nm[1 + n++] = '(';
    ac.nm[0] = (uint8_t)n;
    ac.nm[1 + n] = 0;
    return next;
}

/* ac_nty: the type of the name in ac.nm: a string when it ends in $. */
static uint8_t name_type(void)
{
    uint32_t b = ac.nm[0];
    if (ac.nm[b] == '(')
        b--;
    return ac.nm[b] == '$' ? AT_STR : AT_NUM;
}

/* ac_mask: ac.m from the statement (ac.st) and what came last (ac.pv). */
static void mask(void)
{
    uint8_t pv = ac.pv, st = ac.st;
    if (pv == AP_ST)
        ac.m = AM_ST | AM_VR;
    else if (pv == TK_THEN)
        ac.m = AM_ST | AM_VR | AM_LB;
    else if (pv == TK_GOTO || pv == TK_GOSUB || pv == TK_RUN || pv == TK_LIST ||
             (st == TK_ON && pv == AP_COM))
        ac.m = AM_LB;
    else {
        for (const uint8_t *s = var_stmts; *s; s++) /* the statements that name variables */
            if (st == *s && (pv == *s || pv == AP_COM)) {
                ac.m = AM_VR;
                return;
            }
        ac.m = pv == AP_ID ? AM_OP : AM_FN | AM_VR;
    }
}

/* ac_ctx: what fits at the word: the line before it tokenized, its last statement read item by
   item; the type each operand gives (ac.lt) and the one the expression wants (ac.ty). false:
   nothing (a string, REM, DATA). */
static bool context(void)
{
    memcpy(uimn.build, uimn.edline.text, ac.ps);
    uimn.build[ac.ps] = 0;
    const uint8_t *p = lex();
    if (decl_tail(&p))
        return false;
    ac.st = 0;
    ac.pv = AP_ST;
    ac.ty = AT_ANY;
    ac.fn = 0;
    for (uint8_t c; (c = *p);) {
        if (c & 0x80) { /* a keyword */
            p = decl_item(p);
            if (ac.pv == TK_THEN || ac.pv == AP_ST) /* a statement's start: its keyword */
                ac.st = c;
            ac.pv = c;
            tok_type(c);
            if (c >= TK_PLUS && c <= TK_PLUS + 9) /* + - * / ^ AND OR > = < */
                ac.pv = AP_SEP;
        } else if (c == M_VAR) {
            p = var_name(p);
            uint8_t t = name_type();
            ac.lt = t;
            if (ac.pv == AP_ST)
                ac.st = AP_ID; /* an assignment */
            if (*p == '(') { /* an array's subscripts: numbers */
                p++;
                ac.flt = t;
                ac.fn = 0xFF;
                ac.ty = AT_NUM;
                ac.pv = AP_SEP;
            } else
                ac.pv = AP_ID;
        } else if (c == '"') {
            p = decl_item(p);
            ac.lt = AT_STR;
            ac.pv = AP_ID;
        } else if (c == M_INT || is_digit(c)) {
            p = decl_step(p);
            ac.lt = AT_NUM;
            ac.pv = AP_ID;
        } else {
            p++;
            uint8_t pv = AP_SEP, fn = ac.fn;
            if (c == '(') { /* a function's argument: its type */
                if (fn) {
                    ac.ty = AT_STR; /* LEN, VAL, ASC, LEFT$, RIGHT$, MID$ */
                    if (fn != TK_LEN && fn != TK_LEN + 2 && fn != TK_LEN + 3 && fn < TK_LEN + 5)
                        ac.ty = AT_NUM;
                }
            } else if (c == ')') {
                pv = AP_ID;
                if (fn) { /* the call's value */
                    ac.lt = ac.flt;
                    ac.fn = 0;
                }
            } else if (c == ',' || c == ';') {
                pv = AP_COM;
                ac.ty = fn >= TK_LEN + 5 ? AT_NUM : AT_ANY; /* LEFT$( s$, n */
            }
            ac.pv = pv;
        }
    }
    mask();
    if (is_digit(ac.pfx[0])) /* a number is a label */
        ac.m &= AM_LB;
    return ac.m != 0;
}

/* ac_want: token tok is wanted by ac.m in statement ac.st. */
static bool wanted(uint8_t tok)
{
    if (ac.m & AM_ST && tok <= TK_NEW)
        return true;
    if (ac.m & AM_FN && (tok == TK_TAB || tok == TK_FN || tok == TK_SPC || tok == TK_NOT ||
                         (tok >= TK_PLUS + 10 && tok <= TK_MID)))
        return true;
    if (!(ac.m & AM_OP))
        return false;
    if (tok == TK_PLUS + 5 || tok == TK_PLUS + 6) /* AND, OR */
        return true;
    for (uint32_t i = 0; i < countof(ops); i++)
        if (ops[i][0] == ac.st && (tok == ops[i][1] || tok == ops[i][2]))
            return true;
    return false;
}

/* ac_add: n characters at s to the list, in capitals, once, in order of rank then text; when
   they start with the word and are longer. A variable ranks first when it is the open FOR's. */
static void add(const uint8_t *s, uint32_t n, uint8_t kind, uint8_t rank, uint32_t xt)
{
    if (n <= ac.pn || n > AC_LEN)
        return;
    uint8_t up[AC_LEN + 1];
    for (uint32_t i = 0; i < n; i++)
        up[i] = uimn_upper(s[i]);
    up[n] = 0;
    if (memcmp(up, ac.pfx, ac.pn))
        return;
    if (kind == AK_VR && ac.on[0]) /* the open FOR's variable first */
        rank = memcmp(up, ac.on + 1, n + 1) ? 1 : 0;
    uint16_t i = 0; /* the place: before the first above it */
    for (; i < ac.n; i++) {
        const Entry *e = &ac.list[i];
        if (rank < e->rank)
            break;
        if (rank > e->rank)
            continue;
        int d = strcmp((const char *)up, (const char *)e->text);
        if (!d) /* there already */
            return;
        if (d < 0)
            break;
    }
    if (ac.n >= AC_MAX)
        return;
    memmove(&ac.list[i + 1], &ac.list[i], (ac.n - i) * sizeof(Entry));
    Entry *e = &ac.list[i];
    e->len = (uint8_t)n;
    memcpy(e->text, up, n + 1);
    e->kind = kind;
    e->rank = rank;
    e->xt = xt;
    ac.n++;
}

/* ac_keys: the wanted keywords that start with the word, to the list: a function only of the
   type wanted. */
static void keys(void)
{
    const uint8_t *p = cons_kwlist;
    uint8_t w[64];
    for (uint8_t tok = 0x80; *p; tok++) {
        uint32_t n = 0;
        uint8_t c;
        do {
            c = *p++;
            w[n++] = c & 0x7F;
        } while (!(c & 0x80));
        w[n] = 0;
        if (!wanted(tok) || !is_alpha(w[0]))
            continue;
        if (ac.ty == AT_STR && (tok == TK_FN || tok == TK_NOT)) /* FN and NOT give numbers */
            continue;
        bool tabspc = tok == TK_TAB || tok == TK_SPC;
        if (tabspc && ac.st != TK_PRINT && ac.st != TK_PRINTN) /* TAB( SPC( in PRINT only */
            continue;
        uint8_t kind = AK_KW;
        if (tabspc)
            kind = AK_FN;
        else if (tok >= TK_PLUS + 10) { /* a function: its value's type */
            uint8_t t = tok_type(tok);
            if (ac.ty && ac.ty != t)
                continue;
            kind = AK_FN;
        }
        add(w, n, kind, tok <= TK_NEW ? 0 : 1, tok); /* a function after the variables */
    }
}

/* ac_label: the label that starts the body p of line line, to the list. */
static void label(const uint8_t *p, uint32_t line)
{
    uint32_t num;
    memcpy(&num, p + 1, 4);
    if (p[0] != M_INT || num > 0xFFFF)
        return;
    char *e = uimn_dec((char *)ac.nm, (uint16_t)num);
    uint8_t rank = 1; /* GOSUB: a start first */
    if (ac.pv == TK_GOSUB &&
        (ac.plk == TK_RET || ac.plk == TK_END || ac.plk == TK_STOP || ac.plk == TK_GOTO))
        rank = 0;
    add(ac.nm, (uint32_t)(e - (char *)ac.nm), AK_LB, rank, line);
}

/* ac_ids: the names in the body p, to the list: not after REM or DATA; of the type wanted; no
   array after FOR or NEXT. */
static void ids(const uint8_t *p, uint32_t line)
{
    for (uint8_t c; (c = *p) && c != TK_REM && c != TK_DATA;) {
        if (c != M_VAR) {
            p = decl_step(p);
            continue;
        }
        p = var_name(p);
        if (ac.ty && name_type() != ac.ty)
            continue;
        if (ac.nm[ac.nm[0]] == '(' && ac.m == AM_VR && (ac.st == TK_FOR || ac.st == TK_NEXT))
            continue;
        add(ac.nm + 1, ac.nm[0], AK_VR, 0, line);
    }
}

/* ac_vars: the program's variables or labels that start with the word. GOSUB ranks first a
   label after a RETURN, END, STOP or GOTO: a start. */
static void vars(void)
{
    if (!(ac.m & (AM_VR | AM_LB)))
        return;
    ac.plk = 0;
    for (uint32_t line = 0; line < uimn.text.n; line++) {
        const uint8_t *p = line_body(line);
        if (ac.m & AM_LB && line != uimn.ed.line) /* not the word's own line */
            label(p, line);
        if (ac.m & AM_VR)
            ids(p, line);
        p = comp_cbuf;
        decl_tail(&p); /* its last statement's keyword */
        ac.plk = *p & 0x80 ? *p : 0;
    }
}

/* ac_evs: the body p's FORs and NEXTs, taken last first against the NEXTs after them (ac.dp);
   true: the open FOR found, in ac.on. */
static bool events(const uint8_t *p)
{
    uint32_t k = 0;
    for (uint8_t c; (c = *p) && c != TK_REM && c != TK_DATA;) {
        p = decl_step(p);
        if (c == TK_FOR) {
            if (*p != M_VAR)
                continue;
            p = var_name(p);
            memcpy(ac.ev[k], ac.nm, AC_SZ);
        } else if (c == TK_NEXT) {
            uint8_t n = 0; /* its names */
            for (;;) {
                if (*p == ',')
                    p++;
                else if (*p == M_VAR) {
                    p = decl_item(p);
                    n++;
                } else
                    break;
            }
            ac.ev[k][0] = 0;
            ac.ev[k][1] = n;
        } else
            continue;
        if (k < AC_EVN)
            k++;
    }
    while (k) { /* last first */
        const uint8_t *e = ac.ev[--k];
        if (!e[0]) { /* NEXT: a bare one closes one, but not the one being written */
            uint16_t n = e[1];
            if (!n && !ac.cur)
                n = 1;
            ac.dp += n;
            ac.cur = false;
        } else if (ac.dp)
            ac.dp--;
        else {
            memcpy(ac.on, e, AC_SZ);
            return true;
        }
    }
    return false;
}

/* ac_open: the variable of the innermost FOR that no NEXT closes before the word, to ac.on. */
static void open_for(void)
{
    ac.dp = 0;
    ac.cur = true;
    const uint8_t *p = comp_cbuf; /* the line to the word, as context left it */
    for (int32_t line = (int32_t)uimn.ed.line; !events(p);) {
        ac.cur = false;
        if (--line < 0)
            return;
        p = line_body((uint32_t)line);
    }
}

/* ac_put: characters ac.pn to upto of the name s typed after the word, in small letters when the
   word is; the word that long. */
static void put(const uint8_t *s, uint16_t upto)
{
    if (upto <= ac.pn)
        return;
    uint8_t t[AC_SZ];
    uint16_t n = upto - ac.pn;
    for (uint16_t i = 0; i < n; i++) {
        uint8_t c = s[ac.pn + i];
        t[i] = ac.lc && is_alpha(c) ? c | 0x20 : c;
    }
    ac.pn = upto;
    edit_lb_insert(&uimn.edline, t, n);
    uimn.text.changed = true;
}

/* ac_common: the word as long as all of the list agree: those letters put in. */
static void common(void)
{
    uint16_t b = ac.pn;
    for (;; b++) {
        const Entry *e = &ac.list[0];
        if ((uint8_t)b >= e->len)
            break;
        uint8_t c = e->text[b];
        uint16_t i = 0;
        for (; i < ac.n; i++, e++)
            if ((uint8_t)b >= e->len || e->text[b] != c)
                break;
        if (i < ac.n)
            break;
    }
    if (b != ac.pn)
        put(ac.list[0].text, b);
}

/* ac_sig: the form of function entry e in ac.sig: LEFT$(s$, n). */
static void signature(const Entry *e)
{
    char *d = ac.sig;
    memcpy(d, e->text, e->len);
    d += e->len;
    if (d[-1] != '(')
        *d++ = '(';
    const char *a = "x"; /* its arguments, else x */
    for (uint32_t i = 0; i < countof(func_args); i++)
        if (func_args[i].tok == (uint8_t)e->xt) {
            a = func_args[i].args;
            break;
        }
    while (*a)
        *d++ = *a++;
    *d++ = ')';
    *d = 0;
}

/* ac_ins: entry i completes the word; a function gets its '(' and its form on the status bar. */
static void insert(uint16_t i)
{
    const Entry *e = &ac.list[i];
    put(e->text, e->len);
    if (e->kind != AK_FN)
        return;
    if (e->text[e->len - 1] != '(')
        edit_lb_insert(&uimn.edline, (const uint8_t *)"(", 1);
    signature(e);
    uimn.help = ac.sig;
}

/* ac_prev: label entry e's line after the label, at row, col + 7 in the box. */
static void preview(const Entry *e, uint8_t row, uint8_t col)
{
    const uint8_t *p = line_body(e->xt);
    uint32_t off = 0; /* its text from the item after the label */
    if (p[0] == M_INT) {
        if (!p[5])
            return;
        off = lexr.col[5];
    }
    const uint8_t *s = uimn.build + off;
    uint16_t n = (uint16_t)strlen((const char *)s); /* its length, to the box's edge */
    if (n > AC_PREV - 11)
        n = AC_PREV - 11;
    scrn_strn(row, (uint8_t)(col + 7), s, n, SL_BAR);
}

/* ac_draw: the list's box, ac.sel lit. */
static void draw(void)
{
    scrn_box(ac.at_row, ac.at_col, ac.bn + 2, ac.bw, SL_BAR, nullptr);
    scrn_shadow(ac.at_row, ac.at_col, ac.bn + 2, ac.bw);
    for (uint8_t b = 0; b < ac.bn; b++) {
        uint8_t row = ac.at_row + b + 1, col = ac.at_col + 2;
        const Entry *e = &ac.list[ac.top + b];
        scrn_strn(row, col, e->text, e->len, SL_BAR);
        if (e->kind == AK_LB)
            preview(e, row, col);
        if (ac.top + b == ac.sel)
            scrn_attr(row, col - 1, ac.bw - 2, SL_DSEL);
    }
}

/* ac_pop: the list under the word: arrows choose, Enter or Tab takes, Esc or another key leaves
   it, the key then typed. */
static void popup(void)
{
    ac.sel = 0;
    ac.top = 0;
    uint8_t bw = 4; /* the box: the longest name and 4 */
    for (uint16_t i = 0; i < ac.n; i++) {
        uint8_t w = ac.list[i].kind == AK_LB ? AC_PREV : ac.list[i].len + 4; /* a label: its line */
        if (w > bw)
            bw = w;
    }
    ac.bw = bw;
    ac.bn = (uint8_t)(ac.n > AC_ROWS ? AC_ROWS : ac.n);
    uint8_t h = ac.bn + 2;
    uint8_t row = (uint8_t)(uimn.ed.line - uimn.ed.top + uimn.ed.row0 + 1); /* under, else over */
    if ((uint8_t)(row + h) > 24)
        row = row - h - 1;
    uint16_t c = ac.ps > uimn.ed.left ? ac.ps - uimn.ed.left : 0;
    uint8_t col = (uint8_t)c;
    if ((uint8_t)(col + bw) > 80)
        col = 80 - bw;
    ac.at_row = row;
    ac.at_col = col;
    for (;;) {
        uimn.help = nullptr; /* a function's form on the status bar */
        if (ac.list[ac.sel].kind == AK_FN) {
            signature(&ac.list[ac.sel]);
            uimn.help = ac.sig;
        }
        draw_all();
        draw();
        uint16_t key = uimn_getkey();
        int32_t s = ac.sel;
        if (key == KEY_MOUSE) {
            if (uimn.mouse.event != MS_PRESS)
                continue;
            uint8_t r = uimn.mouse.row - ac.at_row; /* a click on a name takes it */
            uint8_t x = uimn.mouse.col - ac.at_col;
            if (uimn.mouse.row <= ac.at_row || r > ac.bn || uimn.mouse.col <= ac.at_col ||
                x >= ac.bw) {
                uimn.help = nullptr;
                return;
            }
            ac.sel = (uint16_t)(r - 1 + ac.top);
            break;
        }
        uint8_t ch = (uint8_t)key, scan = (uint8_t)(key >> 8);
        if (ch == 0x0D || ch == 0x09)
            break;
        if (ch == 0x1B) {
            uimn.help = nullptr;
            return;
        }
        if (ch || (scan != 0x48 && scan != 0x50 && scan != 0x49 && scan != 0x51)) {
            uimn_key(key); /* another key: typed as ever */
            uimn.help = nullptr;
            return;
        }
        if (scan == 0x48)
            s = s ? s - 1 : 0;
        else if (scan == 0x49)
            s = s >= AC_ROWS ? s - AC_ROWS : 0;
        else {
            s += scan == 0x51 ? AC_ROWS : 1;
            if (s >= ac.n)
                s = ac.n - 1;
        }
        ac.sel = (uint16_t)s;
        if (s < ac.top)
            ac.top = (uint16_t)s;
        if (s >= ac.bn + ac.top)
            ac.top = (uint16_t)(s - (AC_ROWS - 1));
    }
    uimn.help = nullptr;
    insert(ac.sel);
}

bool cmpl_tab(void)
{
    if (!word() || !context())
        return false;
    ac.n = 0;
    ac.on[0] = 0;
    if (ac.m == AM_VR && ac.st == TK_NEXT) { /* NEXT: the open FOR's variable */
        open_for();
        if (!ac.pn) /* no word: that alone */
            add(ac.on + 1, ac.on[0], AK_VR, 0, 0);
        else {
            keys();
            vars();
        }
    } else {
        if (!ac.pn && !(ac.m & AM_LB) && /* no word: labels, or what has the type wanted */
            (ac.ty == AT_ANY || !(ac.m & (AM_FN | AM_VR))))
            return false;
        keys();
        vars();
    }
    if (!ac.n)
        return false;
    common();
    if (ac.n == 1)
        insert(0);
    else
        popup();
    return true;
}
#endif
