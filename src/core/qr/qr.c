/* Quickr, the modern dialect: a .qr file and the modules it imports loaded, checked, linked and
 * run.
 *
 * # How it works
 * Each source line becomes a line of the program store (LF_QR), its body the line's text, so the
 * editor, an error's place and the debugger work line by line as for QBasic. The front end then
 * runs in stages, each over the whole file: lex (qlex), parse (qpar), check (qsem). Every error
 * any stage finds is reported, in the source's order, before anything runs:
 *
 *   prog.qr:3:7: unknown name y
 *     print(y)
 *           ^
 *
 * Each import loads its module's file once (found by io/locate.c, as a .qr): its lines, its own
 * tokens and its statements, after the main file's. The store holds a module's lines before those
 * of the modules importing it, the main's last, so the modules' vars start in that order.
 *
 * A clean program is linked (qlink) and run by the VM as a QBasic one is. A runtime error stops
 * it at its line: prog.qr:5: Division by zero, then the line.
 */
#include "qr/qr.h"
#include "session.h"
#include "io/errmsg.h"
#include "io/locate.h"
#include "plat/pfil.h"
#include "qr/qast.h"
#include "qr/qlex.h"
#include "qr/qlink.h"
#include "qr/qpar.h"
#include "qr/qrt.h"
#include "qr/qsem.h"

enum { QR_CHUNK = 0x10000 }; /* the front end's arena's blocks at least */

bool qr_is_file(const char *const path)
{
    const size_t n = strlen(path);
    return n > 3 && path[n - 3] == '.' && downcase((uint8_t)path[n - 2]) == 'q' &&
           downcase((uint8_t)path[n - 1]) == 'r';
}

/* A platform error dos, if any, raised as QBasic's. */
static void qr_check(Session *const S, const uint16_t dos)
{
    if (dos)
        fault_check(&S->fault, chnl_doserr(dos));
}

/* The bytes of the file at path in the arena, a 0 after them; *len how many. */
static char *qr_read(Session *const S, QMem *const m, const char *const path, uint32_t *const len)
{
    PfilHandle h;
    qr_check(S, pfil_open(path, PFIL_READ, &h));
    uint32_t size = 0, got = 0;
    pfil_seek(h, 0, PFIL_END, &size);
    pfil_seek(h, 0, PFIL_SET, nullptr);
    char *const text = qast_alloc(m, size + 1);
    const uint16_t st = pfil_read(h, text, size, &got);
    pfil_close(h);
    qr_check(S, st);
    *len = got;
    return text;
}

/* The file at path a new module of p: its lines, lexed and parsed. Its number. */
static uint32_t qr_module(Session *const S, QProg *const p, QMem *const m, const char *const path)
{
    uint32_t len, cap = p->src.nlines;
    char *const text = qr_read(S, m, path, &len);
    const uint32_t k = p->nmods;
    const uint32_t n = (uint32_t)strlen(path);
    char *const name = qast_alloc(m, n + 1);
    memcpy(name, path, n);
    p->mods = qast_room(m, p->mods, p->nmods, &p->capmods, sizeof *p->mods);
    p->nmods++;
    p->mods[k] = (QMod){.path = name, .first = p->src.nlines};

    if (k)
        p->mods[k].base = p->mods[k - 1].base + p->mods[k - 1].ntoks;

    for (uint32_t at = 0, src = 1; at < len; src++) {
        uint32_t end = at;

        while (end < len && text[end] != '\n')
            end++;

        const uint32_t w = end - at - (end > at && text[end - 1] == '\r');
        text[at + w] = 0;
        p->src.lines = qast_room(m, p->src.lines, p->src.nlines, &cap, sizeof *p->src.lines);
        p->src.lines[p->src.nlines++] = (QLine){
            .text = text + at,
            .len = w,
            .src = (uint16_t)src,
            .mod = (uint16_t)k,
        };

        at = end + 1;
    }

    QMod *const mod = &p->mods[k];
    mod->nlines = p->src.nlines - mod->first;
    qlex_source(&p->src, mod->first, m, &p->errs);
    mod->toks = p->src.toks;
    mod->ntoks = p->src.ntoks;
    qpar_program(p, m, mod);
    return k;
}

/* Are paths a and b the same, case aside? */
static bool qr_same(const char *a, const char *b)
{
    while (*a && downcase((uint8_t)*a) == downcase((uint8_t)*b))
        a++, b++;

    return !*a && !*b;
}

/* Module im of p, found from module k's file: loaded once, its number in im->mod; a package's
   folder (folder not null) to folder. Not found: an error at its name. */
static void qr_find(
    Session *const S,
    QProg *const p,
    QMem *const m,
    const uint32_t k,
    QImport *const im,
    char *const folder
)
{
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
        .ext = ".qr",
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

    const char *const at = folder ? folder : path;

    for (uint32_t i = 0; i < p->nmods; i++)
        if (qr_same(p->mods[i].path, at)) {
            im->mod = i;
            return;
        }

    im->mod = qr_module(S, p, m, at);
}

/* A name from gives, of the package in folder: its module, loaded once. */
static void qr_part(Session *const S, QProg *const p, QMem *const m, const char *dir, QImport *im)
{
    char path[PFIL_PATH];
    const ImptName name = {.text = (const uint8_t *)im->tok->text, .len = (uint8_t)im->tok->len};

    if (!locate_in(dir, &name, ".qr", path)) {
        qast_error(&p->errs, im->tok, "no module {} in the package", im->tok->text, im->tok->len);
        return;
    }

    for (uint32_t i = 0; i < p->nmods; i++)
        if (qr_same(p->mods[i].path, path)) {
            im->mod = i;
            return;
        }

    im->mod = qr_module(S, p, m, path);
}

/* Each module module k's imports and froms name, loaded once. */
static void qr_imports(Session *const S, QProg *const p, QMem *const m, const uint32_t k)
{
    for (QStmt *s = p->mods[k].head; s; s = s->next) {
        if (s->kind == QS_IMPORT)
            for (QImport *im = s->imps; im; im = im->next)
                qr_find(S, p, m, k, im, nullptr);

        if (s->kind == QS_FROM) {
            char dir[PFIL_PATH];
            qr_find(S, p, m, k, s->imps, dir);

            for (QImport *im = s->names; s->imps->folder && im; im = im->next)
                qr_part(S, p, m, dir, im);
        }

        if (s == p->mods[k].tail)
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

/* A line of the run's own for statement s, a copy of its token's, at the store's end. */
static void qr_own(Session *const S, QProg *const p, QMem *const m, uint32_t *const cap, QStmt *s)
{
    const QLine ln = p->src.lines[s->tok->line];
    p->src.lines = qast_room(m, p->src.lines, p->src.nlines, cap, sizeof *p->src.lines);
    s->line = p->src.nlines;
    p->src.lines[p->src.nlines++] = (QLine){
        .at = prog_at(&S->prog),
        .text = ln.text,
        .len = ln.len,
        .src = ln.src,
        .mod = ln.mod,
    };

    S->prog.add_src = ln.src;
    S->prog.add_file = ln.mod;
    prog_add(&S->prog, NONUM, (const uint8_t *)ln.text, ln.len + 1);
}

/* Each instance's lines, copies of its generic's at the store's end, its statements on them; each
   block lambda's so too, its lines in the store a procedure's own. One code block a line, each copy
   keeping its source's number and text. */
static void qr_instances(Session *const S, QProg *const p, QMem *const m)
{
    uint32_t cap = p->src.nlines;
    S->prog.add_flags = LF_QR;

    for (const QFn *f = p->fns; f; f = f->next) {
        if (!f->of && !(f->lamof && f->sig))
            continue;

        const QFn *const from = f->of ? f->of : f;
        const uint32_t base = p->src.nlines;
        const uint32_t first = from->def->tok->line;

        for (uint32_t i = first; i <= from->end->tok->line; i++) {
            const QLine ln = p->src.lines[i];
            p->src.lines = qast_room(m, p->src.lines, p->src.nlines, &cap, sizeof *p->src.lines);
            p->src.lines[p->src.nlines++] = (QLine){
                .at = prog_at(&S->prog),
                .text = ln.text,
                .len = ln.len,
                .src = ln.src,
                .mod = ln.mod,
            };

            S->prog.add_src = ln.src;
            S->prog.add_file = ln.mod;
            prog_add(&S->prog, NONUM, (const uint8_t *)ln.text, ln.len + 1);
        }

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

    S->prog.add_flags = 0;
    S->prog.add_src = 0;
    S->prog.add_file = 0;
}

uint32_t qr_build(Session *const S)
{
    QMem m = {.fault = &S->fault};

    arena_init(&m.arena, QR_CHUNK);
    QProg p = {.errs = {.mem = &m}, .testing = S->repl.testing};

    S->repl.batch = true;
    modu_clear(&S->modu, S->repl.file);
    qr_module(S, &p, &m, S->repl.file);

    for (uint32_t k = 0; k < p.nmods; k++)
        qr_imports(S, &p, &m, k);

    qr_store(S, &p, &m);
    session_drop(S);
    qsem_program(&p, &m);
    qast_sort(&p.errs);

    for (uint32_t i = 0; i < p.errs.n; i++)
        qr_say(S, &p, &p.errs.at[i]);

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

    if (errors || S->repl.checking)
        cons_exit(&S->cons, errors ? 1 : 0);

    stmt_runfirst(S);
    exec_run(S);
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
    cons_puts(
        co,
        code == E_USR && S->vm.err.user == QRT_ASSERT ? "assert failed"
                                                      : emsg_text(S->vm.err.user, code)
    );
    cons_nl(co);
    cons_spaces(co, 2);
    cons_puts(co, text);
    cons_nl(co);
}
