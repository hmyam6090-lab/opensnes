/**
 * @file config.h
 * @brief Build switches and gameplay tunables for the Temmie battle.
 *
 * Everything a designer is expected to tweak lives here or in content.c.
 * The per-attack tunables are tables in content.c; this file holds the
 * global knobs and the screen layout.
 */
#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

/* ---- Build switches ------------------------------------------------------ */

/** 1 = debug keys and the debug line at the bottom of the screen.
 *  In the menu: SELECT toggles has_flakes, L cycles the forced attack,
 *  R drops HP to 1 (to see the death quickly). During an attack: START
 *  ends it. */
#ifndef GAME_DEBUG
#define GAME_DEBUG 1
#endif

/** 1 = sound effects through the audio module (res/sfx_*.wav). */
#ifndef GAME_AUDIO
#define GAME_AUDIO 1
#endif

/** Does the player start with Temmie Flakes? (debug: SELECT toggles it) */
#define START_WITH_FLAKES 1

/* ---- Player -------------------------------------------------------------- */

#define PLAYER_MAX_HP      20
#define SOUL_SPEED         0x0160   /**< 8.8 px per frame (~1.4 px) */
#define INVULN_FRAMES      60       /**< after a hit; the SOUL flashes */
#define SOUL_SIZE          8        /**< sprite size in px */
#define SOUL_HIT_INSET     2        /**< hitbox = sprite shrunk by this on each side */

/* ---- Death (Undertale's heart break) ------------------------------------ */

#define DEATH_CRACK_AT     40       /**< frames frozen before the SOUL cracks */
#define DEATH_SHATTER_AT   100      /**< frames before it shatters */
#define DEATH_GAMEOVER_AT  200      /**< frames before GAME OVER */
#define DEATH_SHARDS       6
#define DEATH_GRAVITY      3        /**< 12.4 px/frame added to a shard's fall each frame */

/* ---- Text ---------------------------------------------------------------- */

#define TEXT_FRAMES_PER_CHAR 2      /**< typewriter speed */
#define TEXT_BLIP_EVERY      2      /**< blip sound every N characters */

/* ---- Screen layout (tile units, 8 px) ------------------------------------ */

/* Rows 8..22 hold the boxes; each box's rectangle is data (content.c).
 * Whenever the box changes, these rows are cleared first. */
#define BOX_AREA_Y0 8
#define BOX_AREA_Y1 22

/* Text inside the menu / narration box (content.c: text_box). */
#define MBOX_TEXT_X  4
#define MBOX_TEXT_Y  14
#define MBOX_TEXT_W  24
#define MBOX_TEXT_H  8

/* Temmie's speech bubble, right of her portrait. */
#define BUBBLE_X0 20
#define BUBBLE_Y0 0
#define BUBBLE_X1 30
#define BUBBLE_Y1 7
#define BUBBLE_TEXT_X 21
#define BUBBLE_TEXT_Y 1
#define BUBBLE_TEXT_W 9
#define BUBBLE_TEXT_H 6

/* Temmie's portrait (pixels, top-left of temmie_idle's box) and where the
 * idle face sits on it. */
#define PORTRAIT_X 103
#define PORTRAIT_Y 4
#define FACE_DX    11
#define FACE_DY    19
/* The "can't blame a BARK" picture (pixels). */
#define SMUG_TEXT_X 158
#define SMUG_TEXT_Y 0

/* Status line and menu buttons. */
#define HP_ROW       23
#define HP_BAR_COL   18      /**< 5 tiles, 4 HP each */
#define MENU_ROW     25
#define DEBUG_ROW    27

#endif /* GAME_CONFIG_H */
