#include <asm.h>
#include <csr.h>

    ENTRY(setup_exception)

/* save exception_handler_entry into STVEC */
    la                       t0, exception_handler_entry
    csrw                     stvec, t0
/* TODO:
    [p2-task4]               enable interrupts globally */
    csrs sstatus, SR_SIE // 打开了 CPU 处理中断的总开关
    ret
    ENDPROC(setup_exception)
