| SPDX-License-Identifier: GPL-2.0-or-later
| core-dn2 0.4: ev_dsp_tx, every audio block before the frame for the DSP goes out (locks.c core_dsp_put).
        .text
        .globl  core_dsp_tx_stub

| The audio interrupt's frame step (0x40025e82, jsr; it displaced `move.l 0x40287c38,d0`, whose flags
| the caller's bne reads). Handlers: f(), in order; any context rules of an interrupt apply.
        .align  2
core_dsp_tx_stub:
        lea     -16(%sp),%sp
        movem.l %d1/%a0-%a2,(%sp)
        lea     ev_dsp_tx,%a2
1:      move.l  (%a2)+,%d0
        beq.s   2f
        movea.l %d0,%a0
        jsr     (%a0)
        bra.s   1b
2:      movem.l (%sp),%d1/%a0-%a2
        lea     16(%sp),%sp
        move.l  0x40287c38,%d0
        rts
