#ifndef CONFIG_H
#define CONFIG_H

#define __DEBUG
#define __TIME

#define RESULT_FILE "../util/res"

#define PAGE_SIZE     0x1000UL
#define ALIGN_SIZE    PAGE_SIZE

#define KERN_MAP_MIN_ADDR (0xffff800000000000)
#define KERN_MAP_MAX_ADDR (0xffffffffffffffff - (PAGE_SIZE - 1))
// TODO Check if this is constant with different amounts of RAM
// Ignore the last region which can vary in size
#define KERN_MAP_SIZE 0x956459000

#define KERNBASE          0xffffffff80000000
#define KERNTEXTOFF       0xffffffff80200000 
#define KERN_IMAGE_SIZE   (0x1c29120UL & ~(ALIGN_SIZE - 1))

#define START_SEARCH    (KERNBASE - (100 * PAGE_SIZE))
#define END_SEARCH      (KERNBASE + KERN_IMAGE_SIZE + (10 * PAGE_SIZE))


#define NUM_SLOT ((END_SEARCH - START_SEARCH) / ALIGN_SIZE)
//#define NUM_SLOT ((KERN_MAP_MAX_ADDR - KERN_MAP_MIN_ADDR) / ALIGN_SIZE)


// cache and TLB information (2):
//    0x63: data TLB: 2M/4M pages, 4-way, 32 entries
//          data TLB: 1G pages, 4-way, 4 entries
//    0x03: data TLB: 4K pages, 4-way, 64 entries
//    0x76: instruction TLB: 2M/4M pages, fully, 8 entries
//    0xb5: instruction TLB: 4K, 8-way, 64 entries
//    0xc3: L2 TLB: 4K/2M pages, 6-way, 1536 entries

#define ITERATION 20
#define TRAINING_ITERS 20

#endif
