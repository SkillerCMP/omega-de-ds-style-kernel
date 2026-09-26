@ RTS3 sprite-only menu. Do not touch BG scroll/affine/window-edge/BLDY state:
@ those values cannot be recovered by reading the physical GBA registers.
@ All temporary OBJ tiles, OAM and palette bytes are backed up separately from
@ the 1 KiB VRAM execution stack and restored before a SAVE or normal exit.
backup_LCD:
    stmfd sp!,{r4-r7,lr}
    mov r0,#RTS3_LIVE_PAGE
    bl SetRampage
    ldr r0,=0x0E004000
    mov r1,#0x07000000
    mov r2,#0x400
    bl rts3_to_sram
    ldr r0,=0x0E004400
    mov r1,#0x05000000
    mov r2,#0x400
    bl rts3_to_sram
    ldr r0,=0x0E004800
    ldr r1,=0x06014000
    mov r2,#0x800
    bl rts3_to_sram
    ldmfd sp!,{r4-r7,pc}
restore_LCD:
    stmfd sp!,{r4-r7,lr}
    mov r0,#0x04000000
    ldrh r1,[r0]
    orr r1,r1,#0x80
    strh r1,[r0]
    mov r0,#RTS3_LIVE_PAGE
    bl SetRampage
    ldr r0,=0x0E004000
    mov r1,#0x07000000
    mov r2,#0x400
    bl rts3_from_sram
    ldr r0,=0x0E004400
    mov r1,#0x05000000
    mov r2,#0x400
    bl rts3_from_sram
    ldr r0,=0x0E004800
    ldr r1,=0x06014000
    mov r2,#0x800
    bl rts3_from_sram
    ldmfd sp!,{r4-r7,pc}
clean_screen:
    stmfd sp!,{r0-r12,lr}
    mov r0,#0x07000000
    mov r1,#128
    mov r2,#0x200 @ hidden regular object
    mov r3,#0
1:  strh r2,[r0],#2
    strh r3,[r0],#2
    str r3,[r0],#4
    subs r1,r1,#1
    bne 1b
    mov r0,#0x05000000
    strh r3,[r0]
    add r0,r0,#0x200
    ldr r1,=0x7FFF
    strh r1,[r0,#2]
    ldr r1,=0x6A80
    strh r1,[r0,#0x22]
    mov r1,#31
    strh r1,[r0,#0x42]
    RTS_ADRL r4,ASCII
    ldr r5,=0x06014000
    mov r6,#(26*16)
2:  ldrb r7,[r4],#1
    mov r8,#0
    mov r9,#0
3:  tst r7,#0x80
    mov r10,#0
    movne r10,#1
    orr r8,r8,r10,lsl r9
    mov r7,r7,lsl #1
    add r9,r9,#4
    cmp r9,#32
    blo 3b
    str r8,[r5],#4
    subs r6,r6,#1
    bne 2b
    mov r0,#0x04000000
    mov r1,#0
    strh r1,[r0,#0x50]
    strh r1,[r0,#0x52]
    ldr r1,=0x1040 @ MODE0, OBJ on, 1-D tiles; BG/window bits all off
    strh r1,[r0]
    ldmfd sp!,{r0-r12,pc}
    .ltorg

@ One 8x16 vertical OBJ per character. Preserve the menu's r8-r12 iterators.
@ Input r0=A..Z, r1=x, r2=y, r3=color (white/selected/error red).
printchar:
    stmfd sp!,{r0-r12,lr}
    cmp r0,#65
    blo 9f
    cmp r0,#90
    bhi 9f
    sub r4,r0,#65
    mov r4,r4,lsl #1
    add r4,r4,#512
    ldr r5,=0x6A80
    cmp r3,r5
    orreq r4,r4,#0x1000
    beq 1f
    ldr r5,=0x7FFF
    cmp r3,r5
    orrne r4,r4,#0x2000
1:  cmp r2,#128
    bhs 2f
    sub r5,r2,#40
    mov r5,r5,lsr #4
    mov r5,r5,lsl #4
    sub r6,r1,#86
    add r5,r5,r6,lsr #3
    b 3f
2:  sub r5,r1,#64
    mov r5,r5,lsr #3
    add r5,r5,#80
3:  cmp r5,#128
    bhs 9f
    mov r6,#0x07000000
    add r6,r6,r5,lsl #3
    and r2,r2,#255
    orr r2,r2,#0x8000
    strh r2,[r6]
    ldr r5,=0x1FF
    and r1,r1,r5
    strh r1,[r6,#2]
    strh r4,[r6,#4]
9:  ldmfd sp!,{r0-r12,pc}
    .ltorg
