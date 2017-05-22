#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <stdint.h>
#include "cc_allocator.h"

#define BLOCK_ELEMS 1024
struct block_data{
    char d[BLOCK_ELEMS];
}__attribute__((__aligned__(64)));

char block_process(struct block_data *blk){
    char cksum;
    int i;
    for(i = 0, cksum = 0; i < BLOCK_ELEMS; i++){
        cksum ^= blk->d[i];
    }
    return cksum;
}

//50M
//#define PART
#define N 2
#define BLKS (8)
struct block_data *blks[N][BLKS];

cc_allocator_t* blocks_init(){
#ifdef PART
    int grp_ratio[2] = {1, 1};
    cc_allocator_t *allocator = cc_allocator_create(N * BLKS, 
            sizeof(struct block_data),
            2, grp_ratio);
#else
    int grp_ratio[1] = {1};
    cc_allocator_t *allocator = cc_allocator_create(N * BLKS, 
            sizeof(struct block_data),
            1, grp_ratio);
#endif
    assert(allocator);
    int i, j;
    for(i = 0; i < N; i++){
        for(j = 0; j < BLKS; j++){
#ifdef PART
            blks[i][j] = (struct block_data*)cc_allocator_alloc(allocator, i == N - 1);
#else
            blks[i][j] = (struct block_data*)cc_allocator_alloc(allocator, 0);
#endif
            assert(blks[i][j]);
            memset(blks[i][j], 0, sizeof(struct block_data));
        }
    }
    return allocator;
}

void blocks_destroy(cc_allocator_t *allocator){
    int i, j;
    for(i = 0; i < N; i++){
        for(j = 0; j < BLKS; j++){
            cc_allocator_free(allocator, (void*)blks[i][j]);
        }
    }
    cc_allocator_destroy(allocator);
}


#define TIME_DIFF(t1, t2) \
    t2.tv_sec - t1.tv_sec - (t2.tv_nsec < t1.tv_nsec), \
    t2.tv_nsec - t1.tv_nsec + 1000000000ULL * (t2.tv_nsec < t1.tv_nsec)

int main(int argc, char **argv){
    srand(time(NULL));
    int times = 10000000;
    char cksum = 0;
    cc_allocator_t *allocator = blocks_init();

    struct timespec t1, t2;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    while(times--){
        int i;
        int blks_id = random() % (N - 1);
        for(i = 0; i < BLKS; i++){
            int blk_id = random() % BLKS;
            cksum ^= block_process(blks[blks_id][blk_id]);
        }

        /*for(i = 0; i < 16; i++){
            int blk_id = random() % (16);
            cksum ^= block_process(blks[blks_id][blk_id]);
        }*/
    }
    clock_gettime(CLOCK_MONOTONIC, &t2);
    printf("diff: %u.%09u cksum: %x\n", TIME_DIFF(t1, t2), cksum);

    blocks_destroy(allocator);
    return 0;
}
