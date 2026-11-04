.syntax unified
.thumb
.global _start
.thumb_func
_start:
 ldr r0,[sp]
 ldr r1,[sp,#4]
 bl main
 b _exit
.macro sys name, nr
.global \name
.thumb_func
\name:
 push {r7,lr}
 movw r7,#\nr
 svc 0
 pop {r7,pc}
.endm
sys _exit,1
sys _read,3
sys _write,4
sys sysopen,5
sys _close,6
sys _lseek,19
sys _getpid,20
sys _kill,37
.global getheap
.thumb_func
getheap:
 push {r4,r5,r7,lr}
 mov r1,r0
 movs r0,#0
 movs r2,#3
 movs r3,#0x22
 mov r4,#-1
 movs r5,#0
 movs r7,#192
 svc 0
 pop {r4,r5,r7,pc}
