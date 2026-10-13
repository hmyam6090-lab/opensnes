/**
 * @file battle.c
 * @brief The Temmie fight: menus, ACT branches, turns, win and lose.
 *
 *   MENU -> ACT -> NARRATE -> ENEMY_TALK -> DODGE -> MENU ...
 *        -> MERCY (once sparable) -> END
 *        -> FIGHT / ITEM stubs -> NARRATE -> MENU
 *   Feed with Flakes -> PROMPT -> yes: END (fed) / no: annoyed attack
 *   HP 0 during DODGE -> END (lost)
 *
 * Sprites are redrawn every frame (platform.c hands out the OAM slots):
 * the state's update draws the attack, then the scene draws Temmie.
 */
#include "battle.h"
#include "attacks.h"
#include "content.h"
#include "platform.h"
#include "sound.h"
#include "textbox.h"
#include "config.h"

/* ---- State (globals so the luna manifests can read them) -------------- */

u8 battle_state;
u8 battle_outcome;
u8 player_hp;
u8 temmie_sparable;
u8 has_flakes;
u16 soul_x;
u16 soul_y;

static u8 invuln;
static u8 hp_dirty;
static u8 menu_cursor;
static u8 sub_cursor;
static const char *flavor;           /* the line shown in the menu box */

/* How Temmie looks above the box. */
#define LOOK_IDLE 0
#define LOOK_SMUG 1
#define LOOK_GONE 2
static u8 look;
static u8 show_smug_text;            /* the "can't blame a BARK" picture */

/* What follows the current narration / speech. */
#define AFTER_MENU    0
#define AFTER_ENEMY   1
#define AFTER_PROMPT  2
#define AFTER_ATTACK  3
#define AFTER_OUTCOME 4

static u8 after_narrate;
static const char *enemy_lines[2];
static u8 enemy_count;
static u8 enemy_index;
static u8 enemy_picture;             /* 1 = the line is the smug picture */
static u8 after_enemy;
static u8 next_attack;
static u8 pending_outcome;

/* Debug: 0 = random attack, else ATK_* + 1. */
static u8 dbg_force;

/* ---- Small helpers ------------------------------------------------------ */

static u8 rand_below(u8 n) {
    return (u8)(plat_rand() % n);
}

static u8 pick_attack(void) {
    if (dbg_force)
        return (u8)(dbg_force - 1);
    return neutral_attacks[rand_below(NEUTRAL_ATTACKS)];
}

static void draw_hp(void) {
    u8 i, hp_color = player_hp > 5 ? TXT_YELLOW : TXT_RED;

    /* Bar: 5 tiles of 4 HP, yellow while some HP is left in the tile. */
    for (i = 0; i < 5; i++) {
        plat_text_color(player_hp > i * 4 ? TXT_YELLOW : TXT_RED);
        plat_putc((u8)(HP_BAR_COL + i), HP_ROW, GLYPH_BLOCK);
    }
    plat_text_color(hp_color);
    plat_print(24, HP_ROW, "  ");
    plat_print_u16(player_hp < 10 ? 25 : 24, HP_ROW, player_hp);
    plat_text_color(TXT_WHITE);
    plat_print(26, HP_ROW, "/");
    plat_print_u16(27, HP_ROW, PLAYER_MAX_HP);
    hp_dirty = 0;
}

static void draw_menu_buttons(u8 highlight) {
    u8 i, color;

    for (i = 0; i < MENU_COUNT; i++) {
        if (i == 3 && !temmie_sparable)
            color = TXT_GRAY;
        else if (highlight && i == menu_cursor)
            color = TXT_YELLOW;
        else
            color = TXT_ORANGE;
        plat_text_color(color);
        plat_print(menu_cols[i], MENU_ROW, menu_labels[i]);
    }
    plat_text_color(TXT_WHITE);
}

static void draw_debug(void) {
#if GAME_DEBUG
    static const char *const atk_names[4] = { "RANDOM ", "TALL   ", "ANNOYED", "ARM    " };
    plat_text_color(TXT_GRAY);
    plat_print(1, DEBUG_ROW, "SEL FLAKES:");
    plat_print(12, DEBUG_ROW, has_flakes ? "Y" : "N");
    plat_print(14, DEBUG_ROW, "L ATK:");
    plat_print(20, DEBUG_ROW, atk_names[dbg_force]);
    plat_print(28, DEBUG_ROW, "R1HP");
    plat_text_color(TXT_WHITE);
#endif
}

/* Temmie above the box: idle with her face vibrating, or smug. The body
 * is redrawn only when the look changes; the face moves every frame. */
static void draw_temmie(void) {
    static u8 drawn = 0xFF;
    u8 key = (u8)(look | (show_smug_text << 4));
    s16 fx = 0, fy = 0;

    if (key != drawn) {
        drawn = key;
        plat_fixed_begin();
        if (look == LOOK_SMUG)
            plat_image(&img_smug, PORTRAIT_X, PORTRAIT_Y);
        else if (look == LOOK_IDLE)
            plat_image(&img_idle, PORTRAIT_X, PORTRAIT_Y);
        if (show_smug_text)
            plat_image(&img_smug_text, SMUG_TEXT_X, SMUG_TEXT_Y);
        plat_fixed_end();
    }
    if (look != LOOK_IDLE) {
        plat_face(0, 0, 0);
        return;
    }
    if (plat_frame_count() & 2) {               /* "Temmie vibrates intensely." */
        fx = (s16)rand_below(3) - 1;
        fy = (s16)rand_below(3) - 1;
    }
    plat_face(&img_face, PORTRAIT_X + FACE_DX + fx, PORTRAIT_Y + FACE_DY + fy);
}

/* Clear the box rows and draw @p b. */
static void show_box(const BoxRect *b) {
    plat_clear(0, BOX_AREA_Y0, 32, BOX_AREA_Y1 - BOX_AREA_Y0 + 1);
    plat_box(b->x0, b->y0, b->x1, b->y1);
}

static void clear_bubble(void) {
    plat_clear(BUBBLE_X0, BUBBLE_Y0, BUBBLE_X1 - BUBBLE_X0 + 1, BUBBLE_Y1 - BUBBLE_Y0 + 1);
    show_smug_text = 0;
}

static void narrate_text(const char *s) {
    tb_start(s, MBOX_TEXT_X, MBOX_TEXT_Y, MBOX_TEXT_W, MBOX_TEXT_H, SFX_BLIP);
}

/* ---- State entries ------------------------------------------------------ */

static void enter_menu(void) {
    battle_state = ST_MENU;
    look = LOOK_IDLE;
    show_box(&text_box);
    narrate_text(flavor);
    draw_menu_buttons(1);
    draw_debug();
}

static void enter_narrate(const char *s, u8 after) {
    battle_state = ST_NARRATE;
    after_narrate = after;
    plat_soul_hide();
    draw_menu_buttons(0);
    show_box(&text_box);
    narrate_text(s);
}

static void enemy_say(const char *a, const char *b, u8 after, u8 picture) {
    enemy_lines[0] = a;
    enemy_lines[1] = b;
    enemy_count = b ? 2 : 1;
    enemy_index = 0;
    enemy_picture = picture;
    after_enemy = after;
}

static void start_enemy_line(void) {
    if (enemy_picture) {
        clear_bubble();
        show_smug_text = 1;
        look = LOOK_SMUG;
        tb_start("", 0, 0, 0, 0, SFX_NONE);     /* nothing to type: done at once */
        sound_sfx(SFX_SMUG);
        return;
    }
    plat_box(BUBBLE_X0, BUBBLE_Y0, BUBBLE_X1, BUBBLE_Y1);
    tb_start(enemy_lines[enemy_index], BUBBLE_TEXT_X, BUBBLE_TEXT_Y,
             BUBBLE_TEXT_W, BUBBLE_TEXT_H, SFX_TEMMIE_VOICE);
}

static void enter_enemy_talk(void) {
    battle_state = ST_ENEMY_TALK;
    plat_soul_hide();
    draw_menu_buttons(0);
    if (after_enemy == AFTER_ATTACK && !enemy_picture)
        show_box(&atk_box[next_attack]);         /* the box becomes the board */
    else
        show_box(&text_box);                     /* an empty box under the speech */
    start_enemy_line();
}

static void enter_outcome(u8 outcome) {
    const char *s = out_lose;

    battle_state = ST_END;
    battle_outcome = outcome;
    clear_bubble();
    plat_soul_hide();
    draw_menu_buttons(0);
    if (outcome == OUT_SPARED) s = out_spared;
    if (outcome == OUT_FED)    s = out_fed;
    if (outcome == OUT_LEFT)   s = out_left;
    look = (outcome == OUT_LOST || outcome == OUT_LEFT) ? LOOK_GONE : LOOK_IDLE;
    if (outcome == OUT_LOST) {
        sound_bgm(BGM_LOSE);
    } else {
        sound_bgm(BGM_WIN);
        sound_sfx(outcome == OUT_SPARED ? SFX_SPARE : SFX_WIN);
    }
    show_box(&text_box);
    narrate_text(s);
    plat_text_color(TXT_GRAY);
    plat_print(MBOX_TEXT_X, text_box.y1 - 1, ui_press_start);
    plat_text_color(TXT_WHITE);
}

/* ---- Death: freeze, crack, shatter, GAME OVER ---------------------------- */

static u8  death_timer;
static s16 death_x, death_y;                  /* where the SOUL died, px */
static s16 shard_x[DEATH_SHARDS], shard_y[DEATH_SHARDS];      /* 12.4 */
static s16 shard_vx[DEATH_SHARDS], shard_vy[DEATH_SHARDS];    /* 12.4 per frame */

static void enter_death(void) {
    battle_state = ST_DEATH;
    death_timer = 0;
    death_x = (s16)(soul_x >> 8);
    death_y = (s16)(soul_y >> 8);
    look = LOOK_GONE;                         /* everything goes but the SOUL */
    show_smug_text = 0;
    plat_clear(0, 0, 32, 28);
    hp_dirty = 0;                             /* the HP line is gone with the rest */
    sound_bgm(BGM_NONE);
}

static void update_death(void) {
    u8 i;

    death_timer++;
    if (death_timer < DEATH_CRACK_AT) {
        plat_soul(death_x, death_y);
    } else if (death_timer < DEATH_SHATTER_AT) {
        if (death_timer == DEATH_CRACK_AT)
            sound_sfx(SFX_HEART_CRACK);
        plat_soul_tile(death_x, death_y, SPR_SOUL_BROKEN_TILE);
    } else {
        if (death_timer == DEATH_SHATTER_AT) {
            plat_soul_hide();
            sound_sfx(SFX_HEART_SHATTER);
            for (i = 0; i < DEATH_SHARDS; i++) {
                shard_x[i] = (s16)((death_x + 2) << 4);
                shard_y[i] = (s16)((death_y + 2) << 4);
                shard_vx[i] = (s16)(rand_below(49) - 24);       /* -1.5 .. 1.5 px */
                shard_vy[i] = (s16)(-16 - rand_below(40));      /* up 1 .. 3.5 px */
            }
        }
        for (i = 0; i < DEATH_SHARDS; i++) {
            shard_x[i] += shard_vx[i];
            shard_y[i] += shard_vy[i];
            shard_vy[i] += DEATH_GRAVITY;
            plat_image((i & 1) ? &img_shard_b : &img_shard_a, shard_x[i] >> 4, shard_y[i] >> 4);
        }
        if (death_timer >= DEATH_GAMEOVER_AT)
            enter_outcome(OUT_LOST);
    }
}

static void enter_dodge(u8 kind) {
    battle_state = ST_DODGE;
    clear_bubble();
    look = kind == ATK_TALL_ANNOYED ? LOOK_SMUG : LOOK_IDLE;
    show_box(&atk_box[kind]);
    atk_start(kind);
}

static void enter_act(void) {
    u8 i;
    battle_state = ST_ACT;
    sub_cursor = 0;
    show_box(&text_box);
    for (i = 0; i < ACT_COUNT; i++)
        plat_print(MBOX_TEXT_X + 2, (u8)(MBOX_TEXT_Y + i * 2), act_labels[i]);
}

static void enter_mercy(void) {
    battle_state = ST_MERCY;
    sub_cursor = 0;
    show_box(&text_box);
    plat_text_color(TXT_YELLOW);
    plat_print(MBOX_TEXT_X + 2, MBOX_TEXT_Y, ui_spare);
    plat_text_color(TXT_WHITE);
}

static void enter_prompt(void) {
    battle_state = ST_PROMPT;
    sub_cursor = 0;
    show_box(&text_box);
    narrate_text(txt_feed_prompt);
    tb_finish();
    plat_print(MBOX_TEXT_X + 4, MBOX_TEXT_Y + 4, ui_yes);
    plat_print(MBOX_TEXT_X + 14, MBOX_TEXT_Y + 4, ui_no);
}

/* ---- Per-state updates -------------------------------------------------- */

static void update_menu(u16 pressed) {
    u8 old = menu_cursor;

    tb_update();
    if (pressed & KEY_LEFT)
        menu_cursor = (u8)((menu_cursor + MENU_COUNT - 1) % MENU_COUNT);
    if (pressed & KEY_RIGHT)
        menu_cursor = (u8)((menu_cursor + 1) % MENU_COUNT);
    if (old != menu_cursor) {
        sound_sfx(SFX_MOVE);
        draw_menu_buttons(1);
    }
    plat_soul((s16)(menu_cols[menu_cursor] - 2) * 8, MENU_ROW * 8);

#if GAME_DEBUG
    if (pressed & KEY_SELECT) {
        has_flakes ^= 1;
        draw_debug();
    }
    if (pressed & KEY_L) {
        dbg_force = (u8)((dbg_force + 1) % (ATK_COUNT + 1));
        draw_debug();
    }
    if (pressed & KEY_R) {
        player_hp = 1;
        hp_dirty = 1;
    }
#endif

    if (!(pressed & KEY_A))
        return;
    switch (menu_cursor) {
    case 0:
        sound_sfx(SFX_SELECT);
        enter_narrate(txt_fight_stub, AFTER_MENU);
        break;
    case 1:
        sound_sfx(SFX_SELECT);
        enter_act();
        break;
    case 2:
        sound_sfx(SFX_SELECT);
        enter_narrate(has_flakes ? txt_item_have : txt_item_none, AFTER_MENU);
        break;
    case 3:
        if (temmie_sparable) {          /* grayed out and inert until then */
            sound_sfx(SFX_SELECT);
            enter_mercy();
        } else {
            sound_sfx(SFX_DENIED);
        }
        break;
    }
}

static void update_act(u16 pressed) {
    u8 old = sub_cursor;

    if (pressed & KEY_UP)
        sub_cursor = (u8)((sub_cursor + ACT_COUNT - 1) % ACT_COUNT);
    if (pressed & KEY_DOWN)
        sub_cursor = (u8)((sub_cursor + 1) % ACT_COUNT);
    if (old != sub_cursor)
        sound_sfx(SFX_MOVE);
    plat_soul(MBOX_TEXT_X * 8, (s16)(MBOX_TEXT_Y + sub_cursor * 2) * 8);

    if (pressed & KEY_B) {
        sound_sfx(SFX_BACK);
        enter_menu();
        tb_finish();
        return;
    }
    if (!(pressed & KEY_A))
        return;
    sound_sfx(SFX_SELECT);
    next_attack = pick_attack();
    switch (sub_cursor) {
    case 0:     /* Check */
        enemy_say(neutral_pool[rand_below(NEUTRAL_COUNT)], 0, AFTER_ATTACK, 0);
        enter_narrate(txt_check, AFTER_ENEMY);
        break;
    case 1:     /* Talk: one hello is enough to make her sparable */
        temmie_sparable = 1;
        enemy_say(neutral_pool[rand_below(NEUTRAL_COUNT)], 0, AFTER_ATTACK, 0);
        enter_narrate(txt_talk, AFTER_ENEMY);
        break;
    case 2:     /* Feed Temmie Flakes */
        if (has_flakes) {
            enter_narrate(txt_feed, AFTER_PROMPT);
        } else {
            enemy_say(say_no_flakes, 0, AFTER_OUTCOME, 0);
            pending_outcome = OUT_LEFT;
            enter_narrate(txt_feed, AFTER_ENEMY);
        }
        break;
    }
}

static void update_mercy(u16 pressed) {
    plat_soul(MBOX_TEXT_X * 8, MBOX_TEXT_Y * 8);
    if (pressed & KEY_B) {
        sound_sfx(SFX_BACK);
        enter_menu();
        tb_finish();
        return;
    }
    if (pressed & KEY_A)
        enter_outcome(OUT_SPARED);
}

static void update_prompt(u16 pressed) {
    if (pressed & (KEY_LEFT | KEY_RIGHT)) {
        sub_cursor ^= 1;
        sound_sfx(SFX_MOVE);
    }
    plat_soul((s16)(MBOX_TEXT_X + 2 + sub_cursor * 10) * 8, (MBOX_TEXT_Y + 4) * 8);
    if (!(pressed & KEY_A))
        return;
    sound_sfx(SFX_SELECT);
    if (sub_cursor == 0) {
        has_flakes = 0;
        temmie_sparable = 1;
        sound_sfx(SFX_EAT);
        enemy_say(say_foob, say_win, AFTER_OUTCOME, 0);
        pending_outcome = OUT_FED;
    } else {
        next_attack = ATK_TALL_ANNOYED;
        enemy_say(say_refused, 0, AFTER_ATTACK, refusal_as_picture);
    }
    enter_enemy_talk();
}

static void update_narrate(u16 pressed) {
    tb_update();
    if (!(pressed & (KEY_A | KEY_B)))
        return;
    if (!tb_done()) {
        tb_finish();
        return;
    }
    if (!(pressed & KEY_A))
        return;
    if (after_narrate == AFTER_MENU) {
        enter_menu();
        tb_finish();                     /* same flavor line, shown at once */
    } else if (after_narrate == AFTER_PROMPT) {
        enter_prompt();
    } else {
        enter_enemy_talk();
    }
}

static void update_enemy_talk(u16 pressed) {
    tb_update();
    if (!(pressed & (KEY_A | KEY_B)))
        return;
    if (!tb_done()) {
        tb_finish();
        return;
    }
    if (!(pressed & KEY_A))
        return;
    if (++enemy_index < enemy_count) {
        start_enemy_line();
        return;
    }
    if (after_enemy == AFTER_OUTCOME)
        enter_outcome(pending_outcome);
    else
        enter_dodge(next_attack);
}

static void update_dodge(u16 held) {
    u16 lo_x = (u16)board_l << 8, hi_x = (u16)(board_r - SOUL_SIZE) << 8;
    u16 lo_y = (u16)board_t << 8, hi_y = (u16)(board_b - SOUL_SIZE) << 8;
    u8 done;

    if (held & KEY_LEFT)  soul_x = soul_x > lo_x + SOUL_SPEED ? soul_x - SOUL_SPEED : lo_x;
    if (held & KEY_RIGHT) soul_x = soul_x + SOUL_SPEED < hi_x ? soul_x + SOUL_SPEED : hi_x;
    if (held & KEY_UP)    soul_y = soul_y > lo_y + SOUL_SPEED ? soul_y - SOUL_SPEED : lo_y;
    if (held & KEY_DOWN)  soul_y = soul_y + SOUL_SPEED < hi_y ? soul_y + SOUL_SPEED : hi_y;

    done = atk_update();                 /* moves, draws, hurts */

    if (invuln)
        invuln--;
    if (invuln & 4)
        plat_soul_hide();                /* flash while invulnerable */
    else
        plat_soul((s16)(soul_x >> 8), (s16)(soul_y >> 8));

    if (player_hp == 0) {
        enter_death();
        return;
    }
#if GAME_DEBUG
    if (plat_pressed() & KEY_START)
        done = 1;                        /* debug: skip the attack */
#endif
    if (done) {
        invuln = 0;
        flavor = flavor_pool[rand_below(FLAVOR_COUNT)];
        enter_menu();
    }
}

static void update_end(u16 pressed) {
    tb_update();
    if (pressed & KEY_START)
        battle_init();
}

/* ---- Public ------------------------------------------------------------- */

void battle_hurt(u8 damage, u8 force) {
    if (invuln && !force)
        return;
    player_hp = player_hp > damage ? (u8)(player_hp - damage) : 0;
    invuln = INVULN_FRAMES;
    hp_dirty = 1;
    sound_sfx(SFX_HURT);
}

void battle_init(void) {
    player_hp = PLAYER_MAX_HP;
    temmie_sparable = 0;
    has_flakes = START_WITH_FLAKES;
    battle_outcome = OUT_NONE;
    invuln = 0;
    menu_cursor = 0;
    show_smug_text = 0;
    flavor = txt_encounter;

    plat_clear(0, 0, 32, 28);
    plat_soul_hide();
    plat_print(2, HP_ROW, ui_status);
    draw_hp();
    sound_bgm(BGM_BATTLE);
    enter_menu();
}

void battle_update(void) {
    u16 pressed = plat_pressed();

    draw_temmie();
    plat_sprites_begin();
    switch (battle_state) {
    case ST_MENU:       update_menu(pressed);          break;
    case ST_ACT:        update_act(pressed);           break;
    case ST_MERCY:      update_mercy(pressed);         break;
    case ST_NARRATE:    update_narrate(pressed);       break;
    case ST_PROMPT:     update_prompt(pressed);        break;
    case ST_ENEMY_TALK: update_enemy_talk(pressed);    break;
    case ST_DODGE:      update_dodge(plat_held());     break;
    case ST_END:        update_end(pressed);           break;
    case ST_DEATH:      update_death();                break;
    }
    plat_sprites_end();

    if (hp_dirty)
        draw_hp();
}
