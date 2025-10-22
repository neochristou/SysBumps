#ifndef CONFIG_H
#define CONFIG_H

#define __DEBUG
#define __TIME


#define RESULT_FILE "../util/res"

#define KER_START 0xffffffff80200000
#define KER_SIZE 0x1600000
#define KER_END   ((0xffffffff81779880 + 0x86780 + 0x4000))
#define DATA_END_OFFSET (0x1579880 + 0x86780)
#define CNT 1

#define ALIGN_SIZE 0x4000
#define NUM_SLOT ((KER_END - KER_START)/ALIGN_SIZE)
#define ITERATION 10
// #define ESET_OFFSET 0x400000
#define ESET_OFFSET 0x100000
#define ESET_SIZE 6

#define TRAINING_ITERS 6

#endif

