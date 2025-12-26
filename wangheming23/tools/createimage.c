#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>//added

#define IMAGE_FILE "./image"
#define ARGS "[--extended] [--vm] <bootblock> <executable-file> ..."

#define SECTOR_SIZE 512
#define BOOT_LOADER_SIG_OFFSET 0x1fe
#define OS_SIZE_LOC (BOOT_LOADER_SIG_OFFSET - 2)
//task4
#define TASKINFO_START_LOC     (BOOT_LOADER_SIG_OFFSET - 4)
#define TASKINFO_SIZE_LOC      (BOOT_LOADER_SIG_OFFSET - 6)
#define TASKINFO_TASKNUM_LOC   (BOOT_LOADER_SIG_OFFSET - 8)
#define BATCH_FILE_LOC         (BOOT_LOADER_SIG_OFFSET - 10) //task5


#define BOOT_LOADER_SIG_1 0x55
#define BOOT_LOADER_SIG_2 0xaa

#define NBYTES2SEC(nbytes) (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))

/* TODO: [p1-task4] design your own task_info_t */
typedef struct {
    char name[16];
    int phyaddr_start, phyaddr_end;
    uint64_t entrance;

} task_info_t;

#define TASK_MAXNUM 16
static task_info_t taskinfo[TASK_MAXNUM];

/* structure to store command line options */
static struct {
    int vm;
    int extended;
} options;

/* prototypes of local functions */
static void create_image(int nfiles, char *files[]);
static void error(char *fmt, ...);
static void read_ehdr(Elf64_Ehdr *ehdr, FILE *fp);
static void read_phdr(Elf64_Phdr *phdr, FILE *fp, int ph, Elf64_Ehdr ehdr);
static uint64_t get_entrypoint(Elf64_Ehdr ehdr);
static uint32_t get_filesz(Elf64_Phdr phdr);
static uint32_t get_memsz(Elf64_Phdr phdr);
static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr);
static void write_padding(FILE *img, int *phyaddr, int new_phyaddr);
// [Step 1] 自动填充 0，直到对齐到下一个扇区边界
static void write_align_padding(FILE* img, int* phyaddr) {
    int target = (((*phyaddr) + SECTOR_SIZE - 1) / SECTOR_SIZE) * SECTOR_SIZE;
    write_padding(img, phyaddr, target);
}
static void write_img_info(int nbytes_kernel, task_info_t *taskinfo,
                           short tasknum, FILE *img, int *phyaddr); // added phyaddr pointer

int main(int argc, char **argv)
{
    char *progname = argv[0];

    /* process command line options */
    options.vm = 0;
    options.extended = 0;
    while ((argc > 1) && (argv[1][0] == '-') && (argv[1][1] == '-')) {
        char *option = &argv[1][2];

        if (strcmp(option, "vm") == 0) {
            options.vm = 1;
        } else if (strcmp(option, "extended") == 0) {
            options.extended = 1;
        } else {
            error("%s: invalid option\nusage: %s %s\n", progname,
                  progname, ARGS);
        }
        argc--;
        argv++;
    }
    if (options.vm == 1) {
        error("%s: option --vm not implemented\n", progname);
    }
    if (argc < 3) {
        /* at least 3 args (createimage bootblock main) */
        error("usage: %s %s\n", progname, ARGS);
    }
    create_image(argc - 1, argv + 1);
    return 0;
}


/* TODO: [p1-task4] assign your task_info_t somewhere in 'create_image' */
static void create_image(int nfiles, char *files[])
{
    int tasknum = nfiles - 2;
    int nbytes_kernel = 0;
    int phyaddr = 0;
    FILE *fp = NULL, *img = NULL;
    Elf64_Ehdr ehdr;
    Elf64_Phdr phdr;

    /* open the image file */
    img = fopen(IMAGE_FILE, "w");
    assert(img != NULL);

    /* for each input file */
    /* [Step 3] 修改后的循环逻辑 */
    for (int fidx = 0; fidx < nfiles; ++fidx) {

        int taskidx = fidx - 2; // fidx 0=bootblock, 1=kernel, 2=app1...

        // 1. 记录任务开始位置 & 名字
        if (taskidx >= 0) {
            taskinfo[taskidx].phyaddr_start = phyaddr; 
            // 记录文件名 (比如 "app1")
            strncpy(taskinfo[taskidx].name, *files, 15);
            taskinfo[taskidx].name[15] = '\0';
        }

        /* open input file */
        fp = fopen(*files, "r");
        assert(fp != NULL);

        /* read ELF header */
        read_ehdr(&ehdr, fp);
        printf("0x%04lx: %s\n", ehdr.e_entry, *files);

        // 2. 记录入口地址
        if (taskidx >= 0) {
            taskinfo[taskidx].entrance = ehdr.e_entry;
        }

        /* for each program header */
        for (int ph = 0; ph < ehdr.e_phnum; ph++) {
            read_phdr(&phdr, fp, ph, ehdr);
            if (phdr.p_type != PT_LOAD) continue;

            /* write segment to the image */
            write_segment(phdr, fp, img, &phyaddr); // 注意：这里传的是 phyaddr 指针

            /* update nbytes_kernel */
            if (strcmp(*files, "main") == 0) {
                nbytes_kernel += get_filesz(phdr);
            }
        }

        /* write padding bytes */
        // 3. 对齐填充：Bootblock 固定 512，其他按扇区紧密排列
        if (strcmp(*files, "bootblock") == 0) {
            write_padding(img, &phyaddr, SECTOR_SIZE);
        } else {
            write_align_padding(img, &phyaddr); // 使用 Step 1 的辅助函数
        }

        // 4. 记录任务结束位置
        if (taskidx >= 0) {
            taskinfo[taskidx].phyaddr_end = phyaddr;
        }

        fclose(fp);
        files++;
    }

    // 循环结束后，调用写信息函数 
    write_img_info(nbytes_kernel, taskinfo, tasknum, img, &phyaddr);

    fclose(img);
}

static void read_ehdr(Elf64_Ehdr * ehdr, FILE * fp)
{
    int ret;

    ret = fread(ehdr, sizeof(*ehdr), 1, fp);
    assert(ret == 1);
    assert(ehdr->e_ident[EI_MAG1] == 'E');
    assert(ehdr->e_ident[EI_MAG2] == 'L');
    assert(ehdr->e_ident[EI_MAG3] == 'F');
}

static void read_phdr(Elf64_Phdr * phdr, FILE * fp, int ph,
                      Elf64_Ehdr ehdr)
{
    int ret;

    fseek(fp, ehdr.e_phoff + ph * ehdr.e_phentsize, SEEK_SET);
    ret = fread(phdr, sizeof(*phdr), 1, fp);
    assert(ret == 1);
    if (options.extended == 1) {
        printf("\tsegment %d\n", ph);
        printf("\t\toffset 0x%04lx", phdr->p_offset);
        printf("\t\tvaddr 0x%04lx\n", phdr->p_vaddr);
        printf("\t\tfilesz 0x%04lx", phdr->p_filesz);
        printf("\t\tmemsz 0x%04lx\n", phdr->p_memsz);
    }
}

static uint64_t get_entrypoint(Elf64_Ehdr ehdr)
{
    return ehdr.e_entry;
}

static uint32_t get_filesz(Elf64_Phdr phdr)
{
    return phdr.p_filesz;
}

static uint32_t get_memsz(Elf64_Phdr phdr)
{
    return phdr.p_memsz;
}

static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr)
{
    if (phdr.p_memsz != 0 && phdr.p_type == PT_LOAD) {
        /* write the segment itself */
        /* NOTE: expansion of .bss should be done by kernel or runtime env! */
        if (options.extended == 1) {
            printf("\t\twriting 0x%04lx bytes\n", phdr.p_filesz);
        }
        fseek(fp, phdr.p_offset, SEEK_SET);
        while (phdr.p_filesz-- > 0) {
            fputc(fgetc(fp), img);
            (*phyaddr)++;
        }
    }
}

static void write_padding(FILE *img, int *phyaddr, int new_phyaddr)
{
    if (options.extended == 1 && *phyaddr < new_phyaddr) {
        printf("\t\twrite 0x%04x bytes for padding\n", new_phyaddr - *phyaddr);
    }

    while (*phyaddr < new_phyaddr) {
        fputc(0, img);
        (*phyaddr)++;
    }
}

static void write_img_info(int nbytes_kernel, task_info_t *taskinfo,
                           short tasknum, FILE * img, int *phyaddr)
{
    // TODO: [p1-task3] & [p1-task4] write image info to some certain places
    // NOTE: os size, infomation about app-info sector(s) ...
    
    // write taskinfo
    write_align_padding(img, phyaddr);
    short taskinfo_start = (*phyaddr) / SECTOR_SIZE;
    short taskinfo_bytes = sizeof(task_info_t) * tasknum;
    fwrite(taskinfo, sizeof(task_info_t), tasknum, img), *phyaddr += taskinfo_bytes;
    if (options.extended) {
        printf("\ntaskinfo: \t%d bytes, starts at #%d sector\n", taskinfo_bytes, taskinfo_start);
    }

    // preserve batch_file sector
    write_align_padding(img, phyaddr);
    short batch_file_sector = (*phyaddr) / SECTOR_SIZE;
    if (options.extended) {
        printf("batch_file: \treserved at #%d sector\n", batch_file_sector);
    }
    write_padding(img, phyaddr, *phyaddr + SECTOR_SIZE);

    // write 2-byte taskinfo_start to TASKINFO_START_LOC
    fseek(img, TASKINFO_START_LOC, SEEK_SET);
    fwrite(&taskinfo_start, sizeof(taskinfo_start), 1, img);
    if (options.extended)
        printf(
            "taskinfo_start:\t%d,\t%lu bytes at 0x%08x\n", taskinfo_start, sizeof(taskinfo_start),
            TASKINFO_START_LOC);

    // write 2-byte taskinfo_size to TASKINFO_SIZE_LOC
    fseek(img, TASKINFO_SIZE_LOC, SEEK_SET);
    fwrite(&taskinfo_bytes, sizeof(taskinfo_bytes), 1, img);
    if (options.extended)
        printf(
            "taskinfo_bytes:\t%d,\t%lu bytes at 0x%08x\n", taskinfo_bytes, sizeof(taskinfo_bytes),
            TASKINFO_SIZE_LOC);

    // write 2-byte tasknum to TASKINFO_TASKNUM_LOC
    fseek(img, TASKINFO_TASKNUM_LOC, SEEK_SET);
    fwrite(&tasknum, sizeof(tasknum), 1, img);
    if (options.extended)
        printf(
            "tasknum: \t%d,\t%lu bytes at 0x%08x\n", tasknum, sizeof(tasknum),
            TASKINFO_TASKNUM_LOC);

    // write 2-byte batch_file_sector to BATCH_FILE_LOC
    fseek(img, BATCH_FILE_LOC, SEEK_SET);
    fwrite(&batch_file_sector, sizeof(batch_file_sector), 1, img);
    if (options.extended)
        printf(
            "batch_file_loc: %d,\t%lu bytes at 0x%08x\n", batch_file_sector,
            sizeof(batch_file_sector), BATCH_FILE_LOC);

    // write 2-byte size to OS_SIZE_LOC
    fseek(img, OS_SIZE_LOC, SEEK_SET);
    short os_size = nbytes_kernel;
    fwrite(&os_size, sizeof(os_size), 1, img);
    if (options.extended)
        printf("os_size: \t%d,\t%lu bytes at 0x%08x\n", os_size, sizeof(os_size), OS_SIZE_LOC);
}


/* print an error message and exit */
static void error(char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    if (errno != 0) {
        perror(NULL);
    }
    exit(EXIT_FAILURE);
}
