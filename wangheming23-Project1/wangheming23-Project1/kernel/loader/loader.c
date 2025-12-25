#include <os/kernel.h>
#include <os/string.h>
#include <os/task.h>
#include <type.h>

#define LOAD_TEMP_ADDR 0x5f000000

uint64_t load_task_img(task_info_t task) {
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */

    uint64_t entrance = task.entrance;
    int offset = task.phyaddr_start % SECTOR_SIZE;  //任务在 SD 卡扇区内的起始偏移量
    int src_blockid = task.phyaddr_start / SECTOR_SIZE;
    int aligned_phyaddr = task.phyaddr_start - offset;
    int nblocks = NBYTES2SEC(task.phyaddr_end - aligned_phyaddr);
    bios_sd_read(LOAD_TEMP_ADDR, nblocks, src_blockid);
    memcpy((void*)entrance, (void*)LOAD_TEMP_ADDR + offset, task.phyaddr_end - task.phyaddr_start);
    // bios_putstr("Loaded.\n");
    return entrance;
}