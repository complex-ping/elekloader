/* SPDX-License-Identifier: GPL-2.0-or-later */
/* core-dn2 0.3: mod storage on the Digitone II, OS 1.12, the sound scope
 * (docs/ADAPTING.md, "Mod storage (core-dn2 0.3, Digitone II)").
 *
 * Mods declare fields in their mod.json ("storage"); elekloader places them
 * at build time from the build's lockfile and gives each one an absolute
 * symbol, its descriptor, which a mod passes to core_store_get and
 * core_store_set. Zero always means the mod's default.
 *
 * A sound's fields live in 11 bytes of the live sound (1163 B) that stock
 * never reads or writes: +0x180..+0x18a. Stock's own data ends at +0x176
 * (the arpeggiator's offsets end at +0x175); the converters, the engine and
 * the pages never touch the bytes after it, and initialising a sound
 * (0x400e7fca) clears them. Copy, paste, undo and every whole-sound copy
 * carry them with the sound, with no hooks.
 *
 * A saved sound keeps them in the 16 bytes of the stored sound's value block
 * that stock never writes: the values of lock ids 4, 8, .. 32 (+28 + 2 id),
 * the fourth LFO's rank, which has no LFO. They hold the layout ID (4 B),
 * the sound generation (1 B), then the 11 bytes. The two converters are
 * wrapped:
 *   save (0x400dd80a (stored, live, a)): stock's body, then the bytes and the
 *        ID into the stored sound;
 *   load (0x400dd34e (live, stored)): stock's body, then the bytes from the
 *        stored sound when its ID is this build's (or an ancestor's) and its
 *        generation is this build's, else zeros. Stock's load folds those ids
 *        onto the live value slot 0 (the LFOs' sink, 0 for every stock
 *        sound), so the last of them lands there: slot 0 goes back to 0.
 * Every path that saves or loads a sound goes through them: the project, a
 * kit, the pool, a sound file and a sound's SysEx.
 *
 * Stock keeps the whole project as stored data in RAM (the project image
 * that SAVE PROJECT writes and a power cycle brings back) and writes each
 * edit of a sound's parameter into the sound's stored copy there at once,
 * without the converter. So core_store_set writes through too: a field set
 * in a kit's sound or a pool sound goes into its stored copy with the
 * layout ID and the generation. The live and the stored sounds are arrays
 * (read from the boot's converter calls, which pair every one):
 *   kits   129 (the working kit, then the 128), 16 sounds each:
 *          live  0x4210d0c0 + 23921 kit + 1163 track
 *          stored 0x410aeba8 + 10752 kit + 359 track
 *   pool   128 sounds: live 0x4240dbcb + 1163 i, stored 0x41201570 + 359 i
 *
 * A sound saved with mods loads on the stock OS unchanged (stock ignores the
 * bytes), and a sound from the stock OS or another layout loads its fields
 * as zeros. */

#define SC_SOUND 1

#define SOUND_HOME 0x180                        /* the live sound's bytes */
#define SOUND_BYTES 11
#define STORED_VALUES 28                        /* the stored value block, u16 by lock id */
#define LIVE_SLOT0 0x14                         /* the live value slot 0 */

extern const unsigned core_store_ids[];
extern char __store_gen[];

/* store.s: each converter's displaced first instructions, then a jump back */
extern int st_sound_save_body(unsigned char *dst, unsigned char *live, int a);
extern int st_sound_load_body(unsigned char *live, unsigned char *src);

static unsigned gen(void)
{
    return (unsigned)__store_gen >> 24 & 255;   /* the sound generation */
}

/* stored byte k (0..15) of the 16: the low lock ids' values, high byte first */
static unsigned stored_at(unsigned k)
{
    return STORED_VALUES + 2 * (4 + 4 * (k >> 1)) + (k & 1);
}

static int known_id(unsigned id)
{
    const unsigned *p;
    if (!id)
        return 0;
    for (p = core_store_ids; *p; p++)
        if (*p == id)
            return 1;
    return 0;
}

static unsigned short irq_mask(void)
{
    unsigned short sr;
    __asm__ volatile("move.w %%sr,%0\n\tmove.w #0x2700,%%sr" : "=d"(sr) : : "memory");
    return sr;
}

static void irq_restore(unsigned short sr)
{
    __asm__ volatile("move.w %0,%%sr" : : "d"(sr) : "memory");
}

#define LIVE_KITS 0x4210d0c0u
#define LIVE_KIT 23921u
#define STORED_KITS 0x410aeba8u
#define STORED_KIT 10752u
#define KITS 129u
#define LIVE_POOL 0x4240dbcbu
#define STORED_POOL 0x41201570u
#define POOL 128u
#define LIVE_SOUND 1163u
#define STORED_SOUND 359u

/* the stored copy of a live kit or pool sound, else 0 */
static unsigned char *stored_of(const unsigned char *live)
{
    unsigned o = (unsigned)live - LIVE_KITS, k, t;
    if ((unsigned)live >= LIVE_KITS && o < KITS * LIVE_KIT) {
        k = o / LIVE_KIT;
        t = o % LIVE_KIT;
        if (t % LIVE_SOUND || t / LIVE_SOUND >= 16)
            return 0;
        return (unsigned char *)(STORED_KITS + STORED_KIT * k + STORED_SOUND * (t / LIVE_SOUND));
    }
    o = (unsigned)live - LIVE_POOL;
    if ((unsigned)live >= LIVE_POOL && o < POOL * LIVE_SOUND && o % LIVE_SOUND == 0)
        return (unsigned char *)(STORED_POOL + STORED_SOUND * (o / LIVE_SOUND));
    return 0;
}

/* the 16 stored bytes from a live sound's 11 (with this build's ID) */
static void put_stored(unsigned char *dst, const volatile unsigned char *live)
{
    unsigned id = core_store_ids[0], j;
    for (j = 0; j < 4; j++)
        dst[stored_at(j)] = id >> (24 - 8 * j);
    dst[stored_at(4)] = gen();
    for (j = 0; j < SOUND_BYTES; j++)
        dst[stored_at(5 + j)] = live[SOUND_HOME + j];
}

void core_dest_save(unsigned char *stored, const unsigned char *live);   /* lfo.c: a DEST of a mod parameter */
void core_dest_load(unsigned char *live, const unsigned char *stored);

int st_sound_save(unsigned char *dst, unsigned char *live, int a)
{
    int r = st_sound_save_body(dst, live, a);
    if (dst && live) {
        put_stored(dst, live);
        core_dest_save(dst, live);
    }
    return r;
}

int st_sound_load(unsigned char *live, unsigned char *src)
{
    int r = st_sound_load_body(live, src), j;
    if (live && src) {
        unsigned id = 0;
        int ok;
        for (j = 0; j < 4; j++)
            id = id << 8 | src[stored_at(j)];
        ok = known_id(id) && src[stored_at(4)] == gen();
        for (j = 0; j < SOUND_BYTES; j++)
            live[SOUND_HOME + j] = ok ? src[stored_at(5 + j)] : 0;
        live[LIVE_SLOT0] = 0;
        live[LIVE_SLOT0 + 1] = 0;
        core_dest_load(live, src);
    }
    return r;
}

static volatile unsigned char *field_base(unsigned scope, unsigned char *item)
{
    return scope == SC_SOUND && item ? item + SOUND_HOME : 0;
}

unsigned core_store_get(unsigned field, void *item, int index)
{
    unsigned scope = field >> 28, w = field >> 22 & 63, o = field & 0x3fffff, v = 0, i;
    volatile unsigned char *base = field_base(scope, item);
    unsigned short sr;
    (void)index;
    if (!base)
        return 0;
    sr = irq_mask();
    for (i = 0; i < w; i++, o++)
        v = v << 1 | (base[o >> 3] >> (7 - (o & 7)) & 1);
    irq_restore(sr);
    return v;
}

void core_store_set(unsigned field, void *item, int index, unsigned value)
{
    unsigned scope = field >> 28, w = field >> 22 & 63, o = field & 0x3fffff, i;
    volatile unsigned char *base = field_base(scope, item);
    unsigned char *dst;
    unsigned short sr;
    (void)index;
    if (!base)
        return;
    sr = irq_mask();
    for (i = 0; i < w; i++, o++) {
        unsigned bit = 0x80 >> (o & 7);
        if (value >> (w - 1 - i) & 1)
            base[o >> 3] |= bit;
        else
            base[o >> 3] &= ~bit;
    }
    if ((dst = stored_of(item)) != 0)
        put_stored(dst, (volatile unsigned char *)item);
    irq_restore(sr);
}
