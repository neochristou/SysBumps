#ifndef CONFIG_H
#define CONFIG_H

#define __DEBUG
#define __TIME

#define RESULT_FILE "../util/res"

//#define KER_START 0xfffffe000f004000
//#define KER_SIZE 0x67E8000
//#define KER_END   ((0xfffffe002f000000 + KER_SIZE + 0x4000))
//#define DATA_END_OFFSET 0x52D8000
//#define CNT 1

#define ITERATION   	  20
#define TRAINING_ITERS 	  10

#define PAGE_SIZE 	  0x1000
#define PAGESHIFT 	  12 // log2(PAGE_SIZE)
			     
#define WAYS 		  12
#define SETS 		  256
#define SET_SHIFT 	  8 // log2(SETS)
#define ESET_OFFSET 	  (PAGE_SIZE * SETS)
#define STRIDE	 	  (PAGE_SIZE * SETS * 4)
//#define SETS 		  256
//#define SET_SHIFT 	  8 // log2(SETS)
//#define ESET_OFFSET 	  (PAGE_SIZE * SETS * 4)
#define ESET_SIZE 	  WAYS
#define ALIGN_SIZE 	  PAGE_SIZE

//#define KERNBASE          0xffffffff80000000
//#define KERNTEXTOFF       0xffffffff80200000
#define KERNBASE          0xffffc00000000000
#define KERNTEXTOFF          0xffffc00000000000
#define KERN_IMAGE_SIZE   (0x1c29120UL & ~(ALIGN_SIZE - 1))
#define KERN_TEXT_SIZE 	0x74a440
// End of .bss
#define KERN_IMAGE_SIZE (0xffffc00001001000 - KERNBASE + 0x1af060)

#define START_SEARCH    (KERNBASE - (100 * PAGE_SIZE))
#define END_SEARCH      (KERNBASE + KERN_IMAGE_SIZE + (10 * PAGE_SIZE))
//#define START_SEARCH    (0xFFFFC00000000000 - (100 * PAGE_SIZE))
//#define END_SEARCH      (0xFFFFC00040000000 + 1824 * 1024 + (10 * PAGE_SIZE))
#define NUM_SLOT ((END_SEARCH - START_SEARCH) / ALIGN_SIZE)
//#define NUM_SLOT ((KERN_MAP_MAX_ADDR - KERN_MAP_MIN_ADDR) / ALIGN_SIZE)
//#define NUM_SLOT ((KER_END - KER_START)/ALIGN_SIZE)
//
#define TLB_SET(addr) (((uint64_t)addr >> PAGESHIFT) & ((1 << SET_SHIFT) - 1))

#endif

