.syntax unified
.cpu cortex-m4
.thumb
.section .text
.global _start
.thumb_func
_start:
    movs r7, #20                /* ARM EABI getpid */
    svc #0
    cmp r0, #1
    bne idle
    adr r4, message
    movs r5, #(message_end - message)
send:
    movs r0, #1                 /* stdout inherited from /dev/console */
    mov r1, r4
    mov r2, r5
    movs r7, #4                 /* write */
    svc #0
    cmp r0, #0
    ble idle
    adds r4, r4, r0
    subs r5, r5, r0
    bne send
idle:
    movs r7, #29                /* pause, keep PID 1 alive */
    svc #0
    b idle
.balign 4
message:
    .ascii "F429-DISCO /init: PID=1 userspace reached\n"
message_end:
.balign 4
