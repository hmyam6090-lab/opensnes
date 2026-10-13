#!/usr/bin/env python3
"""Generate placeholder sound effects res/sfx_*.wav (contributor tool).

The build never runs this: each res/<name>.wav has a res/<name>.wav.toml
(tool = "opensnes-sample") and `make` converts it to BRR. To use real
sounds, overwrite the .wav files (mono, 16-bit PCM, any rate; short
one-shots) and rebuild. Python standard library only.

    python3 tools/make_placeholder_sfx.py              # every placeholder
    python3 tools/make_placeholder_sfx.py sfx_crack    # just these
"""
import math
import struct
import wave
from pathlib import Path

RATE = 11025
RES = Path(__file__).resolve().parent.parent / "res"


def tone(freqs, ms, vol=0.6, square=True, decay=True):
    """freqs: list of (start_hz, end_hz) sweeps played back to back."""
    out = []
    per = int(RATE * ms / 1000 / len(freqs))
    phase = 0.0
    for f0, f1 in freqs:
        for i in range(per):
            f = f0 + (f1 - f0) * i / per
            phase += f / RATE
            s = 1.0 if (phase % 1.0) < 0.5 else -1.0
            if not square:
                s = math.sin(2 * math.pi * phase)
            env = 1.0 - (len(out) / (per * len(freqs))) if decay else 1.0
            out.append(int(s * vol * env * 32767))
    return out


SOUNDS = {
    "sfx_move":   tone([(880, 880)], 40),
    "sfx_select": tone([(660, 660), (990, 990)], 80),
    "sfx_blip":   tone([(1200, 1200)], 20, vol=0.35),
    "sfx_hurt":   tone([(400, 120)], 160, square=True),
    "sfx_spare":  tone([(523, 523), (659, 659), (784, 784), (1046, 1046)], 320, square=False),
    "sfx_eat":    tone([(300, 600), (300, 600), (300, 700)], 240),
    "sfx_temmie": tone([(1600, 1700)], 25, vol=0.3),
    "sfx_crack":  tone([(220, 90)], 120),
    "sfx_shatter": tone([(900, 200), (700, 100), (500, 60)], 360),
}

import sys
only = set(sys.argv[1:])          # names to (re)write; all when empty
for name, samples in SOUNDS.items():
    if only and name not in only:
        continue
    path = RES / f"{name}.wav"
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", s) for s in samples))
    toml = RES / f"{name}.wav.toml"
    if not toml.exists():
        toml.write_text('tool = "opensnes-sample"\n\n[encode]\n')
    print(f"wrote {path}")
