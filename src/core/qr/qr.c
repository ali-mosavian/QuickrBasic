/* Quickr, the modern dialect: a Quickr file (its first line qr_dialect) and the modules it imports loaded, checked, linked and
 * run.
 *
 * # How it works
 * Each source line becomes a line of the program store (LF_QR), its body the line's text, so the
 * editor, an error's place and the debugger work line by line as for QBasic. The front end then
 * runs in stages, each over the whole file: lex (qlex), parse (qpar), check (qsem). Every error
 * any stage finds is reported, in the source's order, before anything runs:
 *
 *   prog.bas:3:7: unknown name y
 *     print y
 *           ^
 *
 * Each import loads its module's file once (found by io/locate.c, a .bas whose first line is qr_dialect): its lines, its own
 * tokens and its statements, after the main file's. The store holds a module's lines before those
 * of the modules importing it, the main's last, so the modules' vars start in that order.
 *
 * A clean program is linked (qlink) and run by the VM as a QBasic one is. A runtime error stops
 * it at its line: prog.bas:5: Division by zero, then the line.
 *
 * The editor keeps a program's lines in the store as it reads them, each file's number its
 * module's, the main's first (ui/text.h); a module is read in after the rest. A run's lines, a
 * generic's instances and the end of the run past the main's last line, go after them (LF_INC)
 * and out again before the next build. The run goes through the modules in the order above, each
 * module's last line on to the next's first (qlink).
 */
#include "qr/qr.h"
#include "session.h"
#include "io/errmsg.h"
#include "io/locate.h"
#include "plat/pfil.h"
#include "qbrt/qbrt.h"
#include "qr/qast.h"
#include "qr/qlex.h"
#include "qr/qlink.h"
#include "qr/qpar.h"
#include "qr/qrt.h"
#include "qr/qsem.h"

enum { QR_CHUNK = 0x10000 }; /* the front end's arena's blocks at least */
enum { QR_LINE_CHUNK = 0x1000 }; /* a line lexed alone's */

const char qr_dialect[] = "' OPTION DIALECT QUICKR";

bool qr_marker(const char *const text, const uint32_t len)
{
    const uint32_t n = sizeof qr_dialect - 1;

    if (len < n)
        return false;

    for (uint32_t i = 0; i < n; i++)
        if (upcase((uint8_t)text[i]) != (uint8_t)qr_dialect[i])
            return false;

    for (uint32_t i = n; i < len; i++)
        if (text[i] != ' ' && text[i] != '\r' && text[i] != '\n')
            return false;

    return true;
}

bool qr_is_file(const char *const path)
{
    PfilHandle h;

    if (pfil_open(path, PFIL_READ, &h))
        return false;

    char head[sizeof qr_dialect + 2];
    uint32_t got = 0;
    pfil_read(h, head, sizeof head, &got);
    pfil_close(h);
    uint32_t n = 0;

    while (n < got && head[n] != '\n')
        n++;

    return qr_marker(head, n);
}

/* A platform error dos, if any, raised as QBasic's. */
static void qr_check(Session *const S, const uint16_t dos)
{
    if (dos)
        fault_check(&S->fault, chnl_doserr(dos));
}

/* The bytes of the file at path in the arena, a 0 after them; *len how many. PFIL_OK, else the DOS
   error. */
static uint16_t qr_bytes(QMem *const m, const char *const path, char **const text, uint32_t *len)
{
    PfilHandle h;
    const uint16_t e = pfil_open(path, PFIL_READ, &h);

    if (e)
        return e;

    uint32_t size = 0, got = 0;
    pfil_seek(h, 0, PFIL_END, &size);
    pfil_seek(h, 0, PFIL_SET, nullptr);
    *text = qast_alloc(m, size + 1);
    const uint16_t st = pfil_read(h, *text, size, &got);
    pfil_close(h);
    *len = got;
    return st;
}

/* The line of text at *at, len bytes in all, its end (LF, CR LF) a 0 in place; *at past it. Its
   length. */
static uint32_t qr_line(char *const text, const uint32_t len, uint32_t *const at)
{
    uint32_t end = *at;

    while (end < len && text[end] != '\n')
        end++;

    const uint32_t w = end - *at - (end > *at && text[end - 1] == '\r');
    text[*at + w] = 0;
    *at = end + 1;
    return w;
}

/* A new module of p at path, its lines next: its number. */
static uint32_t qr_newmod(QProg *const p, QMem *const m, const char *const path)
{
    const uint32_t k = p->nmods;
    const uint32_t n = (uint32_t)strlen(path);
    char *const name = qast_alloc(m, n + 1);
    memcpy(name, path, n);
    p->mods = qast_room(m, p->mods, p->nmods, &p->capmods, sizeof *p->mods);
    p->nmods++;
    p->mods[k] = (QMod){.path = name, .first = p->src.nlines};

    if (k)
        p->mods[k].base = p->mods[k - 1].base + p->mods[k - 1].ntoks;

    return k;
}

/* ln a line of p's, the last; *cap the lines' room. */
static void qr_addline(QProg *const p, QMem *const m, uint32_t *const cap, const QLine ln)
{
    p->src.lines = qast_room(m, p->src.lines, p->src.nlines, cap, sizeof *p->src.lines);
    p->src.lines[p->src.nlines++] = ln;
}

/* Module k, its lines p's last: lexed and parsed. */
static void qr_parse(QProg *const p, QMem *const m, const uint32_t k)
{
    QMod *const mod = &p->mods[k];
    mod->nlines = p->src.nlines - mod->first;
    qlex_source(&p->src, mod->first, m, &p->errs);
    mod->toks = p->src.toks;
    mod->ntoks = p->src.ntoks;
    qpar_program(p, m, mod);
}

/* The file at path a new module of p: its lines, lexed and parsed. Its number. */
static uint32_t qr_module(Session *const S, QProg *const p, QMem *const m, const char *const path)
{
    uint32_t len, cap = p->src.nlines;
    char *text;
    qr_check(S, qr_bytes(m, path, &text, &len));
    const uint32_t k = qr_newmod(p, m, path);

    for (uint32_t at = 0, src = 1; at < len; src++) {
        const uint32_t start = at;
        const uint32_t w = qr_line(text, len, &at);
        qr_addline(
            p,
            m,
            &cap,
            (QLine){
                .text = text + start,
                .len = w,
                .src = (uint16_t)src,
                .mod = (uint16_t)k,
            }

        );
    }

    qr_parse(p, m, k);
    return k;
}

/* Is record l a text line of file file's: Quickr's, not a run's own? */
static bool qr_text(const Line *const l, const uint32_t file)
{
    return (l->flags & (LF_QR | LF_INC)) == LF_QR && l->file == file;
}

/* The store's lines of file file, a new module of p at path: each's text copied, its src its
   number in the file; lexed and parsed. */
static void qr_gather(
    Session *const S,
    QProg *const p,
    QMem *const m,
    const uint32_t file,
    const char *const path
)
{
    Prog *const pg = &S->prog;
    const uint32_t k = qr_newmod(p, m, path);
    uint32_t cap = p->src.nlines, src = 1;

    for (uint32_t rec = PROGBASE; prog_line(pg, rec)->len; rec += prog_line(pg, rec)->len) {
        Line *const l = prog_line(pg, rec);

        if (!qr_text(l, file))
            continue;

        const char *const body = (const char *)at(prog_addr(pg, rec) + HDR);
        const uint32_t n = (uint32_t)strlen(body);
        char *const text = qast_alloc(m, n + 1);
        memcpy(text, body, n);
        l->src = (uint16_t)src;
        qr_addline(
            p,
            m,
            &cap,
            (QLine){
                .at = rec,
                .text = text,
                .len = n,
                .src = (uint16_t)src,
                .mod = (uint16_t)k,
            }

        );
        src++;
    }

    qr_parse(p, m, k);
}

/* The line at text, n bytes, its tabs spaces to the next column of 8: in the arena, 0-ended;
   *w its length. */
static const char *qr_untab(QMem *const m, const char *const text, const uint32_t n, uint32_t *w)
{
    uint32_t tabs = 0;

    for (uint32_t i = 0; i < n; i++)
        tabs += text[i] == '\t';

    char *const out = qast_alloc(m, n + tabs * 7 + 1);
    uint32_t o = 0;

    for (uint32_t i = 0; i < n; i++) {
        if (text[i] != '\t') {
            out[o++] = text[i];
            continue;
        }

        do
            out[o++] = ' ';
        while (o % 8);
    }

    out[o] = 0;
    *w = o;
    return out;
}

uint16_t qr_read(Session *const S, const char *const path, const uint16_t file)
{
    QMem m = {.fault = &S->fault};

    arena_init(&m.arena, QR_CHUNK);
    char *text;
    uint32_t len;
    const uint16_t e = qr_bytes(&m, path, &text, &len);
    const char *const eof = e ? nullptr : memchr(text, 0x1A, len);

    if (eof)
        len = (uint32_t)(eof - text);

    S->prog.add_flags = LF_QR;
    S->prog.add_file = file;
    S->prog.add_at = 0;

    for (uint32_t at = 0, src = 1; !e && (at < len || src == 1); src++) {
        const uint32_t start = at;
        uint32_t w;
        const char *const line = qr_untab(&m, text + start, qr_line(text, len, &at), &w);
        S->prog.add_src = (uint16_t)src;
        prog_add(&S->prog, NONUM, (const uint8_t *)line, w + 1);
    }

    S->prog.add_flags = 0;
    S->prog.add_src = 0;
    S->prog.add_file = 0;
    arena_reset(&m.arena);
    return e;
}

/* Are paths a and b the same, case aside? */
static bool qr_same(const char *a, const char *b)
{
    while (*a && downcase((uint8_t)*a) == downcase((uint8_t)*b))
        a++, b++;

    return !*a && !*b;
}

/* A program being loaded: its tree's, and whether its lines are the store's (the editor's), a
   module read in after them; else each file's text is read into the arena. */
typedef struct {
    Session *S;
    QProg *p;
    QMem *m;
    bool stored;
} QrLoad;

/* The module at path: p's of that path, else read in, a new module. Its number; QMOD_QBASIC when
   its file is not Quickr's, QMOD_NONE when the editor cannot read it. */
static uint32_t qr_load(const QrLoad *const L, const char *const path)
{
    QProg *const p = L->p;

    for (uint32_t i = 0; i < p->nmods; i++)
        if (qr_same(p->mods[i].path, path))
            return i;

    if (!qr_is_file(path))
        return QMOD_QBASIC;
    if (!L->stored)
        return qr_module(L->S, p, L->m, path);

    Modu *const mu = &L->S->modu;
    const uint32_t k = modu_module(mu, path);

    if (k < p->nmods) /* another spelling of its path */
        return k;
    if (qr_read(L->S, path, (uint16_t)k)) {
        modu_drop(mu, k);
        return QMOD_NONE;
    }

    modu_file(mu, k, path, k);
    qr_gather(L->S, p, L->m, k, path);
    return k;
}

/* Module im, named name (n bytes), not loaded: an error at it, QMOD_NONE its number. */
static void qr_unloaded(QProg *const p, QImport *const im, const char *const name, uint32_t n)
{
    if (im->mod == QMOD_QBASIC)
        qast_error(
            &p->errs,
            im->tok,
            "{} is a QBasic module: its first line is not ' OPTION DIALECT QUICKR",
            name,
            n
        );
    else if (im->mod == QMOD_NONE)
        qast_error(&p->errs, im->tok, "no module {}", name, n);
    else
        return;

    im->mod = QMOD_NONE;
}

/* Module im of p, found from module k's file: loaded once, its number in im->mod; a package's
   folder (folder not null) to folder. Not found: an error at its name. */
static void qr_find(const QrLoad *const L, const uint32_t k, QImport *const im, char *const folder)
{
    Session *const S = L->S;
    QProg *const p = L->p;
    char dotted[PFIL_PATH], path[PFIL_PATH];
    uint32_t n = 0;

    for (uint32_t i = 0; i < im->npath; i++) {
        const QTok *const t = im->path[i];

        if (n + t->len + 1 >= sizeof dotted)
            break;
        if (i)
            dotted[n++] = '.';

        memcpy(dotted + n, t->text, t->len);
        n += t->len;
    }

    const ImptName name =
        {.text = (const uint8_t *)dotted, .len = (uint8_t)n, .up = (uint8_t)im->up};

    const Locate where = {
        .main = S->repl.file,
        .lib = syst_env(&S->syst, "BASICLIB"),
        .from = p->mods[k].path,
        .ext = ".bas",
    };

    bool dir = false;

    if (!locate(&where, &name, folder ? folder : path, folder ? &dir : nullptr)) {
        qast_error(&p->errs, im->tok, "no module {}", n ? dotted : ".", n ? n : 1);
        return;
    }

    if (dir) {
        im->folder = true;
        return;
    }

    im->mod = qr_load(L, folder ? folder : path);
    qr_unloaded(p, im, dotted, n);
}

/* A name from gives, of the package in folder: its module, loaded once. */
static void qr_part(const QrLoad *const L, const char *const dir, QImport *const im)
{
    QProg *const p = L->p;
    char path[PFIL_PATH];
    const ImptName name = {.text = (const uint8_t *)im->tok->text, .len = (uint8_t)im->tok->len};

    if (!locate_in(dir, &name, ".bas", path)) {
        qast_error(&p->errs, im->tok, "no module {} in the package", im->tok->text, im->tok->len);
        return;
    }

    im->mod = qr_load(L, path);
    qr_unloaded(p, im, im->tok->text, im->tok->len);
}

/* Each module module k's imports and froms name, loaded once. */
static void qr_imports(const QrLoad *const L, const uint32_t k)
{
    for (QStmt *s = L->p->mods[k].head; s; s = s->next) {
        if (s->kind == QS_IMPORT)
            for (QImport *im = s->imps; im; im = im->next)
                qr_find(L, k, im, nullptr);

        if (s->kind == QS_FROM) {
            char dir[PFIL_PATH];
            qr_find(L, k, s->imps, dir);

            for (QImport *im = s->names; s->imps->folder && im; im = im->next)
                qr_part(L, dir, im);
        }

        if (s == L->p->mods[k].tail)
            break;
    }
}

/* Module k after each module it imports, not yet ordered, in p->order from *n. */
static void qr_visit(QProg *const p, bool *const seen, const uint32_t k, uint32_t *const n)
{
    seen[k] = true;

    for (const QStmt *s = p->mods[k].head; s; s = s->next) {
        const QImport *const lists[] = {
            s->kind == QS_IMPORT || s->kind == QS_FROM ? s->imps : nullptr,
            s->kind == QS_FROM ? s->names : nullptr,
        };

        for (uint32_t j = 0; j < countof(lists); j++)
            for (const QImport *im = lists[j]; im; im = im->next)
                if (im->mod != QMOD_NONE && !seen[im->mod])
                    qr_visit(p, seen, im->mod, n);

        if (s == p->mods[k].tail)
            break;
    }

    p->order[(*n)++] = k;
}

/* The program's lines into the store, a module's after those it imports, the main's last: so each
   module's vars start in that order, before the main's first line. */
static void qr_store(Session *const S, QProg *const p, QMem *const m)
{
    bool *const seen = qast_alloc(m, p->nmods);
    uint32_t n = 0;
    p->order = qast_alloc(m, p->nmods * sizeof *p->order);
    qr_visit(p, seen, 0, &n);
    S->prog.add_flags = LF_QR;

    for (uint32_t i = 0; i < n; i++) {
        const QMod *const mod = &p->mods[p->order[i]];
        modu_file(&S->modu, p->order[i], mod->path, 0);
        S->prog.add_file = (uint16_t)p->order[i];

        for (uint32_t j = mod->first; j < mod->first + mod->nlines; j++) {
            QLine *const ln = &p->src.lines[j];
            ln->at = prog_at(&S->prog);
            S->prog.add_src = ln->src;
            prog_add(&S->prog, NONUM, (const uint8_t *)ln->text, ln->len + 1);
        }
    }

    S->prog.add_flags = 0;
    S->prog.add_src = 0;
    S->prog.add_file = 0;
}

/* The line text, then a ^ under its byte col, tabs kept so it lines up. */
static void qr_mark(Cons *const co, const char *const text, const uint32_t len, const uint32_t col)
{
    cons_spaces(co, 2);

    for (uint32_t i = 0; i < len; i++)
        cons_putc(co, (uint8_t)text[i]);

    cons_nl(co);
    cons_spaces(co, 2);

    for (uint32_t i = 0; i < col && i < len; i++)
        cons_putc(co, text[i] == '\t' ? '\t' : ' ');

    cons_putc(co, '^');
    cons_nl(co);
}

/* file:line:col: message, then the line marked. */
static void qr_say(Session *const S, const QProg *const p, const QErr *const e)
{
    Cons *const co = &S->cons;
    const QLine *const ln = e->line < p->src.nlines ? &p->src.lines[e->line] : nullptr;
    cons_puts(co, p->mods[ln ? ln->mod : 0].path);
    cons_putc(co, ':');
    cons_uint(co, ln ? ln->src : p->src.nlines);
    cons_putc(co, ':');
    cons_uint(co, e->col + 1);
    cons_puts(co, ": ");

    for (uint32_t i = 0; i < e->len; i++)
        cons_putc(co, (uint8_t)e->text[i]);

    cons_nl(co);

    if (ln)
        qr_mark(co, ln->text, ln->len, e->col);
}

/* Line ln of p's copied, the run's own, at the store's end: its source's mark kept, so a
   breakpoint stops the copy too. Its index. */
static uint32_t qr_copy(Session *const S, QProg *const p, QMem *const m, uint32_t *cap, QLine ln)
{
    const Line *const from = ln.at ? prog_line(&S->prog, ln.at) : nullptr;
    S->prog.add_flags = LF_QR | LF_INC | (from ? from->flags & LF_BP : 0);
    S->prog.add_src = ln.src;
    S->prog.add_file = from ? from->file : ln.mod; /* qr_origin finds its line by these */
    ln.at = prog_at(&S->prog);
    prog_add(&S->prog, NONUM, (const uint8_t *)ln.text, ln.len + 1);
    S->prog.add_flags = 0;
    S->prog.add_src = 0;
    S->prog.add_file = 0;
    qr_addline(p, m, cap, ln);
    return p->src.nlines - 1;
}

uint32_t qr_origin(const Prog *const pg, const uint32_t rec)
{
    const Line *const copy = prog_line(pg, rec);

    if ((copy->flags & (LF_QR | LF_INC)) != (LF_QR | LF_INC))
        return rec;

    for (uint32_t r = PROGBASE; prog_line(pg, r)->len; r += prog_line(pg, r)->len) {
        const Line *const l = prog_line(pg, r);

        if (!(l->flags & LF_INC) && l->file == copy->file && l->src == copy->src)
            return r;
    }

    return rec;
}

void qr_marks(Prog *const pg, const uint32_t rec)
{
    const Line *const l = prog_line(pg, rec);

    if (!(l->flags & LF_QR))
        return;

    for (uint32_t r = PROGBASE; prog_line(pg, r)->len; r += prog_line(pg, r)->len) {
        Line *const copy = prog_line(pg, r);

        if (copy->flags & LF_INC && copy->file == l->file && copy->src == l->src)
            copy->flags = (uint8_t)((copy->flags & ~LF_BP) | (l->flags & LF_BP));
    }
}

/* A line of the run's own for statement s, a copy of its token's, at the store's end. */
static void qr_own(Session *const S, QProg *const p, QMem *const m, uint32_t *const cap, QStmt *s)
{
    s->line = qr_copy(S, p, m, cap, p->src.lines[s->tok->line]);
}

/* Each instance's lines, a generic record's and a generic fn's, copies of its generic's at the
   store's end, its statements on them; each block lambda's so too, its lines in the store a
   procedure's own. One code block a line, each copy keeping its source's number and text. */
static void qr_instances(Session *const S, QProg *const p, QMem *const m)
{
    uint32_t cap = p->src.nlines;

    for (uint32_t i = 0; i < p->tys.n; i++) { /* first: a lambda's, a generic method's own after */
        const QTy *const y = &p->tys.at[i];

        if (!y->of)
            continue;

        const QStmt *const from = qast_ty(p, y->of)->def;
        const uint32_t base = p->src.nlines;
        const uint32_t first = from->tok->line;

        for (uint32_t k = first; k <= from->link->tok->line; k++)
            qr_copy(S, p, m, &cap, p->src.lines[k]);

        for (QStmt *s = y->def; s && s != y->def->link->next; s = s->next)
            s->line = base + s->tok->line - first;
    }

    for (const QFn *f = p->fns; f; f = f->next) {
        if (!f->of && !(f->lamof && f->sig))
            continue;

        const QFn *const from = f->of ? f->of : f;
        const uint32_t base = p->src.nlines;
        const uint32_t first = from->def->tok->line;

        for (uint32_t i = first; i <= from->end->tok->line; i++)
            qr_copy(S, p, m, &cap, p->src.lines[i]);

        for (QStmt *s = f->def;; s = s->next) {
            s->line = base + s->tok->line - first;

            if (s == f->end)
                break;
        }
    }

    for (uint32_t i = 0; i < p->nruns; i++) /* last: the run ends there, past every procedure */
        qr_own(S, p, m, &cap, p->runs[i]);

    if (p->atend)
        qr_own(S, p, m, &cap, p->atend);

    if (p->testend) /* it exits; the handlers past it are jumped to */
        qr_own(S, p, m, &cap, p->testend);

    for (uint32_t i = 0; i < p->nruns; i++)
        qr_own(S, p, m, &cap, p->runs[i]->link);
}

uint32_t qr_build(Session *const S)
{
    QMem m = {.fault = &S->fault};

    arena_init(&m.arena, QR_CHUNK);
    QProg p = {.errs = {.mem = &m}, .copies = {.mem = &m}, .testing = S->repl.testing};

    const QrLoad L = {S, &p, &m, false};

    S->repl.batch = true;
    modu_clear(&S->modu, S->repl.file);
    qr_module(S, &p, &m, S->repl.file);

    for (uint32_t k = 0; k < p.nmods; k++)
        qr_imports(&L, k);

    qr_store(S, &p, &m);
    session_drop(S);
    qsem_program(&p, &m);
    qast_sort(&p.errs);

    for (uint32_t i = 0; i < p.errs.n; i++)
        qr_say(S, &p, &p.errs.at[i]);

    if (!p.errs.n && S->repl.copying) {
        qast_sort(&p.copies);

        for (uint32_t i = 0; i < p.copies.n; i++)
            qr_say(S, &p, &p.copies.at[i]);
    }

    if (!p.errs.n) {
        qr_instances(S, &p, &m);
        qlink_program(&S->comp, &p, &m, S->dbug.emit);
    }

    arena_reset(&m.arena);
    return p.errs.n;
}

void qr_run(Session *const S)
{
    const uint32_t errors = qr_build(S);

    if (errors || S->repl.checking || S->repl.copying)
        cons_exit(&S->cons, errors ? 1 : 0);

    stmt_runfirst(S);
    exec_run(S);
}

void qr_go(Session *const S)
{
    S->vm.stop = 0; /* a key the editor kept */
    music_reset();
    stmt_runfirst(S);
    exec_run(S);
}

const char *qr_message(const Session *const S, const uint8_t code)
{
    return code == E_USR && S->vm.err.user == QRT_ASSERT ? "assert failed"
                                                         : emsg_text(S->vm.err.user, code);
}

void qr_report(Session *const S, const uint8_t code)
{
    Cons *const co = &S->cons;
    const Line *const l = prog_line(&S->prog, S->vm.line);
    const char *const text = (const char *)at(prog_addr(&S->prog, S->vm.line) + HDR);
    cons_puts(co, modu_path(&S->modu, l->file));
    cons_putc(co, ':');
    cons_uint(co, l->src);
    cons_puts(co, ": ");
    cons_puts(co, qr_message(S, code));
    cons_nl(co);
    cons_spaces(co, 2);
    cons_puts(co, text);
    cons_nl(co);
}

/* ---- the editor's */

/* The run's own lines out of the store: a build's. */
static void qr_unrun(Prog *const pg)
{
    for (uint32_t rec = PROGBASE; prog_line(pg, rec)->len;)
        if ((prog_line(pg, rec)->flags & (LF_QR | LF_INC)) == (LF_QR | LF_INC))
            prog_cut(pg, rec);
        else
            rec += prog_line(pg, rec)->len;
}

/* Module d's lines out of the store, those of the files after it a number down; d out of the
   modules. */
static void qr_drop(Session *const S, const uint32_t d)
{
    Prog *const pg = &S->prog;

    for (uint32_t rec = PROGBASE; prog_line(pg, rec)->len;) {
        Line *const l = prog_line(pg, rec);

        if (l->flags & LF_QR && l->file == d) {
            prog_cut(pg, rec);
            continue;
        }

        if (l->flags & LF_QR && l->file > d)
            l->file--;

        rec += l->len;
    }

    modu_drop(&S->modu, d);
    modu_relink(&S->modu, S->repl.file, modu_count(&S->modu));
}

/* The store's program, repl.file the main's, as L's: the run's own lines out; each module's lines
   gathered, then the modules their imports name read in after the rest; p->order each module after
   those it imports, the main's last. A module no import reaches is dropped and the lot gathered
   again, unless edited: that one is the program's still, before the main. */
static void qr_enter(const QrLoad *const L)
{
    Session *const S = L->S;
    QProg *const p = L->p;
    const QProg fresh = *p;
    qr_unrun(&S->prog);
    modu_relink(&S->modu, S->repl.file, modu_count(&S->modu));

    for (uint32_t k = 0; k < modu_count(&S->modu); k++)
        qr_gather(S, p, L->m, k, modu_module_path(&S->modu, k));

    for (uint32_t k = 0; k < p->nmods; k++)
        qr_imports(L, k);

    bool *const seen = qast_alloc(L->m, p->nmods);
    uint32_t n = 0;
    p->order = qast_alloc(L->m, p->nmods * sizeof *p->order);
    qr_visit(p, seen, 0, &n);
    const uint32_t main = n - 1;

    for (uint32_t k = 1; k < p->nmods; k++)
        if (!seen[k] && modu_mod(&S->modu, k)->edited)
            qr_visit(p, seen, k, &n);

    for (uint32_t k = 1; k < p->nmods; k++)
        if (!seen[k]) {
            qr_drop(S, k);
            *p = fresh;
            qr_enter(L);
            return;
        }

    memmove(p->order + main, p->order + main + 1, (n - main - 1) * sizeof *p->order);
    p->order[n - 1] = 0;
}

/* Error e of p's to *out: its line's record, its column, its message. One past the lines: the
   last's end. */
static void qr_said(const QProg *const p, const QErr *const e, QrErr *const out)
{
    const bool past = e->line >= p->src.nlines;
    const QLine *const ln =
        p->src.nlines ? &p->src.lines[past ? p->src.nlines - 1 : e->line] : nullptr;
    const uint32_t n = out->cap && e->len >= out->cap ? out->cap - 1 : e->len;
    out->rec = ln ? ln->at : PROGBASE;
    out->col = ln && past ? ln->len : e->col;

    if (!out->cap)
        return;

    memcpy(out->msg, e->text, n);
    out->msg[n] = 0;
}

void qr_open(Session *const S)
{
    QMem m = {.fault = &S->fault};

    arena_init(&m.arena, QR_CHUNK);
    QProg p = {.errs = {.mem = &m}, .copies = {.mem = &m}};

    qr_enter(&(QrLoad){S, &p, &m, true});

    arena_reset(&m.arena);
}

/* The run's end past the main's last line: an empty line of the run's own, p->end. */
static void qr_end(Session *const S, QProg *const p, QMem *const m)
{
    uint32_t cap = p->src.nlines;
    p->end = qr_copy(S, p, m, &cap, (QLine){.text = ""}) + 1;
}

void qr_forget(QrKeep *const k)
{
    arena_reset(&k->mem.arena);
    qrt_sorts_free(&k->sorts);
    k->prog = nullptr;
    k->said[0] = 0;
}

uint32_t qr_link(Session *const S, QrErr *const e)
{
    QrKeep *const k = &S->qr;
    qr_forget(k);
    k->mem = (QMem){.fault = &S->fault};

    arena_init(&k->mem.arena, QR_CHUNK);
    QMem *const m = &k->mem;
    QProg *const p = qast_alloc(m, sizeof *p);
    *p = (QProg){.errs = {.mem = m}, .copies = {.mem = m}, .keep = true};

    qr_enter(&(QrLoad){S, p, m, true});

    session_drop(S);
    qsem_program(p, m);
    qast_sort(&p->errs);

    if (p->errs.n) {
        qr_said(p, &p->errs.at[0], e);
        qr_forget(k);
        return p->errs.n;
    }

    qr_end(S, p, m);
    qr_instances(S, p, m);
    qlink_program(&S->comp, p, m, S->dbug.emit);
    k->prog = p;
    return 0;
}

uint32_t qr_spell(const QrKeep *const k, const uint32_t id, char *const d)
{
    const QVar *v = k->prog ? k->prog->vars : nullptr;

    while (v && (v->id != id || !v->name))
        v = v->next;

    if (!v)
        return 0;

    const uint32_t n = v->name->len < QR_SPELL ? v->name->len : QR_SPELL;
    memcpy(d, v->name->text, n);
    return n;
}

/* The names in sight at program line rec's statement; null the modules' alone. */
static const QScopeAt *qr_sight(const QProg *const p, const uint32_t rec)
{
    for (const QStmt *s = p->first; rec && rec != DLINE && s; s = s->next)
        if (p->src.lines[s->line].at == rec && s->at)
            return s->at;

    return nullptr;
}

/* The probe's line, cons.inbuf's text: lexed as p's last line, in module mod; its first token. */
static const QTok *qr_probe_line(
    Session *const S,
    QProg *const p,
    QMem *const m,
    const uint16_t mod
)
{
    const char *const text = (const char *)S->cons.inbuf;
    uint32_t cap = p->src.nlines;
    qr_addline(
        p,
        m,
        &cap,
        (QLine){
            .at = DLINE,
            .text = text,
            .len = (uint32_t)strlen(text),
            .mod = mod,
        }

    );
    qlex_source(&p->src, p->src.nlines - 1, m, &p->errs);
    return p->src.toks;
}

/* Is p's last error a value written alone? */
static bool qr_alone(const QProg *const p)
{
    static const char alone[] = "a value alone";
    const QErr *const e = p->errs.n ? &p->errs.at[p->errs.n - 1] : nullptr;
    return e && e->len >= sizeof alone - 1 && !memcmp(e->text, alone, sizeof alone - 1);
}

/* cons.inbuf's text as print text. */
static void qr_printed(Session *const S)
{
    static const char pre[] = "print ";
    uint8_t *const b = S->cons.inbuf;
    const size_t n = strlen((const char *)b);
    const size_t room = INMAX - (sizeof pre - 1) - 1;
    const size_t k = n < room ? n : room;
    memmove(b + sizeof pre - 1, b, k);
    memcpy(b, pre, sizeof pre - 1);
    b[sizeof pre - 1 + k] = 0;
}

void qr_direct_code(Session *const S, const uint32_t rec)
{
    QrKeep *const k = &S->qr;
    QProg *const p = k->prog;
    k->said[0] = 0;

    if (!p) {
        strcpy(k->said, "the program is not linked: run it first");
        fault_error(&S->fault, E_SN);
    }

    QMem m = {.fault = &S->fault}; /* the probe's own: dropped after, p as it was */
    arena_init(&m.arena, QR_CHUNK);
    const QSrc src = p->src;
    const QTys tys = p->tys;
    const QErrs errs = p->errs;
    QVar *last = p->vars;

    while (last && last->next)
        last = last->next;

    p->errs.mem = &m;
    const QScopeAt *const at = qr_sight(p, rec);
    const uint16_t mod = at && rec && rec != DLINE ? prog_line(&S->prog, rec)->file : 0;
    QStmt *s = qpar_line(p, &m, qr_probe_line(S, p, &m, mod));

    if (!s && qr_alone(p)) {
        p->errs.n = errs.n;
        p->src = src;
        qr_printed(S);
        s = qpar_line(p, &m, qr_probe_line(S, p, &m, mod));
    }

    const bool ok =
        s && qsem_probe(p, &m, at, s) && p->tys.n == tys.n &&
        qlink_direct(&S->comp, p, &m, last ? last->next : p->vars, s, addr_of(S->cons.inbuf));

    if (!ok && p->errs.n > errs.n) {
        const QErr *const e = &p->errs.at[errs.n];
        const uint32_t n = e->len < sizeof k->said - 1 ? e->len : sizeof k->said - 1;
        memcpy(k->said, e->text, n);
        k->said[n] = 0;
    } else if (!ok) {
        strcpy(k->said, "this can't run here");
    }

    if (last)
        last->next = nullptr;
    else
        p->vars = nullptr;

    p->src = src;
    p->tys = tys;
    p->errs = errs;
    arena_reset(&m.arena);

    if (!ok)
        fault_error(&S->fault, E_SN);
}

/* File file's lines, p's only module: lexed and parsed (p->loose), the errors sorted. */
static void qr_lines(Session *const S, QProg *const p, QMem *const m, const uint32_t file)
{
    qr_gather(S, p, m, file, "");
    qast_sort(&p->errs);
}

bool qr_syntax(Session *const S, const uint32_t rec, QrErr *const e)
{
    QMem m = {.fault = &S->fault};

    arena_init(&m.arena, QR_CHUNK);
    QProg p = {.errs = {.mem = &m}, .copies = {.mem = &m}, .loose = true};

    qr_lines(S, &p, &m, prog_line(&S->prog, rec)->file);
    bool found = false;

    for (uint32_t i = 0; !found && i < p.errs.n; i++)
        if (p.errs.at[i].line < p.src.nlines && p.src.lines[p.errs.at[i].line].at == rec) {
            qr_said(&p, &p.errs.at[i], e);
            found = true;
        }

    arena_reset(&m.arena);
    return found;
}

void qr_flag(Session *const S)
{
    QMem m = {.fault = &S->fault};

    arena_init(&m.arena, QR_CHUNK);

    for (uint32_t k = 0; k < modu_count(&S->modu); k++) {
        QProg p = {.errs = {.mem = &m}, .copies = {.mem = &m}, .loose = true};

        qr_lines(S, &p, &m, k);

        for (uint32_t i = 0; i < p.src.nlines; i++)
            prog_line(&S->prog, p.src.lines[i].at)->flags &= ~LF_ASIS;

        for (uint32_t i = 0; i < p.errs.n; i++)
            if (p.errs.at[i].line < p.src.nlines)
                prog_line(&S->prog, p.src.lines[p.errs.at[i].line].at)->flags |= LF_ASIS;
    }

    arena_reset(&m.arena);
}

/* Is c a name's: a letter, a digit or _? */
static bool qr_namec(const char c)
{
    return is_letter((uint8_t)c) || is_digit((uint8_t)c) || c == '_';
}

/* Past the word w at text's *at and the blanks after it: true, *at past them. */
static bool qr_word(const char *const text, const uint32_t len, uint32_t *const at, const char *w)
{
    const uint32_t n = (uint32_t)strlen(w);

    if (*at + n > len || memcmp(text + *at, w, n) || (*at + n < len && qr_namec(text[*at + n])))
        return false;

    *at += n;

    while (*at < len && text[*at] == ' ')
        (*at)++;

    return true;
}

bool qr_fn_at(const char *const text, const uint32_t len, uint32_t *const at, uint32_t *const n)
{
    uint32_t i = 0;

    while (i < len && text[i] == ' ')
        i++;

    qr_word(text, len, &i, "public");
    qr_word(text, len, &i, "native");

    if (!qr_word(text, len, &i, "function"))
        return false;

    uint32_t end = i;

    if (qr_word(text, len, &end, "operator")) /* its operator too: function operator + */
        while (end < len && text[end] != ' ' && text[end] != '(')
            end++;
    else
        while (end < len && qr_namec(text[end]))
            end++;

    if (end == i || is_digit((uint8_t)text[i]))
        return false;

    *at = i;
    *n = end - i;
    return true;
}

/* The line text, len bytes, lexed alone in m: its tokens to *toks, their count, the line's end
   left out. */
static uint32_t qr_lexed(
    QMem *const m,
    const char *const text,
    const uint32_t len,
    const QTok **const toks
)
{
    QErrs errs = {.mem = m};

    QLine ln = {.text = text, .len = len};

    QSrc src = {.lines = &ln, .nlines = 1};

    qlex_source(&src, 0, m, &errs);
    uint32_t n = src.ntoks;

    while (n && (src.toks[n - 1].kind == QK_NL || src.toks[n - 1].kind == QK_EOF))
        n--;

    *toks = src.toks;
    return n;
}

uint32_t qr_tokens(
    Fault *const f,
    const char *const text,
    const uint32_t len,
    QrTokFn *const fn,
    void *const ctx
)
{
    QMem m = {.fault = f};

    arena_init(&m.arena, QR_LINE_CHUNK);
    const QTok *toks;
    const uint32_t n = qr_lexed(&m, text, len, &toks);
    uint32_t end = 0;

    for (uint32_t i = 0; i < n; i++) {
        if (toks[i].kind == QK_NL)
            continue;

        fn(ctx, &toks[i]);
        end = toks[i].col + toks[i].len;
    }

    arena_reset(&m.arena);

    while (end < len && text[end] != '\'')
        end++;

    return end;
}

/* ---- completion */

static void qr_no_token(void *const ctx, const QTok *const t) {}

typedef struct {
    Session *S;
    QMem *m;
    QrSuggFn *fn;
    void *ctx;
} QrSugg;

/* File file's next text line past rec, 0 its first; 0 none. */
static uint32_t qr_next_line(const Prog *const pg, const uint32_t file, const uint32_t rec)
{
    uint32_t r = rec ? rec + prog_line(pg, rec)->len : PROGBASE;

    for (; prog_line(pg, r)->len; r += prog_line(pg, r)->len)
        if (qr_text(prog_line(pg, r), file))
            return r;

    return 0;
}

/* Line rec's tokens to *toks: their count. */
static uint32_t qr_line_toks(const QrSugg *const g, const uint32_t rec, const QTok **const toks)
{
    const char *const body = (const char *)at(prog_addr(&g->S->prog, rec) + HDR);
    return qr_lexed(g->m, body, (uint32_t)strlen(body), toks);
}

/* Token t is the name text, n bytes. */
static bool qr_named(const QTok *const t, const char *const text, const uint32_t n)
{
    return t->kind == QK_NAME && t->len == n && !memcmp(t->text, text, n);
}

/* Past a declaration's public and native: its keyword's index in toks. */
static uint32_t qr_decl(const QTok *const toks, const uint32_t n)
{
    uint32_t i = 0;

    while (i < n && (toks[i].kind == QK_PUBLIC || toks[i].kind == QK_NATIVE))
        i++;

    return i;
}

/* The module named name, n bytes: its file's name less its extension; modu_count none. */
static uint32_t qr_module_named(const Session *const S, const char *const name, const uint32_t n)
{
    for (uint32_t k = 0; k < modu_count(&S->modu); k++) {
        const char *base = modu_module_path(&S->modu, k);

        for (const char *c = base; *c; c++)
            if (*c == '/' || *c == '\\')
                base = c + 1;

        uint32_t i = 0;

        while (i < n && base[i] && upcase((uint8_t)base[i]) == upcase((uint8_t)name[i]))
            i++;

        if (i == n && base[n] == '.')
            return k;
    }

    return modu_count(&S->modu);
}

/* Module k's public names. */
static void qr_members(const QrSugg *const g, const uint32_t k)
{
    for (uint32_t rec = qr_next_line(&g->S->prog, k, 0); rec;

         rec = qr_next_line(&g->S->prog, k, rec)) {
        const QTok *toks;
        const uint32_t n = qr_line_toks(g, rec, &toks);
        const uint32_t i = qr_decl(toks, n);

        if (!i || toks[0].kind != QK_PUBLIC || i + 1 >= n || toks[i + 1].kind != QK_NAME)
            continue;

        const bool fn = toks[i].kind == QK_FUNCTION;
        g->fn(g->ctx, toks[i + 1].text, toks[i + 1].len, fn ? QRS_FN : QRS_NAME, rec);
    }
}

/* Variable name's (n bytes) type as file declares it, x as [m.]T or let x = [m.]T(..): T's token,
   nullptr none. */
static const QTok *qr_type_of(
    const QrSugg *const g,
    const uint32_t file,
    const char *const name,
    const uint32_t n
)
{
    for (uint32_t rec = qr_next_line(&g->S->prog, file, 0); rec;

         rec = qr_next_line(&g->S->prog, file, rec)) {
        const QTok *toks;
        const uint32_t nt = qr_line_toks(g, rec, &toks);

        for (uint32_t i = 0; i + 2 < nt; i++) {
            if (!qr_named(&toks[i], name, n) ||
                (toks[i + 1].kind != QK_AS && toks[i + 1].kind != QK_EQ) ||
                toks[i + 2].kind != QK_NAME)
                continue;

            uint32_t t = i + 2; /* m.T's T */

            while (t + 2 < nt && toks[t + 1].kind == QK_DOT && toks[t + 2].kind == QK_NAME)
                t += 2;

            return &toks[t];
        }
    }

    return nullptr;
}

/* Type t's (tn bytes) members as any file declares them, a private one in file alone: on a value
   its fields and its methods with self; on the type its methods without self and an enum's
   values. */
static void qr_fields(
    const QrSugg *const g,
    const uint32_t file,
    const char *const t,
    const uint32_t tn,
    const bool value
)
{
    for (uint32_t k = 0; k < modu_count(&g->S->modu); k++) {
        bool in = false;   /* in type t's lines */
        bool body = false; /* in a method's */

        for (uint32_t rec = qr_next_line(&g->S->prog, k, 0); rec;

             rec = qr_next_line(&g->S->prog, k, rec)) {
            const QTok *all;
            const uint32_t na = qr_line_toks(g, rec, &all);
            const bool priv = na && all[0].kind == QK_PRIVATE;
            const QTok *const toks = all + priv;
            const uint32_t n = na - priv;
            const uint32_t i = qr_decl(toks, n);
            const bool end = n >= 2 && toks[0].kind == QK_END;

            if (priv && k != file)
                continue;

            if (in && end && (toks[1].kind == QK_TYPE || toks[1].kind == QK_ENUM))
                in = false;
            else if (in && end && toks[1].kind == QK_FN)
                body = false;
            else if (in && !body && i + 3 < n && toks[i].kind == QK_FN &&
                     toks[i + 1].kind == QK_NAME && toks[i + 2].kind == QK_LPAREN) {
                const uint32_t at = i + 3 + (toks[i + 3].kind == QK_MUT);
                const bool self = at < n && toks[at].kind == QK_SELF;
                body = true;

                if (self == value)
                    g->fn(g->ctx, toks[i + 1].text, toks[i + 1].len, QRS_FN, rec);
            } else if (in && !body && n && toks[0].kind == QK_NAME) {
                const bool field = n >= 2 && toks[1].kind == QK_AS;

                if (field == value)
                    g->fn(g->ctx, toks[0].text, toks[0].len, QRS_NAME, rec);
            } else if (i + 1 < n && (toks[i].kind == QK_TYPE || toks[i].kind == QK_ENUM)) {
                in = qr_named(&toks[i + 1], t, tn);
                body = false;
            }
        }
    }
}

/* The keywords; file's names, not a field's after '.'. */
static void qr_scope(const QrSugg *const g, const uint32_t file)
{
    for (QKind k = QK_AND; k <= QK_FUNCTION; k++)
        g->fn(g->ctx, qlex_keyword(k), (uint32_t)strlen(qlex_keyword(k)), QRS_KW, 0);

    for (uint32_t rec = qr_next_line(&g->S->prog, file, 0); rec;

         rec = qr_next_line(&g->S->prog, file, rec)) {
        const QTok *toks;
        const uint32_t n = qr_line_toks(g, rec, &toks);

        for (uint32_t i = 0; i < n; i++)
            if (toks[i].kind == QK_NAME && (!i || toks[i - 1].kind != QK_DOT))
                g->fn(
                    g->ctx,
                    toks[i].text,
                    toks[i].len,
                    i && toks[i - 1].kind == QK_FUNCTION ? QRS_FN : QRS_NAME,
                    rec
                );
    }
}

void qr_suggest(
    Session *const S,
    const uint32_t rec,
    const char *const text,
    const uint32_t n,
    QrSuggFn *const fn,
    void *const ctx
)
{
    uint32_t quotes = 0;

    for (uint32_t i = 0; i < n; i++)
        quotes += text[i] == '"';

    if (quotes % 2 ||
        qr_tokens(&S->fault, text, n, qr_no_token, nullptr) < n) /* a string's, a comment's */
        return;

    QMem m = {.fault = &S->fault};

    arena_init(&m.arena, QR_LINE_CHUNK);
    const QrSugg g = {S, &m, fn, ctx};

    const uint32_t file = prog_line(&S->prog, rec)->len ? prog_line(&S->prog, rec)->file : 0;
    /* the name's end before a '.' at n - 1, past its [..]: Stack[int]. */
    uint32_t e = n ? n - 1 : 0;

    for (uint32_t depth = 0; e && (depth || text[e - 1] == ']');) {
        depth += text[e - 1] == ']';
        depth -= text[e - 1] == '[';
        e--;
    }

    uint32_t b = e;

    while (b && qr_namec(text[b - 1]))
        b--;

    if (!n || text[n - 1] != '.')
        qr_scope(&g, file);
    else if (b < e && !is_digit((uint8_t)text[b]) && (!b || text[b - 1] != '.')) {
        const uint32_t k = qr_module_named(S, text + b, e - b);
        const QTok *const t = qr_type_of(&g, file, text + b, e - b);

        if (k < modu_count(&S->modu) && e == n - 1)
            qr_members(&g, k);
        else if (t)
            qr_fields(&g, file, t->text, t->len, true);
        else
            qr_fields(&g, file, text + b, e - b, false);
    }

    arena_reset(&m.arena);
}
