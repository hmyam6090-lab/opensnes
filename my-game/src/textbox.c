/**
 * @file textbox.c
 * @brief Typewriter text: one character every TEXT_FRAMES_PER_CHAR frames.
 *
 * Wrapping is decided at each space: if the next word does not fit on the
 * current line, the space becomes a line break. A word longer than the
 * line is cut where the line ends.
 */
#include "textbox.h"
#include "platform.h"
#include "sound.h"
#include "config.h"

static const char *tb_text;
static u16 tb_pos;
static u8 tb_x0, tb_y0, tb_w, tb_h;
static u8 tb_cx, tb_cy;
static u8 tb_timer;
static u8 tb_count;
static u8 tb_blip;                 /* sound id, SFX_NONE = silent */
static u8 tb_fresh;               /* nothing drawn yet on this line */
static u8 tb_indent;              /* wrapped lines of a "* " paragraph hang under its text */
static u8 tb_finished = 1;

void tb_start(const char *s, u8 x, u8 y, u8 w, u8 h, u8 blip) {
    tb_text = s;
    tb_pos = 0;
    tb_x0 = x;
    tb_y0 = y;
    tb_w = w;
    tb_h = h;
    tb_cx = 0;
    tb_cy = 0;
    tb_timer = 0;
    tb_count = 0;
    tb_blip = blip;
    tb_indent = 0;
    tb_fresh = 1;
    tb_finished = (s[0] == 0);
    plat_text_color(TXT_WHITE);
    plat_clear(x, y, w, h);
}

static u8 word_len(u16 at) {
    u8 n = 0;
    char c;
    while ((c = tb_text[at + n]) != 0 && c != ' ' && c != '\n')
        n++;
    return n;
}

static void newline(void) {
    tb_cx = tb_indent;
    tb_cy++;
    tb_fresh = 1;
}

/* Emit one character; returns 1 when a visible glyph was drawn. */
static u8 step(void) {
    char c = tb_text[tb_pos];

    if (c == 0) {
        tb_finished = 1;
        return 0;
    }
    tb_pos++;

    if (c == '\n') {
        tb_indent = 0;
        newline();
        return 0;
    }
    if (c == '*' && tb_cx == 0)
        tb_indent = 2;
    if (c == ' ') {
        if (tb_fresh)
            return 0;                       /* no leading space on a wrapped line */
        if (tb_cx + 1 + word_len(tb_pos) > tb_w) {
            newline();
            return 0;
        }
    }
    if (tb_cx >= tb_w)
        newline();
    if (tb_cy >= tb_h) {
        tb_finished = 1;                    /* out of room: stop, never overflow */
        return 0;
    }
    plat_putc((u8)(tb_x0 + tb_cx), (u8)(tb_y0 + tb_cy), c);
    tb_cx++;
    tb_fresh = 0;
    return c != ' ';
}

void tb_update(void) {
    if (tb_finished)
        return;
    if (++tb_timer < TEXT_FRAMES_PER_CHAR)
        return;
    tb_timer = 0;
    while (!tb_finished && !step())
        ;                                   /* skip breaks and spaces: one glyph per tick */
    if (tb_blip != SFX_NONE && ++tb_count >= TEXT_BLIP_EVERY) {
        tb_count = 0;
        sound_sfx(tb_blip);
    }
    if (tb_text[tb_pos] == 0)
        tb_finished = 1;
}

void tb_finish(void) {
    while (!tb_finished)
        step();
}

u8 tb_done(void) {
    return tb_finished;
}
