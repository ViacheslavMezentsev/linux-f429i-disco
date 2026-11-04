.syntax unified
.thumb
.text
.thumb_func
puts:
 push {r4,r5,r6,r7,lr}
 mov r4,r0
 movs r5,#0
scan:
 ldrb r2,[r4,r5]
 cbz r2,body
 adds r5,#1
 b scan
body:
 movs r6,#0
write_loop:
 cbz r5,next
 movs r0,#1
 mov r1,r4
 mov r2,r5
 movs r7,#4
 svc 0
 cmn r0,#4
 beq write_loop
 cmp r0,#0
 ble error
 adds r4,r0
 subs r5,r0
 b write_loop
next:
 cbnz r6,done
 adr r4,newline
 movs r5,#1
 movs r6,#1
 b write_loop
error:
 movs r0,#0
 mvns r0,r0
 pop {r4,r5,r6,r7,pc}
done:
 movs r0,#0
 pop {r4,r5,r6,r7,pc}
.balign 4
newline: .byte 10,0
