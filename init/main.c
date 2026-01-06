#include <asm.h>
#include <asm/unistd.h>
#include <assert.h>
#include <common.h>
#include <csr.h>
#include <os/irq.h>
#include <os/kernel.h>
#include <os/loader.h>
#include <os/lock.h>
#include <os/mm.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/task.h>
#include <os/time.h>
#include <printk.h>
#include <screen.h>
#include <sys/syscall.h>
#include <type.h>

#define VERSION_BUF 50

#define TASK_RESULT 0x5fffff00

int version = 3;  // version must between 0 and 9
char buf[VERSION_BUF];

extern void ret_from_exception();

// Task info array
int task_num;
task_info_t tasks[TASK_MAXNUM];

static int bss_check(void) {
    for (int i = 0; i < VERSION_BUF; ++i) {
        if (buf[i] != 0) {
            return 0;
        }
    }
    return 1;
}

static void init_jmptab(void) {
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR] = (volatile long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (volatile long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (volatile long (*)())port_read_ch;
    jmptab[SD_READ] = (volatile long (*)())sd_read;
    jmptab[SD_WRITE] = (volatile long (*)())sd_write;
    jmptab[QEMU_LOGGING] = (volatile long (*)())qemu_logging;
    jmptab[SET_TIMER] = (volatile long (*)())set_timer;
    jmptab[READ_FDT] = (volatile long (*)())read_fdt;
    jmptab[MOVE_CURSOR] = (volatile long (*)())screen_move_cursor;
    jmptab[PRINT] = (volatile long (*)())printk;
    jmptab[YIELD] = (volatile long (*)())do_scheduler;
    jmptab[MUTEX_INIT] = (volatile long (*)())do_mutex_lock_init;
    jmptab[MUTEX_ACQ] = (volatile long (*)())do_mutex_lock_acquire;
    jmptab[MUTEX_RELEASE] = (volatile long (*)())do_mutex_lock_release;

    // TODO: [p2-task1] (S-core) initialize system call table.
    jmptab[CONSOLE_REFLUSH] = (volatile long (*)())screen_reflush;
}

static void init_task_info(void) {
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
}

/************************************************************/

#define KERNEL_STACK_PAGES 1
#define USER_STACK_PAGES   4

extern pcb_t pcb[NUM_MAX_TASK];
int pid_counter = 1;

static void init_pcb_stack(ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point, pcb_t* pcb) {
    /* TODO: [p2-task3] initialization of registers on kernel stack
     * HINT: sp, ra, sepc, sstatus
     * NOTE: To run the task in user mode, you should set corresponding bits
     *     of sstatus(SPP, SPIE, etc.).
     */
    regs_context_t* pt_regs = (regs_context_t*)(kernel_stack - sizeof(regs_context_t));

    /* TODO: [p2-task1] set sp to simulate just returning from switch_to
     * NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t* pt_switchto =
        (switchto_context_t*)((ptr_t)pt_regs - sizeof(switchto_context_t));

    pcb->kernel_sp = (ptr_t)pt_switchto;
    pcb->user_sp = user_stack;
    // pcb->user_sp = pcb->kernel_sp;
    pt_switchto->regs[SAVE_RA] = entry_point;
    pt_switchto->regs[SAVE_SP] = user_stack;
}

static void init_pcb(void) {
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
    current_running = &pid0_pcb;

    for (int i = 0; i < task_num; i++) {
        load_task_img(tasks[i]);
    }

    for (int i = 0; i < NUM_MAX_TASK; i++) {
        pcb[i].status = TASK_EXITED;
    }

    const char* run_tasks[] = {"print1", "print2", "fly"};

    for (int i = 0; i < sizeof(run_tasks) / sizeof(run_tasks[0]); i++) {
        task_info_t* task = NULL;
        for (int j = 0; j < task_num; j++) {
            if (strcmp(run_tasks[i], tasks[j].name) == 0) {
                task = &tasks[j];
                break;
            }
        }
        assert(task);
        printk("> [INIT] Loading task %s.\n", task->name);
        int kernel_stack_top = allocKernelPage(KERNEL_STACK_PAGES) + KERNEL_STACK_PAGES * PAGE_SIZE;
        int user_stack_top = allocUserPage(USER_STACK_PAGES) + USER_STACK_PAGES * PAGE_SIZE;
        pcb_t* alloc_pcb = NULL;
        for (int j = 0; j < NUM_MAX_TASK; j++) {
            if (pcb[j].status == TASK_EXITED) {
                alloc_pcb = &pcb[j];
                break;
            }
        }
        assert(alloc_pcb);
        alloc_pcb->pid = pid_counter;
        alloc_pcb->status = TASK_READY;
        strcpy(alloc_pcb->name, task->name);
        init_pcb_stack(kernel_stack_top, user_stack_top, task->entrance, alloc_pcb);
        list_append(&ready_queue, &alloc_pcb->list);
        pid_counter++;
    }
}

static void init_syscall(void) {
    // TODO: [p2-task3] initialize system call table.
}
/************************************************************/

static void writeint(int val) {
    if (val == 0)
        bios_putchar('0');
    else {
        if (val / 10) writeint(val / 10);
        bios_putchar('0' + val % 10);
    }
}

static void writeptr(void* ptr) {
    bios_putstr("0x");
    uint64_t val = (uint64_t)ptr;
    int started = 0;
    for (int i = 64; i >= 0; i -= 4) {
        int digit = (val >> i) & 0xf;
        if (digit || started || i == 0) {
            started = 1;
            if (digit < 10)
                bios_putchar('0' + digit);
            else
                bios_putchar('a' + (digit - 10));
        }
    }
}

static int getchar() {
    while (1) {
        int ch = bios_getchar();
        if (ch != -1) {
            return ch;
        }
    }
}

static int echoed_getchar() {
    int ch = getchar();
    bios_putchar(ch);
    if (ch == 127) bios_putstr("\b \b");
    // writeint(ch);
    if (ch == '\r') bios_putchar('\n');
    return ch;
}

static int isdigit(char c) { return c >= '0' && c <= '9'; }

static int isalpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

static int readint() {
    char c = echoed_getchar();
    while (!isdigit(c)) c = echoed_getchar();
    int val = 0;
    while (isdigit(c)) {
        val = val * 10 + (c - '0');
        c = echoed_getchar();
    }
    return val;
}

static int readline(char* buffer, int size) {
    int count = 0;
    while (count < size - 1) {
        char c = echoed_getchar();
        if (c == '\n' || c == '\r') {
            break;
        }
        if (c == 127) {
            if (count > 0) {
                buffer[--count] = 0;
            }
            continue;
        }
        buffer[count++] = c;
    }
    buffer[count] = 0;
    return count;
}

static void run_task(char* name) {
    task_info_t* task_info = NULL;
    for (int i = 0; i < task_num; i++) {
        if (strcmp(tasks[i].name, name) == 0) {
            task_info = tasks + i;
            break;
        }
    }
    if (!task_info) {
        bios_putstr("Invalid name!\n");
    } else {
        void (*task)() = (void (*)())(load_task_img(*task_info));
        bios_putstr("Loaded task.\n");
        task();
        bios_putstr("Task completed.\n");
    }
}

void write_batchfile(char* cmd, int location) { bios_sd_write((unsigned int)cmd, 1, location); }
void read_batchfile(char* cmd, int location) { bios_sd_read((unsigned int)cmd, 1, location); }

int main(int argc, char** argv) {
    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // INFO:
    // argc: argc
    // argv+0: int task_num
    // argv+8: task_info_t* task_info
    // argv+16: int batchfile_location
    if (argc != 3) {
        bios_putstr("Invalid argc!\n");
        return -1;
    }
    uint64_t* args = (void*)argv;
    task_num = args[0];
    task_info_t* task_info = (task_info_t*)args[1];
    memcpy((void*)tasks, (void*)task_info, sizeof(task_info_t) * task_num);
    int batchfile_location = args[2];

    // Check whether .bss section is set to zero
    int check = bss_check();
    if (!check) {
        bios_putstr("> [ERROR] .bss check failed");
        while (1) asm volatile("wfi");
    }

    bios_putstr("> [META] OS kernel arguments: \n");
    bios_putstr("> [META] task_num: "), writeint(task_num), bios_putstr("\n");
    bios_putstr("> [META] task_info: "), writeptr(task_info), bios_putstr("\n");
    bios_putstr("> [META] batchfile_location: "), writeint(batchfile_location), bios_putstr("\n");

    // Init Process Control Blocks |•'-'•) ✧
    init_pcb();
    printk("> [INIT] PCB initialization succeeded.\n");
    

    // while (true) {
    // int _ = echoed_bios_getchar();
    // bios_putchar(c);
    // bios_putchar('\n');
    // }
    // Read CPU frequency (｡•ᴗ-)_
    time_base = bios_read_fdt(TIMEBASE);

    // Init lock mechanism o(´^｀)o
    init_locks();
    printk("> [INIT] Lock mechanism initialization succeeded.\n");

    // Init interrupt (^_^)
    init_exception();
    printk("> [INIT] Interrupt processing initialization succeeded.\n");

    // Init system call table (0_0)
    init_syscall();
    printk("> [INIT] System call initialized successfully.\n");

    // Init screen (QAQ)
    init_screen();
    printk("> [INIT] SCREEN initialization succeeded.\n");

    // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
    // NOTE: The function of sstatus.sie is different from sie's

    while (1) {
        // If you do non-preemptive scheduling, it's used to surrender control
        do_scheduler();

        // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
        // enable_preempt();
        // asm volatile("wfi");
    }

    return 0;
}