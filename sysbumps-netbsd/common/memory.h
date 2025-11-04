#ifndef TLB_FLUSH_H
#define TLB_FLUSH_H

#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define PAGESIZE_4K 12
#define PAGESIZE_2M 21
#define PAGE_4K 0x1000

#define HUGEPAGES 128

/**
 * TLB settings
 */
// data TLB: 2M/4M pages, 4-way, 32 entries (8 sets, 2^3)
// data TLB: 1G pages, 4-way, 4 entries
// data TLB: 4K pages, 4-way, 64 entries (16 sets, 2^4)
// instruction TLB: 2M/4M pages, fully, 8 entries
// instruction TLB: 4K, 8-way, 64 entries
// L2 TLB: 4K/2M pages, 6-way, 1536 entries (256 sets, 2^8, this might be wrong, could be 12-way)
//
// Hashsize is probably the number of sets because we logical and with this to
// get the set number
#define STLB_HASHSIZE_4K 7
#define DTLB_HASHSIZE_4K 4
#define STLB_WAYS_4K 12
#define DTLB_WAYS_4K 4
#define STLB_EVSET_SIZE_4K (STLB_WAYS_4K)
#define DTLB_EVSET_SIZE_4K (DTLB_WAYS_4K)
#define DTLB_EVSET_SIZE_2M (DTLB_WAYS_2M)
#define STLB_HASHMASK_4K ((1 << STLB_HASHSIZE_4K) - 1)
#define DTLB_HASHMASK_4K ((1 << DTLB_HASHSIZE_4K) - 1)
#define STLB_SET_4K(addr)                                                      \
  (((addr >> PAGESIZE_4K) ^ (addr >> (PAGESIZE_4K + STLB_HASHSIZE_4K))) &      \
   STLB_HASHMASK_4K)
#define DTLB_SET_4K(addr) ((addr >> PAGESIZE_4K) & DTLB_HASHMASK_4K)

#define STLB_WAYS_2M 6
#define DTLB_WAYS_2M 3
#define STLB_HASHSIZE_2M 8
#define DTLB_HASHSIZE_2M 3
#define STLB_EVSET_SIZE_2M (STLB_WAYS_2M)
#define STLB_HASHMASK_2M ((1 << STLB_HASHSIZE_2M) - 1)
#define DTLB_HASHMASK_2M ((1 << DTLB_HASHSIZE_2M) - 1)
#define STLB_SET_2M(addr) (((addr >> PAGESIZE_2M)) & STLB_HASHMASK_2M)
#define DTLB_SET_2M(addr) ((addr >> PAGESIZE_2M) & DTLB_HASHMASK_2M)

#define FLUSH_SET_SIZE                                                         \
  ((1UL << (PAGESIZE_4K + STLB_HASHSIZE_4K * 2))) * 4
 // 12bit page size 4096 times to cover up to 3072 TLB entries
#define TLB_EVICTION_SIZE                                                      \
  (1UL << (PAGESIZE_4K +12))


#define memory_fence() __asm__ volatile("mfence\nlfence" ::: "memory")

void gen_eset_stlb(size_t addr, void **eset_stlb);
void gen_eset_dtlb(size_t addr, void **eset_dtlb);
void gen_eset_stlb_pc(size_t addr, void **eset_stlb);
void gen_eset_dtlb_pc(size_t addr, void **eset_dtlb);
void prime_stlb(void **eset_stlb);
void prime_dtlb(void **eset_dtlb);
uint64_t probe_stlb(void **eset);
uint64_t probe_stlb_pc(void **eset);
uint64_t probe_dtlb(void **eset);
uint64_t probe_dtlb_pc(void **eset);
void get_timer_overhead(void);
void pin_to_core(size_t core);
void init_tlb_flush(void);
void maccess(void *p);
// void gen_eset_sysbumps(size_t target, void **eset_l1, void **eset_l2, size_t base_offset);

#endif
