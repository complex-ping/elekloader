/* SPDX-License-Identifier: GPL-2.0-or-later */
/* param-check-dn2: the smallest user of core-dn2's mod parameters (core 0.4).
 *
 * Two lockable sound fields, registered as parameters A ("ALFA", 0..127)
 * and B ("BETA", 0..15), on a page TEST after the [FX] key's own: press
 * [FX] twice. Turning A or B keeps the value in the sound, as a stock knob
 * does. Nothing else reads them.
 */
#include "digitone-mk2/params.h"

STORE_FIELD(pc_a);
STORE_FIELD(pc_b);

const struct core_param pc_param_a = {STORE(pc_a), 0, 127, 64, "ALFA", "Alfa Test", "TEST"};
const struct core_param pc_param_b = {STORE(pc_b), 0, 15, 3, "BETA", "Beta Test", "TEST"};
const struct core_page pc_page = {"TEST", {STORE(pc_a), STORE(pc_b), 0, 0, 0, 0, 0, 0}};
