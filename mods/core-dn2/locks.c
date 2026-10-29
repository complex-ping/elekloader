/* SPDX-License-Identifier: GPL-2.0-or-later */
/* core-dn2 0.4: p-locks of mod parameters on the Digitone II, OS 1.12
 * (docs/ADAPTING.md, "Mod parameters (core-dn2 0.4, Digitone II)").
 *
 * A pattern's lock table (its model's vtable[40]) holds 80 records of 258
 * bytes, {slot, track, u16 value per step} (-1: free / no lock), then an
 * index byte per track and slot 0..100 (+20640, the record or -1) and a
 * lock count per track and step (+22256, the trig's lock marker). Every
 * stock routine that takes a slot refuses or clamps one above 100.
 *
 * A mod parameter's locks are records of the same shape, {code, track},
 * with no index byte, and the value itself (0..255), not 8.8. Stock's loops
 * over a track's records (the recount, the page's autocopy, the shift, the
 * step clear, the track swap) see them as theirs; core takes the places
 * that find a record through the index byte by its id:
 *   the recorder, the reader and the remover, for codes 101..127 (locks.s);
 *   the step clear's and the track swap's index byte, skipped for them;
 *   the four pastes, which find the destination's record by index: core
 *   pastes them and stock passes over them;
 *   the trig copy and paste, which copy a step's locks slot by slot;
 *   the pattern save and load, which map slots to stored ids: a saved
 *   record of core's is {code, track | 0x20}, which the stock OS's load
 *   skips (it takes tracks 0..15 only), so a pattern keeps loading there.
 *
 * Playing. The lock-list builder (0x400d874c), run now for every note,
 * builds the step's stock list; core writes the step's mod locks into the
 * list's last 40 bytes, which stock's 101 entries never reach (a list holds
 * 202). At the note path's lock step (0x40026c20), where the voice was just
 * reset to its sound, core takes the list's mod locks for the voice and its
 * track: a lock plays until the track's next trig. Turning the knob ends its
 * playing lock, as stock's does.
 */

#define CODES 27
#define FIRST_CODE 101
#define RECORDS 80
#define REC_BYTES 258
#define INDEX 20640
#define COUNTS 22256
#define TRACKS 16
#define STEPS 128
#define VOICES 16
#define STORED_TAG 0x20
#define LIST_AREA 1596                  /* a list is 20 + 8 x 202 bytes; stock fills 20 + 8 x 101 at most */
#define LIST_STAMP 0xc0de0000u
#define RECOUNT 0x4003ce3c              /* (model, track, step): the step's lock count */
#define LIVE 0x800052a0u                /* -> the live sounds, 52 + 1163 t */

struct core_param;
const struct core_param *core_param_of(unsigned code);             /* params.c */
unsigned core_param_clamp(const struct core_param *p, unsigned value);
unsigned core_param_code(unsigned field);
unsigned core_param_value(unsigned field, void *sound);

struct list_area { unsigned stamp, mask; unsigned char value[CODES]; };

static unsigned track_mask[TRACKS], voice_mask[VOICES];
static unsigned char track_value[TRACKS][CODES], voice_value[VOICES][CODES], voice_track[VOICES], voice_notes[VOICES];

typedef unsigned (*table_fn)(void *);      /* stock's returns in d0: no pointer type (GCC reads those from a0) */
typedef void (*notify_fn)(void *, void *);
typedef void (*recount_fn)(void *, unsigned, unsigned);

static unsigned char *table_of(void *model)
{
    const unsigned *vt;
    if (!model)
        return 0;
    vt = *(const unsigned *const *)model;
    return (unsigned char *)((table_fn)vt[10])(model);
}

/* the model's "changed" message: 0 = the whole table (a slot's message names a stock slot) */
static void changed(void *model)
{
    const unsigned *vt = *(const unsigned *const *)model;
    ((notify_fn)vt[4])(model, 0);
}

static void recount(void *model, unsigned track, unsigned step)
{
    ((recount_fn)RECOUNT)(model, track, step);
}

static int ours(unsigned code)
{
    return code >= FIRST_CODE && code < FIRST_CODE + CODES;
}

static unsigned short *values(unsigned char *rec)
{
    return (unsigned short *)(rec + 2);
}

static unsigned char *find(unsigned char *t, unsigned code, unsigned track)
{
    unsigned i;
    for (i = 0; i < RECORDS; i++, t += REC_BYTES)
        if (t[0] == code && t[1] == track)
            return t;
    return 0;
}

static unsigned char *find_or_claim(unsigned char *t, unsigned code, unsigned track)
{
    unsigned char *r = find(t, code, track);
    unsigned i, s;
    if (r)
        return r;
    for (i = 0, r = t; i < RECORDS; i++, r += REC_BYTES)
        if (r[0] == 0xff) {
            r[0] = code;
            r[1] = track;
            for (s = 0; s < STEPS; s++)
                values(r)[s] = 0xffff;
            return r;
        }
    return 0;
}

static int empty(unsigned char *r)
{
    unsigned s;
    for (s = 0; s < STEPS; s++)
        if (values(r)[s] != 0xffff)
            return 0;
    return 1;
}

/* a lock into the table, the step's count kept: the pastes' writer */
static void put(unsigned char *t, unsigned code, unsigned track, unsigned step, unsigned short v)
{
    unsigned char *r = find_or_claim(t, code, track);
    if (!r || step >= STEPS)
        return;
    if (values(r)[step] == 0xffff)
        t[COUNTS + STEPS * track + step]++;
    values(r)[step] = v;
}

/* ---- the recorder, the reader and the remover (locks.s takes slots 101..127 here) ---- */

int core_lock_record(void *model, unsigned track, unsigned step, unsigned slot, int value)
{
    const struct core_param *p = core_param_of(slot);
    unsigned char *t = table_of(model), *r;
    if (!t || !p || track >= TRACKS || step >= STEPS || value < 0)
        return 0;
    r = find_or_claim(t, slot, track);
    if (!r)
        return 0;
    values(r)[step] = core_param_clamp(p, (unsigned)value >> 8);
    changed(model);
    recount(model, track, step);
    return 1;
}

int core_lock_read(void *model, unsigned track, unsigned step, unsigned slot)
{
    unsigned char *t = table_of(model), *r;
    if (!t || !core_param_of(slot) || track >= TRACKS || step >= STEPS)
        return -1;
    r = find(t, slot, track);
    return r && values(r)[step] != 0xffff ? values(r)[step] << 8 : -1;
}

int core_lock_remove(void *model, int track, int step, int slot)
{
    unsigned char *t = table_of(model), *r;
    if (!t || !ours(slot) || track < 0 || track >= TRACKS || step < 0 || step >= STEPS)
        return 0;
    r = find(t, slot, track);
    if (!r || values(r)[step] == 0xffff)
        return 0;
    values(r)[step] = 0xffff;
    if (empty(r))
        r[0] = r[1] = 0xff;
    changed(model);
    recount(model, track, step);
    return 1;
}

/* ---- the pastes: a record of the clipboard's -> d of the destination ---- */

/* rec: the clipboard's record; vals: its values for the pasted steps (u16 at
 * +2 + 2k, k < count); table, first: the destination's table and first step.
 * -> the record's track for stock to compare, or -2 for one of core's (pasted
 * here when it is the clipboard's track), which matches no track. */
int core_lock_paste(unsigned char *rec, unsigned char *vals, unsigned char *table, unsigned first,
                    unsigned count, unsigned track, int clip_track)
{
    unsigned k;
    if (!ours(rec[0]))
        return (signed char)rec[1];
    if ((signed char)rec[1] == clip_track && track < TRACKS)
        for (k = 0; k < count; k++) {
            unsigned short v = *(unsigned short *)(vals + 2 + 2 * k);
            if (v != 0xffff)
                put(table, rec[0], track, first + k, v);
        }
    return -2;
}

/* ---- trig copy and paste: a step's locks of core's, by clipboard entry ---- */

#define TRIG_ENTRIES 64
#define TRIG_ENTRY 72                   /* the clipboard's entries: a vector {., begin, end} of 72-byte entries */
static struct { unsigned mask; unsigned char value[CODES]; } trig_clip[TRIG_ENTRIES];

/* the copy is about to add the entry at the vector's end */
void core_lock_trig_copy(const unsigned *vec, void *model, unsigned track, unsigned step)
{
    unsigned char *t = table_of(model), *r;
    unsigned entry = (vec[2] - vec[1]) / TRIG_ENTRY, i;
    if (entry >= TRIG_ENTRIES)
        return;
    trig_clip[entry].mask = 0;
    if (!t || track >= TRACKS || step >= STEPS)
        return;
    for (i = 0, r = t; i < RECORDS; i++, r += REC_BYTES)
        if (ours(r[0]) && r[1] == track && values(r)[step] != 0xffff) {
            trig_clip[entry].mask |= 1u << (r[0] - FIRST_CODE);
            trig_clip[entry].value[r[0] - FIRST_CODE] = values(r)[step];
        }
}

void core_lock_trig_paste(const unsigned *vec, unsigned at, void *model, unsigned track, unsigned step)
{
    unsigned char *t = table_of(model);
    unsigned entry = (at - vec[1]) / TRIG_ENTRY, c;
    if (entry >= TRIG_ENTRIES || !trig_clip[entry].mask || !t || track >= TRACKS || step >= STEPS)
        return;
    for (c = 0; c < CODES; c++)
        if (trig_clip[entry].mask >> c & 1)
            put(t, FIRST_CODE + c, track, step, trig_clip[entry].value[c]);
    recount(model, track, step);
}

/* ---- the pattern load: a saved record of core's (track | 0x20) into the table ---- */

void core_lock_load(unsigned char *table, unsigned char *rec, const unsigned char *stored)
{
    unsigned track = stored[1] & ~STORED_TAG, s;
    rec[0] = stored[0];
    rec[1] = track;
    for (s = 0; s < STEPS; s++) {
        unsigned short v = *(const unsigned short *)(stored + 2 + 2 * s);
        values(rec)[s] = v;
        if (v != 0xffff)
            table[COUNTS + STEPS * track + s]++;
    }
}

/* ---- playing ---- */

/* after the stock builder (track, list, table, ., step): the step's mod locks into the list */
void core_lock_build(unsigned track, unsigned char *list, unsigned char *table, unsigned step)
{
    struct list_area *a = (struct list_area *)(list + LIST_AREA);
    unsigned char *r;
    unsigned i;
    a->stamp = LIST_STAMP | track;
    a->mask = 0;
    if (!table || track >= TRACKS || step >= STEPS)
        return;
    for (i = 0, r = table; i < RECORDS; i++, r += REC_BYTES)
        if (ours(r[0]) && r[1] == track && values(r)[step] != 0xffff) {
            a->mask |= 1u << (r[0] - FIRST_CODE);
            a->value[r[0] - FIRST_CODE] = values(r)[step];
        }
}

/* the note path's lock step: the voice was just reset to its sound; list the trig's (or 0) */
void core_lock_note(unsigned voice, unsigned track, unsigned char *list)
{
    const struct list_area *a = list ? (const struct list_area *)(list + LIST_AREA) : 0;
    unsigned mask = a && a->stamp == (LIST_STAMP | track) ? a->mask : 0, c;
    if (voice >= VOICES || track >= TRACKS)
        return;
    voice_track[voice] = track + 1;
    voice_notes[voice]++;
    voice_mask[voice] = mask;
    track_mask[track] = mask;
    for (c = 0; c < CODES; c++)
        if (mask >> c & 1)
            voice_value[voice][c] = track_value[track][c] = a->value[c];
}

/* a value written to a track's sound (the setter: a knob, MIDI, randomize) ends that parameter's playing lock */
void core_lock_end(void *sound, unsigned code)
{
    unsigned base = *(volatile unsigned *)LIVE + 52, t, v, bit;
    if (!ours(code) || (unsigned)sound < base || ((unsigned)sound - base) % 1163)
        return;
    t = ((unsigned)sound - base) / 1163;
    if (t >= TRACKS)
        return;
    bit = 1u << (code - FIRST_CODE);
    track_mask[t] &= ~bit;
    for (v = 0; v < VOICES; v++)
        if (voice_track[v] == t + 1)
            voice_mask[v] &= ~bit;
}

/* ---- the values a mod reads (digitone-mk2/params.h) ---- */

static int code_of(unsigned field)
{
    unsigned c = core_param_code(field);
    return ours(c) && core_param_of(c) ? (int)(c - FIRST_CODE) : -1;
}

unsigned core_param_track(unsigned field, int track)
{
    int c = code_of(field);
    if (c < 0 || track < 0 || track >= TRACKS)
        return 0;
    if (track_mask[track] >> c & 1)
        return track_value[track][c];
    return core_param_value(field, (void *)(*(volatile unsigned *)LIVE + 52 + 1163 * track));
}

int core_lfo_sum(unsigned voice, unsigned code);                     /* lfo.c */
unsigned core_param_range(const struct core_param *p, unsigned *min);   /* params.c */

/* + the voice's LFOs on it, scaled from 0..127 to the parameter's range */
unsigned core_param_voice(unsigned field, int voice)
{
    int c = code_of(field), v, d;
    unsigned min, range;
    if (c < 0 || voice < 0 || voice >= VOICES || !voice_track[voice])
        return 0;
    v = voice_mask[voice] >> c & 1 ? voice_value[voice][c]
        : core_param_value(field, (void *)(*(volatile unsigned *)LIVE + 52 + 1163 * (voice_track[voice] - 1)));
    d = core_lfo_sum(voice, FIRST_CODE + c);
    if (!d)
        return v;
    range = core_param_range(core_param_of(FIRST_CODE + c), &min);
    v += d * (int)range / (127 * 256);
    return v < (int)min ? min : v > (int)(min + range) ? min + range : (unsigned)v;
}

int core_voice_track(int voice)
{
    return voice >= 0 && voice < VOICES ? voice_track[voice] - 1 : -1;
}

unsigned core_voice_notes(int voice)
{
    return voice >= 0 && voice < VOICES ? voice_notes[voice] : 0;
}

/* ---- ev_dsp_tx (dsptx.s): the frame's 30 free halfwords ----
 * The link transfer to the DSP is 2748 bytes and the frame 2688: the driver
 * (0x400cf922) fills its DMA list (0x80002510, 4 bytes an entry: command,
 * data) from the frame for entries 0..1343 and writes only the command half
 * of the rest, so the data halves of entries 1344..1373 reach the DSP as
 * they are: at the end of its link in-buffer (0x2c29d0 + 4096 bank + 2688). */
#define TX_LIST 0x80002510u
#define TX_PAD 1344
#define TX_WORDS 30

void core_dsp_put(unsigned i, unsigned value)
{
    if (i < TX_WORDS)
        *(volatile unsigned short *)(TX_LIST + 4 * (TX_PAD + i) + 2) = value;
}
