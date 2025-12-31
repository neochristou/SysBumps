#include <pthread.h>
#include <pwd.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "memory.h"

#ifndef HIDEMINMAX
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#endif

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

/**
 * todo set this THRESHOLD depending on your system
 */
unsigned THRESHOLD = 30;
unsigned THRESHOLD2 = 32;

uint8_t *flush_set;
unsigned long timer_overhead;
uint64_t timestamp;
pthread_t counting_thread;

#define timer_read(x) x = timestamp
#define memory_access(x)                                                       \
  __asm__ volatile("LDR x10, [%[addr]]" ::[addr] "r"(x) : "x10", "memory")
#define memory_prefetch(x)                                                     \
  __asm__ volatile("PRFM PSTL1STRM,[%[addr]]" ::[addr] "r"(x)                  \
                   : "x10", "memor"                                            \
                            "y")
#define memory_fence() __asm__ volatile("DMB SY\nISB SY" ::: "memory")
#define nop() __asm__ volatile("MOV x10, x10" ::: "x10", "memory")

void *counting(void *ctx) {
  __asm__ volatile("LDR x10, [%[ctx]]\n"
                   "loop:\n"
                   "ADD x10, x10, #1\n"
                   "STR x10, [%[ctx]]\n"
                   "B loop\n"
                   :
                   : [ctx] "r"(ctx)
                   : "x10", "memory");
}

void start_timer() {
  pthread_create(&counting_thread, NULL, counting, &timestamp);
}

void stop_timer() { pthread_cancel(counting_thread); }

void get_timer_overhead(void) {
  register uint64_t start, end;
  memory_fence();
  timer_read(start);
  memory_fence();
  nop();
  memory_fence();
  timer_read(end);
  memory_fence();
  timer_overhead = end - start;
}

void init_tlb_flush(void) {
  flush_set = mmap(0, FLUSH_SET_SIZE, PROT_READ | PROT_WRITE,
                   MAP_ANON | MAP_PRIVATE, -1, 0);
  if (flush_set == MAP_FAILED) {
    perror("mmap(flush_set)");
    exit(-1);
  }
}

/**
 * Timed access
 */
size_t __attribute__((noinline, aligned(4096))) onlyreload(size_t addr) {
  register uint64_t start, end;
  memory_fence();
  timer_read(start);
  memory_fence();
  memory_prefetch(addr);
  memory_fence();
  timer_read(end);
  memory_fence();
  return end - start - timer_overhead;
}

size_t __attribute__((noinline, aligned(4096))) onlyaccess(size_t addr) {
  register uint64_t start, end;
  memory_fence();
  timer_read(start);
  memory_fence();
  memory_access(addr);
  memory_fence();
  timer_read(end);
  memory_fence();
  return end - start - timer_overhead;
}

int comp(const void *e1, const void *e2) {
  return *(size_t *)e1 > *(size_t *)e2;
}

inline __attribute__((always_inline)) void prime_stlb(void **eset_stlb) {
  memory_fence();
  for (int i = 0; i < STLB_EVSET_SIZE; i++) {
    memory_access(eset_stlb[i]);
  }
  memory_fence();
}

inline __attribute__((always_inline)) void prime_dtlb(void **eset_dtlb) {
  memory_fence();
  for (int i = 0; i < DTLB_EVSET_SIZE; i++) {
    memory_access(eset_dtlb[i]);
  }
  memory_fence();
}

inline __attribute__((always_inline)) uint64_t probe_stlb(void **eset) {
  uint64_t time = 0, tmp = 0;
  memory_fence();
  for (int i = 0; i < STLB_EVSET_SIZE; i++) {
    // for (int i = STLB_EVSET_SIZE - 1; i >= 0; i--) {
    tmp = onlyreload((size_t)eset[i]);
    // tmp = onlyaccess((size_t)eset[i]);
    // printf("%lu ", tmp);
    time += tmp;
  }
  memory_fence();
  // printf("\n");
  return time;
}

inline __attribute__((always_inline)) uint64_t probe_dtlb(void **eset) {
  uint64_t time = 0, tmp = 0;
  memory_fence();
  for (int i = 0; i < DTLB_EVSET_SIZE; i++) {
    tmp = onlyreload((size_t)eset[i]);
    // tmp = onlyaccess((size_t)eset[i]);
    // printf("%lu ", tmp);
    time += tmp;
  }
  memory_fence();
  // printf("\n");
  return time;
}

inline __attribute__((always_inline)) void gen_eset_dtlb(size_t addr,
                                                         void **eset_dtlb) {
  size_t dtlb_set = DTLB_SET(addr);
  size_t flush_base = (size_t)flush_set;
  flush_base = (((flush_base >> (PAGESIZE + DTLB_HASHSIZE * 2)))
                << (PAGESIZE + DTLB_HASHSIZE * 2)) +
               (1UL << (PAGESIZE + DTLB_HASHSIZE * 2));

  // printf("Orig set: %lu\n", stlb_set);
  // dtlb
  // for (size_t i = 0; i < DTLB_WAYS * 2; i++) {
  for (size_t i = 0; i < DTLB_EVSET_SIZE; i++) {
    size_t evict_addr = (flush_base + (dtlb_set << PAGESIZE)) ^
                        (i << (PAGESIZE + DTLB_HASHSIZE));
    // printf("base: %p, evict_addr: %lx, dset: %d, target dset: %d\n",
    // flush_base,
    //        evict_addr, DTLB_SET(evict_addr), DTLB_SET(addr));
    memory_access((void *)evict_addr);
    eset_dtlb[i] = (void *)evict_addr;
  }
}

inline __attribute__((always_inline)) void gen_eset_stlb(size_t addr,
                                                         void **eset_stlb) {
  size_t stlb_set = STLB_SET(addr);
  size_t flush_base = (size_t)flush_set;
  flush_base = (((flush_base >> (PAGESIZE + STLB_HASHSIZE * 2)))
                << (PAGESIZE + STLB_HASHSIZE * 2)) +
               (1UL << (PAGESIZE + STLB_HASHSIZE * 2));

  // printf("Orig set: %lu\n", stlb_set);
  // stlb
  // for (size_t i = 0; i < STLB_WAYS * 2; i++) {
  for (size_t i = 0; i < STLB_EVSET_SIZE; i++) {
    // size_t evict_addr = (flush_base + (stlb_set << PAGESIZE)) ^
    //                     (((i << STLB_HASHSIZE) + i) << PAGESIZE);
    size_t evict_addr = (flush_base + (stlb_set << PAGESIZE)) ^
                        (i << (PAGESIZE + STLB_HASHSIZE));
    // printf("base: %p, evict_addr: %lx, set: %d, target set: %d\n",
    // flush_base,
    //        evict_addr, STLB_SET(evict_addr), STLB_SET(addr));
    memory_access((void *)evict_addr);
    eset_stlb[i] = (void *)evict_addr;
  }
}
