# store-check (Digitone II)

The smallest user of core-dn2's mod storage (core 0.3,
[docs/ADAPTING.md](../../docs/ADAPTING.md), "Mod storage (core-dn2 0.3,
Digitone II)"). Three sound fields fill a sound's 88 bits. **STORE CHECK** in
SETTINGS writes a marker into them in all 16 tracks' sounds; three columns at
the bottom left of the screen, one a field, each grow a pixel for every track
whose marker no longer reads back. A square at the right end of the same edge
changes each time the three have been counted.

On a unit: select STORE CHECK, switch the Digitone off and on (no save is
needed: core writes the fields through to the project, as stock does a knob
edit): no column grows. Load a sound or a kit saved before, or on the stock OS,
into a track: its columns grow by one (a sound from another layout loads its
fields as zeros).

```bash
python -m elekloader.sdk.build mods/core-dn2 --stock Digitone_II_OS1.12.syx
python -m elekloader.sdk.build examples/store-check-dn2 --stock Digitone_II_OS1.12.syx
```
