#define _GNU_SOURCE
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
// uint8_t *flush_set_2M;

// TODO adapt this for netbsd
void pin_to_core(size_t core) {
  int ret;
  cpu_set_t cpuset;

  CPU_ZERO(&cpuset);
  CPU_SET(core, &cpuset);

  ret = sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
  if (ret) {
    perror("sched_setaffinity: ");
    exit(-1);
  }
}

void maccess(void *p) { asm volatile("movq (%0), %%rax\n" : : "c"(p) : "rax"); }

void get_timer_overhead(void) {
  unsigned long t1; /* start time */
  unsigned long t2; /* end time */

  asm __volatile__("mfence               \n"
                   "lfence               \n"
                   "rdtsc                \n"
                   "lfence               \n"
                   "movq %%rax, %[start] \n"
                   "nop                  \n"
                   "lfence               \n"
                   "rdtsc                \n"
                   "movq %%rax, %[stop]  \n"
                   /* output */
                   : [start] "=&r"(t1), [stop] "=&r"(t2)
                   /* input */
                   :
                   /* clobber */
                   : "%rax", "%rdx", "memory");
  /* Return timing overhead */
  timer_overhead = t2 - t1;
}

inline size_t rdtsc_nofence(void) {
  size_t a, d;
  asm volatile("rdtsc" : "=a"(a), "=d"(d));
  a = (d << 32) | a;
  return a;
}

inline size_t rdtsc(void) {
  size_t a, d;
  asm volatile("mfence");
  asm volatile("rdtsc" : "=a"(a), "=d"(d));
  a = (d << 32) | a;
  asm volatile("mfence");
  return a;
}

inline size_t rdtsc_cpuid_begin() {
  size_t a, d;
  asm volatile("mfence\n\t"
               "RDTSCP\n\t"
               "mov %%rdx, %0\n\t"
               "mov %%rax, %1\n\t"
               "xor %%rax, %%rax\n\t"
               "CPUID\n\t"
               : "=r"(d), "=r"(a)
               :
               : "%rax", "%rbx", "%rcx", "%rdx");
  a = (d << 32) | a;
  return a;
}

inline size_t rdtsc_cpuid_end() {
  size_t a, d;
  asm volatile("xor %%rax, %%rax\n\t"
               "CPUID\n\t"
               "RDTSCP\n\t"
               "mov %%rdx, %0\n\t"
               "mov %%rax, %1\n\t"
               "mfence\n\t"
               : "=r"(d), "=r"(a)
               :
               : "%rax", "%rbx", "%rcx", "%rdx");
  a = (d << 32) | a;
  return a;
}

static inline size_t rdtsc_begin(void) {
  size_t a, d;
  asm volatile("mfence");
  asm volatile("rdtsc" : "=a"(a), "=d"(d));
  a = (d << 32) | a;
  asm volatile("lfence");
  return a;
}

static inline size_t rdtsc_end(void) {
  size_t a, d;
  asm volatile("lfence");
  asm volatile("rdtsc" : "=a"(a), "=d"(d));
  a = (d << 32) | a;
  asm volatile("mfence");
  return a;
}

inline void prefetcht0(void *p) { asm volatile("prefetcht0 (%0)" : : "r"(p)); }

inline void prefetcht1(void *p) { asm volatile("prefetcht1 (%0)" : : "r"(p)); }

inline void prefetcht2(void *p) { asm volatile("prefetcht2 (%0)" : : "r"(p)); }

inline void prefetchnta(void *p) {
  asm volatile("prefetchnta (%0)" : : "r"(p));
}

// ---------------------------------------------------------------------------
static inline void prefetch2(void *p) {
  asm volatile("prefetchnta (%0)" : : "a"(p));
  asm volatile("prefetcht2 (%0)" : : "a"(p));
}

inline void prefetch(__attribute__((unused)) size_t p) {
  asm volatile(".intel_syntax noprefix");
  asm volatile("prefetchnta qword ptr [%0]" : : "r"(p));
  asm volatile("prefetcht2 qword ptr [%0]" : : "r"(p));
  asm volatile(".att_syntax");
}

#define TIMER(x) rdtsc_##x

#define FLUSH_TLB_ALL flush_tlb_4k
#define FLUSH_TLB_T_4K flush_tlb_targeted_4k
// #define FLUSH_TLB_T_2M flush_tlb_targeted_2M

#define TIMER_START TIMER(begin)
#define TIMER_END TIMER(end)
#define FLUSH_TLB_4K FLUSH_TLB_T_4K
// #define FLUSH_TLB_2M FLUSH_TLB_T_2M

void init_tlb_flush(void) {
  flush_set = mmap(0, FLUSH_SET_SIZE, PROT_READ | PROT_WRITE,
                   MAP_ANON | MAP_PRIVATE, -1, 0);
  if (flush_set == MAP_FAILED) {
    perror("mmap(flush_set)");
    exit(-1);
  }

  // if (posix_memalign((void **)&flush_set_2M, 1 << PAGESIZE_2M,
  //                    (HUGEPAGES + 1) << PAGESIZE_2M) != 0) {
  //   perror("mmap(flush_set_2M)");
  //   exit(-1);
  // }
  // madvise(flush_set_2M, (HUGEPAGES + 1) << PAGESIZE_2M, MADV_HUGEPAGE);

  // for (unsigned i = 0; i < HUGEPAGES + 1; i++) {
  //   flush_set_2M[i << PAGESIZE_2M] = 1;
  //   // check the page is indeed huge
  //   // check_huge_page(buf);
  // }
}

/**
 * Timed access
 */
size_t __attribute__((noinline, aligned(4096))) onlyreload(size_t addr) {
  size_t t = TIMER_START();
  prefetch2((void *)addr);
  // prefetcht0((void*)addr);
  // prefetcht1((void*)addr);
  // prefetcht2((void*)addr);
  // prefetchnta((void*)addr);
  return TIMER_END() - t - timer_overhead;
}

size_t __attribute__((noinline, aligned(4096))) onlyaccess(size_t addr) {
  size_t t = TIMER_START();
  maccess((void *)addr);
  // prefetcht0((void*)addr);
  // prefetcht1((void*)addr);
  // prefetcht2((void*)addr);
  // prefetchnta((void*)addr);
  return TIMER_END() - t - timer_overhead;
}

int comp(const void *e1, const void *e2) {
  return *(size_t *)e1 > *(size_t *)e2;
}

size_t hit(size_t addr, size_t tries) {
  size_t time;
  /* leaking */
  prefetch2((void *)addr);
  for (size_t i = 0; i < tries; ++i) {
    time = onlyreload(addr);
    if (time <= THRESHOLD)
      return 1;
  }
  return 0;
}

size_t hit_accurate(size_t addr, size_t tries) {
  size_t time;
  size_t times[tries];
  /* leaking */
  prefetch2((void *)addr);
  for (size_t i = 0; i < tries; ++i) {
    time = onlyreload(addr);
    times[i] = time;
  }
  qsort(times, tries, sizeof(size_t), comp);
  time = times[tries / 4];
  return time <= THRESHOLD;
}

// static inline __attribute__((always_inline))
void prime_stlb(void **eset_stlb) {
  memory_fence();
  for (int i = 0; i < STLB_EVSET_SIZE_4K; i++) {
    maccess(eset_stlb[i]);
  }
  memory_fence();
}

void prime_dtlb(void **eset_dtlb) {
  memory_fence();
  for (int i = 0; i < DTLB_EVSET_SIZE_4K; i++) {
    maccess(eset_dtlb[i]);
  }
  memory_fence();
}

// static inline __attribute__((always_inline))
uint64_t probe_stlb(void **eset) {
  uint64_t time = 0, tmp = 0;
  memory_fence();
  // for (int i = 0; i < STLB_EVSET_SIZE_4K; i++) {
  for (int i = STLB_EVSET_SIZE_4K - 1; i >= 0; i--) {
    // tmp = onlyreload((size_t)eset[i]);
    tmp = onlyaccess((size_t)eset[i]);
    // printf("%lu ", tmp);
    time += tmp;
  }
  memory_fence();
  // printf("\n");
  return time;
}

uint64_t probe_stlb_pc(void **eset) {
  unsigned long t1; /* start time */
  unsigned long t2; /* end time */
  unsigned long dummy;
  asm __volatile__("mov %[eset_start], %[dummy] \n"
                   "mfence               \n"
                   "lfence               \n"
                   "rdtsc                \n"
                   "lfence               \n"
                   "movq %%rax, %[start] \n"
                   ".rept " TOSTRING(STLB_WAYS_4K) "\n"
                                                   "mov (%[dummy]), %[dummy]\n"
                                                   ".endr                \n"
                                                   "lfence               \n"
                                                   "rdtsc                \n"
                                                   "movq %%rax, %[stop]  \n"
                   /* output */
                   : [start] "=&r"(t1), [stop] "=&r"(t2), [dummy] "+r"(dummy)
                   /* input */
                   : [eset_start] "r"(eset[0])
                   /* clobber */
                   : "%rax", "%rdx", "memory");
  return t2 - t1 - timer_overhead;
}

uint64_t probe_dtlb_pc(void **eset) {
  unsigned long t1; /* start time */
  unsigned long t2; /* end time */
  unsigned long dummy;
  asm __volatile__("mov %[eset_start], %[dummy] \n"
                   "mfence               \n"
                   "lfence               \n"
                   "rdtsc                \n"
                   "lfence               \n"
                   "movq %%rax, %[start] \n"
                   ".rept " TOSTRING(DTLB_WAYS_4K) "\n"
                                                   "mov (%[dummy]), %[dummy]\n"
                                                   ".endr                \n"
                                                   "lfence               \n"
                                                   "rdtsc                \n"
                                                   "movq %%rax, %[stop]  \n"
                   /* output */
                   : [start] "=&r"(t1), [stop] "=&r"(t2), [dummy] "+r"(dummy)
                   /* input */
                   : [eset_start] "r"(eset[0])
                   /* clobber */
                   : "%rax", "%rdx", "memory");
  return t2 - t1 - timer_overhead;
}

uint64_t probe_dtlb(void **eset) {
  uint64_t time = 0, tmp = 0;
  memory_fence();
  for (int i = 0; i < DTLB_EVSET_SIZE_4K; i++) {
    // tmp = onlyreload((size_t)eset[i]);
    tmp = onlyaccess((size_t)eset[i]);
    // printf("%lu ", tmp);
    time += tmp;
  }
  memory_fence();
  // printf("\n");
  return time;
}

void gen_eset_dtlb(size_t addr, void **eset_dtlb) {
  size_t dtlb_set = DTLB_SET_4K(addr);
  size_t flush_base = (size_t)flush_set;
  flush_base = (((flush_base >> (PAGESIZE_4K + STLB_HASHSIZE_4K * 2)))
                << (PAGESIZE_4K + STLB_HASHSIZE_4K * 2)) +
               (1UL << (PAGESIZE_4K + STLB_HASHSIZE_4K * 2));

  // printf("Orig set: %lu\n", stlb_set);
  // dtlb
  // for (size_t i = 0; i < DTLB_WAYS_4K * 2; i++) {
  for (size_t i = 0; i < DTLB_EVSET_SIZE_4K; i++) {
    size_t evict_addr = (flush_base + (dtlb_set << PAGESIZE_4K)) ^
                        (i << (PAGESIZE_4K + DTLB_HASHSIZE_4K));
    // printf("base: %p, evict_addr: %lx, dset: %d, target dset: %d\n",
    // flush_base,
    //        evict_addr, DTLB_SET_4K(evict_addr), DTLB_SET_4K(addr));
    maccess((void *)evict_addr);
    eset_dtlb[i] = (void *)evict_addr;
  }
}

void gen_eset_stlb(size_t addr, void **eset_stlb) {
  size_t stlb_set = STLB_SET_4K(addr);
  size_t flush_base = (size_t)flush_set;
  flush_base = (((flush_base >> (PAGESIZE_4K + STLB_HASHSIZE_4K * 2)))
                << (PAGESIZE_4K + STLB_HASHSIZE_4K * 2)) +
               (1UL << (PAGESIZE_4K + STLB_HASHSIZE_4K * 2));

  // printf("Orig set: %lu\n", stlb_set);
  // stlb
  // for (size_t i = 0; i < STLB_WAYS_4K * 2; i++) {
  for (size_t i = 0; i < STLB_EVSET_SIZE_4K; i++) {
    size_t evict_addr = (flush_base + (stlb_set << PAGESIZE_4K)) ^
                        (((i << STLB_HASHSIZE_4K) + i) << PAGESIZE_4K);
    // printf("base: %p, evict_addr: %lx, set: %d, target set: %d\n",
    // flush_base,
    //        evict_addr, STLB_SET_4K(evict_addr), STLB_SET_4K(addr));
    maccess((void *)evict_addr);
    eset_stlb[i] = (void *)evict_addr;
  }
}

void gen_eset_dtlb_pc(size_t addr, void **eset_dtlb) {
  size_t dtlb_set = DTLB_SET_4K(addr);
  size_t flush_base = (size_t)flush_set;
  flush_base = (((flush_base >> (PAGESIZE_4K + STLB_HASHSIZE_4K * 2)))
                << (PAGESIZE_4K + STLB_HASHSIZE_4K * 2)) +
               (1UL << (PAGESIZE_4K + STLB_HASHSIZE_4K * 2));

  // printf("Orig set: %lu\n", stlb_set);
  // dtlb
  // for (size_t i = 0; i < DTLB_WAYS_4K * 2; i++) {
  for (size_t i = 0; i < DTLB_EVSET_SIZE_4K; i++) {
    size_t evict_addr = (flush_base + (dtlb_set << PAGESIZE_4K)) ^
                        (i << (PAGESIZE_4K + DTLB_HASHSIZE_4K));
    // printf("base: %p, evict_addr: %lx, dset: %d, target dset: %d\n",
    // flush_base,
    //        evict_addr, DTLB_SET_4K(evict_addr), DTLB_SET_4K(addr));
    maccess((void *)evict_addr);
    eset_dtlb[i] = (void *)evict_addr;
  }
  for (size_t i = 0; i < DTLB_EVSET_SIZE_4K - 1; i++) {
    *(uint64_t *)eset_dtlb[i] = (uint64_t)eset_dtlb[i + 1];
  }
}

void gen_eset_stlb_pc(size_t addr, void **eset_stlb) {
  size_t stlb_set = STLB_SET_4K(addr);
  size_t flush_base = (size_t)flush_set;
  flush_base = (((flush_base >> (PAGESIZE_4K + STLB_HASHSIZE_4K * 2)))
                << (PAGESIZE_4K + STLB_HASHSIZE_4K * 2)) +
               (1UL << (PAGESIZE_4K + STLB_HASHSIZE_4K * 2));

  // stlb
  // for (size_t i = 0; i < STLB_WAYS_4K * 2; i++) {
  for (size_t i = 0; i < STLB_EVSET_SIZE_4K; i++) {
    size_t evict_addr = (flush_base + (stlb_set << PAGESIZE_4K)) ^
                        (((i << STLB_HASHSIZE_4K) + i) << PAGESIZE_4K);
    // printf("base: %p, evict_addr: %lx, set: %d, target set: %d\n",
    // flush_base,
    //        evict_addr, STLB_SET_4K(evict_addr), STLB_SET_4K(addr));
    maccess((void *)evict_addr);
    eset_stlb[i] = (void *)evict_addr;
  }

  for (size_t i = 0; i < STLB_EVSET_SIZE_4K - 1; i++) {
    *(uint64_t *)eset_stlb[i] = (uint64_t)eset_stlb[i + 1];
  }
}

// #define L1_DTLB_SETS 16
// #define L1_DTLB_WAYS 4
// #define L2_TLB_SETS 128
// #define L2_TLB_WAYS 12
// // Original ESET_OFFSET from sysbumps = 0x400000
// #define ESET_DTLB_OFFSET (L1_DTLB_SETS * PAGE_4K)
// #define ESET_DTLB_SIZE L1_DTLB_WAYS
// #define ESET_STLB_OFFSET (L2_TLB_SETS * PAGE_4K)
// // Original ESET_SIZE = 12
// #define ESET_STLB_SIZE L2_TLB_WAYS

// void gen_eset_sysbumps(size_t target, void **eset_l2, void **eset_l1,
//                        size_t flush_base) {
//   size_t base_l1 = flush_base + ((uint64_t)target & (ESET_DTLB_OFFSET - 1));
//   for (int i = 0; i < ESET_DTLB_SIZE; i++) {
//     eset_l1[i] = (void *)(base_l1 + (ESET_DTLB_OFFSET + 128) * (i + 1));
//   }

//   size_t base_l2 = flush_base + ((uint64_t)target & (ESET_STLB_OFFSET - 1));
//   for (int i = 0; i < ESET_STLB_SIZE; i++) {
//     eset_l2[i] = (void *)(base_l2 + (ESET_STLB_OFFSET + 128) * (i + 1));
//   }
//   memory_fence();
// }
