#!/usr/bin/env python3
"""Add WAV sound effects to a SNESGSS driver export (contributor tool).

Only one program can drive the SNES sound chip, and the music runs on the
SNESGSS driver, so the sound effects have to be SNESGSS sound effects too.
This script builds them from WAV files instead of the SNESGSS editor:

    python3 tools/gss_add_sfx.py

Run it after changing res/gss/sfx.toml, a WAV it names, or after copying a
new spc700.bin from SNESGSS. The build never runs Python: it includes the
files this writes.

  reads   res/gss/spc700.bin     the driver as SNESGSS exported it
          res/gss/sfx.toml       which WAVs, in which order
          res/gss/music_*.bin    only to check that the songs still fit
  writes  res/gss/spc700_sfx.bin the driver with the new instruments and
                                 effects (same format, uploaded at $0200)
          res/gss/sfx_bank_N.bin the samples and effect data, in pieces of
                                 at most 16 KB (a ROM bank holds 32 KB, and an
                                 upload cannot cross from one bank to the next)
          gss_sfx_data.asm       the .incbin of each piece
          src/gss_sfx.h          GSS_SFX_<NAME> numbers, and where each piece
                                 goes in APU RAM, for src/sound.c

How (from SNESGSS's snes/spc700.asm): the header at $0208 points to the
instruments' ADSR list (2 bytes per instrument n, at ptr + 2n), $020A to the
effect table (count, then a pointer per effect), $020C to the music. An
instrument n plays BRR sample n of the directory at $0B00 (4 bytes per
entry: start, loop; entries 0-8 are the driver's streamer). An effect is
the same byte code as a song channel: a channel count, a pointer per
channel, then FE <instrument>, <note 150+k>, and a delay that jumps onto
itself (FF <addr>) so the channel idles once the sample has played.

The directory grows by one entry per new instrument, so the samples that
followed it move up to the bank, with the new ones. The music stays where
SNESGSS put it ($020C), so the songs need no change.
"""
import math
import struct
import subprocess
import sys
import tempfile
import tomllib
import wave
from fractions import Fraction
from pathlib import Path

import numpy as np
from scipy.signal import resample_poly

ROOT = Path(__file__).resolve().parent.parent
GSS = ROOT / "res" / "gss"
ENCODER = ROOT.parent / "bin" / "opensnes-sample"

DIR = 0x0B00                 # DSP DIR = sampleDirAddr / 256, baked in the driver
FIRST_INSTRUMENT = 9         # 0-8 are the streamer's
DRIVER = 0x0200


def fail(msg):
    sys.exit(f"gss_add_sfx: {msg}")


def word(m, a):
    return m[a] | m[a + 1] << 8


def put_word(m, a, v):
    m[a] = v & 0xFF
    m[a + 1] = (v >> 8) & 0xFF


def brr_length(m, start):
    """Bytes of the BRR sample at start, up to its end block."""
    a = start
    while not m[a] & 1:
        a += 9
        if a >= 0x10000:
            fail(f"sample at ${start:04X} has no end block")
    return a + 9 - start


def note_table(m):
    i = bytes(m).find(bytes([0x42, 0, 0x46, 0, 0x4A, 0, 0x4F, 0]))
    if i < 0:
        fail("no note period table in spc700.bin (not a SNESGSS driver?)")
    return [word(m, i + 2 * k) for k in range(96)]


def load_wav(path, trim_db):
    with wave.open(str(path)) as w:
        ch, width, rate = w.getnchannels(), w.getsampwidth(), w.getframerate()
        raw = w.readframes(w.getnframes())
    if width != 2:
        fail(f"{path.name}: 16-bit PCM only")
    x = np.frombuffer(raw, dtype="<i2").astype(np.float64).reshape(-1, ch).mean(axis=1)
    # Trim the silence around the sound, with 2 ms kept on each side.
    peak = np.max(np.abs(x)) or 1.0
    loud = np.nonzero(np.abs(x) > peak * 10 ** (trim_db / 20))[0]
    pad = int(rate * 0.002)
    x = x[max(0, loud[0] - pad): loud[-1] + pad + 1]
    return x, rate


def encode(x, src_rate, dst_rate, gain):
    ratio = Fraction(dst_rate / src_rate).limit_denominator(2000)
    y = resample_poly(x, ratio.numerator, ratio.denominator) * gain
    y = np.clip(np.round(y), -32768, 32767).astype("<i2")
    with tempfile.TemporaryDirectory() as tmp:
        wav = Path(tmp) / "s.wav"
        with wave.open(str(wav), "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(int(round(dst_rate)))
            w.writeframes(y.tobytes())
        subprocess.run([str(ENCODER), "encode", "-q", "-o", tmp, str(wav)], check=True)
        brr = bytearray((Path(tmp) / "s.brr").read_bytes())
    brr[-9] = (brr[-9] | 1) & ~2              # end block, no loop: a one-shot
    return bytes(brr), len(y) / dst_rate


def main():
    cfg = tomllib.loads((GSS / "sfx.toml").read_text())
    note = int(cfg.get("note", 71))
    trim_db = float(cfg.get("trim_db", -50))
    effects = cfg.get("sfx", [])
    if not effects:
        fail("res/gss/sfx.toml lists no [[sfx]]")

    blob = (GSS / "spc700.bin").read_bytes()
    size = word(blob, 0)
    m = bytearray(0x10000)
    m[DRIVER:DRIVER + size] = blob[2:2 + size]
    driver_end = DRIVER + size

    adsr = word(m, 0x208)
    music = word(m, 0x20C)
    n_old = (adsr + 2 * FIRST_INSTRUMENT - DIR) // 4     # directory entries SNESGSS wrote
    top = min(word(m, DIR + 4 * i) for i in range(FIRST_INSTRUMENT)) & 0xFFF0
    periods = note_table(m)
    rate = 32000 * periods[note] / 4096
    print(f"SNESGSS export: {n_old - FIRST_INSTRUMENT} instrument(s), music at ${music:04X}; "
          f"effects play note {note}, encoded at {rate:.0f} Hz")

    # The instruments SNESGSS exported: their samples move to the bank.
    old = []
    for n in range(FIRST_INSTRUMENT, n_old):
        start, loop = word(m, DIR + 4 * n), word(m, DIR + 4 * n + 2)
        data = bytes(m[start:start + brr_length(m, start)])
        old.append((n, data, loop - start, m[adsr + 2 * n], m[adsr + 2 * n + 1]))

    # The new instruments.
    new = []
    for i, e in enumerate(effects):
        path = (GSS / e["wav"]).resolve()
        x, src = load_wav(path, trim_db)
        data, secs = encode(x, src, rate, float(e.get("gain", 1.0)))
        new.append((n_old + i, e["name"], data, secs))
        print(f"  {e['name']:<10} {path.name:<16} {secs * 1000:5.0f} ms  {len(data):6d} bytes")

    n_total = n_old + len(new)
    dir_end = DIR + 4 * n_total
    new_adsr = dir_end - 2 * FIRST_INSTRUMENT
    low_end = new_adsr + 2 * n_total
    if low_end > music:
        fail(f"directory and ADSR list reach ${low_end:04X}, past the music at ${music:04X}")

    # Bank: the effect table, then every sample.
    bank = bytearray()
    table_len = 1 + 2 * len(new)
    effect_code = 1 + 2 + 2 + 1 + 1 + 3       # count, pointer, FE n, note, delay, FF lo hi
    bank_len = table_len + effect_code * len(new) + sum(len(d) for _, d, *_ in old) \
        + sum(len(d) for _, _, d, _ in new)
    base = top - bank_len
    songs = [p for p in GSS.glob("music_*.bin")]
    song_end = max((music + word(p.read_bytes(), 0) for p in songs), default=music)
    if base < max(song_end, driver_end):
        fail(f"no room: the bank needs ${bank_len:04X} bytes, ${top - max(song_end, driver_end):04X} "
             f"are free (lower `note` in sfx.toml, or shorten the sounds)")

    table = base
    bank += bytes([len(new)]) + bytes(2 * len(new))
    for i in range(len(new)):
        here = base + len(bank)
        put_word(bank, 1 + 2 * i, here)
        n = new[i][0]
        delay = here + 6                      # the delay byte: the jump idles on it
        bank += bytes([1]) + struct.pack("<H", here + 3) + bytes([0xFE, n, 150 + note, 100, 0xFF]) \
            + struct.pack("<H", delay)
    # Clear the old directory tail / ADSR list / samples, then rewrite them.
    for a in range(DIR + 4 * FIRST_INSTRUMENT, driver_end):
        m[a] = 0
    for n, data, loop_off, a1, a2 in old:
        start = base + len(bank)
        bank += data
        put_word(m, DIR + 4 * n, start)
        put_word(m, DIR + 4 * n + 2, start + loop_off)
        m[new_adsr + 2 * n], m[new_adsr + 2 * n + 1] = a1, a2
    for n, _, data, _ in new:
        start = base + len(bank)
        bank += data
        put_word(m, DIR + 4 * n, start)
        put_word(m, DIR + 4 * n + 2, start)
        m[new_adsr + 2 * n], m[new_adsr + 2 * n + 1] = 0xFF, 0xE0   # instant attack, full sustain
    assert len(bank) == bank_len
    put_word(m, 0x208, new_adsr)
    put_word(m, 0x20A, table)

    (GSS / "spc700_sfx.bin").write_bytes(struct.pack("<H", size) + bytes(m[DRIVER:driver_end]))
    for old_piece in GSS.glob("sfx_bank_*.bin"):
        old_piece.unlink()
    CHUNK = 0x4000
    pieces = []
    for k, off in enumerate(range(0, len(bank), CHUNK)):
        part = bytes(bank[off:off + CHUNK])
        (GSS / f"sfx_bank_{k}.bin").write_bytes(part)
        pieces.append((k, base + off, len(part)))
    asm = ["; gss_sfx_data.asm - generated by tools/gss_add_sfx.py; do not edit", ""]
    for k, _, _ in pieces:
        asm += [f'ASSET_SECTION "gss_sfx_bank_{k}"', f'gss_sfx_bank_{k}: .incbin "res/gss/sfx_bank_{k}.bin"', ".ENDS", ""]
    (ROOT / "gss_sfx_data.asm").write_text("\n".join(asm))
    h = ["/* gss_sfx.h - generated by tools/gss_add_sfx.py from res/gss/sfx.toml; do not edit */",
         "#ifndef GAME_GSS_SFX_H", "#define GAME_GSS_SFX_H", "",
         "/* SNESGSS sound effect numbers (sfx_play's effect argument). */"]
    for i, (_, name, _, secs) in enumerate(new):
        h.append(f"#define GSS_SFX_{name.upper():<12} {i}   /* {secs * 1000:.0f} ms */")
    h += [f"#define GSS_SFX_TOTAL        {len(new)}", "",
          "/* The samples and effect data, uploaded after the driver: {ROM, APU address, size}. */"]
    for k, _, _ in pieces:
        h.append(f"extern const u8 gss_sfx_bank_{k}[];")
    h.append(f"#define GSS_SFX_BANK_PIECES {len(pieces)}")
    h.append("#define GSS_SFX_BANK { " + ", ".join(
        f"{{ gss_sfx_bank_{k}, 0x{addr:04X}, {n} }}" for k, addr, n in pieces) + " }")
    h += ["", "#endif", ""]
    (ROOT / "src" / "gss_sfx.h").write_text("\n".join(h))
    print(f"bank: {bank_len} bytes at ${base:04X}-${top - 1:04X}; "
          f"free between the music and the bank: {base - max(song_end, driver_end)} bytes")


if __name__ == "__main__":
    main()
