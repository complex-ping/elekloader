| SPDX-License-Identifier: GPL-2.0-or-later
| core-dn2 0.4, p-locks of mod parameters (locks.c): the sites' entries. OS 1.12.
        .text
        .globl  core_rec_stub, core_read_stub, core_remove_stub, core_clear_index
        .globl  core_swap_index_read, core_swap_index_write
        .globl  core_paste_a, core_paste_b, core_paste_c, core_paste_d
        .globl  core_trig_copy, core_trig_paste, core_save_rec, core_load_rec
        .globl  core_build_stub, core_note_stub

        .set    CLIP_TRACK, 96668               | the clipboard's track, in the clipboard (fp-16)

| ---- the recorder (model, track, step, slot, value), the reader and the remover (model, track, step, slot):
| slots 101..127 are core's, the others stock's, entered after the first eight bytes they displaced.
        .align  2
core_rec_stub:
        move.l  16(%sp),%d0
        subi.l  #101,%d0
        cmpi.l  #27,%d0
        bcc.s   1f
        jmp     core_lock_record
1:      link.w  %fp,#-44
        movem.l %d2-%d7/%a2-%a3,(%sp)
        jmp     0x4003cef6

core_read_stub:
        move.l  16(%sp),%d0
        subi.l  #101,%d0
        cmpi.l  #27,%d0
        bcc.s   1f
        jmp     core_lock_read
1:      lea     -16(%sp),%sp
        movem.l %d2-%d4/%a2,(%sp)
        jmp     0x4003cdc2

core_remove_stub:
        move.l  16(%sp),%d0
        subi.l  #101,%d0
        cmpi.l  #27,%d0
        bcc.s   1f
        jmp     core_lock_remove
1:      lea     -32(%sp),%sp
        movem.l %d2-%d7/%a2-%a3,(%sp)
        jmp     0x4003d016

| ---- the index byte of a freed or copied record, which core's records do not have ----

| The step clear's (0x4003d266, jsr): a5 = 101 track, d7 = the record's slot, d0 = the table.
core_clear_index:
        add.l   %a5,%d0
        movea.l %d0,%a0
        adda.l  %d7,%a0
        moveq   #100,%d0
        cmp.l   %d7,%d0
        blt.s   1f
        st      %d0
        move.b  %d0,20640(%a0)
1:      rts

| The track swap's record copy (0x4003d554): its look for the destination's record
| (0x4003d5a2, jsr), d2 the slot, d4 = 101 track; core's record is never found (a fresh one is taken)...
core_swap_index_read:
        movea.l 4(%a0),%a1
        movea.l (%a1),%a1
        lea     (%a1,%d4.l),%a2
        adda.l  %d2,%a2
        cmpi.l  #100,%d2
        bgt.s   1f
        mvs.b   20640(%a2),%d0                  | the flags for the caller's bge
        rts
1:      moveq   #-1,%d0
        rts

| ... and its write of the index byte (0x4003d5f4, jsr), d0 the record.
core_swap_index_write:
        adda.l  %d4,%a1
        adda.l  %d2,%a1
        cmpi.l  #100,%d2
        bgt.s   1f
        move.b  %d0,20640(%a1)
1:      move.l  %d0,%d2
        lsl.l   #7,%d2
        rts

| ---- the pastes: the clipboard's record's track for the compare that follows (d1 = CLIP_TRACK), -2
| for core's (pasted by core_lock_paste) ----

| Page pastes 0x40052634 (-> d7) and 0x40053246 (-> d5): a1 + d0 the record, d1 its page's values,
| a3 the table, d3 the page's first step, a2 the context (+60 the track).
core_paste_a:
        move.l  -20(%fp),%d1
        bsr.s   page_paste
        move.l  %d1,%d7
        move.l  #CLIP_TRACK,%d1
        rts
core_paste_b:
        move.l  %d6,%d1
        bsr.s   page_paste
        move.l  %d1,%d5
        move.l  #CLIP_TRACK,%d1
        rts
page_paste:
        lea     -12(%sp),%sp
        movem.l %d0/%a0-%a1,(%sp)
        movea.l -16(%fp),%a0
        adda.l  #CLIP_TRACK,%a0
        move.l  (%a0),-(%sp)                    | the clipboard's track
        move.l  60(%a2),-(%sp)                  | the destination's
        pea     16
        move.l  %d3,-(%sp)
        move.l  %a3,-(%sp)
        add.l   %d0,%d1
        move.l  %d1,-(%sp)
        pea     (%a1,%d0.l)
        jsr     core_lock_paste
        lea     28(%sp),%sp
        move.l  %d0,%d1
        movem.l (%sp),%d0/%a0-%a1
        lea     12(%sp),%sp
        rts

| Track pastes 0x4005389c (-> d5) and 0x40052948 (-> d4): a3 + a0 the record, d2 the table.
core_paste_c:
        bsr.s   track_paste
        move.l  %d1,%d5
        move.l  #CLIP_TRACK,%d1
        rts
core_paste_d:
        bsr.s   track_paste
        move.l  %d1,%d4
        move.l  #CLIP_TRACK,%d1
        rts
track_paste:
        lea     -12(%sp),%sp
        movem.l %d0/%a0-%a1,(%sp)
        movea.l -16(%fp),%a1
        adda.l  #CLIP_TRACK,%a1
        move.l  (%a1),-(%sp)
        move.l  60(%a2),-(%sp)
        pea     128
        clr.l   -(%sp)
        move.l  %d2,-(%sp)
        pea     (%a3,%a0.l)                     | its values: the record's own
        pea     (%a3,%a0.l)
        jsr     core_lock_paste
        lea     28(%sp),%sp
        move.l  %d0,%d1
        movem.l (%sp),%d0/%a0-%a1
        lea     12(%sp),%sp
        rts

| ---- trig copy and paste ----

| Trig copy (0x40053f48, jsr), after the step's stock locks are gathered: the clipboard's entry
| vector at fp-92, a3 the context (+44 the model, +60 the track), (a2) the step. Then the two
| pushes it displaced, under the return (a0 is free there).
core_trig_copy:
        lea     -16(%sp),%sp
        movem.l %d0-%d1/%a0-%a1,(%sp)
        move.l  (%a2),-(%sp)
        move.l  60(%a3),-(%sp)
        move.l  44(%a3),-(%sp)
        move.l  -92(%fp),-(%sp)
        jsr     core_lock_trig_copy
        lea     16(%sp),%sp
        movem.l (%sp),%d0-%d1/%a0-%a1
        lea     16(%sp),%sp
        movea.l (%sp)+,%a0
        move.l  (%a2),-(%sp)
        move.l  60(%a3),-(%sp)
        jmp     (%a0)

| Trig paste (0x40055440, jsr), after the destination step is cleared: a3 the entry ((a3) the
| step), a2 the context. Then what it displaced.
core_trig_paste:
        lea     -16(%sp),%sp
        movem.l %d0-%d1/%a0-%a1,(%sp)
        move.l  (%a3),-(%sp)
        move.l  60(%a2),-(%sp)
        move.l  44(%a2),-(%sp)
        move.l  %a3,-(%sp)
        move.l  -92(%fp),-(%sp)
        jsr     core_lock_trig_paste
        lea     20(%sp),%sp
        movem.l (%sp),%d0-%d1/%a0-%a1
        lea     16(%sp),%sp
        mvz.b   4(%a3),%d0
        lea     0x40054690,%a4
        rts

| ---- pattern save and load ----

| The save (0x400de8c2 .. 0x400de8d7, jmp): d1 the record's slot, d3 its track (d2 the byte), a0 the
| saved record, a3 / a4 the tracks' / FX lane's slot -> id tables. Core's record is saved as
| {code, track | 0x20}.
core_save_rec:
        cmpi.l  #101,%d1
        blt.s   1f
        cmpi.l  #127,%d1
        bgt.s   1f
        move.b  %d1,(%a0)
        move.l  %d2,%d4
        ori.l   #0x20,%d4
        move.b  %d4,1(%a0)
        jmp     0x400de8d8
1:      moveq   #16,%d4
        cmp.l   %d3,%d4
        bne.s   2f
        move.l  (%a4,%d1.l*4),%d1
        bra.s   3f
2:      move.l  (%a3,%d1.l*4),%d1
3:      move.b  %d1,(%a0)
        move.b  %d2,1(%a0)
        jmp     0x400de8d8

| The load (0x400de6f8 .. 0x400de709, jmp): d0 the saved record's id byte, d3 its track byte, a4 it,
| a3 the record, a2 the table. A record of core's goes to core_lock_load; stock's checks follow.
core_load_rec:
        move.l  %d3,%d1
        andi.l  #0xf0,%d1
        cmpi.l  #0x20,%d1
        bne.s   2f
        mvz.b   %d0,%d1
        subi.l  #101,%d1
        cmpi.l  #27,%d1
        bcc.s   9f
        move.l  %a4,-(%sp)
        move.l  %a3,-(%sp)
        move.l  %a2,-(%sp)
        jsr     core_lock_load
        lea     12(%sp),%sp
9:      jmp     0x400de75a
2:      moveq   #106,%d4
        mvz.b   %d0,%d1
        cmp.l   %d1,%d4
        bcs.s   9b
        mvz.b   %d3,%d1
        moveq   #15,%d4
        cmp.l   %d1,%d4
        bcs.s   9b
        jmp     0x400de70a

| ---- playing ----

| The lock-list builder (0x400d874c, jmp): (track, list, table, ., step, step, 0). Stock's body, then
| core's locks into the list.
core_build_stub:
        move.l  28(%sp),-(%sp)
        move.l  28(%sp),-(%sp)
        move.l  28(%sp),-(%sp)
        move.l  28(%sp),-(%sp)
        move.l  28(%sp),-(%sp)
        move.l  28(%sp),-(%sp)
        move.l  28(%sp),-(%sp)
        jsr     build_body
        lea     28(%sp),%sp
        move.l  20(%sp),-(%sp)                  | step
        move.l  16(%sp),-(%sp)                  | table
        move.l  16(%sp),-(%sp)                  | list
        move.l  16(%sp),-(%sp)                  | track
        jsr     core_lock_build
        lea     16(%sp),%sp
        rts
build_body:
        lea     -24(%sp),%sp
        moveq   #101,%d0
        jmp     0x400d8752

| The note path's lock step (0x40026c20, jsr): d2 the voice, the trig's record at fp-36 (+16 its
| track, +84 its list or 0). Then what it displaced: a0 the record, d0 the list and its flags.
core_note_stub:
        lea     -8(%sp),%sp
        movem.l %d1/%a1,(%sp)
        movea.l -36(%fp),%a0
        move.l  84(%a0),-(%sp)
        move.l  16(%a0),-(%sp)
        move.l  %d2,-(%sp)
        jsr     core_lock_note
        lea     12(%sp),%sp
        movem.l (%sp),%d1/%a1
        lea     8(%sp),%sp
        movea.l -36(%fp),%a0
        move.l  84(%a0),%d0
        rts
