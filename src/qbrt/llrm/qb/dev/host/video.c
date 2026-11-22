/* The graphics and text screens of a host: the device interface's gd_* and dev_text_* over framebuffers of
   pixel values (video.h), drawn by pixmem.c. */
#include <stdio.h>
#include <pthread.h>
#include <string.h>

#include "dev.h"
#include "gfxdev.h"
#include "pixmem.h"
#include "vgadata.h"
#include "video.h"

enum {
    KIND_TEXT,
    KIND_PLANAR,
    KIND_LINEAR,
    KIND_PACKED4,
    KIND_PACKED2,
    PLANES = 4,
    MODE_320 = 0x0D,
    MODE_640 = 0x0E,
    MODE_350 = 0x10,
    MODE_480 = 0x12,
    MODE_LINEAR = 0x13,
    FONT_ROWS_8 = 8,
    FONT_ROWS_14 = 14,
    PALETTE_REGISTERS = 16,
    MODE_BIT_KEEP_SCREEN = 0x80
};

_Atomic uint8_t host_dirty[HOST_MAX_HEIGHT];

/* A platform that shows the frame (sdl) lets the first one of a new mode reach the screen here. */
/* A weak no-op, not a weak undefined reference: Mach-O's linker rejects the latter. */
__attribute__((weak)) void host_video_mode_changed(void) {}

/* The pages SCREEN can name (mode 0Dh has eight), each taken when first used and kept: the presenter may still read
   one. Drawing goes to `pixels`, the page being drawn, and the presenter reads `shown`. A page is taken before the
   state lock, as dev_take may raise Out of memory. */
enum { MAX_PAGES = 8, TEXT_PAGES = 8, PAGE_BYTES = HOST_MAX_WIDTH * HOST_MAX_HEIGHT };
static uint8_t *page_store[MAX_PAGES];
static uint8_t *pixels;
static _Atomic(const uint8_t *) shown;
static unsigned page_count = 1;
static unsigned width = HOST_TEXT_COLUMNS * HOST_CELL_WIDTH, height = HOST_TEXT_ROWS * HOST_CELL_HEIGHT;
static unsigned mode = HOST_MODE_TEXT, kind = KIND_TEXT, serial;

/* Guards the mode, palette and cursor state the SDL thread reads while the program changes it. */
static pthread_mutex_t state_lock = PTHREAD_MUTEX_INITIALIZER;

void host_state_lock(void)
{
    pthread_mutex_lock(&state_lock);
}

void host_state_unlock(void)
{
    pthread_mutex_unlock(&state_lock);
}

/* The text pages, each taken when first used: the program writes to `cells`, page text_active; text_shown is drawn. */
typedef uint16_t TextRow[HOST_TEXT_COLUMNS];
static TextRow *text_store[TEXT_PAGES];
static TextRow *cells;
static unsigned text_active, text_shown;
static unsigned cursor_row, cursor_column;
static int cursor_visible = 1;

/* The DAC (6 bits a channel; taken with the first mode), the attribute controller's palette registers, and the CGA
   colour select. */
enum { DAC_COLORS = 256 };
static uint8_t (*dac)[3];
static uint8_t attributes[PALETTE_REGISTERS];
static unsigned cga_background, cga_palette = 1;

/* The attribute registers a mode starts with: the colours 0 to 5 and 7 as they are, 6 brown, 8 to 15 bright. */
static const uint8_t default_attributes[PALETTE_REGISTERS] = {0, 1, 2, 3, 4, 5, 0x14, 7,
                                                              0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F};

static void touch(unsigned top, unsigned count)
{
    unsigned y;

    for (y = top; y < top + count && y < HOST_MAX_HEIGHT; y++)
        atomic_store_explicit(&host_dirty[y], 1, memory_order_release);
}

static void touch_all(void)
{
    touch(0, height);
}

static uint8_t *page(unsigned number)
{
    if (!page_store[number])
        page_store[number] = dev_take(PAGE_BYTES);
    return page_store[number];
}

static void text_blank(TextRow *rows)
{
    unsigned row, column;

    for (row = 0; row < HOST_TEXT_ROWS; row++)
        for (column = 0; column < HOST_TEXT_COLUMNS; column++)
            rows[row][column] = 0x0700 | ' ';
}

static TextRow *text_page(unsigned number)
{
    if (!text_store[number]) {
        TextRow *const rows = dev_take(HOST_TEXT_ROWS * sizeof *rows);

        text_blank(rows);
        text_store[number] = rows;
    }
    return text_store[number];
}

static uint8_t *pixel_at(unsigned x, unsigned y)
{
    return pixels + (size_t)y * width + x;
}

unsigned host_width(void)
{
    return width;
}

unsigned host_height(void)
{
    return height;
}

unsigned host_mode(void)
{
    return mode;
}

const uint8_t *host_pixels(void)
{
    return atomic_load_explicit(&shown, memory_order_acquire);
}

unsigned host_mode_serial(void)
{
    return serial;
}

void host_cursor(unsigned *row, unsigned *column, int *visible)
{
    *row = cursor_row;
    *column = cursor_column;
    *visible = cursor_visible;
}

static uint8_t eight_bits(uint8_t six)
{
    return (uint8_t)(six << 2 | six >> 4);
}

void host_colors(uint8_t rgb[256][3])
{
    unsigned value, channel;
    uint8_t six[256][3];

    memset(six, 0, sizeof six);
    switch (kind) {
    case KIND_LINEAR:
        memcpy(six, dac, sizeof six);
        break;
    case KIND_PACKED4:
    case KIND_PACKED2: {
        /* the colour select: the background, and the palette of 3 colours (green red brown, cyan magenta white) */
        static const uint8_t palettes[2][3] = {{2, 4, 6}, {3, 5, 7}};
        unsigned at;

        memcpy(six[0], dac[default_attributes[kind == KIND_PACKED4 ? cga_background : 0]], 3);
        if (kind == KIND_PACKED2) {
            memcpy(six[1], dac[default_attributes[15]], 3);
            break;
        }
        for (at = 0; at < 3; at++)
            memcpy(six[1 + at], dac[default_attributes[palettes[cga_palette][at]]], 3);
        break;
    }
    default:
        for (value = 0; value < PALETTE_REGISTERS; value++)
            memcpy(six[value], dac[attributes[value] & 0xFF], 3);
    }
    for (value = 0; value < 256; value++)
        for (channel = 0; channel < 3; channel++)
            rgb[value][channel] = eight_bits(six[value][channel]);
}

/* The state a mode starts with. */
static void reset_palette(void)
{
    memcpy(dac, qb_vga_palette, DAC_COLORS * sizeof *dac);
    if (kind != KIND_LINEAR)
        memcpy(dac, qb_ega_palette, sizeof qb_ega_palette);
    memcpy(attributes, default_attributes, sizeof attributes);
    cga_background = 0;
    cga_palette = 1;
}

static void draw_cell(unsigned row, unsigned column)
{
    unsigned value = text_store[text_shown][row][column], character = value & 0xFF, foreground = value >> 8 & 0x0F,
             background = value >> 12 & 0x07, line, bit;
    const uint8_t *glyph = qb_vga_font_16 + character * HOST_CELL_HEIGHT;

    for (line = 0; line < HOST_CELL_HEIGHT; line++) {
        uint8_t *at = pixel_at(column * HOST_CELL_WIDTH, row * HOST_CELL_HEIGHT + line);

        for (bit = 0; bit < HOST_CELL_WIDTH; bit++)
            at[bit] = (uint8_t)(glyph[line] & 0x80 >> bit ? foreground : background);
    }
    touch(row * HOST_CELL_HEIGHT, HOST_CELL_HEIGHT);
}

static unsigned pm_kind(void)
{
    return kind == KIND_LINEAR    ? PM_LINEAR
           : kind == KIND_PACKED4 ? PM_PACKED4
           : kind == KIND_PACKED2 ? PM_PACKED2
                                  : PM_PLANAR;
}

static void draw_on(unsigned page)
{
    pixels = page_store[page];
    pm_use(pixels, width, height, pm_kind(), touch);
}

static void draw_rows(unsigned top, unsigned bottom)
{
    unsigned row, column;

    for (row = top; row <= bottom; row++)
        for (column = 0; column < HOST_TEXT_COLUMNS; column++)
            draw_cell(row, column);
}

void host_video_reset(void)
{
    gd_set_mode(HOST_MODE_TEXT);
}

int gd_set_mode(unsigned requested)
{
    unsigned number = requested & ~(unsigned)MODE_BIT_KEEP_SCREEN;
    unsigned new_kind, new_width, new_height, at;
    uint8_t (*palette)[3];

    switch (number) {
    case HOST_MODE_TEXT:
        new_kind = KIND_TEXT, new_width = HOST_TEXT_COLUMNS * HOST_CELL_WIDTH,
        new_height = HOST_TEXT_ROWS * HOST_CELL_HEIGHT;
        break;
    case HOST_MODE_CGA4:
        new_kind = KIND_PACKED4, new_width = 320, new_height = 200;
        break;
    case HOST_MODE_CGA2:
        new_kind = KIND_PACKED2, new_width = 640, new_height = 200;
        break;
    case MODE_320:
        new_kind = KIND_PLANAR, new_width = 320, new_height = 200;
        break;
    case MODE_640:
        new_kind = KIND_PLANAR, new_width = 640, new_height = 200;
        break;
    case MODE_350:
        new_kind = KIND_PLANAR, new_width = 640, new_height = 350;
        break;
    case MODE_480:
        new_kind = KIND_PLANAR, new_width = 640, new_height = 480;
        break;
    case MODE_LINEAR:
        new_kind = KIND_LINEAR, new_width = 320, new_height = 200;
        break;
    default:
        return 0;
    }
    palette = dac ? dac : dev_take(DAC_COLORS * sizeof *dac);
    page(0);
    text_page(0);
    host_state_lock();
    dac = palette;
    mode = number;
    kind = new_kind;
    width = new_width;
    height = new_height;
    reset_palette();
    for (at = 0; at < MAX_PAGES; at++)
        if (page_store[at])
            memset(page_store[at], 0, (size_t)width * height);
    page_count = number == MODE_320 ? 8 : number == MODE_640 ? 4 : 2;
    draw_on(0);
    atomic_store_explicit(&shown, page_store[0], memory_order_release);
    if (kind == KIND_TEXT) {
        for (at = 0; at < TEXT_PAGES; at++)
            if (text_store[at])
                text_blank(text_store[at]);
        text_active = text_shown = 0;
        cursor_row = cursor_column = 0;
        cursor_visible = 1;
    }
    cells = text_store[text_active];
    serial++;
    touch_all();
    host_state_unlock();
    host_video_mode_changed();
    return 1;
}

int gd_solid_box(unsigned x, unsigned y, unsigned count, unsigned rows, unsigned color)
{
    return 0;
}

int gd_set_pages(unsigned active, unsigned visible)
{
    if (active >= page_count || visible >= page_count)
        return 0;
    page(active);
    page(visible);
    host_state_lock();
    draw_on(active);
    atomic_store_explicit(&shown, page_store[visible], memory_order_release);
    touch_all();
    host_state_unlock();
    return 1;
}

int gd_copy_page(unsigned from, unsigned to)
{
    if (from >= page_count || to >= page_count)
        return 0;
    memcpy(page(to), page(from), (size_t)width * height);
    touch_all();
    return 1;
}

void gd_palette(unsigned index, unsigned color)
{
    host_state_lock();
    if (kind == KIND_PLANAR || kind == KIND_TEXT) {
        attributes[index % PALETTE_REGISTERS] = (uint8_t)(color & 0x3F);
        touch_all();
    }
    host_state_unlock();
}

void gd_cga_color(unsigned background, unsigned palette)
{
    host_state_lock();
    cga_background = background & 15;
    cga_palette = palette & 1;
    touch_all();
    host_state_unlock();
}

void gd_palette_mix(unsigned index, unsigned red, unsigned green, unsigned blue)
{
    uint8_t *entry = dac[index & 0xFF];

    host_state_lock();
    entry[0] = (uint8_t)(red & 0x3F);
    entry[1] = (uint8_t)(green & 0x3F);
    entry[2] = (uint8_t)(blue & 0x3F);
    touch_all();
    host_state_unlock();
}

void gd_plot(unsigned x, unsigned y, unsigned color, unsigned operation)
{
    pm_plot(x, y, color, operation);
}

unsigned gd_read(unsigned x, unsigned y)
{
    return pm_read(x, y);
}

int gd_search(int x, int last, unsigned y, unsigned c1, unsigned c2, int match)
{
    return pm_search(x, last, y, c1, c2, match);
}

int gd_run(int x, int last, unsigned y, unsigned edge, unsigned paint, int limit, int *left, int *right)
{
    return pm_run(x, last, y, edge, paint, limit, left, right);
}

void gd_move_rows(unsigned to, unsigned from, unsigned count)
{
    pm_move_rows(to, from, count);
}

void gd_glyph(unsigned x, unsigned y, const u8 *bits, unsigned rows, unsigned foreground)
{
    pm_glyph(x, y, bits, rows, foreground);
}

const u8 *gd_font(unsigned rows)
{
    return rows == FONT_ROWS_14 ? qb_vga_font_14 : rows == FONT_ROWS_8 ? qb_vga_font_8 : qb_vga_font_16;
}

void gd_dots_begin(const GdFill *fill)
{
    (void)fill;
}

void gd_dots_end(const GdFill *fill)
{
    (void)fill;
}

void gd_fill_select(GdFill *fill, unsigned color, unsigned operation)
{
    pm_fill_select(fill, color, operation);
}

void gd_get(unsigned x, unsigned y, unsigned box_width, unsigned rows, u8 *out)
{
    pm_get(x, y, box_width, rows, out);
}

void gd_put(unsigned x, unsigned y, unsigned box_width, unsigned rows, const u8 *in, unsigned operation,
            unsigned invert)
{
    pm_put(x, y, box_width, rows, in, operation, invert);
}

int gd_put_step(unsigned x, unsigned y, unsigned width, unsigned rows, const u8 *in, unsigned step,
                unsigned operation, unsigned invert)
{
    return 0;
}

/* The text screen. */
unsigned dev_text_size(void)
{
    return HOST_TEXT_ROWS << 8 | HOST_TEXT_COLUMNS;
}

unsigned dev_text_cursor(void)
{
    return cursor_row << 8 | cursor_column;
}

void dev_text_move(unsigned row, unsigned column)
{
    host_state_lock();
    cursor_row = row < HOST_TEXT_ROWS ? row : HOST_TEXT_ROWS - 1;
    cursor_column = column < HOST_TEXT_COLUMNS ? column : HOST_TEXT_COLUMNS - 1;
    host_state_unlock();
}

void dev_text_cursor_show(int visible)
{
    host_state_lock();
    cursor_visible = visible != 0;
    host_state_unlock();
}

unsigned dev_text_get(unsigned row, unsigned column)
{
    return row < HOST_TEXT_ROWS && column < HOST_TEXT_COLUMNS ? cells[row][column] : 0;
}

void dev_text_put(unsigned row, unsigned column, unsigned character, unsigned attribute)
{
    if (row < HOST_TEXT_ROWS && column < HOST_TEXT_COLUMNS) {
        cells[row][column] = (uint16_t)(attribute << 8 | (character & 0xFF));
        if (kind == KIND_TEXT && text_active == text_shown)
            draw_cell(row, column);
    }
}

void dev_text_write(unsigned row, unsigned column, const char *text, unsigned count, unsigned attribute)
{
    while (count--)
        dev_text_put(row, column++, (unsigned char)*text++, attribute);
}

void dev_text_scroll(unsigned top, unsigned bottom, unsigned lines, unsigned attribute)
{
    unsigned rows = bottom - top + 1, row, column;

    if (lines == 0 || lines >= rows)
        lines = rows;
    memmove(cells[top], cells[top + lines], (size_t)(rows - lines) * sizeof cells[0]);
    for (row = bottom + 1 - lines; row <= bottom; row++)
        for (column = 0; column < HOST_TEXT_COLUMNS; column++)
            cells[row][column] = (uint16_t)(attribute << 8 | ' ');
    if (kind == KIND_TEXT && text_active == text_shown)
        draw_rows(top, bottom);
}

void dev_text_pages_reset(void)
{
    dev_text_pages(0, 0);
}

int dev_text_pages(unsigned active, unsigned visible)
{
    if (active >= TEXT_PAGES || visible >= TEXT_PAGES)
        return 0;
    text_page(visible);
    cells = text_page(text_active = active);
    if (visible != text_shown) {
        text_shown = visible;
        if (kind == KIND_TEXT)
            draw_rows(0, HOST_TEXT_ROWS - 1);
    }
    return 1;
}

int dev_text_copy(unsigned from, unsigned to)
{
    if (from >= TEXT_PAGES || to >= TEXT_PAGES)
        return 0;
    memcpy(text_page(to), text_page(from), HOST_TEXT_ROWS * sizeof *cells);
    if (to == text_shown && kind == KIND_TEXT)
        draw_rows(0, HOST_TEXT_ROWS - 1);
    return 1;
}

void host_text_row(unsigned row, char out[HOST_TEXT_COLUMNS + 1])
{
    unsigned column;

    for (column = 0; column < HOST_TEXT_COLUMNS; column++)
        out[column] = (char)(text_store[text_shown][row][column] & 0xFF);
    out[HOST_TEXT_COLUMNS] = 0;
}

int host_dump_ppm(const char *path)
{
    FILE *file = fopen(path, "wb");
    uint8_t rgb[256][3];
    unsigned at;

    if (!file)
        return -1;
    host_colors(rgb);
    fprintf(file, "P6\n%u %u\n255\n", width, height);
    for (at = 0; at < width * height; at++)
        fwrite(rgb[host_pixels()[at]], 1, 3, file);
    return fclose(file) ? -1 : 0;
}

/* The DAC's ports, as a program reaches them with OUT and INP: 3C7 the index a read starts at, 3C8 the one a write
   starts at, 3C9 the data, three 6-bit components a colour in turn (red, green, blue), the index moving on after the
   third. Any other port reads as 0xFF and ignores a write. */
enum { PORT_DAC_READ_INDEX = 0x3C7, PORT_DAC_WRITE_INDEX = 0x3C8, PORT_DAC_DATA = 0x3C9 };

static unsigned dac_read_index, dac_write_index, dac_read_part, dac_write_part;
static uint8_t dac_written[3];

void qb_port_out(unsigned port, unsigned value)
{
    port &= 0xFFFF;
    value &= 0xFF;
    host_state_lock();
    if (port == PORT_DAC_READ_INDEX) {
        dac_read_index = value;
        dac_read_part = 0;
    } else if (port == PORT_DAC_WRITE_INDEX) {
        dac_write_index = value;
        dac_write_part = 0;
    } else if (port == PORT_DAC_DATA) {
        dac_written[dac_write_part++] = (uint8_t)(value & 0x3F);
        if (dac_write_part == 3) {
            memcpy(dac[dac_write_index], dac_written, 3);
            dac_write_index = (dac_write_index + 1) & 0xFF;
            dac_write_part = 0;
            touch_all();
        }
    }
    host_state_unlock();
}

unsigned qb_port_in(unsigned port)
{
    unsigned value = 0xFF;

    port &= 0xFFFF;
    host_state_lock();
    if (port == PORT_DAC_READ_INDEX) {
        value = 3;
    } else if (port == PORT_DAC_WRITE_INDEX) {
        value = dac_write_index;
    } else if (port == PORT_DAC_DATA) {
        value = dac[dac_read_index][dac_read_part++];
        if (dac_read_part == 3) {
            dac_read_index = (dac_read_index + 1) & 0xFF;
            dac_read_part = 0;
        }
    }
    host_state_unlock();
    return value;
}
