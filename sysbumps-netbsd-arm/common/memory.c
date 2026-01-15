
#include "memory.h"

#define TLB_SET(addr) (((uint64_t)addr >> PAGESHIFT) & ((1 << SET_SHIFT) - 1))
#define TRUNC_PAGE(x)  (((uint64_t)x) & ~(PAGE_SIZE - 1))
#define TRUNC_PAGE_16K(x)  (((uint64_t)x) & ~(0x4000 - 1))

void gen_eset(void * target, void ** eset, void * addr) {
    void * base = addr + (uint64_t)((uint64_t)target & (ESET_OFFSET - 1));

    //printf("Eset base: %#lx\n", addr);
    //printf("Eset offset: %#lx\n", ESET_OFFSET);
    //printf("Stride: %#lx\n", STRIDE);
    //printf("Target: %p (set %d)\n", target, TLB_SET(target));
    //printf("Base: %p (set %d)\n", base, TLB_SET(base));

    for(int i = 0; i < ESET_SIZE; i++){
        eset[i] = base + (STRIDE+128)* (i + 1); 
        //printf("\tEset addr: %p (set %d)\n", eset[i], TLB_SET(eset[i]));
    }
    memory_fence();
}
