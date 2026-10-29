# param-check (Digitone II)

The smallest user of core-dn2's mod parameters (core 0.4,
[docs/ADAPTING.md](../../docs/ADAPTING.md), "Mod parameters (core-dn2 0.4,
Digitone II)"). Two lockable sound fields are knobs A (**ALFA**, 0..127,
default 64) and B (**BETA**, 0..15, default 3) of a page **TEST** after the
[FX] key's own.

On a unit: press [FX] twice (the header shows FX 2/2), turn A and B, switch
tracks and back: each track's sound keeps its own values, and after a power
cycle too.

```bash
python -m elekloader.sdk.build mods/core-dn2 --stock Digitone_II_OS1.12.syx
python -m elekloader.sdk.build examples/param-check-dn2 --stock Digitone_II_OS1.12.syx
```
