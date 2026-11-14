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

void tree_draw(Widget *const root, Session *const S)
{
    if (root->ops && root->ops->draw)
        root->ops->draw(root, S);

    for (uint8_t i = 0; i < root->nchildren; i++)
        if (root->children[i]->visible)
            tree_draw(root->children[i], S);
}
