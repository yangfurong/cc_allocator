#ifndef __MEM_BASIS__
#define __MEM_BASIS__

#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define HPAGE_SHIFT (21)
#define HPAGE_SIZE (1 << HPAGE_SHIFT)
#define MEMORY_SIZE (8 * HPAGE_SIZE)

//addr in uint64_t format
#define HPAGE_OFFSET(addr) (addr & (HPAGE_SIZE - 1))

#define ROUNDUP(size) \
    ((size + (HPAGE_SIZE - 1)) & ~(HPAGE_SIZE - 1))
#define PAGE_SHIFT 12
#define PAGE_SIZE (1<<PAGE_SHIFT)
#define PAGEMAP_MASK_PFN        (((uint64_t)1 << 55) - 1)
#define PAGEMAP_PAGE_PRESENT    ((uint64_t)1 << 63)

#define HPAGE_DIFF(orig, addr) ((uint64_t)(addr - orig) >> HPAGE_SHIFT)

struct memzone_s{
    char *hugefile;
    int fd;
    void *addr;
    int size;
    uint64_t *pfn_map;
};
typedef struct memzone_s memzone_t;

static int addr_pfn(void *addr, uint64_t *pfnbuf, int n)
{
    int fid, i;

    fid = open("/proc/self/pagemap", O_RDONLY);

    if(fid < 0)
    {
        perror("failed to open pagemap address translator");
        return -1;
    }

    for(i=0;i<n;i++) {

        if(lseek(fid, (((unsigned long)addr + i*HPAGE_SIZE) >> PAGE_SHIFT) * 8, SEEK_SET) == (off_t)-1)
        {
            perror("failed to seek to translation start address");
            close(fid);
            return -1;
        }

        if(read(fid, pfnbuf+i, 8) < 8)
        {
            perror("failed to read in all pfn info");
            close(fid);
            return -1;
        }

        if(pfnbuf[i] & PAGEMAP_PAGE_PRESENT)
        {
            pfnbuf[i] &= PAGEMAP_MASK_PFN;
        }
        else
        {
            pfnbuf[i] = 0;
        }
        printf("vaddr %p paddr 0x%lx\n", addr + i*HPAGE_SIZE, pfnbuf[i] << PAGE_SHIFT);
    }
    close(fid);
    return 0;
}

static void memzone_destroy(memzone_t *mz){
    free(mz->pfn_map);
    munmap(mz->addr, mz->size);
    close(mz->fd);
    unlink(mz->hugefile);
    free(mz->hugefile);
    free(mz);
}

static memzone_t* memzone_create(char *hugefile, int memory_size)
{
    int fd;
    void *addr;
    uint64_t *pfnbuf;
    memzone_t *mz;

    memory_size = ROUNDUP(memory_size);

    fd = open(hugefile, O_CREAT | O_RDWR, S_IRWXU);
    if (fd < 0) {
        perror("open hugetlbfs file error!");
        return NULL;
    }
    addr = mmap(0, memory_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); 
    if(addr == MAP_FAILED){ 
        perror("map hugetlbfs error!"); 
        goto error;
    }   

    int nr = memory_size/HPAGE_SIZE;

    pfnbuf = (uint64_t*)malloc(nr * sizeof(uint64_t));
    if (!pfnbuf) {
        perror("create pfnbuf error!");
        goto merror;
    }

    int k;
    for(k=0;k<nr;k++)
    {
        *(unsigned char*)(addr + k * HPAGE_SIZE) = 'x';
        *(unsigned char*)(addr + PAGE_SIZE) = 'm';
    }

    if(addr_pfn(addr, pfnbuf, nr) < 0){
        goto pfnerror;
    }

    mz = (memzone_t*)malloc(sizeof(memzone_t));
    if(!mz){
        perror("create memzone_t structure error!");
        goto mzerror;
    }
    mz->addr = addr;
    mz->pfn_map = pfnbuf;
    mz->fd = fd;
    mz->hugefile = (char*)malloc(strlen(hugefile) + 1);
    mz->size = memory_size;
    if(!mz->hugefile){
        perror("hugefile name alloc error!");
        goto hnerror;
    }
    strcpy(mz->hugefile, hugefile);
    return mz;

hnerror:
    free(mz);
mzerror:
pfnerror:
    free(pfnbuf);
merror:
    munmap(addr, memory_size); 
error:
    close(fd); 
    unlink(hugefile);
    return NULL;
}

#endif
