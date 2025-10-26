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

#define ALIGN_SIZE 0x1000
#define NUM_SLOT ((KERN_MAP_MAX_ADDR - KERN_MAP_MIN_ADDR) / ALIGN_SIZE)
#define ITERATION 10
// #define ESET_OFFSET 0x400000
#define ESET_OFFSET 0x100000
#define ESET_SIZE 6

#define TRAINING_ITERS 6

#endif
