/**
 * @file platform.h
 * @brief Thin wrapper over the OpenSNES SDK.
 *
 * The battle logic talks only to these functions (and sound.h), never to
 * the SDK directly, so a fix to how the hardware is driven lands in one
 * file.
 */
#ifndef GAME_PLATFORM_H
#define GAME_PLATFORM_H

#include <snes.h>

#include "sprite_layout.h"

/* Text colours (BG1 palettes in Mode 0). */
#define TXT_WHITE  0
#define TXT_YELLOW 1
#define TXT_GRAY   2
#define TXT_RED    3
#define TXT_ORANGE 4

/* Tiles uploaded right after the 96 font glyphs. */
#define TILE_BOX_H   96
#define TILE_BOX_V   97
#define TILE_BOX_TL  98
#define TILE_BOX_TR  99
#define TILE_BOX_BL  100
#define TILE_BOX_BR  101

/* The arm inside the board: 16 BG tiles, one per set of sides the 4 px
 * line reaches (ARM_UP | ARM_RIGHT | ARM_DOWN | ARM_LEFT; 0 = a dot). */
#define TILE_ARM_BASE 102
#define ARM_UP    1
#define ARM_RIGHT 2
#define ARM_DOWN  4
#define ARM_LEFT  8
#define TILE_EMPTY 0     /* the font's space */

/* The font's glyph 127 is a solid block (the HP bar). Printing it goes
 * through the text module, which flushes only what changed. */
#define GLYPH_BLOCK  ((char)127)

/** Video and input setup, with the screen still off. */
void plat_init(void);
/** Turn the screen on (after the slow work of boot, e.g. the sound upload). */
void plat_show(void);
/** Wait for the next frame and latch the pad. */
void plat_frame(void);
u16  plat_pressed(void);
u16  plat_held(void);
u16  plat_frame_count(void);

void plat_text_color(u8 color);
void plat_print(u8 x, u8 y, const char *s);
void plat_putc(u8 x, u8 y, char c);
void plat_print_u16(u8 x, u8 y, u16 v);
void plat_clear(u8 x, u8 y, u8 w, u8 h);
/** Draw a white frame of border tiles from (x0,y0) to (x1,y1) inclusive
 *  and blank its inside. */
void plat_box(u8 x0, u8 y0, u8 x1, u8 y1);

/** Put one BG1 tile by hand (the arm). The text layer is flushed whole on
 *  the next frame, so keep it to a few tiles at a time. */
void plat_put_tile(u8 x, u8 y, u16 tile);

/* Sprites. The SOUL has its own slot (0, drawn above everything). Every
 * other sprite is redrawn each frame between begin and end: slots are
 * handed out in call order, and the ones left over are hidden. */
void plat_soul(s16 x, s16 y);
/** The SOUL's slot with another 8x8 tile (the broken heart). */
void plat_soul_tile(s16 x, s16 y, u16 tile);
void plat_soul_hide(void);
void plat_sprites_begin(void);
/* Fixed sprites (Temmie's portrait): drawn between plat_fixed_begin and
 * plat_fixed_end only when they change, and kept in front of the
 * per-frame ones. Slot 1, in front of them all, is the face. */
void plat_fixed_begin(void);
void plat_fixed_end(void);
/** One-piece image in slot 1, or NULL to hide it. */
void plat_face(const SpriteImage *img, s16 x, s16 y);
/** Draw @p img with its top-left at (x, y) (oamDrawMetasprite: off-screen
 *  pieces are skipped). */
void plat_image(const SpriteImage *img, s16 x, s16 y);
/** Draw a METASPR_TERM-ended list of 16x16 pieces at absolute positions
 *  (a list the caller builds in RAM, e.g. tall Temmie's legs). */
void plat_pieces(const MetaspriteItem *items);
void plat_sprites_end(void);

u16  plat_rand(void);
void plat_seed(u16 seed);

#endif /* GAME_PLATFORM_H */
