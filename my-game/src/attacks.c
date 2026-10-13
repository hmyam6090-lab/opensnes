/**
 * @file attacks.c
 * @brief Attack A: tall Temmie walks left and right on her own, on four
 *        wobbly legs, and the SOUL has to follow her to stay in the cage
 *        between her legs (plus its annoyed variant). Attack B: her arm
 *        comes down into the board and crawls on its 8 px grid like a
 *        snake, toward the SOUL.
 *
 * Positions are 8.8 fixed point (u16, the screen is 0..255 px) or whole
 * pixels (s16); hit tests are AABB in whole pixels. Every frame the attack
 * draws its sprites between the battle's plat_sprites_begin/end; the arm's
 * body is BG tiles.
 */
#include "attacks.h"
#include "battle.h"
#include "content.h"
#include "platform.h"
#include "sound.h"
#include "config.h"

#define FX(px) ((u16)(px) << 8)
#define PX(fx) ((s16)((fx) >> 8))

s16 board_l, board_r, board_t, board_b;

static u8  atk_kind;
static u16 atk_timer;

/* ---- Shared helpers ------------------------------------------------------ */

static u8 overlap(s16 ax, s16 ay, s16 aw, s16 ah, s16 bx, s16 by, s16 bw, s16 bh) {
    return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

#define SOUL_HW (SOUL_SIZE - 2 * SOUL_HIT_INSET)
static s16 soul_hx(void) { return PX(soul_x) + SOUL_HIT_INSET; }
static s16 soul_hy(void) { return PX(soul_y) + SOUL_HIT_INSET; }
static u8 hits_soul(s16 x, s16 y, s16 w, s16 h) {
    return overlap(soul_hx(), soul_hy(), SOUL_HW, SOUL_HW, x, y, w, h);
}

/* Move @p pos toward @p target by at most @p speed (all 8.8). */
static u16 approach(u16 pos, u16 target, u16 speed) {
    if (pos < target)
        return (u16)(target - pos <= speed ? target : pos + speed);
    return (u16)(pos - target <= speed ? target : pos - speed);
}

static s16 abs16(s16 v) { return v < 0 ? -v : v; }

static u16 rand_range(u16 lo, u16 hi) {        /* lo..hi inclusive */
    return (u16)(lo + plat_rand() % (u16)(hi - lo + 1));
}

/* ---- Attack A: tall Temmie ---------------------------------------------- */

static const TallParams *tp;
static u16 tem_x;            /* 8.8, left edge of the "tall" image */
static u16 walk_target;      /* 8.8 */
static u16 walk_speed;       /* 8.8 px/frame */
static u8  walk_pause;       /* frames left standing */
static u16 feint_left;       /* frames until a feint turns around, 0 = none */
static u8  sway_phase;       /* 0..63 */

/* The four legs, rebuilt every frame and drawn in one call. Each leg is a
 * stack of 16 px bars, one every LEG_STEP px, bent by the sway. */
#define LEG_STEP 12
#define LEG_PIECES (TALL_LEGS * 8)
static MetaspriteItem leg_ms[LEG_PIECES + 1];
static u8 leg_n;

static void leg_piece(s16 x, s16 y, const SpriteImage *img) {
    MetaspriteItem *m = &leg_ms[leg_n++];
    m->dx = x;
    m->dy = y;
    m->tile = img->frame[0].tile;
    m->attr = OBJ_PRIO(3);
}

/* She walks only where the whole of her stays inside the board. */
static s16 tall_lo(void) { return board_l; }
static s16 tall_hi(void) { return board_r - IMG_TALL_W; }

/* Pick the next walk: a spot at least min_dist away, a speed, maybe a feint. */
static void tall_pick_walk(void) {
    s16 lo = tall_lo(), hi = tall_hi(), cur = PX(tem_x), t;
    u16 frames;

    t = (s16)rand_range((u16)lo, (u16)hi);
    if (abs16(t - cur) < tp->min_dist) {
        /* Too close: go to the far side instead. */
        if (cur - lo > hi - cur)
            t = lo + (s16)rand_range(0, tp->min_dist / 2);
        else
            t = hi - (s16)rand_range(0, tp->min_dist / 2);
    }
    walk_target = FX(t);
    walk_speed = rand_range(tp->speed_min, tp->speed_max);
    feint_left = 0;
    if ((u8)plat_rand() < tp->feint_chance) {
        frames = (u16)(FX(abs16(t - cur)) / walk_speed);
        feint_left = (u16)(frames / 2 + 1);          /* turn around half way */
    }
}

static void tall_start(u8 annoyed) {
    s16 x = (board_l + board_r) / 2 - tall_cage_x;

    tp = &tall_params[annoyed];
    tem_x = FX(x);
    walk_target = tem_x;
    walk_pause = tp->first_pause;
    feint_left = 0;
    sway_phase = 0;
    if (tp->spawn_in_front) {
        /* On the floor just past her right end: in front of her legs. */
        soul_x = FX(x + IMG_TALL_W + 10);
        soul_y = FX(board_b - SOUL_SIZE - 1);
    } else {
        /* In the cage between her inner legs. */
        soul_x = FX(x + tall_cage_x - SOUL_SIZE / 2);
        soul_y = FX(board_b - SOUL_SIZE - 16);
    }
}

/* One leg: bars whose foot swings by @p sway px. Queues its pieces and
 * returns 1 if any part of it touches the SOUL. */
static u8 tall_leg(s16 x, s16 top, s16 sway) {
    s16 foot_y = board_b - IMG_FOOT_H;
    s16 len = foot_y - top;
    u8 n = (u8)((len + LEG_STEP - 1) / LEG_STEP);
    s16 step, off, y;
    u8 i, near, hit = 0;

    if (n == 0)
        n = 1;
    if (n > 7)
        n = 7;
    /* One box around the whole leg first; segment tests only if inside. */
    near = hits_soul(x + (sway < 0 ? sway - 4 : -4), top, 10 + abs16(sway), board_b - top);
    step = (s16)((sway << 6) / n);       /* 10.6 fixed point */
    off = 0;
    for (i = 0; i < n; i++) {
        y = top + (s16)(i * LEG_STEP);
        if (y > foot_y - 13)
            y = foot_y - 13;             /* the last bar overlaps the foot's own bar */
        leg_piece(x + (off >> 6), y, &img_leg);
        if (near && hits_soul(x + (off >> 6), y, 6, LEG_STEP))
            hit = 1;
        off += step;
    }
    leg_piece(x + sway - 4, foot_y, &img_foot);
    if (near && hits_soul(x + sway - 4, foot_y, 10, IMG_FOOT_H))
        hit = 1;
    return hit;
}

static void tall_update(void) {
    u16 before = tem_x;
    s16 hx, hy, sway, moved, cur;
    u8 i, hit = 0, old_phase;

    /* Her walk: stand, then go somewhere, sometimes turning around. */
    if (walk_pause) {
        if (--walk_pause == 0)
            tall_pick_walk();
    } else {
        if (feint_left && --feint_left == 0) {
            cur = PX(tem_x);
            if (walk_target > tem_x)
                walk_target = FX(rand_range((u16)tall_lo(), (u16)cur));
            else
                walk_target = FX(rand_range((u16)cur, (u16)tall_hi()));
            walk_speed = tp->feint_speed;
        }
        tem_x = approach(tem_x, walk_target, walk_speed);
        if (tem_x == walk_target)
            walk_pause = (u8)rand_range(tp->pause_min, tp->pause_max);
    }
    moved = abs16((s16)(tem_x - before));                  /* 8.8 */
    old_phase = sway_phase;
    sway_phase = (u8)((sway_phase + tp->sway_idle + ((moved * tp->sway_walk) >> 8)) & 63);
    if (moved && ((old_phase ^ sway_phase) & 32))
        sound_sfx(SFX_TEMMIE_STEP);                        /* a foot lands */

    hx = PX(tem_x);
    hy = board_t;
    leg_n = 0;
    for (i = 0; i < TALL_LEGS; i++) {
        sway = (s16)((sine64[(sway_phase + tall_leg_phase[i]) & 63] * (s16)tp->sway_amp) >> 6);
        hit |= tall_leg(hx + tall_leg_x[i], hy + tall_leg_top, sway);
    }
    leg_ms[leg_n].dx = -128;                 /* METASPR_TERM */
    plat_pieces(leg_ms);
    plat_image(&img_tall, hx, hy);

    if (atk_timer < tp->grace)
        return;

    /* The head is harmless; body and legs hurt. */
    for (i = 0; i < TALL_BOXES; i++) {
        const HitBox *b = &tall_boxes[i];
        if (b->hurts && hits_soul(hx + b->dx, hy + b->dy, b->w, b->h))
            hit = 1;
    }
    if (hit)
        battle_hurt(tp->damage, 0);
}

/* ---- Attack B: the arm -------------------------------------------------- */

/* The board's inside is a grid of 8 px cells (the BG tiles). The arm
 * enters at the top of column entry_col, coming down from Temmie, and
 * moves one cell at a time: at every cell the hand turns toward the SOUL,
 * never back on itself and never onto a cell the arm already holds. The
 * arm's body is BG tiles, a 4 px line; the hand is a sprite gliding from
 * cell to cell. The arm pulls back when it is as long as it may be, when
 * it is boxed in, or when the SOUL has traced the board's edges to the
 * middle of one. */

#define CELL 8
#define ARM_EXTEND  0
#define ARM_RETRACT 1
#define ARM_PAUSE   2
#define OCCUPIED    0x10                 /* in grid[]: on top of the line mask */
#define GRID_MAX    (16 * 16)

static const s8 dir_dx[4] = { 0, 1, 0, -1 };     /* down, right, up, left */
static const s8 dir_dy[4] = { 1, 0, -1, 0 };
static const u8 dir_bit[4] = { ARM_DOWN, ARM_RIGHT, ARM_UP, ARM_LEFT };

static u8  grid[GRID_MAX];               /* line mask | OCCUPIED, per cell */
static u8  gcols, grows;                 /* grid size */
static u8  tile_c0, tile_r0;             /* BG tile of cell (0, 0) */
static u8  cell_c[ARM_MAX], cell_r[ARM_MAX];
static u8  cells;
static u8  arm_dir;                      /* 0xFF = none chosen */
static u16 arm_prog;                     /* 8.8 px past the last cell */
static u8  arm_phase;
static u8  arm_wait;
static u16 prev_sx, prev_sy;
static u16 traced;

static void cell_set(u8 c, u8 r, u8 mask) {
    grid[r * gcols + c] = (u8)(mask | OCCUPIED);
    plat_put_tile((u8)(tile_c0 + c), (u8)(tile_r0 + r), TILE_ARM_BASE + mask);
}

static void cell_clear(u8 c, u8 r) {
    grid[r * gcols + c] = 0;
    plat_put_tile((u8)(tile_c0 + c), (u8)(tile_r0 + r), TILE_EMPTY);
}

static u8 cell_mask(u8 c, u8 r) {
    return (u8)(grid[r * gcols + c] & 0x0F);
}

static u8 cell_free(s16 c, s16 r) {
    if (c < 0 || r < 0 || c >= gcols || r >= grows)
        return 0;
    return grid[r * gcols + c] == 0;
}

/* From the last cell: the next direction, or 0xFF when boxed in. */
static u8 arm_turn(void) {
    u8 c = cell_c[cells - 1], r = cell_r[cells - 1];
    s16 dx = PX(soul_x) + SOUL_SIZE / 2 - (board_l + c * CELL + CELL / 2);
    s16 dy = PX(soul_y) + SOUL_SIZE / 2 - (board_t + r * CELL + CELL / 2);
    u8 h = dx >= 0 ? 1 : 3;                 /* toward the SOUL, horizontally */
    u8 v = dy >= 0 ? 0 : 2;                 /* and vertically */
    u8 order[4], i, d;

    if (abs16(dx) >= abs16(dy)) {
        order[0] = h; order[1] = v; order[2] = (u8)(v ^ 2); order[3] = (u8)(h ^ 2);
    } else {
        order[0] = v; order[1] = h; order[2] = (u8)(h ^ 2); order[3] = (u8)(v ^ 2);
    }
    for (i = 0; i < 4; i++) {
        d = order[i];
        if (arm_dir != 0xFF && d == (arm_dir ^ 2))
            continue;                       /* no U-turn */
        if (cell_free((s16)c + dir_dx[d], (s16)r + dir_dy[d]))
            return d;
    }
    return 0xFF;
}

/* Choose the way on from the last cell and draw the line out to its edge. */
static u8 arm_choose(void) {
    u8 d = arm_turn(), c, r;
    if (d == 0xFF)
        return 0;
    arm_dir = d;
    c = cell_c[cells - 1];
    r = cell_r[cells - 1];
    cell_set(c, r, (u8)(cell_mask(c, r) | dir_bit[d]));
    return 1;
}

static void arm_begin_retract(void) {
    u8 c = cell_c[cells - 1], r = cell_r[cells - 1];
    if (arm_dir != 0xFF)
        cell_set(c, r, (u8)(cell_mask(c, r) & ~dir_bit[arm_dir]));   /* drop the stub */
    arm_dir = 0xFF;
    arm_prog = 0;
    arm_wait = 0;
    arm_phase = ARM_RETRACT;
    sound_sfx(SFX_ARM_RETRACT);
}

static void arm_reset(void) {
    cells = 1;
    cell_c[0] = arm_params.entry_col;
    cell_r[0] = 0;
    cell_set(cell_c[0], 0, ARM_UP);         /* joined to the part above the board */
    arm_dir = 0xFF;
    arm_prog = 0;
    arm_phase = ARM_EXTEND;
    traced = 0;
    if (!arm_choose())
        arm_begin_retract();
}

static void arm_start(void) {
    u16 i;

    gcols = (u8)((board_r - board_l) / CELL);
    grows = (u8)((board_b - board_t) / CELL);
    tile_c0 = (u8)(board_l / 8);
    tile_r0 = (u8)(board_t / 8);
    for (i = 0; i < GRID_MAX; i++)
        grid[i] = 0;
    soul_x = FX(board_l + 12);               /* a corner, away from where the arm enters */
    soul_y = FX(board_b - SOUL_SIZE - 12);
    prev_sx = soul_x;
    prev_sy = soul_y;
    arm_reset();
}

/* Edge tracing: 1 when the SOUL has followed the edges for retract_dist
 * px and now sits near the middle of an edge. */
static u8 arm_traced_to_middle(void) {
    const ArmParams *ap = &arm_params;
    s16 cx = PX(soul_x) + SOUL_SIZE / 2;
    s16 cy = PX(soul_y) + SOUL_SIZE / 2;
    s16 tol = ap->edge_tol + SOUL_SIZE / 2;
    u8 on_lr = (cx - board_l < tol) || (board_r - cx < tol);
    u8 on_tb = (cy - board_t < tol) || (board_b - cy < tol);

    if (!on_lr && !on_tb) {
        traced = 0;
        return 0;
    }
    traced += (u16)(abs16(PX(soul_x) - PX(prev_sx)) + abs16(PX(soul_y) - PX(prev_sy)));
    if (traced < ap->retract_dist)
        return 0;
    if (on_lr && abs16(cy - (board_t + board_b) / 2) <= ap->center_tol)
        return 1;
    if (on_tb && abs16(cx - (board_l + board_r) / 2) <= ap->center_tol)
        return 1;
    return 0;
}

/* The arm's body near the SOUL: only the cells the SOUL's hitbox covers. */
static u8 arm_body_hits(void) {
    s16 sx = soul_hx() - board_l, sy = soul_hy() - board_t;
    s16 c0 = sx >> 3, c1 = (sx + SOUL_HW - 1) >> 3;
    s16 r0 = sy >> 3, r1 = (sy + SOUL_HW - 1) >> 3;
    s16 c, r, x0, y0;
    u8 m;

    for (r = r0; r <= r1; r++) {
        for (c = c0; c <= c1; c++) {
            if (c < 0 || r < 0 || c >= gcols || r >= grows || !grid[r * gcols + c])
                continue;
            m = cell_mask((u8)c, (u8)r);
            x0 = board_l + c * CELL;
            y0 = board_t + r * CELL;
            /* The line in this cell: a horizontal and a vertical bar. */
            if (hits_soul(x0 + ((m & ARM_LEFT) ? 0 : 2), y0 + 2,
                          ((m & ARM_RIGHT) ? 8 : 6) - ((m & ARM_LEFT) ? 0 : 2), 4))
                return 1;
            if (hits_soul(x0 + 2, y0 + ((m & ARM_UP) ? 0 : 2),
                          4, ((m & ARM_DOWN) ? 8 : 6) - ((m & ARM_UP) ? 0 : 2)))
                return 1;
        }
    }
    return 0;
}

static void arm_update(void) {
    const ArmParams *ap = &arm_params;
    s16 hx, hy, p, ex, y;
    u8 c, r, gone;

    if (arm_phase == ARM_EXTEND) {
        arm_prog += ap->speed;
        if (arm_prog >= FX(CELL)) {
            /* The hand reached the next cell: the line grows into it. */
            arm_prog -= FX(CELL);
            c = (u8)(cell_c[cells - 1] + dir_dx[arm_dir]);
            r = (u8)(cell_r[cells - 1] + dir_dy[arm_dir]);
            cell_c[cells] = c;
            cell_r[cells] = r;
            cells++;
            cell_set(c, r, dir_bit[arm_dir ^ 2]);
            sound_sfx(SFX_ARM_STEP);
            arm_dir = 0xFF;
            if (cells >= ap->max_cells || cells >= ARM_MAX || !arm_choose())
                arm_begin_retract();
        }
        if (arm_phase == ARM_EXTEND && arm_traced_to_middle())
            arm_begin_retract();
    } else if (arm_phase == ARM_RETRACT) {
        if (++arm_wait >= ap->retract_every) {
            arm_wait = 0;
            cells--;
            c = cell_c[cells];
            r = cell_r[cells];
            cell_clear(c, r);
            if (cells) {
                /* The new last cell no longer reaches toward the removed one. */
                u8 lc = cell_c[cells - 1], lr = cell_r[cells - 1];
                gone = c > lc ? ARM_RIGHT : c < lc ? ARM_LEFT : r > lr ? ARM_DOWN : ARM_UP;
                cell_set(lc, lr, (u8)(cell_mask(lc, lr) & ~gone));
            } else {
                arm_wait = 0;
                arm_phase = ARM_PAUSE;
            }
        }
    } else if (++arm_wait >= ap->pause) {
        arm_reset();
    }
    prev_sx = soul_x;
    prev_sy = soul_y;

    if (arm_phase == ARM_PAUSE)
        return;

    /* The hand, gliding from the last cell toward the next one. */
    p = PX(arm_prog);
    hx = board_l + cell_c[cells - 1] * CELL + CELL / 2;
    hy = board_t + cell_r[cells - 1] * CELL + CELL / 2;
    if (arm_dir != 0xFF) {
        hx += dir_dx[arm_dir] * p;
        hy += dir_dy[arm_dir] * p;
    }
    plat_image(&img_arm_head, hx - IMG_ARM_HEAD_W / 2, hy - IMG_ARM_HEAD_H / 2);

    /* From Temmie down to the board, the arm is sprites. */
    ex = board_l + ap->entry_col * CELL + 2;
    for (y = PORTRAIT_Y + IMG_IDLE_H - 6; y < board_t; y += 16)
        plat_image(&img_arm_line, ex, y > board_t - 16 ? board_t - 16 : y);

    if (hits_soul(hx - 4, hy - 4, 8, 8))
        battle_hurt(ap->head_damage, 0);
    else if (ap->body_damage && arm_body_hits())
        battle_hurt(ap->body_damage, 0);
}

/* ---- Dispatch ----------------------------------------------------------- */

void atk_start(u8 kind) {
    const BoxRect *b = &atk_box[kind];

    board_l = (s16)(b->x0 + 1) * 8;
    board_r = (s16)b->x1 * 8;
    board_t = (s16)(b->y0 + 1) * 8;
    board_b = (s16)b->y1 * 8;
    atk_kind = kind;
    atk_timer = 0;
    sound_sfx(SFX_ATTACK_START);
    if (kind == ATK_ARM)
        arm_start();
    else
        tall_start(kind == ATK_TALL_ANNOYED);
}

u8 atk_update(void) {
    u16 duration;

    atk_timer++;
    if (atk_kind == ATK_ARM) {
        arm_update();
        duration = arm_params.duration;
    } else {
        tall_update();
        duration = tp->duration;
    }
    return atk_timer >= duration;
}
