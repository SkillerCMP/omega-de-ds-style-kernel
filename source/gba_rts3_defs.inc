@ RTS3 kernel ABI. Fixed file size; no C calls or dynamic allocation in IRQ.
@ Explicit PC-relative address macro, accepted by GNU as and LLVM ARM assembler.
@ The data word is at the ADD's architectural PC; no absolute relocation.
.macro RTS_ADRL reg, target
    ldr \reg, 991f
    add \reg, pc, \reg
    b 992f
991: .word \target - 991b
992:
.endm

@ SRAM is an 8-bit bus. Never use LDRH/LDR/STRH/STR for its contents.
.macro RTS_GET16 dst, base, off, tmp
    ldrb \dst, [\base, #\off]
    ldrb \tmp, [\base, #(\off + 1)]
    orr \dst, \dst, \tmp, lsl #8
.endm
.macro RTS_GET32 dst, base, off, tmp
    RTS_GET16 \dst, \base, \off, \tmp
    ldrb \tmp, [\base, #(\off + 2)]
    orr \dst, \dst, \tmp, lsl #16
    ldrb \tmp, [\base, #(\off + 3)]
    orr \dst, \dst, \tmp, lsl #24
.endm


@ RTS3. All cartridge accesses use the low 32 KiB mapped window.
RTS3_ABI = 0x138A0001
RTS3_FILE_BYTES = 0x70000
RTS3_REGION_COUNT = 18
RTS3_HEADER_BYTES = 0x100
RTS3_LIVE_PAGE = 0x30
RTS3_META_PAGE = 0xA8
RTS3_LIVE_HW = 0x0E003000
RTS3_CONTROL = 0x0E003080
RTS3_LIVE_CPU = 0x0E003100
RTS3_LIVE_IO = 0x0E003200
RTS3_WORK_BACKUP = 0x0E003800
RTS3_LIVE_HEADER = 0x0E003C00
RTS3_WORK_BASE = 0x06017C00
RTS3_WORK_TOP = 0x06018000
RTS3_WORK_BYTES = 0x400
RTS3_WORK_BUFFER = 0x06017C00 @ bottom 0x100 bytes; stack uses remaining 0x300
RTS3_CPU = 0x0E000400 @ physical file offset 0x68400, page A8
RTS3_HW = 0x0E000500
RTS3_IO = 0x0E000600
RTS3_HEADER = 0x0E000800
RTS3_DIAG = 0x0E000C00
RTS3_FOOTER = 0x0E007FF0
RTS3_CAPABILITIES = 0x000F0001 @ IRQ-boundary checkpoint; four untracked HW classes
RTS3_CPU_BYTES = 0x100
RTS3_HW_BYTES = 0x80
RTS3_IO_BYTES = 0x80
RTS3_MODE_CANCEL = 0
RTS3_MODE_LOAD = 1
RTS3_MODE_SAVE = 2
HW_DMA = 0x08
HW_TIMER_COUNT = 0x10 @ observations, NOT reload values
HW_TIMER_CONTROL = 0x18
HW_IME = 0x20
HW_IE = 0x22
HW_IF = 0x24 @ observation, NOT a replayable register
HW_WAITCNT = 0x26
HW_DISPCNT = 0x28
HW_DISPSTAT = 0x2A
HW_VCOUNT = 0x2C
HW_SOUND_MASTER = 0x2E
HW_SOUND_L = 0x30
HW_SOUND_H = 0x32
.macro RTS3_PUT reg, base, off, tmp
    strb \reg, [\base, #\off]
    mov \tmp, \reg, lsr #8
    strb \tmp, [\base, #(\off+1)]
    mov \tmp, \reg, lsr #16
    strb \tmp, [\base, #(\off+2)]
    mov \tmp, \reg, lsr #24
    strb \tmp, [\base, #(\off+3)]
.endm
