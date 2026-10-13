/**
 * @file sound.h
 * @brief Sound effect and music hooks.
 *
 * The game calls sound_sfx() / sound_bgm() at every moment that could make
 * a sound, whether a sound is assigned or not; an id with no sample is
 * silent. How to give a hook a sound: see the comment at the top of
 * sound.c (three steps), or README.md, "Sound".
 */
#ifndef GAME_SOUND_H
#define GAME_SOUND_H

#include <snes.h>

/* Every hook the game fires. Comment = where. */
enum {
    SFX_MOVE,           /**< a menu cursor moves */
    SFX_SELECT,         /**< a menu choice is confirmed */
    SFX_BACK,           /**< B leaves a sub-menu */
    SFX_DENIED,         /**< MERCY pressed before Temmie is sparable */
    SFX_BLIP,           /**< narration typing ("* ...") */
    SFX_TEMMIE_VOICE,   /**< Temmie's speech typing */
    SFX_SMUG,           /**< the "can't blame a BARK" picture appears */
    SFX_ATTACK_START,   /**< the box becomes the board, an attack begins */
    SFX_TEMMIE_STEP,    /**< a foot of tall Temmie lands */
    SFX_ARM_STEP,       /**< the arm advances one cell */
    SFX_ARM_RETRACT,    /**< the arm starts pulling back */
    SFX_HURT,           /**< the SOUL takes damage */
    SFX_HEART_CRACK,    /**< death: the SOUL cracks */
    SFX_HEART_SHATTER,  /**< death: the SOUL shatters */
    SFX_SPARE,          /**< Temmie is spared */
    SFX_EAT,            /**< Temmie eats the Flakes */
    SFX_WIN,            /**< the fight is won, any way */
    SFX_COUNT
};

enum {
    BGM_NONE,           /**< silence (death) */
    BGM_BATTLE,         /**< the fight starts */
    BGM_WIN,            /**< won */
    BGM_LOSE,           /**< GAME OVER */
    BGM_COUNT
};

#define SFX_NONE 0xFF   /**< "no sound" where an id is expected */

void sound_init(void);
void sound_sfx(u8 id);
void sound_bgm(u8 id);

#endif /* GAME_SOUND_H */
