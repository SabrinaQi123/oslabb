#include "asm/regs.h"
#include <assert.h>
#include <os/list.h>
#include <os/lock.h>
#include <os/mm.h>
#include <os/sched.h>
#include <os/time.h>
#include <printk.h>
#include <screen.h>

pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack,
    .name = "init",
};

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* global process id */
pid_t process_id = 1;

#define SCHED_FRAME_OFFSET   "56"
#define SCHED_FRAME_OFFSET_I 56

#define LOAD_SCHED_RA(var, sp) \
    asm volatile("ld %0, " SCHED_FRAME_OFFSET "(%1)" : "=r"(var) : "r"(sp))

void do_scheduler(void) {
    // asm volatile("mv %0, sp" : "=r"(sp));
    // printk("pid: %d, sp: 0x%x", current_running->pid, sp);
    // TODO: [p2-task3] Check sleep queue to wake up PCBs

    /************************************************************/
    /* Do not touch this comment. Reserved for future projects. */
    /************************************************************/

    // TODO: [p2-task1] Modify the current_running pointer.

    // simulate kernel state entrance
    asm volatile("sd sp, %0" ::"m"(current_running->user_sp));
    if (current_running->pid) {
        asm volatile("ld sp, %0" : "=m"(current_running->kernel_sp));
    } else {
        asm volatile("sd sp, %0" ::"m"(current_running->kernel_sp));
    }

    list_node_t* front_node;
    list_append(&ready_queue, &current_running->list);
    list_shift(&ready_queue, &front_node);
    assert(front_node);
    pcb_t* next_running = container_of(front_node, pcb_t, list);
    
    current_running->status = TASK_READY;
    next_running->status = TASK_RUNNING;

    // TODO: [p2-task1] switch_to current_running
    switch_to(current_running, next_running);

    asm volatile("sd sp, %0" ::"m"(current_running->kernel_sp));
    asm volatile("ld sp, %0" : "=m"(current_running->user_sp));
    ptr_t sp, target;
    asm volatile("mv %0, sp" : "=r"(sp));
    LOAD_SCHED_RA(target, sp);
}

void do_sleep(uint32_t sleep_time) {
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running is blocked.
}

void do_block(list_node_t* pcb_node, list_head* queue) {
    // TODO: [p2-task2] block the pcb task into the block queue
}

void do_unblock(list_node_t* pcb_node) {
    // TODO: [p2-task2] unblock the `pcb` from the block queue
}
