# TELOS for the Digitone mk1: research notes

Started 2026-10-28. Working material for this branch, not a doc: when TELOS
is built, what holds here moves to its home (docs/DEVICES.md, docs/FORMAT.md,
docs/ADAPTING.md, the mods' READMEs), and the rest goes into the pull
request's description.

**What this is based on.** Static reading and host-side measurement only.
The stock Digitone OS files are not in this environment: its network policy
refuses www.elektron.se, so nothing here was read from the firmware itself.
Marks: **[E]** established for the mk1 elsewhere, with its source; **[P]**
prior art, measured by digikit on the Digitakt II and Digitone II;
**[M]** measured here, reproducible with `tools/`; **[O]** open.

## The short version

1. **digikit has already built this for the Digitakt II and Digitone II**: a
   runtime with `.dkm` modules, a module store on the card, loading at boot,
   a selection record with safe mode, APPLY & RESTART and upload over USB
   SysEx. TELOS for the mk1 should take its designs and speak its upload
   protocol, so one host tool serves all three devices. The code has to be
   written again: digikit is AGPL-3.0-or-later, elekloader
   GPL-2.0-or-later.
2. **The released mk1 mods load at run time as they are [M].** Ten of
   eleven have no site of their own. None of their 955 references to
   imports is PC-relative, and nine of the ten built for both OS versions
   are the same code on 1.43 and 1.44.
3. **Two changes to the outline:**
   - **Packages are packed on the user's machine.** Every mod's `.run`
     carries 284 to 2,862 bytes of stock references (rule 2), so a package
     holds firmware bytes and is never distributed: the shop keeps shipping
     `.elemod` files.
   - **Packages load at boot, before the views are built and before the
     first tick.** `core_params` is read in `.boot`, `fx_effects` at fxkit's
     first tick, and the pages' key lists when the views are built. Loading
     at boot is digikit's choice too. Switching a package on or off applies
     after a restart, and there is no hot load in v1.
4. **The mk1's SysEx path is known [E].** digihealth hooks the SysEx
   router's entry (0x400ea880 in 1.43, 0x400eaaf4 in 1.44) and answers
   through the stock sender. In the emulator on 1.44, a request marked as
   from USB was answered. That hook becomes core-dn1's `ev_sysex`.
5. **The +Drive is the big unknown [O]:** where the block read and write
   routines are, how big the card is, and what part of it is free. That is
   the first job once the stock files are available (section 6).

## 1. Prior art: digikit's runtime

digikit at `11ad79e` (github.com/m-dwyer/digikit): `docs/design/digikit-runtime.md`,
`digikit-dkm.md`, `digikit-store.md`, `digikit-upload.md`, and findings 36,
37, 42, 43 and 59. Every result there is from its emulator; nothing has run
on a device.

| concern | digikit's answer [P] | for the mk1 |
|---|---|---|
| module file | `.dkm`: a 0xa0-byte header (`DKMF`, a sha256 of the whole file, contract versions), the image linked at base 0, `.bss` size, imports as 32-byte names, relocations `u32 offset, kind << 24 \| index` (kind 0: add the load address; kind 1: add the import's address), PC-relative references between the module's own sections resolved by the packer and to an import refused | the same, plus elekloader's contributions and claims. Its 64 KiB file limit is too small: digiprism's `.run` is 71,625 bytes |
| a module's needs | inferred from the file by one function (`needs_of`), never declared by the module | the same |
| the store | raw card sectors that no firmware structure uses; two header copies (`seq`, CRC); a table streamed through small buffers; first fit; a settings record; the selection record in the last two sectors; one card-write function that checks the range and writes only to an area that is "ours or blank" | the same design; the area has to be found (S2) |
| upload | `F0 7D 64 6B 01 <cmd> <seq> <8-in-7 body> F7`; INFO, LIST, BEGIN, DATA, END, VERIFY, DELETE, CLEAR, SELECT, RESTART, LOG; a reply only when the firmware's TX ring has room | the same protocol: a hook at the router's entry sees `F0 7D` messages (only `F0 7E` skips it); the sender's behaviour when full is open (S4) |
| when modules load | at boot, at the main OS task's entry, before the main screen view is built; the set is fixed for a session; no unload | the same; the mk1's point is S3 |
| safe mode | a boot counter in the selection record, raised before loading and cleared after 30 s stable; a flag for one safe boot; a key at power-on (untested on a device) | the same |
| restart | refuse during a card transfer (eSDHC present state, 0xFC0CC024 bits 0-2); mask interrupts; clear DDR word 0 through the uncached alias (0x48000000); `0x80` to RCM.RCR (0xEC090000) | chip-level, so the same on the MCF5441x; the mk1's own drain steps are S5 (core-ar2's `finish.s`: stock's restart after an OS upgrade can hang on the panel link) |
| code written at run time | `cpushl bc` on each 16-byte line before it runs | the same chip |
| crashes | the default exception handler's function pointer in RAM → record and soft reset | S6, optional for v1 |

## 2. What is known on the mk1

| what | 1.43 | 1.44 | source |
|---|---|---|---|
| SysEx router entry: every F0 message except F0 7E, in the MIDI input task with no lock held; a0 = a1 = the message (F0 to F7), d1 its length, d2 its source (2 or 4 USB, 8 or 0x10 DIN). Past the hook the router routes only device bytes 0x00-0x17 (digihealth's comment, written for the Digitakt mk1's same routine). | 0x400ea880, `lea 0x401cde1a,a3` | 0x400eaaf4, `lea 0x401ce1ea,a3` | [E] digihealth `dn1/mod.json`, `sysinfo.s`; on 1.44 in the emulator, a HELLO marked as from USB was answered |
| SysEx sender `bool (buf, len, abort*, port)`, port 2 = USB | 0x400ee354 | in `dn1/dn144.inc` | [E] digihealth |
| RTOS context switch | 0x40000438 | the same | [E] digihealth (idle time, CPU load) |
| the project's serializer entry; deserialize in a slot's load; the load of a block already in RAM (at boot, and from SysEx); the new project; the template copy | 0x4000eb10, 0x400acc40, 0x40010cfa, 0x4001570e, 0x400acbf4 | `ports` | [E] `mods/core-dn1/projdata.s` |
| project block: `projectStorage_v14_t`, 2,782,212 bytes, magic 0xBEEFBACE; a 480-byte gap at +0x20 that the OS neither fills nor reads | | | [E] core-dn1 |
| the working copy for power-up: compressed at sectors 0x7a000/0x7c000, read back at boot | | | [E] `projdata.s` comment |
| project slots: 4 MB each, 128 of them from 256 MB on (sector 0x80000 to 0x180000), with other regions below and above; no part of the card known to be free | | | [E] `mods/digiswarm-dn1/README.md`; where this was found is not recorded |
| the heap is a buddy allocator, and allocations at boot can stall a first boot on "FACTORY PROJECT >> +DRIVE" | | | [E] `mods/core-dn1/rows.s`: TELOS takes nothing from the firmware's heap |
| mods' RAM 0x43EC0000-0x44000000 (core-dn1 4.0, 1.25 MB); bulk 0x44000000-0x47BE0000 (59.875 MiB), never cleared by the OS; the uncached alias is +0x08000000 | | | [E] `elekloader/devices/digitone_mk1.py` |
| MCF5441x (V4m, EMAC), two CPUs; the FM voices run on the second CPU, which no mod changes; an 8 KB instruction cache | | | [E] the profile, digihealth README |

## 3. The released mods as packages [M]

`tools/fetch_dn1_mods.py` downloads the 29 Digitone mk1 files that
`web/catalog.json` pins, checking each sha256 (all matched).
`tools/pack_survey.py` and `tools/os_compare.py` produce this. "Stock refs"
are the runs of a mod's own `.run` stored as references to the user's
firmware.

| mod | own sites | .run + .bss | stock refs | relocations (to imports) | imports from | tables | claims | bulk |
|---|---|---|---|---|---|---|---|---|
| fxkit 0.1 | 0 | 19,221 | 186 runs, 1,640 B | 415 (33) | core 12 | core_audio, core_projdata, ev_note_queue, render in/out, tick | projdata:FXKT | - |
| machines 0.1 | 0 | 6,655 | 33, 284 B | 73 (31) | core 9 | core_menu, core_param_override, ev_render_voices, ev_voice_on | soundslot:0 | - |
| digitables 1.3 | 0 | 9,013 | 68, 598 B | 113 (2) | core 2 | core_params x2, core_pages, core_menu, core_projdata, 7 events | param:182, param:183, page:27, projdata:TBL1 | - |
| digihealth 1.1 | **2** | 4,480 | 38, 382 B | 85 (3) | core 1, 2 unresolved | ev_settings, tick, draw, render in/out | settings:SYSTEM INFO, sysex:0x7d | - |
| digiswarm 0.2 | 0 | 40,655 | 302, 2,622 B | 1,460 (214) | fxkit 29, core 8 | fx_effects, core_menu, core_projdata, 6 events | projdata:DCSM | 27 MiB |
| digiprism 0.2 | 0 | 73,426 | 332, 2,862 B | 2,149 (248) | fxkit 32, core 8 | + core_audio | projdata:DCHR | 7 MiB |
| digitide, digitriad, digireel, digihoard, digihalo 0.2 | 0 | 27,163-39,567 | 190-287, 1,674-2,516 B | 920-1,272 (83-87) | fxkit 31, core 7-8 | fx_effects, core_menu, core_projdata, 8 events | one projdata tag each | 0.88-17 MiB |

- **No code sites to move** except digihealth's two, which are its SysEx and
  context-switch hooks: both are core candidates.
- **9,523 relocations, 955 of them to imports: every one to an import is
  `abs32`.** The only PC-relative one is a `pc16` in digihealth, inside its
  own code, which the packer resolves. A loader that knows two relocation
  kinds takes them all.
- **Every mod carries stock references** (33 to 332 runs). A package built
  from one holds those bytes, so it is packed where the stock file is: by
  elekloader on the user's machine, at the moment it sends.
- **One package for both OS versions.** Nine of the ten with a 1.44 build
  ship the same bytes, relocations, imports and contributions in both.
  digihealth differs in 15 bytes (firmware addresses in its code) and has a
  third site on 1.44.
- **The pedals depend on fxkit** (29 to 32 imports each). Either fxkit is in
  the base build, or packages can export to packages. For v1: fxkit,
  machines-dn1 and seq-events-dn1 go in the base.
- **The seven pedals' fixed bulk regions tile the bulk area exactly**:
  59.875 MiB claimed, 0 bytes free, no overlaps. Their regions can stay as
  fixed claims, which the loader checks against each other when a package
  is switched on. The TELOS arena then goes in the mods' RAM instead: a
  base of core 4.1 (4,988 + 940 bytes), fxkit and machines takes 31,804 of
  its 1,310,720 bytes, and the nine packages need 285,814.
- **Project data**: the released mods put nine tags in the 480-byte gap
  (fxkit, the seven pedals and digitables). DigiTide's README counts the
  seven pedals, GAIN and fxkit at about 470 bytes. This is unchanged and
  still has no solution.

## 4. When each table is read

Every Digitone table's reader walks it to its zero entry, and none uses the
count `NAME_n` (`grep` of `mods/core`, `core-dn1`, `fxkit-dn1`,
`machines-dn1`, `seq-events*`). A zero slot after the static entries
therefore takes a new entry with no change to the code that reads the
table. **When** each one is read is what limits a run-time load:

| table | read | an entry added at boot works |
|---|---|---|
| the 15 events | on each call | yes |
| `core_menu` | each time the menu opens (`menu.s`) | yes |
| `core_audio` | each block (`audio.s`) | yes |
| `core_projdata` | each save and load | yes (the space is the problem) |
| `core_param_override` | each lookup | yes |
| `dn_machines`, seq-events' `ev_fire_*` | each use | yes |
| `core_pages` | the record lookup on each use; the key lists when the views are built (`pages.s`) | only before the views are built |
| `core_params` | once, in `.boot`, before the OS runs (`core_dn_boot`, `params.s`) | no: core needs a run-time install call |
| `fx_effects` | once, at fxkit's first `ev_tick` (`fxkit.c`, `ready`) | only before the first tick |

So TELOS loads at the earliest point where the card can be read, before
the views and the first tick (S3). That is digikit's choice for the same
reason: the machine view reads its list once.

## 5. Changes to the outline

1. **Adopt digikit's module, store and upload designs, and stay
   compatible with its protocol.** The package becomes `.dkm` with
   elekloader's contributions and claims added, and a staging buffer larger
   than 64 KiB.
2. **Pack locally.** `elekloader` (and the TypeScript port) packs a `.elemod`
   with the user's stock file and sends it. The web page can build the
   upload as a `.syx` download for any SysEx tool, but it cannot send it
   itself (no Web MIDI, by its rules).
3. **Load at boot, with changes applied after a restart**, at the point S3
   finds. Entries go into spare slots before any reader's first read.
4. **Spare slots stay the linker change**: a collection declares
   `"spare": N`, and the linker writes N zero entries after its static ones
   (`link.py`, `table_len`; the same in `js/src`) and defines the capacity
   for the loader.
5. **`core_param_install`**: core-dn1 does for one record, at the load
   point, what `core_dn_boot` does in `.boot`.
6. **`ev_sysex` in core-dn1, at digihealth's site**, with core-dn2's
   contract (a MIDI-task observer that validates header, length and
   source); digihealth then subscribes to it. That is the core-first route
   in AGENTS.md, so offer the pull request.
7. **The base build holds the companions** (fxkit, machines-dn1,
   seq-events-dn1), and **the arena is TELOS's `.bss` in the mods' RAM**
   (512 KB holds all nine packages). Fixed bulk regions stay claims,
   checked when a package is switched on.
8. **Optional, from digihealth's other site:** an event at the context
   switch, which gives the browser a CPU-load figure per package.

## 6. Firmware work, in order (needs the stock files)

`python research/telos-dn1/tools/anchors.py <1.43.syx> <1.44.syx>` checks the
known sites and lists leads for S1, S2 and S5. Leads are not findings: read
the code they point to. digikit's emulator (`ELEKLOADER_DIGI_EMUD`,
docs/TESTS.md) runs the mk1 for S3 to S7.

| | question | how (digikit's method) | done when |
|---|---|---|---|
| S1 | **The card's block read and write**: arguments, alignment, lock, which tasks may call them | the working copy's writer (sectors 0x7a000/0x7c000) and the boot's read of it lead to them; eSDHC references (0xFC0CC000-0xFC0CC0FF); finding 36 (a) | addresses in 1.43 and 1.44, a read from the UI task checked in the emulator |
| S2 | **The card map**: every range the firmware reads or writes (project slots, the sound pool, the working copy, the temp area, anything else), the card's size, what the startup menu's FORMAT erases, and any SysEx request that reaches a sector by an index it does not check (on the DN2, the Data API reached everything below 0x800000) | finding 59; the slot routines from `projdata.s`'s sites | a map like digikit's `devices/digitone-ii.toml` `[card]`, and a store area above everything, or a decision to reserve a project slot |
| S3 | **The load point**: the boot thread and the main OS task; the earliest point where the block read works, before the views, the parameter tables' first read and the first tick | finding 36 §1 | one site in each version, and `core_dn_boot`'s install shown to work there |
| S4 | **SysEx limits**: the largest message and the pool, what the sender does when its ring is full (on the DT2 it spins), a count of bytes in flight, and whether the hook may block (it runs in the MIDI input task) | finding 42 §2, digikit-upload "Framing" | `ev_sysex`'s contract, and the largest DATA chunk |
| S5 | **Restart**: the RCM.RCR writes, the OS upgrade's restart and its drains, and what the mk1's bootstrap does with DDR word 0 | finding 36 §3, `mods/core-ar2/finish.s` | a sequence that reboots from the UI task in the emulator with the card idle |
| S6 | **Crash capture**: the default exception handler and its pointer in RAM | finding 46 | optional for v1 |
| S7 | **Code written at run time runs**: `cpushl` over the arena, then a call into it | finding 29's flush check | a test package's `init` runs, in the emulator |
| S8 | **Power loss**: whether the mk1 has the DT2's emergency write (a GPIO pin, then the boot record) | finding 36 §2 | what a power cut during a store write can leave |

## 7. Questions for you

1. **Stock files.** Allow `www.elektron.se` in this environment's network
   settings, or give a session the two files another way. They are never
   committed (`.gitignore`; AGENTS.md).
2. **Compatibility with digikit.** Speak its upload protocol so that one
   host tool serves the Digitakt II, the Digitone II and the mk1? Talk to
   its maintainer first? Either way the code is written again, because of
   the licences.
3. **The store's area**: the card's end or a reserved project slot. This
   waits on S2.
4. **digihealth**: move its SysEx hook into core-dn1 as `ev_sysex`, with a
   pull request?

## Sources

- elekloader: `mods/core-dn1` (README, `projdata.s`, `rows.s`, `params.s`,
  `pages.s`, `menu.s`, `audio.s`), `mods/core/core.s`, `mods/fxkit-dn1/fxkit.c`,
  `mods/machines-dn1/machines.c`, `mods/seq-events/seq.c`,
  `mods/core-ar2/finish.s`, `elekloader/devices/digitone_mk1.py`,
  docs/DEVICES.md, docs/FORMAT.md, docs/ADAPTING.md ("Remote panel input
  and SysEx (core-dn2 0.1, Digitone II)").
- digihealth (github.com/irpina/digihealth, GPL-2.0-or-later): `dn1/mod.json`,
  `dn1/dn143.inc`, `sysinfo.s` ("the USB diagnostics channel"), README.
- digikit (github.com/m-dwyer/digikit at `11ad79e`, AGPL-3.0-or-later): the
  four design documents above and findings 29, 36, 37, 42, 43, 46 and 59.
- `tools/`: `anchors.py` (`--selftest` passes), `fetch_dn1_mods.py`,
  `pack_survey.py`, `os_compare.py`.
