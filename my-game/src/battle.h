/**
 * @file battle.h
 * @brief The fight's state machine and the shared player state.
 */
#ifndef GAME_BATTLE_H
#define GAME_BATTLE_H

#include <snes.h>

/* States (battle_state). Exposed for the luna test manifests. */
enum {
    ST_MENU,         /**< FIGHT / ACT / ITEM / MERCY */
    ST_ACT,          /**< ACT sub-menu */
    ST_MERCY,        /**< MERCY sub-menu */
    ST_NARRATE,      /**< text in the big box, A to go on */
    ST_PROMPT,       /**< feed Yes / No */
    ST_ENEMY_TALK,   /**< Temmie's speech bubble */
    ST_DODGE,        /**< an attack runs */
    ST_END,          /**< won or lost, START restarts */
    ST_DEATH         /**< HP hit 0: the SOUL cracks and shatters */
};

/* Outcomes (battle_outcome). */
enum { OUT_NONE, OUT_SPARED, OUT_FED, OUT_LEFT, OUT_LOST };

extern u8 battle_state;
extern u8 battle_outcome;
extern u8 player_hp;
extern u8 temmie_sparable;
extern u8 has_flakes;

/* The SOUL, shared with the attacks: 8.8 fixed point, top-left of the
 * 8x8 sprite. */
extern u16 soul_x;
extern u16 soul_y;

void battle_init(void);
void battle_update(void);

/** Damage the player. @p force ignores (and refreshes) the i-frames. */
void battle_hurt(u8 damage, u8 force);

#endif /* GAME_BATTLE_H */
