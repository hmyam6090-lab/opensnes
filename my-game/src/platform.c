/**
 * @file platform.c
 * @brief The only file of the game that calls the OpenSNES SDK for video
 *        and input (sound is in sound.c).
 *
 * VRAM layout:
 *   $0000  BG1 2bpp font (96 glyphs) + 6 border tiles (tiles 96..101)
 *   $3800  BG1 32x32 tilemap (the text module's buffer, flushed by the NMI)
 *   $4000  OBJ tiles (res/sprites.png, 4bpp)
 */
#include "platform.h"
#include "../res/sprites.inc"

#define MAX_SLOTS 128

#define OBJ_VRAM    0x4000
#define BOX_VRAM    (96 * 8)       /* word address of tile 96 (2bpp = 8 words) */

/* The text module's RAM copy of the BG1 tilemap. */
extern u8 tilemapBuffer[2048];

/* Six border tiles, 2bpp (plane 0 only = colour 1). */
static const u8 box_tiles[6 * 16] = {
    /* H */  0,0, 0,0, 0,0, 0xFF,0, 0xFF,0, 0xFF,0, 0,0, 0,0,
    /* V */  0x1C,0, 0x1C,0, 0x1C,0, 0x1C,0, 0x1C,0, 0x1C,0, 0x1C,0, 0x1C,0,
    /* TL */ 0,0, 0,0, 0,0, 0x1F,0, 0x1F,0, 0x1F,0, 0x1C,0, 0x1C,0,
    /* TR */ 0,0, 0,0, 0,0, 0xFC,0, 0xFC,0, 0xFC,0, 0x1C,0, 0x1C,0,
    /* BL */ 0x1C,0, 0x1C,0, 0x1C,0, 0x1F,0, 0x1F,0, 0x1F,0, 0,0, 0,0,
    /* BR */ 0x1C,0, 0x1C,0, 0x1C,0, 0xFC,0, 0xFC,0, 0xFC,0, 0,0, 0,0,
};

static u16 pad_pressed;
static u16 pad_held;
static u16 frame_counter;
static u8  tiles_dirty;
static u8  next_slot;         /* first free slot this frame */
static u8  used_slots;        /* slots used last frame */
static u8  fixed_end = 2;     /* first slot after the fixed sprites */
static u8  fixed_used = 2;

/* The 16 arm tiles, built at boot: a 4 px line through the middle of the
 * cell, out to each side named in the tile's mask. */
static u8 arm_tiles[16 * 16];

static void build_arm_tiles(void) {
    u8 m, r, v;
    for (m = 0; m < 16; m++) {
        for (r = 0; r < 8; r++) {
            if (r >= 2 && r <= 5) {
                v = 0x3C;
                if (m & ARM_LEFT)  v |= 0xC0;
                if (m & ARM_RIGHT) v |= 0x03;
            } else if (r < 2) {
                v = (m & ARM_UP) ? 0x3C : 0;
            } else {
                v = (m & ARM_DOWN) ? 0x3C : 0;
            }
            arm_tiles[m * 16 + r * 2] = v;          /* plane 0: colour 1, white */
            arm_tiles[m * 16 + r * 2 + 1] = 0;
        }
    }
}

void plat_init(void) {
    u8 i;

    textModeInit();                     /* Mode 0, BG1 text, font at $0000, map at $3800 */
    dmaCopyVram(box_tiles, BOX_VRAM, sizeof box_tiles);
    build_arm_tiles();
    dmaCopyVram(arm_tiles, TILE_ARM_BASE * 8, sizeof arm_tiles);

    /* BG palettes: colour 0 is the black backdrop, colour 1 of each
     * palette is the glyph colour. */
    setColor(0, RGB(0, 0, 0));
    setColor(TXT_WHITE  * 4 + 1, RGB(31, 31, 31));
    setColor(TXT_YELLOW * 4 + 1, RGB(31, 31, 0));
    setColor(TXT_GRAY   * 4 + 1, RGB(12, 12, 12));
    setColor(TXT_RED    * 4 + 1, RGB(31, 4, 4));
    setColor(TXT_ORANGE * 4 + 1, RGB(31, 16, 2));

    /* Sprites: the converted sheet and its 16-colour palette. */
    dmaCopyVram(sprites_tiles, OBJ_VRAM, (u16)(sprites_tiles_end - sprites_tiles));
    dmaCopyCGram(sprites_pal, OBJ_CGRAM_BASE, 32);
    oamInit(OBJ_SIZE8_L16, OBJ_NAME_BASE(OBJ_VRAM));
    for (i = 0; i < MAX_SLOTS; i++) {
        oamSetSize(i, i == 0 ? OBJ_SMALL : OBJ_LARGE);   /* the SOUL is 8x8 */
        oamHide(i);
    }

    setMainScreen(LAYER_BG1 | LAYER_OBJ);
}

void plat_show(void) {
    WaitForVBlank();
    setScreenOn();
}

void plat_frame(void) {
    if (tiles_dirty) {
        textFlush();                    /* border tiles were written by hand */
        tiles_dirty = 0;
    }
    WaitForVBlank();
    pad_pressed = padPressed(0);
    pad_held = padHeld(0);
    frame_counter++;
}

u16 plat_pressed(void)     { return pad_pressed; }
u16 plat_held(void)        { return pad_held; }
u16 plat_frame_count(void) { return frame_counter; }

void plat_text_color(u8 color) {
    text_config.palette = color;
}

void plat_print(u8 x, u8 y, const char *s) {
    textPrintAt(x, y, s);
}

void plat_putc(u8 x, u8 y, char c) {
    textSetPos(x, y);
    textPutChar(c);
}

void plat_print_u16(u8 x, u8 y, u16 v) {
    textSetPos(x, y);
    textPrintU16(v);
}

void plat_clear(u8 x, u8 y, u8 w, u8 h) {
    textClearRect(x, y, w, h);
}

static void put_tile(u8 x, u8 y, u16 tile) {
    u16 off = ((u16)y * 32 + x) * 2;
    tilemapBuffer[off] = (u8)tile;
    tilemapBuffer[off + 1] = (u8)(tile >> 8);   /* VHOPPPTT: palette in bits 10-12 */
}


void plat_put_tile(u8 x, u8 y, u16 tile) {
    put_tile(x, y, tile);
    tiles_dirty = 1;
}

void plat_box(u8 x0, u8 y0, u8 x1, u8 y1) {
    u8 i;

    textClearRect(x0, y0, (u8)(x1 - x0 + 1), (u8)(y1 - y0 + 1));
    for (i = x0 + 1; i < x1; i++) {
        put_tile(i, y0, TILE_BOX_H);
        put_tile(i, y1, TILE_BOX_H);
    }
    for (i = y0 + 1; i < y1; i++) {
        put_tile(x0, i, TILE_BOX_V);
        put_tile(x1, i, TILE_BOX_V);
    }
    put_tile(x0, y0, TILE_BOX_TL);
    put_tile(x1, y0, TILE_BOX_TR);
    put_tile(x0, y1, TILE_BOX_BL);
    put_tile(x1, y1, TILE_BOX_BR);
    tiles_dirty = 1;
}

void plat_soul(s16 x, s16 y) {
    oamSet(0, (u16)x & 0x1FF, (u16)y, SPR_SOUL_TILE, 0, 3, 0);
}

void plat_soul_tile(s16 x, s16 y, u16 tile) {
    oamSet(0, (u16)x & 0x1FF, (u16)y, tile, 0, 3, 0);
}

void plat_soul_hide(void) {
    oamHide(0);
}

/* Slot 1 is the face, in front of the portrait behind it. */
void plat_face(const SpriteImage *img, s16 x, s16 y) {
    if (!img) {
        oamHide(1);
        return;
    }
    oamSet(1, (u16)x & 0x1FF, (u16)y & 0xFF, img->frame[0].tile, 0, 3, 0);
}

void plat_fixed_begin(void) {
    next_slot = 2;
}

void plat_fixed_end(void) {
    u8 i;
    for (i = next_slot; i < fixed_used; i++)
        oamHide(i);
    fixed_used = next_slot;
    fixed_end = next_slot;
    if (used_slots < fixed_end)
        used_slots = fixed_end;  /* plat_sprites_end hides what the last frame left above */
}

void plat_sprites_begin(void) {
    next_slot = fixed_end;
}

static const MetaspriteStyle piece_style = {
    .baseTile = 0, .basePalette = 0, .size = OBJ_LARGE, .pieceSize = 16, .width = 16, .height = 16,
};

void plat_pieces(const MetaspriteItem *items) {
    if (next_slot < MAX_SLOTS)
        next_slot = (u8)oamDrawMetasprite(next_slot, 0, 0, items, &piece_style, 0);
}

void plat_image(const SpriteImage *img, s16 x, s16 y) {
    if (next_slot < MAX_SLOTS)
        next_slot = (u8)oamDrawMetasprite(next_slot, x, y, img->frame, &img->style, 0);
}

void plat_sprites_end(void) {
    u8 i;
    for (i = next_slot; i < used_slots; i++)
        oamHide(i);
    used_slots = next_slot;
}

u16 plat_rand(void) {
    return rngNext();
}

void plat_seed(u16 seed) {
    rngSeed(seed);
}
