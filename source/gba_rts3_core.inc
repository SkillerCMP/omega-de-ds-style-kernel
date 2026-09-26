@ RTS3 hardware-checkpoint core.
@ ROM-resident, ARM7TDMI, position independent. No C calls or allocation.
@ The 1 KiB VRAM workspace is backed up before use and restored stacklessly.
@ DMA/timer hidden latches and write-only video values are NOT fabricated.
@ Hardware-fidelity limits are documented by the RTS3 source and inspector.

@ SRAM byte helpers. These leaf functions never touch SP or r4-r11.
rts3_put32:
    RTS3_PUT r1,r0,0,r2
    bx lr
rts3_to_sram: @ r0=SRAM, r1=word-aligned memory, r2=multiple-of-four length
    cmp r2,#0
    bxeq lr
1:  ldr r3,[r1],#4
    strb r3,[r0],#1
    mov r3,r3,lsr #8
    strb r3,[r0],#1
    mov r3,r3,lsr #8
    strb r3,[r0],#1
    mov r3,r3,lsr #8
    strb r3,[r0],#1
    subs r2,r2,#4
    bne 1b
    bx lr
rts3_from_sram: @ r0=SRAM, r1=word-aligned memory, r2=multiple-of-four length
    cmp r2,#0
    bxeq lr
1:  ldrb r3,[r0],#1
    ldrb r12,[r0],#1
    orr r3,r3,r12,lsl #8
    ldrb r12,[r0],#1
    orr r3,r3,r12,lsl #16
    ldrb r12,[r0],#1
    orr r3,r3,r12,lsl #24
    str r3,[r1],#4
    subs r2,r2,#4
    bne 1b
    bx lr

@ CRC32 (IEEE), reading each byte once. All mapped input ranges are <=0x8000
@ and remain wholly inside the selected LOW window. r4-r11 and SP untouched.
rts3_crc:
    mvn r2,#0
    RTS_ADRL r12,rts3_crc_table
    b rts3_crc_loop
rts3_crc_update:
    RTS_ADRL r12,rts3_crc_table
rts3_crc_loop:
    cmp r1,#0
    beq 2f
1:  ldrb r3,[r0],#1
    eor r2,r2,r3
    and r3,r2,#15
    ldr r3,[r12,r3,lsl #2]
    eor r2,r3,r2,lsr #4
    and r3,r2,#15
    ldr r3,[r12,r3,lsl #2]
    eor r2,r3,r2,lsr #4
    subs r1,r1,#1
    bne 1b
2:  mvn r0,r2
    bx lr

@ Header checksum covers all 256 bytes with word 0x3C treated as zero.
@ It does not mutate the state being validated. r4-r11 preserved.
rts3_header_crc:
    stmfd sp!,{r4-r5,lr}
    mov r4,r0
    mov r1,#0x3C
    mvn r2,#0
    bl rts3_crc_update
    mov r5,#8 @ eight zero nibbles for the omitted checksum word
1:  and r3,r2,#15
    ldr r3,[r12,r3,lsl #2]
    eor r2,r3,r2,lsr #4
    subs r5,r5,#1
    bne 1b
    add r0,r4,#0x40
    mov r1,#0xC0
    bl rts3_crc_update
    ldmfd sp!,{r4-r5,pc}

@ Copies between two mapped windows via a 64-byte VRAM buffer.
@ r0=dest page, r1=dest LOW offset, r2=source page, r3=source LOW offset,
@ r4=length. Caller guarantees aligned bounded ranges. All r4-r11 preserved.
rts3_copy_banked:
    stmfd sp!,{r4-r11,lr}
    mov r5,r0
    mov r6,r1
    mov r7,r2
    mov r9,r3
    mov r8,r4
1:  cmp r8,#0
    beq 2f
    mov r10,#64
    cmp r8,r10
    movlo r10,r8
    mov r0,r7
    bl SetRampage
    add r0,r9,#0x0E000000
    ldr r1,=RTS3_WORK_BUFFER
    mov r2,r10
    bl rts3_from_sram
    mov r0,r5
    bl SetRampage
    add r0,r6,#0x0E000000
    ldr r1,=RTS3_WORK_BUFFER
    mov r2,r10
    bl rts3_to_sram
    add r6,r6,r10
    add r9,r9,r10
    sub r8,r8,r10
    b 1b
2:  ldmfd sp!,{r4-r11,pc}
    .ltorg

@ Entry contract: caller pushed {r0-r12,lr} on the original BIOS IRQ stack;
@ r0 points to that frame. Capture original GPRs from the BIOS frame, not the
@ dispatcher scratch r0-r3/r12. Move to the VRAM stack before copying any RAM.
@ This routine itself uses no push/pop and restores every probed CPU bank.
rts3_enter:
    mov r11,r0
    mov r10,lr
    mov r0,#0x04000000
    add r1,r0,#0x200
    ldrh r8,[r1,#8] @ original IME, before cart switching
    ldrh r9,[r1,#2] @ original IF, observation only
    mov r2,#0
    strh r2,[r1,#8]
    mov r0,#RTS3_LIVE_PAGE
    bl SetRampage
    ldr r4,=RTS3_LIVE_CPU
    mov r0,r4
    mov r1,#0
    mov r2,#RTS3_CPU_BYTES
1:  strb r1,[r0],#1
    subs r2,r2,#1
    bne 1b
    add r5,r11,#56 @ original callback SP, points at BIOS {r0-r3,r12,lr_irq}
    mov r0,r4
    mov r1,r5
    mov r2,#16
    bl rts3_to_sram
    add r0,r4,#0x10
    add r1,r11,#16
    mov r2,#32
    bl rts3_to_sram
    ldr r1,[r5,#16]
    RTS3_PUT r1,r4,0x30,r2
    ldr r1,[r5,#20]
    sub r1,r1,#4
    RTS3_PUT r1,r4,0x3C,r2
    mrs r1,SPSR
    RTS3_PUT r1,r4,0x40,r2
    mrs r6,CPSR
    RTS3_PUT r6,r4,0x44,r2
    RTS3_PUT r5,r4,0x48,r2
    ldr r1,[r11,#52]
    RTS3_PUT r1,r4,0x4C,r2
    add r0,r4,#0x50
    mov r1,r5
    mov r2,#24
    bl rts3_to_sram
    @ No BL in a foreign bank: its LR must remain the original game's LR.
    mov r0,#0xDF
    msr cpsr_c,r0
    nop
    RTS3_PUT sp,r4,0x68,r0
    RTS3_PUT lr,r4,0x6C,r0
    msr cpsr_c,r6
    nop
    mov r0,#0xD3
    msr cpsr_c,r0
    nop
    RTS3_PUT sp,r4,0x70,r0
    RTS3_PUT lr,r4,0x74,r0
    mrs r1,SPSR
    RTS3_PUT r1,r4,0x78,r0
    msr cpsr_c,r6
    nop
    mov r0,#0xD7
    msr cpsr_c,r0
    nop
    RTS3_PUT sp,r4,0x7C,r0
    RTS3_PUT lr,r4,0x80,r0
    mrs r1,SPSR
    RTS3_PUT r1,r4,0x84,r0
    msr cpsr_c,r6
    nop
    mov r0,#0xDB
    msr cpsr_c,r0
    nop
    RTS3_PUT sp,r4,0x88,r0
    RTS3_PUT lr,r4,0x8C,r0
    mrs r1,SPSR
    RTS3_PUT r1,r4,0x90,r0
    msr cpsr_c,r6
    nop
    mov r0,#0xD1
    msr cpsr_c,r0
    nop
    RTS3_PUT r8,r4,0x94,r0
    RTS3_PUT r9,r4,0x98,r0
    RTS3_PUT r10,r4,0x9C,r0
    RTS3_PUT r11,r4,0xA0,r0
    RTS3_PUT r12,r4,0xA4,r0
    RTS3_PUT sp,r4,0xA8,r0
    RTS3_PUT lr,r4,0xAC,r0
    mrs r1,SPSR
    RTS3_PUT r1,r4,0xB0,r0
    msr cpsr_c,r6
    nop
    @ Fill visible SP/LR from the interrupted mode's bank (SVC or USR/SYS).
    RTS_GET32 r1,r4,0x40,r2
    and r1,r1,#31
    cmp r1,#0x13
    addeq r3,r4,#0x70
    addne r3,r4,#0x68
    RTS_GET32 r1,r3,0,r2
    RTS3_PUT r1,r4,0x34,r2
    RTS_GET32 r1,r3,4,r2
    RTS3_PUT r1,r4,0x38,r2
    mov r3,#0x04000000
    ldr r1,[r3,#-12]
    RTS3_PUT r1,r4,0xB4,r2
    ldr r1,[r3,#-4]
    RTS3_PUT r1,r4,0xB8,r2
    mov r1,#0x3F @ CPU banks + BIOS frame are captured; no pipeline-cycle claim
    RTS3_PUT r1,r4,0xBC,r2
    ldr r1,=0x33555043 @ CPU3
    RTS3_PUT r1,r4,0xC0,r2
    RTS3_PUT r11,r4,0xC4,r2

    ldr r4,=RTS3_LIVE_HW
    mov r0,r4
    mov r1,#0
    mov r2,#0x100 @ HW block plus transient CONTROL area
2:  strb r1,[r0],#1
    subs r2,r2,#1
    bne 2b
    ldr r1,=0x33574852 @ RHW3
    RTS3_PUT r1,r4,0,r2
    mov r1,#0x1F
    RTS3_PUT r1,r4,4,r2
    add r4,r4,#8
    mov r5,#0x04000000
    RTS_ADRL r6,rts3_hw_registers
    mov r7,#22
3:  ldrh r0,[r6],#2
    ldrh r1,[r5,r0]
    strb r1,[r4],#1
    mov r1,r1,lsr #8
    strb r1,[r4],#1
    subs r7,r7,#1
    bne 3b
    ldr r4,=RTS3_LIVE_HW
    strb r8,[r4,#HW_IME]
    mov r1,r8,lsr #8
    strb r1,[r4,#(HW_IME+1)]
    strb r9,[r4,#HW_IF]
    mov r1,r9,lsr #8
    strb r1,[r4,#(HW_IF+1)]
    mov r1,#0x0F @ unknown: DMA progress/initials, timer reload, LCD WO, sound phase
    RTS3_PUT r1,r4,0x38,r2
    mov r1,#3
    RTS3_PUT r1,r4,0x3C,r2
    ldr r4,=RTS3_LIVE_IO
    mov r0,r4
    mov r1,#0
    mov r2,#RTS3_IO_BYTES
4:  strb r1,[r0],#1
    subs r2,r2,#1
    bne 4b
    RTS_ADRL r6,rts3_io_registers
    mov r7,#28
5:  ldrh r0,[r6],#2
    ldrh r1,[r5,r0]
    strb r1,[r4],#1
    mov r1,r1,lsr #8
    strb r1,[r4],#1
    subs r7,r7,#1
    bne 5b
    bl rts3_quiesce
    ldr r0,=RTS3_WORK_BASE
    mov r1,#RTS3_WORK_BYTES
    bl rts3_crc
    mov r1,r0
    ldr r0,=(RTS3_CONTROL+4)
    bl rts3_put32
    ldr r0,=RTS3_WORK_BACKUP
    ldr r1,=RTS3_WORK_BASE
    mov r2,#RTS3_WORK_BYTES
    bl rts3_to_sram
    ldr sp,=RTS3_WORK_TOP
    bx r10

rts3_quiesce:
    mov r0,#0x04000000
    add r1,r0,#0x200
    mov r2,#0
    strh r2,[r1,#8]
    add r1,r0,#0x100
    strh r2,[r1,#2]
    strh r2,[r1,#6]
    strh r2,[r1,#10]
    strh r2,[r1,#14]
    strh r2,[r0,#0xBA]
    strh r2,[r0,#0xC6]
    strh r2,[r0,#0xD2]
    strh r2,[r0,#0xDE]
    strh r2,[r0,#0x80]
    strh r2,[r0,#0x82]
    ldrh r1,[r0]
    orr r1,r1,#0x80
    strh r1,[r0]
    bx lr
    .ltorg

@ Diagnostics are independent of the immutable state manifest. Every update is
@ self-checksummed. The host may not flush SRAM to SD after a hung game; these
@ markers describe the last persisted observation, not proof of gameplay.
@ r0=stage, r1=detail (e.g. chunk index or rejection reason).
rts3_diag:
    stmfd sp!,{r4-r7,lr}
    mov r4,r0
    mov r5,r1
    mov r0,#RTS3_META_PAGE
    bl SetRampage
    ldr r6,=RTS3_DIAG
    ldr r1,=0x33445452 @ RTD3
    RTS3_PUT r1,r6,0,r2
    ldr r1,=RTS3_ABI
    RTS3_PUT r1,r6,4,r2
    RTS3_PUT r4,r6,8,r2
    RTS3_PUT r5,r6,12,r2
    mov r0,r6
    mov r1,#0xFC
    bl rts3_crc
    RTS3_PUT r0,r6,0xFC,r2
    ldmfd sp!,{r4-r7,pc}
rts3_diag_init:
    stmfd sp!,{lr}
    mov r0,#RTS3_META_PAGE
    bl SetRampage
    ldr r0,=RTS3_DIAG
    mov r1,#0
    mov r2,#0x100
1:  strb r1,[r0],#1
    subs r2,r2,#1
    bne 1b
    ldmfd sp!,{pc}

@ Table-driven SAVE. The footer is invalidated before any payload write and
@ published byte-zero-last only after all CRC/complement pairs and header CRC.
rts3_save:
    stmfd sp!,{r4-r11,lr}
    bl rts3_diag_init
    ldr r0,=RTS3_FOOTER
    mov r1,#0
    strb r1,[r0]
    mov r0,#0x100
    mov r1,#0
    bl rts3_diag
    mov r0,#RTS3_LIVE_PAGE
    bl SetRampage
    ldr r0,=RTS3_LIVE_HEADER
    RTS_ADRL r1,rts3_header_template
    mov r2,#RTS3_HEADER_BYTES
    bl rts3_to_sram
    RTS_ADRL r5,rts3_regions
    mov r8,#0
rts3_save_region:
    ldmia r5,{r6,r7,r9,r10,r11}
    mov r0,#RTS3_LIVE_PAGE
    bl SetRampage
    mov r0,r6
    mov r1,r10
    bl rts3_crc
    mov r4,r0
    ldr r0,=(RTS3_LIVE_HEADER+0x40)
    add r0,r0,r8,lsl #3
    mov r1,r4
    bl rts3_put32
    add r0,r0,#4
    mvn r1,r4
    bl rts3_put32
    cmp r11,#0
    bne rts3_save_staged
    mov r0,r7
    bl SetRampage
    add r0,r9,#0x0E000000
    mov r1,r6
    mov r2,r10
    bl rts3_to_sram
    b rts3_save_next
rts3_save_staged:
    mov r0,r7
    mov r1,r9
    mov r2,#RTS3_LIVE_PAGE
    sub r3,r6,#0x0E000000
    mov r4,r10
    bl rts3_copy_banked
rts3_save_next:
    mov r0,#0x110
    mov r1,r8
    bl rts3_diag
    add r5,r5,#20
    add r8,r8,#1
    cmp r8,#RTS3_REGION_COUNT
    blo rts3_save_region
    mov r0,#RTS3_LIVE_PAGE
    bl SetRampage
    ldr r0,=RTS3_LIVE_HEADER
    bl rts3_header_crc
    mov r1,r0
    ldr r0,=(RTS3_LIVE_HEADER+0x3C)
    bl rts3_put32
    mov r0,#RTS3_META_PAGE
    mov r1,#0x800
    mov r2,#RTS3_LIVE_PAGE
    mov r3,#0x3C00
    mov r4,#RTS3_HEADER_BYTES
    bl rts3_copy_banked
    RTS_ADRL r1,S_RTS_FLAG
    ldr r0,=RTS3_FOOTER
    ldrb r4,[r1],#1
    add r0,r0,#1
    mov r2,#15
1:  ldrb r3,[r1],#1
    strb r3,[r0],#1
    subs r2,r2,#1
    bne 1b
    ldr r0,=RTS3_FOOTER
    strb r4,[r0]
    mov r0,#0x180 @ SAVE_COMMITTED (in SRAM, not an SD flush acknowledgement)
    mov r1,#0
    bl rts3_diag
    ldmfd sp!,{r4-r11,pc}
    .ltorg

@ Complete non-destructive preflight. Return r0=1 only for this ROM/runtime/ABI,
@ intact manifest + payload, and a coherent IRQ return frame. A diagnostic may
@ change, but game RAM and the live captured CPU/HW are not overwritten here.
rts3_validate:
    stmfd sp!,{r4-r11,lr}
    mov r0,#RTS3_META_PAGE
    bl SetRampage
    mov r11,#1 @ footer/format
    ldr r4,=RTS3_FOOTER
    RTS_ADRL r5,S_RTS_FLAG
    mov r6,#16
1:  ldrb r0,[r4],#1
    ldrb r1,[r5],#1
    cmp r0,r1
    bne rts3_validate_reject
    subs r6,r6,#1
    bne 1b
    mov r11,#2 @ identity/fixed header
    ldr r4,=RTS3_HEADER
    RTS_ADRL r5,rts3_header_template
    mov r6,#0x3C
2:  ldrb r0,[r4],#1
    ldrb r1,[r5],#1
    cmp r0,r1
    bne rts3_validate_reject
    subs r6,r6,#1
    bne 2b
    ldr r4,=RTS3_HEADER
    mov r0,r4
    bl rts3_header_crc
    RTS_GET32 r1,r4,0x3C,r2
    mov r11,#3
    cmp r0,r1
    bne rts3_validate_reject
    add r4,r4,#0x40
    mov r5,#RTS3_REGION_COUNT
3:  RTS_GET32 r0,r4,0,r2
    RTS_GET32 r1,r4,4,r2
    mvn r0,r0
    cmp r0,r1
    mov r11,#4
    bne rts3_validate_reject
    add r4,r4,#8
    subs r5,r5,#1
    bne 3b
    @ Reserved manifest bytes have defined zero semantics, not hidden state.
    mov r5,#0x30
4:  ldrb r0,[r4],#1
    cmp r0,#0
    bne rts3_validate_reject
    subs r5,r5,#1
    bne 4b
    bl rts3_diag_init
    mov r0,#0x200 @ MANIFEST_OK
    mov r1,#0
    bl rts3_diag
    RTS_ADRL r5,rts3_regions
    mov r8,#0
rts3_validate_region:
    ldmia r5,{r6,r7,r9,r10,r11}
    mov r0,r7
    bl SetRampage
    add r0,r9,#0x0E000000
    mov r1,r10
    bl rts3_crc
    mov r4,r0
    mov r0,#RTS3_META_PAGE
    bl SetRampage
    ldr r0,=(RTS3_HEADER+0x40)
    add r0,r0,r8,lsl #3
    RTS_GET32 r1,r0,0,r2
    cmp r4,r1
    add r11,r8,#0x100
    bne rts3_validate_reject
    mov r0,#0x210
    mov r1,r8
    bl rts3_diag
    add r5,r5,#20
    add r8,r8,#1
    cmp r8,#RTS3_REGION_COUNT
    blo rts3_validate_region
    bl rts3_validate_cpu
    cmp r0,#1
    mov r11,#5
    bne rts3_validate_reject
    mov r0,#0x280 @ PREFLIGHT_OK, no restore writes yet
    mov r1,#0
    bl rts3_diag
    mov r0,#1
    ldmfd sp!,{r4-r11,pc}
rts3_validate_reject:
    mov r0,#0xE00
    mov r1,r11
    bl rts3_diag
    mov r0,#0
    ldmfd sp!,{r4-r11,pc}
    .ltorg

@ CPU frame checks (including its duplicate in saved IWRAM). This is not a
@ signature/authentication boundary against a malicious state author; it is
@ strict corruption and wrong-context protection for the hardware return path.
rts3_validate_cpu:
    stmfd sp!,{r4-r11,lr}
    mov r0,#RTS3_META_PAGE
    bl SetRampage
    ldr r0,=RTS3_CPU
    ldr r1,=RTS3_WORK_BUFFER
    mov r2,#RTS3_CPU_BYTES
    bl rts3_from_sram
    ldr r4,=RTS3_WORK_BUFFER
    ldr r0,[r4,#0xC0]
    ldr r1,=0x33555043
    cmp r0,r1
    bne rts3_cpu_bad
    ldr r0,[r4,#0xBC]
    cmp r0,#0x3F
    bne rts3_cpu_bad
    ldr r5,[r4,#0x40]
    and r0,r5,#31
    cmp r0,#0x10
    cmpne r0,#0x1F
    cmpne r0,#0x13
    bne rts3_cpu_bad
    ldr r0,[r4,#0x44]
    and r0,r0,#0xBF
    cmp r0,#0x92 @ IRQ, ARM, IRQ-disabled
    bne rts3_cpu_bad
    ldr r0,[r4,#0x3C]
    tst r5,#0x20
    bne 8f
    tst r0,#3
    bne rts3_cpu_bad
    b 9f
8:  tst r0,#1
    bne rts3_cpu_bad
9:
    bl rts3_executable
    cmp r0,#1
    bne rts3_cpu_bad
    ldr r0,[r4,#0x64] @ BIOS lr_irq
    sub r0,r0,#4
    ldr r1,[r4,#0x3C]
    cmp r0,r1
    bne rts3_cpu_bad
    ldr r6,[r4,#0x48]
    tst r6,#3
    bne rts3_cpu_bad
    mov r0,r6,lsr #24
    cmp r0,#3
    bne rts3_cpu_bad
    mov r0,r6,lsl #17
    mov r0,r0,lsr #17
    cmp r0,#0x100
    blo rts3_cpu_bad
    ldr r1,=0x7FD8
    cmp r0,r1
    bhi rts3_cpu_bad
    ldr r0,[r4,#0x4C]
    tst r0,#3
    bne rts3_cpu_bad
    cmp r0,#0x100
    blo rts3_cpu_bad
    cmp r0,#0x4000
    bhs rts3_cpu_bad
    ldr r0,[r4,#0xB8]
    RTS_ADRL r1,RTS_irq
    cmp r0,r1
    bne rts3_cpu_bad
    ldr r0,[r4,#0xB4]
    bic r0,r0,#1
    bl rts3_executable
    cmp r0,#1
    bne rts3_cpu_bad
    and r0,r5,#31
    cmp r0,#0x13
    addeq r7,r4,#0x70
    addne r7,r4,#0x68
    ldr r0,[r4,#0x34]
    ldr r1,[r7]
    cmp r0,r1
    bne rts3_cpu_bad
    tst r0,#3
    bne rts3_cpu_bad
    mov r1,r0,lsr #24
    cmp r1,#2
    ldreq r2,=0x02040000
    beq 7f
    cmp r1,#3
    bne rts3_cpu_bad
    ldr r2,=0x03008000
7:  cmp r0,r2
    bhi rts3_cpu_bad
    ldr r0,[r4,#0x38]
    ldr r1,[r7,#4]
    cmp r0,r1
    bne rts3_cpu_bad
    mov r7,#0
1:  ldr r0,[r4,r7]
    add r1,r4,#0x50
    ldr r1,[r1,r7]
    cmp r0,r1
    bne rts3_cpu_bad
    add r7,r7,#4
    cmp r7,#16
    blo 1b
    ldr r0,[r4,#0x30]
    ldr r1,[r4,#0x60]
    cmp r0,r1
    bne rts3_cpu_bad
    @ The readable HW block must be plausible, not a blindly replayed dump.
    ldr r7,=RTS3_HW
    RTS_GET32 r0,r7,0,r1
    ldr r1,=0x33574852
    cmp r0,r1
    bne rts3_cpu_bad
    RTS_GET32 r0,r7,0x3C,r1
    cmp r0,#3
    bne rts3_cpu_bad
    RTS_GET16 r0,r7,HW_IME,r1
    cmp r0,#1
    bhi rts3_cpu_bad
    RTS_GET16 r0,r7,HW_VCOUNT,r1
    cmp r0,#160
    blo rts3_cpu_bad
    cmp r0,#228
    bhs rts3_cpu_bad
    RTS_GET16 r0,r7,HW_DISPCNT,r1
    and r0,r0,#7
    cmp r0,#5
    bhi rts3_cpu_bad
    mov r0,#0x80
    bl SetRampage
    mov r6,r6,lsl #17
    mov r6,r6,lsr #17
    add r6,r6,#0x0E000000
    add r7,r4,#0x50
    mov r8,#24
2:  ldrb r0,[r6],#1
    ldrb r1,[r7],#1
    cmp r0,r1
    bne rts3_cpu_bad
    subs r8,r8,#1
    bne 2b
    ldr r6,=0x0E007FF4
    RTS_GET32 r0,r6,0,r2
    ldr r1,[r4,#0xB4]
    cmp r0,r1
    bne rts3_cpu_bad
    RTS_GET32 r0,r6,8,r2
    ldr r1,[r4,#0xB8]
    cmp r0,r1
    bne rts3_cpu_bad
    mov r0,#1
    ldmfd sp!,{r4-r11,pc}
rts3_cpu_bad:
    mov r0,#0
    ldmfd sp!,{r4-r11,pc}
@ executable address predicate, r0 in / Boolean out; r1-r3 may change.
rts3_executable:
    mov r1,r0,lsr #24
    cmp r1,#8
    cmpne r1,#9
    beq 1f
    cmp r0,#0x4000
    blo 1f
    cmp r1,#2
    bne 2f
    ldr r1,=0x02040000
    cmp r0,r1
    blo 1f
2:  mov r1,r0,lsr #24
    cmp r1,#3
    bne 3f
    ldr r1,=0x03008000
    cmp r0,r1
    blo 1f
3:  mov r0,#0
    bx lr
1:  mov r0,#1
    bx lr
    .ltorg

@ Destructive restore begins only after preflight. Every destination gets its
@ own CRC reread before execution continues; the VRAM workspace is last.
rts3_load:
    mov r0,#0x04000000
    ldrh r1,[r0]
    orr r1,r1,#0x80
    strh r1,[r0] @ blank before any destructive video restore
    RTS_ADRL r5,rts3_regions
    mov r8,#0
rts3_load_region:
    mov r0,#0x300
    mov r1,r8
    bl rts3_diag
    ldmia r5,{r6,r7,r9,r10,r11}
    cmp r11,#0
    bne rts3_load_staged
    mov r0,r7
    bl SetRampage
    add r0,r9,#0x0E000000
    mov r1,r6
    mov r2,r10
    bl rts3_from_sram
    b rts3_load_check
rts3_load_staged:
    mov r0,#RTS3_LIVE_PAGE
    sub r1,r6,#0x0E000000
    mov r2,r7
    mov r3,r9
    mov r4,r10
    bl rts3_copy_banked
rts3_load_check:
    mov r0,#RTS3_LIVE_PAGE
    bl SetRampage
    mov r0,r6
    mov r1,r10
    bl rts3_crc
    mov r4,r0
    mov r0,#RTS3_META_PAGE
    bl SetRampage
    ldr r0,=(RTS3_HEADER+0x40)
    add r0,r0,r8,lsl #3
    RTS_GET32 r1,r0,0,r2
    cmp r4,r1
    bne rts3_load_failed
    cmp r8,#13
    bne 1f
    mov r0,#RTS3_LIVE_PAGE
    bl SetRampage
    ldr r0,=(RTS3_CONTROL+4)
    mov r1,r4
    bl rts3_put32
1:  mov r0,#0x310
    mov r1,r8
    bl rts3_diag
    add r5,r5,#20
    add r8,r8,#1
    cmp r8,#RTS3_REGION_COUNT
    blo rts3_load_region
    mov r0,#0x380 @ RAM + staged CPU/HW + staged workspace destination CRCs OK
    mov r1,#0
    bl rts3_diag
    mov r0,#RTS3_MODE_LOAD
    b rts3_finish
rts3_load_failed:
    mov r0,#0xE10
    mov r1,r8
    bl rts3_diag
    b rts3_halt
rts3_halt: @ partial state cannot safely return; solid red screen = restore fault
    mov r0,#0x04000000
    mov r1,#0x80
    strh r1,[r0]
    mov r1,#0
    add r2,r0,#0x200
    strh r1,[r2,#8]
    mov r2,#0x05000000
    mov r1,#31
    strh r1,[r2]
    mov r1,#0
    strh r1,[r0]
1:  b 1b
    .ltorg

@ Normal menu/SAVE returns and LOAD are distinct policies. No diagnostic DMA
@ disabling is inherited from f6/f7 on normal exit. LOAD remains an explicitly
@ approximate live-latch restart; the format marks its unknown hardware state.
rts3_live_exit:
    mov r0,#RTS3_MODE_CANCEL
    b rts3_finish
rts3_save_exit:
    mov r0,#RTS3_MODE_SAVE
rts3_finish:
    mov r9,r0
    mov r0,#RTS3_LIVE_PAGE
    bl SetRampage
    ldr r0,=RTS3_CONTROL
    mov r1,r9
    bl rts3_put32
    cmp r9,#RTS3_MODE_LOAD
    bne rts3_finish_normal_io
    bl rts3_restore_io
    b rts3_finish_banks
rts3_finish_normal_io:
    bl rts3_restore_menu_io
rts3_finish_banks:
    ldr r4,=RTS3_LIVE_CPU
    mrs r6,CPSR
    mov r0,#0xDF
    msr cpsr_c,r0
    nop
    RTS_GET32 sp,r4,0x68,r0
    RTS_GET32 lr,r4,0x6C,r0
    msr cpsr_c,r6
    nop
    mov r0,#0xD3
    msr cpsr_c,r0
    nop
    RTS_GET32 sp,r4,0x70,r0
    RTS_GET32 lr,r4,0x74,r0
    RTS_GET32 r1,r4,0x78,r0
    msr spsr_cxsf,r1
    msr cpsr_c,r6
    nop
    mov r0,#0xD7
    msr cpsr_c,r0
    nop
    RTS_GET32 sp,r4,0x7C,r0
    RTS_GET32 lr,r4,0x80,r0
    RTS_GET32 r1,r4,0x84,r0
    msr spsr_cxsf,r1
    msr cpsr_c,r6
    nop
    mov r0,#0xDB
    msr cpsr_c,r0
    nop
    RTS_GET32 sp,r4,0x88,r0
    RTS_GET32 lr,r4,0x8C,r0
    RTS_GET32 r1,r4,0x90,r0
    msr spsr_cxsf,r1
    msr cpsr_c,r6
    nop
    mov r0,#0xD1
    msr cpsr_c,r0
    nop
    RTS_GET32 r8,r4,0x94,r0
    RTS_GET32 r9,r4,0x98,r0
    RTS_GET32 r10,r4,0x9C,r0
    RTS_GET32 r11,r4,0xA0,r0
    RTS_GET32 r12,r4,0xA4,r0
    RTS_GET32 sp,r4,0xA8,r0
    RTS_GET32 lr,r4,0xAC,r0
    RTS_GET32 r1,r4,0xB0,r0
    msr spsr_cxsf,r1
    msr cpsr_c,r6
    nop
    RTS_GET32 r1,r4,0x40,r0
    msr spsr_cxsf,r1
    @ Reconstruct the BIOS frame explicitly; never assume today's IRQ SP.
    RTS_GET32 r5,r4,0x48,r0
    add r0,r4,#0x50
    mov r1,r5
    mov r2,#24
    bl rts3_from_sram
    RTS_GET32 lr,r4,0x4C,r0
    ldr r0,=RTS3_CONTROL
    ldrb r0,[r0]
    cmp r0,#RTS3_MODE_LOAD
    bne 1f
    RTS_ADRL lr,rts3_irq_return_probe
1:  mov sp,r5
    @ Load r4 last: r0 is the byte-bus CPU block pointer.
    mov r0,r4
    RTS_GET32 r5,r0,0x14,r1
    RTS_GET32 r6,r0,0x18,r1
    RTS_GET32 r7,r0,0x1C,r1
    RTS_GET32 r8,r0,0x20,r1
    RTS_GET32 r9,r0,0x24,r1
    RTS_GET32 r10,r0,0x28,r1
    RTS_GET32 r11,r0,0x2C,r1
    RTS_GET32 r4,r0,0x10,r1
    stmfd sp!,{r4-r11,lr}
    @ No live stack or buffer remains in VRAM. Restore its final 1 KiB now.
    ldr r0,=RTS3_WORK_BACKUP
    ldr r1,=RTS3_WORK_BASE
    mov r2,#RTS3_WORK_BYTES
    bl rts3_from_sram
    ldr r0,=RTS3_WORK_BASE
    mov r1,#RTS3_WORK_BYTES
    bl rts3_crc
    ldr r1,=RTS3_CONTROL
    RTS_GET32 r2,r1,4,r3
    cmp r0,r2
    bne rts3_tail_failed
    ldrb r0,[r1]
    cmp r0,#RTS3_MODE_LOAD
    bne 2f
    mov r0,#0x400 @ ABOUT_TO_CHAIN_GAME_IRQ, not proof of successful gameplay
    mov r1,#0
    bl rts3_diag
2:  mov r0,#RTS3_LIVE_PAGE
    bl SetRampage
    b rts3_rearm
rts3_tail_failed:
    ldr r0,=0xE11
    mov r1,#13
    bl rts3_diag
    b rts3_halt
    .ltorg

@ Normal exit only undoes the readable blend registers touched by the menu;
@ it does not replay wave RAM/sound/channel configuration unnecessarily.
rts3_restore_menu_io:
    ldr r0,=RTS3_LIVE_IO
    mov r3,#0x04000000
    RTS_GET16 r1,r0,14,r2
    strh r1,[r3,#0x50]
    RTS_GET16 r1,r0,16,r2
    strh r1,[r3,#0x52]
    bx lr
rts3_restore_io:
    stmfd sp!,{r4-r8,lr}
    ldr r4,=RTS3_LIVE_IO
    RTS_ADRL r5,rts3_io_registers
    mov r6,#28
    mov r7,#0x04000000
1:  ldrh r8,[r5],#2
    RTS_GET16 r1,r4,0,r2
    add r4,r4,#2
    cmp r8,#0x80 @ routing restored only at terminal rearm
    cmpne r8,#0x82
    beq 3f
    cmp r8,#4
    bne 2f
    ldr r2,=0xFF38 @ discard read-only VCOUNT/display status bits
    and r1,r1,r2
2:  cmp r8,#0x84
    andeq r1,r1,#0x80 @ SOUNDCNT_X master enable, not read-only channel status
    strh r1,[r7,r8]
3:  subs r6,r6,#1
    bne 1b
    ldmfd sp!,{r4-r8,pc}

@ The only remaining stack is the protected original IRQ frame. All values
@ needed after the final bank switch are staged in registers first.
rts3_rearm:
    ldr r0,=RTS3_LIVE_HW
    RTS_GET16 r4,r0,HW_DMA,r1
    RTS_GET16 r5,r0,(HW_DMA+2),r1
    RTS_GET16 r6,r0,(HW_DMA+4),r1
    RTS_GET16 r7,r0,(HW_DMA+6),r1
    RTS_GET32 r8,r0,HW_TIMER_CONTROL,r1
    RTS_GET32 r9,r0,(HW_TIMER_CONTROL+4),r1
    RTS_GET16 r10,r0,HW_IME,r1
    RTS_GET16 r11,r0,HW_DISPCNT,r1
    mov r12,#0x04000000
    RTS_GET16 r1,r0,HW_WAITCNT,r2
    add r2,r12,#0x200
    strh r1,[r2,#4]
    RTS_GET16 r1,r0,HW_IE,r3
    ldr r2,=RTS3_CONTROL
    ldrb r2,[r2]
    cmp r2,#RTS3_MODE_LOAD
    biceq r1,r1,#0x2000 @ LOAD-only GamePak quarantine
    add r2,r12,#0x200
    strh r1,[r2]
    RTS_GET16 r1,r0,HW_SOUND_L,r2
    strh r1,[r12,#0x80]
    RTS_GET16 r1,r0,HW_SOUND_H,r2
    bic r1,r1,#0x8800 @ FIFO reset strobes are not saved configuration
    strh r1,[r12,#0x82]
    mov r0,#0
    bl SetRampage
    mov r12,#0x04000000
    @ Bound the alignment wait: pathological VCOUNT must not strand the kernel.
    ldr r2,=0x100000
1:  ldrh r1,[r12,#6]
    cmp r1,#160
    bhs 2f
    subs r2,r2,#1
    bne 1b
2:  add r0,r12,#0x200
    mov r1,#0x2000
    strh r1,[r0,#2] @ acknowledge GamePak only; IF is write-one-to-clear
    @ DMA programming registers and timer reloads are not readable on hardware.
    @ Reuse retained live latches here, explicitly flagged in manifest. This is
    @ a restart policy, NOT restoration of mGBA's DMA progress/timer scheduler.
    strh r4,[r12,#0xBA]
    strh r5,[r12,#0xC6]
    strh r6,[r12,#0xD2]
    strh r7,[r12,#0xDE]
    add r1,r12,#0x100
    strh r8,[r1,#2]
    mov r8,r8,lsr #16
    strh r8,[r1,#6]
    strh r9,[r1,#10]
    mov r9,r9,lsr #16
    strh r9,[r1,#14]
    strh r11,[r12]
    strh r10,[r0,#8] @ final hardware write in the handoff
    ldmfd sp!,{r4-r11,lr}
    mov r0,#0x04000000
    ldr r1,[r0,#0x200]
    ldr r12,[r0,#-12]
    bx r12
    .ltorg

@ LOAD-only first-return marker. The original handler has actually returned
@ here, unlike a marker written just before branching to it. The BIOS then
@ restores r0-r3/r12/LR and performs its exception return. No periodic hook.
rts3_irq_return_probe:
    stmfd sp!,{r0-r12,lr}
    mrs r0,CPSR
    orr r0,r0,#0x80
    msr cpsr_c,r0 @ the BIOS exception return restores the game's IRQ mask
    mov r0,#0x04000000
    add r0,r0,#0x200
    ldrh r4,[r0,#8]
    mov r1,#0
    strh r1,[r0,#8]
    mov r0,#RTS3_LIVE_PAGE
    bl SetRampage
    ldr r0,=RTS3_LIVE_CPU
    RTS_GET32 r1,r0,0x4C,r2
    str r1,[sp,#52]
    mov r0,#0x500 @ FIRST_GAME_IRQ_RETURNED (still not a frame/gameplay witness)
    mov r1,#0
    bl rts3_diag
    mov r0,#0
    bl SetRampage
    mov r0,#0x04000000
    add r0,r0,#0x200
    mov r1,#0x2000
    strh r1,[r0,#2]
    strh r4,[r0,#8]
    ldmfd sp!,{r0-r12,pc}
    .ltorg
    .align 2
rts3_hw_registers:
    .hword 0xBA,0xC6,0xD2,0xDE,0x100,0x104,0x108,0x10C
    .hword 0x102,0x106,0x10A,0x10E,0x208,0x200,0x202,0x204
    .hword 0,4,6,0x84,0x80,0x82
rts3_io_registers:
    .hword 0x004,0x008,0x00A,0x00C,0x00E,0x048,0x04A,0x050,0x052
    .hword 0x084,0x060,0x062,0x068,0x070,0x072,0x078,0x080,0x082,0x088
    .hword 0x090,0x092,0x094,0x096,0x098,0x09A,0x09C,0x09E,0x132
    .align 2
rts3_regions: @ memory/staged address, cartridge page, low offset, bytes, staged
    .word 0x02000000,0x40,0,0x8000,0
    .word 0x02008000,0x48,0,0x8000,0
    .word 0x02010000,0x50,0,0x8000,0
    .word 0x02018000,0x58,0,0x8000,0
    .word 0x02020000,0x60,0,0x8000,0
    .word 0x02028000,0x68,0,0x8000,0
    .word 0x02030000,0x70,0,0x8000,0
    .word 0x02038000,0x78,0,0x8000,0
    .word 0x03000000,0x80,0,0x8000,0
    .word 0x05000000,0x88,0,0x400,0
    .word 0x06000000,0x90,0,0x8000,0
    .word 0x06008000,0x98,0,0x8000,0
    .word 0x06010000,0xA0,0,0x7C00,0
    .word RTS3_WORK_BACKUP,0xA0,0x7C00,0x400,1
    .word 0x07000000,0xA8,0,0x400,0
    .word RTS3_LIVE_CPU,0xA8,0x400,0x100,1
    .word RTS3_LIVE_HW,0xA8,0x500,0x80,1
    .word RTS3_LIVE_IO,0xA8,0x600,0x80,1
rts3_crc_table:
    .word 0x00000000,0x1DB71064,0x3B6E20C8,0x26D930AC
    .word 0x76DC4190,0x6B6B51F4,0x4DB26158,0x5005713C
    .word 0xEDB88320,0xF00F9344,0xD6D6A3E8,0xCB61B38C
    .word 0x9B64C2B0,0x86D3D2D4,0xA00AE278,0xBDBDF21C
rts3_header_template:
    .ascii "EZRTS3K0"
    .word 3,RTS3_HEADER_BYTES,RTS3_FILE_BYTES
    .word 0,0,0,0,0 @ game, ROM bytes, header CRC, runtime CRC, runtime address
    .word 1,RTS3_REGION_COUNT,RTS3_CAPABILITIES,RTS3_ABI,0,0
    .space 0xC0,0
