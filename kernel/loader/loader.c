#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>
#include <printk.h>


uint64_t load_single_task(char *task_name);


uint64_t load_task_img(char *taskname)
{

    return load_single_task(taskname);
}


uint64_t load_single_task(char *task_name)
{
    // 遍历所有注册的任务 (在 image 尾部的信息)
    for (int i = 0; i < TASK_MAXNUM; i++)
    {
       
        if (tasks[i].block_nums > 0 && strcmp(task_name, tasks[i].task_name) == 0)
        {
          
            //固定分区策略 (TASK_MEM_BASE + i * SIZE)
            // 在 Project 4 引入虚存前，这种方式是可行的。
            uint64_t mem_addr = TASK_MEM_BASE + TASK_SIZE * i;

            // 2. 计算 SD 卡中的扇区位置
            int start_sec = tasks[i].start_addr / 512;
            
            // 3. 执行读取操作 (从 SD 卡 -> 内存)
           
            bios_sd_read(mem_addr, tasks[i].block_nums, start_sec);

            // 4. 计算并返回入口地址
            // 入口地址 = 内存基地址 + (文件内的偏移量)
            return mem_addr + (tasks[i].start_addr - start_sec * 512);
        }
    }
    
   
    return 0;
}

// 辅助功能：列出所有可用任务
void list_user_tasks()
{
    for (int i = 0; i < TASK_MAXNUM; i++) {
        if (strlen(tasks[i].task_name) > 0) {
        
            printk("%s\n", tasks[i].task_name); 
        }
    }
}