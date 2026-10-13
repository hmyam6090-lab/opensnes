/**
 * @file sound.c
 * @brief Sound hooks, over one of two backends (config.h: GAME_SOUND).
 *
 * The game only calls sound_sfx(SFX_...) and sound_bgm(BGM_...). What
 * they play depends on the backend; a hook with nothing assigned is
 * silent. Only one backend can run: each one uploads its own program into
 * the SPC700.
 *
 * ---- SOUND_GSS (the default): SNESGSS --------------------------------------
 *
 * res/gss/spc700.bin is the GSS driver, res/gss/music_N.bin the songs,
 * both straight from SNESGSS's export ("sounds.asm / sounds.h" are not
 * needed: gss_data.asm includes the two .bin files). After a new export,
 * copy spc700.bin and the music_N.bin files over the old ones and rebuild.
 *
 *   - Music: gss_music[] says which song each BGM_ hook plays (0 = stop
 *     the music), gss_music_loop[] whether it loops (done in code: no loop
 *     point needed in SNESGSS). A new song: an .incbin line in gss_data.asm, an extern
 *     below, a row in gss_music[].
 *   - Sound effects: WAV files, listed in res/gss/sfx.toml and added to
 *     the driver by tools/gss_add_sfx.py (it writes spc700_sfx.bin, the
 *     sample bank and gss_sfx.h). gss_sfx[] gives each hook its effect,
 *     volume and channel (GSS_NONE = silent). To add one: a WAV in
 *     assets/sfx/, an [[sfx]] in sfx.toml, run the script, fill the row.
 *
 * The protocol (65816 side) is SNESGSS's own snes/sneslib.asm and
 * sneslib.h: the driver is uploaded at $0200 through the IPL boot ROM,
 * started, sent INITIALIZE; a song is loaded by sending LOAD (the driver
 * returns to the IPL), uploading the song at the address the driver's
 * header gives (bytes 14-15 of spc700.bin), restarting the driver at $0200
 * and sending MUSIC_PLAY. The IPL transfers use the OpenSNES apu module.
 *
 * ---- SOUND_SAMPLES: the OpenSNES audio module ------------------------------
 *
 * Sound effects from WAV files, no music. To give a hook a sound:
 *   1. res/<name>.wav (mono, 16-bit PCM, a short one-shot) and beside it
 *      res/<name>.wav.toml (copy any res/sfx_*.wav.toml); `make` converts
 *      it and writes res/<name>.h;
 *   2. include that header below, next to the others;
 *   3. fill the hook's row of sfx_table: SFX(<name>, volume 0..127).
 *
 * ---- SOUND_OFF: everything compiles to nothing.
 */
#include "sound.h"
#include "config.h"

#if GAME_SOUND == SOUND_GSS

#include <snes/apu.h>
#include "gss_sfx.h"

/* gss_data.asm */
extern const u8 gss_driver[];
extern const u8 gss_music_1[];

/* SNESGSS driver commands (snes/sneslib.h). */
#define SCMD_INITIALIZE      0x01
#define SCMD_LOAD            0x02
#define SCMD_STEREO          0x03
#define SCMD_GLOBAL_VOLUME   0x04
#define SCMD_MUSIC_PLAY      0x06
#define SCMD_MUSIC_STOP      0x07
#define SCMD_SFX_PLAY        0x09
#define SCMD_STOP_ALL_SOUNDS 0x0A

#define GSS_DRIVER_ADDR      0x0200
#define GSS_NONE             0xFF
#define GSS_SFX_PAN          128   /* 0 left .. 255 right */

/* Which song each music hook plays; 0 = stop the music. */
static const u8 *const gss_music[BGM_COUNT] = {
    [BGM_NONE]   = 0,
    [BGM_BATTLE] = gss_music_1,
    [BGM_WIN]    = 0,
    [BGM_LOSE]   = 0,
};

/* 1 = the song loops back to its start when it ends. Only songs without a
 * loop point of their own are changed (see gss_find_loops). */
static const u8 gss_music_loop[BGM_COUNT] = {
    [BGM_BATTLE] = 1,
};

/* Which SNESGSS sound effect each hook plays (GSS_SFX_* from gss_sfx.h,
 * made by tools/gss_add_sfx.py from res/gss/sfx.toml), how loud (0..127)
 * and on which channel. Effects only play on channels the song leaves
 * free (the song here uses channel 0); one channel plays one effect at a
 * time, so sounds that may overlap get different channels. */
typedef struct { u8 sfx; u8 volume; u8 channel; } GssSfx;
static const GssSfx gss_sfx[SFX_COUNT] = {
    [SFX_MOVE]          = { GSS_NONE,           0,   6 },
    [SFX_SELECT]        = { GSS_SFX_SELECT,     110, 6 },
    [SFX_BACK]          = { GSS_NONE,           0,   6 },
    [SFX_DENIED]        = { GSS_NONE,           0,   6 },
    [SFX_BLIP]          = { GSS_SFX_TEXT,       90,  7 },
    [SFX_TEMMIE_VOICE]  = { GSS_SFX_TEMMIE,     100, 7 },
    [SFX_SMUG]          = { GSS_NONE,           0,   5 },
    [SFX_ATTACK_START]  = { GSS_NONE,           0,   5 },
    [SFX_TEMMIE_STEP]   = { GSS_NONE,           0,   4 },
    [SFX_ARM_STEP]      = { GSS_NONE,           0,   4 },
    [SFX_ARM_RETRACT]   = { GSS_NONE,           0,   4 },
    [SFX_HURT]          = { GSS_SFX_HURT,       120, 5 },
    [SFX_HEART_CRACK]   = { GSS_SFX_CRACK,      127, 5 },
    [SFX_HEART_SHATTER] = { GSS_SFX_SHATTER,    127, 5 },
    [SFX_SPARE]         = { GSS_NONE,           0,   6 },
    [SFX_EAT]           = { GSS_NONE,           0,   6 },
    [SFX_WIN]           = { GSS_NONE,           0,   6 },
};

/* The effects' samples and code, uploaded after the driver. */
typedef struct { const u8 *data; u16 addr; u16 size; } GssPiece;
static const GssPiece gss_bank[GSS_SFX_BANK_PIECES] = GSS_SFX_BANK;

static u8  gss_ok;             /* the driver answered */
static u16 gss_load_addr;      /* where songs go in APU RAM */
static u8  gss_playing = BGM_NONE;

/* Wait (bounded) for APU port 0 to read zero or non-zero. A driver that
 * stops answering silences the game instead of freezing it. */
static u8 gss_wait_port0(u8 want_zero) {
    u16 n = 0;
    do {
        if ((REG_APUIO0 == 0) == want_zero)
            return 1;
    } while (++n);
    gss_ok = 0;
    return 0;
}

/* spc_command_asm: wait for the driver to be idle, parameter on ports
 * 2-3, command on ports 1-0 (port 0 last: it triggers the driver), then
 * wait for the acknowledge - except after LOAD, where the driver jumps
 * back to the IPL instead. */
static void gss_command(u16 command, u16 param) {
    if (!gss_ok || !gss_wait_port0(1))
        return;
    REG_APUIO2 = (u8)param;
    REG_APUIO3 = (u8)(param >> 8);
    REG_APUIO1 = (u8)(command >> 8);
    REG_APUIO0 = (u8)command;
    if ((u8)command != SCMD_LOAD)
        gss_wait_port0(0);
}

/* Looping in code. A SNESGSS song is: channel count, one start address
 * per channel, then each channel's data, which ends with FF L H, "jump to
 * H:L" (the readme's loop command). A song exported without a loop point
 * ends each channel with a jump onto its own last byte, so the channel
 * holds its final rest forever. Each such jump gets a 2-byte patch that
 * points it back to the channel's start; the patches go up to the APU as
 * extra IPL blocks right after the song. */
#define GSS_MAX_CHANNELS 8
static u8  gss_patch[GSS_MAX_CHANNELS * 2];
static u16 gss_patch_at[GSS_MAX_CHANNELS];
static u8  gss_patches;

static void gss_find_loops(const u8 *song, u16 addr) {
    const u8 *d = song + 2;
    u16 size = (u16)(song[0] | (song[1] << 8));
    u8 channels = d[0], c;
    u16 j, here, target, start, best;

    gss_patches = 0;
    if (channels > GSS_MAX_CHANNELS)
        channels = GSS_MAX_CHANNELS;
    for (j = (u16)(1 + 2 * channels); j + 2 < size; j++) {
        if (d[j] != 0xFF)
            continue;
        here = (u16)(addr + j);
        target = (u16)(d[j + 1] | (d[j + 2] << 8));
        if (target != (u16)(here - 1))
            continue;                       /* not a "stop here" jump */
        best = 0;                           /* the channel this jump ends */
        for (c = 0; c < channels; c++) {
            start = (u16)(d[1 + 2 * c] | (d[2 + 2 * c] << 8));
            if (start <= here && start > best)
                best = start;
        }
        if (!best || gss_patches >= GSS_MAX_CHANNELS)
            continue;
        gss_patch[gss_patches * 2] = (u8)best;
        gss_patch[gss_patches * 2 + 1] = (u8)(best >> 8);
        gss_patch_at[gss_patches] = (u16)(here + 1);
        gss_patches++;
    }
}

/* Upload a size-prefixed blob through the IPL (plus the loop patches, if
 * any), then (re)start the driver at $0200 and wait until it has cleared
 * the four ports. */
static u8 gss_upload(const u8 *blob, u16 addr, u8 patches, u8 with_bank) {
    u16 size = (u16)(blob[0] | (blob[1] << 8)), n = 0;
    u8 i;

    if (apuWaitBoot() != 0)
        return 0;
    apuUpload(blob + 2, addr, size);
    for (i = 0; i < patches; i++)
        apuUpload(&gss_patch[i * 2], gss_patch_at[i], 2);
    if (with_bank)
        for (i = 0; i < GSS_SFX_BANK_PIECES; i++)
            apuUpload(gss_bank[i].data, gss_bank[i].addr, gss_bank[i].size);
    apuExecute(GSS_DRIVER_ADDR);
    do {
        if ((REG_APUIO0 | REG_APUIO1 | REG_APUIO2 | REG_APUIO3) == 0)
            return 1;
    } while (++n);
    return 0;
}

void sound_init(void) {
    gss_load_addr = (u16)(gss_driver[14] | (gss_driver[15] << 8));
    gss_ok = gss_upload(gss_driver, GSS_DRIVER_ADDR, 0, 1);
    gss_command(SCMD_INITIALIZE, 0);
    /* Stereo, as SNESGSS's preview and its .spc export play it: without
     * it the driver runs in mono and plays every voice at full volume on
     * both sides, twice as loud as the preview. */
    gss_command(SCMD_STEREO, 1);
}

void sound_bgm(u8 id) {
    const u8 *song = id < BGM_COUNT ? gss_music[id] : 0;

    if (!gss_ok)
        return;
    if (!song) {
        gss_command(SCMD_MUSIC_STOP, 0);
        gss_playing = BGM_NONE;
        return;
    }
    if (id == gss_playing)
        return;                    /* already on: keep it going */
    gss_command(SCMD_MUSIC_STOP, 0);
    gss_command(SCMD_LOAD, 0);
    gss_patches = 0;
    if (gss_music_loop[id])
        gss_find_loops(song, gss_load_addr);
    gss_ok = gss_upload(song, gss_load_addr, gss_patches, 0);
    gss_command(SCMD_MUSIC_PLAY, 0);
    gss_playing = id;
}

void sound_sfx(u8 id) {
    if (!gss_ok || id >= SFX_COUNT || gss_sfx[id].sfx == GSS_NONE)
        return;
    /* sfx_play(chn, sfx, vol, pan): port 0 = chn << 4 | SFX_PLAY,
     * port 1 = volume, port 2 = effect, port 3 = pan. */
    gss_command((u16)((gss_sfx[id].volume << 8) | ((gss_sfx[id].channel & 0x0F) << 4) | SCMD_SFX_PLAY),
                (u16)((GSS_SFX_PAN << 8) | gss_sfx[id].sfx));
}

#elif GAME_SOUND == SOUND_SAMPLES


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


void sound_bgm(u8 id) {
    (void)id;                      /* no music on this backend */
}

#else

void sound_init(void) {}
void sound_sfx(u8 id) { (void)id; }
void sound_bgm(u8 id) { (void)id; }

#endif
