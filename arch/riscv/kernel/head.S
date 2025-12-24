/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define KERNEL_STACK		0x50500000

.equ kernel_stack, 0x50500000

.extern __bss_start
.extern __BSS_END__

.section ".entry_function","ax"
ENTRY(_start)
  /* Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero

  /* TODO: [p1-task2] clear BSS for flat non-ELF images */

  la t0, __bss_start
  la t1, __BSS_END__
  mv t2, t0

.clear:
  sw zero, 0(t2)
  addi t2, t2, 4
  blt t2, t1, .clear

  /* TODO: [p1-task2] setup C environment */

  la sp, kernel_stack

  jal main


loop:
  wfi
  j loop

END(_start)
