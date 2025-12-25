#include <asm/biosdef.h>

// os size location (os_size could be sectors [p1-task3] or bytes [p1-task4])
.equ os_size_loc, 0x502001fc
.equ task_start_loc, 0x502001fa
.equ task_size_loc, 0x502001f8
.equ task_num_loc, 0x502001f6

// kernel address (move kernel to here, and jmp here to start kernel)
.equ kernel, 0x50201000

// BIOS function entry (jump here to use BIOS APIs)
.equ bios_func_entry, 0x50150000

.equ bios_putstr, 9
.equ bios_sdread, 11

.equ temp, 0x50600000
.extern tasks

.text
.global main

main:
	// fence on all memory and I/O
	fence

.hello_world:
	// TODO: [p1-task1] call BIOS to print string "It's bootblock!"
	la a7, bios_putstr
	la a0, msg
	jal bios_func_entry


	// TODO: [p1-task2] call BIOS to read kernel in SD card
	
.load_kernel:
	la t0, os_size_loc
	lh t1, 0(t0) // kernel bytes
	la a0, temp
	add a1, t1, 511
	sra a1, a1, 9 // sectors = ((bytes + 511) / 512)
	li a2, 1
	la a7, bios_sdread
	// a0: memory_addr
	// a1: num_of_blocks
	// a2: block_id
	// a7: bios_sdread
	jal bios_func_entry

.copy_kernel:
	la a0, kernel
	la t0, os_size_loc
	lh a1, 0(t0) // kernel bytes
	jal copy_data



	// TODO: [p1-task4] load task-related arguments and pass them to kernel
.load_taskinfo:
	la t0, task_start_loc
	lh a2, 0(t0) // tasks start block_id
	la t0, task_size_loc
	lh t1, 0(t0) // tasks bytes
	add a1, t1, 511
	sra a1, a1, 9
	la a0, temp
	la a7, bios_sdread
	jal bios_func_entry

//.copy_taskinfo:
//	la a0, tasks
//	la t0, task_size_loc
//	lh a1, 0(t0)
//	jal copy_data


	// TODO: [p1-task2] jump to kernel to start UCAS-OS
.start_kernel:
	la t0, task_num_loc
	lh a0, 0(t0)
	la a1, temp
	j kernel

	/************************************************************/
	/* Do not touch this comment. Reserved for future projects. */
	/************************************************************/
// while(1) --> stop here
stop:
	j stop
	nop

copy_data:
	// a0: dest
	// a1: src size (src locations is set to `temp`)
.copy_start:
	mv t0, a0 // t0: dest_cur
	la t1, temp // t1: src_cur
	la t2, temp
	add t2, t2, a1 // t2: src_end
.copy_loop:
	lw t3, 0(t1)
	sw t3, 0(t0)
	add t0, t0, 4
	add t1, t1, 4
	blt t1, t2, .copy_loop
.copy_end:
	ret

.data

msg: .string "It's wangheming23's bootloader...\n\r"

