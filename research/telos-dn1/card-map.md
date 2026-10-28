# Digitone mk1 card map (the +Drive)

From the stock OS 1.43 and 1.44, read in the code (marks as in
[README.md](README.md), section 6). Sectors are 512 bytes; "1.44" gives the
address where it differs. The shape follows digikit's
`devices/digitone-ii.toml` `[card]`. The card is an eMMC on the main board
(the firmware's own messages say "eMMC").

## The part

The firmware identifies the part by its CID and checks its size
(0x400f5566; 1.44 0x400f57da) against a table of seven entries, 32 bytes
each, at 0x40240410 (1.44 0x40240810). A card that matches none still works:
the join's status word is nonzero (README S3). **[D]**

| entries | manufacturer id | SLC-mode size, sectors | other mode, sectors |
|---|---|---|---|
| 4 | 0x11 | 3,866,624 (0x3B0000, 1.84 GiB) | 7,733,248 (0x760000) |
| 1 | 0x15 | 3,817,472 (0x3A4000, 1.82 GiB) | 7,634,944 (0x748000) |
| 2 | 0x70 | 48,340,992 (0x2E1A000, 23.0 GiB) | 61,579,264 (0x3A4A000) |

The units are expected to run in SLC mode (a maintenance screen says "MMC
NOT IN SLC MODE" otherwise), so the first column is the card's size; the
emulator's card is the first row. The smallest is 0x3A4000 sectors. Sector-addressed on all three: the address multiplier
(0x419c92d0) is 1.

## Regions

| sectors | size | what | who | FORMAT |
|---|---|---|---|---|
| 0 | 1 sector (8 bytes used) | the store's header: magic 0xBEEFBACE, then 0 | store write 0x400f747a (1.44 0x400f76ee) | rewritten |
| 0x800 | 1 sector (272 bytes used) | the used bitmap: 16 bytes for the 128 project slots, then 256 bytes for the 2,048 sounds | 0x400f74fc (1.44 0x400f7770) | cleared |
| 0x1000-0x1FFF | 2 MiB | the sound pool: sound n (0-2047) in the two sectors at (n + 0x800) * 2, a 302-byte (0x12E) record | sector function 0x400f6db6 (1.44 0x400f702a); store write 0x400f739e | erased |
| 0x2000-0x79FFF | ~244 MiB | no reference found | - | untouched |
| 0x7A000-0x7B53A | 2.65 MiB | working copy A: a header sector (magic `COKI`), then the 2,782,484-byte project image (0x2A7514) | recovery at start 0x400a8340 (1.44 0x400a8360); emergency save 0x400a7b6e (1.44 0x400a7b8e) | untouched |
| 0x7C000-0x7D53A | 2.65 MiB | working copy B, the same | the same | untouched |
| 0x7E000-0x7FFFF | 4 MiB | the working project, slot 128 (the project image, 2,782,212 bytes = 0x2A7404, magic 0xBEEFBACE, from the first sector) | store, 0x400f68c2 (1.44 0x400f6b36) for the start | untouched |
| 0x80000-0x17FFFF | 512 MiB | project slots 0-127, 4 MiB each at (n + 64) << 13 | store; RPC `/projects/1..128` | erased, in 1 MiB steps |
| 0x180000 and up | - | no reference found. The store refuses a sector above 0x17FFFF and a range past 768 MiB (error 2) | - | untouched |
| 0x400000, 0x800000, 0xC00000 | 1 sector each | legacy COKI probes: read 4 bytes, and erase the sector if it begins with `COKI`. They exist on the 23 GiB part only | 0x400a81fa (1.44 0x400a821a); table 0x401c2a84 (1.44 0x401c2dd4) | untouched |

- **[D]** The store functions: `write(store, sector, bytes, buf, ...)`
  0x400f739e (1.44 0x400f7612), `read` 0x400f62ac (0x400f6520), `erase`
  0x400f6336 (0x400f65aa). All three take the store object, lock its mutex
  (at +0x12F74) and call the card's `wr`/`rd`/`er` (README S1). Only the
  write refuses a sector above 0x17FFFF and a range past 0x30000000 bytes.
  The sector of a slot is clamped to 0-127 (128 is the working project),
  that of a sound to 0-2047, so no index leaves the table above.
- **[D]** The bitmap is the only index of what is used on the card. The
  project slot table (28-byte entries: a valid byte at +12, the start
  sector at +16) lives in the store object in RAM; how it is filled at boot
  was not read [O].
- **[V]** After the first boot on a blank card the emulator's reads span
  sectors 0-0x7E000 and its writes 0x7E000-0x7F500 (the working project),
  and the card file ends just past slot 0 (0x80000 + 0x1539).
- **[D]** The working copies: at start 0x400a8340 reads the first 256 bytes
  of A, then of B, takes the first that validates (0x4008523a), and reads
  its body into the project image at 0x40554ee4 (1.44 0x40555ee4); with
  neither, it scans the three legacy sectors. The emergency save writes the
  image to the copy that is not current, in 32 KB pieces, then clears the
  first byte of the old one.

## What FORMAT does

`PlusDriveFormatMenuView`; the store's format 0x400f8132 (1.44 0x400f83a6),
**[D]**: erase the project slots, sectors 0x80000-0x17FFFF in 1 MiB steps,
then the sounds, 0x1000-0x1FFF in 1 MiB steps; write the header (sector 0);
clear the bitmap (0x800). It touches nothing else. The working copies and
the working project survive it. "Factory reset" at the next boot goes
through the same functions [O: not read].

## Requests that reach a sector

**[D]** The SysEx RPC knows only `/projects[/*[/.metadata]]` and
`/soundbanks[/*[/*[/.metadata]]]`. `/projects/n` takes n = 1-128 (0 is
refused as the active project); `/soundbanks/b/n` takes a bank that resolves
and n = 1-256. No unchecked index found in the RPC. A factory-test console
reads any sector but was not found to be reached from SysEx or MIDI (README
S2). The MIDI program-change and bank-select handlers were not read [O].

## The area for TELOS

**Sectors 0x380000-0x39FFFF** (1792-1856 MiB, 64 MiB). Above 0x180000 the
firmware's store never goes, no RPC path reaches it, FORMAT leaves it, and
every part ends above it (the smallest at 0x3A4000, 8 MiB later). It holds
none of the three legacy probe sectors, and the same absolute area is free
on the 23 GiB part too.

TELOS's own layout inside it (headers, table, selection record) is
digikit's (`digikit-store.md`: the selection record in the last two sectors
of the area, 0x39FFFE and 0x39FFFF) and is for the build to fix.
