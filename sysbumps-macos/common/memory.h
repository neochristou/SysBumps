#ifndef TLB_FLUSH_H
#define TLB_FLUSH_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#define PAGESIZE_4K 12
#define PAGE_4K 0x1000
#define PAGESIZE_16K 14
#define PAGE_16K 0x4000

#define PAGESIZE PAGESIZE_4K
#define PAGE PAGE_4K

/**
 * TLB settings (M1)
 */
// data TLB: 5-way, 32 set
// L2 TLB: 12-way, 256 set
//
// Hashsize is probably the number of sets because we logical and with this to
// get the set number
#define STLB_HASHSIZE 7
#define DTLB_HASHSIZE 4
#define STLB_WAYS 12
#define DTLB_WAYS 5
#define STLB_EVSET_SIZE (STLB_WAYS)
#define DTLB_EVSET_SIZE (DTLB_WAYS)
#define STLB_HASHMASK ((1 << STLB_HASHSIZE) - 1)
#define DTLB_HASHMASK ((1 << DTLB_HASHSIZE) - 1)
#define STLB_SET(addr)                                                         \
  (((addr >> PAGESIZE) ^ (addr >> (PAGESIZE + STLB_HASHSIZE))) & STLB_HASHMASK)
#define DTLB_SET(addr) ((addr >> PAGESIZE) & DTLB_HASHMASK)

#define FLUSH_SET_SIZE ((1UL << (PAGESIZE + STLB_HASHSIZE * 2))) * 4
// 12bit page size 4096 times to cover up to 3072 TLB entries
#define TLB_EVICTION_SIZE (1UL << (PAGESIZE + 12))

void gen_eset_stlb(size_t addr, void **eset_stlb);
void gen_eset_dtlb(size_t addr, void **eset_dtlb);
// void gen_eset_stlb_pc(size_t addr, void **eset_stlb);
// void gen_eset_dtlb_pc(size_t addr, void **eset_dtlb);
void prime_stlb(void **eset_stlb);
void prime_dtlb(void **eset_dtlb);
uint64_t probe_stlb(void **eset);
// uint64_t probe_stlb_pc(void **eset);
uint64_t probe_dtlb(void **eset);
// uint64_t probe_dtlb_pc(void **eset);
void get_timer_overhead(void);
void pin_to_core(size_t core);
void init_tlb_flush(void);
void maccess(void *p);
void start_timer();
void stop_timer();

#endif
