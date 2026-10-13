; gss_data.asm - the SNESGSS export, as data (replaces SNESGSS's sounds.asm)
;
; res/gss/spc700.bin  the GSS sound driver (2-byte size, then the code
;                     that runs at APU $0200)
; res/gss/music_N.bin one song each (2-byte size, then the song)
;
; After a new export from SNESGSS, copy spc700.bin and every music_N.bin
; over these, then run `python3 tools/gss_add_sfx.py`: the game uses
; spc700_sfx.bin, the export plus the WAV sound effects of res/gss/sfx.toml
; (their samples are in gss_sfx_data.asm).
;
; Songs loop in code: sound.c points each channel's end jump back to the
; channel's start when it loads the song (gss_music_loop in sound.c), so
; no loop point is needed in SNESGSS.

ASSET_SECTION "gss_driver"
gss_driver:     .incbin "res/gss/spc700_sfx.bin"
gss_driver_end:
.ENDS

ASSET_SECTION "gss_music_1"
gss_music_1:    .incbin "res/gss/music_1.bin"
gss_music_1_end:
.ENDS
