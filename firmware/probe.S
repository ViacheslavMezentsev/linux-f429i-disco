.syntax unified
.cpu cortex-m4
.thumb

.section .vectors, "a"
.word stack_top
.word probe_start

.text
.global probe_start, probe_done
.thumb_func
probe_start:
    cpsid i
    ldr r0, =probe_result
    movw r1, #0x429
    str r1, [r0]
    dsb
probe_done:
    b probe_done

.bss
.balign 4
.global probe_result
probe_result:
    .space 4
