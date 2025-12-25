#include <asm.h>

.extern __bss_start
.extern __BSS_END__

.equ USER_STACK_TOP, 0x5f000000

.section ".entry_function","ax"
ENTRY(_start)

    /* TODO: [p1-task3] setup C runtime environment for the user program */

    // clear bss
    la t0, __bss_start
    la t1, __BSS_END__
    mv t2, t0
.clear:
    sw zero, 0(t2)
    addi t2, t2, 4
    blt t2, t1, .clear

.setup:
    // setup stack
    la sp, USER_STACK_TOP


    /* TODO: [p1-task3] enter main function */
.enter:
    addi sp, sp, -8
    sd ra, 0(sp)
    jal main


    /* TODO: [p1-task3] finish task and return to the kernel, replace this in p3-task2! */
.ret:
    ld ra, 0(sp)
    addi sp, sp, 8
    ret

    /************************************************************/
	/* Do not touch this comment. Reserved for future projects. */
	/************************************************************/
// while(1) loop, unreachable here
loop:
    wfi
    j loop

END(_start)
