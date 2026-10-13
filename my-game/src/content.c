/**
 * @file content.c
 * @brief The fight's words and numbers. Edit freely.
 *
 * Text conventions: "* " starts a narration line; '\n' forces a line
 * break; the typewriter wraps on spaces. The font is ASCII 32-127.
 */
#include "content.h"

/* ---- Narration (the big box) ------------------------------------------ */

const char *const txt_encounter   = "* Special enemy Temmie appears here to defeat you!!";
const char *const txt_check       = "* TEMMIE - ATK 2 DEF 2\n* RATED TEM OUTTA TEM. Loves to pet cute humans. But you're allergic!";
const char *const txt_talk        = "* You say hello to Temmie.";
const char *const txt_feed        = "* Temmie only wants the Temmie Flakes.";
const char *const txt_feed_prompt = "* Give Temmie the Temmie Flakes?";
const char *const txt_fight_stub  = "* (FIGHT is not implemented.)";
const char *const txt_item_have   = "* You have Temmie Flakes.\n* Use ACT to feed them.";
const char *const txt_item_none   = "* Your pockets are empty.";

const char *const flavor_pool[FLAVOR_COUNT] = {
    "* Temmie is trying to glomp you.",
    "* Temmie forgot her other attack.",
    "* Temmie is doing her hairs.",
    "* Smells like Temmie Flakes.",
    "* Temmie vibrates intensely.",
    "* Temmiy accidentally misspells her own name.",
};

/* ---- Temmie's lines (the speech bubble) ------------------------------- */

const char *const neutral_pool[NEUTRAL_COUNT] = {
    "fhsdhjfdsfjsddshjfsd",
    "hOI!!!!!! i'm tEMMIE!!",
    "awwAwa cute!! (pets u)",
};

const char *const say_no_flakes = "NO!!! so hungr...";
const char *const say_foob      = "FOOB!!!";
const char *const say_win       = "OMG!! humans TOO CUTE";
const char *const say_refused   = "can't blame a BARK for tryin'...";

/* ---- Outcomes (placeholder wording, not from the source text) --------- */

const char *const out_spared = "* YOU WON!\n* You spared Temmie.";
const char *const out_fed    = "* YOU WON!\n* Temmie is full and happy.";
const char *const out_left   = "* YOU WON!\n* Temmie ran off to find food.";
const char *const out_lose   = "* GAME OVER";

/* ---- Menus -------------------------------------------------------------- */

const char *const ui_status      = "YOU   LV 1   HP";
const char *const ui_spare       = "* Spare";
const char *const ui_yes         = "Yes";
const char *const ui_no          = "No";
const char *const ui_press_start = "PRESS START";

const u8 refusal_as_picture = 1;

const char *const menu_labels[MENU_COUNT] = { "FIGHT", "ACT", "ITEM", "MERCY" };
const u8 menu_cols[MENU_COUNT] = { 3, 11, 18, 25 };

const char *const act_labels[ACT_COUNT] = { "* Check", "* Talk", "* Feed Temmie Flakes" };

/* ---- Boxes and attacks ------------------------------------------------ */

const BoxRect text_box = { 2, 13, 29, 22 };

const BoxRect atk_box[ATK_COUNT] = {
    [ATK_TALL]         = { 3, 8, 28, 22 },    /* 192 x 104 px inside */
    [ATK_TALL_ANNOYED] = { 3, 8, 28, 22 },
    [ATK_ARM]          = { 9, 9, 22, 22 },    /* 96 x 96 px inside: a 12 x 12 grid */
};

const u8 neutral_attacks[NEUTRAL_ATTACKS] = { ATK_TALL, ATK_ARM };

/* Relative to the "tall" image (96 x 57): the head is harmless, the body hurts. */
const HitBox tall_boxes[TALL_BOXES] = {
    {  0,  0, 30, 33, 0 },   /* head */
    { 13, 29, 83, 28, 1 },   /* body */
};
const u8 tall_leg_x[TALL_LEGS]     = { 22, 34, 74, 86 };
const u8 tall_leg_phase[TALL_LEGS] = { 0, 24, 8, 40 };
const u8 tall_leg_top = 56;
const u8 tall_cage_x = 57;

const TallParams tall_params[2] = {
    /* normal: you start in the cage between her legs and follow her */
    { .duration = 480, .speed_min = 0x0080, .speed_max = 0x0110, .feint_speed = 0x0120,
      .min_dist = 24, .pause_min = 10, .pause_max = 40, .first_pause = 40, .feint_chance = 64,
      .sway_amp = 5, .sway_idle = 1, .sway_walk = 3, .damage = 3, .grace = 20,
      .spawn_in_front = 0 },
    /* annoyed: you start on the floor in front of her and must climb on top */
    { .duration = 360, .speed_min = 0x00C0, .speed_max = 0x0130, .feint_speed = 0x0140,
      .min_dist = 32, .pause_min = 4, .pause_max = 20, .first_pause = 20, .feint_chance = 96,
      .sway_amp = 7, .sway_idle = 2, .sway_walk = 4, .damage = 4, .grace = 45,
      .spawn_in_front = 1 },
};

const ArmParams arm_params = {
    .duration = 540, .speed = 0x0100, .entry_col = 5, .max_cells = 40,
    .retract_every = 2, .pause = 30, .head_damage = 4, .body_damage = 2,
    .edge_tol = 6, .retract_dist = 48, .center_tol = 8,
};

const s8 sine64[64] = {
       0,    6,   12,   19,   24,   30,   36,   41,   45,   49,   53,   56,   59,   61,   63,   64,
      64,   64,   63,   61,   59,   56,   53,   49,   45,   41,   36,   30,   24,   19,   12,    6,
       0,   -6,  -12,  -19,  -24,  -30,  -36,  -41,  -45,  -49,  -53,  -56,  -59,  -61,  -63,  -64,
     -64,  -64,  -63,  -61,  -59,  -56,  -53,  -49,  -45,  -41,  -36,  -30,  -24,  -19,  -12,   -6,
};
