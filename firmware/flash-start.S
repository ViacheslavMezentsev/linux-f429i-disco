.syntax unified
.cpu cortex-m4
.thumb
.section .vectors,"a"
.word 0x20020000
.word reset
.rept 14
.word fault
.endr
.text
.thumb_func
.global reset
reset:
 cpsid i
 ldr r0,=data_rom
 ldr r1,=data_start
 ldr r2,=data_end
1: cmp r1,r2
 bhs 2f
 ldr r3,[r0],#4
 str r3,[r1],#4
 b 1b
2: ldr r1,=bss_start
 ldr r2,=bss_end
 movs r3,#0
3: cmp r1,r2
 bhs 4f
 str r3,[r1],#4
 b 3b
4: bl boot
.thumb_func
fault: b fault
.thumb_func
.global enter_linux
enter_linux:
 ldr r3,=0x20001000
 msr msp,r3
 movs r0,#0
 mvn r1,#0
 ldr r2,=0x902f0000
 ldr r3,=0x90000001
 dsb
 isb
 bx r3
