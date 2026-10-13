/**
 * @file textbox.h
 * @brief Typewriter text with word wrap inside a rectangle of tiles.
 */
#ifndef GAME_TEXTBOX_H
#define GAME_TEXTBOX_H

#include <snes.h>

/** Start revealing @p s at (x,y), wrapping at @p w columns, @p h rows max.
 *  @p blip is the sound played while typing (an SFX_* id, or SFX_NONE). */
void tb_start(const char *s, u8 x, u8 y, u8 w, u8 h, u8 blip);
/** Advance one frame. */
void tb_update(void);
/** Reveal the rest at once. */
void tb_finish(void);
u8   tb_done(void);

#endif /* GAME_TEXTBOX_H */
