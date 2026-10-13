#!/usr/bin/env python3
"""Pack the art in assets/ into the sprite sheet the game uses (contributor tool).

Run it after changing anything in assets/:

    python3 tools/build_sprites.py

It writes
    res/sprites.png        128 px wide, indexed, 16 colours; `make` converts it
                           with opensnes-sprite (res/sprites.png.toml)
    src/sprite_layout.c/.h every image as a list of 16x16 pieces
                           (offset in the image + OBJ tile number)

The build itself never runs Python: it only converts res/sprites.png.

Each image is cut into 16x16 blocks; empty blocks are dropped and identical
blocks are stored once. Block b of the sheet is OBJ tile (b % 8) * 2 +
(b // 8) * 32. 512 tiles (16 KB of VRAM) is the ceiling.

Colours: alpha < 128 is transparent, dark pixels are black, the rest white.
Requires Pillow.
"""
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
ASSETS = ROOT / "assets"

TRANSPARENT, RED, WHITE, BLACK = 0, 1, 2, 3
PALETTE = [(255, 0, 255), (255, 0, 0), (255, 255, 255), (0, 0, 0)]

# Width of the stretched middle of tall Temmie's body (px).
TALL_STRETCH = 35


def load(name):
    """An asset as a 2D list of colour indices."""
    im = Image.open(ASSETS / name).convert("RGBA")
    return to_idx(im)


def to_idx(im):
    w, h = im.size
    out = []
    for y in range(h):
        row = []
        for x in range(w):
            r, g, b, a = im.getpixel((x, y))
            if a < 128:
                row.append(TRANSPARENT)
            elif r + g + b < 384:
                row.append(BLACK)
            else:
                row.append(WHITE)
        out.append(row)
    return out


def crop(px, x0, y0, x1, y1):
    return [row[x0:x1] for row in px[y0:y1]]


def bbox(px):
    ys = [y for y, row in enumerate(px) if any(row)]
    xs = [x for row in px for x, v in enumerate(row) if v]
    return min(xs), min(ys), max(xs) + 1, max(ys) + 1


def trim(px):
    """Crop to the non-transparent box; returns (pixels, x0, y0)."""
    x0, y0, x1, y1 = bbox(px)
    return crop(px, x0, y0, x1, y1), x0, y0


def from_rows(rows, mapping):
    return [[mapping[c] for c in row] for row in rows]


# ---- The images ------------------------------------------------------------

images = {}   # name -> (pixels, comment)

images["soul"] = (from_rows([
    ".##..##.",
    "########",
    "########",
    "########",
    ".######.",
    "..####..",
    "...##...",
    "........",
], {".": TRANSPARENT, "#": RED}), "the SOUL, 8x8 (drawn as a small sprite)")

# The SOUL breaking (Undertale's death): cracked down the middle, then shards.
images["soul_broken"] = (from_rows([
    ".##..##.",
    "###.####",
    "####.###",
    "###.####",
    ".###.##.",
    "..#.##..",
    "...#....",
    "........",
], {".": TRANSPARENT, "#": RED}), "the SOUL cracked in two, 8x8")
images["shard_a"] = (from_rows(["##..", "###.", ".#.."], {".": TRANSPARENT, "#": RED}),
                     "a shard of the SOUL")
images["shard_b"] = (from_rows([".##.", "##..", "#..."], {".": TRANSPARENT, "#": RED}),
                     "another shard of the SOUL")

idle, _, _ = trim(load("temmie_idle.png"))
images["idle"] = (idle, "temmie_idle.png, cropped to its box")
images["face"] = (load("temmie_idle_face.png"), "temmie_idle_face.png, laid over the idle head")
smug, _, _ = trim(load("temmie_smug.png"))
images["smug"] = (smug, "temmie_smug.png, cropped to its box")

# Tall Temmie's top: attack_1 (head + body start) + stretched body + attack_2 (body end).
a1 = load("temmie_attack_1.png")
a1, _, top = trim(a1)                        # rows 6..62 of the file
a2 = load("temmie_attack_2.png")
a2 = crop(a2, 0, top, 11, top + len(a1))     # same rows, the rounded end
mid_col = [row[-1] for row in a1]
tall = [a1[y] + [mid_col[y]] * TALL_STRETCH + a2[y] for y in range(len(a1))]
images["tall"] = (tall, f"attack_1 + {TALL_STRETCH} px of body + attack_2")

images["leg"] = (from_rows(["o####o"] * 16, {"o": BLACK, "#": WHITE}),
                 "one leg segment, 6x16 (stacked every 8 px)")
images["foot"] = (load("temmie_attack_3.png"), "temmie_attack_3.png: a foot, its bar at x 4..9")
images["arm_line"] = (from_rows(["####"] * 16, {"#": WHITE}),
                      "the arm between Temmie and the board, 4x16 (inside it, the arm is BG tiles)")
images["arm_head"] = (load("temmie_attack_4.png"), "temmie_attack_4.png: the hand at the arm's end")
text, _, _ = trim(load("temmie_smug_text_english.png"))
images["smug_text"] = (text, "temmie_smug_text_english.png, cropped")

# ---- Cut into blocks -------------------------------------------------------

blocks = []          # list of 16x16 pixel blocks
block_ids = {}       # bytes -> block index
pieces = {}          # name -> [(dx, dy, tile)]


def block_index(blk):
    key = bytes(v for row in blk for v in row)
    if key not in block_ids:
        block_ids[key] = len(blocks)
        blocks.append(blk)
    return block_ids[key]


def tile_of(b):
    return (b % 8) * 2 + (b // 8) * 32


for name, (px, _) in images.items():
    h, w = len(px), len(px[0])
    lst = []
    for by in range(0, h, 16):
        for bx in range(0, w, 16):
            blk = [[px[y][x] if y < h and x < w else 0 for x in range(bx, bx + 16)]
                   for y in range(by, by + 16)]
            if not any(v for row in blk for v in row):
                continue
            lst.append((bx, by, tile_of(block_index(blk))))
    pieces[name] = lst

assert pieces["soul"][0][2] == 0, "the SOUL must be tile 0"
assert pieces["soul_broken"][0][0:2] == (0, 0), "the broken SOUL is drawn as one 8x8 sprite"
tiles = len(blocks) * 4
assert tiles <= 512, f"{tiles} tiles: more than the 512 OBJ VRAM holds"

# ---- Write the sheet -------------------------------------------------------

rows = (len(blocks) + 7) // 8
sheet = Image.new("P", (128, rows * 16), 0)
flat = []
for c in PALETTE + [(0, 0, 0)] * (16 - len(PALETTE)):
    flat += c
sheet.putpalette(flat)
for b, blk in enumerate(blocks):
    ox, oy = (b % 8) * 16, (b // 8) * 16
    for y in range(16):
        for x in range(16):
            sheet.putpixel((ox + x, oy + y), blk[y][x])
sheet.save(ROOT / "res" / "sprites.png", transparency=0)

# ---- Write the C layout ----------------------------------------------------

h = ["/* sprite_layout.h - generated by tools/build_sprites.py; do not edit */",
     "#ifndef GAME_SPRITE_LAYOUT_H", "#define GAME_SPRITE_LAYOUT_H", "",
     "#include <snes.h>", "",
     "/** An image as a metasprite of 16x16 pieces, drawn by oamDrawMetasprite(). */",
     "typedef struct { const MetaspriteItem *frame; MetaspriteStyle style; } SpriteImage;", ""]
c = ["/* sprite_layout.c - generated by tools/build_sprites.py; do not edit */",
     '#include "sprite_layout.h"', ""]
for name, (px, comment) in images.items():
    lst = pieces[name]
    up = name.upper()
    w, hh = len(px[0]), len(px)
    h.append(f"/* {comment} */")
    h.append(f"extern const SpriteImage img_{name};")
    h.append(f"#define IMG_{up}_W {w}")
    h.append(f"#define IMG_{up}_H {hh}")
    h.append("")
    c.append(f"static const MetaspriteItem ms_{name}[{len(lst) + 1}] = {{")
    for dx, dy, t in lst:
        c.append(f"    METASPR_ITEM({dx}, {dy}, {t}, OBJ_PRIO(3)),")
    c.append("    METASPR_TERM")
    c.append("};")
    c.append(f"const SpriteImage img_{name} = {{ ms_{name}, "
             f"{{ .baseTile = 0, .basePalette = 0, .size = OBJ_LARGE, .pieceSize = 16, "
             f".width = {(w + 15) // 16 * 16}, .height = {(hh + 15) // 16 * 16} }} }};")
    c.append("")
h.append("#define SPR_SOUL_TILE 0")
h.append(f"#define SPR_SOUL_BROKEN_TILE {pieces['soul_broken'][0][2]}")
h += ["", "#endif", ""]
(ROOT / "src" / "sprite_layout.h").write_text("\n".join(h))
(ROOT / "src" / "sprite_layout.c").write_text("\n".join(c))

print(f"{len(blocks)} blocks, {tiles} tiles of 512; "
      + ", ".join(f"{n} {len(p)}" for n, p in pieces.items()))
