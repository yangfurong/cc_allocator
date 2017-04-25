#include "cc_allocator.h"
#include "list.h"
#include "mem_basis.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static int get_color(cc_allocator_t *allocator, void *addr){
    uint64_t pfn_index = HPAGE_DIFF(allocator->zone->addr, addr);
    uint64_t pfn = allocator->zone->pfn_map[pfn_index];
    uint64_t phys_addr = (pfn << PAGE_SHIFT) + HPAGE_OFFSET((uint64_t)addr);
    return CC_GET_COLOR(phys_addr);
}

static int get_grp(cc_allocator_t *allocator, void *addr){
    int color = get_color(allocator, addr);
    int i, accum = 0;
    for(i = 0; i < allocator->grp_nr; i++){
        if(color >= accum && color < (accum + allocator->grp_colors[i])){
            return i;
        }
        accum += allocator->grp_colors[i];
    }
    return -1;
}

cc_allocator_t* cc_allocator_create(int obj_nr, int obj_size,
        int grp_nr, int *grp_ratio){
    cc_allocator_t *allocator = (cc_allocator_t*)malloc(sizeof(cc_allocator_t));
    if(!allocator){
        perror("cc_allocator_create: allocator alloc error\n");
        return NULL;
    }

    allocator->grp_colors = (int*)malloc(sizeof(int) * grp_nr);
    if(!allocator->grp_colors){
        perror("cc_allocator_create: grp_colors alloc error\n");
        goto grp_colors_error;
    }

    allocator->grps = (struct list_head*)malloc(sizeof(struct list_head) * grp_nr);
    if(!allocator->grps){
        perror("cc_allocator_create: grps alloc error\n");
        goto grps_error;
    }
    
    int i, ratio_sum = 0, color_sum = 0;
    for(i = 0; i < grp_nr; i++){
        ratio_sum += grp_ratio[i];
        INIT_LIST_HEAD(&allocator->grps[i]);
    }
    for(i = 0; i < grp_nr - 1; i++){
        allocator->grp_colors[i] = CC_COLOR_MAX * grp_ratio[i] / ratio_sum;
        color_sum += allocator->grp_colors[i];
    }
    allocator->grp_colors[i] = CC_COLOR_MAX - color_sum;

    allocator->grp_nr = grp_nr;
    allocator->obj_nr = obj_nr;
    allocator->obj_size = CC_CACHE_LINE_ROUNDUP(obj_size);

    int mem_size = obj_nr * allocator->obj_size * CC_MEM_FACTOR;
    allocator->zone = memzone_create("/mnt/huge/ccmem", mem_size);
    if(!allocator->zone){
        goto mz_error;
    }

    //first color of hugepage is always 0
    void *curr_addr = allocator->zone->addr;
    void *max_addr = curr_addr + allocator->zone->size;
    void *obj_ptr, *right_bound;
    obj_t *entry;
    while(curr_addr < max_addr){
        printf("color: %d\n", get_color(allocator, curr_addr));
        obj_ptr = curr_addr;
        curr_addr += HPAGE_SIZE;
        i = 0;
        while(obj_ptr < curr_addr){
            right_bound = obj_ptr + allocator->grp_colors[i] * CC_PAGE_SIZE;
            while(obj_ptr + allocator->obj_size <= right_bound){
                entry = (obj_t*)malloc(sizeof(obj_t));
                if(!entry){
                    perror("cc_allocator_create: entry alloc error\n");
                    cc_allocator_destroy(allocator);
                    return NULL;
                }
                entry->addr = obj_ptr;
                list_add(&entry->list, &allocator->grps[i]);
                obj_ptr += allocator->obj_size;
            }
            obj_ptr = right_bound;
            i++;
        }
    }

    return allocator;

mz_error:
    free(allocator->grps);
grps_error:
    free(allocator->grp_colors);
grp_colors_error:
    free(allocator);
    return NULL;
}

void cc_allocator_destroy(cc_allocator_t *allocator){
    struct list_head *pos;
    int i;
    for(i = 0; i < allocator->grp_nr; i++){
        while(!list_empty(&allocator->grps[i])){
            pos = allocator->grps[i].next;
            list_del(pos);
            free(list_entry(pos, obj_t, list));
        }
    }
    free(allocator->grps);
    free(allocator->grp_colors);
    memzone_destroy(allocator->zone);
    free(allocator);
}

void* cc_allocator_alloc(cc_allocator_t *allocator, int grp_id){
    struct list_head *pos;
    obj_t *entry;
    void *ret;
    if(!list_empty(&allocator->grps[grp_id])){
        pos = allocator->grps[grp_id].next;
        list_del(pos);
        entry = list_entry(pos, obj_t, list);
        ret = entry->addr;
        free(entry);
        return ret;
    }
    return NULL;
}

void cc_allocator_free(cc_allocator_t *allocator, void *mem){
    obj_t *entry = (obj_t*)malloc(sizeof(obj_t));
    if(!entry){
        perror("cc_allocator_free: alloc entry error\n");
        assert(entry);
    }
    int grp_id = get_grp(allocator, mem);
    assert(grp_id >= 0);
    entry->addr = mem;
    list_add(&entry->list, &allocator->grps[grp_id]);
}

