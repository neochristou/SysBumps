#ifndef CONFIG_H
#define CONFIG_H

#define __DEBUG
#define __TIME

#define RESULT_FILE "../util/res"

#define ITERATION   	  1000
#define TRAINING_ITERS 	  6

#define PAGE_SIZE 	  0x1000ULL
#define PAGESHIFT 	  12 // log2(PAGE_SIZE)
			     
#define WAYS 		  12
#define SETS 		  256
#define SET_SHIFT 	  8 // log2(SETS)
#define ESET_OFFSET 	  (PAGE_SIZE * SETS * 4)
#define STRIDE 	 	  (PAGE_SIZE * SETS)
#define ESET_SIZE 	  WAYS

#endif

