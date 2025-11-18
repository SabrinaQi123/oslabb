/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define KERNEL_STACK		0x50500000

.section ".entry_function","ax"
ENTRY(_start)
  /* Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero

  /* TODO: [p1-task2] clear BSS for flat non-ELF images */
  la t0, __bss_start      # t0 = BSS 起始地址
  la t1, __BSS_END__      # t1 = BSS 结束地址

do_clear:
    sw zero, 0(t0)      # *t0 = 0
    addi t0, t0, 4      # t0 += 4，每次写 4 个字节
    bltu t0, t1, do_clear


  /* TODO: [p1-task2] setup C environment */

  la sp, KERNEL_STACK     # sp = 0x50500000  
  call main
  
loop:
  wfi
  j loop

END(_start)
