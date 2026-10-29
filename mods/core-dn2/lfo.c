/* SPDX-License-Identifier: GPL-2.0-or-later */
/* core-dn2 0.4: mod parameters as LFO destinations on the Digitone II, OS 1.12
 * (docs/ADAPTING.md, "Mod parameters (core-dn2 0.4, Digitone II)").
 *
 * An LFO's DEST is the value of its stock parameter (live slots 4, 12, 20):
 * the destination's slot << 8. The DEST list walks slots 0..100 and asks the
 * slot -> row lookup (0x400dc18e) for each; lfo.s walks 101..127 too, and
 * the lookup answers a registered code's row (its +44 flags are Overdrive's:
 * an LFO destination).
 *
 * The LFO evaluator (per voice, per LFO, every audio block) adds an LFO's
 * modulation to the voice's cell for a DEST 0..100 and skips the others;
 * for a code lfo.s computes the same modulation (8.8, 0..127 full scale)
 * into core_lfo_delta[voice][lfo] with the code in core_lfo_code, which
 * core_param_voice adds, scaled to the parameter's range.
 *
 * Saved, stock writes a DEST as its stored id << 8 (the slot's id from its
 * table, which has no id for a code). A code is saved as id 0 (slot 0: no
 * destination on a stock OS) with the code in the low byte, by core's wraps
 * of the sound converters (save, load) and at stock's per-edit write into
 * the project image (0x4004cb16, 0x4004cb82: core_dest_store).
 */

#define FIRST_CODE 101
#define LAST_CODE 127
#define VOICES 16
#define LFOS 4                          /* 3 on the Digitone II; 4 for the index's shift */
#define DESTS 3
#define CODE_ROW(c) (321 + 41 + (c) - FIRST_CODE)

struct core_param;
const struct core_param *core_param_of(unsigned code);   /* params.c */

unsigned char core_lfo_code[VOICES * LFOS];               /* lfo.s writes them */
int core_lfo_delta[VOICES * LFOS];

static const unsigned char live_dest[DESTS] = {0x14 + 2 * 4, 0x14 + 2 * 12, 0x14 + 2 * 20};
static const unsigned char stored_dest[DESTS] = {28 + 2 * 13, 28 + 2 * 14, 28 + 2 * 15};

static int code(unsigned c)
{
    return c >= FIRST_CODE && c <= LAST_CODE;
}

/* the slot -> row lookup (0x400dc18e) for a slot past 100: a registered code's row, else 0 (stock's) */
unsigned core_code_row(unsigned slot)
{
    return code(slot) && core_param_of(slot) ? CODE_ROW(slot) : 0;
}

/* the summed LFO modulation of a code on a voice, 8.8 of 0..127 */
int core_lfo_sum(unsigned voice, unsigned c)
{
    int sum = 0, i;
    if (voice >= VOICES)
        return 0;
    for (i = 0; i < LFOS; i++)
        if (core_lfo_code[voice * LFOS + i] == c)
            sum += core_lfo_delta[voice * LFOS + i];
    return sum;
}

/* a sound's save (st_sound_save): its codes' DESTs as {0, code} */
void core_dest_save(unsigned char *stored, const unsigned char *live)
{
    int i;
    for (i = 0; i < DESTS; i++)
        if (code(live[live_dest[i]])) {
            stored[stored_dest[i]] = 0;
            stored[stored_dest[i] + 1] = live[live_dest[i]];
        }
}

/* ... and its load (st_sound_load) */
void core_dest_load(unsigned char *live, const unsigned char *stored)
{
    int i;
    for (i = 0; i < DESTS; i++)
        if (!stored[stored_dest[i]] && code(stored[stored_dest[i] + 1])) {
            live[live_dest[i]] = stored[stored_dest[i] + 1];
            live[live_dest[i] + 1] = 0;
        }
}
