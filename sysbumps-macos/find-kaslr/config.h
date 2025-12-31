#ifndef CONFIG_H
#define CONFIG_H

#define __DEBUG
#define __TIME

#define RESULT_FILE "../util/res"

#define PAGE_SIZE 0x4000UL
#define ALIGN_SIZE PAGE_SIZE

#define KER_START 0xfffffe000f004000
#define KER_SIZE 0x67E8000
#define KER_END ((0xfffffe002f000000 + KER_SIZE + 0x4000))
#define DATA_END_OFFSET 0x52D8000

#define START_SEARCH (KER_START)
#define END_SEARCH (KER_END)

#define NUM_SLOT ((END_SEARCH - START_SEARCH) / ALIGN_SIZE)

#define ITERATION 20
#define TRAINING_ITERS 4

#endif
