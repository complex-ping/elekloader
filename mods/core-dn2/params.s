| SPDX-License-Identifier: GPL-2.0-or-later
| core-dn2 0.4, mod parameters (params.c): the sites' entries. OS 1.12.
        .text
        .globl  core_stock_rows, core_params_start, core_value_class_stub, core_page_lookup
        .globl  core_fx_pages_stub, core_fx_lock_stub, core_get_stub, core_set_stub
        .set    core_stock_rows, 0x401f83a4

        .macro  SAVE
        lea     -16(%sp),%sp
        movem.l %d0-%d1/%a0-%a1,(%sp)
        .endm
        .macro  RESTORE
        movem.l (%sp),%d0-%d1/%a0-%a1
        lea     16(%sp),%sp
        .endm

| The OS entry's call of main (0x40000568): the stock rows into core_rows,
| the mods' rows built, then main.
        .align  2
core_params_start:
        lea     0x401f83a4,%a0
        lea     core_rows,%a1
        move.l  #321*15,%d0
1:      move.l  (%a0)+,(%a1)+
        subq.l  #1,%d0
        bne.s   1b
        jsr     core_params_init
        jmp     0x40186fa2

| The value-class accessor (0x400c257c, `move.l 4(sp),d0 ; cmpi.l #321,d0`):
| an added row reads its template's object.
core_value_class_stub:
        SAVE
        move.l  20(%sp),-(%sp)
        jsr     core_value_class
        addq.l  #4,%sp
        move.l  %d0,(%sp)
        RESTORE
        cmpi.l  #321,%d0
        jmp     0x400c2586

| The page lookup (0x400c25d8, the whole leaf routine): id -> its record.
core_page_lookup:
        move.l  4(%sp),%d0
        cmpi.l  #36,%d0
        bhi.s   1f
        mulu.w  #44,%d0
        add.l   #0x42433c00,%d0
        rts
1:      move.l  %d0,-(%sp)
        jsr     core_page_record
        addq.l  #4,%sp
        rts

| The [FX] page list (0x400614e2 .. 0x400614f2): the count pushed, 5 (the
| mode) at -40(fp), the list in d0, as the stock instructions left them.
core_fx_pages_stub:
        moveq   #5,%d0
        move.l  core_fx_page_count,%d1
        move.l  %d1,-(%sp)
        move.l  %d0,-40(%fp)
        move.l  #core_fx_pages,%d0
        jmp     0x400614f2

| The lock indicator's [FX] list (0x4005cc0e .. 0x4005cc1c).
core_fx_lock_stub:
        move.l  %d2,(%sp)
        move.l  core_fx_page_count,%d1
        move.l  #core_fx_pages,%d0
        move.l  %d1,-(%sp)
        move.l  %d0,-(%sp)
        jmp     0x4005cc1c

| The getter's bound (0x4003717c: `moveq #100,d0 ; cmp.l d2,d0 ; blt`): d2
| the slot, d0 the live sound. 101..127 come from core_param_get.
core_get_stub:
        moveq   #100,%d1
        cmp.l   %d2,%d1
        bge.s   2f
        moveq   #127,%d1
        cmp.l   %d2,%d1
        blt.s   1f
        move.l  %d2,-(%sp)
        move.l  %d0,-(%sp)
        jsr     core_param_get
        addq.l  #8,%sp
        ext.l   %d0
        jmp     0x4003719c
1:      jmp     0x4003719a                      | clr.l d0: stock's answer
2:      jmp     0x40037182                      | stock's read of slot <= 100 (d0 is reloaded there)

| The setter's bound (0x40037bd0, the same shape): d2 the slot, d0 the live
| sound, d3 the value. 101..127 go to core_param_set; then the setter's end.
core_set_stub:
        moveq   #100,%d1
        cmp.l   %d2,%d1
        bge.s   2f
        moveq   #127,%d1
        cmp.l   %d2,%d1
        blt.s   1f
        move.l  %d3,-(%sp)
        move.l  %d2,-(%sp)
        move.l  %d0,-(%sp)
        jsr     core_param_set
        lea     12(%sp),%sp
1:      jmp     0x40037c48                      | the setter's end (stock's answer past 100)
2:      jmp     0x40037bd6                      | stock's write of slot <= 100 (d0 is reloaded there)
