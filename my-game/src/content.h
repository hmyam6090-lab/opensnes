/**
 * @file content.h
 * @brief Everything the fight says and every attack tunable, as data.
 *
 * Tuning or rewriting the fight means editing content.c, not battle.c.
 */
#ifndef GAME_CONTENT_H
#define GAME_CONTENT_H

#include <snes.h>

/* ---- Text --------------------------------------------------------------- */

extern const char *const txt_encounter;
extern const char *const txt_check;
extern const char *const txt_talk;
extern const char *const txt_feed;
extern const char *const txt_feed_prompt;
extern const char *const txt_fight_stub;
extern const char *const txt_item_have;
extern const char *const txt_item_none;

extern const char *const say_no_flakes;     /* feed without Flakes */
extern const char *const say_foob;          /* feed accepted */
extern const char *const say_win;           /* after the successful feed */
extern const char *const say_refused;       /* the player keeps the Flakes */

extern const char *const out_spared;
extern const char *const out_fed;
extern const char *const out_left;
extern const char *const out_lose;

#define FLAVOR_COUNT  6
#define NEUTRAL_COUNT 3
extern const char *const flavor_pool[FLAVOR_COUNT];
extern const char *const neutral_pool[NEUTRAL_COUNT];

/* Fixed UI words. */
extern const char *const ui_status;
extern const char *const ui_spare;
extern const char *const ui_yes;
extern const char *const ui_no;
extern const char *const ui_press_start;

/** 1 = the refusal line is shown as Temmie's smug picture-text
 *  (assets/temmie_smug_text_english.png) instead of the speech bubble. */
extern const u8 refusal_as_picture;

/* Menu and sub-menu labels. */
#define MENU_COUNT 4
extern const char *const menu_labels[MENU_COUNT];
extern const u8 menu_cols[MENU_COUNT];

#define ACT_COUNT 3
extern const char *const act_labels[ACT_COUNT];

/* ---- Boxes -------------------------------------------------------------- */

/** A box of BG tiles, border included (tile units). The inside, in pixels,
 *  runs from (x0+1)*8 to x1*8 and from (y0+1)*8 to y1*8. */
typedef struct { u8 x0, y0, x1, y1; } BoxRect;

extern const BoxRect text_box;              /* menu and narration */

/* ---- Attacks ------------------------------------------------------------ */

enum {
    ATK_TALL,           /**< A: tall Temmie walks over you on wobbly legs */
    ATK_TALL_ANNOYED,   /**< A, annoyed: you start in front of her */
    ATK_ARM,            /**< B: her arm chases you like a snake */
    ATK_COUNT
};

extern const BoxRect atk_box[ATK_COUNT];    /* the board of each attack */

/** Attacks the random pick chooses from on a neutral turn. */
#define NEUTRAL_ATTACKS 2
extern const u8 neutral_attacks[NEUTRAL_ATTACKS];

/** Hitbox relative to an image's top-left. */
typedef struct {
    u8 dx, dy, w, h;
    u8 hurts;           /**< 0 = harmless (her head), 1 = deals damage */
} HitBox;

/* Tall Temmie = the "tall" image (head + body) on four legs. */
#define TALL_BOXES 2
extern const HitBox tall_boxes[TALL_BOXES];
#define TALL_LEGS 4
extern const u8 tall_leg_x[TALL_LEGS];      /* leg bar's left x, in the image */
extern const u8 tall_leg_phase[TALL_LEGS];  /* sine offset of each leg's sway */
extern const u8 tall_leg_top;               /* y in the image where legs start */
extern const u8 tall_cage_x;                /* centre of the gap between the inner legs */

/* Tall Temmie walks on her own, left and right inside the board, to make
 * a leg catch the SOUL: stay in the cage between her inner legs by
 * following her. Each walk goes to a random spot at a random speed; some
 * walks are feints that turn around half way. */
typedef struct {
    u16 duration;        /**< frames */
    u16 speed_min;       /**< 8.8 px/frame, slowest walk */
    u16 speed_max;       /**< 8.8 px/frame, fastest walk (the SOUL does 1.375) */
    u16 feint_speed;     /**< 8.8 px/frame after turning around */
    u8  min_dist;        /**< px, shortest walk */
    u8  pause_min;       /**< frames standing between two walks */
    u8  pause_max;
    u8  first_pause;     /**< frames before the first walk */
    u8  feint_chance;    /**< out of 256, chance a walk is a feint */
    u8  sway_amp;        /**< px the feet swing */
    u8  sway_idle;       /**< sine steps per frame standing still */
    u8  sway_walk;       /**< extra sine steps per px walked */
    u8  damage;          /**< touching body or legs */
    u8  grace;           /**< frames before any damage at the start */
    u8  spawn_in_front;  /**< 0 = SOUL starts in the cage under her,
                              1 = on the floor beside her (get to the top!) */
} TallParams;

/* The arm moves cell by cell on the board's 8 px grid, like a snake: at
 * each cell it turns toward the SOUL, never back on itself and never onto
 * itself. Inside the board it is BG tiles (a 4 px line); the hand is
 * temmie_attack_4. */
typedef struct {
    u16 duration;        /**< frames */
    u16 speed;           /**< 8.8 px/frame the hand travels (8 px = one cell) */
    u8  entry_col;       /**< grid column it comes in through (top edge) */
    u8  max_cells;       /**< longest arm, in cells (ARM_MAX at most) */
    u8  retract_every;   /**< frames per cell pulled back */
    u8  pause;           /**< frames between two arms */
    u8  head_damage;     /**< the hand */
    u8  body_damage;     /**< the arm behind it (0 = harmless) */
    u8  edge_tol;        /**< px from an edge that counts as "on the edge" */
    u8  retract_dist;    /**< px of edge tracing that makes the arm pull back */
    u8  center_tol;      /**< px from an edge's middle that counts as "middle" */
} ArmParams;
#define ARM_MAX 64

extern const TallParams tall_params[2];   /* [0] normal, [1] annoyed */
extern const ArmParams arm_params;

/** 64-entry sine, -64..64. */
extern const s8 sine64[64];

#endif /* GAME_CONTENT_H */
