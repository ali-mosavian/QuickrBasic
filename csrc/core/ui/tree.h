/* The editor's widget tree: keys down the focus path and back up, drawing parent first.
 *
 * # How it works
 * A widget has children, one of them focused. A key goes to the deepest visible focused widget,
 * then up through its parents till one handles it; a modal child takes the focus while it is
 * shown. A mouse event goes to the deepest visible widget whose rect holds it, then up; a shown
 * modal child takes it wherever it is. Drawing draws a widget, then its visible children in
 * order. Widgets are static instances; tree_add links them once.
 *
 * A widget's ops are the editor's event handlers, top-level: they take the Session, which the tree
 * only passes on.
 */
#ifndef UI_TREE_H
#define UI_TREE_H
#include "base.h"

enum { TREE_MAX_CHILDREN = 8 };

typedef struct Widget Widget;

typedef struct {
    uint8_t row, col, rows, cols;
} Rect;

/* An op may be null: nothing, not handled. */
typedef struct {
    void (*draw)(Widget *w, Session *S);                            /* itself; children after */
    bool (*key)(Widget *w, Session *S, uint16_t key);               /* true: handled */
    bool (*mouse)(Widget *w, Session *S, uint8_t row, uint8_t col); /* uimn.mouse's event */
} WidgetOps;

struct Widget {
    const WidgetOps *ops;
    Rect rect; /* its cells on the screen */
    Widget *parent, *children[TREE_MAX_CHILDREN];
    uint8_t nchildren, focus; /* focus: the focused child's index */
    bool visible, modal;      /* modal: shown, it has the keys whatever the focus */
};

/* child the last of parent's children. */
void tree_add(Widget *parent, Widget *child);
/* The deepest visible focused widget under w, w itself if none. */
Widget *tree_focused(Widget *w);
/* w's index among its parent's children. */
uint8_t tree_index(const Widget *w);
/* w focused, and each parent's focus on the path down to it. */
void tree_focus(Widget *w);
/* key to the focused widget under root, then up; false: none handled it. */
bool tree_key(Widget *root, Session *S, uint16_t key);
/* The deepest visible widget under root whose rect holds row, col; root if none. */
Widget *tree_at(Widget *root, uint8_t row, uint8_t col);
/* The mouse at row, col to the widget under it under root, then up; false: none handled it. */
bool tree_mouse(Widget *root, Session *S, uint8_t row, uint8_t col);
/* root and its visible children drawn, parents first. */
void tree_draw(Widget *root, Session *S);
#endif
