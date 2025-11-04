#ifndef CONFIG_H
#define CONFIG_H

#define __DEBUG
#define __TIME

#define RESULT_FILE "../util/res"

#define KERN_MAP_MIN_ADDR 0xffff800000000000
#define KERN_MAP_MAX_ADDR 0xfffff00000000000
// TODO Check if this is constant with different amounts of RAM
// Ignore the last region which can vary in size
#define KERN_MAP_SIZE 0x956459000
#define CNT 1

// cache and TLB information (2):
//    0x63: data TLB: 2M/4M pages, 4-way, 32 entries
//          data TLB: 1G pages, 4-way, 4 entries
//    0x03: data TLB: 4K pages, 4-way, 64 entries
//    0x76: instruction TLB: 2M/4M pages, fully, 8 entries
//    0xb5: instruction TLB: 4K, 8-way, 64 entries
//    0xc3: L2 TLB: 4K/2M pages, 6-way, 1536 entries

#define ITERATION 10
#define PAGE_SIZE     0x1000
#define ALIGN_SIZE    PAGE_SIZE
#define NUM_SLOT ((KERN_MAP_MAX_ADDR - KERN_MAP_MIN_ADDR) / ALIGN_SIZE)

#define L1_DTLB_SETS   16
#define L1_DTLB_WAYS   4
#define L2_TLB_SETS    256
#define L2_TLB_WAYS    6
// Original ESET_OFFSET from sysbumps = 0x400000
#define ESET_L1_OFFSET   (L1_DTLB_SETS * PAGE_SIZE)
#define ESET_L1_SIZE     L1_DTLB_WAYS
#define ESET_L2_OFFSET   (L2_TLB_SETS * PAGE_SIZE)
// Original ESET_SIZE = 12
#define ESET_L2_SIZE     L2_TLB_WAYS

#define TRAINING_ITERS 6

#endif
