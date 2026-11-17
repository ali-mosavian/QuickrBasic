/* ld's --wrap for a linker without it (Apple's): included in every runtime file of a display's
 * build, a call to one of these is to __wrap_NAME (rtext.c, the display's tone), and the file that
 * defines it, built with VIEW_REAL_TEXT or VIEW_REAL_TONE, names its own __real_NAME.
 */
#ifdef VIEW_REAL_TEXT
#define VIEW_TEXT(name) __real_##name
#else
#define VIEW_TEXT(name) __wrap_##name
#endif
#ifdef VIEW_REAL_TONE
#define dev_tone __real_dev_tone
#else
#define dev_tone __wrap_dev_tone
#endif
#define gd_set_mode VIEW_TEXT(gd_set_mode)
#define dev_text_size VIEW_TEXT(dev_text_size)
#define dev_text_cursor VIEW_TEXT(dev_text_cursor)
#define dev_text_move VIEW_TEXT(dev_text_move)
#define dev_text_cursor_show VIEW_TEXT(dev_text_cursor_show)
#define dev_text_get VIEW_TEXT(dev_text_get)
#define dev_text_put VIEW_TEXT(dev_text_put)
#define dev_text_write VIEW_TEXT(dev_text_write)
#define dev_text_scroll VIEW_TEXT(dev_text_scroll)
