.syntax unified
.include "gba_rts3_defs.inc"
@;--------------------------------------------------------------------
	.section   	.rodata,"a",%progbits

	.global  RTS_only_ReplaceIRQ_start
	.global  RTS_only_ReplaceIRQ_end
	.global  RTS_only_Return_address_L
	.global  RTS_only_SAVE_key
	.global  RTS_only_LOAD_key
	.global  RTS_only_state_identity
	.global RTS_only_rts2_header



REG_BASE		= 0x4000000
REG_DISPCNT		= 0x00
REG_DISPSTAT	= 0x04
REG_VCOUNT		= 0x06
REG_BG0CNT		= 0x08
REG_BG1CNT		= 0x0A
REG_BG2CNT		= 0x0C
REG_BG3CNT		= 0x0E
REG_BG0HOFS		= 0x10
REG_BG0VOFS		= 0x12
REG_BG1HOFS		= 0x14
REG_BG1VOFS		= 0x16
REG_BG2HOFS		= 0x18
REG_BG2VOFS		= 0x1A
REG_BG3HOFS		= 0x1C
REG_BG3VOFS		= 0x1E
REG_WIN0H		= 0x40
REG_WIN1H		= 0x42
REG_WIN0V		= 0x44
REG_WIN1V		= 0x46
REG_WININ		= 0x48
REG_WINOUT		= 0x4A
REG_BLDCNT		= 0x50
REG_BLDALPHA	= 0x52
REG_BLDY		= 0x54
REG_SOUND1CNT_L	= 0x60
REG_SOUND1CNT_H	= 0x62
REG_SOUND1CNT_X	= 0x64
REG_SOUND2CNT_L	= 0x68
REG_SOUND2CNT_H	= 0x6C
REG_SOUND3CNT_L	= 0x70
REG_SOUND3CNT_H	= 0x72
REG_SOUND3CNT_X	= 0x74
REG_SOUND4CNT_L	= 0x78
REG_SOUND4CNT_H	= 0x7c
REG_SOUNDCNT_L		= 0x80
REG_SOUND2CNT_H		= 0x82
REG_SOUNDCNT_X		= 0x84
REG_SOUNDBIAS		= 0x88
REG_WAVE_RAM0_L		= 0x90
REG_FIFO_A_L	= 0xA0
REG_FIFO_A_H	= 0xA2
REG_FIFO_B_L	= 0xA4
REG_FIFO_B_H	= 0xA6
REG_DM0SAD		= 0xB0
REG_DM0DAD		= 0xB4
REG_DM0CNT_L	= 0xB8
REG_DM0CNT_H	= 0xBA
REG_DM1SAD		= 0xBC
REG_DM1DAD		= 0xC0
REG_DM1CNT_L	= 0xC4
REG_DM1CNT_H	= 0xC6
REG_DM2SAD		= 0xC8
REG_DM2DAD		= 0xCC
REG_DM2CNT_L	= 0xD0
REG_DM2CNT_H	= 0xD2
REG_DM3SAD		= 0xD4
REG_DM3DAD		= 0xD8
REG_DM3CNT_L	= 0xDC
REG_DM3CNT_H	= 0xDE
REG_TM0D		= 0x100
REG_TM0CNT		= 0x102
REG_IE			= 0x200
REG_IF			= 0x202
REG_P1			= 0x130
REG_P1CNT		= 0x132
REG_WAITCNT		= 0x204

@;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	.arm
RTS_only_ReplaceIRQ_start:
	MOV             R0, #0x4000000
	ADR             R1, RTS_irq
	STR             R1, [R0,#-4] @; 0x3FFFFFC = RTS_irq;
	LDR             R0, =0x12345678
	BX              R0
	.align
RTS_only_Return_address_L:
	.ltorg 			@;return address need modify
@;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
spend_0x80:
	.word 0x0203FE00   @; default
@;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
RTS_irq:
	LDR			R1, [R0,#0x200]
	TST			R1, #0x10000
	TSTEQ		R1, #0x10000000
	LDREQ		PC, [R0,#-0xC]			@;old_interrupt_handler

	add 		r2,r0,#0x100
	ldrh		r2,[r2,#0x30]		@;KEYINPUT (0x04000130), 16-bit

check_save:
    ldrh r1,[r0,#6]
    cmp r1,#160
    ldrlo pc,[r0,#-0xC]
    mrs r1,SPSR
    and r1,r1,#31
    cmp r1,#0x10
    cmpne r1,#0x1F
    cmpne r1,#0x13
    ldrne pc,[r0,#-0xC]
	RTS_ADRL 		r3,RTS_only_SAVE_key
	ldr 		r3,[r3]
	cmp 		r2,r3
	beq			call_Save
check_load:
	RTS_ADRL 		r3,RTS_only_LOAD_key
	ldr 		r3,[r3]
	cmp 		r2,r3
	beq 		call_Load
	ldr 		pc,[r0,#-(0x04000000-0x03FFFFF4)] @;to normal IRQ routine
@;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
@;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

@;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
RTS_only_SAVE_key:
	.word 0xFB @;L+R+select
RTS_only_LOAD_key:
	.word 0xF7 @;L+R+Start
@;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
@;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	.arm
@;------------------------------------------------------
SetRampage:
	ldr 	r1,=0xD200
	ldr 	r2,=0x1500
	ldr 	r3,=0x9fe0000
	strh 	r1,[r3]
	mov 	r3,#0x8000000
	strh  r2,[r3]
	ldr 	r3,=0x8020000
	strh 	r1,[r3]
	ldr 	r3,=0x8040000
	strh  r2,[r3]
	ldr 	r3,=0x9C00000
	strh  r0,[r3]
	ldr 	r3,=0x9FC0000
	strh  r2,[r3]
	bx		lr
@;------------------------------------------------------
WriteSram: @;(u32 address, u8 *data, u32 size)
	CMP     R2, #0
	BXEQ    LR
wSram_loop:
	LDR     R3, [R1],#4
	STRB    R3, [R0],#1
	LSR     R3, R3, #0x8
	STRB    R3, [R0],#1
	LSR     R3, R3, #0x8
	STRB    R3, [R0],#1
	LSR     R3, R3, #0x8
	STRB    R3, [R0],#1
	SUBS    R2, R2, #4
	BNE     wSram_loop
	BX      LR
@;------------------------------------------------------
ReadSram: @;(u32 address, u8 *data, u32 size)
	CMP     R2, #0
	BXEQ    LR
rSram_loop:
	LDRB    R4, [R0],#1
	LDRB    R3, [R0],#1
	ORR     R4, R4, R3, LSL #8
	LDRB    R3, [R0],#1
	ORR     R4, R4, R3, LSL #16
	LDRB    R3, [R0],#1
	ORR     R4, R4, R3, LSL #24
	STR     R4, [R1],#4
	SUBS    R2, R2, #4
	BNE     rSram_loop
	BX      LR
@;------------------------------------------------------
@;------------------------------------------------------
restore2_IO:        @; IOaddress, offset
	LDRB    R4, [R1]
	LDRB    R3, [R1,#1]
	ORR     R4, R4, R3, LSL #8
	STRH    R4, [R0]
	bx lr
	.ltorg
@;------------------------------------------------------
@;------------------------------------------------------
call_Save:
    stmfd sp!,{r0-r12,lr}
    mov r0,sp
    bl rts3_enter
    bl rts3_save
    b rts3_save_exit
call_Load:
    stmfd sp!,{r0-r12,lr}
    mov r0,sp
    bl rts3_enter
    bl rts3_validate
    cmp r0,#1
    beq rts3_load
    b rts3_live_exit

	.align
@;------------------------------------------------

	.align
.include "gba_rts3_core.inc"
.set RTS_only_rts2_header, rts3_header_template
S_RTS_INVALID:
	.word 0x00000000,0x00000000,0x00000000,0x00000000
S_RTS_FLAG:
	.byte 'E','Z','R','T','S','O','0','3'
RTS_only_state_identity:
	.word 0x00000000,0x00000000	@ patched game code + ROM size
	.align
RTS_only_ReplaceIRQ_end:
   .end
