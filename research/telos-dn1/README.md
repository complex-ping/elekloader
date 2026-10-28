# TELOS for the Digitone mk1: research notes

Started 2026-10-28; S1-S8 filled in the same day. Working material for this
branch, not a doc: when TELOS is built, what holds here moves to its home
(docs/DEVICES.md, docs/FORMAT.md, docs/ADAPTING.md, the mods' READMEs), and
the rest goes into the pull request's description.

**What this is based on.** Static reading of the stock Digitone main OS,
1.43 and 1.44 (the user's own files, never stored here), checked in digiemu
0.4.1's emulator where a finding says so, and host-side measurement of the
released mods. Nothing was built for, or run on, a unit.
Marks in sections 1-5: **[E]** established for the mk1 elsewhere, with its
source; **[P]** prior art, measured by digikit on the Digitakt II and
Digitone II; **[M]** measured here with `tools/`; **[O]** open.
Marks in the short version and section 6: **[D]** read once, in the code,
here; **[V]** checked a second way (the same code found, addresses masked,
byte for byte in the other OS version, or run in the emulator);
**[O]** open, with what would settle it.

**Reading the addresses.** 1.44 is 1.43 with its code moved: unchanged below
about 0x40080000, +0x20 in 0x4008xxxx-0x400axxxx, +0x274 in
0x400exxxx-0x4010xxxx. Data moves too: +0x400 at 0x4022xxxx and in the
tables at 0x401cxxxx-0x4024xxxx (the card map's two tables differ, given
below), +0x1000 at 0x4052xxxx and above. Every 1.44 address below is a
function whose bytes match its 1.43 twin's, addresses masked, at the address
the rule gives.

## The short version

1. **digikit has already built this for the Digitakt II and Digitone II**: a
   runtime with `.dkm` modules, a module store on the card, loading at boot,
   a selection record with safe mode, APPLY & RESTART and upload over USB
   SysEx. TELOS for the mk1 takes its designs and speaks its upload
   protocol, so one host tool serves all three devices. The code is written
   again: digikit is AGPL-3.0-or-later, elekloader GPL-2.0-or-later.
2. **No blocker found.** Everything TELOS needs exists on the mk1 and is
   located in both OS versions: the card's block read, write and erase (S1),
   a place to load (S3), the SysEx router and sender (S4), a restart (S5), a
   crash hook (S6). The released mods still load at run time as they are
   (section 3).
3. **TELOS loads at a join in the init task**: 1.43 0x4008d27c, 1.44
   0x4008d29c, the six-byte store of the card's status word that every exit
   of the card bring-up reaches [V]. The card is ready there (in the
   emulator on 1.43: ready flag 1, 0x3B0000 sectors, mutex free), and
   nothing TELOS has to precede has run: the COKI recovery, the project
   load, the UI task, the views, the first tick (the order, in the emulator
   on both versions) [V]. The C++ constructors
   and the exception hook have run. `core_params` stays in `.boot`; a
   run-time `core_param_install` is still to build [O].
4. **The store goes above sector 0x180000, at an absolute address** (e.g.
   0x380000-0x39FFFF, 64 MiB). Every card access found is bounded below
   0x180000 (its indexes are clamped and its write refuses more), the RPC checks its ids, FORMAT erases
   only [0x1000,0x2000) and [0x80000,0x180000), and the smallest supported
   part ends at 0x3A4000 [D]. A project slot is not needed, and would show
   in Transfer and be erased by FORMAT. The area is not "the card's end":
   the three parts the firmware accepts end in different places
   ([card-map.md](card-map.md)). Your decision: section 7.
5. **Risks that change the design**
   - **Power loss.** The unit has a supercap and an emergency save (S8): a
     power-fail interrupt re-initialises the eMMC from scratch and writes
     the 2.78 MB project image to the other COKI copy. A TELOS write in
     flight is abandoned between any two sectors, so the store needs
     digikit's two headers with a CRC and a last-written commit.
   - **Restart.** Both stock restarts that wait for the panel link mask
     every interrupt first and then spin on counts only an interrupt moves
     (generic reset, OS-upgrade finish): they can hang for ever, the fault
     core-ar2's `finish.s` fixes. TELOS drains with interrupts on (S5).
   - **A SysEx reply can freeze the UI.** The USB sender busy-waits, with no
     timeout, in the MIDI task (priority 7, above the UI's 6) while its
     4,096-byte ring is full. Replies stay small (stock sends 144 bytes per
     call) and carry the abort byte (S4).
   - **A crash halts the unit.** Stock prints a dump on the panel link and
     stops; there is no reset. Safe mode needs TELOS's own hook (S6).
   - **Code written at run time** has no stock precedent: the firmware only
     ever flushes the data cache over DMA buffers (S7) [O].
6. **Open**: the largest SysEx the unit receives (S4), the cache flush and
   the restart on a unit (S5, S7), the power-fail path (S8), and the loader
   itself.
7. **Changes to the outline** (section 5): item 3 names the join; add the
   store's area, the restart sequence and the crash hook as core-dn1
   features (core first: AGENTS.md); question 1 is answered.

## 1. Prior art: digikit's runtime

digikit at `11ad79e` (github.com/m-dwyer/digikit): `docs/design/digikit-runtime.md`,
`digikit-dkm.md`, `digikit-store.md`, `digikit-upload.md`, and findings 36,
37, 42, 43 and 59. Every result there is from its emulator; nothing has run
on a device.

| concern | digikit's answer [P] | for the mk1 |
|---|---|---|
| module file | `.dkm`: a 0xa0-byte header (`DKMF`, a sha256 of the whole file, contract versions), the image linked at base 0, `.bss` size, imports as 32-byte names, relocations `u32 offset, kind << 24 \| index` (kind 0: add the load address; kind 1: add the import's address), PC-relative references between the module's own sections resolved by the packer and to an import refused | the same, plus elekloader's contributions and claims. Its 64 KiB file limit is too small: digiprism's `.run` is 71,625 bytes |
| a module's needs | inferred from the file by one function (`needs_of`), never declared by the module | the same |
| the store | raw card sectors that no firmware structure uses; two header copies (`seq`, CRC); a table streamed through small buffers; first fit; a settings record; the selection record in the last two sectors; one card-write function that checks the range and writes only to an area that is "ours or blank" | the same design; the area is above sector 0x180000 (S2) |
| upload | `F0 7D 64 6B 01 <cmd> <seq> <8-in-7 body> F7`; INFO, LIST, BEGIN, DATA, END, VERIFY, DELETE, CLEAR, SELECT, RESTART, LOG; a reply only when the firmware's TX ring has room | the same protocol: a hook at the router's entry sees `F0 7D` messages (only `F0 7E` skips it); the sender spins when full (S4) |
| when modules load | at boot, at the main OS task's entry, before the main screen view is built; the set is fixed for a session; no unload | the same; the mk1's point is the join (S3) |
| safe mode | a boot counter in the selection record, raised before loading and cleared after 30 s stable; a flag for one safe boot; a key at power-on (untested on a device) | the same |
| restart | refuse during a card transfer (eSDHC present state, 0xFC0CC024 bits 0-2); mask interrupts; clear DDR word 0 through the uncached alias (0x48000000); `0x80` to RCM.RCR (0xEC090000) | chip-level, so the same; the mk1's own steps are S5 |
| code written at run time | `cpushl bc` on each 16-byte line before it runs | the same chip (S7) |
| crashes | the default exception handler's function pointer in RAM -> record and soft reset | S6 |

## 2. What is known on the mk1

| what | 1.43 | 1.44 | source |
|---|---|---|---|
| SysEx router entry: every F0 message except F0 7E, in the MIDI input task with no lock held; a0 = a1 = the message (F0 to F7), d1 its length, d2 its source (2 or 4 USB, 8 or 0x10 DIN). Past the hook the router routes only device bytes 0x00-0x17 (digihealth's comment, written for the Digitakt mk1's same routine). | 0x400ea880, `lea 0x401cde1a,a3` | 0x400eaaf4, `lea 0x401ce1ea,a3` | [E] digihealth `dn1/mod.json`, `sysinfo.s`; on 1.44 in the emulator, a HELLO marked as from USB was answered |
| SysEx sender `bool (buf, len, abort*, port)`, port 2 = USB | 0x400ee354 | 0x400ee5c8 | [E] digihealth; S4 |
| RTOS context switch | 0x40000438 | the same | [E] digihealth (idle time, CPU load) |
| the project's serializer entry; deserialize in a slot's load; the load of a block already in RAM (at boot, and from SysEx); the new project; the template copy | 0x4000eb10, 0x400acc40, 0x40010cfa, 0x4001570e, 0x400acbf4 | `ports` | [E] `mods/core-dn1/projdata.s` |
| project block: `projectStorage_v14_t`, 2,782,212 bytes, magic 0xBEEFBACE; a 480-byte gap at +0x20 that the OS neither fills nor reads | | | [E] core-dn1 |
| the working copy for power-up: two COKI copies at sectors 0x7a000 and 0x7c000, read back at boot, written by the power-fail handler | | | [E] `projdata.s`; S2, S8 |
| project slots: 4 MB each, 128 of them from sector 0x80000; the working project in slot 128 at 0x7e000; every sector range the firmware uses | | | [D] [card-map.md](card-map.md) |
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

| table | read | an entry added at the join works |
|---|---|---|
| the 15 events | on each call | yes |
| `core_menu` | each time the menu opens (`menu.s`) | yes |
| `core_audio` | each block (`audio.s`) | yes |
| `core_projdata` | each save and load | yes (the space is the problem) |
| `core_param_override` | each lookup | yes |
| `dn_machines`, seq-events' `ev_fire_*` | each use | yes |
| `core_pages` | the record lookup on each use; the key lists when the views are built (`pages.s`): in the UI task, after the join (S3) | yes |
| `core_params` | once, in `.boot`, before the OS runs (`core_dn_boot`, `params.s`) | no: core needs a run-time install call |
| `fx_effects` | once, at fxkit's first `ev_tick` (`fxkit.c`, `ready`): after the join | yes |

So TELOS loads at the earliest point where the card can be read, which is
before the views and the first tick (S3). That is digikit's choice for the
same reason: the machine view reads its list once.

## 5. Changes to the outline

1. **Adopt digikit's module, store and upload designs, and stay
   compatible with its protocol.** The package becomes `.dkm` with
   elekloader's contributions and claims added, and a staging buffer larger
   than 64 KiB.
2. **Pack locally.** `elekloader` (and the TypeScript port) packs a `.elemod`
   with the user's stock file and sends it. The web page can build the
   upload as a `.syx` download for any SysEx tool, but it cannot send it
   itself (no Web MIDI, by its rules).
3. **Load at the join** (S3: 1.43 0x4008d27c, 1.44 0x4008d29c), with changes
   applied after a restart. Entries go into spare slots before any reader's
   first read.
4. **Spare slots stay the linker change**: a collection declares
   `"spare": N`, and the linker writes N zero entries after its static ones
   (`link.py`, `table_len`; the same in `js/src`) and defines the capacity
   for the loader.
5. **`core_param_install`**: core-dn1 does for one record, at the join, what
   `core_dn_boot` does in `.boot`.
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
9. **New, from section 6:** core-dn1 gets (a) an event at the join
   (`ev_card_ready`: the card is up, nothing else is), (b) `core_restart`,
   the S5 sequence, (c) a crash hook that records and restarts (S6), and
   (d) a card-write function that accepts only the store's area (S2), after
   digikit's. Offer them as pull requests (core first).

## 6. Findings, S1-S8

`python research/telos-dn1/tools/anchors.py <1.43.syx> <1.44.syx>` was the
start; `m68k-linux-gnu-objdump -m m68k:cfv4e` read the code (the
`-m68k:54455` name is not in this binutils; `cfv4e` decodes the same
instructions). Chain of the card map: [card-map.md](card-map.md).
"Same" means the same address in both.

### S1. The card's block read and write

| | 1.43 | 1.44 |
|---|---|---|
| `int rd(u32 sector, u32 bytes, void *buf)` | 0x400f5860 | 0x400f5ad4 |
| `int wr(u32 sector, u32 bytes, void *buf)` | 0x400f5a46 | 0x400f5cba |
| `int er(u32 first, u32 count)` (eMMC erase) | 0x400f5778 | 0x400f59ec |
| sector count, -1 if not ready | 0x400f5480 | 0x400f56f4 |
| ready flag / sector count / address multiplier | 0x419c92a8 / 92cc / 92d0 | 0x419ca2a8 / a2cc / a2d0 |
| card mutex / activity mutex | 0x4321ef24 / 0x419ce750 | 0x4321ff24 / 0x419cf750 |
| activity bracket `enter(0)` / `leave()` | 0x400f9918 / 0x400f99ac | 0x400f9b8c / 0x400f9c20 |
| chunking wrappers (32 KB) `rd`, `wr` | 0x400a78cc, 0x400a791e | 0x400a78ec, 0x400a793e |

- **[V]** The arguments are the sector (512 B), the byte count and the
  buffer, pushed in reverse. A count over 64 KiB is split by recursion. No
  alignment is asked of the caller: a buffer that is not 16-byte aligned,
  or a count that is not a multiple of 512, goes through a 64 KiB bounce
  buffer at 0x43E37280 (the code names it by its uncached alias, 0x4BE37280)
  with a copy; an aligned buffer gets a `cpushl dc` over its lines, then DMA.
- **[V]** Returns: 0; `rd`: -1 (not ready, sector past the end, DMA fail);
  `wr`: -1, -10 (sector < 0 or past the end), -11 (null buffer), -12
  (negative count), -13 (range past the end), -2 (the transfer failed);
  count 0 returns 0. The multiplier is 1 on a sector-addressed part (all
  three in the part table are) and 512 on a byte-addressed one.
- **[V]** Locks and blocking: every call takes two owner-recursive RTOS
  mutexes (the card's, and the activity one that also drives the card LED
  pin 0xEC09404E) and waits on RTOS semaphores (0x419c92e4/ec/f4) with no
  timeout. So: from a task only, with interrupts enabled; never from an
  interrupt; recursion from a task that holds them is fine.
- **[V]** Which tasks call them: in the emulator on 1.43, the init task
  (prio 1, the COKI recovery at 0x400a7902), the UI task (prio 6, store
  reads at 0x400f62fe) and the project-save task (prio 2, entry 0x400a89a6,
  store writes at 0x400f743e) all do, so any task may.
- **[D]** `er` is the eMMC erase command sequence (35, 36, 38) over
  [first, first+count). The store's own entry to it, 0x400f6336 (1.44
  0x400f65aa), refuses start 0 unless told otherwise.

### S2. The card map

Complete in [card-map.md](card-map.md). The short answer:

- **[D]** The firmware's own accesses are the store (sector 0 header, the
  bitmap at 0x800, 2,048 sounds at 0x1000-0x1FFF, project slots 0-127 from
  0x80000, the working project at 0x7e000), and the two COKI copies at
  0x7a000 and 0x7c000. The store computes every address from a clamped
  index (slot 0-127, or 128 for the working project: 0x400f68c2; sound
  0-2047: 0x400f6db6; 1.44 0x400f6b36, 0x400f702a), and its write entry
  0x400f739e (1.44 0x400f7612) refuses a sector above 0x17FFFF and a range
  past 768 MiB. Its read (0x400f62ac, 1.44 0x400f6520) and erase
  (0x400f6336, 1.44 0x400f65aa) leave the bound to the card layer, which
  knows only the card's size.
- **[D]** Three legacy COKI probes at sectors 0x400000, 0x800000 and
  0xC00000 (table at 0x401c2a84, 1.44 0x401c2dd4) exist on the 23 GiB part
  only; they read four bytes and erase the sector if it begins with `COKI`.
  The store's area below holds none of them.
- **[D]** What FORMAT erases (`PlusDriveFormatMenuView`'s worker,
  the call is at 0x4006bca2; the store's format 0x400f8132, 1.44 0x400f83a6): the project
  slots [0x80000,0x180000) in 1 MiB steps, the sounds [0x1000,0x2000), then
  it rewrites sector 0 and the bitmap. It does not touch 0x7a000-0x7ffff or
  anything above 0x180000.
- **[D]** The card's size is the part's EXT_CSD SEC_COUNT, checked against a
  table of seven entries (0x40240410, 1.44 0x40240810; check 0x400f5566, 1.44
  0x400f57da) by manufacturer, parameters and size. Each entry holds two
  accepted sizes, chosen by a mode byte of the EXT_CSD: the SLC-mode size
  3,866,624 sectors (four entries), 3,817,472, or 48,340,992, and a larger
  one in the other mode (7,733,248, 7,634,944, 61,579,264). A part that does
  not fit leaves a nonzero status in the word the join stores (6 or 7: not
  in the table; 8: its parameters differ; 9: neither size) and the card
  still works. The smallest size is 3,817,472 sectors (0x3A4000).
- **[D]** Requests that reach a sector by an index: the RPC paths are
  `/projects[/*[/.metadata]]` and `/soundbanks[/*[/*[/.metadata]]]`, and only
  those. `/projects/n` accepts 1-128 (0 is refused: "Active project not
  supported"; handler 0x400a2f54, 1.44 0x400a2f74); `/soundbanks/b/n` takes
  a bank id that must resolve (0x400a4116) and n-1 <= 255 (handler
  0x400a4a76, 1.44 0x400a4a96). So no unchecked index was found in the RPC.
  **[O]** A factory-test console (task entry 0x4008b440, 1.44 0x4008b460; a
  `read` command takes any sector) exists; it starts only on a boot-argument
  bit (0x20) the bootstrap passes in, and nothing in the SysEx or MIDI code
  reaches it. What would settle it: the bootstrap's setting of that bit
  (section 2 of the .syx).
- **Store area (S2's "done when").** Sectors 0x380000-0x39FFFF (64 MiB; 1792
  to 1856 MiB). Free on every part (the smallest ends at 0x3A4000), outside
  everything above, and not in the RPC's reach. Not a project slot.

### S3. The load point

- **[V]** Boot chain, 1.43 (1.44 in brackets): the entry 0x400004e8 sets SP
  to 0x48000000, points all 256 vectors at the default handler, copies
  `.data` to the SRAM at 0x80000000, clears bss and sets CACR and ACR0, then
  0x40144ff6 starts the RTOS (task 0: prio 0, entry 0x4008cda2 [0x4008cdc2],
  16 KB stack) which starts the **init task** (prio 1, entry 0x4008cee0
  [0x4008cf00], 16 KB stack, tcb 0x4137365c). The init task: pins, the C++ constructors (the table at 0x4028d63c: count, then
  pointers; run at 0x4008d07c-0x4008d09c), the crash hook (0x400e786e),
  timers, **the card** (0x4008d22c: reset and identify 0x400f4d9c, bus
  0x400f4e12, DMA 0x400f541e, identity and size check 0x400f5606), **the
  join**, the COKI recovery 0x400a8340, the project load 0x40084ed4
  (called at 0x4008d2e0), two engine initialisers (0x4009d0b8, 0x4009512a), the
  MIDI system (0x400eb172: the MIDI task, prio 7, entry 0x400e990a) and the
  UI spawner 0x40019444, which creates the **UI task** (prio 6, entry
  0x40019852, 160 KB stack, same in both). The init task then idles.
- **[V]** The join: 1.43 0x4008d27c, bytes `23 c0 41 36 f5 f8`; 1.44
  0x4008d29c, `23 c0 41 37 05 f8`: `move.l d0, <card status word>` (0x4136f5f8,
  1.44 0x413705f8). Every exit of the card bring-up reaches it with d0 = 0
  (ready) or 1, 6, 7, 8, 9 (the failures); nothing bypasses it. It is six
  bytes, so a `jsr` replaces it; core's routine stores d0 itself, and loads
  only if d0 is 0.
- **[V]** In the emulator (digiemu 0.4.1's first run, a hook on each stage),
  on 1.43 and 1.44, the order is: init task, card bring-up, **the join**,
  COKI recovery, project load, UI task created, UI task running, then in the
  UI task the first VIEW_INIT (0x40044490, 18 calls), the first UI tick
  (0x4001900c) and the first draw (0x40019072). At the join on 1.43: ready
  flag 1, 0x3B0000 sectors, multiplier 1, status 0, card mutex free.
- **[D]** What the join precedes, and why that matters: the COKI recovery
  and project load read the 480-byte gap through `core_proj_load`, so a
  package's project tags are in place before the project loads. What it
  follows: the C++ constructors (they build the UI records for parameter
  ids 0-181; core builds ids 182-184 itself) and the crash hook.
- **[D]** Not earlier: before the card bring-up the ready flag (set at
  0x400f539a in the bring-up, 1.44 0x400f560e) is 0 and every call
  returns -1. Not heap: the join runs in the init task with the heap up, but
  a buffer kept from it can stall a first boot (section 2), so the arena is
  static.
- **[O]** `core_dn_boot`'s install shown to work there. Settles: building
  `core_param_install` and running it in the emulator.

### S4. SysEx

- **[V]** The router: `route(msg, len, src)` at 0x400ea854 (1.44 0x400eaac8);
  `F0 7E` first, then the 4-byte header `F0 00 20 3C` (0x401cde1a, 1.44
  0x401ce1ea), a device byte <= 0x17, a table of handlers (0x4023fc84, 1.44
  0x40240084) with a sub-id and a message-type range. Digihealth's hook
  (0x400ea880, 1.44 0x400eaaf4) is inside it, after the `F0 7E` test.
- **[D]** Its caller: the system-message dispatcher 0x400e954c (1.44
  0x400e97c0) indexes a 16-entry table at 0x401cde20 by the status byte's low
  nibble; entry 0 is the router. It is called from the **MIDI task** (prio 7,
  32 KB stack, entry 0x400e990a, 1.44 0x400e9b7e), which waits on three
  queues of 256 items (0x4199ef04, ef24, ef44; 1.44 0x4199ff04, ff24, ff44),
  each item `{length, pointer}`. The message arrives whole.
- **[O]** The largest message the unit accepts. The firmware has a setting
  key `midi.sysex_size_max` and another, `midi.rpc_file_chunk_size_max`
  (registered at 0x4014b896 and 0x4014b8a6 as string objects 0x40553ee0 and
  0x40553edc); their values were not found. What would settle it: the
  producer of the three queues (the DIN parser, vector 156, and the USB OUT
  path), or sending dumps of growing length into the emulator's DIN ring
  (`emu/midi.py`) with a hook at 0x400ea854. The DT2 note of a 64 KiB
  staging buffer applies to a file, not to one message.
- **[V]** The sender `send(buf, len, abort*, port)` at 0x400ee354 (1.44
  0x400ee5c8): returns without sending if the MIDI system is off
  ([0x419efde0] = 0) or `*abort` is set; otherwise sends **144 bytes at a
  time** through 0x400edf32 (1.44 0x400ee1a6) and returns `!*abort`. Port 2 is
  USB, 3 DIN, 4 both, 1 the current default.
- **[V]** The USB branch, 0x40004bcc (same in 1.44): under the USB lock
  0x429f1290 it drops the bytes silently unless the bus is configured
  ([0x40528244] = 1, 1.44 0x40529244); then it packs three bytes to a
  four-byte USB-MIDI packet into a 4,096-byte ring at 0x43C0C000, adds to
  the **bytes-in-flight word [0x40528248]** (1.44 0x40529248; in ring bytes,
  four per packet; the USB completion subtracts) and **busy-waits, with no
  yield and no timeout, while that word plus what this call has packed
  exceeds 4,091.** The DIN branch is
  0x40002a3a (a 4 KB ring and DMA, `emu/midi.py`).
- **[D]** So a hook at the router may block, but it blocks the MIDI task
  (every MIDI input waits; at priority 7 it runs over the UI task). Reply in
  pieces of at most 144 bytes, check the in-flight word first, pass an
  abort byte. `ev_sysex`'s contract: runs in the MIDI task, may not block
  for a USB host that is not reading.

### S5. Restart

| path | 1.43 | 1.44 | masks IRQs, then drains | clears word 0 |
|---|---|---|---|---|
| generic `reset()` | 0x40005aa0 | same | yes (0x40005aa4) | yes (0x40005aac) |
| OS-upgrade finish (3 s, drain) | 0x401001cc | 0x40100440 | yes (0x401002b8; 1.44 0x4010052c) | yes |
| pre-reset handler (power-fail ISR A) | 0x400a7988 | 0x400a79a8 | no | yes (0x400a7a06) |
| emergency save, then reset (ISR B) | 0x400a7b6e | 0x400a7b8e | no | yes (0x400a8024) |
| power-fail pin ISR | 0x40001e54 | same | no | yes (0x40001ec0) |
| console reboot (magic) | 0x4008c0be | 0x4008c0de | no | writes 0xB0B0DADA |

- **[V]** Every path writes DDR word 0 through the uncached alias (a `movel`
  to 0x48000000), then 27 NOPs, then `moveb #0x80,0xEC090000` (RCM.RCR) and
  loops (the upgrade finish has the NOPs after its drain). Why 27 NOPs
  (presumably to let the uncached store land) was not measured [O].
- **[V]** The drain, 0x4000281e (same in 1.44): while [0x405275fc] > 0 (bytes
  behind the panel link's DMA) or [0x4052760c] > 0 (the UART queue) spin, then
  wait for EPORT 0xEC070004 bit 3. 1.44: 0x405285fc, 0x4052860c. Both of its
  stock callers mask interrupts first, so the two counts, which only
  interrupts move, can stay above 0 for ever: the Digitone has the Analog
  Rytm's hang (`mods/core-ar2/finish.s`). The emulator does not model
  RCM.RCR, so the hang is read, not run [O].
- **[V]** The bootstrap (sections 2 and 4 of the .syx, decoded and identical
  in 1.43 and 1.44) uses DDR word 0 once: if bit 5 of the byte at 0xEC090001
  (RCM.RSR) is set and word 0 is 0xB0B0DADA, it sets a console flag.
  Nothing else reads it; its own restart paths clear it. So word 0 holds no
  TELOS state: the OS clears it, and the bootstrap only compares it.
- **[D]** TELOS's sequence, from the UI task: refuse while a card transfer
  runs (eSDHC present state 0xFC0CC024 bits 0-2, or the card mutex held);
  with interrupts on, spin until both counts are 0; mask (`move #0x2700,sr`);
  clear word 0 through 0x48000000; the NOPs; RCR. The same as core-ar2's
  `core_finish_drain`, with 1.43's addresses (FINISH_PANEL 0x405275fc,
  FINISH_UART 0x4052760c, FINISH_DRAIN 0x4000281e). **[O]** Settles on a
  unit: it reboots with the card idle and with a transfer.

### S6. The default exception handler

- **[V]** All 256 vectors start at 0x40001696 (VBR 0x40000000, set at
  0x40001df4 from the entry; the RTOS and the drivers then install their own
  interrupt and trap vectors). It masks interrupts, saves SP at
  [0x40527188], and if the **RAM pointer [0x40527184]** (1.44 0x40528184;
  setter 0x40001dca, same) is not 0, calls it with the frame pointer on the
  stack; then `rte`, to the frame's PC, which for an access error is the
  faulting instruction [O: not run]. A hook for a fault must not return.
- **[D]** The init task installs 0x400e77a6 (1.44 0x400e7a1a) at 0x400e786e,
  before the join. It prints the fault status, the PC and twelve more words of the
  frame through the panel-text sender 0x400e7714 and executes `halt`
  (0x400e7848): the unit stops
  and needs the power switch. TELOS overwrites the pointer after its own load
  with a routine that records the frame in its settings record and restarts
  (S5). A fault inside a mod is therefore a hang, not a reboot, unless TELOS
  hooks it.

### S7. Code written at run time

- **[V]** The entry sets CACR = 0xA50CE100 (bit 31, enable, set) and ACR0 =
  0x4007E020 (base 0x40, mask 0x07: all of 0x40000000-0x47FFFFFF cacheable;
  0x48000000 and above outside it, so uncached: the DMA buffers' and word 0's
  alias) at 0x4000055a and 0x40000564. No other `movec` to CACR or ACRn, none
  to MMUBAR; the OS never turns on the MMU, so nothing stops code written to
  RAM from running [D].
- **[D]** The firmware's only flush is `cpushl dc,(a0)` over a buffer in
  16-byte steps before a DMA (0x400f5928 and 0x400f5b3a; 1.44 0x400f5b9c and
  0x400f5dae; an unrolled copy at 0x40006c24). It never flushes the
  instruction cache. The other `cpushl bc`/`ic` hits are data tables.
- **[O]** So loaded code needs `cpushl bc,(a0)` over its lines (digikit's
  finding for the same core) and a barrier before the call; that this is
  enough on a unit is not shown. The emulator has no cache model, so it can
  not show it either. What settles it: a test package on a unit that runs
  code written after the I-cache has seen older bytes there.

### S8. Power loss

- **[D]** The mk1 has an emergency write path, like the Digitakt II. At the
  end of the card bring-up 0x400a8340 stores two callbacks
  ([0x40527594] = 0x400a7988, [0x40527598] = 0x400a7b6e; 1.44 0x40528594,
  0x40528598) through 0x400020fa and 0x40002106. The ISR 0x40001e54 (vector
  66, 0x40000108) reads GPIO 0xEC09401A bit 2. Set: it raises the power-fail
  flag [0x402292fc] and forces INTC1 source 29 (handler 0x40001e00, callback
  A: clear word 0 and reset, unless a save is running). Clear: it clears the
  flag, reconfigures clocks and pins and forces source 30 (0x40001e2a,
  callback B: the save). Both forces go through INTFRCL, 0xFC050014. Which
  level is the supply failing was not told apart [O]. The "armed" byte
  [0x4139320c] (1.44 0x4139420c) is set by 0x400a8428 from a periodic function
  (0x4008cdd2, 1.44 0x4008cdf2) while GPIO 0xEC09401E bit 3 is set.
- **[D]** Callback B (0x400a7b6e) sets [0x41393208], reinitialises the eMMC
  from scratch (CMD1, 2, 3, 7, 6, 16 with their arguments; clocks and GPIO
  0xEC09404E), and, if armed, writes the project image (2,782,484 bytes at
  0x40554ee4, 1.44 0x40555ee4) in 32 KB pieces to the COKI copy that is not
  the current one (target index [0x402365e0], current [0x402365e4]; 1.44
  0x402369e0/e4), then clears the first byte of the old copy. It
  runs in an interrupt with its own polling and does not take the card
  mutex. It resets only if the power-fail flag is set (0x40002112 reads it).
- **[D]** What a power cut during a store write can leave: the write in
  flight is stopped at an arbitrary sector and the eMMC is reset under it.
  The stock copies are written header first and the old copy is invalidated
  last; whether a torn new copy is detected (0x4008523a checks the 256-byte
  header; whether it covers the body was not read) is [O]. A TELOS store must
  survive the same: two headers
  with a sequence and a CRC, the header written last, payload sectors never
  reused in place. **[O]** The supercap's holdup time, and whether the
  emergency save still finishes with a TELOS transfer in flight: a unit.

## 7. Questions for you

1. **Compatibility with digikit.** Speak its upload protocol so that one
   host tool serves the Digitakt II, the Digitone II and the mk1? Talk to
   its maintainer first? Either way the code is written again, because of
   the licences.
2. **The store's area.** Sectors 0x380000-0x39FFFF (64 MiB), absolute, on
   every part (S2). Or smaller (the loader needs under 1 MiB for ten
   packages), or a different place? A project slot is the alternative and is
   worse.
3. **digihealth**: move its SysEx hook into core-dn1 as `ev_sysex`, with a
   pull request?
4. **Core features** (section 5, item 9): `ev_card_ready` at the join,
   `core_restart`, the crash hook, the store-bounded write. Offer them as
   pull requests to elekloader, ahead of TELOS?

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
- The stock Digitone and Digitone Keys OS 1.43 and 1.44 (Elektron's files,
  not stored) and digiemu 0.4.1's emulator (`emu.bootstrap.first_run` with an
  `on_machine` hook), for section 6.
- `tools/`: `anchors.py` (`--selftest` passes), `fetch_dn1_mods.py`,
  `pack_survey.py`, `os_compare.py`.
