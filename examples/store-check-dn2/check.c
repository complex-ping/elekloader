/* SPDX-License-Identifier: GPL-2.0-or-later */
/* store-check-dn2: the smallest user of core-dn2's mod storage (core 0.3).
 *
 * mod.json declares three sound fields that fill a sound's 88 bits, and
 * elekloader gives each one a symbol whose value is the field's descriptor.
 * STORE CHECK in SETTINGS writes a marker into every field of the 16 tracks'
 * live sounds, through core_store_set. Three columns at the bottom left of
 * the screen, one a field, each grow a pixel for every track whose marker
 * core_store_get no longer reads back, up to 16. A 3x3 square at the right
 * end of the same edge changes each time the three have been counted.
 *
 * To check it on a unit: select STORE CHECK, switch the Digitone off and on:
 * no column grows (core writes the fields through to the project's stored
 * copy, as stock does a knob edit). Load a kit or a
 * sound saved before (or on the stock OS) into a track: its columns grow,
 * since a sound from another layout loads its fields as zeros.
 */
#include "digitone-mk2/store.h"

typedef void (*fillrect_t)(void *bmp, int x0, int y0, int x1, int y1, int colour);
#define FILLRECT ((fillrect_t)FILLRECT_AT)

STORE_FIELD(sc_sound_a);
STORE_FIELD(sc_sound_b);
STORE_FIELD(sc_sound_c);

extern void core_additem(void *menu, const void *row);
extern void sc_label(void);

static volatile unsigned sc_ticks;
volatile unsigned char sc_bad[3];
volatile unsigned sc_rounds;

static unsigned field(int col)
{
    return col == 0 ? STORE(sc_sound_a) : col == 1 ? STORE(sc_sound_b) : STORE(sc_sound_c);
}

/* a marker for track t of a field, cut to the field's width, never 0 */
static unsigned mark(unsigned f, unsigned t)
{
    unsigned w = STORE_BITS(f), v = (t + 1) * 2654435761u ^ f;
    v = w < 32 ? v & ((1u << w) - 1) : v;
    return v ? v : 1;
}

/* mode 0 writes the markers, mode 1 counts the tracks that read back wrong */
static int walk(int col, int mode)
{
    unsigned f = field(col);
    int t, bad = 0;
    for (t = 0; t < 16; t++) {
        if (mode == 0)
            core_store_set(f, DN2_LIVE_SOUND(t), 0, mark(f, t));
        else
            bad += core_store_get(f, DN2_LIVE_SOUND(t), 0) != mark(f, t);
    }
    return bad;
}

void sc_tick(void *ctrl)
{
    static int col, redraw;
    int bad;
    if ((++sc_ticks & 3) != 0)
        return;
    bad = walk(col, 1);
    if (sc_bad[col] != bad) {
        sc_bad[col] = bad;
        redraw = 1;
    }
    if (col == 2) {
        sc_rounds++;
        redraw = 1;
    }
    col = col == 2 ? 0 : col + 1;
    if (redraw) {
        *((volatile unsigned char *)ctrl + 0x20) = 1;
        redraw = 0;
    }
}

static void sc_select(void)
{
    int col;
    for (col = 0; col < 3; col++)
        walk(col, 0);
}

static int sc_nothing(void)
{
    return 0;
}

static const void *const sc_row[4] = {
    (const void *)sc_label, (const void *)sc_select,
    (const void *)sc_nothing, (const void *)sc_nothing,
};

void sc_settings(void *menu)
{
    core_additem(menu, sc_row);
}

void sc_draw(void *bmp, void *ctrl)
{
    int col, bad;
    (void)ctrl;
    /* y counts from the bottom row: 0..16 is the bottom edge */
    FILLRECT(bmp, 0, 0, 24, 16, 0);
    for (col = 0; col < 3; col++) {
        bad = sc_bad[col];
        if (bad)
            FILLRECT(bmp, 8 * col, 0, 8 * col + 5, bad - 1, 1);
    }
    FILLRECT(bmp, 125, 0, 127, 2, sc_rounds & 1);
}
