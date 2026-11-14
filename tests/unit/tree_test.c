/* The editor's widget tree (ui/tree). */
#include "ui/tree.h"
#include "unit.h"

static char seen[16]; /* the widgets' names, in the order their ops ran */
static uint32_t nseen;

typedef struct {
    Widget w;
    char name;
    uint16_t takes; /* the key it handles, 0 none */
} Probe;

static void note(const Widget *const w) { seen[nseen++] = ((const Probe *)w)->name; }
static void draw(Widget *const w, Session *const S) { note(w); }
static bool key(Widget *const w, Session *const S, const uint16_t k)
{
    note(w);
    return k == ((const Probe *)w)->takes;
}
/* A widget handles the mouse on the row its key is. */
static bool mouse(Widget *const w, Session *const S, const uint8_t row, const uint8_t col)
{
    note(w);
    return row == ((const Probe *)w)->takes;
}
static const WidgetOps ops = {.draw = draw, .key = key, .mouse = mouse};

/* r, its children a and b, a's child c; all shown, the focus on a then c. */
static Probe r, a, b, c;

static void build(void)
{
    r = (Probe){.w = {.ops = &ops, .visible = true}, .name = 'r', .takes = 1};

    a = (Probe){.w = {.ops = &ops, .visible = true}, .name = 'a', .takes = 2};

    b = (Probe){.w = {.ops = &ops, .visible = true}, .name = 'b', .takes = 3};

    c = (Probe){.w = {.ops = &ops, .visible = true}, .name = 'c', .takes = 4};

    tree_add(&r.w, &a.w);
    tree_add(&r.w, &b.w);
    tree_add(&a.w, &c.w);
    nseen = 0;
    memset(seen, 0, sizeof seen);
}

TEST(tree_key_goes_to_the_deepest_focused_then_up)
{
    build();
    CHECK(tree_key(&r.w, S, 1));
    CHECK(!strcmp(seen, "car"));
    nseen = 0;
    memset(seen, 0, sizeof seen);
    CHECK(!tree_key(&r.w, S, 3)); /* b's key, b not focused: none */
    CHECK(!strcmp(seen, "car"));
}

TEST(tree_key_skips_a_hidden_child_and_obeys_a_modal_one)
{
    build();
    c.w.visible = false;
    CHECK(tree_key(&r.w, S, 2));
    CHECK(!strcmp(seen, "a"));
    nseen = 0;
    memset(seen, 0, sizeof seen);
    b.w.modal = true;
    CHECK(tree_key(&r.w, S, 3));
    CHECK(!strcmp(seen, "b"));
}

TEST(tree_focus_sets_the_path)
{
    build();
    tree_focus(&b.w);
    CHECK(tree_focused(&r.w) == &b.w);
    tree_focus(&c.w);
    CHECK(tree_focused(&r.w) == &c.w);
}

/* r rows 0-9; a 0-4, its c 0-1; b 5-9. */
static void place(void)
{
    r.w.rect = (Rect){0, 0, 10, 10};

    a.w.rect = (Rect){0, 0, 5, 10};

    c.w.rect = (Rect){0, 0, 2, 10};

    b.w.rect = (Rect){5, 0, 5, 10};

    nseen = 0;
    memset(seen, 0, sizeof seen);
}

TEST(tree_mouse_goes_to_the_widget_under_it_then_up)
{
    build();
    place();
    CHECK(tree_mouse(&r.w, S, 1, 3)); /* c's cells, r's row */
    CHECK(!strcmp(seen, "car"));
    place();
    CHECK(!tree_mouse(&r.w, S, 3, 3)); /* a's cells: neither a nor r takes row 3 */
    CHECK(!strcmp(seen, "ar"));
    place();
    CHECK(!tree_mouse(&r.w, S, 7, 3)); /* b's cells: b takes row 3 only */
    CHECK(!strcmp(seen, "br"));
    place();
    b.w.modal = true;
    CHECK(tree_mouse(&r.w, S, 1, 3)); /* in c's cells, but a modal b has it first */
    CHECK(!strcmp(seen, "br"));
}

TEST(tree_draw_parents_first_shown_only)
{
    build();
    b.w.visible = false;
    tree_draw(&r.w, S);
    CHECK(!strcmp(seen, "rac"));
}
