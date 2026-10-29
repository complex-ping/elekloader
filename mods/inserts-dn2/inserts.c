/* SPDX-License-Identifier: GPL-2.0-or-later */
/* inserts-dn2: a flanger insert on every voice of the Digitone II, OS 1.12
 * (README.md). This is its ColdFire side: the FLNG page's eight parameters
 * (core-dn2 0.4: kept in the sound, p-lockable, LFO destinations) and,
 * once an audio block, their values per voice to the DSP.
 *
 * The packet, 30 halfwords after the frame (core_dsp_put), one group of
 * seven voices a block (0..6, 7..13, 14, 15, 0..4: every voice within three
 * blocks):
 *   0       0xf100 | group
 *   1..28   per voice, four halfwords: DEL << 8 | DPTH, SPD << 8 | FDBK,
 *           WDTH << 8 | SPH, MIX << 8 | ROUT << 7 | (the voice's trigs & 0x7f)
 *   29      0x5a5a - the sum of 0..28 (mod 2^16)
 * Each value is the voice's own: its trig's p-lock or its sound's, with its
 * LFOs (core_param_voice).
 */
#include "digitone-mk2/params.h"

STORE_FIELD(fl_del);
STORE_FIELD(fl_dpth);
STORE_FIELD(fl_spd);
STORE_FIELD(fl_fdbk);
STORE_FIELD(fl_wdth);
STORE_FIELD(fl_sph);
STORE_FIELD(fl_rout);
STORE_FIELD(fl_mix);

const struct core_param fl_param_del = {STORE(fl_del), 0, 127, 32, "DEL", "Flanger Delay", "FLNG"};
const struct core_param fl_param_dpth = {STORE(fl_dpth), 0, 127, 64, "DPTH", "Flanger Depth", "FLNG"};
const struct core_param fl_param_spd = {STORE(fl_spd), 0, 127, 32, "SPD", "Flanger Speed", "FLNG"};
const struct core_param fl_param_fdbk = {STORE(fl_fdbk), 0, 127, 64, "FDBK", "Flanger Feedbk", "FLNG"};
const struct core_param fl_param_wdth = {STORE(fl_wdth), 0, 127, 0, "WDTH", "Flanger Width", "FLNG"};
const struct core_param fl_param_sph = {STORE(fl_sph), 0, 127, 0, "SPH", "Flanger Phase", "FLNG"};
const struct core_param fl_param_rout = {STORE(fl_rout), 0, 1, 1, "ROUT", "Flanger Route", "FLNG", CORE_ROUTE};
const struct core_param fl_param_mix = {STORE(fl_mix), 0, 127, 0, "MIX", "Flanger Mix", "FLNG"};
const struct core_page fl_page = {"FLNG", {STORE(fl_del), STORE(fl_dpth), STORE(fl_spd), STORE(fl_fdbk),
                                           STORE(fl_wdth), STORE(fl_sph), STORE(fl_rout), STORE(fl_mix)}};

#define VOICES 16
#define GROUP 7

static unsigned group;

static unsigned v(unsigned field, int voice)
{
    return core_param_voice(field, voice) & 0xff;
}

/* ev_dsp_tx: the next group's packet */
void fl_tx(void)
{
    unsigned sum, i, h;
    sum = 0xf100 | group;
    core_dsp_put(0, sum);
    for (i = 0; i < GROUP; i++) {
        int voice = (GROUP * group + i) % VOICES;
        unsigned w[4];
        w[0] = v(STORE(fl_del), voice) << 8 | v(STORE(fl_dpth), voice);
        w[1] = v(STORE(fl_spd), voice) << 8 | v(STORE(fl_fdbk), voice);
        w[2] = v(STORE(fl_wdth), voice) << 8 | v(STORE(fl_sph), voice);
        w[3] = v(STORE(fl_mix), voice) << 8 | (v(STORE(fl_rout), voice) & 1) << 7 | (core_voice_notes(voice) & 0x7f);
        for (h = 0; h < 4; h++) {
            core_dsp_put(1 + 4 * i + h, w[h]);
            sum += w[h];
        }
    }
    core_dsp_put(29, (0x5a5a - sum) & 0xffff);
    group = group == 2 ? 0 : group + 1;
}
