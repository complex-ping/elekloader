# core (Digitone II)

The boot copier and the hook bus for Digitone II OS 1.12. Every set of
linkable (format 2) mods for the Digitone II needs it. On its own it changes
nothing the unit does.

It builds the shared [../core/core.s](../core/core.s) and
[settings.s](../core/settings.s) with the Digitone II's addresses
(`mod.json`), and the Digitakt II's
[../core-dt2/personalize.s](../core-dt2/personalize.s) unchanged: the
PERSONALIZE builder ends the same way. There is no `fast.s` (SRAM is not
qualified, so there is no `.fast` area) and no `render.s` (the audio renders
on the DSP).

Core **0.2** adds the bridge for SHARC mods and keeps 0.1's control-side
sites, tables and exports. A SHARC mod sets `resources.core: "0.2"`; mods
made for 0.1 can keep asking for 0.1. The [machines-dn2](../machines-dn2)
companion adds selectable synths and filters on top of it
([MACHINES-DN2.md](../../docs/MACHINES-DN2.md)).

Core **0.3** adds mod storage in each sound (`store.c`, `store.s`;
[docs/ADAPTING.md](../../docs/ADAPTING.md), "Mod storage (core-dn2 0.3,
Digitone II)") and keeps 0.2's sites, tables and exports. Two more sites
jump from the sound converters' first eight bytes (`lea`, `movem.l`, both
replayed in `store.s`) to core's wraps:

| site | converter | core |
|---|---|---|
| 0x400dd80a | save (stored, live, a) | `st_sound_save`: stock's body, then the layout ID, the sound generation and the 11 bytes into the stored values of lock ids 4, 8 .. 32 |
| 0x400dd34e | load (live, stored) | `st_sound_load`: stock's body, then the 11 bytes (zeros for another layout or generation), and the live value slot 0, where stock's load folds those ids, back to 0 |

Core **0.4** adds mod parameters (`params.c`, `params.s`;
[docs/ADAPTING.md](../../docs/ADAPTING.md), "Mod parameters (core-dn2 0.4,
Digitone II)") and keeps 0.3's sites, tables and exports. It moves the
parameter table (321 rows of 60 bytes at 0x401f83a4) into `core_rows` at
boot: its 56 references point at the copy and its 54 row bounds take its
389 rows (321 stock, 41 machines-dn2's, 27 mod parameters'). Seven more
sites:

| site | stock | core |
|---|---|---|
| 0x40000568 | the OS entry's call of main | `core_params_start`: the rows copied and the mods' built, then main |
| 0x400c257c | the value-class accessor | `core_value_class_stub`: an added row's class is its template's |
| 0x400c25d8 | the page lookup (ids 0..36) | `core_page_lookup`: ids 37.. are the mods' pages |
| 0x400614e2 | [FX]'s page list ({16}) | `core_fx_pages_stub`: {16, 37, 38, ...} |
| 0x4005cc0e | the lock indicator's copy of it | `core_fx_lock_stub`: the same list |
| 0x4003717c | the value getter's bound (slot <= 100) | `core_get_stub`: slots 101..127 read the parameter's field |
| 0x40037bd0 | the value setter's bound | `core_set_stub`: slots 101..127 write it |

P-locks of mod parameters (`locks.c`, `locks.s`) are records of the
pattern's lock table like stock's, `{code, track}`, without the index byte
stock keeps for slots 0..100, and with the value itself (not 8.8). Saved, a
record is `{code, track | 0x20}`: a stock OS's load skips it. Core takes:

| site | stock | core |
|---|---|---|
| 0x4003ceee, 0x4003cdba, 0x4003d00e | the lock recorder, reader and remover | codes 101..127 to `core_lock_record`, `_read`, `_remove` |
| 0x4003d266 | the step clear's index byte of a freed record | skipped for a code |
| 0x4003d5a2, 0x4003d5f4 | the track swap's record copy: its index byte look and write | skipped for a code |
| 0x40052634, 0x40053246, 0x4005389c, 0x40052948 | the page and track pastes' record match | a code's record pasted by core, passed over by stock |
| 0x40053f48, 0x40055440 | trig copy and paste | the step's codes kept by clipboard entry |
| 0x400de8c2, 0x400de6f8 | pattern save (slot -> id) and load (id -> slot) | `{code, track | 0x20}` |
| 0x400d874c, 0x400d8b14 | the lock-list builder, and the test that skips it on a step without stock locks | the step's codes into the list's last 40 bytes; run for every note |
| 0x40026c20 | the note path's lock step (a voice reset to its sound) | the voice's and its track's playing codes |

`core_store_set` also writes the field's sound into its stored copy in the
project image the OS keeps in RAM (what SAVE PROJECT writes and a power
cycle brings back), with the layout ID and the generation, as stock writes
each parameter edit there. The live and the stored sounds are arrays, read
from the boot's converter calls, which pair every one:

| | live | stored |
|---|---|---|
| kits (the working kit, then the 128), 16 sounds each | 0x4210d0c0 + 23921 kit + 1163 track | 0x410aeba8 + 10752 kit + 359 track |
| pool, 128 sounds | 0x4240dbcb + 1163 i | 0x41201570 + 359 i |

What the store relies on, checked in an emulator on stock 1.12: stock reads
or writes a live sound only up to +0x176 (the arpeggiator's offsets end at
+0x175), playing with the arpeggiator on, through every page's knobs,
machine changes and both converters; initialising a sound (0x400e7fca)
clears the bytes after it; a stock save leaves the 16 stored bytes 0, and
a load of nonzero ones changes only the live value slot 0. On a unit,
with store-check: the markers of all 16 tracks are back after a power cycle
without a save, read over USB.

`bridge.sharc.json` is the bridge, already compiled and linked from
`sharc-bridge.s`, with the slot and pitch reader the
[sine example](../../examples/sharc-sine/README.md) relies on. The linker
adds it, and replaces the six-byte stock instruction that runs it, only for
a SHARC mod with callbacks that passes the
[checks](../../docs/FORMAT.md#digitone-ii-sharc-mod). The device profile pins
the bridge's bytes; a mod's code and callback offsets may change. With core
alone, or with an inert SHARC mod, the DSP's audio stays stock. Building
this core needs only the ColdFire tools, no SHARC compiler.

- **At boot** (the OS entry's call at 0x40000538, which called 0x400019b0,
  the Digitakt II's: it sets the VBR to 0x40000000 and returns), `boot`
  copies the RAM image the linker built to the device's DDR area
  (0x47F00000), zeroes `.bss`, and goes on to the call it replaced, before
  the OS copies its data to SRAM and clears its `.bss`. It does not start
  DTIM0 (`NO_DTIM0`): as on the Digitakt II, the OS reads its counter at ten
  sites (0xfc07000c) and never writes its mode register, so stock reads it
  as stopped, and this core keeps that.
- **The sites** become the Digitakt II's events, with the same prototypes
  ([docs/ADAPTING.md](../../docs/ADAPTING.md), "The hook bus"):

  | site | was | event |
  |---|---|---|
  | 0x4002e464 | `jsr 0x4011d5d4`: returns the view controller's byte +0x20 | `ev_tick` |
  | 0x4002e4ca | `jsr 0x4011d60a`: `drawAll(ctrl, bmp)`, the 128x64 frame | `ev_draw` |
  | 0x4002f20c | `jsr 0x4002c5c8`: `Brain::key(brain, event)` | `ev_key` |
  | 0x4002f256 | `jsr 0x4002c55c`: `Brain::enc(brain, event)` | `ev_enc` |
  | 0x4009ff88 | `movea.l (a2),a0; pea 2.w`, the SETTINGS builder 0x4009fbe0 going on at 0x4009ff8e | `ev_settings` |
  | 0x400987a2 | `movem.l $18(a7),d2-d6/a2-a6`, the end of the PERSONALIZE builder 0x40097eac, after NOTE EDIT MENU | `ev_personalize` |

  `core_additem(menu, row)` adds a row with the firmware's routines:
  `operator new` 0x40120544, `MenuItem`'s constructor 0x401173b8,
  `Menu::addItem` 0x40116bee and the `std::function` manager 0x4018de3e
  (for `PFSs15FileStorageInfoE`, as on the Digitakt II). A row's label
  callback builds a `std::string` with 0x401ceab8, as the firmware's own
  labels do (0x4002b294). In SETTINGS the row comes after SYSTEM; in
  PERSONALIZE after NOTE EDIT MENU.

Each address was found by the Digitakt II 1.17's code, with the addresses
masked, then checked in the Digitone II's own, with what the shared
sources assume about it:
- the UI loop around the tick, draw, key and encoder sites is the
  Digitakt II's instruction for instruction. The tick routine is
  `move.l 4(sp),a0; move.b $20(a0),d0; rts`, what `core_tick` does after
  its handlers. The draw site pushes the frame's Bitmap and then the view
  controller; the key and encoder sites push the event and then the brain.
  Each routine is its site's only caller. `Brain::key` hands the event to
  the listeners at brain +0x94..+0x98, as on the Digitakt II.
- the SETTINGS builder keeps the menu in a2 and its site is the two
  instructions `core_settings` replays.
- the PERSONALIZE builder makes its last row exactly as `core_additem`
  does: `new(0x54)`, the constructor with (item, label, select, draw,
  change, -1, 8), each callback a 16-byte `std::function` (manager at +8,
  invoker at +12), then `Menu::addItem(menu, item)` with the menu in a4;
  it ends with the `movem`, `lea $80(a7),a7` and `rts` that `personalize.s`
  replays. Its frame is the Digitakt II's (0x80 bytes).
- `KeyEvent` (built by 0x40116278 from the UI queue's message): +12 the key
  code and +16 the action, as on the other devices (docs/ADAPTING.md);
  read back in the emulator: a tap of TRIG 1 gives code 25, a release 0x10.

`tests/test_digitone2.py` checks each of these against the stock file.

Build it with the SDK (it needs m68k binutils; with Homebrew's
`m68k-elf-binutils`, set `ELEKLOADER_CROSS=m68k-elf-`):

```bash
python -m elekloader.sdk.build mods/core-dn2 --stock Digitone_II_OS1.12.syx      # out/core-0.2.elemod
python -m elekloader.lint mods/core-dn2/out/core-0.2.elemod --stock Digitone_II_OS1.12.syx
```

## Input bridge and SysEx observers

DigiHealth's DN2 port uses these shared facilities; it owns no stock code
sites. The six UI events and their existing implementations remain intact.

- `input.c` merges physical and synthetic key masks and refuses physical
  overlap or a full UI queue. `core_keys_set` and `core_keys_clear` run only
  on the UI task. The stock driver at `0x4011fc8c` retains its 68-byte frame;
  the bridge replays its complete eight-byte prologue and continues at
  `0x4011fc94`. Its state bytes are at `0x445a2e30`.
- The queue object is `0x4059e214`: the UI receives it at `0x4002f178`, and
  initialization passes it to `0x4011fa7c`. `0x4029f0d0` is the panel's
  pointer cell, not the queue object. Queue checks reserve room below the
  stock key/encoder record-pool limits.
- `core_encoder_turn(1..9, -30..30)` (nonzero detents) calls the stock panel
  decoder `0x4011ff0c`. The stock periodic callback queues the accumulated
  encoder event through the same core/stock UI path as physical input.
- `core_millis()` uses the existing panel callback, registered at
  `0x401200c4` with mask 1. PIT2's stock setup has prescaler 6 and modulus
  17187: approximately 120 Hz at the derived 132 MHz bus clock. The bridge
  adds 25 ms per three callbacks (8/9 ms resolution), separately from the
  stock counter that wraps at 4096. It starts no timer. On the physical
  DS03 build, three one-second comparisons with the host clock differed
  by at most 0.3%. A stalled scheduler can delay input cleanup.
- `ev_sysex`: `void f(const unsigned char *message, unsigned length,
  unsigned source)`, on the MIDI task before stock routing. It preserves
  the router ABI at `0x4012197a` and always continues stock routing. Handlers
  validate their own header, exact lengths and source; 2/4 are USB and 8/16
  DIN. `fw_sysex_send` exports the stock sender `0x401236d2`.

The C declarations are in `elekloader/sdk/include/digitone-mk2/core.h`.
No arbitrary memory-write or function-call command is added.

## Validation and limits

Core 0.2 is experimental. `tests/test_digitone2.py` checks the stock sites,
stack/queue contracts, clock setup and exact patch boundaries.
`tests/test_dn2_input.py` checks ownership and input/queue bounds without
firmware. Digikit's ColdFire emulator checks stock-identical core-only
screens, the marker, all six UI events, reopened menu callbacks, subscriber
order and key consumption. It does not emulate DSP audio.

On a physical unit, the DS03 showcase (core + DigiHealth + hooks-check)
passed the six UI events, encoder delivery, reopened menu callbacks,
subscriber ordering and consumed/pass-through TEMPO. The DH03 build
(core + DigiHealth) passed clean-screen, normal TEMPO, encoder, bounded
hold-expiry and SYSTEM INFO checks. Both installed builds matched their
exact immutable runtime bytes through read-only diagnostics. These are
functional checks; longer project/audio qualification remains pending.
The [device guide](../../docs/DEVICES.md#the-digitone-ii-112) records the
candidate identities, measured 2 MiB USB update cap and separate emulator
and physical evidence.

DigiHealth is available on the [DN2 fork branch](https://github.com/simonjohansson/digihealth/tree/codex/dn2-remote-control).
The hooks-check showcase draws five bars over the top of every screen;
these deliberately erase parts of the stock interface. They are diagnostic
drawing, not a display-tearing symptom. Omit hooks-check for normal use.
