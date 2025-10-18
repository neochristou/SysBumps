#ifndef MEMORY_H
#define MEMORY_H

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "config.h"
#include "timing.h"

#define memory_access(x)                                                       \
  __asm__ volatile("movq (%[addr]), %%rax" ::[addr] "r"(x) : "%rax", "memory")
#define memory_prefetch(x)                                                     \
  __asm__ volatile("prefetchw (%[addr])" ::[addr] "r"(x) : "memory")
#define memory_fence() __asm__ volatile("mfence\nlfence" ::: "memory")

static inline __attribute__((always_inline)) uint64_t
probe_access(void *address) {
  register uint64_t start, end;
  memory_fence();
  timer_read(start);
  memory_fence();
  memory_access(address);
  memory_fence();
  timer_read(end);
  memory_fence();
  return end - start;
}

static inline __attribute__((always_inline)) uint64_t
probe_prefetch(void *address) {
  register uint64_t start, end;
  memory_fence();
  timer_read(start);
  memory_fence();
  memory_prefetch(address);
  memory_fence();
  timer_read(end);
  memory_fence();
  return end - start;
}

static inline __attribute__((always_inline)) void prime(void **eset) {
  for (int i = 0; i < ESET_SIZE; i++) {
    memory_fence();
    memory_access(eset[i]);
    memory_fence();
  }
}

static inline __attribute__((always_inline)) uint64_t probe(void **eset) {
  uint64_t time = 0;
  for (int i = 0; i < ESET_SIZE; i++) {
    memory_fence();
    time += probe_prefetch(eset[i]);
    memory_fence();
  }
  return time;
}

void gen_eset(void *target, void **eset, void *addr);

#endif
