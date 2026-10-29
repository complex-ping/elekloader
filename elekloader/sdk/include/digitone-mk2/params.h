/* SPDX-License-Identifier: GPL-2.0-or-later */
/* core-dn2 0.4: mod parameters and pages on the Digitone II (docs/ADAPTING.md,
 * "Mod parameters (core-dn2 0.4, Digitone II)").
 *
 * A mod parameter is a sound storage field declared lockable in mod.json:
 *
 *     "storage": [{"sym": "my_depth", "scope": "sound", "bits": 7, "lock": true}],
 *     "resources": {"core": "0.4"}
 *
 * The linker gives it a code, 101..127: the value slot it has past stock's
 * 0..100. Register it in core_params, and put it on a page in core_pages:
 *
 *     STORE_FIELD(my_depth);
 *     const struct core_param my_depth_param = {STORE(my_depth), 0, 127, 64, "DPTH", "Depth", "MINE"};
 *     const struct core_page my_page = {"MINE", {STORE(my_depth), 0, 0, 0, 0, 0, 0, 0}};
 *
 *     "contribute": [
 *       {"to": "core_params", "order": 50, "data": "00000000", "relocs": [[0, "abs32", "sym:my_depth_param", 0]]},
 *       {"to": "core_pages", "order": 50, "data": "00000000", "relocs": [[0, "abs32", "sym:my_page", 0]]}]
 *
 * Its knob then turns, shows and keeps its value like a stock one: the
 * value lives in the field, saved with the sound (read it with
 * core_param_value). The pages come after the [FX] key's own, in order.
 */
#ifndef ELEKLOADER_DN2_PARAMS_H
#define ELEKLOADER_DN2_PARAMS_H
#include "digitone-mk2/store.h"

struct core_param {
    unsigned field;              /* STORE(sym) of a lockable sound field */
    unsigned short min, max;     /* the values the field takes, 0..255 */
    unsigned short def;          /* a new sound's value */
    const char *short_name;      /* the knob's label, up to 4 characters */
    const char *name;            /* the long name, up to 15 */
    const char *group;           /* the group shown with it (a page's or an effect's name), up to 4 */
};

struct core_page {
    const char *name;            /* the page's name in the header, up to 4 characters */
    unsigned knobs[8];           /* STORE(sym) of a registered parameter, A..H; 0 = an empty knob */
};

/* the parameter's code (its value slot, 101..127), or 0 if the build gives it none */
unsigned core_param_code(unsigned field);
/* the parameter's value in a live sound (min..max), and a write of it. The
 * field holds the value less the default, so read and write it through these:
 * a sound that never had the field (new, from the stock OS or another layout)
 * then reads the default. */
unsigned core_param_value(unsigned field, void *sound);
void core_param_put(unsigned field, void *sound, unsigned value);

#endif
