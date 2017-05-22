#ifndef __CC_ALLOCATOR__
#define __CC_ALLOCATOR__

#include "mem_basis.h"
#include "list.h"

//platform dependent
#define CC_PAGE_SIZE 4096
#define CC_PAGE_SHIFT 12
#define CC_CACHE_SIZE (256 * 1024)
#define CC_CACHE_LINE_SIZE 64
#define CC_CACHE_WAY 8
////////////////////////////////////////////
#define CC_COLOR_MAX \
    (CC_CACHE_SIZE / CC_PAGE_SIZE / CC_CACHE_WAY)
#define CC_CACHE_LINE_ROUNDUP(size) \
    ((size + CC_CACHE_LINE_SIZE - 1) & ~(CC_CACHE_LINE_SIZE - 1))
//uint64_t phys_addr
#define CC_GET_COLOR(phys_addr) \
    ((phys_addr >> CC_PAGE_SHIFT) % CC_COLOR_MAX)

#define CC_MEM_FACTOR 2

struct obj_s{
    void *addr;
    struct list_head list;
};
typedef struct obj_s obj_t;

struct cc_allocator_s{
    //object parameters
    int obj_nr;
    int obj_size;
    //color parameters
    int grp_nr;
    int *grp_colors;
    struct list_head *grps;
    //memory zone
    memzone_t *zone;
};
typedef struct cc_allocator_s cc_allocator_t;

cc_allocator_t* cc_allocator_create(int obj_nr, int obj_size,
        int grp_nr, int *grp_ratio);
void cc_allocator_destroy(cc_allocator_t *allocator);
void* cc_allocator_alloc(cc_allocator_t *allocator, int grp_id);
void cc_allocator_free(cc_allocator_t *allocator, void *mem);

#endif
