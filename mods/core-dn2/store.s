| SPDX-License-Identifier: GPL-2.0-or-later
| core-dn2 0.3, mod storage (store.c): each hooked converter's first
| instructions (the bytes its jmp site replaces), then a jump to the next.
| Called like the stock routine. OS 1.12.
        .text
        .globl  st_sound_save_body, st_sound_load_body
        .align  2
st_sound_save_body:                     | 0x400dd80a
        lea     -36(%sp),%sp
        movem.l %d2-%d5/%a2-%a6,(%sp)
        jmp     0x400dd812
st_sound_load_body:                     | 0x400dd34e
        lea     -32(%sp),%sp
        movem.l %d2-%d4/%a2-%a6,(%sp)
        jmp     0x400dd356
