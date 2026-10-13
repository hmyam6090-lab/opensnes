/**
 * @file attacks.h
 * @brief Temmie's bullet-board attacks. Tunables live in content.c.
 */
#ifndef GAME_ATTACKS_H
#define GAME_ATTACKS_H

#include <snes.h>

/** Inside of the current attack's board, in pixels (right/bottom exclusive). */
extern s16 board_l, board_r, board_t, board_b;

/** Set the board up, place the SOUL. @p kind is an ATK_* id. */
void atk_start(u8 kind);
/** One frame (after the SOUL moved): moves, draws, deals damage.
 *  Call between plat_sprites_begin() and plat_sprites_end().
 *  Returns 1 when the attack is over. */
u8   atk_update(void);

#endif /* GAME_ATTACKS_H */
