# dsp-upload-dn2

Digitone II OS 1.12. The main OS uploads section 7 to the DSP through two
buffers of 1 MiB and refuses a stored section over 1 MiB or a decoded one
over 1 MiB - 1. This mod raises all four to 1.5 MiB (four sites, nothing
else), and names the resource `dn2-dsp-upload-1536k`, which the linker reads
to raise its own section-7 limits for a [blocks mod](../../docs/FORMAT.md#digitone-ii-sharc-blocks).

| site | stock | now |
|---|---|---|
| 0x400cf6b0 | `move.l #0x100000,-(sp)`: the stored buffer's size | 0x180000 |
| 0x400cf6da | `move.l #0x100000,-(sp)`: the decoded buffer's size | 0x180000 |
| 0x400cf710 | `cmpi.l #0x100000,d0`: the stored size check | 0x180000 |
| 0x400cf748 | `cmpi.l #0x100000,d2`: the decoded size + 1 check | 0x180000 |

[SHARC-DN2.md](../../docs/SHARC-DN2.md#blocks-mods-and-the-15-mib-upload)
has the evidence. Recovery is as for any build: stock over MIDI DIN.
