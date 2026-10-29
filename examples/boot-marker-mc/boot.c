/* SPDX-License-Identifier: GPL-2.0-or-later */
/* boot-marker for the Model:Cycles: the smallest mod on core-mc 0.3's ev_boot (docs/ADAPTING.md, "Boot (core-mc 0.3,
 * Model:Cycles)").
 *
 * At boot, before the OS starts, core calls bootmark_boot (ev_boot), which may call no firmware routine and writes
 * only the mods' RAM: here, one word. After that every frame (ev_draw) gets a 3 x 3 square in the bottom-right
 * corner if the word is there, so a unit shows that the boot handler ran. */
#include "model-cycles/core.h"

#define BOOTMARK_SEEN 0xb007b007u

volatile uint32_t bootmark_seen;

void bootmark_boot(void)
{
    bootmark_seen = BOOTMARK_SEEN;
}

void bootmark_draw(void *bmp, void *ctrl)
{
    (void)ctrl;
    if (bootmark_seen == BOOTMARK_SEEN)
        fw_fill(bmp, 125, 0, 127, 2, 1, 1);
}
