/* SPDX-License-Identifier: GPL-2.0-or-later */
/* core-dn2 0.3: mod storage in each sound (docs/ADAPTING.md, "Mod storage
 * (core-dn2 0.3, Digitone II)"). Declare each field in mod.json:
 *
 *     "storage": [{"sym": "my_depth", "scope": "sound", "bits": 7}],
 *     "resources": {"core": "0.3"}
 *
 * and in C:
 *
 *     STORE_FIELD(my_depth);
 *     unsigned v = core_store_get(STORE(my_depth), sound, 0);
 *     core_store_set(STORE(my_depth), sound, 0, v + 1);
 *
 * item is a live sound: track t's is DN2_LIVE_SOUND(t). The Digitone II keeps
 * the sound scope only, 88 bits a sound. Zero is always the default: a field
 * reads 0 in a new sound, in one from the stock OS or from another layout.
 * Both calls mask interrupts while they read or write the field: use them
 * from any context, the audio interrupt too.
 */
#ifndef ELEKLOADER_DN2_STORE_H
#define ELEKLOADER_DN2_STORE_H

/* a field's descriptor: elekloader defines the field's symbol as its value */
#define STORE_FIELD(sym) extern char sym[]
#define STORE(sym) ((unsigned)(sym))

#define STORE_SCOPE(f) ((f) >> 28)            /* 1: sound */
#define STORE_BITS(f) ((f) >> 22 & 63)        /* 1..32 */
#define STORE_OFFSET(f) ((f) & 0x3fffff)      /* the first bit, most significant first */

#define STORE_SOUND 1

/* OS 1.12: track t's live sound (1163 B a track, t 0..15) */
#define DN2_LIVE_SOUND(t) ((void *)(*(unsigned *)0x800052a0 + 52 + 1163 * (t)))

unsigned core_store_get(unsigned field, void *item, int index);
void core_store_set(unsigned field, void *item, int index, unsigned value);

#endif
