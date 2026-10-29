/* SPDX-License-Identifier: GPL-2.0-or-later */
/* core-dn2 0.4: mod parameters and pages on the Digitone II, OS 1.12
 * (docs/ADAPTING.md, "Mod parameters (core-dn2 0.4, Digitone II)").
 *
 * The parameter table. Stock reads its 321 rows of 60 bytes at 0x401f83a4
 * through 56 references, bounded by 54 compares with 321 / 320. Core copies
 * them at boot into core_rows (the OS entry's call of main, before any OS
 * initializer), points the references at the copy and raises the bounds to
 * ROWS (params.s, mod.json). Rows 321..361 are machines-dn2's (its 40 synth
 * knobs and a filter's G); 362..388 are the mod parameters, row
 * 362 + code - 101 for code 101..127. A mod parameter's row is the Overdrive
 * row's (304: a common 0..127 knob, owner 15) with its own slot (the code),
 * range, default and names, no CC or NRPN and a unique id stock does not use.
 *
 * Values. The value getter and setter (0x4003713c, 0x40037b74) refuse a
 * slot above 100; params.s takes slots 101..127 to core_param_get and
 * core_param_set, which keep the value in the parameter's sound field (8.8
 * as stock: the field's value << 8). The setter skips stock's change event
 * for them (its listeners index their arrays by slot) and its push to the
 * voices; the store writes the field through to the project.
 *
 * Pages. The page lookup (0x400c25d8) answers ids 0..36 from the OS's page
 * table; core answers 37.. with its pages, each built from the [FX] page's
 * record (16) with its own name and knobs. The [FX] key's page list ({16} at
 * 0x401e0464, and the lock indicator's copy at 0x401e03c0) becomes
 * {16, 37, 38, ...}: the header shows FX (1/n), and [FX] steps through them.
 */

#define STOCK_ROWS 321
#define MACHINE_ROWS 41                 /* 321..361: machines-dn2's */
#define FIRST_CODE 101
#define CODES 27                        /* 101..127 */
#define ROWS (STOCK_ROWS + MACHINE_ROWS + CODES)
#define CODE_ROW(c) (STOCK_ROWS + MACHINE_ROWS + (c) - FIRST_CODE)
#define TEMPLATE 304                    /* Overdrive: owner 15 (every machine), 0..127 */
#define MACHINE_TEMPLATE 0xd5           /* machines-dn2's knobs: A Env Decay */
#define MACHINE_G 361
#define MACHINE_G_TEMPLATE 0x26         /* MULTI-MODE's TYPE */
#define NO_NRPN 0xffffffffu
#define NO_CC (-1)

#define PAGE_TABLE 0x42433c00u          /* the OS's page records, ids 0..36, 44 B */
#define PAGE_WORDS 11
#define STOCK_PAGES 37
#define FX_PAGE 16
#define MAX_PAGES 8

struct row {
    int owner, slot, min, max, def, flag;
    short cc, cc_low;
    unsigned nrpn, unique, mask;
    const char *name, *owner_name, *label;
    unsigned format, end;
};

struct string_rep { unsigned length, capacity; int references; char text[16]; };

struct core_param {
    unsigned field;
    unsigned short min, max, def;
    const char *short_name, *name, *group;
};

struct core_page {
    const char *name;
    unsigned knobs[8];
};

extern const struct row core_stock_rows[STOCK_ROWS];         /* 0x401f83a4 (params.s) */
extern const struct core_param *const core_params[];
extern const struct core_page *const core_pages[];
extern const unsigned core_lock_codes[];                    /* {field, code}, zero-terminated */
unsigned core_store_get(unsigned field, void *item, int index);
#define STORE_BITS(f) ((f) >> 22 & 63)
void core_store_set(unsigned field, void *item, int index, unsigned value);

struct row core_rows[ROWS];
static const struct core_param *by_code[CODES];
static struct string_rep names[CODES][3];
static unsigned page_records[MAX_PAGES][PAGE_WORDS];
unsigned core_fx_pages[1 + MAX_PAGES], core_fx_page_count;

static const char *rep(struct string_rep *r, const char *s, unsigned max)
{
    unsigned n = 0;
    while (s && n < max && n < sizeof r->text - 1 && s[n]) {
        unsigned char c = s[n];
        r->text[n++] = c;
    }
    r->text[n] = 0;
    r->length = r->capacity = n;
    r->references = -1;                 /* stock's std::string must not free it */
    return r->text;
}

static const struct core_param *param_of(unsigned slot)
{
    return slot >= FIRST_CODE && slot < FIRST_CODE + CODES ? by_code[slot - FIRST_CODE] : 0;
}

/* for locks.c */
const struct core_param *core_param_of(unsigned code)
{
    return param_of(code);
}

unsigned core_param_clamp(const struct core_param *p, unsigned value)
{
    return value < p->min ? p->min : value > p->max ? p->max : value;
}

unsigned core_param_range(const struct core_param *p, unsigned *min)
{
    *min = p->min;
    return p->max - p->min;
}

void core_lock_end(void *sound, unsigned code);

unsigned core_param_code(unsigned field)
{
    const unsigned *p;
    for (p = core_lock_codes; *p; p += 2)
        if (p[0] == field)
            return p[1] & 0xff;
    return 0;
}

static unsigned row_of(unsigned field)
{
    unsigned c = core_param_code(field);
    return c >= FIRST_CODE && c < FIRST_CODE + CODES && by_code[c - FIRST_CODE] ? CODE_ROW(c) : 0;
}

/* a word at a time: no memcpy in core */
static void copy_row(struct row *to, const struct row *from)
{
    const volatile unsigned *f = (const volatile unsigned *)from;
    volatile unsigned *t = (volatile unsigned *)to;
    unsigned i;
    for (i = 0; i < sizeof *to / 4; i++)
        t[i] = f[i];
}

/* a unique id stock does not use: the gaps in its range, as many as needed */
static unsigned next_unique(unsigned from)
{
    unsigned lo = 0xffffffffu, hi = 0, v, i;
    for (i = 0; i < STOCK_ROWS; i++) {
        v = core_stock_rows[i].unique;
        if (v == 0xffffffffu)
            continue;
        if (v < lo)
            lo = v;
        if (v > hi)
            hi = v;
    }
    for (v = from > lo ? from : lo; v < hi; v++) {
        for (i = 0; i < STOCK_ROWS && core_stock_rows[i].unique != v; i++)
            ;
        if (i == STOCK_ROWS)
            return v;
    }
    return hi + 1 + (from > hi ? from - hi : 0);
}

/* called from params.s after the stock rows are copied */
void core_params_init(void)
{
    const struct core_param *const *e;
    unsigned unique = 0, i, n = 0;
    for (e = core_params; *e; e++) {
        const struct core_param *p = *e;
        unsigned c = core_param_code(p->field);
        struct row *r;
        if (c < FIRST_CODE || c >= FIRST_CODE + CODES || by_code[c - FIRST_CODE] || p->max > 255 || p->min > p->max)
            continue;
        by_code[c - FIRST_CODE] = p;
        r = &core_rows[CODE_ROW(c)];
        copy_row(r, &core_stock_rows[TEMPLATE]);
        r->slot = c;
        r->min = p->min << 8;
        r->max = p->max << 8;
        r->def = (p->def < p->min ? p->min : p->def > p->max ? p->max : p->def) << 8;
        r->cc = r->cc_low = NO_CC;
        r->nrpn = NO_NRPN;
        r->unique = unique = next_unique(unique + 1);
        r->label = rep(&names[c - FIRST_CODE][0], p->short_name, 4);
        r->name = rep(&names[c - FIRST_CODE][1], p->name, 15);
        r->owner_name = rep(&names[c - FIRST_CODE][2], p->group, 4);
    }
    core_fx_pages[n++] = FX_PAGE;
    for (i = 0; core_pages[i] && i < MAX_PAGES; i++)
        core_fx_pages[n++] = STOCK_PAGES + i;
    core_fx_page_count = n;
}

/* the value-class accessor's row (0x400c257c): an added row's class is its template's; a code
 * row nobody registered keeps stock's answer past the table */
unsigned core_value_class(unsigned row)
{
    if (row >= CODE_ROW(FIRST_CODE) && row < ROWS)
        return by_code[row - CODE_ROW(FIRST_CODE)] ? TEMPLATE : row;
    if (row == MACHINE_G)
        return MACHINE_G_TEMPLATE;
    if (row >= STOCK_ROWS && row < MACHINE_G)
        return MACHINE_TEMPLATE;
    return row;
}

/* the page lookup (0x400c25d8) for ids past stock's: a record built from the [FX] page's */
unsigned *core_page_record(unsigned id)
{
    unsigned i = id - STOCK_PAGES, k;
    const unsigned *fx = (const unsigned *)(PAGE_TABLE + 4 * PAGE_WORDS * FX_PAGE);
    unsigned *rec;
    if (id < STOCK_PAGES || i >= MAX_PAGES || !core_pages[i] || !fx[0])
        return (unsigned *)(PAGE_TABLE - 4 * PAGE_WORDS);      /* stock's answer past 36 */
    rec = page_records[i];
    if (!rec[0]) {
        for (k = 1; k < PAGE_WORDS; k++)
            rec[k] = fx[k];
        for (k = 0; k < 8; k++)
            rec[2 + k] = core_pages[i]->knobs[k] ? row_of(core_pages[i]->knobs[k]) : 0;
        rec[0] = (unsigned)core_pages[i]->name;               /* last: a complete record */
    }
    return rec;
}


/* A field holds its value less the parameter's default (mod its width), so
 * that a sound without it (new, stock, another layout: the field reads 0)
 * has the default. */
static unsigned decode(const struct core_param *p, unsigned stored)
{
    unsigned w = STORE_BITS(p->field), v = (stored + p->def) & ((1u << w) - 1);
    return v < p->min ? p->min : v > p->max ? p->max : v;
}

unsigned core_param_value(unsigned field, void *sound)
{
    const struct core_param *p = param_of(core_param_code(field));
    return p ? decode(p, core_store_get(field, sound, 0)) : 0;
}

void core_param_put(unsigned field, void *sound, unsigned value)
{
    const struct core_param *p = param_of(core_param_code(field));
    if (!p)
        return;
    value = core_param_clamp(p, value);
    core_store_set(field, sound, 0, (value - p->def) & ((1u << STORE_BITS(field)) - 1));
}

/* the getter's value (8.8) of a slot 101..127 in a live sound */
int core_param_get(void *sound, unsigned slot)
{
    const struct core_param *p = param_of(slot);
    return p ? (int)(decode(p, core_store_get(p->field, sound, 0)) << 8) : 0;
}

/* the setter's write (8.8) of a slot 101..127: it also ends the track's playing lock of it */
void core_param_set(void *sound, unsigned slot, int value)
{
    const struct core_param *p = param_of(slot);
    if (p) {
        core_param_put(p->field, sound, value < 0 ? 0 : (unsigned)value >> 8);
        core_lock_end(sound, slot);
    }
}
