| SPDX-License-Identifier: GPL-2.0-or-later
| core-dn2 0.4, mod parameters as LFO destinations (lfo.c): the sites' entries. OS 1.12.
        .text
        .globl  core_lfo_stub, core_dest_walk, core_slot_row, core_dest_store

        .set    LFOS, 4

| The LFO evaluator's DEST bound (0x40137d6e .. 0x40137d79, jmp): d2 the DEST byte, d0 the LFO's
| output, a2 its state, a4 its parameters (+82 the depth), a5 the voice, 48(sp) the LFO. A DEST 0..100
| goes on to stock's cell; a code's modulation goes to core_lfo_delta, its code to core_lfo_code.
core_lfo_stub:
        mvs.w   %d2,%d7
        move.l  %d7,104(%a2)
        move.l  %a5,%d1
        lsl.l   #2,%d1
        add.l   48(%sp),%d1                     | voice x 4 + the LFO
        lea     core_lfo_code,%a0
        clr.b   (%a0,%d1.l)
        cmpi.l  #100,%d7
        bls.s   8f
        cmpi.l  #127,%d7
        bhi.s   9f
        move.b  %d7,(%a0,%d1.l)
        lea     core_lfo_delta,%a0
        lsl.l   #2,%d1
        adda.l  %d1,%a0
        mvs.w   82(%a4),%d1                     | stock's (depth - 0x4000) x 2 times the output, Q31:
        addi.l  #-16384,%d1                     | here without the EMAC, to 16 bits of the output
        add.l   %d1,%d1
        swap    %d0
        ext.l   %d0
        muls.l  %d1,%d0
        moveq   #15,%d1
        asr.l   %d1,%d0
        move.l  %d0,108(%a2)
        move.l  %d0,(%a0)
9:      jmp     0x40137db2
8:      jmp     0x40137d7a

| The DEST list's walk (0x400395b6, jmp): slots 0..127.
core_dest_walk:
        addq.l  #1,%d2
        cmpi.l  #128,%d2
        bne.s   1f
        jmp     0x400395be
1:      jmp     0x4003958a

| The slot -> row lookup (0x400dc18e (slot, type, filter), jmp): a slot past 100 is a code's.
core_slot_row:
        move.l  4(%sp),%d0
        cmpi.l  #100,%d0
        bls.s   1f
        move.l  %d0,-(%sp)
        jsr     core_code_row
        addq.l  #4,%sp
        rts
1:      move.l  %d2,-(%sp)
        moveq   #100,%d1
        movea.l 8(%sp),%a0
        jmp     0x400dc196

| Stock's per-edit write of a DEST into the project image (0x4004cb16, 0x4004cb82, jsr): d1 the
| live value -> d1 the stored one: a code as {0, code}, a slot as its id << 8.
core_dest_store:
        move.l  %d1,%d0
        asr.l   #8,%d0
        cmpi.l  #101,%d0
        blt.s   1f
        cmpi.l  #127,%d0
        bgt.s   1f
        move.l  %d0,%d1
        rts
1:      move.l  %d0,-(%sp)
        clr.l   -(%sp)
        jsr     0x400dce5e
        addq.l  #8,%sp
        move.l  %d0,%d1
        lsl.l   #8,%d1
        rts
