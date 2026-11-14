/* The editor's widget tree (tree.h). */
#include "ui/tree.h"

void tree_add(Widget *const parent, Widget *const child)
{
    child->parent = parent;
    parent->children[parent->nchildren++] = child;
}

Widget *tree_focused(Widget *w)
{
    for (;;) {
        Widget *next = nullptr;

        for (uint8_t i = w->nchildren; i-- > 0 && !next;) /* a shown modal child, the last first */
            if (w->children[i]->visible && w->children[i]->modal)
                next = w->children[i];

        if (!next && w->nchildren && w->children[w->focus]->visible)
            next = w->children[w->focus];
        if (!next)
            return w;

        w = next;
    }
}

uint8_t tree_index(const Widget *const w)
{
    uint8_t i = 0;

    while (w->parent->children[i] != w)
        i++;

    return i;
}

void tree_focus(Widget *w)
{
    for (; w->parent; w = w->parent)
        w->parent->focus = tree_index(w);
}

bool tree_key(Widget *const root, Session *const S, const uint16_t key)
{
    for (Widget *w = tree_focused(root);; w = w->parent) {
        if (w->ops && w->ops->key && w->ops->key(w, S, key))
            return true;
        if (w == root)
            return false;
    }
}

static bool holds(const Rect r, const uint8_t row, const uint8_t col)
{
    return row >= r.row && row - r.row < r.rows && col >= r.col && col - r.col < r.cols;
}

/* The deepest visible widget under w holding row, col: a shown modal child first. */
static Widget *under(Widget *w, const uint8_t row, const uint8_t col)
{
    for (;;) {
        Widget *next = nullptr;

        for (uint8_t i = w->nchildren; i-- > 0 && !next;)
            if (w->children[i]->visible && w->children[i]->modal)
                next = w->children[i];
        for (uint8_t i = w->nchildren; i-- > 0 && !next;) /* the last drawn is on top */
            if (w->children[i]->visible && holds(w->children[i]->rect, row, col))
                next = w->children[i];

        if (!next)
            return w;

        w = next;
    }
}

bool tree_mouse(Widget *const root, Session *const S, const uint8_t row, const uint8_t col)
{
    for (Widget *w = under(root, row, col);; w = w->parent) {
        if (w->ops && w->ops->mouse && w->ops->mouse(w, S, row, col))
            return true;
        if (w == root)
            return false;
    }
}

void tree_draw(Widget *const root, Session *const S)
{
    if (root->ops && root->ops->draw)
        root->ops->draw(root, S);

    for (uint8_t i = 0; i < root->nchildren; i++)
        if (root->children[i]->visible)
            tree_draw(root->children[i], S);
}
