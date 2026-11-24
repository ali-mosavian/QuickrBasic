/* Help: F1's keyword help, the help window, the help box. How it works: help.c. */
#ifndef UI_HELP_H
#define UI_HELP_H
#include "ui/ui.h"

/* The help file found beside the program and opened; its status lines are uimn.help_text. */
void help_init(void);
/* The help file at path opened in its place: false none there. */
bool help_open(const char *path);
/* The help window's widget, a child of the workspace; and the box's, modal, of the screen. */
void help_build(Widget *workspace, Widget *screen);
/* F1: the keyword under the cursor's topic in the help window; in it, the link chosen. */
void help_f1(Session *S);
/* Shift+F1, the Help menu's Using Help, Contents and Index. */
void help_using(Session *S);
void help_contents(Session *S);
void help_index(Session *S);
/* Alt+F1, Ctrl+F1, Shift+Ctrl+F1 anywhere: back, the next topic, the one before. */
void help_back(Session *S);
void help_next(Session *S, bool previous);
/* The topic with key shown in the help box, over whatever is on the screen: false (a beep) if none. */
bool help_box(Session *S, const char *key, uint32_t len);
/* There is a topic with key. */
bool help_has(const char *key, uint32_t len);
/* The keyword under the cursor, the Help menu's Topic: item's; its length, 0 none. */
uint16_t help_keyword(char *out, uint16_t max);
/* The cursor is on a word the help has a topic for. */
bool help_topic_exists(Session *S);
/* The help window closed (Esc), the editor's rows back. */
void help_close(void);
/* The help window's status bar keys. */
extern const StatusKeys help_keys;
#endif
