/**
 * @file main.c
 * @brief Temmie battle — an Undertale-style encounter on the SNES.
 *
 * The fight lives in src/: battle.c (state machine), attacks.c (bullet
 * board), content.c (every line and tunable), textbox.c (typewriter),
 * platform.c / sound.c (the only files that call the SDK).
 */
#include <snes.h>
#include "src/platform.h"
#include "src/sound.h"
#include "src/battle.h"

int main(void) {
    plat_init();
    sound_init();            /* about a second to upload the sound driver and effects */
    plat_seed(0x7E3A);
    battle_init();
    plat_show();

    while (1) {
        plat_frame();
        battle_update();
    }
    return 0;
}
