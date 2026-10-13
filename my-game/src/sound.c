/**
 * @file sound.c
 * @brief Sound hooks over the OpenSNES audio module.
 *
 * HOW TO GIVE A HOOK A SOUND (three steps):
 *
 *   1. Put the sound in res/ as a WAV (mono, 16-bit PCM, any rate, a short
 *      one-shot), e.g. res/sfx_step.wav, and beside it a settings file
 *      res/sfx_step.wav.toml with the two lines
 *          tool = "opensnes-sample"
 *          [encode]
 *      (copy any res/sfx_*.wav.toml). `make` converts it to BRR and writes
 *      res/sfx_step.h, which declares sfx_step_brr, sfx_step_brr_end and
 *      sfx_step_tuning.
 *   2. Include that header below, next to the others.
 *   3. Fill the hook's row of sfx_table: SFX(sfx_step, volume).
 *
 * To replace a sound, overwrite its WAV and rebuild; nothing else.
 * To add a new hook: an id in sound.h, a sound_sfx(...) where it happens,
 * then the three steps. A row left empty (or an id with no row) is a
 * silent hook. The sound driver holds 64 samples, in about 58 KB of APU RAM.
 *
 * Music: sound_bgm() is a hook only. The audio module plays samples; for
 * tracker music, switch this file to the snesmod module (LIB_MODULES
 * snesmod, an .it in res/ with an opensnes-music .toml, see
 * examples/audio/snesmod_sfx), which plays both music and effects.
 *
 * Build with GAME_AUDIO=0 (config.h) to compile all of it out.
 */
#include "sound.h"
#include "config.h"

#if GAME_AUDIO

#include <snes/audio.h>
#include "../res/sfx_move.h"
#include "../res/sfx_select.h"
#include "../res/sfx_blip.h"
#include "../res/sfx_temmie.h"
#include "../res/sfx_hurt.h"
#include "../res/sfx_crack.h"
#include "../res/sfx_shatter.h"
#include "../res/sfx_spare.h"
#include "../res/sfx_eat.h"

typedef struct {
    const u8 *brr;          /**< 0 = no sound on this hook */
    const u8 *brr_end;
    u16 pitch;              /**< DSP pitch that plays the WAV at its own rate */
    u8 volume;              /**< 0..127 */
} SfxDef;

/* tuning from opensnes-sample: x4096 = as encoded */
#define PITCH(tuning) ((u16)(((u32)AUDIO_PITCH_DEFAULT * (tuning)) >> 12))
/* One table row from the name of a converted WAV. */
#define SFX(name, vol) { name##_brr, name##_brr_end, PITCH(name##_tuning), (vol) }

static const SfxDef sfx_table[SFX_COUNT] = {
    [SFX_MOVE]          = SFX(sfx_move, 80),
    [SFX_SELECT]        = SFX(sfx_select, 90),
    [SFX_BACK]          = { 0 },
    [SFX_DENIED]        = { 0 },
    [SFX_BLIP]          = SFX(sfx_blip, 60),
    [SFX_TEMMIE_VOICE]  = SFX(sfx_temmie, 60),
    [SFX_SMUG]          = { 0 },
    [SFX_ATTACK_START]  = { 0 },
    [SFX_TEMMIE_STEP]   = { 0 },
    [SFX_ARM_STEP]      = { 0 },
    [SFX_ARM_RETRACT]   = { 0 },
    [SFX_HURT]          = SFX(sfx_hurt, 110),
    [SFX_HEART_CRACK]   = SFX(sfx_crack, 120),
    [SFX_HEART_SHATTER] = SFX(sfx_shatter, 120),
    [SFX_SPARE]         = SFX(sfx_spare, 100),
    [SFX_EAT]           = SFX(sfx_eat, 100),
    [SFX_WIN]           = { 0 },
};

static u8 audio_ok;

void sound_init(void) {
    u8 i;

    audio_ok = (audioInit() == AUDIO_OK);
    if (!audio_ok)
        return;
    for (i = 0; i < SFX_COUNT; i++)
        if (sfx_table[i].brr)
            audioLoadSample(i, sfx_table[i].brr,
                            (u16)(sfx_table[i].brr_end - sfx_table[i].brr), 0);
}

void sound_sfx(u8 id) {
    if (!audio_ok || id >= SFX_COUNT || !sfx_table[id].brr)
        return;
    audioPlaySampleEx(id, sfx_table[id].volume, AUDIO_PAN_CENTER, sfx_table[id].pitch);
}

#else

void sound_init(void) {}
void sound_sfx(u8 id) { (void)id; }

#endif

void sound_bgm(u8 id) {
    /* Hook: wire music here (see the file comment). */
    (void)id;
}
