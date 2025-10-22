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

static inline __attribute__((always_inline)) unsigned long
probe_access(const char *addr) {
  unsigned long t1;    /* start time */
  unsigned long t2;    /* end time */
  unsigned long dummy; /* dummy variable to load *addr into */

  /*
   * Note: according to the `rdtsc` manual, the high bits
   * of %rax and %rdx are cleared
   */
  asm __volatile__("mfence               \n"
                   "lfence               \n"
                   "rdtsc                \n"
                   "lfence               \n"
                   "movq %%rax, %[start] \n"
                   "movq (%[in]), %[out] \n"
                   "lfence               \n"
                   "rdtsc                \n"
                   "movq %%rax, %[stop]  \n"
                   /* output */
                   : [start] "=&r"(t1), [stop] "=&r"(t2), [out] "=&r"(dummy)
                   /* input */
                   : [in] "p"(addr)
                   /* clobber */
                   : "%rax", "%rdx", "memory");

  /* Return time of load (ldr) and don't count timing overhead */
  return t2 - t1 - timer_overhead;
}

static inline __attribute__((always_inline)) unsigned long
probe_prefetch(const char *addr) {
  unsigned long t1;    /* start time */
  unsigned long t2;    /* end time */
  unsigned long dummy; /* dummy variable to load *addr into */

  /*
   * Note: according to the `rdtsc` manual, the high bits
   * of %rax and %rdx are cleared
   */
  asm __volatile__("mfence               \n"
                   "lfence               \n"
                   "rdtsc                \n"
                   "lfence               \n"
                   "movq %%rax, %[start] \n"
                   "prefetchw %[in]      \n"
                   "lfence               \n"
                   "rdtsc                \n"
                   "movq %%rax, %[stop]  \n"
                   /* output */
                   : [start] "=&r"(t1), [stop] "=&r"(t2)
                   /* input */
                   : [in] "p"(addr)
                   /* clobber */
                   : "%rax", "%rdx", "memory");

  /* Return time of load (ldr) and don't count timing overhead */
  return t2 - t1 - timer_overhead;
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
